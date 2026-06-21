# ProjectZelda64 integration build settings.
#
# This file is included by soh/CMakeLists.txt before the soh target is created.
# MSVC treats warnings as errors in this branch. The randomizer save setup currently
# has legacy integer-to-smaller-integer assignments that trigger C4244 on clean
# Release builds, which blocks rebuilding a clean ProjectZelda64 workspace.
if(MSVC)
    add_compile_options(/wd4244)
endif()
