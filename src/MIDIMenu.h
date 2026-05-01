#include "MIDIService.h"

// ============================================================================
// MIDIMenu.h — MIDI menu entries with one variable per setting
//
// Each entry stores its value in EEPROM via the matching get/store pair
// in EepromMgr.h. The variables themselves live as globals (declared in
// Parameters.h) and are loaded once at boot via loadMidiSettings().
// ============================================================================

// --- Label arrays ---
// Channel arrays: index 0 = "OFF" (or "ALL"), 1..16 = channel number
static const char* midiChannelOffValues[]    = { "OFF", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "\0" };
static const char* midiSwitchValues[]        = { "OFF", "ON", "\0" };
static const char* midiSysexFourStateValues[]= { "OFF", "RCV", "SEND", "ON", "\0" };

// --- Forward declarations of all handlers and getters ---

// Existing (kept for backward compatibility with Settings menu)
void midiMIDICh(int index, const char *value);
void midiMIDIOutCh(int index, const char *value);
void midiUpdateParams(int index, const char *value);
int  idxMIDICh();
int  idxMIDIOutCh();
int  idxUpdateParams();

// New per-setting handlers
void setUpperLocal     (int index, const char *value);
void setLowerLocal     (int index, const char *value);
void setUpperChannel   (int index, const char *value);
void setLowerChannel   (int index, const char *value);
void setControlChannel (int index, const char *value);
void setPatchProgChange(int index, const char *value);
void setSysexExclusive (int index, const char *value);
void setSysexApr       (int index, const char *value);
void setRealTime       (int index, const char *value);
void setUpperProgChange(int index, const char *value);
void setUpperAfterTouch(int index, const char *value);
void setUpperBender    (int index, const char *value);
void setUpperModulation(int index, const char *value);
void setUpperPortamento(int index, const char *value);
void setUpperHold      (int index, const char *value);
void setUpperVolume    (int index, const char *value);
void setLowerProgChange(int index, const char *value);
void setLowerAfterTouch(int index, const char *value);
void setLowerBender    (int index, const char *value);
void setLowerModulation(int index, const char *value);
void setLowerPortamento(int index, const char *value);
void setLowerHold      (int index, const char *value);
void setLowerVolume    (int index, const char *value);
void setC1C2ToneEdit   (int index, const char *value);

int idxUpperLocal();
int idxLowerLocal();
int idxUpperChannel();
int idxLowerChannel();
int idxControlChannel();
int idxPatchProgChange();
int idxSysexExclusive();
int idxSysexApr();
int idxRealTime();
int idxUpperProgChange();
int idxUpperAfterTouch();
int idxUpperBender();
int idxUpperModulation();
int idxUpperPortamento();
int idxUpperHold();
int idxUpperVolume();
int idxLowerProgChange();
int idxLowerAfterTouch();
int idxLowerBender();
int idxLowerModulation();
int idxLowerPortamento();
int idxLowerHold();
int idxLowerVolume();
int idxC1C2ToneEdit();

// --- Existing handlers (unchanged) ---
void midiMIDICh(int index, const char *value) {
  if (strcmp(value, "ALL") == 0) {
    midiChannel = MIDI_CHANNEL_OMNI;
  } else {
    midiChannel = atoi(value);
  }
  storeMidiChannel(midiChannel);
}

void midiMIDIOutCh(int index, const char *value) {
  if (strcmp(value, "OFF") == 0) {
    midiOutCh = 0;
  } else {
    midiOutCh = atoi(value);
  }
  storeMidiOutCh(midiOutCh);
}

void midiUpdateParams(int index, const char *value) {
  updateParams = (strcmp(value, "SEND PARAMS") == 0);
  storeUpdateParams(updateParams ? 1 : 0);
}

int idxMIDICh()       { return getMIDIChannel(); }
int idxMIDIOutCh()    { return getMIDIOutCh(); }
int idxUpdateParams() { return getUpdateParams() ? 1 : 0; }

// --- New per-setting handlers — each just stores index to its variable + EEPROM ---

#define MIDI_SETTER(NAME, VAR, STORE)                       \
  void NAME(int index, const char *value) {                 \
    (void)value;                                            \
    VAR = (byte)index;                                      \
    STORE((byte)index);                                     \
  }                                                          \
  int idx##NAME() { return (int)VAR; }

