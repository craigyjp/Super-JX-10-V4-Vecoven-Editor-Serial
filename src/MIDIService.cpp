#include "MIDIService.h"
#include <stdint.h>
#include <vector>

std::vector<midimenu::MIDIOption> midiOptions;

static int selectedMIDIIndex = 0;
static int selectedMIDIValueIndex = 0;

static int currentMIDIIndex() { return selectedMIDIIndex; }

static int nextMIDIIndex() {
  return (selectedMIDIIndex + 1) % midiOptions.size();
}

static int prevMIDIIndex() {
  if (selectedMIDIIndex == 0) return midiOptions.size() - 1;
  return selectedMIDIIndex - 1;
}

static void refresh_current_value_index() {
  selectedMIDIValueIndex = midiOptions[currentMIDIIndex()].currentIndex();
}

void midimenu::append(MIDIOption option) {
  midiOptions.push_back(option);
  if (midiOptions.size() == 1) {
    selectedMIDIIndex = 0;
    refresh_current_value_index();
  }
}

void midimenu::reset() { midiOptions.clear(); }

const char* midimenu::current_setting()  { return midiOptions[currentMIDIIndex()].option; }
const char* midimenu::previous_setting() { return midiOptions[prevMIDIIndex()].option; }
const char* midimenu::next_setting()     { return midiOptions[nextMIDIIndex()].option; }

const char* midimenu::previous_setting_value() {
  return midiOptions[prevMIDIIndex()].value[midiOptions[prevMIDIIndex()].currentIndex()];
}
const char* midimenu::next_setting_value() {
  return midiOptions[nextMIDIIndex()].value[midiOptions[nextMIDIIndex()].currentIndex()];
}

const char* midimenu::current_setting_value() {
  return midiOptions[currentMIDIIndex()].value[selectedMIDIValueIndex];
}

const char* midimenu::current_setting_previous_value() {
  if (selectedMIDIValueIndex == 0) return "";
  return midiOptions[currentMIDIIndex()].value[selectedMIDIValueIndex - 1];
}

const char* midimenu::current_setting_next_value() {
  if (midiOptions[currentMIDIIndex()].value[selectedMIDIValueIndex + 1][0] == '\0') return "";
  return midiOptions[currentMIDIIndex()].value[selectedMIDIValueIndex + 1];
}

void midimenu::increment_setting() {
  selectedMIDIIndex = nextMIDIIndex();
  refresh_current_value_index();
}

void midimenu::decrement_setting() {
  selectedMIDIIndex = prevMIDIIndex();
  refresh_current_value_index();
}

void midimenu::increment_setting_value() {
  if (midiOptions[currentMIDIIndex()].value[selectedMIDIValueIndex + 1][0] == '\0') return;
  selectedMIDIValueIndex++;
}

void midimenu::decrement_setting_value() {
  if (selectedMIDIValueIndex == 0) return;
  selectedMIDIValueIndex--;
}

void midimenu::save_current_value() {
  midiOptions[currentMIDIIndex()].updateHandler(selectedMIDIValueIndex, current_setting_value());
}

void midimenu::refresh_value() {
  refresh_current_value_index();
}
