#--------------------------------------------------------------------------------------
# iOS build
#--------------------------------------------------------------------------------------
# Build step
# 1) mkdir build
# 2) cd build
# 3) cmake ../yabause -DGIT_EXECUTABLE=/usr/bin/git -DSH2_DYNAREC=OFF -DYAB_WANT_DYNAREC_DEVMIYAX=OFF -DCMAKE_TOOLCHAIN_FILE=../yabause/src/ios/iOS.cmake -DIOS_PLATFORM=OS -DYAB_PORTS=ios
# 4) make
# 5) open yabause/src/ios/ios/uoyabause/uoyabause.xcodeproj
# 6) >
#--------------------------------------------------------------------------------------


project(yabause-ios)

yab_port_start()

# Include RetroAchievements support for iOS
option(HAVE_RETROACHIEVEMENTS "Enable RetroAchievements support" ON)
if(HAVE_RETROACHIEVEMENTS)
    add_subdirectory(../retroachievements retroachievements)
endif()

set( LIBRARY_NAME yabause_ios )


set( SOURCES 
 yui.cpp
 osdcore.h
 osdcore.c
)


set( LIBRARY_OUTPUT_PATH  ${PROJECT_SOURCE_DIR}/uoyabause/ )

# Include RetroAchievements object files directly if enabled
if(HAVE_RETROACHIEVEMENTS)
    add_library(${LIBRARY_NAME}  
        $<TARGET_OBJECTS:yabause>
        $<TARGET_OBJECTS:rcheevos>
        $<TARGET_OBJECTS:yabause_retroachievements>
        ${SOURCES}
    )
else()
    add_library(${LIBRARY_NAME}  $<TARGET_OBJECTS:yabause>  ${SOURCES}  )
endif()

#set_xcode_property (${LIBRARY_NAME} GCC_GENERATE_DEBUGGING_SYMBOLS YES)
include_directories( ${YABAUSE_INCLUDE_DIR} ${PROJECT_BINARY_DIR}/../ ${PROJECT_SOURCE_DIR}/../ )

# Include RetroAchievements headers if enabled
if(HAVE_RETROACHIEVEMENTS)
    include_directories(${PROJECT_SOURCE_DIR}/../retroachievements)
    include_directories(${PROJECT_SOURCE_DIR}/../retroachievements/rcheevos/include)
endif()

# Link only core libraries (RetroAchievements objects are already included)
target_link_libraries(${LIBRARY_NAME} ${YABAUSE_LIBRARIES})

# Installation
#set (CMAKE_INSTALL_PREFIX "${hello-lib_SOURCE_DIR}/../hello-app/hello-app")
#install (TARGETS hello-lib DESTINATION .)
#install (FILES HelloIOS.h DESTINATION .)

add_custom_command(TARGET ${LIBRARY_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_BINARY_DIR}/src/zlib/install/lib/libz.a"
        "${CMAKE_SOURCE_DIR}/src/ios/uoyabause/"
    #COMMAND ${CMAKE_COMMAND} -E copy_if_different
    #    "${CMAKE_BINARY_DIR}/src/libchdr-prefix/src/libchdr-build/libflac-static.a"
    #    "${CMAKE_SOURCE_DIR}/src/ios/uoyabause/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_BINARY_DIR}/src/libchdr-prefix/src/libchdr-build/deps/zstd-1.5.6/build/cmake/lib/libzstd.a"
        "${CMAKE_SOURCE_DIR}/src/ios/uoyabause/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_BINARY_DIR}/src/libchdr-prefix/src/libchdr-build/deps/lzma-24.05/liblzma.a"
        "${CMAKE_SOURCE_DIR}/src/ios/uoyabause/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_BINARY_DIR}/src/libchdr-prefix/src/libchdr-build/libchdr-static.a"
        "${CMAKE_SOURCE_DIR}/src/ios/uoyabause/"
    COMMENT "Copying libraries to uoyabause directory..."
)

# Note: RetroAchievements object files are now included directly in libyabause_ios.a
# No separate library files need to be copied to the iOS project
