# UI Guideline

## Formatting

* Indent / align with spaces only
* Indent every level with 4 spaces

Example:

```
{
    name TexNoise
    label "Noise"
    parmtag { spare_category "TexNoise" }

    groupsimple {
        name "VfhTexNoiseMain"
        label "Noise Settings"

        parm {
            name "noiseType"
            type ordinal
            label "Type"
            help "Noise type"
            menu {
                "0" "Noise"
                "1" "Perlin Noise"
                "2" "Inflected Perlin Noise"
                "3" "Marble (With Perlin)"
            }
            default { "0" }
            parmtag { "vray_pluginattr" "noiseType" }
            parmtag { "vray_type" "enum" }
        }
    }

#define VFH_USE_3DMAPPING
#include "vfh_texture_common.ds"
}
```

## Naming

* All labels "Title Case Only"
* No dashes in names (allowed only where needed like "F-Number")
* Abbrevations are in UPPERCASE (e.g. "SSS")
* Don't shorten labels if they are fitting in GUI. Use shorten names only where needed or very common (like "Subdivs" is fine)

## Grouping

* Prefer folder to avoid much scrolling
* Don't use group title in labels. If you are grouping attributes under "Refraction" group just use "Color" as the refraction_color label.

## V-Ray Spare Data

### Parameters

#### Enum (Menu)

* Use `parmtag { "vray_type" "enum" }` for enums
* Use integer keys with the exact values as specified in V-Ray plugin.

### Sockets

* Use macros defined at vfh_texture_defines.ds

```
#include "vfh_texture_defines.ds"

VFH_TEXTURE_OUTPUT(color_a)
```