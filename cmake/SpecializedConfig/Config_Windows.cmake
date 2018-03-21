
set(LCR_DEPS_PACKAGE "${CMAKE_SOURCE_DIR}/../WindowsCompileDeps"
		CACHE PATH "Platform-specific package with pre-built dependencies")

set(LCR_DEPS_SRC_PACKAGE "${CMAKE_SOURCE_DIR}/../WindowsCompile"
		CACHE PATH "Platform-specific source package for dependency build")

list(APPEND CMAKE_INCLUDE_PATH "${LCR_DEPS_PACKAGE}/include")
list(APPEND CMAKE_LIBRARY_PATH "${LCR_DEPS_PACKAGE}/x64/Release/lib")

set(OPENEXR_ROOT ${LCR_DEPS_PACKAGE})

set(Boost_NO_SYSTEM_PATHS ON)
set(BOOST_INCLUDEDIR "${LCR_DEPS_PACKAGE}/include/Boost")
set(BOOST_LIBRARYDIR "${LCR_DEPS_PACKAGE}/x64/Release/lib")

SET(BISON_EXECUTABLE "${LCR_DEPS_SRC_PACKAGE}/support/bin/win_bison.exe")
SET(FLEX_EXECUTABLE "${LCR_DEPS_SRC_PACKAGE}/support/bin/win_flex.exe")
