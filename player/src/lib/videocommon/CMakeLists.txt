# 引入外部函数
include(${ROOT_DIR}/cmake/module.cmake)

# 获取绝对路径
set(AbsolutePathProject ${CMAKE_CURRENT_SOURCE_DIR})
get_filename_component(ProjectName ${AbsolutePathProject} NAME)
 
set(QT_LIBRARY_LIST "Core" "Gui" "Widgets" "Network" "Concurrent")

set(SELF_LIBRARY_LIST_DEBUG "Ws2_32"  "avcodec" "avdevice" "avfilter" "avformat" "avutil" "swscale" "swresample" "SDL2")

set(SELF_LIBRARY_LIST_RELEASE "Ws2_32"  "avcodec" "avdevice" "avfilter" "avformat" "avutil" "swscale" "swresample"  "SDL2")

add_compile_definitions(VIDEOCOMMON_LIBRARY)
 
# 创建项目 
CreateTarget(${ProjectName} "Dll")    
 