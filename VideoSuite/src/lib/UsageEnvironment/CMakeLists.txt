# 引入外部函数
include(${ROOT_DIR}/cmake/module.cmake)

# 获取文件名称
set(AbsolutePathProject ${CMAKE_CURRENT_SOURCE_DIR})
get_filename_component(ProjectName ${AbsolutePathProject} NAME)

include_directories(../BasicUsageEnvironment/include)
include_directories(../groupsock/include)
include_directories(../liveMedia/include)
include_directories(../UsageEnvironment/include)

add_compile_definitions(VIDEOCOMMON_LIBRARY) 
# 创建项目
CreateTarget(${ProjectName} "Lib")
