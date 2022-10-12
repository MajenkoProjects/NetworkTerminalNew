BOARD=esp32:esp32:esp32
OPTS=PSRAM=enabled,FlashSize=4M,PartitionScheme=min_spiffs,CPUFreq=240
SKETCH=$(shell basename $$(pwd))
HOST=192.168.0.69
PASS=changeme
PORT?=/dev/ttyUSB0


SUBDIR=$(subst :,.,$(BOARD))
BIN=bin/$(SKETCH).ino.bin
#IP=$(shell avahi-resolve -n $(HOST).local | cut -f2 -d'	')
IP=192.168.0.69
OTA=~/.arduino15/packages/esp32/hardware/esp32/1.0.6/tools/espota.py

$(BIN): $(SKETCH).ino
	arduino-cli compile --fqbn $(BOARD):$(OPTS) --output-dir bin

install: $(BIN)
	arduino-cli upload --fqbn $(BOARD):$(OPTS) -p $(PORT) 

upload: $(BIN)
	python $(OTA) -i $(HOST) "--auth=$(PASS)" -f $(BIN)

clean:
	rm -rf build bin

watch:
	echo ${SKETCH}.ino | entr -c -s 'make'
