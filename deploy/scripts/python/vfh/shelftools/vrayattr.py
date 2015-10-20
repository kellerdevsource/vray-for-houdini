import hou


def applyDisplacementToSelection(vrayDisplType):
    vrayTabName = "V-Ray"
    vrayDisplacementParm = "vray_displacement"
    vrayDisplacementParmLabel = "V-Ray Displacement"

    geometryList = filter(lambda item: item.type().category().typeName() == 'OBJ' and item.type().name() == 'geo', hou.selectedNodes())
    for node in geometryList:
        ptg = node.parmTemplateGroup()
        prmDisplacement = ptg.find(vrayDisplacementParm)
        if not prmDisplacement:
            vrayFolder = ptg.findFolder(vrayTabName)
            if not vrayFolder:
                vrayFolder = hou.FolderParmTemplate("vray", vrayTabName)
                ptg.append(vrayFolder)

            parm_args = {}
            parm_args["num_components"] = 1
            parm_args["string_type"] = hou.stringParmType.NodeReference
            parm_args["tags"] = {'spare_category': 'vray', 'opfilter': '!!SHOP!!', 'oprelative': '.'}
            prmDisplacement = hou.StringParmTemplate(vrayDisplacementParm, vrayDisplacementParmLabel, **parm_args)
            ptg.appendToFolder(vrayFolder, prmDisplacement)
            node.setParmTemplateGroup(ptg)
