# Temporary workaround until resolvo-cpp static libraries are published on conda-forge. resolvo-cpp
# 0.3.0 on Windows installs Library/bin/resolvo_cpp.dll and Library/lib/resolvo_cpp.lib, but
# ResolvoConfig.cmake still references the 0.2.x layout (Library/lib/resolvo_cpp.dll and
# resolvo_cpp.dll.lib).

if(NOT WIN32 OR NOT TARGET resolvo_cpp-shared)
    return()
endif()

if(DEFINED ENV{LIBRARY_PREFIX})
    set(_resolvo_prefix "$ENV{LIBRARY_PREFIX}")
elseif(DEFINED ENV{CONDA_PREFIX})
    set(_resolvo_prefix "$ENV{CONDA_PREFIX}/Library")
else()
    get_target_property(_resolvo_implib resolvo_cpp-shared IMPORTED_IMPLIB)
    if(_resolvo_implib)
        get_filename_component(_resolvo_prefix "${_resolvo_implib}" DIRECTORY)
        get_filename_component(_resolvo_prefix "${_resolvo_prefix}" DIRECTORY)
    endif()
endif()

if(NOT _resolvo_prefix)
    message(WARNING "Could not determine resolvo-cpp prefix for Windows import fix")
    return()
endif()

set(_resolvo_dll "${_resolvo_prefix}/bin/resolvo_cpp.dll")
set(_resolvo_lib "${_resolvo_prefix}/lib/resolvo_cpp.lib")

if(EXISTS "${_resolvo_lib}" AND EXISTS "${_resolvo_dll}")
    set_target_properties(
        resolvo_cpp-shared
        PROPERTIES IMPORTED_IMPLIB "${_resolvo_lib}" IMPORTED_LOCATION "${_resolvo_dll}"
    )
    message(STATUS "Adjusted Resolvo Windows import lib to ${_resolvo_lib}")
endif()

unset(_resolvo_prefix)
unset(_resolvo_dll)
unset(_resolvo_lib)
