
add_subdirectory(capturesave) 
set_property(TARGET capturesave PROPERTY FOLDER "App")

add_subdirectory(rtspserver) 
set_property(TARGET rtspserver PROPERTY FOLDER "App")


add_subdirectory(player) 
set_property(TARGET player PROPERTY FOLDER "App")
