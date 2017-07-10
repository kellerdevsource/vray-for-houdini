# Copyright (c) 2015-2017, Chaos Software Ltd.
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#
# Main help resource:
#   http://www.sidefx.com/docs/hdk15.5/_h_d_k__s_o_h_o.html
#

import sys

import soho
import sohoglue
import hou

import _vfh_ipr

from soho import SohoParm

def printDebug(fmt, *args):
    sys.stdout.write("V-Ray For Houdini IPR| ")
    sys.stdout.write(fmt % args)
    sys.stdout.write("\n")

def dumpObjects(listName):
    printDebug("Checking \"%s\"" % listName)
    for obj in soho.objectList(listName):
        printDebug("   %s", obj.getName())

def exportObjects(listName):
    for obj in soho.objectList(listName):
        _vfh_ipr.exportOpNode(opNode=obj.getName())

def deleteObjects(listName):
    for obj in soho.objectList(listName):
        _vfh_ipr.deleteNode(opNode=obj.getName())

mode = soho.getDefaultedString('state:previewmode', ['default'])[0]

# Evaluate an intrinsic parameter (see HDK_SOHO_API::evaluate())
# The 'state:time' parameter evaluates the time from the ROP.
now = soho.getDefaultedFloat('state:time', [0.0])[0]

# Evaluate the 'camera' parameter as a string.
# If the 'camera' parameter doesn't exist, use ['/obj/cam1'].
# SOHO always returns lists of values.
camera = soho.getDefaultedString('camera', ['/obj/cam1'])[0]

# MPlay / Render View port.
port = soho.getDefaultedInt("vm_image_mplay_socketport", [0])[0]
pid = soho.getDefaultedInt("soho:pipepid", [0])[0]

# ROP node.
ropPath = soho.getOutputDriver().getName()

# Initialize SOHO with the camera.
# XXX: This doesn't work for me, but it should according to the documentation...
#   soho.initialize(now, camera)
if not sohoglue.initialize(now, camera, None):
    soho.error("Unable to initialize rendering module with given camera")

# Now, add objects to our scene
soho.addObjects(now, "*", "*", "*", True)

# Before we can evaluate the scene from SOHO, we need to lock the object lists.
soho.lockObjects(now)

printDebug("Processing Mode: \"%s\"" % mode)

if mode in {"generate"}:
    # generate: Generation phase of IPR rendering 
    # In generate mode, SOHO will keep the pipe (soho_pipecmd)
    # command open between invocations of the soho_program.
    #   objlist:all
    #   objlist:camera
    #   objlist:light
    #   objlist:instance
    #   objlist:fog
    #   objlist:space
    #   objlist:mat
    #
    printDebug("IPR Port: %s" % port)
    printDebug("Driver: %s" % ropPath)
    printDebug("Camera: %s" % camera)
    printDebug("Now: %.3f" % now)
    printDebug("PID: %i" % pid)

    _vfh_ipr.init(rop=ropPath, port=port, now=now)

elif mode in {"update"}:
    # update: Send updated changes from previous generation
    #
    # In this rendering mode, the special object list parameters:
    #   objlist:dirtyinstance
    #   objlist:dirtylight
    #   objlist:dirtyspace
    #   objlist:dirtyfog
    # will contain the list of all objects modified since the last render
    # (whether a generate or update).
    #
    # As well, the parameters:
    #   objlist:deletedinstance
    #   objlist:deletedlight
    #   objlist:deletedspace
    #   objlist:deletedfog
    # will list all objects which have been deleted from the scene.
    #
    exportObjects("objlist:dirtyinstance")
    exportObjects("objlist:dirtylight")
    # exportObjects("objlist:dirtyspace")
    # exportObjects("objlist:dirtyfog")

    deleteObjects("objlist:deletedinstance")
    deleteObjects("objlist:deletedlight")
    # deleteObjects("objlist:deletedspace")
    # deleteObjects("objlist:deletedfog")
