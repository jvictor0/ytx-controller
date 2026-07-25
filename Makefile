# Top-level build wrapper for YTX controller firmware.
#
# Builds:
# - AUX firmware via its own Makefile
# - MAIN firmware via arduino-cli

AUX_DIR ?= ytx-aux-controller/SAMD11-NeoPixel
MAIN_SKETCH ?= ytx-main-controller
MAIN_FQBN ?= yaeltexv2:samd:kilomuxv2-main
MAIN_OUTPUT_DIR ?= $(MAIN_SKETCH)/build
ARDUINO_CLI ?= arduino-cli
PRIMARY_WORKTREE ?= $(abspath $(dir $(shell git rev-parse --git-common-dir 2>/dev/null)))
PROJECT_VENV_PYTHON ?= $(PRIMARY_WORKTREE)/tools/.venv/bin/python
PYTHON ?= $(if $(wildcard tools/.venv/bin/python),tools/.venv/bin/python,$(if $(wildcard $(PROJECT_VENV_PYTHON)),$(PROJECT_VENV_PYTHON),python3))
INSTALL_SCRIPT ?= tools/install_split_firmware.py
MAIN_BIN ?= $(MAIN_OUTPUT_DIR)/ytx-main-controller.ino.app.bin
AUX_BIN ?= $(AUX_DIR)/build/SAMD11-NeoPixel.bin
BOOT_PORT ?= KilomuxBOOT
AUTO_BOOT_FROM ?= WRLD.BLDR
APP_PORT ?=
INSTALL_TIMEOUT ?= 20
INSTALL_BEGIN_TIMEOUT ?= 90
BOOT_PORT_INDEX ?= 0
INSTALL_DEBUG ?= 0

.DEFAULT_GOAL := all

.PHONY: all aux main clean clean-aux clean-main install install-aux list-ports help

all: aux main

aux:
	$(MAKE) -C $(AUX_DIR) all

main:
	@command -v $(ARDUINO_CLI) >/dev/null 2>&1 || { \
		echo "error: '$(ARDUINO_CLI)' not found in PATH"; \
		echo "hint: brew install arduino-cli"; \
		exit 1; \
	}
	$(ARDUINO_CLI) compile --fqbn $(MAIN_FQBN) --output-dir $(MAIN_OUTPUT_DIR) $(MAIN_SKETCH)

clean: clean-aux clean-main

clean-aux:
	$(MAKE) -C $(AUX_DIR) clean

clean-main:
	rm -rf $(MAIN_OUTPUT_DIR)

install: all
	@command -v $(PYTHON) >/dev/null 2>&1 || { \
		echo "error: '$(PYTHON)' not found"; \
		exit 1; \
	}
	@$(PYTHON) -c "import mido" >/dev/null 2>&1 || { \
		echo "error: Python package 'mido' is missing for $(PYTHON)"; \
		echo "hint: pip install mido python-rtmidi"; \
		exit 1; \
	}
	@test -f "$(MAIN_BIN)" || { \
		echo "error: main firmware binary not found at $(MAIN_BIN)"; \
		echo "hint: set MAIN_BIN=<path/to/main.bin>"; \
		exit 1; \
	}
	@test -f "$(AUX_BIN)" || { \
		echo "error: aux firmware binary not found at $(AUX_BIN)"; \
		echo "hint: set AUX_BIN=<path/to/aux.bin>"; \
		exit 1; \
	}
	$(PYTHON) $(INSTALL_SCRIPT) \
		--boot-port "$(BOOT_PORT)" \
		--auto-boot-from "$(AUTO_BOOT_FROM)" \
		$(if $(strip $(APP_PORT)),--app-port "$(APP_PORT)",) \
		$(if $(strip $(APP_PORT)),--force-boot,) \
		--main-bin "$(MAIN_BIN)" \
		--aux-bin "$(AUX_BIN)" \
		--timeout "$(INSTALL_TIMEOUT)" \
		--begin-timeout "$(INSTALL_BEGIN_TIMEOUT)" \
		--boot-port-index "$(BOOT_PORT_INDEX)" \
		$(if $(filter 1,$(INSTALL_DEBUG)),--debug,)

install-aux:
	$(MAKE) -C $(AUX_DIR) install

list-ports:
	@command -v $(PYTHON) >/dev/null 2>&1 || { \
		echo "error: '$(PYTHON)' not found"; \
		exit 1; \
	}
	$(PYTHON) $(INSTALL_SCRIPT) --list-ports

help:
	@echo "YTX firmware build targets:"
	@echo "  make all        Build AUX and MAIN firmware"
	@echo "  make aux        Build AUX firmware only"
	@echo "  make main       Build MAIN firmware only"
	@echo "  make clean      Clean AUX and MAIN build outputs"
	@echo "  make install    Build + install MAIN and AUX via bootloader MIDI"
	@echo "  make list-ports List MIDI ports seen by uploader script"
	@echo "  make install-aux  Copy AUX bin via AUX makefile rule"
	@echo ""
	@echo "Configurable vars:"
	@echo "  AUX_DIR=$(AUX_DIR)"
	@echo "  MAIN_SKETCH=$(MAIN_SKETCH)"
	@echo "  MAIN_FQBN=$(MAIN_FQBN)"
	@echo "  MAIN_OUTPUT_DIR=$(MAIN_OUTPUT_DIR)"
	@echo "  MAIN_BIN=$(MAIN_BIN)"
	@echo "  AUX_BIN=$(AUX_BIN)"
	@echo "  BOOT_PORT=$(BOOT_PORT)"
	@echo "  AUTO_BOOT_FROM=$(AUTO_BOOT_FROM)"
	@echo "  APP_PORT=$(APP_PORT)"
	@echo "  INSTALL_TIMEOUT=$(INSTALL_TIMEOUT)"
	@echo "  INSTALL_BEGIN_TIMEOUT=$(INSTALL_BEGIN_TIMEOUT)"
	@echo "  BOOT_PORT_INDEX=$(BOOT_PORT_INDEX)"
	@echo "  INSTALL_DEBUG=$(INSTALL_DEBUG)"
	@echo "  ARDUINO_CLI=$(ARDUINO_CLI)"
	@echo "  PYTHON=$(PYTHON)"
