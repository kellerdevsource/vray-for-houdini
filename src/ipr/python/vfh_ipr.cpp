//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini Python IPR Module
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include <Python.h>

#include "vfh_exporter.h"
#include "vfh_log.h"
#include "vfh_ipr_viewer.h"
#include "vfh_ipr_client.h"
#include "vfh_vray_instances.h"
#include "vfh_attr_utils.h"
#include "vfh_process_check.h"

#include <HOM/HOM_Module.h>

#include <QThread>
#include <QByteArray>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QHostAddress>
#include <QApplication>

#include <mutex>

using namespace VRayForHoudini;

/// Class wrapping callback function and flag to call it only once
/// the flag is protected by mutex so the callback is called only once
/// NOTE: consider doing this with atomic bool
class CallOnceUntilReset {
public:
	typedef std::function<void()> CB;

	CallOnceUntilReset(CB callback)
		: m_isCalled(false)
		, m_callback(callback)
	{}

	/// Get function that when called will inturn call the callback if flag is not set
	/// Used to avoid the need to pass the object and function pointer
	CB getCallableFunction() {
		return std::bind(&CallOnceUntilReset::call, this);
	}

	/// Reset the "called" flag to false
	void reset() {
		std::lock_guard<std::mutex> lock(m_mtx);
		m_isCalled = false;
	}

	/// Manually set the "called" flag to false
	void set() {
		std::lock_guard<std::mutex> lock(m_mtx);
		m_isCalled = true;
	}

	/// Manually call the saved callback if the "called" flag is false
	/// This function call's are serialized with mutex
	void call() {
		if (!m_isCalled) {
			std::lock_guard<std::mutex> lock(m_mtx);
			if (!m_isCalled) {
				m_isCalled = true;
				m_callback();
			}
		}
	}

	/// Return the "called" flag
	/// NOTE: until this function returns the flag could have changed already
	bool isCalled() const {
		return m_isCalled;
	}

	CallOnceUntilReset(const CallOnceUntilReset &) = delete;
	CallOnceUntilReset & operator=(const CallOnceUntilReset &) = delete;

private:
	/// Lock protecting the called flag
	std::mutex m_mtx;
	/// Flag so that we call the callback only once
	bool m_isCalled;
	/// The saved callback function
	CB m_callback;
} * stopCallback;

FORCEINLINE int getInt(PyObject *list, int idx)
{
	return PyInt_AS_LONG(PyList_GET_ITEM(list, idx));
}

FORCEINLINE float getFloat(PyObject *list, int idx)
{
	return PyFloat_AS_DOUBLE(PyList_GET_ITEM(list, idx));
}

FORCEINLINE int getInt(PyObject *dict, const char *key, int defValue)
{
	if (!dict)
		return defValue;
	PyObject *item = PyDict_GetItemString(dict, key);
	if (!item)
		return defValue;
	return PyInt_AS_LONG(item);
}

FORCEINLINE float getFloat(PyObject *dict, const char *key, float defValue)
{
	if (!dict)
		return defValue;
	PyObject *item = PyDict_GetItemString(dict, key);
	if (!item)
		return defValue;
	return PyFloat_AS_DOUBLE(item);
}

static VRayExporter *vrayExporter = nullptr;
static PingPongClient *stopChecker = nullptr;
static ProcessCheckPtr procCheck = nullptr;
std::recursive_mutex expMtx;


/// RAII wrapper over the lock of the vrayExporter pointer
/// Has bool cast operator so it can be used directly in if statements
struct WithExporter {
	WithExporter() {
		expMtx.lock();
	}

	~WithExporter() {
		expMtx.unlock();
	}

	explicit operator bool() const & {
		if (!vrayExporter) {
			Log::getLog().warning("WithExporter RAII lock - NULL exporter");
		}
		return !!vrayExporter;
	}

	// make sure we execute this only on variables of this clas
	// NOTE: dissalows this: if (WithExporter()) {...} which will not hold lock inside the if body
	explicit operator bool() && = delete;

	VfhDisableCopy(WithExporter)
};

static VRayExporter& getOrCreateExporter()
{
	{
		WithExporter lk;
		if (!vrayExporter) {
			vrayExporter = new VRayExporter(nullptr);
		}
	}
	return *vrayExporter;
}

