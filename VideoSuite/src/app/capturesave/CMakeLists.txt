# 引入外部函数
include(${ROOT_DIR}/cmake/module.cmake)

# 获取绝对路径
set(AbsolutePathProject ${CMAKE_CURRENT_SOURCE_DIR})
get_filename_component(ProjectName ${AbsolutePathProject} NAME)

set(QT_LIBRARY_LIST "Core" "Network" "Concurrent" "Gui" "Widgets")

set(SELF_LIBRARY_LIST_DEBUG "videocommond" )

set(SELF_LIBRARY_LIST_RELEASE  "videocommon")  


#set(SELF_LIBRARY_LIST_DEBUG "BasicUsageEnvironmentd" "groupsockd" "videocommond" "UsageEnvironmentd" "liveMediad" "avcodec" "avfilter" "avformat" "avutil" )

#set(SELF_LIBRARY_LIST_RELEASE "BasicUsageEnvironment" "groupsock"  "videocommon" "UsageEnvironment" "liveMedia"  "avcodec" "avfilter" "avformat" "avutil")  

include_directories(../../lib/BasicUsageEnvironment/include)
include_directories(../../lib/groupsock/include)
include_directories(../../lib/liveMedia/include)
include_directories(../../lib/UsageEnvironment/include)
 
#set(OTHER_LIBRARY_LIST_DEBUG "Ws2_32")
#set(OTHER_LIBRARY_LIST_RELEASE "Ws2_32")

# 预编译
add_compile_definitions(RTSPSERVER_LIBRARY) 

# 创建项目 
CreateTarget(${ProjectName} "ExeCMD")  
 