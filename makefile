
BUILD_PATH=build
INC=-I. -I./include
CFLAGS=-g -Wall $(INC)

all:$(BUILD_PATH)/frerd $(BUILD_PATH)/frerctl

$(BUILD_PATH)/frerd:main.cc net.cc config.cc control.cc
	@if [ ! -d $(BUILD_PATH) ]; then mkdir -p $(BUILD_PATH); fi;
	$(CXX) $(CFLAGS) $^ -o $@

$(BUILD_PATH)/frerctl:tapctl.cc
	@if [ ! -d $(BUILD_PATH) ]; then mkdir -p $(BUILD_PATH); fi;
	$(CXX) $(CFLAGS) $^ -o $@

.PHONY:
clean:
	-rm -rf $(BUILD_PATH)