static void freeExporter()
{
//	UT_ASSERT_MSG(stopChecker, "stopChecker is null in freeExporter()");
//	stopChecker->stop();

	closeImdisplay();

	if (WithExporter lk{}) {
		if (vrayExporter) {
			vrayExporter->reset();
			FreePtr(vrayExporter);
		}
	}
}

static struct VRayExporterIprUnload {
	~VRayExporterIprUnload() {
		if (stopCallback) {
			// Prevent stop cb from being called here
			stopCallback->set();
		}
		delete stopChecker;
		deleteVRayInit();
		Log::Logger::stopLogging();
	}
} exporterUnload;

static void onVFBClosed(VRay::VRayRenderer&, void*)
{
	freeExporter();
}

static void fillViewParamsFromDict(PyObject *viewParamsDict, ViewParams &viewParams)
{
	if (!viewParamsDict)
		return;
	if (!PyDict_Check(viewParamsDict))
		return;

	PyObject *transform = PyDict_GetItemString(viewParamsDict, "transform");
	PyObject *res = PyDict_GetItemString(viewParamsDict, "res");

	const int ortho = getInt(viewParamsDict, "ortho", 0);

	const float cropLeft   = getFloat(viewParamsDict, "cropl", 0.0f);
	const float cropRight  = getFloat(viewParamsDict, "cropr", 1.0f);
	const float cropBottom = getFloat(viewParamsDict, "cropb", 0.0f);
	const float cropTop    = getFloat(viewParamsDict, "cropt", 1.0f);

	const float aperture = getFloat(viewParamsDict, "aperture", 41.4214f);
	const float focal = getFloat(viewParamsDict, "focal", 50.0f);

	const int resX = getInt(res, 0);
	const int resY = getInt(res, 1);

	viewParams.renderView.fov = getFov(aperture, focal);
	viewParams.renderView.ortho = ortho;

	viewParams.renderSize.w = resX;
	viewParams.renderSize.h = resY;

	viewParams.cropRegion.x = resX * cropLeft;
	viewParams.cropRegion.y = resY * (1.0f - cropTop);
	viewParams.cropRegion.width  = resX * (cropRight - cropLeft);
	viewParams.cropRegion.height = resY * (cropTop - cropBottom);

	if (transform &&
		PyList_Check(transform) &&
		PyList_Size(transform) == 16)
	{
		VRay::Transform tm;
		tm.matrix.v0.set(getFloat(transform, 0), getFloat(transform, 1), getFloat(transform, 2));
		tm.matrix.v1.set(getFloat(transform, 4), getFloat(transform, 5), getFloat(transform, 6));
		tm.matrix.v2.set(getFloat(transform, 8), getFloat(transform, 9), getFloat(transform, 10));
		tm.offset.set(getFloat(transform, 12), getFloat(transform, 13), getFloat(transform, 14));

		viewParams.renderView.tm = tm;
	}
}

static PyObject* vfhExportView(PyObject*, PyObject *args, PyObject *keywds)
{
	PyObject *viewParamsDict = nullptr;

	static char *kwlist[] = {
		/* 0 */ "viewParams",
	    NULL
	};

	//                                 0 12345678911
	//                                            01
	static const char kwlistTypes[] = "O";

	if (!PyArg_ParseTupleAndKeywords(args, keywds, kwlistTypes, kwlist,
		/* 0 */ &viewParamsDict
	)) {
		PyErr_Print();
		Py_RETURN_NONE;
	}

	HOM_AutoLock autoLock;

	VRayExporter &exporter = getOrCreateExporter();
	exporter.exportDefaultHeadlight(true);

	const char *camera = PyString_AsString(PyDict_GetItemString(viewParamsDict, "camera"));

	OBJ_Node *cameraNode = nullptr;
	if (UTisstring(camera)) {
		cameraNode = CAST_OBJNODE(getOpNodeFromPath(camera));
	}

	ViewParams viewParams(cameraNode);
	if (cameraNode && !cameraNode->getName().contains("ipr_camera")) {
		exporter.fillViewParamFromCameraNode(*cameraNode, viewParams);
	}
	else {
		fillViewParamsFromDict(viewParamsDict, viewParams);
	}

	// Copy params; no const ref!
	ViewParams oldViewParams = exporter.getViewParams();

	// Update view.
	exporter.exportView(viewParams);

	// Update pipe if needed.
	if (oldViewParams.changedSize(viewParams)) {
		getImdisplay().restart();
		initImdisplay(exporter.getRenderer().getVRay());
	}

	Py_RETURN_NONE;
}

