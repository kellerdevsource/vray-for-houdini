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
#include "vfh_vray_instances.h"
#include "vfh_attr_utils.h"
#include "vfh_process_check.h"

#include <HOM/HOM_Module.h>

#include <mutex>

using namespace VRayForHoudini;

/// Class wrapping callback function and flag to call it only once
/// the flag is protected by mutex so the callback is called only once
/// NOTE: consider doing this with atomic bool
class CallOnceUntilReset {
public:
	typedef std::function<void()> CB;

	explicit CallOnceUntilReset(CB callback)
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

static ProcessCheckPtr procCheck = nullptr;

/// RAII wrapper over the lock of the vrayExporter pointer
/// Has bool cast operator so it can be used directly in if statements
class WithExporter
{
public:
	/// Locks the mutex
	WithExporter() {
		exporterMtx.lock();
	}

	/// Unlocks the mutex
	~WithExporter() {
		exporterMtx.unlock();
	}

	/// Return true if the exporter is already allocated
	explicit operator bool() const & {
		return !!exporter;
	}

	/// Dereference the exporter pointer and return it (this must not be called if the pointer is nullptr)
	static VRayExporter &getExporter() {
		if (!exporter) {
			Log::getLog().error("Trying to dereference NULL exporter!");
			vassert(false && "Trying to dereference NULL exporter!");
		}
		return *exporter;
	}

	/// Get copy of the exporter pointer
	static VRayExporter *getPointer() {
		return exporter;
	}

	/// Allocate the exporter
	static void allocExporter() {
		if (!exporter) {
			exporter = new VRayExporter(nullptr);
		}
	}

	/// Free the exporter
	static void freeExporter() {
		FreePtr(exporter);
	}

	/// Unguarded static method to access the pointer without locking the mutex
	/// Used to avoid locking if the exporter is nullptr and it is not needed to do operations with the pointer
	static VRayExporter *getPointerUnguarded() {
		return exporter;
	}

	// make sure we execute this only on variables of this clas
	// NOTE: dissalows this: if (WithExporter()) {...} which will not hold lock inside the if body
	explicit operator bool() && = delete;

private:
	static VRayExporter *exporter;
	static std::recursive_mutex exporterMtx;

