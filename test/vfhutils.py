

def getPluginName(opNode, prefix="", suffix=""):
    return "%s@%s|%s|%s" % (prefix, opNode.name(), opNode.parent().name(), suffix)


def getLightPluginName(objNode):
    return "Light@%s" % (objNode.name())


def getCameraPluginName(objNode):
    return "Camera@%s" % (objNode.name())


def getNodePluginName(objNode):
    return "Node@%s" % (objNode.name())


def findSubNodesByType(opNode, typeName):
    nodeType = opNode.childTypeCategory().nodeTypes()[ typeName ]
    return filter(lambda subNode: subNode.type() == nodeType, opNode.children())