static PyObject* vfhDeleteOpNode(PyObject*, PyObject *args, PyObject *keywds)
{
	const char *opNodePath = nullptr;

	static char *kwlist[] = {
	    /* 0 */ "opNode",
	    NULL
	};

	//                                 012345678911
	//                                           01
	static const char kwlistTypes[] = "s";

	if (PyArg_ParseTupleAndKeywords(args, keywds, kwlistTypes, kwlist,
		/* 0 */ &opNodePath))
	{
		HOM_AutoLock autoLock;

		if (UTisstring(opNodePath)) {
			Log::getLog().debug("vfhDeleteOpNode(\"%s\")", opNodePath);

			ObjectExporter &objExporter = getOrCreateExporter().getObjectExporter();
			objExporter.removeObject(opNodePath);
		}
	}

	Py_RETURN_NONE;
}

static PyObject* vfhExportOpNode(PyObject*, PyObject *args, PyObject *keywds)
{
	const char *opNodePath = nullptr;

	static char *kwlist[] = {
	    /* 0 */ "opNode",
	    NULL
	};

	//                                 012345678911
	//                                           01
	static const char kwlistTypes[] = "s";

	if (PyArg_ParseTupleAndKeywords(args, keywds, kwlistTypes, kwlist,
		/* 0 */ &opNodePath))
	{
		HOM_AutoLock autoLock;

		OBJ_Node *objNode = CAST_OBJNODE(getOpNodeFromPath(opNodePath));
		if (objNode) {
			Log::getLog().debug("vfhExportOpNode(\"%s\")", opNodePath);

			ObjectExporter &objExporter = getOrCreateExporter().getObjectExporter();

			// Otherwise we won't update plugin.
			objExporter.clearOpPluginCache();
			objExporter.clearPrimPluginCache();

			// Update node
			objExporter.removeGenerated(*objNode);
			objExporter.exportObject(*objNode);
		}
	}

    Py_RETURN_NONE;
}

