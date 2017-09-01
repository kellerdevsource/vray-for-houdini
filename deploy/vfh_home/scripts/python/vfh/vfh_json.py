#
# Copyright (c) 2015-2017, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

import glob
import json
import os
import sys


_plugins_desc = {}


def getPluginsDescDir():
    return os.environ['VRAY_PLUGIN_DESC_PATH']


def loadPluginsDescriptions():
    if not _plugins_desc:
        descDirpath = getPluginsDescDir()

        for filePath in glob.glob("%s/*/*.json" % descDirpath):
            pluginDesc = json.loads(open(filePath, 'r').read())

            pluginID     = pluginDesc.get('ID')
            pluginParams = pluginDesc.get('Parameters')
            pluginName   = pluginDesc.get('Name')
            pluginType   = pluginDesc.get('Type')
            pluginIdDesc = pluginDesc.get('Description', "")
            pluginWidget = pluginDesc.get('Widget', {})

            _plugins_desc[pluginID] = {
                'Type'         : pluginType,
                'ID'           : pluginID,
                'Name'         : pluginName,
                'Description'  : pluginIdDesc,
                'PluginParams' : pluginParams,
                'PluginWidget' : pluginWidget,
            }


def getPluginDesc(pluginID):
    if not _plugins_desc:
        loadPluginsDescriptions()

    if pluginID not in _plugins_desc:
        sys.stderr.write("Plugin \"%s\" description is not found!\n" % pluginID)
    else:
        return _plugins_desc[pluginID]
