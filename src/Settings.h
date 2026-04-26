#include "SettingsService.h"

void sendTuneCommands(uint8_t masterTune);

// ─────────────────────────────────────────────
// Master Tune label table
// 128 entries, index 0x00–0x7F
// 0x2C (44) = A 440.00 Hz
// ─────────────────────────────────────────────
static char masterTuneLabels[128][8];
static const char *masterTunePtrs[129];  // 128 + sentinel

void buildMasterTuneLabels() {
  for (int i = 0; i < 128; i++) {
    float hz;
    if (i < 44) {
      hz = 435.8f + (440.0f - 435.8f) * (float)i / 44.0f;
    } else if (i == 44) {
      hz = 440.0f;                        // anchor A440 exactly
    } else {
      hz = 440.0f + (448.1f - 440.0f) * (float)(i - 44) / 83.0f;
    }
    dtostrf(hz, 5, 1, masterTuneLabels[i]);
    masterTunePtrs[i] = masterTuneLabels[i];
  }
  masterTunePtrs[128] = "\0";            // sentinel
}

// ─────────────────────────────────────────────
// Forward declarations
// ─────────────────────────────────────────────
void settingsMasterTune(int index, const char *value);
void settingsMIDICh(int index, const char *value);
void settingsMIDIOutCh(int index, const char *value);
void settingsEncoderDir(int index, const char *value);
void settingsUpdateParams(int index, const char *value);

int currentIndexMasterTune();
int currentIndexMIDICh();
int currentIndexMIDIOutCh();
int currentIndexEncoderDir();
int currentIndexUpdateParams();

// ─────────────────────────────────────────────
// Handlers
// ─────────────────────────────────────────────
void settingsMasterTune(int index, const char *value) {
  masterTune = (uint8_t)index;           // index IS the hex value, direct 1:1
  storeMasterTune(masterTune);

  sendTuneCommands(masterTune);
}

void settingsMIDICh(int index, const char *value) {
  if (strcmp(value, "ALL") == 0) {
    midiChannel = MIDI_CHANNEL_OMNI;
  } else {
    midiChannel = atoi(value);
  }
  storeMidiChannel(midiChannel);
}

void settingsMIDIOutCh(int index, const char *value) {
  if (strcmp(value, "Off") == 0) {
    midiOutCh = 0;
  } else {
    midiOutCh = atoi(value);
  }
  storeMidiOutCh(midiOutCh);
}

void settingsEncoderDir(int index, const char *value) {
  if (strcmp(value, "Type 1") == 0) {
    encCW = true;
  } else {
    encCW = false;
  }
  storeEncoderDir(encCW ? 1 : 0);
}

void settingsUpdateParams(int index, const char *value) {
  if (strcmp(value, "Send Params") == 0) {
    updateParams = true;
  } else {
    updateParams = false;
  }
  storeUpdateParams(updateParams ? 1 : 0);
}

// ─────────────────────────────────────────────
// Current index functions
// ─────────────────────────────────────────────
int currentIndexMasterTune() {
  return (int)getMasterTune();           // 0x2C → 44, direct 1:1
}

int currentIndexMIDICh() {
  return getMIDIChannel();
}

int currentIndexMIDIOutCh() {
  return getMIDIOutCh();
}

int currentIndexEncoderDir() {
  return getEncoderDir() ? 0 : 1;
}

int currentIndexUpdateParams() {
  return getUpdateParams() ? 1 : 0;
}

// ─────────────────────────────────────────────
// Setup — build labels first, then append
// ─────────────────────────────────────────────
static const char* midiChValues[]     = { "All", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "\0" };
static const char* midiOutChValues[]  = { "Off", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "\0" };
static const char* encoderValues[]    = { "Type 1", "Type 2", "\0" };
static const char* midiParamValues[]  = { "Off", "Send Params", "\0" };

void setUpSettings() {
  buildMasterTuneLabels();

  settings::append({ "Master Tune",  masterTunePtrs,   128, settingsMasterTune,  currentIndexMasterTune });
  settings::append({ "MIDI Ch.",     midiChValues,      17, settingsMIDICh,      currentIndexMIDICh });
  settings::append({ "MIDI Out Ch.", midiOutChValues,   17, settingsMIDIOutCh,   currentIndexMIDIOutCh });
  settings::append({ "Encoder",      encoderValues,      2, settingsEncoderDir,  currentIndexEncoderDir });
  settings::append({ "MIDI Params",  midiParamValues,    2, settingsUpdateParams,currentIndexUpdateParams });
}
