# Include shared CMake helpers
include(${ROOT_DIR}/cmake/module.cmake)

# Resolve project name from current folder
set(AbsolutePathProject ${CMAKE_CURRENT_SOURCE_DIR})
get_filename_component(ProjectName ${AbsolutePathProject} NAME)

# Qt libs
set(QT_LIBRARY_LIST "Core" "Gui" "Widgets")

# Third-party link libs
set(SELF_LIBRARY_LIST_DEBUG "avcodec" "avdevice" "avfilter" "avformat" "avutil" "SDL2d")
set(SELF_LIBRARY_LIST_RELEASE "avcodec" "avdevice" "avfilter" "avformat" "avutil" "SDL2")

# Compile definitions
add_compile_definitions(VIDEOCOMMON_LIBRARY)

# Create target
CreateTarget(${ProjectName} "Dll")
