#pragma once

#define MIDIOPTIONSNO 8

namespace midimenu {

typedef void (*updater)(int index, const char* value);
typedef int (*index)();

struct MIDIOption
{
  const char *  option;
  const char ** value;
  int           valueCount;
  updater       updateHandler;
  index         currentIndex;
};

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
void refresh_value();

void append(MIDIOption option);
void reset();

}
