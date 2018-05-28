#
# Copyright (c) 2015-2018, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

from vfh import vfh_reload

from . import vfh_tests_obj_per_tex

def testsObjPerTex():
    vfh_reload.reload_module(vfh_tests_obj_per_tex)
    vfh_tests_obj_per_tex.main()
