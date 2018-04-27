#
# Copyright (c) 2015-2018, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

import hou
import os

UI_FILEPATHS = {}

if not UI_FILEPATHS:
    dsDirPath = os.environ.get('VRAY_UI_DS_PATH', None)
    assert dsDirPath

    for root, dirs, files in os.walk(dsDirPath):
        for f in files:
            if f.endswith(".ds"):
                dsFileName = os.path.splitext(f)[0]
                UI_FILEPATHS[dsFileName] = os.path.join(root, f)

def _getDsFilePathFromName(fileName):
    assert fileName in UI_FILEPATHS
    return UI_FILEPATHS[fileName]

def getParmTemplatesFromDS(fileName, prefix=None, defines=()):
    pluginDs = _getDsFilePathFromName(fileName)

    pluginPtg = hou.ParmTemplateGroup()

    # This will make attribute names prefixed with plugin ID name.
    pluginPrefixDef = "" if not prefix else "#define PREFIX \"%s_\"\n" % (prefix)
    for d in defines:
        pluginPrefixDef += "#define %s %s\n" % (d[0], d[1])
    if pluginPrefixDef:
        pluginPrefixDef += "\n"

    dsContents = pluginPrefixDef + open(pluginDs, 'r').read()

    pluginPtg.setToDialogScript(dsContents)

    return pluginPtg.parmTemplates()

def removeFolderIfEmpty(ptg, folderName, removeNotEmpty=False, removeNotExact=False):
    folderParm = ptg.findFolder(folderName)
    if not folderParm:
        return
    if not folderParm.type() == hou.parmTemplateType.Folder:
        return
    if not removeNotExact and not folderParm.isActualFolder():
        return

    if not removeNotEmpty and len(folderParm.parmTemplates()):
        return

    ptg.remove(folderParm)

def ptgParmTemplatesIt(ptg):
    for pt in ptg.parmTemplates():
        yield pt

        if pt.type() == hou.parmTemplateType.Folder and pt.isActualFolder():
            for _pt in ptgParmTemplatesIt(pt):
                yield _pt

def _getFirstNonTabParmTemplate(ptf):
    return next((pt for pt in ptf.parmTemplates() if pt.type() != hou.parmTemplateType.Folder or pt.folderType() != hou.folderType.Tabs), None)

def insertInFolderAfterLastTab(ptg, ptf, pt):
    p = _getFirstNonTabParmTemplate(ptf)
    if p:
        ptg.insertBefore(p, pt)
    elif isinstance(ptf, hou.ParmTemplateGroup):
        ptg.append(pt)
    elif isinstance(ptf, hou.FolderParmTemplate):
        ptg.appendToFolder(ptf, pt)
