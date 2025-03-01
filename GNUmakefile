# Credits to Blender Cycles

# TODO Make debug targets
#
ifndef BUILD_CMAKE_ARGS
	BUILD_CMAKE_ARGS:=
endif

ifndef OUTPUT_DIR
	OUTPUT_DIR:=out
endif

BUILD_DIR = ${OUTPUT_DIR}/build
INSTALL_DIR = ${OUTPUT_DIR}/install

SOURCE_DIR:=$(shell pwd)


ifndef PYTHON
	PYTHON:=python3
endif


build-targets = pyluxcore luxcoreui luxcoreconsole luxcore

.PHONY: clean deps config luxcore pyluxcore luxcoreui luxcoreconsole

all: luxcore pyluxcore luxcoreui luxcoreconsole

$(build-targets): %: config
	cmake $(BUILD_CMAKE_ARGS) --build --preset conan-release --target $*
	cmake --install $(BUILD_DIR)/Release --prefix $(INSTALL_DIR) --component $*

config:
	cmake $(BUILD_CMAKE_ARGS) --preset conan-release \
		-DCMAKE_INSTALL_PREFIX=$(INSTALL_DIR) \
		-S $(SOURCE_DIR)

clean:
	cmake --build --preset conan-release --target clean

install:
	cmake --install $(BUILD_DIR)/Release --prefix $(BUILD_DIR)

# Preset-independant targets
clear:
	rm -rf $(OUTPUT_DIR)

deps:
	$(PYTHON) cmake/make_deps.py -o ${OUTPUT_DIR}

list-presets:
	cmake --list-presets
