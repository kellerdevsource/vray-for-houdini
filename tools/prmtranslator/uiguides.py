import re


def getBracketSubstr(mystr):
    br = 0
    for i, c in enumerate(mystr):
        if c == '(':
            br += 1
        if c == ')':
            br -= 1
        if br < 0:
            return mystr[:i]

    return mystr


def parseEnum(value):
    value = value.strip()

    items = []
    for val in value.split(";"):
        val = val.strip()
        if not val:
            continue

        e_val = val.split(":")
        assert( len(e_val) > 1 )

        e_val = map(lambda x: x.strip(), e_val)
        # add enum item
        items.append( [ e_val[0], e_val[1], e_val[1] ] )

    return items


def parseStringEnum(value):
    value = value.strip()

    items = []
    for val in value.split(";"):
        val = val.strip()
        if not val:
            continue

        e_val = val.split(":")
        assert( len(e_val) > 0 )

        e_val = map(lambda x: x.strip(), e_val)
        # add enum item
        items.append( e_val[0] )

    return items


def parseValue(value):
    value = value.strip()

    val = value
    try:
        val = int(value)
        return val
    except Exception:
        try:
            val = float(value)
            return val
        except Exception:
            val = value

    return val


def parseCondition(value):
    value = value.strip()

    rex = re.compile("[:.]+")
    vlist = rex.split(value)
    assert( len(vlist) > 0 )
    assert( len(vlist) < 3 )

    parmowner = None
    if len(vlist) > 1:
        parmowner = vlist[0]
        value = vlist[1]
    else:
        value = vlist[0]

    rex = re.compile("(=|!=|<|<=|>|>=)")
    m = rex.search(value)
    if not m:
        return None

    op = m.group(0)
    if op == "=":
        op = "=="

    parm = value[:m.start()]
    condval = parseValue( value[m.end():] )

    conditional = {
                "parm": parm.strip(),
                "op": op,
                "value": condval,
                "parmowner" : parmowner
                }

    return conditional


def parseEnable(value):
    value = value.strip()

    # ignore sub expressions
    value = value.replace("(", "")
    value = value.replace(")", "")

    orcondlist = []
    # split or operator
    for orval in value.split(";"):
        andcondlist = []
        for andval in orval.split(","):
            conditional = parseCondition(andval)
            if conditional:
                andcondlist.append(conditional)

        if andcondlist:
            orcondlist.append(andcondlist)

    return orcondlist


def parseDisplayName(value):
    value = value.strip()
    return value


def parseMinValue(value):
    value = value.strip()

    # ignore sub expressions
    value = value.replace("(", "")
    value = value.replace(")", "")

    rex = re.compile("([+-]?\d+(.\d+)?(e[+-]?\d+)?)", re.I)
    m = rex.search(value)
    if not m:
        return 0

    return parseValue(m.group(0))


def parseMaxValue(value):
    value = value.strip()

    # ignore sub expressions
    value = value.replace("(", "")
    value = value.replace(")", "")

    rex = re.compile("([+-]?\d+(.\d+)?(e[+-]?\d+)?)", re.I)
    m = rex.search(value)
    if not m:
        return 0

    return parseValue(m.group(0))


def parseSoftMinValue(value):
    value = value.strip()

    # ignore sub expressions
    value = value.replace("(", "")
    value = value.replace(")", "")

    rex = re.compile("([+-]?\d+(.\d+)?(e[+-]?\d+)?)", re.I)
    m = rex.search(value)
    if not m:
        return 0

    return parseValue(m.group(0))


def parseSoftMaxValue(value):
    value = value.strip()

    # ignore sub expressions
    value = value.replace("(", "")
    value = value.replace(")", "")

    rex = re.compile("([+-]?\d+(.\d+)?(e[+-]?\d+)?)", re.I)
    m = rex.search(value)
    if not m:
        return 0

    return parseValue(m.group(0))