	VfhDisableCopy(WithExporter)
};

VRayExporter* WithExporter::exporter = nullptr;
std::recursive_mutex WithExporter::exporterMtx;

static float lastExportTime = 0.0;

static void setExportTime(VRayExporter &exp, float time)
{
	exp.setTime(time);

	lastExportTime = time;
}

static void freeExporter()
{
	closeImdisplay();

	if (WithExporter lk{}) {
		lk.getExporter().reset();
		lk.freeExporter();
	}
}

struct VRayExporterIprUnload {
	~VRayExporterIprUnload() {
		if (stopCallback) {
			// Prevent stop cb from being called here
			stopCallback->set();
		}
		deleteVRayInit();
		Log::Logger::stopLogging();
	}
};

static const VRayExporterIprUnload exporterUnload;

static void fillRenderRegionFromDict(PyObject *viewParamsDict, ViewParams &viewParams)
{
	if (!viewParamsDict)
		return;
	if (!PyDict_Check(viewParamsDict))
		return;

	const float cropLeft   = getFloat(viewParamsDict, "cropl", 0.0f);
	const float cropRight  = getFloat(viewParamsDict, "cropr", 1.0f);
	const float cropBottom = getFloat(viewParamsDict, "cropb", 0.0f);
	const float cropTop    = getFloat(viewParamsDict, "cropt", 1.0f);

	const int resX = viewParams.renderSize.w;
	const int resY = viewParams.renderSize.h;

	viewParams.cropRegion.x = resX * cropLeft;
	viewParams.cropRegion.y = resY * (1.0f - cropTop);
	viewParams.cropRegion.width  = resX * (cropRight - cropLeft);
	viewParams.cropRegion.height = resY * (cropTop - cropBottom);
}

static void fillViewParamsFromDict(PyObject *viewParamsDict, ViewParams &viewParams)
{
	if (!viewParamsDict)
		return;
	if (!PyDict_Check(viewParamsDict))
		return;

	PyObject *transform = PyDict_GetItemString(viewParamsDict, "transform");
	PyObject *res = PyDict_GetItemString(viewParamsDict, "res");

	const float aperture = getFloat(viewParamsDict, "aperture", 41.4214f);
	const float focal = getFloat(viewParamsDict, "focal", 50.0f);

	viewParams.renderView.fov = getFov(aperture, focal);
	viewParams.renderView.ortho = getInt(viewParamsDict, "ortho", 0);

	viewParams.renderSize.w = getInt(res, 0);
	viewParams.renderSize.h = getInt(res, 1);

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

static void fillViewParams(VRayExporter &exporter, PyObject *viewParamsDict, ViewParams &viewParams)
{
	fillViewParamsFromDict(viewParamsDict, viewParams);
	fillRenderRegionFromDict(viewParamsDict, viewParams);

	OBJ_Node *cameraNode = nullptr;

	const char *camera = PyString_AsString(PyDict_GetItemString(viewParamsDict, "camera"));
	if (UTisstring(camera)) {
		cameraNode = CAST_OBJNODE(getOpNodeFromPath(camera));
	}

	if (cameraNode) {
		if (cameraNode->getName().equal("ipr_camera") ||
			exporter.usePhysicalCamera(*cameraNode) != PhysicalCameraMode::modeNone)
		{
			exporter.fillViewParamFromCameraNode(*cameraNode, viewParams);
		}
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
	WithExporter lock;
	if (!lock) {
		Py_RETURN_NONE;
	}

	VRayExporter &exporter = lock.getExporter();

	ViewParams viewParams;
	fillViewParams(exporter, viewParamsDict, viewParams);

	// Copy params; no const ref!
	ViewParams oldViewParams = exporter.getViewParams();

	// Update view.
	if (exporter.exportView(viewParams) == ReturnValue::Success) {
		exporter.exportDefaultHeadlight(true);

		// Update pipe if needed.
		if (oldViewParams.changedSize(viewParams)) {
			getImdisplay().restart();
			initImdisplay(exporter.getRenderer().getVRay());
		}
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

		if (WithExporter lock{}) {
			if (UTisstring(opNodePath)) {
				Log::getLog().debug("vfhDeleteOpNode(\"%s\")", opNodePath);

				ObjectExporter &objExporter = lock.getExporter().getObjectExporter();
				objExporter.removeObject(opNodePath);
			}
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
		if (WithExporter lock{}) {
			OBJ_Node *objNode = CAST_OBJNODE(getOpNodeFromPath(opNodePath));
			if (objNode) {
				Log::getLog().debug("vfhExportOpNode(\"%s\")", opNodePath);

				ObjectExporter &objExporter = lock.getExporter().getObjectExporter();

				// Otherwise we won't update plugin.
				objExporter.clearOpPluginCache();
				objExporter.clearPrimPluginCache();

				// Update node
				objExporter.removeGenerated(*objNode);
				objExporter.exportObject(*objNode);
			}
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

	HOM_AutoLock autoLock;

	getImdisplay().setPort(port);

	if (!stopCallback) {
		stopCallback = new CallOnceUntilReset(freeExporter);
		getImdisplay().setOnStopCallback(stopCallback->getCallableFunction());
	}

	if (!procCheck) {
#ifdef _WIN32
		const char * ipr_proc_exe = "vfh_ipr.exe";
#else
		const char * ipr_proc_exe = "vfh_ipr";
#endif
		procCheck = makeProcessChecker(stopCallback->getCallableFunction(), ipr_proc_exe);
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
		{
			WithExporter lk;
			vassert(!lk && "Exporter should be NULL in vfhInit");
			lk.allocExporter();
		}
		if (WithExporter lk{}) {
			getImdisplay().init();
			getImdisplay().restart();
		}

		if (WithExporter lk{}) {
			VRayExporter &exporter = lk.getExporter();
			exporter.setRopPtr(ropNode);
			exporter.setIPR(VRayExporter::iprModeSOHO);
			if (!exporter.initRenderer(false, false)) {
				Py_RETURN_NONE;
			}
		}

		if (WithExporter lk{}) {
			VRayExporter &exporter = lk.getExporter();

			exporter.setDRSettings();
			exporter.setRendererMode(getRendererIprMode(*ropNode));
			exporter.setWorkMode(getExporterWorkMode(*ropNode));
		}

		if (WithExporter lk{}) {
			VRayExporter &exporter = lk.getExporter();
			exporter.getRenderer().getVRay().setOnImageReady(onImageReady);
			exporter.getRenderer().getVRay().setOnRTImageUpdated(onRTImageUpdated);
			exporter.getRenderer().getVRay().setOnBucketReady(onBucketReady);

			exporter.getRenderer().getVRay().setKeepBucketsInCallback(true);
			exporter.getRenderer().getVRay().setKeepRTframesInCallback(true);
			exporter.getRenderer().getVRay().setRTImageUpdateTimeout(250);
		}

		if (WithExporter lk{}) {
			VRayExporter &exporter = lk.getExporter();
			exporter.initExporter(getFrameBufferType(*ropNode), 1, now, now);

			setExportTime(exporter, now);
		}

		if (WithExporter lk{}) {
			VRayExporter &exporter = lk.getExporter();
			if (exporter.exportSettings() == ReturnValue::Success) {
				ViewParams viewParams;
				fillViewParams(exporter, viewParamsDict, viewParams);

				exporter.exportView(viewParams);

				exporter.exportScene();
				exporter.renderFrame();
				initImdisplay(exporter.getRenderer().getVRay());
			}
		}
	}

    Py_RETURN_NONE;
}

static PyObject * vfhIsRopValid(PyObject *)
{
	// First try without locking
	if (!WithExporter::getPointerUnguarded()) {
		Py_RETURN_FALSE;
	}

	// We must lock if we will use the exporter
	if (WithExporter lk{}) {
		if (!lk.getExporter().getRopPtr()) {
			Py_RETURN_FALSE;
		}
	}

	Py_RETURN_TRUE;
}

static PyObject *vfhSetTime(PyObject*, PyObject *args, PyObject*)
{
	float time;

	static const char kwlistTypes[] = "f";

	if (!PyArg_ParseTuple(args, kwlistTypes, &time)) {
		PyErr_Print();
	}
	else {
		const bool isSameTime = IsFloatEq(time, lastExportTime);
		if (!isSameTime) {
			if (WithExporter lk{}) {
				setExportTime(lk.getExporter(), time);
			}
			Py_RETURN_TRUE;
		}
	}

	Py_RETURN_FALSE;
}

static PyObject* vfhLogMessage(PyObject*, PyObject *args, PyObject*)
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
		"setTime",
		reinterpret_cast<PyCFunction>(vfhSetTime),
		METH_VARARGS,
		"Sets export time. Returns True if time has changed, False otherwise."
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
