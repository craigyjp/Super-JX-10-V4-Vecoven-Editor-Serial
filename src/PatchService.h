#pragma once

#define PATCHOPTIONSNO 32  // bump as you add more options

namespace patchmenu {

typedef void (*updater)(int index, const char* value);
typedef int (*index)();

struct PatchOption
{
  const char *  option;
  const char ** value;        // pointer to external array, any size
  int           valueCount;   // number of values (excluding sentinel)
  updater       updateHandler;
  index         currentIndex;
};

// setting names
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

// Forces re-read of selected value from currentIndex() callback.
// Call when external state changes (e.g. UPPER/LOWER toggled for tone-
// scope menus) so the value shown on screen reflects the live value.
void refresh_value();

void append(PatchOption option);
void reset();

}
