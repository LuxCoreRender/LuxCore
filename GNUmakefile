# Credits to Blender Cycles

ifeq ($(OS),Windows_NT)
	$(error On Windows, use "cmd //c make.bat" instead of "make")
endif

OS:=$(shell uname -s)

ifndef BUILD_CMAKE_ARGS
	BUILD_CMAKE_ARGS:=
endif

ifndef BUILD_DIR
	BUILD_DIR:=./build
endif

SOURCE_DIR:=$(shell pwd)

ifndef PYTHON
	PYTHON:=python3
endif

.PHONY: clean deps config luxcore pyluxcore luxcoreui luxcoreconsole

all: luxcore pyluxcore luxcoreui luxcoreconsole

luxcore: config
	cmake $(BUILD_CMAKE_ARGS) --build --preset conan-release --target luxcore

pyluxcore: config
	cmake $(BUILD_CMAKE_ARGS) --build --preset conan-release --target pyluxcore

luxcoreui: config
	cmake $(BUILD_CMAKE_ARGS) --build --preset conan-release --target luxcoreui

luxcoreconsole: config
	cmake $(BUILD_CMAKE_ARGS) --build --preset conan-release --target luxcoreconsole

# TODO Make debug targets

clean:
	cmake --build --preset conan-release --target clean

config:
	cmake $(BUILD_CMAKE_ARGS) --preset conan-release -S $(SOURCE_DIR)

# Presets independant
clear:
	rm -rf $(BUILD_DIR)

deps:
	$(PYTHON) cmake/make_deps.py

list-presets:
	cmake --list-presets
