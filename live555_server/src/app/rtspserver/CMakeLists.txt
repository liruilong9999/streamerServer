# 引入外部函数
include(${ROOT_DIR}/cmake/module.cmake)

# 获取绝对路径
set(AbsolutePathProject ${CMAKE_CURRENT_SOURCE_DIR})
get_filename_component(ProjectName ${AbsolutePathProject} NAME)

set(QT_LIBRARY_LIST "Core" "Network" "Concurrent")

set(SELF_LIBRARY_LIST "BasicUsageEnvironment" "groupsock" "UsageEnvironment" "liveMedia" )

include_directories(../../lib/BasicUsageEnvironment/include)
include_directories(../../lib/groupsock/include)
include_directories(../../lib/liveMedia/include)
include_directories(../../lib/UsageEnvironment/include)

set(OTHER_LIBRARY_LIST "Ws2_32")

# 创建项目 
CreateTarget(${ProjectName} "ExeCMD")  
 