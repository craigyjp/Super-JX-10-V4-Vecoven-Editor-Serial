#include "ToneService.h"
#include <stdint.h>
#include <vector>

extern bool upperSW;

std::vector<tonemenu::ToneOption> toneOptions;

static int selectedToneIndex = 0;
static int selectedToneValueIndex = 0;

static int currentToneIndex() { return selectedToneIndex; }

static int nextToneIndex() {
  return (selectedToneIndex + 1) % toneOptions.size();
}

static int prevToneIndex() {
  if (selectedToneIndex == 0) return toneOptions.size() - 1;
  return selectedToneIndex - 1;
}

static void refresh_current_value_index() {
  selectedToneValueIndex = toneOptions[currentToneIndex()].currentIndex();
}

void tonemenu::append(ToneOption option) {
  toneOptions.push_back(option);
  if (toneOptions.size() == 1) {
    selectedToneIndex = 0;
    refresh_current_value_index();
  }
}

void tonemenu::reset() { toneOptions.clear(); }

const char* tonemenu::current_setting()  { return toneOptions[currentToneIndex()].option; }
const char* tonemenu::previous_setting() { return toneOptions[prevToneIndex()].option; }
const char* tonemenu::next_setting()     { return toneOptions[nextToneIndex()].option; }

const char* tonemenu::previous_setting_value() {
  return toneOptions[prevToneIndex()].value[toneOptions[prevToneIndex()].currentIndex()];
}
const char* tonemenu::next_setting_value() {
  return toneOptions[nextToneIndex()].value[toneOptions[nextToneIndex()].currentIndex()];
}

const char* tonemenu::current_setting_value() {
  return toneOptions[currentToneIndex()].value[selectedToneValueIndex];
}

const char* tonemenu::other_tone_value() {
  const ToneOption& opt = toneOptions[currentToneIndex()];

  bool saved = upperSW;
  upperSW = !saved;
  int idx = opt.currentIndex();
  upperSW = saved;

  if (idx < 0)               idx = 0;
  if (idx >= opt.valueCount) idx = opt.valueCount - 1;
  return opt.value[idx];
}

const char* tonemenu::current_setting_previous_value() {
  if (selectedToneValueIndex == 0) return "";
  return toneOptions[currentToneIndex()].value[selectedToneValueIndex - 1];
}

const char* tonemenu::current_setting_next_value() {
  if (toneOptions[currentToneIndex()].value[selectedToneValueIndex + 1][0] == '\0') return "";
  return toneOptions[currentToneIndex()].value[selectedToneValueIndex + 1];
}

void tonemenu::increment_setting() {
  selectedToneIndex = nextToneIndex();
  refresh_current_value_index();
}

void tonemenu::decrement_setting() {
  selectedToneIndex = prevToneIndex();
  refresh_current_value_index();
}

void tonemenu::increment_setting_value() {
  if (toneOptions[currentToneIndex()].value[selectedToneValueIndex + 1][0] == '\0') return;
  selectedToneValueIndex++;
}

void tonemenu::decrement_setting_value() {
  if (selectedToneValueIndex == 0) return;
  selectedToneValueIndex--;
}

void tonemenu::save_current_value() {
  toneOptions[currentToneIndex()].updateHandler(selectedToneValueIndex, current_setting_value());
}

void tonemenu::refresh_value() {
  refresh_current_value_index();
}
