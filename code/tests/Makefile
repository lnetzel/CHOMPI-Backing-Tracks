# Host tests for backing-tracks (no MCU/Daisy dependencies).
# Usage: make -C code/tests test

CXX      ?= g++
CC       ?= gcc
CXXFLAGS ?= -std=c++14 -O2 -Wall -Wextra -I../src

# coreJSON is vendored in this repo's code/libs (needed by BackingSet.h)
COREJSON_DIR = ../libs/coreJSON/source

test: backing_mixer_test backing_format_test backing_timing_test backing_transport_test backing_settings_test backing_midi_test
	./backing_mixer_test
	./backing_format_test
	./backing_timing_test
	./backing_transport_test
	./backing_settings_test
	./backing_midi_test

backing_mixer_test: backing_mixer_test.cpp ../src/BackingMixer.h
	$(CXX) $(CXXFLAGS) -o $@ $<

backing_format_test: backing_format_test.cpp ../src/BackingSet.h ../src/BackingTypes.h ../src/BackingWav.h ../libs/coreJSON/source/core_json.c
	$(CC) -std=c99 -O2 -I$(COREJSON_DIR)/include -c $(COREJSON_DIR)/core_json.c -o backing_core_json_test.o
	$(CXX) $(CXXFLAGS) -I$(COREJSON_DIR)/include -o $@ $< backing_core_json_test.o -lm
	rm -f backing_core_json_test.o

backing_timing_test: backing_timing_test.cpp ../src/BackingTiming.h ../src/BackingSet.h
	$(CXX) $(CXXFLAGS) -o $@ $<

backing_transport_test: backing_transport_test.cpp ../src/BackingTransport.h ../src/BackingFeedback.h
	$(CXX) $(CXXFLAGS) -o $@ $<

backing_settings_test: backing_settings_test.cpp ../src/BackingSettings.h ../libs/coreJSON/source/core_json.c
	$(CC) -std=c99 -O2 -I$(COREJSON_DIR)/include -c $(COREJSON_DIR)/core_json.c -o backing_settings_core_json_test.o
	$(CXX) $(CXXFLAGS) -I$(COREJSON_DIR)/include -o $@ $< backing_settings_core_json_test.o
	rm -f backing_settings_core_json_test.o

backing_midi_test: backing_midi_test.cpp ../src/BackingMidi.h ../src/BackingTiming.h ../src/BackingTypes.h
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f backing_mixer_test backing_mixer_test.exe backing_format_test backing_format_test.exe backing_timing_test backing_timing_test.exe backing_transport_test backing_transport_test.exe backing_settings_test backing_settings_test.exe backing_midi_test backing_midi_test.exe backing_core_json_test.o \
		core_json_test.o backing_settings_core_json_test.o midi_cc_map_test.exe
.PHONY: test clean
