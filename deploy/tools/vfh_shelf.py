import json
import pathlib
import os

SHELF_TMPL = open(os.path.join(os.path.dirname(__file__), "vfh_vop.shelf.in"), 'r').read()
TOOL_TMPL  = open(os.path.join(os.path.dirname(__file__), "vfh_tool.in"), 'r').read()
PLUGINS_DESCS = {}

descDirpath = pathlib.Path(os.path.join(os.path.dirname(__file__), "..", "plugins_desc"))


def getPluginsDescs():
    if len(PLUGINS_DESCS) != 0:
        return PLUGINS_DESCS

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

    return PLUGINS_DESCS


def isInContextOBJ(pluginID):
    pluginsDescs = getPluginsDescs()
    pluginDesc = pluginsDescs[pluginID]
    pluginType = pluginDesc['Type']
    if pluginType == "LIGHT":
        return True

    return False


def genShelf_vfh_vop(shelfPath):
    tools = ""
    pluginsDescs = getPluginsDescs()
    for pluginID in sorted(pluginsDescs):
        pluginDesc = pluginsDescs[pluginID]

        name = pluginDesc['Name']
        node_name = name.replace(" ", "").lower()
        for c in ";:@|(":
            node_name = node_name.replace(c, "_")
        node_name = node_name.replace(")", "")

        menu_name = pluginDesc['Type']

        if menu_name == "RENDERCHANNEL":
            menu_name = "Render Channel"
        elif menu_name == "UVWGEN":
            menu_name = "Mapping"

        if menu_name != "BRDF":
            menu_name = menu_name.title()

        tools += TOOL_TMPL.format(
            CONTEXT="vop",
            NET_CONTEXT="VOP",
            OP_CONTEXT="Vop",
            OP_NAME="VRayNode%s" % pluginDesc['ID'],
            OP_NODE_NAME=node_name,
            ITEM_NAME=name,
            ITEM_MENU=menu_name,
        )
        tools += "\n"

    shelf = SHELF_TMPL.format(TOOLS=tools)
    with open(shelfPath, "w") as f:
        f.write(shelf)


def genShelf_vfh_obj(shelfPath):
    tools = ""
    pluginsDescs = getPluginsDescs()
    for pluginID in sorted(pluginsDescs):
        if isInContextOBJ(pluginID):
            pluginDesc = pluginsDescs[pluginID]
            menu_name = "V-Ray"

            tools += TOOL_TMPL.format(
                CONTEXT="object",
                NET_CONTEXT="OBJ",
                OP_CONTEXT="Object",
                OP_NAME="VRayNode%s" % pluginID,
                OP_NODE_NAME=pluginID.lower(),
                ITEM_NAME=pluginID,
                ITEM_MENU=menu_name,
            )
            tools += "\n"

    shelf = SHELF_TMPL.format(TOOLS=tools)
    with open(shelfPath, "w") as f:
        f.write(shelf)


def main():
    getPluginsDescs()
    vopShelfPath = os.path.join(os.path.dirname(__file__), "..", "vfh_vop.shelf")
    genShelf_vfh_vop(vopShelfPath)
    objShelfPath = os.path.join(os.path.dirname(__file__), "..", "vfh_obj.shelf")
    genShelf_vfh_obj(objShelfPath)


if __name__ == "__main__":
    main()