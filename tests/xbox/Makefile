XBE_TITLE = miniaudio_xbox
GEN_XISO = $(XBE_TITLE).iso
SRCS = $(CURDIR)/miniaudio_xbox.c
NXDK_DIR ?= /opt/toolchains/nxdk

all: 

include $(NXDK_DIR)/Makefile

TARGET += $(OUTPUT_DIR)/test.mp3
$(GEN_XISO): $(OUTPUT_DIR)/test.mp3
$(OUTPUT_DIR)/test.mp3: $(CURDIR)/test.mp3 $(OUTPUT_DIR)
	$(VE)cp '$<' '$@'
