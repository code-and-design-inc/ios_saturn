cmake_minimum_required(VERSION 3.20)

# Pocket Arcade Sega Saturn core: builds the pinned Yaba Sanshiro 1.20.37
# emulator core (SH-2 interpreter, Musashi 68000, software VDP1/VDP2 renderer,
# ISO/CUE/CHD disc images) plus the Pocket Arcade bridge into a dynamic
# framework driven through the C ABI in PASaturnBridge.h. The same project also
# builds a headless macOS harness used to verify a disc boots before the app is
# involved.
#
# The upstream tree is compiled unmodified: no OpenGL, no dynarec, no ports.

set(SATURN_UPSTREAM "${CMAKE_CURRENT_SOURCE_DIR}/../upstream/yabasanshiro-1.20.37" CACHE PATH "Yaba Sanshiro source tree (contains yabause/src)")
set(SATURN_LIBCHDR "${CMAKE_CURRENT_SOURCE_DIR}/../upstream/libchdr" CACHE PATH "libchdr source tree")
get_filename_component(SATURN_UPSTREAM "${SATURN_UPSTREAM}" ABSOLUTE)
get_filename_component(SATURN_LIBCHDR "${SATURN_LIBCHDR}" ABSOLUTE)

project(PocketSaturn LANGUAGES C CXX)

option(SATURN_BUILD_FRAMEWORK "Build the iOS SaturnCore dynamic framework" ON)
option(SATURN_BUILD_HEADLESS "Build the macOS headless harness" OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

set(YS "${SATURN_UPSTREAM}/yabause/src")
if(NOT EXISTS "${YS}/yabause.c")
	message(FATAL_ERROR "Yaba Sanshiro sources not found at ${YS}; run build-ios.sh")
endif()

# libchdr (CHD disc images) with its bundled lzma/zstd; zlib comes from the SDK.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(WITH_SYSTEM_ZLIB ON CACHE BOOL "" FORCE)
set(INSTALL_STATIC_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_LTO OFF CACHE BOOL "" FORCE)
add_subdirectory(${SATURN_LIBCHDR} ${CMAKE_CURRENT_BINARY_DIR}/libchdr EXCLUDE_FROM_ALL)

# The upstream file list mirrors yabause/src/CMakeLists.txt with every port,
# OpenGL/Vulkan renderer, dynarec, SSF player and platform sound/CD/joystick
# core left out. Threading comes from thr-linux.cpp (pthreads; its IOS branch
# disables the Linux-only affinity and filesystem helpers).
set(SATURN_CORE_SOURCES
	bios.c cdbase.c cheat.c coffelf.c cs0.c cs1.c cs2.c debug.c error.c gameinfo.c
	japmodem.c m68kcore.c m68kd.c memory.c movie.c state_save.cpp netlink.c osdcore.c
	peripheral.c profile.c frameprofile.cpp scspdsp.c scu.c sh2core.c sh2d.c sh2iasm.c
	sh2idle.c sh2int.c sh2trace.c smpc.c snddummy.c titan/titan.c
	vdp1.cpp vdp2.cpp vdp2debug.c vidshared.c vidsoft.c yabause.c Counter.cpp
	BackupManager.cpp jsoncpp.cpp PlayRecorder.cpp sh2cache.c
	scsp.cpp
	musashi/m68kopac.c musashi/m68kopdm.c musashi/m68kopnz.c musashi/m68kops.c
	musashi/m68kdasm.c musashi/m68kcpu.c m68kmusashi.c
	sock-dummy.c thr-linux.cpp
)
list(TRANSFORM SATURN_CORE_SOURCES PREPEND "${YS}/")

# config.h normally comes from CMake's configure_file in the upstream tree.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated/config.h"
"#define HAVE_MUSASHI 1\n#define YAB_VERSION \"1.20.37\"\n#define GIT_SHA1 \"\"\n")

add_library(saturncore STATIC ${SATURN_CORE_SOURCES})
target_include_directories(saturncore PUBLIC
	${YS}
	${YS}/musashi
	${CMAKE_CURRENT_BINARY_DIR}/generated
	${SATURN_LIBCHDR}/include
	${SATURN_LIBCHDR}/include/libchdr
)
# IOS=1 selects the upstream iOS branches (no <malloc.h>, synchronous SCSP,
# no CPU affinity). HAVE_LIBGL is deliberately not defined, so the software
# renderer never touches OpenGL.
target_compile_definitions(saturncore PUBLIC
	IOS=1
	AARCH64
	ARCH_IS_MACOSX=1
	HAVE_MUSASHI=1
	IMPROVED_SAVESTATES
	HAVE_C99_VARIADIC_MACROS=1
	HAVE_GETTIMEOFDAY=1
	HAVE_FLOORF=1
	HAVE_STRCASECMP=1
	HAVE_BUILTIN_BSWAP16=1
	HAVE_BUILTIN_BSWAP32=1
	HAVE_SYS_TIME_H=1
	HAVE_STDINT_H=1
	CACHE_ENABLE=1
	EXEC_FROM_CACHE=1
	SH2_TRACE=1
	JSONCPP_NO_LOCALE_SUPPORT
	PACKAGE="yabasanshiro"
	VERSION="1.20.37"
)
target_compile_options(saturncore PRIVATE
	-O3 -fno-aligned-allocation -fno-strict-aliasing
	-Wno-everything
)
target_link_libraries(saturncore PUBLIC chdr-static z)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
	# The macOS harness compiles the same IOS=1 sources; ygl.h then wants the
	# OpenGLES type headers, which the shim maps onto the macOS OpenGL ones.
	target_include_directories(saturncore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/HostShim)
endif()

set(SATURN_BRIDGE_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/PASaturnBridge.h
	${CMAKE_CURRENT_SOURCE_DIR}/PASaturnBridge.cpp
)

if(SATURN_BUILD_FRAMEWORK)
	add_library(SaturnCore SHARED ${SATURN_BRIDGE_SOURCES})
	target_include_directories(SaturnCore PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
	target_link_libraries(SaturnCore PRIVATE saturncore)
	set_target_properties(SaturnCore PROPERTIES
		OUTPUT_NAME "SaturnCore"
		PREFIX ""
		SUFFIX ""
		LINK_FLAGS "-Wl,-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/SaturnCore-exported-symbols.txt -Wl,-install_name,@rpath/SaturnCore.framework/SaturnCore -Wl,-dead_strip"
	)
endif()

if(SATURN_BUILD_HEADLESS)
	add_executable(saturnheadless
		${CMAKE_CURRENT_SOURCE_DIR}/Tools/saturnheadless.cpp
		${SATURN_BRIDGE_SOURCES}
	)
	target_include_directories(saturnheadless PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
	target_link_libraries(saturnheadless PRIVATE saturncore)
endif()
