# 引入外部函数
include(${ROOT_DIR}/cmake/module.cmake)

# 获取绝对路径
set(AbsolutePathProject ${CMAKE_CURRENT_SOURCE_DIR})
get_filename_component(ProjectName ${AbsolutePathProject} NAME)

include_directories(../../lib/BasicUsageEnvironment/include)
include_directories(../../lib/groupsock/include)
include_directories(../../lib/liveMedia/include)
include_directories(../../lib/UsageEnvironment/include)

set(QT_LIBRARY_LIST "Core" "Gui" "Widgets" "Network" "Concurrent")

set(SELF_LIBRARY_LIST_DEBUG "BasicUsageEnvironmentd" "groupsockd"  "Ws2_32" 
"UsageEnvironmentd" "liveMediad" "avcodec" "avdevice" "avfilter" "avformat" "avutil" "swscale" "swresample" "SDL2")

set(SELF_LIBRARY_LIST_RELEASE "BasicUsageEnvironment" "groupsock"  "Ws2_32" 
"UsageEnvironment" "liveMedia"  "avcodec" "avdevice" "avfilter" "avformat" "avutil" "swscale" "swresample"  "SDL2")

add_compile_definitions(VIDEOCOMMON_LIBRARY)
 
# 创建项目 
CreateTarget(${ProjectName} "Dll")    
 