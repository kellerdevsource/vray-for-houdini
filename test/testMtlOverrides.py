import unittest
import os

import houmodule as hou
import vfhutils
import vray

class TestMtlOverrides(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        hou.hipFile.load("./scenes/vfh_mtloverrides.hipnc", suppress_save_prompt=True, ignore_load_warnings=False)
        cls.vrayRop = hou.node("/out/vray_renderer1")

        prm = cls.vrayRop.parm("render_export_filepath")
        scenePath = prm.evalAsString()

        prm = cls.vrayRop.parm("render_export_mode")
        prm.set("Export")

        cls.vrayRop.render()

        cls.vrayRenderer = vray.VRayRenderer()
        cls.vrayRenderer.load(scenePath)


    @classmethod
    def tearDownClass(cls):
        hou.hipFile.clear(suppress_save_prompt=True)
        cls.vrayRenderer.close()


    def test_exportVRayMaterialWithMapChannelOverrides(self):
        box = hou.node("/obj/box_mtloverride1")
        self.assertIsNotNone( box )

        plgNode = self.vrayRenderer.plugins[ vfhutils.getNodePluginName(box) ]
        self.assertIsNotNone( plgNode )

        boxGeo = box.renderNode()
        self.assertIsNotNone( boxGeo )

        plgGeo = self.vrayRenderer.plugins[ vfhutils.getPluginName(boxGeo, "Geom") ]
        self.assertIsNotNone( plgGeo )
        self.assertEqual(plgNode["geometry"], plgGeo)

        plgMtl = self.vrayRenderer.plugins[ vfhutils.getPluginName(boxGeo, "Mtl") ]
        self.assertEqual(plgNode["material"], plgMtl)
        self.assertEqual(plgMtl.getType(), 'MtlMulti')

        # filter all map channels representing material override
        gdp = boxGeo.geometry()
        shopPathAttr = gdp.findPrimAttrib("shop_materialpath")
        self.assertIsNotNone( shopPathAttr )
        mtlOverrideAttr = gdp.findPrimAttrib("material_override")
        self.assertIsNotNone( mtlOverrideAttr )

        mtlOverrideChannelNames = set()
        mtlOverrides = map( lambda x: eval(x) if x else None, mtlOverrideAttr.strings() )
        for mtlOverride in mtlOverrides:
            if not mtlOverride:
                continue

            for channel in mtlOverride.keys():
                mtlOverrideChannelNames.add( channel )

        shopOverrides = {}
        for shopPath in shopPathAttr.strings():
            if not shopPath:
                continue

            shopNode = hou.node(shopPath)
            self.assertIsNotNone( shopNode )

            if not shopPath in shopOverrides:
                shopOverrides[ shopPath ] = set()

            for channel in mtlOverrideChannelNames:
                prm = shopNode.parm(channel)
                if prm:
                    shopOverrides[ shopPath ].add( prm.parmTemplate().name() )

        # check that all map channels exist on the geometry plugin
        plgMapChannelsNames = plgGeo["map_channels_names"]
        self.assertIsNotNone( plgMapChannelsNames )
        self.assertTrue( len(plgMapChannelsNames) > 0 )

        for shopPath, shopMapChannelNames in shopOverrides.iteritems():
            shopNode = hou.node(shopPath)
            self.assertIsNotNone( shopNode )

            for mapChannelName in shopMapChannelNames:
                self.assertIn( mapChannelName, plgMapChannelsNames )

        # check that for all VOPs we have corresponding plugin
        for shopPath in shopOverrides.keys():
            shopNode = hou.node(shopPath)
            self.assertIsNotNone( shopNode )

            mtlOut = vfhutils.findSubNodesByType(shopNode, "vray_material_output")[0]
            inpCon = mtlOut.inputConnectors()[ mtlOut.inputIndex("Material") ]
            inpCon = inpCon[0]
            vopNode = inpCon.inputNode()

            self.checkPluginExistsForVOP(vopNode, box)

            plgVop = self.vrayRenderer.plugins[ vfhutils.getPluginName( vopNode, "", box.name() ) ]
            if inpCon.outputDataType() == 'bsdf' :
                plg = self.vrayRenderer.plugins[ vfhutils.getPluginName(vopNode, "Mtl", box.name()) ]
                self.assertIsNotNone( plg )
                self.assertEqual( plg.getType(), "MtlSingleBRDF" )
                self.assertIn( plg, plgMtl["mtls_list"] )
                self.assertEqual( plg["brdf"], plgVop )
            else :
                self.assertIn( plgVop, plgMtl["mtls_list"] )

        # check that plugins for map channel overrides exist
        for shopPath, shopMapChannelNames in shopOverrides.iteritems():
            shopNode = hou.node(shopPath)
            self.assertIsNotNone( shopNode )

            for prm in shopNode.parmsReferencingThis():
                if prm.node().parent() != shopNode:
                    continue

                vopNode = prm.node()
                vopPrmName = prm.parmTemplate().name()
                shopPrmName = prm.getReferencedParm().parmTemplate().name()
                inpCon = vopNode.inputConnectors()[ vopNode.inputIndex(vopPrmName) ]
                # vop inputs always take precedence over overrides
                # so skip if the node parm has corresponding connected input
                if inpCon:
                    plg = self.vrayRenderer.plugins[ vfhutils.getPluginName( vopNode, vopPrmName, box.name() ) ]
                    self.assertIsNone( plg )
                    continue

                if not shopPrmName in shopMapChannelNames:
                    continue

                plgVop = self.vrayRenderer.plugins[ vfhutils.getPluginName( vopNode, "", box.name() ) ]
                self.assertIsNotNone( plgVop )

                plg = self.vrayRenderer.plugins[ vfhutils.getPluginName( vopNode, vopPrmName, box.name() ) ]
                self.assertIsNotNone( plg )

                self.assertEqual( plgVop[ vopPrmName ], plg )


    def test_exportVRayMaterialWithObjectOverrides(self):
        box2 = hou.node("/obj/box_justbox2")
        self.assertIsNotNone( box2 )

        plgNode2 = self.vrayRenderer.plugins[ vfhutils.getNodePluginName(box2) ]
        self.assertIsNotNone( plgNode2 )

        plgMtl = plgNode2["material"]
        self.assertIsNotNone( plgMtl )

        shopNode = hou.node(box2.parm("shop_materialpath").evalAsString())
        self.assertIsNotNone( shopNode )

        mtlOut = vfhutils.findSubNodesByType(shopNode, "vray_material_output")[0]
        inpCon = mtlOut.inputConnectors()[ mtlOut.inputIndex("Material") ]
        inpCon = inpCon[0]
        vopNode = inpCon.inputNode()

        plgVop = self.vrayRenderer.plugins[ vfhutils.getPluginName(vopNode, "", box2.name()) ]
        self.assertIsNotNone( plgVop )

        if inpCon.outputDataType() == 'bsdf' :
            plgMtl1 = self.vrayRenderer.plugins[ vfhutils.getPluginName(vopNode, "Mtl", box2.name()) ]
            self.assertIsNotNone( plgMtl1 )
            self.assertEqual( plgMtl1.getType(), "MtlSingleBRDF" )
            self.assertEqual( plgMtl1["brdf"], plgVop )
            self.assertEqual( plgMtl1, plgMtl )
        else :
            self.assertEqual( plgVop, plgMtl )

        vopQueue = [ vopNode ]
        while len(vopQueue):
            node = vopQueue.pop(0)

            plgVop = self.vrayRenderer.plugins[ vfhutils.getPluginName( node, "", box2.name() ) ]
            self.assertIsNotNone( plgVop )

            for inpCon in node.inputConnections():
                vopQueue.append( inpCon.inputNode() )

        for objPrm in box2.parmsInFolder(("Material",)):
            shopPrm = shopNode.parm(objPrm.name())
            if shopPrm:
                for prm in shopPrm.parmsReferencingThis():
                    if prm.node().parent() == shopNode:
                        vopNode = prm.node()
                        vopPrmName = prm.parmTemplate().name()

                        plgVop = self.vrayRenderer.plugins[ vfhutils.getPluginName(vopNode, "", box2.name()) ]
                        self.assertIsNotNone( plgVop )
                        self.assertIsNotNone( plgVop[ vopPrmName ] )

                        # vop inputs always take precedence over overrides
                        # so skip if the node parm has corresponding connected input
                        inpCon = vopNode.inputConnectors()[ vopNode.inputIndex(vopPrmName) ]
                        if inpCon:
                            self.assertIsInstance( plgVop[ vopPrmName ], vray.Plugin )
                            continue

                        print shopNode.name(), vopNode.name(), vopPrmName

                        if prm.parmTemplate().numComponents() <= 1:
                            self.assertEqual( plgVop[ vopPrmName ] , prm.eval() )

                        elif prm.componentIndex() < len(plgVop[ vopPrmName ]):
                            self.assertEqual( plgVop[ vopPrmName ][ prm.componentIndex() ] , prm.eval() )


    def test_exportVRayMaterialWithAndWithNoOverrides(self):
        box1 = hou.node("/obj/box_mtloverride1")
        self.assertIsNotNone( box1 )

        plgNode1 = self.vrayRenderer.plugins[ vfhutils.getNodePluginName(box1) ]
        self.assertIsNotNone( plgNode1 )
        self.assertIsNotNone( plgNode1["material"] )

        box2 = hou.node("/obj/box_mtloverride2")
        self.assertIsNotNone( box2 )

        plgNode2 = self.vrayRenderer.plugins[ vfhutils.getNodePluginName(box2) ]
        self.assertIsNotNone( plgNode2 )
        self.assertIsNotNone( plgNode2["material"] )

        shopNode = hou.node(box2.parm("shop_materialpath").evalAsString())
        self.assertIsNotNone( shopNode )

        mtlOut = vfhutils.findSubNodesByType(shopNode, "vray_material_output")[0]
        inpCon = mtlOut.inputConnectors()[ mtlOut.inputIndex("Material") ]
        inpCon = inpCon[0]
        vopNode = inpCon.inputNode()

        plgVopBox1 = self.vrayRenderer.plugins[ vfhutils.getPluginName( vopNode, "", box1.name() ) ]
        self.assertIsNotNone( plgVopBox1 )
        plgVopBox2 = self.vrayRenderer.plugins[ vfhutils.getPluginName( vopNode ) ]
        self.assertIsNotNone( plgVopBox2 )

        if inpCon.outputDataType() == 'bsdf' :
            plgMtlBox1 = self.vrayRenderer.plugins[ vfhutils.getPluginName(vopNode, "Mtl", box1.name()) ]
            self.assertIsNotNone( plgMtlBox1 )
            self.assertEqual( plgMtlBox1.getType(), "MtlSingleBRDF" )
            self.assertEqual( plgMtlBox1["brdf"], plgVopBox1 )
            self.assertIn( plgMtlBox1, plgNode1["material"]["mtls_list"] )

            plgMtlBox2 = self.vrayRenderer.plugins[ vfhutils.getPluginName(vopNode, "Mtl", "") ]
            self.assertIsNotNone( plgMtlBox2 )
            self.assertEqual( plgMtlBox2.getType(), "MtlSingleBRDF" )
            self.assertEqual( plgMtlBox2["brdf"], plgVopBox2 )
            self.assertEqual( plgMtlBox2, plgNode2["material"] )
        else :
            self.assertIn( plgVopBox1, plgNode1["material"]["mtls_list"] )
            self.assertEqual( plgVopBox2, plgNode2["material"] )

        vopQueue = [ vopNode ]
        while len(vopQueue):
            node = vopQueue.pop(0)

            plgNodeBox1 = self.vrayRenderer.plugins[ vfhutils.getPluginName( node, "", box1.name() ) ]
            self.assertIsNotNone( plgNodeBox1 )
            plgNodeBox2 = self.vrayRenderer.plugins[ vfhutils.getPluginName( node, "", ) ]
            self.assertIsNotNone( plgNodeBox2 )
            self.assertNotEqual( plgNodeBox1, plgNodeBox2 )

            for inpCon in node.inputConnections():
                vopQueue.append( inpCon.inputNode() )


    def test_exportVRayMaterialWithNoOverridesOnlyOnce(self):
        box2 = hou.node("/obj/box_mtloverride2")
        self.assertIsNotNone( box2 )

        plgNode2 = self.vrayRenderer.plugins[ vfhutils.getNodePluginName(box2) ]
        self.assertIsNotNone( plgNode2 )
        self.assertIsNotNone( plgNode2["material"] )

        shopNode = hou.node(box2.parm("shop_materialpath").evalAsString())
        self.assertIsNotNone( shopNode )

        box3 = hou.node("/obj/box_mtloverride3")
        self.assertIsNotNone( box3 )

        plgNode3 = self.vrayRenderer.plugins[ vfhutils.getNodePluginName(box3) ]
        self.assertIsNotNone( plgNode3 )
        self.assertIsNotNone( plgNode3["material"] )

        self.assertEqual(box2.parm("shop_materialpath").evalAsString(), box3.parm("shop_materialpath").evalAsString())
        self.assertEqual(plgNode2["material"], plgNode3["material"])

        plgMtl = plgNode2["material"]

        mtlOut = vfhutils.findSubNodesByType(shopNode, "vray_material_output")[0]
        inpCon = mtlOut.inputConnectors()[ mtlOut.inputIndex("Material") ]
        inpCon = inpCon[0]
        vopNode = inpCon.inputNode()

        plgVop = self.vrayRenderer.plugins[ vfhutils.getPluginName( vopNode ) ]
        self.assertIsNotNone(plgVop)

        if inpCon.outputDataType() == 'bsdf' :
            self.assertIsNone( self.vrayRenderer.plugins[ vfhutils.getPluginName( vopNode, "Mtl", box2.name() ) ] )
            self.assertIsNone( self.vrayRenderer.plugins[ vfhutils.getPluginName( vopNode, "Mtl", box3.name() ) ] )

            plgMtl1 = self.vrayRenderer.plugins[ vfhutils.getPluginName(vopNode, "Mtl", "") ]
            self.assertIsNotNone( plgMtl1 )
            self.assertEqual( plgMtl1.getType(), "MtlSingleBRDF" )
            self.assertEqual( plgMtl1["brdf"], plgVop )
            self.assertEqual( plgMtl1, plgMtl )

        else :
            self.assertEqual( plgVop, plgMtl )

        vopQueue = [ vopNode ]
        while len(vopQueue):
            node = vopQueue.pop(0)

            self.assertIsNone( self.vrayRenderer.plugins[ vfhutils.getPluginName( node, "", box2.name() ) ] )
            self.assertIsNone( self.vrayRenderer.plugins[ vfhutils.getPluginName( node, "", box3.name() ) ] )
            self.assertIsNotNone( self.vrayRenderer.plugins[ vfhutils.getPluginName( vopNode ) ] )

            for inpCon in node.inputConnections():
                vopQueue.append( inpCon.inputNode() )


    def checkPluginExistsForVOP(self, vopNode, objNode, recursive = True):
        self.assertIsNotNone( vopNode )

        pluginName = vfhutils.getPluginName( vopNode, "", objNode.name() if objNode else "" )
        plgVop = self.vrayRenderer.plugins[ pluginName ]
        self.assertIsNotNone( plgVop )

        if not recursive:
            return

        for inpCon in vopNode.inputConnections():
            inpVop = inpCon.inputNode()
            self.checkPluginExistsForVOP(inpVop, objNode, recursive)
