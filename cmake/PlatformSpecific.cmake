################################################################################
# Copyright 1998-2020 by authors (see AUTHORS.txt)
#
#   This file is part of LuxCoreRender.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
################################################################################

# Use relative paths
# This is mostly to reduce path size for command-line limits on windows
if(WIN32)
  # This seems to break Xcode projects so definitely don't enable on Apple builds
  set(CMAKE_USE_RELATIVE_PATHS true)
  set(CMAKE_SUPPRESS_REGENERATION true)
endif(WIN32)

include(AdjustToolFlags)

###########################################################################
#
# Compiler Flags
#
###########################################################################

###########################################################################
#
# VisualStudio
#
###########################################################################

IF(MSVC)
	message(STATUS "MSVC")

	# Change warning level to something saner
	# Force to always compile with W0
	if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
		string(REGEX REPLACE "/W[0-4]" "/W0" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W0")
	endif()

	# Minimizes Windows header files
	ADD_DEFINITIONS(-DWIN32_LEAN_AND_MEAN)
	# Do not define MIN and MAX macros
	ADD_DEFINITIONS(-DNOMINMAX)
	# Do not warn about standard but insecure functions
	ADD_DEFINITIONS(-D_CRT_SECURE_NO_WARNINGS -D_SCL_SECURE_NO_WARNINGS)
	# Enable Unicode
	ADD_DEFINITIONS(-D_UNICODE)
	# Enable SSE2/SSE/MMX
	ADD_DEFINITIONS(-D__SSE2__ -D__SSE__ -D__MMX__)

	SET(FLEX_FLAGS "--wincompat")

	# Base settings

	# Disable incremental linking, because LTCG is currently triggered by
	# linking with a pre-built lib and then we'll see a warning:
	AdjustToolFlags(
				CMAKE_EXE_LINKER_FLAGS_DEBUG
				CMAKE_MODULE_LINKER_FLAGS_DEBUG
				CMAKE_SHARED_LINKER_FLAGS_DEBUG
			ADDITIONS "/INCREMENTAL:NO"
			REMOVALS "/INCREMENTAL(:YES|:NO)?")

	# Always link with the release runtime DLL:
	AdjustToolFlags(CMAKE_C_FLAGS CMAKE_CXX_FLAGS "/MD")
	AdjustToolFlags(
				CMAKE_C_FLAGS_DEBUG
				CMAKE_C_FLAGS_RELEASE
				CMAKE_C_FLAGS_MINSIZEREL
				CMAKE_C_FLAGS_RELWITHDEBINFO
				CMAKE_CXX_FLAGS_DEBUG
				CMAKE_CXX_FLAGS_RELEASE
				CMAKE_CXX_FLAGS_MINSIZEREL
				CMAKE_CXX_FLAGS_RELWITHDEBINFO
			REMOVALS "/MDd? /MTd? /RTC1 /D_DEBUG")

	# Optimization options:

	# Whole Program Opt. gui display fixed in cmake 2.8.5
	# See http://public.kitware.com/Bug/view.php?id=6794
	# /GL will be used to build the code but the selection is not displayed in the menu
	set(MSVC_RELEASE_COMPILER_FLAGS "/WX- /MP /Ox /Ob2 /Oi /Oy /GT /GL /Gm- /EHsc /MD /GS /fp:precise /Zc:wchar_t /Zc:forScope /GR /Gd /TP /GL /GF /Ot")

	#set(MSVC_RELEASE_LINKER_FLAGS "/LTCG /OPT:REF /OPT:ICF")
	#set(MSVC_RELEASE_WITH_DEBUG_LINKER_FLAGS "/DEBUG")
	# currently not in release version but should be soon - in meantime linker will inform you about switching this flag automatically because of /GL
	#set(MSVC_RELEASE_LINKER_FLAGS "/LTCG /OPT:REF /OPT:ICF")
	set(MSVC_RELEASE_LINKER_FLAGS "/INCREMENTAL:NO /LTCG")

	IF(MSVC90)
		message(STATUS "Version 9")
	ENDIF(MSVC90)

	IF(MSVC10)
		message(STATUS "Version 10")
		list(APPEND MSVC_RELEASE_COMPILER_FLAGS "/arch:SSE2 /openmp")
	ENDIF(MSVC10)

	IF(MSVC_VERSION GREATER_EQUAL 1800)
		message(STATUS "MSVC Version: " ${MSVC_VERSION} )

		list(APPEND MSVC_RELEASE_COMPILER_FLAGS "/openmp /Qfast_transcendentals /wd\"4244\" /wd\"4756\" /wd\"4267\" /wd\"4056\" /wd\"4305\" /wd\"4800\"")

		# Use multiple processors in debug mode, for faster rebuild:
		AdjustToolFlags(
				CMAKE_C_FLAGS_DEBUG CMAKE_CXX_FLAGS_DEBUG ADDITIONS "/MP")

		# Enable /bigobj for debug builds to prevent error C1128 when compiling OpenVDB
		AdjustToolFlags(
				CMAKE_C_FLAGS_DEBUG CMAKE_CXX_FLAGS_DEBUG ADDITIONS "/bigobj")
	ENDIF(MSVC_VERSION GREATER_EQUAL 1800)

	AdjustToolFlags(
				CMAKE_C_FLAGS_RELEASE
				CMAKE_CXX_FLAGS_RELEASE
			ADDITIONS ${MSVC_RELEASE_COMPILER_FLAGS})

	set(MSVC_RELEASE_WITH_DEBUG_COMPILER_FLAGS ${MSVC_RELEASE_COMPILER_FLAGS} "/Zi")
	AdjustToolFlags(CMAKE_C_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS_RELWITHDEBINFO
			ADDITIONS ${MSVC_RELEASE_WITH_DEBUG_COMPILER_FLAGS})

	AdjustToolFlags(
				CMAKE_EXE_LINKER_FLAGS_RELEASE
				CMAKE_STATIC_LINKER_FLAGS_RELEASE
                CMAKE_SHARED_LINKER_FLAGS_RELEASE
                CMAKE_MODULE_LINKER_FLAGS_RELEASE
				CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO
				CMAKE_STATIC_LINKER_FLAGS_RELWITHDEBINFO
                CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO
                CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO
			ADDITIONS ${MSVC_RELEASE_LINKER_FLAGS}
			REMOVALS "/INCREMENTAL(:YES|:NO)?")
	AdjustToolFlags(
				CMAKE_STATIC_LINKER_FLAGS_RELEASE
				CMAKE_STATIC_LINKER_FLAGS_RELWITHDEBINFO
			REMOVALS "/INCREMENTAL(:YES|:NO)?")

ENDIF(MSVC)

###########################################################################
#
# Apple
#
###########################################################################

IF(APPLE)

  CMAKE_MINIMUM_REQUIRED(VERSION 3.12) # Required for FindBoost 1.67.0

	########## OS and hardware detection ###########

	EXECUTE_PROCESS(COMMAND uname -r OUTPUT_VARIABLE MAC_SYS) # check for actual system-version

	SET(CMAKE_OSX_DEPLOYMENT_TARGET 10.9) # Minimum OS requirements for LuxCore

  IF(${MAC_SYS} MATCHES 20)
    SET(OSX_SYSTEM 11.1)
    ELSEIF(${MAC_SYS} MATCHES 19)
		SET(OSX_SYSTEM 10.15)
    ELSEIF(${MAC_SYS} MATCHES 18)
		SET(OSX_SYSTEM 10.15)
    ELSEIF(${MAC_SYS} MATCHES 17)
		SET(OSX_SYSTEM 10.14)
	  ELSEIF(${MAC_SYS} MATCHES 16)
		SET(OSX_SYSTEM 10.13)
	  ELSE()
		SET(OSX_SYSTEM unsupported)
	  ENDIF()

	SET(QT_BINARY_DIR /usr/local/bin)

	IF(NOT ${CMAKE_GENERATOR} MATCHES "Xcode") # unix makefile generator does not fill XCODE_VERSION var !
		EXECUTE_PROCESS(COMMAND xcodebuild -version OUTPUT_VARIABLE XCODE_VERS_BUILDNR )
		STRING(SUBSTRING ${XCODE_VERS_BUILDNR} 6 3 XCODE_VERSION) # truncate away build-nr
	ENDIF()

	SET(CMAKE_XCODE_ATTRIBUTE_ARCHS $(NATIVE_ARCH_ACTUAL))

  SET(CMAKE_OSX_SYSROOT /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX${OSX_SYSTEM}.sdk)

  SET(CMAKE_XCODE_ATTRIBUTE_SDKROOT macosx) # to silence sdk not found warning, just overrides CMAKE_OSX_SYSROOT, gets latest available

	# set a precedence of sdk path over all other default search pathes
	SET(CMAKE_FIND_ROOT_PATH ${CMAKE_OSX_SYSROOT})

	### options
	OPTION(OSX_BUILD_DEMOS "Compile benchsimple, luxcoredemo, luxcorescenedemo and luxcoreimplserializationdemo" FALSE)

	SET(LUXRAYS_NO_DEFAULT_CONFIG true)
	SET(LUXRAYS_CUSTOM_CONFIG "Config_OSX" CACHE STRING "")

	IF(NOT ${CMAKE_GENERATOR} MATCHES "Xcode") # will be set later in XCode
		# Setup binaries output directory in Xcode manner
		SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE} CACHE PATH "per configuration" FORCE)
		SET(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE} CACHE PATH "per configuration" FORCE)
	ELSE() # replace CMAKE_BUILD_TYPE with XCode env var $(CONFIGURATION) globally
		SET(CMAKE_BUILD_TYPE "$(CONFIGURATION)" )
	ENDIF()
		SET(CMAKE_BUILD_RPATH "@loader_path")
		SET(CMAKE_INSTALL_RPATH "@loader_path")

	#### OSX-flags by jensverwiebe
	ADD_DEFINITIONS(-Wno-everything -DHAVE_PTHREAD_H) # global compile definitions
	ADD_DEFINITIONS(-fvisibility=hidden -fvisibility-inlines-hidden)
	ADD_DEFINITIONS(-Wno-unused-local-typedef -Wno-unused-variable) # silence boost __attribute__((unused)) bug

  SET(CMAKE_CXX_STANDARD 11)
  SET(CMAKE_CXX_EXTENSIONS OFF)
  SET(CMAKE_CXX_STANDARD_REQUIRED ON)

  SET(OSX_FLAGS_RELEASE "-ftree-vectorize -msse -msse2 -msse3 -mssse3") # additional RELEASE flags

  SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${OSX_FLAGS_RELEASE}")
  SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${OSX_FLAGS_RELEASE}")

  SET(CMAKE_EXE_LINKER_FLAGS "-Wl,-unexported_symbols_list -Wl,\"${CMAKE_SOURCE_DIR}/cmake/exportmaps/unexported_symbols.map\"")
  SET(CMAKE_MODULE_LINKER_FLAGS "-Wl,-unexported_symbols_list -Wl,\"${CMAKE_SOURCE_DIR}/cmake/exportmaps/unexported_symbols.map\"")

  SET(CMAKE_XCODE_ATTRIBUTE_DEPLOYMENT_POSTPROCESSING YES) # strip symbols in whole project, disabled in pylux target
  SET(CMAKE_XCODE_ATTRIBUTE_DEAD_CODE_STRIPPING YES)
  SET(CMAKE_XCODE_ATTRIBUTE_LLVM_LTO YES)

  MESSAGE(STATUS "")
  MESSAGE(STATUS "################ GENERATED XCODE PROJECT INFORMATION ################")
  MESSAGE(STATUS "")
  MESSAGE(STATUS "DETECTED SYSTEM-VERSION: " ${MAC_SYS})
  MESSAGE(STATUS "DETECTED SDK-VERSION: " ${OSX_SYSTEM})
  MESSAGE(STATUS "OSX_DEPLOYMENT_TARGET : " ${CMAKE_OSX_DEPLOYMENT_TARGET})
  MESSAGE(STATUS "CMAKE_XCODE_ATTRIBUTE_ARCHS: " ${CMAKE_XCODE_ATTRIBUTE_ARCHS})
  MESSAGE(STATUS "OSX SDK SETTING : " ${CMAKE_XCODE_ATTRIBUTE_SDKROOT}${OSX_SYSTEM})
  MESSAGE(STATUS "XCODE_VERSION : " ${XCODE_VERS_BUILDNR})
  MESSAGE(STATUS "")

  IF(${CMAKE_GENERATOR} MATCHES "Xcode")
		MESSAGE(STATUS "BUILD_TYPE : Please set in Xcode ALL_BUILD target to aimed type")
  ELSE()
		MESSAGE(STATUS "BUILD_TYPE : " ${CMAKE_BUILD_TYPE} " - compile with: make " )
  ENDIF()
  MESSAGE(STATUS "")
  MESSAGE(STATUS "#####################################################################")

ENDIF(APPLE)

###########################################################################
#
# Linux and GCC
#
###########################################################################

IF(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	# Update if necessary
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wno-long-long -pedantic")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse -msse2 -msse3 -mssse3")
	IF(NOT CYGWIN)
		SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
	ENDIF(NOT CYGWIN)

	SET(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
	SET(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -O3 -ftree-vectorize -funroll-loops -fvariable-expansion-in-unroller")
ENDIF()

IF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	SET(CMAKE_EXE_LINKER_FLAGS -Wl,--disable-new-dtags,--version-script='${CMAKE_SOURCE_DIR}/cmake/exportmaps/linux_symbol_exports.map')
	SET(CMAKE_SHARED_LINKER_FLAGS -Wl,--disable-new-dtags,--version-script='${CMAKE_SOURCE_DIR}/cmake/exportmaps/linux_symbol_exports.map')
	SET(CMAKE_MODULE_LINKER_FLAGS -Wl,--disable-new-dtags,--version-script='${CMAKE_SOURCE_DIR}/cmake/exportmaps/linux_symbol_exports.map')
	SET(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
	SET(CMAKE_INSTALL_RPATH "$ORIGIN")
ENDIF()
