add_subdirectory(BasicUsageEnvironment)
set_property(TARGET BasicUsageEnvironment PROPERTY FOLDER "lib")

add_subdirectory(groupsock)
set_property(TARGET groupsock PROPERTY FOLDER "lib")

add_subdirectory(liveMedia)
set_property(TARGET liveMedia PROPERTY FOLDER "lib")

add_subdirectory(UsageEnvironment)
set_property(TARGET UsageEnvironment PROPERTY FOLDER "lib")

# add_subdirectory(qhttpserver)
# set_property(TARGET qhttpserver PROPERTY FOLDER "lib")

