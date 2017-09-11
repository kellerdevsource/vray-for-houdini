#
# Copyright (c) 2015-2017, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

import hou
import sys

def show_vfbs():
    didShow = False
    for node in hou.node('/out/').children():
        if node.type().name() == 'vray_renderer':
            for param in node.allParms():
                if param.name() == 'show_current_vfb':
                    didShow = True
                    param.pressButton()
                    break

    if not didShow:
        sys.stdout.write('No V-Ray Renderer out nodes found to show VFB for!')
        sys.stdout.flush()
