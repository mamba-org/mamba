vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO openSUSE/libsolv
    REF 0.7.19
    SHA512 dfcebea314d549a15bf5af19db775ff1b1850bfc6fb147b68fe094e43bf1541fcfe22d2f6c6607a6393e01905a086cea606d0b25da2e3ce376d100c4ef4fee00
    HEAD_REF master
    PATCHES
        win_static_build.patch
        conda_variant_priorization.patch
        add_strict_repo_prio_rule.patch
        memcpy_to_memmove.patch
)

string(COMPARE EQUAL "${VCPKG_LIBRARY_LINKAGE}" "dynamic" BUILD_DYNAMIC_LIBS)
string(COMPARE EQUAL "${VCPKG_LIBRARY_LINKAGE}" "static" BUILD_STATIC_LIBS)

if (NOT BUILD_DYNAMIC_LIBS)
    set(DISABLE_SHARED ON)
else()
    set(DISABLE_SHARED OFF)
endif()

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    conda   ENABLE_CONDA
)

if(WIN32)
    list(APPEND FEATURE_OPTIONS "-DWITHOUT_COOKIEOPEN=ON")
endif()

vcpkg_configure_cmake(
    SOURCE_PATH ${SOURCE_PATH}
    PREFER_NINJA
    OPTIONS
        ${FEATURE_OPTIONS}
        -DDISABLE_SHARED=${DISABLE_SHARED}
        -DENABLE_STATIC=${BUILD_STATIC_LIBS}
        -DMULTI_SEMANTICS=ON
        -DBUILD_EXAMPLE_PROGRAMS=OFF
        ..
)


vcpkg_install_cmake()
# vcpkg_fixup_cmake_targets()
# vcpkg_copy_pdbs()

if(VCPKG_LIBRARY_LINKAGE STREQUAL "static")
    file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/bin" "${CURRENT_PACKAGES_DIR}/debug/bin")
endif()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

file(INSTALL ${SOURCE_PATH}/LICENSE.BSD DESTINATION ${CURRENT_PACKAGES_DIR}/share/libsolv RENAME copyright)

vcpkg_test_cmake(PACKAGE_NAME libsolv)
