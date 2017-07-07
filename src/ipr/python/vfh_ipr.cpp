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

#include <HOM/HOM_Module.h>

#include "vfh_exporter.h"
#include "vfh_log.h"
#include "vfh_hou_utils.h"

using namespace VRayForHoudini;

VRayExporter exporter(nullptr);

static PyObject* vfhDeleteOpNode(PyObject*, PyObject *args, PyObject *keywds)
{
	Py_INCREF(Py_None);
    return Py_None;
}

static PyObject* vfhExportOpNode(PyObject*, PyObject *args, PyObject *keywds)
{
	const char *opNodePath = nullptr;

	static char *kwlist[] = {
	    /* 0 */"opNode",
	    NULL
	};

	//                                 012345678911
	//                                           01
	static const char kwlistTypes[] = "s";

	if (PyArg_ParseTupleAndKeywords(args, keywds, kwlistTypes, kwlist,
		/* 0 */ &opNodePath))
	{
		HOM_AutoLock autoLock;

		// exporter.exportDefaultHeadlight(true);
		// exporter.exportView();

		OBJ_Node *objNode = CAST_OBJNODE(getOpNodeFromPath(opNodePath));
		if (objNode) {
			Log::getLog().debug("vfhExportOpNode(\"%s\")", opNodePath);

			ObjectExporter &objExporter = exporter.getObjectExporter();

			// Otherwise we won't update plugin.
			objExporter.clearOpPluginCache();
			objExporter.clearPrimPluginCache();

			// Update node
			objExporter.removeGenerated(*objNode);
			objExporter.exportObject(*objNode);
		}
	}

	Py_INCREF(Py_None);
    return Py_None;
}

static PyObject* vfhInit(PyObject*, PyObject *args, PyObject *keywds)
{
	Log::getLog().debug("vfhInit()");

	const char *rop = nullptr;
	const int port = 0;
	const float now = 0.0f;

	static char *kwlist[] = {
	    /* 0 */"rop",
	    /* 1 */"port",
	    /* 2 */"now",
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
		HOM_AutoLock autoLock;
		VRayPluginRenderer::initialize();

		UT_String ropPath(rop);
		OP_Node *ropNode = getOpNodeFromPath(ropPath);
		if (ropNode) {
			exporter.reset();
			exporter.setROP(*ropNode);
			exporter.setIPR(VRayExporter::iprModeRenderView);

			// Renderer mode (CPU / GPU)
			const int rendererMode = getRendererIprMode(*ropNode);

			// Rendering device
			const int wasGPU = exporter.isGPU();
			const int isGPU  = (rendererMode > VRay::RendererOptions::RENDER_MODE_RT_CPU);

			// Whether to re-create V-Ray renderer
			const int reCreate = wasGPU != isGPU;

			if (exporter.initRenderer(HOU::isUIAvailable(), reCreate)) {
				exporter.setRendererMode(rendererMode);
				exporter.setDRSettings();
				exporter.setWorkMode(getExporterWorkMode(*ropNode));

				exporter.initExporter(getFrameBufferType(*ropNode), 1, now, now);

				exporter.exportSettings();
				exporter.exportFrame(now);
			}
		}
	}
   
	Py_INCREF(Py_None);
    return Py_None;
}

static PyObject* vfhFree(PyObject*, PyObject *args, PyObject *keywds)
{
	exporter.reset();

	Py_INCREF(Py_None);
    return Py_None;
}

static PyMethodDef methods[] = {
	{
		"init",
		reinterpret_cast<PyCFunction>(vfhInit),
		METH_VARARGS | METH_KEYWORDS,
		"Init V-Ray IPR."
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
		"Export object."
	},
	{
		"free",
		reinterpret_cast<PyCFunction>(vfhFree),
		METH_VARARGS | METH_KEYWORDS,
		"Free V-Ray IPR."
	},
	{ NULL, NULL, 0, NULL }
};

PyMODINIT_FUNC init_vfh_ipr()
{
	Py_InitModule("_vfh_ipr", methods);
}
