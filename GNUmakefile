# Credits to Blender Cycles

OS:=$(shell uname -s)

ifeq ($(OS),Windows_NT)
	$(error On Windows, use "cmd //c make.bat" instead of "make")
endif


ifndef BUILD_CMAKE_ARGS
	BUILD_CMAKE_ARGS:=
endif

ifndef BUILD_DIR
	BUILD_DIR:=./build
endif

ifndef INSTALL_DIR
	INSTALL_DIR:=./build
endif


SOURCE_DIR:=$(shell pwd)
SOURCE_DIR=.


ifndef PYTHON
	PYTHON:=python3
endif

.PHONY: clean deps config luxcore pyluxcore luxcoreui luxcoreconsole

all: pyluxcore luxcoreui luxcoreconsole

luxcore: _TARGET = LUXCORE_LIBONLY
luxcore: config
	cmake $(BUILD_CMAKE_ARGS) --build --preset conan-release

pyluxcore: _TARGET = PYLUXCORE_LIBONLY
pyluxcore: config
	cmake $(BUILD_CMAKE_ARGS) --build --preset conan-release

luxcoreui: _TARGET = LUXCORE_UI
luxcoreui: config
	cmake $(BUILD_CMAKE_ARGS) --build --preset conan-release

luxcoreconsole: _TARGET = LUXCORE_CON
luxcoreconsole: config
	cmake $(BUILD_CMAKE_ARGS) --build --preset conan-release

# TODO Make debug targets

clean:
	cmake --build --preset conan-release --target clean

config:
	cmake $(BUILD_CMAKE_ARGS) --preset conan-release \
		-DCMAKE_INSTALL_PREFIX=$(INSTALL_DIR) \
		-S $(SOURCE_DIR) \
		-D$(_TARGET)=ON

install:
	cmake --install $(BUILD_DIR)/cmake --prefix $(BUILD_DIR)

# Presets independant
clear:
	rm -rf $(BUILD_DIR)

deps:
	$(PYTHON) cmake/make_deps.py

list-presets:
	cmake --list-presets
