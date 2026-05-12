#include <EEPROM.h>

#define EEPROM_MIDI_CH 0
#define EEPROM_ENCODER_DIR 1
#define EEPROM_LAST_PATCH 2
#define EEPROM_MIDI_OUT_CH 3
#define EEPROM_MASTER_TUNE   4   // fits in the gap
#define EEPROM_UPDATE_PARAMS 5
// Existing addresses 0-5...
#define EEPROM_UPPER_LOCAL          6
#define EEPROM_LOWER_LOCAL          7
#define EEPROM_UPPER_CHANNEL        8
#define EEPROM_LOWER_CHANNEL        9
#define EEPROM_CONTROL_CHANNEL      10
#define EEPROM_PATCH_PROG_CHANGE    11
#define EEPROM_SYSEX_EXCLUSIVE      12
#define EEPROM_SYSEX_APR            13
#define EEPROM_REAL_TIME            14
#define EEPROM_UPPER_PROG_CHANGE    15
#define EEPROM_UPPER_AFTER_TOUCH    16
#define EEPROM_UPPER_BENDER         17
#define EEPROM_UPPER_MODULATION     18
#define EEPROM_UPPER_PORTAMENTO     19
#define EEPROM_UPPER_HOLD           20
#define EEPROM_UPPER_VOLUME         21
#define EEPROM_LOWER_PROG_CHANGE    22
#define EEPROM_LOWER_AFTER_TOUCH    23
#define EEPROM_LOWER_BENDER         24
#define EEPROM_LOWER_MODULATION     25
#define EEPROM_LOWER_PORTAMENTO     26
#define EEPROM_LOWER_HOLD           27
#define EEPROM_LOWER_VOLUME         28
#define EEPROM_C1_C2_TONE_EDIT      29
#define EEPROM_PEDAL_ASSIGN         30
#define EEPROM_C1_ASSIGN            31
#define EEPROM_C2_ASSIGN            32

// --- Generic helpers for switch (0-1), channel (0-16), sysex (0-3) bytes ---
// All clamp to safe defaults if EEPROM is uninitialised.

static byte readByteOrDefault(int addr, byte minV, byte maxV, byte deflt) {
  byte v = EEPROM.read(addr);
  if (v < minV || v > maxV) return deflt;
  return v;
}

// --- Switch settings (0=OFF, 1=ON) ---
byte getUpperLocal()    { return readByteOrDefault(EEPROM_UPPER_LOCAL,    0, 1, 1); }  // default ON
byte getLowerLocal()    { return readByteOrDefault(EEPROM_LOWER_LOCAL,    0, 1, 1); }
byte getRealTime()      { return readByteOrDefault(EEPROM_REAL_TIME,      0, 1, 1); }
byte getC1C2ToneEdit()  { return readByteOrDefault(EEPROM_C1_C2_TONE_EDIT,0, 1, 0); }

void storeUpperLocal(byte v)    { EEPROM.update(EEPROM_UPPER_LOCAL,    v); }
void storeLowerLocal(byte v)    { EEPROM.update(EEPROM_LOWER_LOCAL,    v); }
void storeRealTime(byte v)      { EEPROM.update(EEPROM_REAL_TIME,      v); }
void storeC1C2ToneEdit(byte v)  { EEPROM.update(EEPROM_C1_C2_TONE_EDIT, v); }

// --- Channel settings (0=OFF, 1-16=channel) ---
byte getUpperChannel()    { return readByteOrDefault(EEPROM_UPPER_CHANNEL,    0, 16, 1); }
byte getLowerChannel()    { return readByteOrDefault(EEPROM_LOWER_CHANNEL,    0, 16, 1); }
byte getControlChannel()  { return readByteOrDefault(EEPROM_CONTROL_CHANNEL,  0, 16, 1); }

void storeUpperChannel(byte v)    { EEPROM.update(EEPROM_UPPER_CHANNEL,    v); }
void storeLowerChannel(byte v)    { EEPROM.update(EEPROM_LOWER_CHANNEL,    v); }
void storeControlChannel(byte v)  { EEPROM.update(EEPROM_CONTROL_CHANNEL,  v); }

// --- Sysex four-state (0=OFF, 1=RCV, 2=SEND, 3=ON) ---
byte getPatchProgChange()    { return readByteOrDefault(EEPROM_PATCH_PROG_CHANGE,    0, 3, 3); }
byte getSysexExclusive()     { return readByteOrDefault(EEPROM_SYSEX_EXCLUSIVE,      0, 3, 3); }
byte getSysexApr()           { return readByteOrDefault(EEPROM_SYSEX_APR,            0, 3, 3); }
byte getUpperProgChange()    { return readByteOrDefault(EEPROM_UPPER_PROG_CHANGE,    0, 3, 3); }
byte getUpperAfterTouch()    { return readByteOrDefault(EEPROM_UPPER_AFTER_TOUCH,    0, 3, 3); }
byte getUpperBender()        { return readByteOrDefault(EEPROM_UPPER_BENDER,         0, 3, 3); }
byte getUpperModulation()    { return readByteOrDefault(EEPROM_UPPER_MODULATION,     0, 3, 3); }
byte getUpperPortamento()    { return readByteOrDefault(EEPROM_UPPER_PORTAMENTO,     0, 3, 3); }
byte getUpperHold()          { return readByteOrDefault(EEPROM_UPPER_HOLD,           0, 3, 3); }
byte getUpperVolume()        { return readByteOrDefault(EEPROM_UPPER_VOLUME,         0, 3, 3); }
byte getLowerProgChange()    { return readByteOrDefault(EEPROM_LOWER_PROG_CHANGE,    0, 3, 3); }
byte getLowerAfterTouch()    { return readByteOrDefault(EEPROM_LOWER_AFTER_TOUCH,    0, 3, 3); }
byte getLowerBender()        { return readByteOrDefault(EEPROM_LOWER_BENDER,         0, 3, 3); }
byte getLowerModulation()    { return readByteOrDefault(EEPROM_LOWER_MODULATION,     0, 3, 3); }
byte getLowerPortamento()    { return readByteOrDefault(EEPROM_LOWER_PORTAMENTO,     0, 3, 3); }
byte getLowerHold()          { return readByteOrDefault(EEPROM_LOWER_HOLD,           0, 3, 3); }
byte getLowerVolume()        { return readByteOrDefault(EEPROM_LOWER_VOLUME,         0, 3, 3); }
byte getPedalAssign()        { return EEPROM.read(EEPROM_PEDAL_ASSIGN); }
byte getC1Assign()           { return EEPROM.read(EEPROM_C1_ASSIGN); }
byte getC2Assign()           { return EEPROM.read(EEPROM_C2_ASSIGN); }

