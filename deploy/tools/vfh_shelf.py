import json
import pathlib
import os

SHELF_TMPL = open(os.path.join(os.path.dirname(__file__), "vfh_vop.shelf.in"), 'r').read()
TOOL_TMPL  = open(os.path.join(os.path.dirname(__file__), "vfh_tool.in"), 'r').read()

descDirpath = pathlib.Path(os.path.join(os.path.dirname(__file__), "..", "plugins_desc"))

PLUGINS_DESCS = {}

for filePath in descDirpath.glob("*/*.json"):
    pluginDesc = json.loads(filePath.open().read())

    pluginID     = pluginDesc.get('ID')
    pluginParams = pluginDesc.get('Parameters')
    pluginName   = pluginDesc.get('Name')
    pluginType   = pluginDesc.get('Type')
    plugiIDDesc  = pluginDesc.get('Description', "")
    plguinWidget = pluginDesc.get('Widget', {})

    PLUGINS_DESCS[pluginID] = {
        'ID'         : pluginID,
        'Name'       : pluginName,
        'Type'       : pluginType,
        'Parameters' : pluginParams,
        'Widget'     : plguinWidget,
    }

tools = ""

for pluginID in sorted(PLUGINS_DESCS):
    pluginDesc = PLUGINS_DESCS[pluginID]

    name = pluginDesc['Name']
    node_name = name.replace(" ", "").lower()

    menu_name = pluginDesc['Type']

    if menu_name == "RENDERCHANNEL":
        menu_name = "Render Channel"
    elif menu_name == "UVWGEN":
        menu_name = "Mapping"

    if menu_name != "BRDF":
        menu_name = menu_name.title()

    tools += TOOL_TMPL.format(
        CONTEXT="vop",
        OP_CONTEXT="Vop",
        OP_NAME="VRayNode%s" % pluginDesc['ID'],
        OP_NODE_NAME=node_name,
        ITEM_NAME=name,
        ITEM_MENU=menu_name,
    )
    tools += "\n"

shelf = SHELF_TMPL.format(TOOLS=tools)

print(shelf)