def parseSpinStep(value):
    value = value.strip()

    # ignore sub expressions
    value = value.replace("(", "")
    value = value.replace(")", "")

    rex = re.compile("([+-]?\d+(.\d+)?(e[+-]?\d+)?)", re.I)
    m = rex.search(value)
    if not m:
        return 0

    return parseValue(m.group(0))


def parseUnits(value):
    value = value.strip()

    # ignore sub expressions
    value = value.replace("(", "")
    value = value.replace(")", "")

    return value


def parseFileAsset(value):
    value = value.strip()

    # ignore sub expressions
    value = value.replace("(", "")
    value = value.replace(")", "")

    return value.split(";")


def parseFileAssetOp(value):
    value = value.strip()

    # ignore sub expressions
    value = value.replace("(", "")
    value = value.replace(")", "")

    return value


def parseAttributes(value):
    value = value.strip()

    # ignore sub expressions
    value = value.replace("(", "")
    value = value.replace(")", "")

    attrid = {
            "objectSet": 1,
            "textureSlot": 2,
            "lightSet": 4,
            }

    return map(lambda x: [x, attrid.get(x, 0)], value.split(";"))


def parseUIGuides(uiGuides):
    ui = {}
    if not uiGuides:
        return ui

    uiGuides = getBracketSubstr(uiGuides)
    while True:
        # special symbols regex [^=(),; ]
        i = uiGuides.find("=")
        if i < 0:
            break
        # split token and rest of the string
        token = uiGuides[:i]
        value = uiGuides[i+1:]

        # normalize token i.e. ignore any special symbols
        token = token.replace("(", "")
        token = token.replace(")", "")
        token = token.replace("=", "")
        token = token.replace(",", "")
        token = token.replace(";", "")
        token = token.replace(" ", "")

        # normalize value i.e. skip everything till the first opening bracket
        j = value.find("(")
        value = value[j+1:]

        if not value:
            break

        # isolate the value string
        tokenval = getBracketSubstr(value)
        # remove parsed (token , tokenval) from uiGuides
        uiGuides = value[ len(tokenval)+1: ]

        if token == "enum":
            ui[ "enum" ]  = parseEnum(tokenval)

        elif token == "string_enum":
            ui[ "string_enum" ] = parseStringEnum(tokenval)

        elif token == "enable":
            ui[ "enabled" ] = parseEnable(tokenval)

        elif token == "displayName":
            ui[ "display_name" ] = parseDisplayName(tokenval)

        elif token == "minValue":
            ui[ "min" ] = parseMinValue(tokenval)

        elif token == "maxValue":
            ui[ "max" ] = parseMaxValue(tokenval)

        elif token == "softMinValue":
            ui[ "soft_min" ] = parseSoftMinValue(tokenval)

        elif token == "softMaxValue":
            ui[ "soft_max" ] = parseSoftMaxValue(tokenval)

        elif token == "spinStep":
            ui[ "spin_step" ] = parseSpinStep(tokenval)

        elif token == "units":
            ui[ "units" ] = parseUnits(tokenval)

        elif token == "fileAsset":
            ui[ "file_extensions" ] = parseFileAsset(tokenval)

        elif token == "fileAssetNames":
            ui[ "file_names" ] = parseFileAsset(tokenval)

        elif token == "fileAssetOp":
            ui[ "file_op" ] = parseFileAssetOp(tokenval)

        elif token == "attributes":
            ui[ "attributes" ] = parseAttributes(tokenval)

        elif token == "startRollout":
            ui[ "rollout" ] = parseDisplayName(tokenval)

        elif token == "startTab":
            ui[ "tab" ] = parseDisplayName(tokenval)

    return ui


def dumpPlugin(vrayPlugin):
    for k, v in vrayPlugin.getMeta().iteritems():
        print("{}::{}".format(vrayPlugin.getName(), k))
        print("-------------")
        print(v["uiGuides"])
        print(parseUIGuides( v["uiGuides"] ))
        print("-------------")