void storePedalAssign(byte v)        { EEPROM.update(EEPROM_PEDAL_ASSIGN, v); }
void storeC1Assign(byte v)           { EEPROM.update(EEPROM_C1_ASSIGN, v); }
void storeC2Assign(byte v)           { EEPROM.update(EEPROM_C2_ASSIGN, v); }
void storePatchProgChange(byte v)    { EEPROM.update(EEPROM_PATCH_PROG_CHANGE,    v); }
void storeSysexExclusive(byte v)     { EEPROM.update(EEPROM_SYSEX_EXCLUSIVE,      v); }
void storeSysexApr(byte v)           { EEPROM.update(EEPROM_SYSEX_APR,            v); }
void storeUpperProgChange(byte v)    { EEPROM.update(EEPROM_UPPER_PROG_CHANGE,    v); }
void storeUpperAfterTouch(byte v)    { EEPROM.update(EEPROM_UPPER_AFTER_TOUCH,    v); }
void storeUpperBender(byte v)        { EEPROM.update(EEPROM_UPPER_BENDER,         v); }
void storeUpperModulation(byte v)    { EEPROM.update(EEPROM_UPPER_MODULATION,     v); }
void storeUpperPortamento(byte v)    { EEPROM.update(EEPROM_UPPER_PORTAMENTO,     v); }
void storeUpperHold(byte v)          { EEPROM.update(EEPROM_UPPER_HOLD,           v); }
void storeUpperVolume(byte v)        { EEPROM.update(EEPROM_UPPER_VOLUME,         v); }
void storeLowerProgChange(byte v)    { EEPROM.update(EEPROM_LOWER_PROG_CHANGE,    v); }
void storeLowerAfterTouch(byte v)    { EEPROM.update(EEPROM_LOWER_AFTER_TOUCH,    v); }
void storeLowerBender(byte v)        { EEPROM.update(EEPROM_LOWER_BENDER,         v); }
void storeLowerModulation(byte v)    { EEPROM.update(EEPROM_LOWER_MODULATION,     v); }
void storeLowerPortamento(byte v)    { EEPROM.update(EEPROM_LOWER_PORTAMENTO,     v); }
void storeLowerHold(byte v)          { EEPROM.update(EEPROM_LOWER_HOLD,           v); }
void storeLowerVolume(byte v)        { EEPROM.update(EEPROM_LOWER_VOLUME,         v); }

int getMasterTune() {
  byte mt = EEPROM.read(EEPROM_MASTER_TUNE);
  if (mt > 0x7F) mt = 0x2C;   // if EEPROM uninitialised, default to A440
  return mt;
}

void storeMasterTune(byte tuning) {
  EEPROM.update(EEPROM_MASTER_TUNE, tuning);
}

int getMIDIChannel() {
  byte midiChannel = EEPROM.read(EEPROM_MIDI_CH);
  if (midiChannel < 0 || midiChannel > 16) midiChannel = MIDI_CHANNEL_OMNI;//If EEPROM has no MIDI channel stored
  return midiChannel;
}

void storeMidiChannel(byte channel)
{
  EEPROM.update(EEPROM_MIDI_CH, channel);
}

boolean getEncoderDir() {
  byte ed = EEPROM.read(EEPROM_ENCODER_DIR); 
  if (ed < 0 || ed > 1)return true; //If EEPROM has no encoder direction stored
  return ed == 1 ? true : false;
}

void storeEncoderDir(byte encoderDir)
{
  EEPROM.update(EEPROM_ENCODER_DIR, encoderDir);
}

boolean getUpdateParams() {
  byte params = EEPROM.read(EEPROM_UPDATE_PARAMS); 
  if (params < 0 || params > 1)return true; //If EEPROM has no encoder direction stored
  return params == 1 ? true : false;
}

void storeUpdateParams(byte updateParameters)
{
  EEPROM.update(EEPROM_UPDATE_PARAMS, updateParameters);
}

int getLastPatch() {
  int lastPatchNumber = EEPROM.read(EEPROM_LAST_PATCH);
  if (lastPatchNumber < 1 || lastPatchNumber > 999) lastPatchNumber = 1;
  return lastPatchNumber;
}

void storeLastPatch(int lastPatchNumber)
{
  EEPROM.update(EEPROM_LAST_PATCH, lastPatchNumber);
}

int getMIDIOutCh() {
  byte mc = EEPROM.read(EEPROM_MIDI_OUT_CH);
  if (mc < 0 || midiOutCh > 16) mc = 0;//If EEPROM has no MIDI channel stored
  return mc;
}

void storeMidiOutCh(byte midiOutCh){
  EEPROM.update(EEPROM_MIDI_OUT_CH, midiOutCh);
}



