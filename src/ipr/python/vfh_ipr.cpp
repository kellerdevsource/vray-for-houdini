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

using namespace VRayForHoudini;

VRayExporter exporter(nullptr);

static PyObject* vfhInit(PyObject *self, PyObject *args, PyObject *keywds)
{
    Log::getLog().debug("vfhInit()");

    Py_INCREF(Py_None);

    return Py_None;
}

static PyMethodDef methods[] = {
	{ "init", reinterpret_cast<PyCFunction>(vfhInit), METH_VARARGS | METH_KEYWORDS, "Init V-Ray IPR." },
	{ NULL, NULL, 0, NULL }
};

PyMODINIT_FUNC init_vfh_ipr()
{
	Py_InitModule("_vfh_ipr", methods);
}
