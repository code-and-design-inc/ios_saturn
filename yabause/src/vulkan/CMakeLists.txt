project(yabause-vulkan)

yab_port_start()

#include(external_glfw.cmake)
#include(external_glm.cmake)
#include(external_stb.cmake)
#include(external_shaderc.cmake)

include( ${CMAKE_SOURCE_DIR}/CMake/Packages/external_libpng.cmake )
find_package(SDL2 COMPONENTS sdl)
find_package(SDL2 REQUIRED)


set(YABAUSE_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../")
set(PORT_INCLUDE_DIRS  ${SDL2_INCLUDE_DIR} ${GLM_INCLUDE_DIRS} ${GLFW_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/include  ${SHADERC_INCLUDE_DIR} ${png_INCLUDE_DIR} ${zlib_INCLUDE_DIR} )
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(VULKAN_LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lib/x86_64/vulkan-1.lib)
else()
	set(VULKAN_LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lib/x86/vulkan-1.lib)
endif()
set(PORT_LIBRARIES ${GLFW_LIBRARIES} ${VULKAN_LIB_PATH} ${SDL2_LIBRARY} ${LIBCHDR_LIBRARIES}  ${zlib_STATIC_LIBRARIES} ${SHADERC_LIBRARIES} ${png_STATIC_LIBRARIES} )

option(HAVE_RETROACHIEVEMENTS "Enable RetroAchievements support" ON)
if(HAVE_RETROACHIEVEMENTS)
	if(NOT TARGET yabause_retroachievements)
		add_subdirectory(../retroachievements retroachievements)
	endif()
endif()

include_directories( ${YABAUSE_INCLUDE_DIR}  ${PORT_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})

# --- ImGui (docking branch, vendored) ---
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
)

# --- Debug UI module ---
set(DEBUG_UI_SOURCES
    debug/DebugUI.cpp
    debug/DebugUI.h
    debug/DebugSnapshot.cpp
    debug/DebugSnapshot.h
    debug/Vdp1JsonlExporter.cpp
    debug/Vdp1JsonlExporter.h
    debug/MemoryDecode.cpp
    debug/MemoryDecode.h
    debug/FbReadback.cpp
    debug/FbReadback.h
    debug/Vdp2Snapshot.cpp
    debug/Vdp2Snapshot.h
    debug/Vdp2DebugUI.cpp
    debug/Vdp2DebugUI.h
    debug/Vdp2PixelProbe.cpp
    debug/Vdp2PixelProbe.h
)

set(yabause_vulkan_SOURCES
	yui.cpp
	Window.h
	Window_glfw.cpp
	${IMGUI_SOURCES}
	${DEBUG_UI_SOURCES}
)

link_directories( ${link_directories} ${LIBCHDR_LIB_DIR} )
add_executable(yabause-vulkan ${yabause_vulkan_SOURCES})
target_include_directories(yabause-vulkan PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)
target_link_libraries(yabause-vulkan yabause ${YABAUSE_LIBRARIES} ${PORT_LIBRARIES}  )

if(HAVE_RETROACHIEVEMENTS)
	add_dependencies(yabause-vulkan yabause_retroachievements rcheevos)
	target_sources(yabause-vulkan PRIVATE $<TARGET_OBJECTS:yabause_retroachievements> $<TARGET_OBJECTS:rcheevos>)
endif()

if (${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "AMD64")
  set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin/glslangValidator.exe")
else()
  set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin32/glslangValidator.exe")
endif()

file(GLOB_RECURSE GLSL_SOURCE_FILES
    "shaders/*.frag"
    "shaders/*.vert"
    )

foreach(GLSL ${GLSL_SOURCE_FILES})
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${PROJECT_BINARY_DIR}/shaders/${FILE_NAME}.spv")
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shaders/"
    COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
    DEPENDS ${GLSL})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
    Shaders 
    DEPENDS ${SPIRV_BINARY_FILES}
    )

add_dependencies(yabause-vulkan Shaders png)
add_dependencies(yabause-vulkan shaderc)
add_dependencies(yabause-vulkan glfw)

add_custom_command(TARGET yabause-vulkan POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:yabause-vulkan>/shaders/"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_BINARY_DIR}/shaders"
        "$<TARGET_FILE_DIR:yabause-vulkan>/shaders"
        )
        
#set_property(TARGET yabause-vulkan PROPERTY
#  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")        
				
yab_port_success(yabause-vulkan)

