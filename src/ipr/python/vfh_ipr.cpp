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

#include <HOM/HOM_Module.h>

#include <QThread>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QHostAddress>

using namespace VRayForHoudini;

static VRayExporter *exporter = nullptr;

static class WorkerThread
	: public QThread
{
public:
	void setCallback(std::function<void()> value) {
		cb = value;
	}

private:
	void run() VRAY_OVERRIDE {
		while (true) {
			QTcpSocket socket;
			socket.connectToHost(QHostAddress(QHostAddress::LocalHost), 424242);
			if (!socket.waitForConnected(1000)) {
				Log::getLog().debug("Stop requested...");
				cb();
				return;
			}
			msleep(100);
		}
    }

	std::function<void()> cb;
} stopPoll;

static VRayExporter& getExporter()
{
	if (!exporter) {
		exporter = new VRayExporter(nullptr);
	}
	return *exporter;
}

static void freeExporter()
{
	stopPoll.exit();

	getExporter().reset();

	FreePtr(exporter);
}

static struct VRayExporterIprUnload {
	~VRayExporterIprUnload() {
		deleteVRayInit();
	}
} exporterUnload;

static void onVFBClosed(VRay::VRayRenderer&, void*)
{
	freeExporter();
}

static PyObject* vfhExportView(PyObject*, PyObject *args, PyObject *keywds)
{
	const char *rop = nullptr;

	static char *kwlist[] = {
	    /* 0 */ "rop",
	    NULL
	};

	//                                 012345678911
	//                                           01
	static const char kwlistTypes[] = "s";

	if (PyArg_ParseTupleAndKeywords(args, keywds, kwlistTypes, kwlist,
		/* 0 */ &rop))
	{
		HOM_AutoLock autoLock;

		VRayExporter &exporter = getExporter();

		exporter.exportDefaultHeadlight(true);
		exporter.exportView();
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

			ObjectExporter &objExporter = getExporter().getObjectExporter();
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

			ObjectExporter &objExporter = getExporter().getObjectExporter();

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

static int port = 0;

static PyObject* vfhInit(PyObject*, PyObject *args, PyObject *keywds)
{
	Log::getLog().debug("vfhInit()");

	const char *rop = nullptr;
	float now = 0.0f;

	static char *kwlist[] = {
	    /* 0 */ "rop",
	    /* 1 */ "port",
	    /* 2 */ "now",
	    NULL
	};

	//                                 012345678911
	//                                           01
	static const char kwlistTypes[] = "sif";

	if (PyArg_ParseTupleAndKeywords(args, keywds, kwlistTypes, kwlist,
		/* 0 */ &rop,
		/* 1 */ &port,
		/* 2 */ &now))
	{
		enum IPROutput {
			iprOutputRenderView = 0,
			iprOutputVFB,
		};

		HOM_AutoLock autoLock;

		UT_String ropPath(rop);
		OP_Node *ropNode = getOpNodeFromPath(ropPath);
		if (ropNode) {
			const IPROutput iprOutput = static_cast<IPROutput>(ropNode->evalInt("render_rt_output", 0, 0.0));

			const int isRenderView = iprOutput == iprOutputRenderView;
			const int isVFB = iprOutput == iprOutputVFB;

			VRayExporter &exporter = getExporter();

			exporter.setROP(*ropNode);
			exporter.setIPR(VRayExporter::iprModeRenderView);

			if (exporter.initRenderer(isVFB, false)) {
				exporter.setDRSettings();

				exporter.setRendererMode(getRendererIprMode(*ropNode));
				exporter.setWorkMode(getExporterWorkMode(*ropNode));

				exporter.getRenderer().showVFB(isVFB);
				exporter.getRenderer().getVRay().setOnVFBClosed(isVFB ? onVFBClosed : nullptr);
				exporter.getRenderer().getVRay().setOnImageReady(isRenderView ? onImageReady : nullptr);
				exporter.getRenderer().getVRay().setOnRTImageUpdated(isRenderView? onRTImageUpdated : nullptr);
				exporter.getRenderer().getVRay().setOnBucketReady(isRenderView ? onBucketReady : nullptr);

				exporter.getRenderer().getVRay().setKeepBucketsInCallback(isRenderView);
				exporter.getRenderer().getVRay().setKeepRTframesInCallback(isRenderView);

				if (isRenderView) {
					exporter.getRenderer().getVRay().setRTImageUpdateTimeout(250);
				}

				exporter.initExporter(getFrameBufferType(*ropNode), 1, now, now);

				exporter.exportSettings();
				exporter.exportFrame(now);

#if 0
				stopPoll.setCallback([]{
					closeImdisplay();
					freeExporter();
				});
				stopPoll.start(QThread::LowPriority);
#endif

				initImdisplay(exporter.getRenderer().getVRay(), port);
			}
		}
	}
   
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
	{ NULL, NULL, 0, NULL }
};

PyMODINIT_FUNC init_vfh_ipr()
{
	Py_InitModule("_vfh_ipr", methods);
}