MIDI_SETTER(setUpperLocal,      upperLocal,      storeUpperLocal)
MIDI_SETTER(setLowerLocal,      lowerLocal,      storeLowerLocal)
MIDI_SETTER(setUpperChannel,    upperChannel,    storeUpperChannel)
MIDI_SETTER(setLowerChannel,    lowerChannel,    storeLowerChannel)
MIDI_SETTER(setControlChannel,  controlChannel,  storeControlChannel)
MIDI_SETTER(setPatchProgChange, patchProgChange, storePatchProgChange)
MIDI_SETTER(setSysexExclusive,  sysexExclusive,  storeSysexExclusive)
MIDI_SETTER(setSysexApr,        sysexApr,        storeSysexApr)
MIDI_SETTER(setRealTime,        realTime,        storeRealTime)
MIDI_SETTER(setUpperProgChange, upperProgChange, storeUpperProgChange)
MIDI_SETTER(setUpperAfterTouch, upperAfterTouch, storeUpperAfterTouch)
MIDI_SETTER(setUpperBender,     upperBender,     storeUpperBender)
MIDI_SETTER(setUpperModulation, upperModulation, storeUpperModulation)
MIDI_SETTER(setUpperPortamento, upperPortamento, storeUpperPortamento)
MIDI_SETTER(setUpperHold,       upperMIDIHold,       storeUpperHold)
MIDI_SETTER(setUpperVolume,     upperVolume,     storeUpperVolume)
MIDI_SETTER(setLowerProgChange, lowerProgChange, storeLowerProgChange)
MIDI_SETTER(setLowerAfterTouch, lowerAfterTouch, storeLowerAfterTouch)
MIDI_SETTER(setLowerBender,     lowerBender,     storeLowerBender)
MIDI_SETTER(setLowerModulation, lowerModulation, storeLowerModulation)
MIDI_SETTER(setLowerPortamento, lowerPortamento, storeLowerPortamento)
MIDI_SETTER(setLowerHold,       lowerMIDIHold,       storeLowerHold)
MIDI_SETTER(setLowerVolume,     lowerVolume,     storeLowerVolume)
MIDI_SETTER(setC1C2ToneEdit,    c1c2ToneEdit,    storeC1C2ToneEdit)

#undef MIDI_SETTER

// ============================================================================
// setUpMIDI()
// ============================================================================

void setUpMIDI() {
  midimenu::append({ "11 UPPER LOCAL",            midiSwitchValues,         2,  setUpperLocal,      idxsetUpperLocal      });
  midimenu::append({ "12 LOWER LOCAL",            midiSwitchValues,         2,  setLowerLocal,      idxsetLowerLocal      });
  midimenu::append({ "13 UPPER CHANNEL",          midiChannelOffValues,    17,  setUpperChannel,    idxsetUpperChannel    });
  midimenu::append({ "14 LOWER CHANNEL",          midiChannelOffValues,    17,  setLowerChannel,    idxsetLowerChannel    });
  midimenu::append({ "15 CONTROL CHANNEL",        midiChannelOffValues,    17,  setControlChannel,  idxsetControlChannel  });
  midimenu::append({ "16 PATCH PROGRAM CHANGE",   midiSysexFourStateValues, 4,  setPatchProgChange, idxsetPatchProgChange });

  midimenu::append({ "21 SYSEX EXCLUSIVE",        midiSysexFourStateValues, 4,  setSysexExclusive,  idxsetSysexExclusive  });
  midimenu::append({ "22 SYSEX APR",              midiSysexFourStateValues, 4,  setSysexApr,        idxsetSysexApr        });
  midimenu::append({ "24 REAL TIME",              midiSwitchValues,         2,  setRealTime,        idxsetRealTime        });

  midimenu::append({ "31 UPPER PROGRAM CHANGE",   midiSysexFourStateValues, 4,  setUpperProgChange, idxsetUpperProgChange });
  midimenu::append({ "32 UPPER AFTER TOUCH",      midiSysexFourStateValues, 4,  setUpperAfterTouch, idxsetUpperAfterTouch });
  midimenu::append({ "33 UPPER BENDER",           midiSysexFourStateValues, 4,  setUpperBender,     idxsetUpperBender     });
  midimenu::append({ "34 UPPER MODULATION",       midiSysexFourStateValues, 4,  setUpperModulation, idxsetUpperModulation });
  midimenu::append({ "35 UPPER PORTAMENTO",       midiSysexFourStateValues, 4,  setUpperPortamento, idxsetUpperPortamento });
  midimenu::append({ "36 UPPER HOLD",             midiSysexFourStateValues, 4,  setUpperHold,       idxsetUpperHold       });
  midimenu::append({ "37 UPPER MIDI VOLUME",      midiSysexFourStateValues, 4,  setUpperVolume,     idxsetUpperVolume     });

  midimenu::append({ "41 LOWER PROGRAM CHANGE",   midiSysexFourStateValues, 4,  setLowerProgChange, idxsetLowerProgChange });
  midimenu::append({ "42 LOWER AFTER TOUCH",      midiSysexFourStateValues, 4,  setLowerAfterTouch, idxsetLowerAfterTouch });
  midimenu::append({ "43 LOWER BENDER",           midiSysexFourStateValues, 4,  setLowerBender,     idxsetLowerBender     });
  midimenu::append({ "44 LOWER MODULATION",       midiSysexFourStateValues, 4,  setLowerModulation, idxsetLowerModulation });
  midimenu::append({ "45 LOWER PORTAMENTO",       midiSysexFourStateValues, 4,  setLowerPortamento, idxsetLowerPortamento });
  midimenu::append({ "46 LOWER HOLD",             midiSysexFourStateValues, 4,  setLowerHold,       idxsetLowerHold       });
  midimenu::append({ "47 LOWER MIDI VOLUME",      midiSysexFourStateValues, 4,  setLowerVolume,     idxsetLowerVolume     });

  midimenu::append({ "53 C1 C2 TONE EDIT",        midiSwitchValues,         2,  setC1C2ToneEdit,    idxsetC1C2ToneEdit    });
}
