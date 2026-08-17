project(yabause_tests)

# Unit tests for the parts of the emulator that are pure data logic and can be
# exercised without a window, a GPU or a real input device. Enable with
# -DYAB_TESTS=ON and run with ctest.
#
# peripheral.c only pulls in debug.h and peripheral.h, so the input mapping
# tests link it directly instead of the whole emulator.

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

# The parent directory does add_definitions(-DHAVE_LIBSDL=1) whenever SDL is
# found, and that leaks into this one. It makes InputPortConfig.cpp compile the
# SdlDeviceSource branch, which calls into persdljoy.c - a file no test target
# links, so the link fails with LNK2019 in exactly the configuration the desktop
# build uses. The tests replace DeviceSource with their own, so the SDL-backed
# one is not what is under test: drop the definition here instead of dragging
# the peripheral core (and SDL itself) into a QtCore-only target.
remove_definitions(-DHAVE_LIBSDL=1 -DHAVE_LIBSDL2=1)

add_executable(test_peripheral
	test_peripheral.c
	${CMAKE_CURRENT_SOURCE_DIR}/../peripheral.c)

# core.h guards its logging macros behind the same feature checks the main
# build performs; without them it does not even preprocess.
target_compile_definitions(test_peripheral PRIVATE
	HAVE_C99_VARIADIC_MACROS=1
	_CRT_SECURE_NO_WARNINGS)

add_test(NAME peripheral_mapping COMMAND test_peripheral)

# Committing a wizard binding on release rather than on press. The latch is a
# header-only value type over u32, so this needs neither Qt nor SDL.
add_executable(test_UISetupInputLatch test_UISetupInputLatch.cpp)

target_compile_definitions(test_UISetupInputLatch PRIVATE
	HAVE_C99_VARIADIC_MACROS=1
	_CRT_SECURE_NO_WARNINGS)

add_test(NAME setup_input_latch COMMAND test_UISetupInputLatch)

# The port / physical device policy. It reads and writes QSettings and asks a
# DeviceSource what is plugged in, so QtCore is all it takes - no widgets, no
# SDL, no display.
find_package(Qt6 COMPONENTS Core QUIET)
if (Qt6Core_FOUND)
	add_executable(test_input_port_config
		test_input_port_config.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/../qt/InputPortConfig.cpp)

	target_compile_definitions(test_input_port_config PRIVATE
		HAVE_C99_VARIADIC_MACROS=1
		_CRT_SECURE_NO_WARNINGS)

	target_link_libraries(test_input_port_config Qt6::Core)

	# On Windows the test executable lands in its own directory with no Qt DLLs
	# beside it, and Qt is not on PATH for a ctest run: the process then dies at
	# load time with 0xC0000135 (missing DLL) before a single assertion runs.
	# Copy whatever shared libraries the linked targets need next to the exe.
	if (WIN32)
		add_custom_command(TARGET test_input_port_config POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
				$<TARGET_RUNTIME_DLLS:test_input_port_config> $<TARGET_FILE_DIR:test_input_port_config>
			COMMAND_EXPAND_LISTS)
	endif()

	add_test(NAME input_port_config COMMAND test_input_port_config)

	# The graphics preset layer. Like the port configuration above it is pure
	# data logic: GraphicsPreset.cpp depends only on Qt containers, and the
	# tests build their own GraphicsPresetCores rather than asking the running
	# binary what it registered.
	#
	# Unlike test_input_port_config, this target also needs Qt6::Test (the
	# test file uses QCOMPARE/QVERIFY/QTEST_MAIN from <QtTest/QtTest>) and
	# Qt6::Widgets (UISetupWizard.h includes <QWizard>, which lives in the
	# QtWidgets module; without linking it the include path for qwizard.h is
	# not even set up, so the build fails with C1083, not just a link error).
	# Linking Qt6::Widgets also carries its INTERFACE_COMPILE_DEFINITIONS
	# (QT_WIDGETS_LIB) to this target, so QTEST_MAIN() below builds a
	# QApplication rather than a QCoreApplication -- required because
	# UISetupWizard derives from QWizard, a widget.
	find_package(Qt6 COMPONENTS Test Widgets QUIET)
	add_executable(test_GraphicsPreset
		test_GraphicsPreset.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/../qt/ui/GraphicsPreset.cpp)

	# TestGraphicsPreset derives from QObject and uses Q_OBJECT / private
	# slots, and the file #includes its own generated test_GraphicsPreset.moc
	# -- AUTOMOC is what generates that file. It is not enabled globally in
	# this CMakeLists.txt because test_peripheral and test_input_port_config
	# have no QObject classes to moc.
	set_target_properties(test_GraphicsPreset PROPERTIES AUTOMOC ON)

	target_compile_definitions(test_GraphicsPreset PRIVATE
		HAVE_C99_VARIADIC_MACROS=1
		_CRT_SECURE_NO_WARNINGS)

	target_link_libraries(test_GraphicsPreset Qt6::Core Qt6::Test Qt6::Widgets)

	if (WIN32)
		add_custom_command(TARGET test_GraphicsPreset POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
				$<TARGET_RUNTIME_DLLS:test_GraphicsPreset> $<TARGET_FILE_DIR:test_GraphicsPreset>
			COMMAND_EXPAND_LISTS)
	endif()

	# QTEST_MAIN builds a QApplication here rather than a QCoreApplication (see
	# the Qt6::Widgets note above), and a QApplication will not start without a
	# platform plugin: it aborts with 0xC0000409 before a single test runs.
	# Deploy the offscreen plugin from the very Qt this target linked against.
	#
	# win_template/platforms carries the plugins that ship with the released
	# application, but they come from a different Qt build and Qt rejects them
	# as incompatible ("Could not find the Qt platform plugin"), so they cannot
	# stand in here.
	add_custom_command(TARGET test_GraphicsPreset POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory
			$<TARGET_FILE_DIR:test_GraphicsPreset>/platforms
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			$<TARGET_FILE:Qt6::QOffscreenIntegrationPlugin>
			$<TARGET_FILE_DIR:test_GraphicsPreset>/platforms)

	add_test(NAME graphics_preset COMMAND test_GraphicsPreset)

	# Offscreen, always: a unit test must not open a window on whatever machine
	# runs it, and the developer's desktop is one of those machines.
	set_tests_properties(graphics_preset PROPERTIES
		ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

	# The wizard's write-or-not decision for the input port. Like
	# test_input_port_config this is pure QtCore: no QObject, no widgets, no
	# window, so it needs none of the QtTest/Widgets/AUTOMOC machinery that
	# test_GraphicsPreset above requires.
	add_executable(test_UISetupInputPlan
		test_UISetupInputPlan.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/../qt/ui/UISetupInputPlan.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/../qt/InputPortConfig.cpp)

	target_compile_definitions(test_UISetupInputPlan PRIVATE
		HAVE_C99_VARIADIC_MACROS=1
		_CRT_SECURE_NO_WARNINGS)

	target_link_libraries(test_UISetupInputPlan Qt6::Core)

	if (WIN32)
		add_custom_command(TARGET test_UISetupInputPlan POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
				$<TARGET_RUNTIME_DLLS:test_UISetupInputPlan> $<TARGET_FILE_DIR:test_UISetupInputPlan>
			COMMAND_EXPAND_LISTS)
	endif()

	add_test(NAME setup_input_plan COMMAND test_UISetupInputPlan)
else()
	message(STATUS "Qt6Core not found: skipping test_input_port_config")
endif()