static PyObject* vfhInit(PyObject*, PyObject *args, PyObject *keywds)
{
	Log::getLog().debug("vfhInit()");

	const char *rop = nullptr;
	float now = 0.0f;
	int port = 0;
	PyObject *viewParamsDict = nullptr;

	static char *kwlist[] = {
	    /* 0 */ "rop",
	    /* 1 */ "port",
	    /* 2 */ "now",
	    /* 3 */ "viewParams",
	    NULL
	};

	//                                 012 345678911
	//                                           01
	static const char kwlistTypes[] = "sif|O";

	if (!PyArg_ParseTupleAndKeywords(args, keywds, kwlistTypes, kwlist,
		/* 0 */ &rop,
		/* 1 */ &port,
		/* 2 */ &now,
		/* 3 */ &viewParamsDict
	)) {
		PyErr_Print();
		Py_RETURN_NONE;
	}

	enum IPROutput {
		iprOutputRenderView = 0,
		iprOutputVFB,
	};

	HOM_AutoLock autoLock;
	
	getImdisplay().setPort(port);
	
	if (!stopCallback) {
		stopCallback = new CallOnceUntilReset(freeExporter);
		getImdisplay().setOnStopCallback(stopCallback->getCallableFunction());
	}

	if (!procCheck) {
		procCheck = makeProcessChecker(stopCallback->getCallableFunction(), "vfh_ipr.exe");
	}
	getImdisplay().setProcCheck(procCheck);
	procCheck->stop();
	procCheck->start();
	// Reset the cb's flag so it can be called asap
	stopCallback->reset();

	UT_String ropPath(rop);
	OP_Node *ropNode = getOpNodeFromPath(ropPath);
	if (ropNode) {
		// Start the imdisplay thread so we can get pipe signals sooner
		getImdisplay().init();
		getImdisplay().restart();
		const IPROutput iprOutput =
			static_cast<IPROutput>(ropNode->evalInt("render_rt_output", 0, 0.0));

		const int iprModeMenu = ropNode->evalInt("render_rt_update_mode", 0, 0.0);
		const VRayExporter::IprMode iprMode = iprModeMenu == 0 ? VRayExporter::iprModeRT : VRayExporter::iprModeSOHO;

		const int isRenderView = iprOutput == iprOutputRenderView;
		const int isVFB = iprOutput == iprOutputVFB;

		VRayExporter &exporter = getOrCreateExporter();

		exporter.setROP(*ropNode);
		exporter.setIPR(iprMode);

		if (WithExporter lk{}) {
			if (!exporter.initRenderer(isVFB, false)) {
				Py_RETURN_NONE;
			}
		}

		ViewParams viewParams;
		if (WithExporter lk{}) {
			fillViewParamsFromDict(viewParamsDict, viewParams);

			exporter.setDRSettings();

			exporter.setRendererMode(getRendererIprMode(*ropNode));
			exporter.setWorkMode(getExporterWorkMode(*ropNode));
		}

		if (WithExporter lk{}) {
			exporter.getRenderer().showVFB(isVFB);
			exporter.getRenderer().getVRay().setOnVFBClosed(isVFB ? onVFBClosed : nullptr);
			exporter.getRenderer().getVRay().setOnImageReady(isRenderView ? onImageReady : nullptr);
			exporter.getRenderer().getVRay().setOnRTImageUpdated(isRenderView ? onRTImageUpdated : nullptr);
			exporter.getRenderer().getVRay().setOnBucketReady(isRenderView ? onBucketReady : nullptr);

			exporter.getRenderer().getVRay().setKeepBucketsInCallback(isRenderView);
			exporter.getRenderer().getVRay().setKeepRTframesInCallback(isRenderView);

			if (isRenderView) {
				exporter.getRenderer().getVRay().setRTImageUpdateTimeout(250);
			}
		}

		if (WithExporter lk{}) {
			exporter.initExporter(getFrameBufferType(*ropNode), 1, now, now);

			exporter.setFrame(now);
		}

		if (WithExporter lk{}) {
			exporter.exportSettings();
			exporter.exportScene();
			exporter.exportView(viewParams);
			exporter.renderFrame();
			initImdisplay(exporter.getRenderer().getVRay());
		}
	}
	
    Py_RETURN_NONE;
}

static PyObject * vfhIsRopValid(PyObject *)
{
	if (!vrayExporter || !getOrCreateExporter().getRopPtr()) {
		Py_RETURN_FALSE;
	}
	Py_RETURN_TRUE;
}

static PyObject* vfhLogMessage(PyObject*, PyObject *args, PyObject *keywds)
{
	int logLevel;
	const char * message = nullptr;

	//                                 012345678911
	//                                           01
	static const char kwlistTypes[] = "is";

	if (!PyArg_ParseTuple(args, kwlistTypes, &logLevel, &message)) {
		PyErr_Print();
		Py_RETURN_NONE;
	}

	Log::getLog().log(static_cast<Log::LogLevel>(logLevel), message);

	Py_RETURN_NONE;
}

static PyMethodDef methods[] = {
	{
		"init",
		reinterpret_cast<PyCFunction>(vfhInit),
		METH_VARARGS | METH_KEYWORDS,
		"Init V-Ray IPR."
	},
	{
		"exportView",
		reinterpret_cast<PyCFunction>(vfhExportView),
		METH_VARARGS | METH_KEYWORDS,
		"Export view."
	},
	{
		"exportOpNode",
		reinterpret_cast<PyCFunction>(vfhExportOpNode),
		METH_VARARGS | METH_KEYWORDS,
		"Export object."
	},
	{
		"deleteOpNode",
		reinterpret_cast<PyCFunction>(vfhDeleteOpNode),
		METH_VARARGS | METH_KEYWORDS,
		"Delete object."
	},
	{
		"isRopValid",
		reinterpret_cast<PyCFunction>(vfhIsRopValid),
		METH_NOARGS,
		"Check if current rop is valid."
	},
	{
		"logMessage",
		reinterpret_cast<PyCFunction>(vfhLogMessage),
		METH_VARARGS,
		"Log message."
	},
	{ NULL, NULL, 0, NULL }
};

PyMODINIT_FUNC init_vfh_ipr()
{
	disableSIGPIPE();
	Log::Logger::startLogging();
	Py_InitModule("_vfh_ipr", methods);
}
