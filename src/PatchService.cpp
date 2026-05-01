#include "PatchService.h"
#include <stdint.h>
#include <vector>

// global patch menu buffer
std::vector<patchmenu::PatchOption> patchOptions;

// currently selected option and value index
static int selectedPatchIndex = 0;
static int selectedPatchValueIndex = 0;

// Helpers

static int currentPatchIndex() {
  return selectedPatchIndex;
}

static int nextPatchIndex() {
  return (selectedPatchIndex + 1) % patchOptions.size();
}

static int prevPatchIndex() {
  if (selectedPatchIndex == 0) {
    return patchOptions.size() - 1;
  }
  return selectedPatchIndex - 1;
}

static void refresh_current_value_index() {
  selectedPatchValueIndex = patchOptions[currentPatchIndex()].currentIndex();
}

// Add new option

void patchmenu::append(PatchOption option) {
  patchOptions.push_back(option);

  if (patchOptions.size() == 1) {
    selectedPatchIndex = 0;
    refresh_current_value_index();
  }
}

void patchmenu::reset() {
  patchOptions.clear();
}

// Setting names

const char* patchmenu::current_setting() {
  return patchOptions[currentPatchIndex()].option;
}

const char* patchmenu::previous_setting() {
  return patchOptions[prevPatchIndex()].option;
}

const char* patchmenu::next_setting() {
  return patchOptions[nextPatchIndex()].option;
}

// Values

const char* patchmenu::previous_setting_value() {
  return patchOptions[prevPatchIndex()].value[patchOptions[prevPatchIndex()].currentIndex()];
}
const char* patchmenu::next_setting_value() {
  return patchOptions[nextPatchIndex()].value[patchOptions[nextPatchIndex()].currentIndex()];
}

const char* patchmenu::current_setting_value() {
  return patchOptions[currentPatchIndex()].value[selectedPatchValueIndex];
}

const char* patchmenu::current_setting_previous_value() {
  if (selectedPatchValueIndex == 0) {
    return "";
  }
  return patchOptions[currentPatchIndex()].value[selectedPatchValueIndex - 1];
}

const char* patchmenu::current_setting_next_value() {
  if (patchOptions[currentPatchIndex()].value[selectedPatchValueIndex + 1][0] == '\0') {
    return "";
  }
  return patchOptions[currentPatchIndex()].value[selectedPatchValueIndex + 1];
}

// Change settings

void patchmenu::increment_setting() {
  selectedPatchIndex = nextPatchIndex();
  refresh_current_value_index();
}

void patchmenu::decrement_setting() {
  selectedPatchIndex = prevPatchIndex();
  refresh_current_value_index();
}

// Change setting values

void patchmenu::increment_setting_value() {
  if (patchOptions[currentPatchIndex()].value[selectedPatchValueIndex + 1][0] == '\0') {
    return;
  }
  selectedPatchValueIndex++;
}

void patchmenu::decrement_setting_value() {
  if (selectedPatchValueIndex == 0) {
    return;
  }
  selectedPatchValueIndex--;
}

void patchmenu::save_current_value() {
  patchOptions[currentPatchIndex()].updateHandler(selectedPatchValueIndex, current_setting_value());
}

void patchmenu::refresh_value() {
  refresh_current_value_index();
}
