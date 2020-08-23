BOARD=esp32:esp32:esp32
OPTS=PSRAM=enabled,FlashSize=4M,PartitionScheme=default,CPUFreq=240
SKETCH=NetworkTerminalNew
HOST=lat-01-07
PASS=wibble


SUBDIR=$(subst :,.,$(BOARD))
BIN=build/$(SUBDIR)/$(SKETCH).ino.bin
IP=$(shell avahi-resolve -n $(HOST).local | cut -f2 -d'	')
OTA=~/.arduino15/packages/esp32/hardware/esp32/1.0.4/tools/espota.py

$(BIN): $(SKETCH).ino
	arduino-cli compile --fqbn $(BOARD):$(OPTS)

install: $(BIN)
	python $(OTA) -i $(IP) "--auth=$(PASS)" -f $(BIN)

upload: $(BIN)
	arduino-cli upload --fqbn $(BOARD):$(OPTS) -p /dev/ttyUSB0

clean:
	rm -rf build
