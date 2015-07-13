import sys
import re
import hou


_skipped_types = {
    'LIST',
    'INT_LIST',
    'FLOAT_LIST',
    'VECTOR_LIST',
    'COLOR_LIST',
    'MAPCHANNEL_LIST',
    'TRANSFORM_LIST',
    'TRANSFORM_TEXTURE',
}

_type_to_parm_func = {
    'BOOL'   : hou.ToggleParmTemplate,
    # 'COLOR'  : bpy.props.FloatVectorProperty,
    # 'ACOLOR' : bpy.props.FloatVectorProperty,
    # 'VECTOR' : bpy.props.FloatVectorProperty,
    # 'ENUM'   : bpy.props.EnumProperty,
    'FLOAT'  : hou.FloatParmTemplate,
    'INT'    : hou.IntParmTemplate,
    # 'STRING' : bpy.props.StringProperty,

    # 'TRANSFORM' : bpy.props.StringProperty,
    # 'MATRIX'    : bpy.props.StringProperty,

    # 'BRDF'     : bpy.props.StringProperty,
    # 'GEOMETRY' : bpy.props.StringProperty,
    # 'MATERIAL' : bpy.props.StringProperty,
    # 'PLUGIN'   : bpy.props.StringProperty,
    # 'UVWGEN'   : bpy.props.StringProperty,

    # 'INT_TEXTURE'   : bpy.props.IntProperty,
    # 'FLOAT_TEXTURE' : bpy.props.FloatProperty,
    # 'TEXTURE'       : bpy.props.FloatVectorProperty,
    # 'VECTOR_TEXTURE' : bpy.props.FloatVectorProperty,

    # 'OUTPUT_COLOR'             : bpy.props.FloatVectorProperty,
    # 'OUTPUT_PLUGIN'            : bpy.props.StringProperty,
    # 'OUTPUT_FLOAT_TEXTURE'     : bpy.props.FloatProperty,
    # 'OUTPUT_TEXTURE'           : bpy.props.FloatVectorProperty,
    # 'OUTPUT_VECTOR_TEXTURE'    : bpy.props.FloatVectorProperty,
    # 'OUTPUT_TRANSFORM_TEXTURE' : bpy.props.FloatVectorProperty,
}


# When there is no name specified for the attribute we could "guess" the name
# from the attribute like: 'dist_near' will become "Dist Near"
#
def getNameFromAttr(attr):
    attr_name = attr.replace("_", " ")
    attr_name = re.sub(r"\B([A-Z])", r" \1", attr_name)

    return attr_name.title()


def add_attribute(propGroup, attrDesc, prefix=None):
    if attrDesc['type'] in _skipped_types:
        return

    # TODO: Widget attributes
    if attrDesc['type'].startswith('WIDGET_'):
        return

    if attrDesc['type'] not in _type_to_parm_func:
        sys.stderr.write("Unimplemented attribute type %s!\n" % attrDesc['type'])
        return

    parm_func = _type_to_parm_func[attrDesc['type']]

    # XXX: Why "dot" is allowed in C++, but is not allowed here?
    parm_name  = "%s_%s" % (prefix, attrDesc['attr']) if prefix else attrDesc['attr']
    parm_label = attrDesc.get('name', getNameFromAttr(attrDesc['attr']))

    parm_args = {}

    if 'default' in attrDesc:
        if attrDesc['type'] in {'INT'}:
            parm_args['default_value'] = [attrDesc['default']]
            parm_args['naming_scheme'] = hou.parmNamingScheme.Base1
            parm_args['num_components'] = 1
            parm_args['min'] = 0
            parm_args['max'] = 1024

        elif attrDesc['type'] in {'FLOAT'}:
            parm_args['default_value'] = [attrDesc['default']]
            parm_args['naming_scheme'] = hou.parmNamingScheme.Base1
            parm_args['num_components'] = 1
            parm_args['min'] = 0.0
            parm_args['max'] = 1024.0

        elif attrDesc['type'] in {'BOOL'}:
            parm_args['default_value'] = attrDesc['default']

        parm_args['disable_when'] = ""

    # attrArgs = {
    #     'attr'        : attrDesc['attr'],
    #     'description' : attrDesc['desc'],
    # }

    # if 'default' in attrDesc:
    #     attrArgs['default'] = attrDesc['default']

    # if 'update' in attrDesc:
    #     attrArgs['update'] = attrDesc['update']

    # defUi = {
    #     'min'      : -1<<20,
    #     'max'      :  1<<20,
    #     'soft_min' : 0,
    #     'soft_max' : 64,
    # }


    if attrDesc['type'] in {'STRING'}:
        pass

    elif attrDesc['type'] in {'COLOR', 'ACOLOR', 'TEXTURE'}:
        # c = attrDesc['default']
        # attrArgs['subtype'] = 'COLOR'
        # attrArgs['default'] = (c[0], c[1], c[2])
        # attrArgs['min'] = 0.0
        # attrArgs['max'] = 1.0
        pass

    elif attrDesc['type'] in {'VECTOR'}:
        # if 'subtype' not in attrDesc:
        #     attrArgs['subtype'] = 'TRANSLATION'
        # attrArgs['precision'] = 3
        pass

    elif attrDesc['type'] in {'FLOAT', 'FLOAT_TEXTURE'}:
        # attrArgs['precision'] = attrDesc.get('precision', 3)
        pass

    elif attrDesc['type'] in {'INT', 'INT_TEXTURE'}:
        pass

    elif attrDesc['type'] in {'TRANSFORM', 'MATRIX'}:
        # Currenlty used as fake string attribute
        # attrArgs['size']    = 16
        # attrArgs['subtype'] = 'MATRIX'
        # attrArgs['default'] = (1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,0)
        # Override default
        # attrArgs['default'] = ""
        pass

    elif attrDesc['type'] in {'ENUM'}:
        # NOTE: JSON parser returns lists but need tuples
        # attrArgs['items'] = (tuple(item) for item in attrDesc['items'])
        pass

    # if 'options' in attrDesc:
    #     options = set()
    #     for opt in attrDesc['options'].split():
    #         options.add(opt)
    #     attrArgs['options'] = options

    # for optionalKey in {'size', 'precision', 'subtype'}:
    #     if optionalKey in attrDesc:
    #         attrArgs[optionalKey] = attrDesc[optionalKey]

    # if attrDesc['type'] in {'INT', 'INT_TEXTURE', 'FLOAT', 'FLOAT_TEXTURE'}:
    #     if 'ui' not in attrDesc:
    #         attrDesc['ui'] = defUi

    #     attrArgs['min'] = attrDesc['ui'].get('min', defUi['min'])
    #     attrArgs['max'] = attrDesc['ui'].get('max', defUi['max'])
    #     attrArgs['soft_min'] = attrDesc['ui'].get('soft_min', attrArgs['min'])
    #     attrArgs['soft_max'] = attrDesc['ui'].get('soft_max', attrArgs['max'])

    propGroup.addParmTemplate(parm_func(parm_name, parm_label, **parm_args))


def add_attributes(propGroup, pluginDesc, prefix=None):
    for attrDesc in pluginDesc['PluginParams']:
        add_attribute(propGroup, attrDesc, prefix)
