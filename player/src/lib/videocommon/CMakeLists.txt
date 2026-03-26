# 引入外部函数
include(${ROOT_DIR}/cmake/module.cmake)

# 获取文件名称
set(AbsolutePathProject ${CMAKE_CURRENT_SOURCE_DIR})
get_filename_component(ProjectName ${AbsolutePathProject} NAME)
# Qt 库
set(QT_LIBRARY_LIST "Core" "Gui" "Widgets")

# 链接库
set(SELF_LIBRARY_LIST "avcodec" "avdevice" "avfilter" "avformat" "avutil" "SDL2d" "SDL2")
# 预编译
add_compile_definitions(VIDEOCOMMON_LIBRARY)

# 创建项目
CreateTarget(${ProjectName} "Dll")
