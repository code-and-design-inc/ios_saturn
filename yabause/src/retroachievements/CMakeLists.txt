# RetroAchievements (rcheevos) library integration for YabaSanshiro
# This file builds the rcheevos library and integrates it with YabaSanshiro

cmake_minimum_required(VERSION 3.5)

project(retroachievements)

# RetroAchievements build options
option(HAVE_RETROACHIEVEMENTS "Enable RetroAchievements support" ON)
option(RC_CLIENT_SUPPORTS_RAINTEGRATION "Enable RAIntegration support" OFF)
option(RC_DISABLE_LUA "Disable Lua support" ON)

if(NOT HAVE_RETROACHIEVEMENTS)
    return()
endif()

# rcheevos source files
set(RCHEEVOS_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/rcheevos/src)

# Core rcheevos sources
set(RCHEEVOS_SOURCES
    # Main rc_client files
    ${RCHEEVOS_SRC_DIR}/rc_client.c
    ${RCHEEVOS_SRC_DIR}/rc_client_external.c
    ${RCHEEVOS_SRC_DIR}/rc_compat.c
    ${RCHEEVOS_SRC_DIR}/rc_util.c
    ${RCHEEVOS_SRC_DIR}/rc_version.c
    
    # API sources
    ${RCHEEVOS_SRC_DIR}/rapi/rc_api_common.c
    ${RCHEEVOS_SRC_DIR}/rapi/rc_api_editor.c
    ${RCHEEVOS_SRC_DIR}/rapi/rc_api_info.c
    ${RCHEEVOS_SRC_DIR}/rapi/rc_api_runtime.c
    ${RCHEEVOS_SRC_DIR}/rapi/rc_api_user.c
    
    # Core achievement engine
    ${RCHEEVOS_SRC_DIR}/rcheevos/alloc.c
    ${RCHEEVOS_SRC_DIR}/rcheevos/condition.c
    ${RCHEEVOS_SRC_DIR}/rcheevos/condset.c
    ${RCHEEVOS_SRC_DIR}/rcheevos/consoleinfo.c
    ${RCHEEVOS_SRC_DIR}/rcheevos/format.c
    ${RCHEEVOS_SRC_DIR}/rcheevos/lboard.c
    ${RCHEEVOS_SRC_DIR}/rcheevos/memref.c
    ${RCHEEVOS_SRC_DIR}/rcheevos/operand.c
    ${RCHEEVOS_SRC_DIR}/rcheevos/rc_validate.c
    ${RCHEEVOS_SRC_DIR}/rcheevos/richpresence.c
    ${RCHEEVOS_SRC_DIR}/rcheevos/runtime.c
    ${RCHEEVOS_SRC_DIR}/rcheevos/runtime_progress.c
    ${RCHEEVOS_SRC_DIR}/rcheevos/trigger.c
    ${RCHEEVOS_SRC_DIR}/rcheevos/value.c
    
    # Hash engine
    ${RCHEEVOS_SRC_DIR}/rhash/aes.c
    ${RCHEEVOS_SRC_DIR}/rhash/cdreader.c
    ${RCHEEVOS_SRC_DIR}/rhash/hash.c
    ${RCHEEVOS_SRC_DIR}/rhash/hash_disc.c
    ${RCHEEVOS_SRC_DIR}/rhash/hash_encrypted.c
    ${RCHEEVOS_SRC_DIR}/rhash/hash_rom.c
    ${RCHEEVOS_SRC_DIR}/rhash/hash_zip.c
    ${RCHEEVOS_SRC_DIR}/rhash/md5.c
)

# Optional RAIntegration support
if(RC_CLIENT_SUPPORTS_RAINTEGRATION)
    list(APPEND RCHEEVOS_SOURCES
        ${RCHEEVOS_SRC_DIR}/rc_client_raintegration.c
    )
endif()

# YabaSanshiro RetroAchievements integration sources
set(YABAUSE_RA_SOURCES
    yabause_ra_integration.cpp
    yabause_ra_memory.cpp
    yabause_ra_network.cpp
    yabause_ra_game_mapper.cpp
    yabause_ra_integration_cwrapper.cpp
)

# Create rcheevos as OBJECT library for easier integration
add_library(rcheevos OBJECT ${RCHEEVOS_SOURCES})

# Include directories
target_include_directories(rcheevos PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/rcheevos/include
)

target_include_directories(rcheevos PRIVATE
    ${RCHEEVOS_SRC_DIR}
    ${RCHEEVOS_SRC_DIR}/rapi
    ${RCHEEVOS_SRC_DIR}/rcheevos
    ${RCHEEVOS_SRC_DIR}/rhash
)

# Compile definitions
target_compile_definitions(rcheevos PRIVATE
    RC_CLIENT_SUPPORTS_EXTERNAL=1
    RC_CLIENT_SUPPORTS_HASH=1
    $<$<BOOL:${RC_CLIENT_SUPPORTS_RAINTEGRATION}>:RC_CLIENT_SUPPORTS_RAINTEGRATION=1>
    $<$<BOOL:${RC_DISABLE_LUA}>:RC_DISABLE_LUA=1>
)

# Platform specific settings
if(ANDROID)
    target_compile_definitions(rcheevos PRIVATE
        RC_NO_STDIO=1
        RC_LITTLE_ENDIAN=1
    )
    target_link_libraries(rcheevos log)
endif()

# Compiler settings
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(rcheevos PRIVATE
        -Wall
        -Wno-unused-parameter
        -fno-strict-aliasing
    )
endif()

# Create YabaSanshiro RetroAchievements integration as OBJECT library
add_library(yabause_retroachievements OBJECT ${YABAUSE_RA_SOURCES})

# Object libraries don't use target_link_libraries for linking
# Dependencies are handled at the final library/executable level

target_include_directories(yabause_retroachievements PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/rcheevos/include
)

target_include_directories(yabause_retroachievements PRIVATE
    ${CMAKE_SOURCE_DIR}/src  # YabaSanshiro core headers
)

# Add same compile definitions to integration library
target_compile_definitions(yabause_retroachievements PRIVATE
    RC_CLIENT_SUPPORTS_EXTERNAL=1
    RC_CLIENT_SUPPORTS_HASH=1
    $<$<BOOL:${RC_CLIENT_SUPPORTS_RAINTEGRATION}>:RC_CLIENT_SUPPORTS_RAINTEGRATION=1>
    $<$<BOOL:${RC_DISABLE_LUA}>:RC_DISABLE_LUA=1>
)

# Export variables for parent CMake files
set(RCHEEVOS_LIBRARIES rcheevos yabause_retroachievements PARENT_SCOPE)
set(RCHEEVOS_INCLUDE_DIRS 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/rcheevos/include 
    PARENT_SCOPE
)