#
# Copyright (c) 2015, Chaos Software Ltd
#
# V-Ray For Houdini
#
# Andrei Izrantcev <andrei.izrantcev@chaosgroup.com>
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

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
    'ENUM'   : hou.MenuParmTemplate,
    'FLOAT'  : hou.FloatParmTemplate,
    'INT'    : hou.IntParmTemplate,
    'COLOR'  : hou.FloatParmTemplate,
    'ACOLOR' : hou.FloatParmTemplate,
    'VECTOR' : hou.FloatParmTemplate,
    'STRING' : hou.StringParmTemplate,
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

    if attrDesc['type'] in {'BOOL'}:
        # BOOL must have 'default' in the description
        parm_args['default_value'] = attrDesc['default']

    elif attrDesc['type'] in {'STRING'}:
        string_type = hou.stringParmType.Regular
        file_type   = hou.fileType.Any

        if 'subtype' in attrDesc:
            str_subtype = attrDesc['subtype']
            if str_subtype == 'FILE_PATH':
                string_type = hou.stringParmType.FileReference
                file_type   = hou.fileType.Any
            elif str_subtype == 'DIR_PATH':
                string_type = hou.stringParmType.FileReference
                file_type   = hou.fileType.Directory

        parm_args['string_type'] = string_type
        parm_args['file_type']   = file_type
        parm_args['num_components'] = 1

    elif attrDesc['type'] in {'COLOR', 'ACOLOR', 'TEXTURE'}:
        c = attrDesc['default']

        parm_args['naming_scheme'] = hou.parmNamingScheme.RGBA
        parm_args['default_value']  = c
        parm_args['num_components'] = len(c)

    elif attrDesc['type'] in {'VECTOR'}:
        v = attrDesc['default']

        parm_args['naming_scheme'] = hou.parmNamingScheme.XYZW
        parm_args['default_value']  = v
        parm_args['num_components'] = len(v)

    elif attrDesc['type'] in {'FLOAT', 'FLOAT_TEXTURE'}:
        if 'default' in attrDesc:
            parm_args['default_value'] = [attrDesc['default']]
        parm_args['naming_scheme'] = hou.parmNamingScheme.Base1
        parm_args['num_components'] = 1

    elif attrDesc['type'] in {'INT', 'INT_TEXTURE'}:
        if 'default' in attrDesc:
            parm_args['default_value'] = [attrDesc['default']]
        parm_args['naming_scheme'] = hou.parmNamingScheme.Base1
        parm_args['num_components'] = 1

    elif attrDesc['type'] in {'TRANSFORM', 'MATRIX'}:
        pass

    elif attrDesc['type'] in {'ENUM'}:
        parm_args['menu_items']  = [item[0] for item in attrDesc['items']]
        parm_args['menu_labels'] = [item[1] for item in attrDesc['items']]

    # TODO: Parse widget and extract active conditions
    # Active State
    # parm_args['disable_when'] = ""

    if attrDesc['type'] in {'INT', 'INT_TEXTURE', 'FLOAT', 'FLOAT_TEXTURE'}:
        if 'ui' in attrDesc:
            ui_desc = attrDesc['ui']

            # Use soft bounds and allow manual override
            if 'soft_min' in ui_desc:
                parm_args['min'] = ui_desc['soft_min']
            if 'soft_max' in ui_desc:
                parm_args['max'] = ui_desc['soft_max']

            parm_args['min_is_strict'] = False
            parm_args['max_is_strict'] = False

    propGroup.addParmTemplate(parm_func(parm_name, parm_label, **parm_args))


def add_attributes(propGroup, pluginDesc, prefix=None):
    # TODO: Parse widget and insert parameters in widget defined order

    for attrDesc in pluginDesc['PluginParams']:
        add_attribute(propGroup, attrDesc, prefix)
