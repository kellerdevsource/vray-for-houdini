#
# Copyright (c) 2015-2016, Chaos Software Ltd
#
# V-Ray For Houdini
#
# ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
#
# Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
#

set(_phoenix_for_maya_roots "")

if(WIN32)
    set(CGR_PHOENIX_SHARED aurloader.dll)
else()
    set(CGR_PHOENIX_SHARED libaurloader.so)
endif()

set(_maya_versons "2017;2016;2015;2014")
foreach(_maya_version ${_maya_versons})
    if(WIN32)
        set(_phoenix_for_maya_root "C:/Program Files/Chaos Group/Phoenix FD/Maya ${_maya_version} for x64/SDK")
    elseif(APPLE)
        set(_phoenix_for_maya_root "")
    else()
        set(_phoenix_for_maya_root "/usr/ChaosGroup/PhoenixFD/Maya${_maya_version}-x64/SDK")
    endif()

    if(EXISTS ${_phoenix_for_maya_root})
        list(APPEND _phoenix_for_maya_roots
            ${_phoenix_for_maya_root}
        )
    endif()
endforeach()

find_path(Phoenix_INCLUDES include/aurinterface.h
    ${_phoenix_for_maya_roots}
)

find_path(Phoenix_LIBRARIES lib/${CGR_PHOENIX_SHARED}
    ${_phoenix_for_maya_roots}
)

if(Phoenix_INCLUDES AND Phoenix_LIBRARIES)
    set(Phoenix_INCLUDES  ${Phoenix_INCLUDES}/include)
    set(Phoenix_LIBRARIES ${Phoenix_LIBRARIES}/lib)
    set(Phoenix_FOUND TRUE)
endif()
