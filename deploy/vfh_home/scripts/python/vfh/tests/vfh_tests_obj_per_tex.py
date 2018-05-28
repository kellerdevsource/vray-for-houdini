#
# Copyright (c) 2015-2018, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

def main():
    import hou
    import math

    PREFIX = "VRayNodeTex"

    MAT = hou.node("/mat")
    OBJ = hou.node("/obj")

    skipTexTags = (
        "User", "Particle", "Normal", "Combine", "RGB",
        "Ptex", "Switch", "OCIO", "OSL", "Correct", "Condition",
        "Output", "Mask", "Mix", "Color", "Clamp", "Layered", "Range",
        "Invert", "Float", "Int", "UVW", "Sampler", "Sky", "Product",
        "Distance", "Multi", "ICC", "Lut", "Luminance", "Blend", "Bezier",
        "Remap", "CompMax",
    )

    skipTexSuffix = ("Op", "ToColor", "ToFloat", "ToInt")

    def vrayVopFilter(vopType):
        if not vopType.startswith(PREFIX):
            return False
        if vopType.endswith(skipTexSuffix):
            return False
        if any(skipTag in vopType for skipTag in skipTexTags):
            return False
        return True

    def getCreate(network, nodeType, nodeName):
        node = network.node(nodeName)
        if not node:
            node = network.createNode(nodeType, node_name=nodeName)
        return node

    def getCreateEmpty(network, nodeType, nodeName):
        node = getCreate(network, nodeType, nodeName)
        node.deleteItems(node.children())
        return node

    def generateTextures(objNode):
        print("OBJ: \"%s\"" % (objNode.path()))

        vopTypes = hou.vopNodeTypeCategory().nodeTypes()
        vrayVopTypes = sorted([vopType for vopType in vopTypes if vrayVopFilter(vopType)])

        # FOR TESTS
        # vrayVopTypes = vrayVopTypes[:2]

        bbox = objNode.renderNode().geometry().boundingBox()
        bboxWidth = bbox.sizevec().x()
        bboxDepth = bbox.sizevec().z()

        col = 0
        row = 0
        bboxOffsetPerc = 2.0
        offsetX = bboxWidth * bboxOffsetPerc
        offsetY = bboxDepth * bboxOffsetPerc
        cellW = bboxWidth + offsetX
        cellD = bboxDepth + offsetY
        maxColCount = int(math.sqrt(len(vrayVopTypes)))

        matNet = getCreateEmpty(MAT, "vray_material", "TEXTURES_ALL_PER_FRAME")
        objNet = getCreateEmpty(OBJ, "subnet", "TEXTURES_ALL_PER_FRAME")

        fontMat = getCreate(MAT, "VRayNodeBRDFVRayMtl", "font")
        fontMat.setMaterialFlag(True)
        fontMat.parm("diffuser").set(0.05)
        fontMat.parm("diffuseg").set(0.05)
        fontMat.parm("diffuseb").set(0.05)

        uvw = matNet.createNode("VRayNodeUVWGenMayaPlace2dTexture", node_name="channel_uv")
        uvw.parm("repeat_u").set(3)
        uvw.parm("repeat_v").set(3)

        for texType in vrayVopTypes:
            tex = None
            texName = texType.replace(PREFIX, "")

            try:
                mtl = matNet.createNode("VRayNodeBRDFVRayMtl", node_name="mtl%s" % texName)

                # Attach texture to "diffuse".
                tex = mtl.createInputNode(0, texType, node_name=texName)

                uvwGenIndex = tex.inputIndex("uvwgen")
                if uvwGenIndex >= 0:
                    tex.setNamedInput("uvwgen", uvw, 0)
            except:
                print("Failed: \"%s\"" % (texType))

            if tex:
                objForTex = getCreateEmpty(objNet, "geo", "obj%s" % texName)

                # Copy source geo.
                hou.copyNodesTo(objNode.children(), objForTex)

                testMtl = objForTex.createNode("material")
                testMtl.setNextInput(objForTex.renderNode())

                # Assign material
                objForTex.parm("shop_materialpath").set("")
                testMtl.parm("shop_materialpath1").set(mtl.path())

                # Add text
                font = objForTex.createNode("font")
                font.parm("file").set("Consolas")
                font.parm("text").set(texName)
                font.parm("fontsize").set(0.2)

                fontDivide = objForTex.createNode("divide")
                fontDivide.setNextInput(font)

                fontExt = objForTex.createNode("polyextrude")
                fontExt.parm("dist").set(0.02)
                fontExt.setNextInput(fontDivide)

                fontTm = objForTex.createNode("xform")
                fontTm.setNextInput(fontExt)
                fontTm.parm("tx").set(0.3)
                fontTm.parm("rx").set(-90.0)
                fontTm.parm("ry").set(90.0)

                fontMtl = objForTex.createNode("material")
                fontMtl.setNextInput(fontTm)
                fontMtl.parm("shop_materialpath1").set(fontMat.path())

                merge = objForTex.createNode("merge")
                merge.setNextInput(testMtl)
                merge.setNextInput(fontMtl)
                merge.setDisplayFlag(True)
                merge.setRenderFlag(True)

                objForTex.layoutChildren()

                pos = (row * cellD, 0, col * cellW)

                tm = hou.hmath.buildTranslate(pos)
                objForTex.setWorldTransform(tm)

                if col == maxColCount - 1:
                    col = 0
                    row += 1
                else:
                    col += 1

        matNet.layoutChildren()
        objNet.layoutChildren()

    selection = hou.selectedNodes()
    if not selection:
        hou.ui.displayMessage("Select OBJ node!")
    else:
        if len(selection) > 1:
            print("Multiple OBJ nodes are selected. Using the first one...")

        objNode = selection[0]
        generateTextures(objNode)
        objNode.setCurrent(True, clear_all_selected=True)

if __name__ == '__main__':
    import sys
    sys.path.append("C:\\Program Files\\Side Effects Software\\Houdini 16.5.439\\houdini\\python2.7libs")

    try:
        import hrpyc
        connection, hou = hrpyc.import_remote_module()
    except:
        print "Failed connecting to Houdini!"
        sys.exit(1)
