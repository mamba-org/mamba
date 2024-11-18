if(APPLE)
    # It seems that there are some flags added to CXXFLAGS during `conda-build`. It might be a bug
    # in https://github.com/conda-forge/clang-compiler-activation-feedstock. One way to fix this is
    # to turn these errors to warnings or silence them (using `-Wno-unused-command-line-argument`
    # and `-Wno-ignored-optimization-argument`)

    # But we will go another way: we will remove wrong options from CMAKE_CXX_FLAGS
    set(
        WRONG_OPTIONS
        "-framework CoreFoundation"
        "-framework CoreServices"
        "-framework Security"
        "-framework Kerberos"
        "-fno-merge-constants"
    )
    foreach(WRONG_OPT ${WRONG_OPTIONS})
        string(REPLACE "${WRONG_OPT}" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    endforeach(WRONG_OPT)
endif()
