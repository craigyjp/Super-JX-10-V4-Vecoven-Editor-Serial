#pragma once

#define TONEOPTIONSNO 16

namespace tonemenu {

typedef void (*updater)(int index, const char* value);
typedef int (*index)();

struct ToneOption
{
  const char *  option;
  const char ** value;
  int           valueCount;
  updater       updateHandler;
  index         currentIndex;
};

const char* other_tone_value();
const char* current_setting();
const char* previous_setting();
const char* next_setting();

const char* previous_setting_value();
const char* next_setting_value();

const char* current_setting_value();
const char* current_setting_previous_value();
const char* current_setting_next_value();

void increment_setting();
void decrement_setting();

void increment_setting_value();
void decrement_setting_value();

void save_current_value();

// Call when upperSW flips so the displayed value re-reads from the
// currently-selected tone (upperData vs lowerData).
void refresh_value();

void append(ToneOption option);
void reset();

}
