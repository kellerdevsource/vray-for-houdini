import houprm
import pluginutils as plgutils

import os
import re

plugindescpath = os.getenv("VRAY_PLUGIN_DESC_PATH", "")
dsdestpath = os.path.join(os.getenv("VRAY_UI_DS_PATH", ""), "plugins")


def getDialogScript(vrayPlugin, jsonDesc):
    assert( jsonDesc )

    ptg = houprm.createParmTemplateGroup(vrayPlugin, jsonDesc)
    dscontent = ptg.asDialogScript()

    rex = re.compile("\A\{(\s+)name(\s+)(\w+)(\s+)(label\s+\"(\w+)\"\s+)?")
    m = rex.match(dscontent)
    if m:
        dshead = m.string[ m.start():m.end() ]
        dstail = m.string[ m.end(): ]

        if m.group(3):
            dshead = dshead.replace(m.group(3), jsonDesc["ID"])

        if m.group(6):
            dshead = dshead.replace(m.group(6), jsonDesc["Name"])
        else:
            dshead += "label{}\"{}\"{}".format(m.group(2), jsonDesc["Name"],m.group(4))

        dshead += "parmtag{}{{ spare_category \"{}\" }}{}".format(m.group(2), jsonDesc["Name"],m.group(4))
        dshead += "parmtag{}{{ vray_plugin \"{}\" }}{}".format(m.group(2), jsonDesc["ID"],m.group(4))

        dscontent = dshead + dstail

    return dscontent


def saveJSONDescAsDialogScript(vrayPlugin, jsonDesc, filepath):
    dscontent = getDialogScript(vrayPlugin, jsonDesc)
    if not dscontent:
        return None

    dirpath, filename = os.path.split(filepath)
    if not os.path.isdir(dirpath):
        os.makedirs(dirpath)

    with open(filepath, "w") as file:
        file.write(dscontent)

    return filepath


def saveAllJSONDescAsDialogScripts(plugindescpath, destpath):
    descriptors = plgutils.getPluginDescriptorsFromJSON(plugindescpath)
    cnt = 0
    totalcnt = 0
    for descriptor in descriptors:
        assert( descriptor )

        totalcnt += 1

        vrayPlugin, jsonDesc = descriptor
        assert( jsonDesc )

        filepath = os.path.join(destpath, "{}.ds".format( jsonDesc["ID"] ) )
        filepath = saveJSONDescAsDialogScript(vrayPlugin, jsonDesc, filepath)
        if not filepath:
            print("!!ERROR!! {} - Could not translate JSON plugin description to .ds file.".format( jsonDesc["ID"] ))
        else:
            cnt += 1

    print("!!INFO!! DONE exported {} of total {}".format( cnt, totalcnt ))


def saveJSONAsDialogScript(filepath, destpath):
    descriptor = plgutils.getPluginDescriptorFromJSON(filepath)
    assert( descriptor )

    vrayPlugin, jsonDesc = descriptor
    assert( jsonDesc )

    filepath = os.path.join(destpath, "{}.ds".format( jsonDesc["ID"] ) )
    filepath = saveJSONDescAsDialogScript(vrayPlugin, jsonDesc, filepath)
    if not filepath:
        print("!!ERROR!! {} - Could not translate JSON plugin description to .ds file.".format( jsonDesc["ID"] ))


def savePluginWithJSONAsDialogScript(vrayPlugin, destpath):
    descriptor = plgutils.getPluginDescriptorFromVRayPlugin(vrayPlugin, plugindescpath)
    assert( descriptor )

    vrayPlugin, jsonDesc = descriptor
    assert( vrayPlugin )
    assert( jsonDesc )

    filepath = os.path.join(destpath, "{}.ds".format( jsonDesc["ID"] ) )
    filepath = saveJSONDescAsDialogScript(vrayPlugin, jsonDesc, filepath)
    if not filepath:
        print("!!ERROR!! {} - Could not translate JSON plugin description to .ds file.".format( jsonDesc["ID"] ))


def savePluginAsDialogScript(vrayPlugin, destpath):
    jsonDesc = plgutils.asJSONDesc(vrayPlugin)
    assert( vrayPlugin )
    assert( jsonDesc )

    filepath = os.path.join(destpath, "{}.ds".format( jsonDesc["ID"] ) )
    filepath = saveJSONDescAsDialogScript(vrayPlugin, jsonDesc, filepath)
    if not filepath:
        print("!!ERROR!! {} - Could not translate JSON plugin description to .ds file.".format( jsonDesc["ID"] ))
