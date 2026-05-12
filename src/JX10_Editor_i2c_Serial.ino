#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <MIDI.h>
#include <USBHost_t36.h>
#include "MidiCC.h"
#include "Constants.h"
#include "Parameters.h"
#include "PatchMgr.h"
#include "Button.h"
#include "HWControls.h"
#include "EepromMgr.h"
#include "Arp.h"
#include "ToneLoader.h"
#include "Sysexhandler.h"
#include "DisplayValues.h"
#include "ChasePlay.h"
#include "imxrt.h"
#include <map>


std::map<int, int> voiceAssignment;

#define PARAMETER 0     //The main page for displaying the current patch and control (parameter) changes
#define RECALL 1        //Patches list
#define SAVE 2          //Save patch page
#define REINITIALISE 3  // Reinitialise message
#define PATCH 4         // Show current patch bypassing PARAMETER
#define PATCHNAMING 5   // Patch naming page
#define BANK_SELECT 6
#define BANK_SELECT_SAVE 7
#define SETTINGS 8       //Settings page
#define SETTINGSVALUE 9  //Settings page
#define ARP_EDIT 10
#define ARP_EDITVALUE 11
#define PATCH_EDIT 20
#define PATCH_EDITVALUE 21
#define TONE_EDIT 22
#define TONE_EDITVALUE 23
#define MIDI_EDIT 24
#define MIDI_EDITVALUE 25
#define CHASE_EDIT 26
#define PEDAL_EDIT 27
#define C1_EDIT 28
#define C2_EDIT 29

unsigned int state = PARAMETER;

#include "ST7735Display.h"

bool cardStatus = false;

//USB HOST MIDI Class Compliant
USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
MIDIDevice midi1(myusb);

struct MySettings : public midi::DefaultSettings {
  static const unsigned SysExMaxSize = 16384;
};

MIDI_CREATE_CUSTOM_INSTANCE(HardwareSerial, Serial1, MIDI, MySettings);

#include "Settings.h"
#include "PatchService.h"
#include "ToneService.h"
#include "MIDIService.h"
#include "PatchMenu.h"
#include "ToneMenu.h"
#include "MIDIMenu.h"

/* ============================================================
   SETUP / RUNTIME
   ============================================================ */

void pollAllMCPs();

void initButtons();

void updateUpperToneData();
void updateLowerToneData();

// ISRs must be short - just set flags
void VOICE1_RESET_ISR() {
  voice1ResetTriggered = true;
}

void VOICE2_RESET_ISR() {
  voice2ResetTriggered = true;
}

void setup() {
  Serial.begin(115200);
  Serial3.begin(31250, SERIAL_8N1);

  suppressParamAnnounce = true;
  bootInitInProgress = true;

  setupDisplay();
  setUpSettings();
  setUpPatch();
  setUpTone();
  setUpMIDI();
  setupHardware();
  primeMuxBaseline();

  // Attach interrupts - trigger on FALLING edge (HIGH -> LOW)
  attachInterrupt(digitalPinToInterrupt(VOICE1_RESET), VOICE1_RESET_ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(VOICE2_RESET), VOICE2_RESET_ISR, FALLING);

  delay(100);

  // ---------------------------------------------------------------------------
  // SD startup - replaces old cardStatus block in setup()
  // ---------------------------------------------------------------------------
  initSDCard();

  delay(100);

  // Set PWM frequency to 8 MHz
  analogWriteFrequency(VOICE_CLOCK, 8000000);
  analogWrite(VOICE_CLOCK, 128);  // 50% of default 8-bit range

  Wire.begin();
  Wire.setClock(400000);  // Slow down I2C to 100kHz
  Wire1.begin();
  Wire1.setClock(400000);  // Slow down I2C to 100kHz
  Wire2.begin();
  Wire2.setClock(400000);  // Slow down I2C to 100kHz
  delay(10);

  mcp1.begin(0, Wire1);
  delay(10);
  mcp2.begin(1, Wire1);
  delay(10);
  mcp3.begin(2, Wire);
  delay(10);
  mcp4.begin(3, Wire);
  delay(10);
  mcp5.begin(4, Wire);
  delay(10);
  mcp6.begin(5, Wire);
  delay(10);
  mcp7.begin(6, Wire1);
  delay(10);
  mcp8.begin(4, Wire1);
  delay(10);
  mcp9.begin(5, Wire1);
  delay(10);
  mcp10.begin(3, Wire1);
  delay(10);
  mcp11.begin(2, Wire1);
  delay(10);
  mcp12.begin(7, Wire);
  delay(10);

  //groupEncoders();
  //initRotaryEncoders();
  initButtons();

  setupMCPoutputs();
  sendInitSequence();

  //Read MIDI In Channel from EEPROM
  midiChannel = getMIDIChannel();
  masterTune = getMasterTune();
  sendInitialTuneCommands(masterTune);

  //USB HOST MIDI Class Compliant
  delay(400);  //Wait to turn on USB Host
  myusb.begin();
  midi1.setHandleControlChange(myControlConvert);
  midi1.setHandleNoteOff(myNoteOff);
  midi1.setHandleNoteOn(myNoteOn);
  midi1.setHandlePitchChange(myPitchBend);
  midi1.setHandleAfterTouchChannel(myAfterTouch);
  Serial.println("USB HOST MIDI Class Compliant Listening");

  // USB MIDI
  usbMIDI.setHandleNoteOn(myNoteOn);
  usbMIDI.setHandleNoteOff(myNoteOff);
  usbMIDI.setHandlePitchChange(myPitchBend);
  usbMIDI.setHandleControlChange(myControlConvert);
  usbMIDI.setHandleProgramChange(myProgramChange);
  usbMIDI.setHandleAfterTouchChannel(myAfterTouch);
  usbMIDI.setHandleClock(arpOnMidiClockTick);
  usbMIDI.setHandleStart(arpOnMidiStart);
  usbMIDI.setHandleStop(arpOnMidiStop);
  usbMIDI.setHandleContinue(arpOnMidiContinue);
  usbMIDI.setHandleSystemExclusive(handleSysexByte);


  MIDI.begin();
  MIDI.setHandleNoteOn(myNoteOn);
  MIDI.setHandleNoteOff(myNoteOff);
  MIDI.setHandlePitchBend(myPitchBend);
  MIDI.setHandleControlChange(myControlConvert);
  MIDI.setHandleProgramChange(myProgramChange);
  MIDI.setHandleAfterTouchChannel(myAfterTouch);
  MIDI.setHandleClock(arpOnMidiClockTick);
  MIDI.setHandleStart(arpOnMidiStart);
  MIDI.setHandleStop(arpOnMidiStop);
  MIDI.setHandleContinue(arpOnMidiContinue);
  MIDI.setHandleSystemExclusive(handleSysexByte);
  MIDI.turnThruOn(midi::Thru::Mode::Off);
  Serial.println("MIDI In DIN Listening");

  loadMidiSettings();
  loadControlsSettings();

  //Read Encoder Direction from EEPROM
  encCW = getEncoderDir();

  //Read MIDI Out Channel from EEPROM
  midiOutCh = getMIDIOutCh();

  arpInit();
  chaseInit();
  sendPatchInit();
  loadInitialPatch();
  bootInitInProgress = false;
  suppressParamAnnounce = false;
  startParameterDisplay();
}

void initSDCard() {
  cardStatus = SD.begin(BUILTIN_SDCARD);
  if (cardStatus) {
    Serial.println(F("SD card is connected"));
    ensureAllBanksInitialized();  // create folders/files if missing
  } else {
    Serial.println(F("SD card is not connected or unusable"));
    showPatchPage("No SD", "conn'd / usable");
  }
}

void sendInitSequence() {
  mcp9.digitalWrite(PATCH_LED_RED, HIGH);
  mcp9.digitalWrite(TONE_LED_RED, HIGH);

  delay(1);
  // 8x F1
  for (int i = 0; i < 16; i++) {
    Serial3.write((uint8_t)0xF1);
    delay(100);
  }

  // 8x F9
  for (int i = 0; i < 16; i++) {
    Serial3.write((uint8_t)0xF9);
    delay(100);
  }

  Serial3.flush();  // wait until bytes have left the TX buffer

  // Invalidate running status since we wrote a prefix directly
  board = 0x00;
  EXTRA_OFFSET = 0xFF;
}

// ---------------------------------------------------------------------------
// SD Initialisation - JX-10 style
// Creates /bank0 .. /bank7 folders and populates files 11-88 if missing
// ---------------------------------------------------------------------------

// Ensure a single bank folder exists
void ensureJX10BankFolder(uint8_t bank) {
  String folderPath = "/bank" + String(bank);
  if (!SD.exists(folderPath.c_str())) {
    SD.mkdir(folderPath.c_str());
  }
}

// Ensure all 8 bank folders exist
void ensureJX10BankFolders() {
  for (uint8_t bank = 0; bank < NUM_BANKS; bank++) {
    ensureJX10BankFolder(bank);
  }
}

// Populate a single bank with default patches for any missing slots
void ensureJX10BankInitialized(uint8_t bank) {
  ensureJX10BankFolder(bank);
  for (uint8_t group = 1; group <= NUM_GROUPS; group++) {  // 1-8 (A-H)
    for (uint8_t slot = 1; slot <= NUM_SLOTS; slot++) {    // 1-8
      uint8_t fileNum = group * 10 + slot;                 // e.g. 11, 12 .. 88
      String path = "/bank" + String(bank) + "/" + String(fileNum);
      if (!SD.exists(path.c_str())) {
        // Build a default patch name e.g. "A1 Init" for group A slot 1
        String defaultName = String((char)('A' + group - 1)) + String(slot) + " Init";
        // INITPATCH is your existing default patch data string constant
        // Replace the first field (patch name) with our default name
        String initData = defaultName + "," + INITPATCH;
        savePatch(path.c_str(), initData);
      }
    }
  }
}

// Populate all banks - only fills missing files, safe to call on every boot
void ensureAllBanksInitialized() {
  for (uint8_t bank = 0; bank < NUM_BANKS; bank++) {
    ensureJX10BankInitialized(bank);
    ensureToneFiles(bank);
  }
}

void loadMidiSettings() {
  upperLocal = getUpperLocal();
  lowerLocal = getLowerLocal();
  upperChannel = getUpperChannel();
  lowerChannel = getLowerChannel();
  controlChannel = getControlChannel();
  patchProgChange = getPatchProgChange();
  sysexExclusive = getSysexExclusive();
  sysexApr = getSysexApr();
  realTime = getRealTime();
  upperProgChange = getUpperProgChange();
  upperAfterTouch = getUpperAfterTouch();
  upperBender = getUpperBender();
  upperModulation = getUpperModulation();
  upperPortamento = getUpperPortamento();
  upperHold = getUpperHold();
  upperVolume = getUpperVolume();
  lowerProgChange = getLowerProgChange();
  lowerAfterTouch = getLowerAfterTouch();
  lowerBender = getLowerBender();
  lowerModulation = getLowerModulation();
  lowerPortamento = getLowerPortamento();
  lowerHold = getLowerHold();
  lowerVolume = getLowerVolume();
  c1c2ToneEdit = getC1C2ToneEdit();
}

void loadControlsSettings() {
  pedalAssign = getPedalAssign();
  c1Assign = getC1Assign();
  c2Assign = getC2Assign();

  // Defensive: EEPROM is 0xFF when uninitialised
  if (pedalAssign >= PEDAL_ASSIGN_COUNT) pedalAssign = 0;
  if (c1Assign >= CONTROLLER_ASSIGN_COUNT) c1Assign = 0;
  if (c2Assign >= CONTROLLER_ASSIGN_COUNT) c2Assign = 0;

  // Mirror into working copies in case anything reads them before menu entry
  pedalAssignWorking = pedalAssign;
  c1AssignWorking = c1Assign;
  c2AssignWorking = c2Assign;
}

inline uint8_t clampTune(int16_t val) {
  if (val < TUNE_MIN) return TUNE_MIN;
  if (val > TUNE_MAX) return TUNE_MAX;
  return (uint8_t)val;
}

void sendInitialTuneCommands(uint8_t masterTune) {
  lowerMT1 = masterTune;
  lowerMT2 = masterTune;
  upperMT1 = masterTune;
  upperMT2 = masterTune;
  send5(kBoardLowerPrefix, 0xB4, lowerMT1, 0xBE, lowerMT2);
  send5(kBoardUpperPrefix, 0xB4, upperMT1, 0xBE, upperMT2);
}

uint8_t calcLowerDetune(uint8_t masterTune, uint8_t dualdetune) {
  uint8_t lowerFloor = (masterTune >= TUNE_442HZ) ? (masterTune - TUNE_442HZ) : 0x00;
  uint8_t upperCeiling = (uint8_t)min((int16_t)masterTune + 0x3F, (int16_t)0x7F);
  int16_t offset = (int16_t)dualdetune - (int16_t)DETUNE_CENTER;
  return (uint8_t)constrain((int16_t)masterTune + offset,
                            (int16_t)lowerFloor,
                            (int16_t)upperCeiling);
}

void sendTuneCommands(uint8_t masterTune) {
  lowerMT1 = masterTune;
  lowerMT2 = masterTune;
  upperMT1 = masterTune;
  upperMT2 = masterTune;

  if (keyMode == 0 || keyMode == 4 || keyMode == 4) {
    uint8_t lowerSend = calcLowerDetune(masterTune, dualdetune);
    send5(kBoardLowerPrefix, 0xB4, lowerSend, 0xBE, lowerSend);
  } else {
    send5(kBoardLowerPrefix, 0xB4, lowerMT1, 0xBE, lowerMT2);
  }
  send5(kBoardUpperPrefix, 0xB4, upperMT1, 0xBE, upperMT2);
}

void primeMuxBaseline() {
  for (int ch = 0; ch < MUXCHANNELS; ch++) {
    digitalWriteFast(MUX_0, ch & B0001);
    digitalWriteFast(MUX_1, ch & B0010);
    digitalWriteFast(MUX_2, ch & B0100);
    digitalWriteFast(MUX_3, ch & B1000);
    delayMicroseconds(2);

    mux1ValuesPrev[ch] = adc->adc1->analogRead(MUX1_S);
    mux2ValuesPrev[ch] = adc->adc1->analogRead(MUX2_S);
    mux3ValuesPrev[ch] = adc->adc1->analogRead(MUX3_S);
    mux4ValuesPrev[ch] = adc->adc1->analogRead(MUX4_S);
  }
  muxInput = 0;
}

void startParameterDisplay() {
  updateScreen();

  lastDisplayTriggerTime = millis();
  waitingToUpdate = true;
}

void myProgramChange(byte channel, byte program) {
  if (program > 63) return;  // only 64 patches in a bank

  // Convert 0-63 to group (0-7) and slot (1-8)
  int group = program / 8;       // 0-7  (A-H)
  int slot = (program % 8) + 1;  // 1-8

  state = PATCH;
  recallPatch(currentBank, group, slot);
  // Serial.print(F("MIDI Pgm Change: "));
  // Serial.print(program);
  // Serial.print(F(" -> "));
  // Serial.print((char)('A' + group));
  // Serial.println(slot);
  state = PARAMETER;
}

// Aftertouch

void myAfterTouch(byte channel, byte value) {
  sendVoiceParam(kBoardLowerPrefix, NO_OFFSET, 0xB3, value);
  sendVoiceParam(kBoardUpperPrefix, NO_OFFSET, 0xB3, value);
}

// PITCH BEND

static inline uint16_t clamp_u16(int v, uint16_t lo, uint16_t hi) {
  if (v < (int)lo) return lo;
  if (v > (int)hi) return hi;
  return (uint16_t)v;
}

static inline uint8_t map_u8_round(uint16_t x, uint16_t in_min, uint16_t in_max, uint8_t out_min, uint8_t out_max) {
  if (in_max == in_min) return out_min;
  const uint32_t num = (uint32_t)(x - in_min) * (uint32_t)(out_max - out_min);
  const uint32_t den = (uint32_t)(in_max - in_min);
  const uint32_t y = (uint32_t)out_min + (num + den / 2) / den;
  return clamp_u8(y, out_min, out_max);
}

void sendVoiceParam(uint8_t prefix, uint8_t offset, uint8_t param, uint8_t value) {

  // In whole mode (keyMode 1 or 2) both boards respond to F4 broadcast
  if (keyMode == 1 || keyMode == 2) {
    prefix = kBoardBothPrefix;
  }

  // --- Board prefix (running status) ---
  if (prefix != board) {
    Serial3.write(prefix);
    //Serial.print(prefix, HEX);
    //Serial.print(" ");
    board = prefix;
  }

  // --- Offset (running status) ---
  if (offset != NO_OFFSET) {
    if (offset != EXTRA_OFFSET) {
      Serial3.write(0xFD);
      //Serial.print("FD");
      //Serial.print(" ");
      Serial3.write(offset);
      if (offset < 0x10) Serial.print("0");
      //Serial.print(offset, HEX);
      //Serial.print(" ");
      EXTRA_OFFSET = offset;
    }
  }

  // --- Parameter + value always sent ---
  Serial3.write(param);
  //if (param < 0x10) Serial.print("0");
  //Serial.print(param, HEX);
  //Serial.print(" ");
  Serial3.write(value);
  //if (value < 0x10) Serial.print("0");
  //Serial.print(value, HEX);
  //Serial.println(" ");
}

void sendBoardVoiceParam(uint8_t prefix, uint8_t offset, uint8_t param, uint8_t value) {

  // --- Board prefix (running status) ---
  if (prefix != board) {
    Serial3.write(prefix);
    //Serial.print(prefix, HEX);
    //Serial.print(" ");
    board = prefix;
  }

  // --- Offset (running status) ---
  if (offset != NO_OFFSET) {
    if (offset != EXTRA_OFFSET) {
      Serial3.write(0xFD);
      //Serial.print("FD");
      //Serial.print(" ");
      Serial3.write(offset);
      //if (offset < 0x10) Serial.print("0");
      //Serial.print(offset, HEX);
      //Serial.print(" ");
      EXTRA_OFFSET = offset;
    }
  }

  // --- Parameter + value always sent ---
  Serial3.write(param);
  //if (param < 0x10) Serial.print("0");
  //Serial.print(param, HEX);
  //Serial.print(" ");
  Serial3.write(value);
  //if (value < 0x10) Serial.print("0");
  //Serial.print(value, HEX);
  //Serial.println(" ");
}

static inline void send5(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e) {
  Serial3.write(a);
  Serial3.write(b);
  Serial3.write((uint8_t)(c & 0x7F));
  Serial3.write(d);
  Serial3.write((uint8_t)(e & 0x7F));
  Serial3.flush();
  // Invalidate running status since we wrote a prefix directly
  board = 0x00;
  EXTRA_OFFSET = 0xFF;
}

// Convert signed pitch bend (-8192 to +8191) to unsigned 14-bit (0 to 16383)
static inline uint16_t toPb14(int pitchValue) {
  // Clamp to valid range
  if (pitchValue < -8192) pitchValue = -8192;
  if (pitchValue > 8191) pitchValue = 8191;
  // Convert to 0-16383 range (center = 8192)
  return (uint16_t)(pitchValue + 8192);
}

void myPitchBend(byte channel, int pitchValue) {
  MIDI.sendPitchBend(pitchValue, controlChannel);

  static uint8_t lastSign = 0xFF;
  static uint8_t lastMag = 0xFF;

  // Convert signed MIDI value to 0-16383 range
  uint16_t pb14 = toPb14(pitchValue);

  // DISABLE THIS if you want normal behavior
  // if (kInvertPb14) pb14 = (uint16_t)(16383 - pb14);

  // Determine sign: center (8192) and above = positive
  const bool neg = (pb14 < 8192);
  const uint8_t signVal = neg ? 0x00 : 0x7F;

  // Calculate magnitude (distance from center)
  uint16_t magIn;
  if (neg) {
    magIn = (uint16_t)(8192 - pb14);  // 0 to 8192
  } else {
    magIn = (uint16_t)(pb14 - 8192);  // 0 to 8191
  }

  // Map to 0x00-0x7F
  uint8_t mag;
  if (magIn == 0) {
    mag = 0x00;
  } else {
    uint16_t magMax = neg ? 8192u : 8191u;
    mag = map_u8_round(magIn, 0, magMax, 0x00, 0x7F);
  }

  switch (bend_enable) {

    case 1:

      // Send sign change message when crossing zero
      if (signVal != lastSign) {
        sendVoiceParam(kPrefixBroadcast, NO_OFFSET, kPitchSignParam, signVal);
        lastSign = signVal;
      }

      // Send magnitude updates
      if (mag != lastMag) {
        sendVoiceParam(kBoardLowerPrefix, NO_OFFSET, kPitchValueParam, mag);
        lastMag = mag;
      }

      break;

    case 2:
      // Send sign change message when crossing zero
      if (signVal != lastSign) {
        sendVoiceParam(kPrefixBroadcast, NO_OFFSET, kPitchSignParam, signVal);
        lastSign = signVal;
      }

      // Send magnitude updates
      if (mag != lastMag) {
        sendVoiceParam(kBoardUpperPrefix, NO_OFFSET, kPitchValueParam, mag);
        lastMag = mag;
      }
      break;

    case 3:
      // Send sign change message when crossing zero
      if (signVal != lastSign) {
        sendVoiceParam(kPrefixBroadcast, NO_OFFSET, kPitchSignParam, signVal);
        lastSign = signVal;
      }

      // Send magnitude updates
      if (mag != lastMag) {
        sendVoiceParam(kBoardUpperPrefix, NO_OFFSET, kPitchValueParam, mag);
        sendVoiceParam(kBoardLowerPrefix, NO_OFFSET, kPitchValueParam, mag);
        lastMag = mag;
      }
      break;

    case 0:
      break;
  }
}

void sendModToBoards(uint8_t wheelValue) {
  lastModWheelValue = wheelValue;

  uint8_t lowerMod = (uint8_t)((wheelValue * lowerLFOModDepth) / 127);
  uint8_t upperMod = (uint8_t)((wheelValue * upperLFOModDepth) / 127);

  sendVoiceParam(kBoardUpperPrefix, NO_OFFSET, 0xBC, upperMod);
  sendVoiceParam(kBoardLowerPrefix, NO_OFFSET, 0xBC, lowerMod);
  ;
}

void myControlConvert(byte channel, byte control, byte value) {
  switch (control) {

    case 1:  // mod wheel
      sendModToBoards(value);
      break;

    case 2:
      MIDI.sendControlChange(control, value, controlChannel);
      break;

    case 7:
      MIDI.sendControlChange(control, value, controlChannel);
      break;

    case 64:
      MIDI.sendControlChange(control, value, controlChannel);
      break;

    default:
      int newvalue = value;
      myControlChange(channel, control, newvalue);
      break;
  }
}

void myControlChange(byte channel, byte control, int value) {
  switch (control) {

    case CCmod_lfo:
      if (upperSW) {
        upperLFOModDepth = value;
        if (keyMode == 1) {
          lowerLFOModDepth = upperLFOModDepth;
        }
      } else {
        lowerLFOModDepth = value;
        if (keyMode == 2) {
          upperLFOModDepth = lowerLFOModDepth;
        }
      }
      updatemod_lfo(1);
      break;

    case CCbend_range:
      bend_range = value;
      bend_range_str = value;
      updatebend_range(1);
      break;

    case CClfo1_wave:
      if (upperSW) {
        upperData[P_lfo1_wave] = map(value, 0, 127, 0, 4);
        if (keyMode == 1) {
          lowerData[P_lfo1_wave] = upperData[P_lfo1_wave];
        }
      } else {
        lowerData[P_lfo1_wave] = map(value, 0, 127, 0, 4);
        if (keyMode == 2) {
          upperData[P_lfo1_wave] = lowerData[P_lfo1_wave];
        }
      }
      updatelfo1_wave(1);
      break;

    case CClfo1_rate:
      if (upperSW) {
        upperData[P_lfo1_rate] = value;
        if (keyMode == 1) {
          lowerData[P_lfo1_rate] = upperData[P_lfo1_rate];
        }
      } else {
        lowerData[P_lfo1_rate] = value;
        if (keyMode == 2) {
          upperData[P_lfo1_rate] = lowerData[P_lfo1_rate];
        }
      }
      updatelfo1_rate(1);
      break;

    case CClfo1_delay:
      if (upperSW) {
        upperData[P_lfo1_delay] = value;
        if (keyMode == 1) {
          lowerData[P_lfo1_delay] = upperData[P_lfo1_delay];
        }
      } else {
        lowerData[P_lfo1_delay] = value;
        if (keyMode == 2) {
          upperData[P_lfo1_delay] = lowerData[P_lfo1_delay];
        }
      }
      updatelfo1_delay(1);
      break;

    case CClfo1_lfo2:
      if (upperSW) {
        upperData[P_lfo1_lfo2] = value;
        if (keyMode == 1) {
          lowerData[P_lfo1_lfo2] = upperData[P_lfo1_lfo2];
        }
      } else {
        lowerData[P_lfo1_lfo2] = value;
        if (keyMode == 2) {
          upperData[P_lfo1_lfo2] = lowerData[P_lfo1_lfo2];
        }
      }
      updatelfo1_lfo2(1);
      break;

    case CCdco1_PW:
      if (upperSW) {
        upperData[P_dco1_PW] = value;
        if (keyMode == 1) {
          lowerData[P_dco1_PW] = upperData[P_dco1_PW];
        }
      } else {
        lowerData[P_dco1_PW] = value;
        if (keyMode == 2) {
          upperData[P_dco1_PW] = lowerData[P_dco1_PW];
        }
      }
      updatedco1_PW(1);
      break;

    case CCdco1_PWM_env:
      if (upperSW) {
        upperData[P_dco1_PWM_env] = value;
        if (keyMode == 1) {
          lowerData[P_dco1_PWM_env] = upperData[P_dco1_PWM_env];
        }
      } else {
        lowerData[P_dco1_PWM_env] = value;
        if (keyMode == 2) {
          upperData[P_dco1_PWM_env] = lowerData[P_dco1_PWM_env];
        }
      }
      updatedco1_PWM_env(1);
      break;

    case CCdco1_PWM_lfo:
      if (upperSW) {
        upperData[P_dco1_PWM_lfo] = value;
        if (keyMode == 1) {
          lowerData[P_dco1_PWM_lfo] = upperData[P_dco1_PWM_lfo];
        }
      } else {
        lowerData[P_dco1_PWM_lfo] = value;
        if (keyMode == 2) {
          upperData[P_dco1_PWM_lfo] = lowerData[P_dco1_PWM_lfo];
        }
      }
      updatedco1_PWM_lfo(1);
      break;

    case CCdco1_pitch_env:
      if (upperSW) {
        upperData[P_dco1_pitch_env] = value;
        if (keyMode == 1) {
          lowerData[P_dco1_pitch_env] = upperData[P_dco1_pitch_env];
        }
      } else {
        lowerData[P_dco1_pitch_env] = value;
        if (keyMode == 2) {
          upperData[P_dco1_pitch_env] = lowerData[P_dco1_pitch_env];
        }
      }
      updatedco1_pitch_env(1);
      break;

    case CCdco1_pitch_lfo:
      if (upperSW) {
        upperData[P_dco1_pitch_lfo] = value;
        if (keyMode == 1) {
          lowerData[P_dco1_pitch_lfo] = upperData[P_dco1_pitch_lfo];
        }
      } else {
        lowerData[P_dco1_pitch_lfo] = value;
        if (keyMode == 2) {
          upperData[P_dco1_pitch_lfo] = lowerData[P_dco1_pitch_lfo];
        }
      }
      updatedco1_pitch_lfo(1);
      break;

    case CCdco1_wave:
      if (upperSW) {
        upperData[P_dco1_wave] = map(value, 0, 127, 0, 3);
        if (keyMode == 1) lowerData[P_dco1_wave] = upperData[P_dco1_wave];
      } else {
        lowerData[P_dco1_wave] = map(value, 0, 127, 0, 3);
        if (keyMode == 2) upperData[P_dco1_wave] = lowerData[P_dco1_wave];
      }
      updatedco1_wave(1);
      break;

    case CCdco1_range:
      if (upperSW) {
        upperData[P_dco1_range] = map(value, 0, 127, 0, 3);
        if (keyMode == 1) lowerData[P_dco1_range] = upperData[P_dco1_range];
      } else {
        lowerData[P_dco1_range] = map(value, 0, 127, 0, 3);
        if (keyMode == 2) upperData[P_dco1_range] = lowerData[P_dco1_range];
      }
      updatedco1_range(1);
      break;

    case CCdco1_tune:
      if (upperSW) {
        upperData[P_dco1_tune] = value;
        if (keyMode == 1) {
          lowerData[P_dco1_tune] = upperData[P_dco1_tune];
        }
      } else {
        lowerData[P_dco1_tune] = value;
        if (keyMode == 2) {
          upperData[P_dco1_tune] = lowerData[P_dco1_tune];
        }
      }
      updatedco1_tune(1);
      break;

    case CCdco1_xmod:
      if (upperSW) {
        upperData[P_dco1_mode] = map(value, 0, 127, 0, 3);
        if (keyMode == 1) lowerData[P_dco1_mode] = upperData[P_dco1_mode];
      } else {
        lowerData[P_dco1_mode] = map(value, 0, 127, 0, 3);
        if (keyMode == 2) upperData[P_dco1_mode] = lowerData[P_dco1_mode];
      }
      updatedco1_xmod(1);
      break;

    case CClfo2_wave:
      if (upperSW) {
        upperData[P_lfo2_wave] = map(value, 0, 127, 0, 4);
        if (keyMode == 1) {
          lowerData[P_lfo2_wave] = upperData[P_lfo2_wave];
        }
      } else {
        lowerData[P_lfo2_wave] = map(value, 0, 127, 0, 4);
        if (keyMode == 2) {
          upperData[P_lfo2_wave] = lowerData[P_lfo2_wave];
        }
      }
      updatelfo2_wave(1);
      break;

    case CClfo2_rate:
      if (upperSW) {
        upperData[P_lfo2_rate] = value;
        if (keyMode == 1) {
          lowerData[P_lfo2_rate] = upperData[P_lfo2_rate];
        }
      } else {
        lowerData[P_lfo2_rate] = value;
        if (keyMode == 2) {
          upperData[P_lfo2_rate] = lowerData[P_lfo2_rate];
        }
      }
      updatelfo2_rate(1);
      break;

    case CClfo2_delay:
      if (upperSW) {
        upperData[P_lfo2_delay] = value;
        if (keyMode == 1) {
          lowerData[P_lfo2_delay] = upperData[P_lfo2_delay];
        }
      } else {
        lowerData[P_lfo2_delay] = value;
        if (keyMode == 2) {
          upperData[P_lfo2_delay] = lowerData[P_lfo2_delay];
        }
      }
      updatelfo2_delay(1);
      break;

    case CClfo2_lfo1:
      if (upperSW) {
        upperData[P_lfo2_lfo1] = value;
        if (keyMode == 1) {
          lowerData[P_lfo2_lfo1] = upperData[P_lfo2_lfo1];
        }
      } else {
        lowerData[P_lfo2_lfo1] = value;
        if (keyMode == 2) {
          upperData[P_lfo2_lfo1] = lowerData[P_lfo2_lfo1];
        }
      }
      updatelfo2_lfo1(1);
      break;

    case CCdco2_PW:
      if (upperSW) {
        upperData[P_dco2_PW] = value;
        if (keyMode == 1) {
          lowerData[P_dco2_PW] = upperData[P_dco2_PW];
        }
      } else {
        lowerData[P_dco2_PW] = value;
        if (keyMode == 2) {
          upperData[P_dco2_PW] = lowerData[P_dco2_PW];
        }
      }
      updatedco2_PW(1);
      break;

    case CCdco2_PWM_env:
      if (upperSW) {
        upperData[P_dco2_PWM_env] = value;
        if (keyMode == 1) {
          lowerData[P_dco2_PWM_env] = upperData[P_dco2_PWM_env];
        }
      } else {
        lowerData[P_dco2_PWM_env] = value;
        if (keyMode == 2) {
          upperData[P_dco2_PWM_env] = lowerData[P_dco2_PWM_env];
        }
      }
      updatedco2_PWM_env(1);
      break;

    case CCdco2_PWM_lfo:
      if (upperSW) {
        upperData[P_dco2_PWM_lfo] = value;
        if (keyMode == 1) {
          lowerData[P_dco2_PWM_lfo] = upperData[P_dco2_PWM_lfo];
        }
      } else {
        lowerData[P_dco2_PWM_lfo] = value;
        if (keyMode == 2) {
          upperData[P_dco2_PWM_lfo] = lowerData[P_dco2_PWM_lfo];
        }
      }
      updatedco2_PWM_lfo(1);
      break;

    case CCdco2_pitch_env:
      if (upperSW) {
        upperData[P_dco2_pitch_env] = value;
        if (keyMode == 1) {
          lowerData[P_dco2_pitch_env] = upperData[P_dco2_pitch_env];
        }
      } else {
        lowerData[P_dco2_pitch_env] = value;
        if (keyMode == 2) {
          upperData[P_dco2_pitch_env] = lowerData[P_dco2_pitch_env];
        }
      }
      updatedco2_pitch_env(1);
      break;

    case CCdco2_pitch_lfo:
      if (upperSW) {
        upperData[P_dco2_pitch_lfo] = value;
        if (keyMode == 1) {
          lowerData[P_dco2_pitch_lfo] = upperData[P_dco2_pitch_lfo];
        }
      } else {
        lowerData[P_dco2_pitch_lfo] = value;
        if (keyMode == 2) {
          upperData[P_dco2_pitch_lfo] = lowerData[P_dco2_pitch_lfo];
        }
      }
      updatedco2_pitch_lfo(1);
      break;

    case CCdco2_wave:
      if (upperSW) {
        upperData[P_dco2_wave] = map(value, 0, 127, 0, 3);
        if (keyMode == 1) lowerData[P_dco2_wave] = upperData[P_dco2_wave];
      } else {
        lowerData[P_dco2_wave] = map(value, 0, 127, 0, 3);
        if (keyMode == 2) upperData[P_dco2_wave] = lowerData[P_dco2_wave];
      }
      updatedco2_wave(1);
      break;

    case CCdco2_range:
      if (upperSW) {
        upperData[P_dco2_range] = map(value, 0, 127, 0, 3);
        if (keyMode == 1) lowerData[P_dco2_range] = upperData[P_dco2_range];
      } else {
        lowerData[P_dco2_range] = map(value, 0, 127, 0, 3);
        if (keyMode == 2) upperData[P_dco2_range] = lowerData[P_dco2_range];
      }
      updatedco2_range(1);
      break;

    case CCdco2_tune:
      if (upperSW) {
        upperData[P_dco2_tune] = value;
        if (keyMode == 1) {
          lowerData[P_dco2_tune] = upperData[P_dco2_tune];
        }
      } else {
        lowerData[P_dco2_tune] = value;
        if (keyMode == 2) {
          upperData[P_dco2_tune] = lowerData[P_dco2_tune];
        }
      }
      updatedco2_tune(1);
      break;

    case CCdco2_fine:
      if (upperSW) {
        upperData[P_dco2_fine] = value;
        if (keyMode == 1) {
          lowerData[P_dco2_fine] = upperData[P_dco2_fine];
        }
      } else {
        lowerData[P_dco2_fine] = value;
        if (keyMode == 2) {
          upperData[P_dco2_fine] = lowerData[P_dco2_fine];
        }
      }
      updatedco2_fine(1);
      break;

    case CCdco1_level:
      if (upperSW) {
        upperData[P_dco1_level] = value;
        if (keyMode == 1) {
          lowerData[P_dco1_level] = upperData[P_dco1_level];
        }
      } else {
        lowerData[P_dco1_level] = value;
        if (keyMode == 2) {
          upperData[P_dco1_level] = lowerData[P_dco1_level];
        }
      }
      updatedco1_level(1);
      break;

    case CCdco2_level:
      if (upperSW) {
        upperData[P_dco2_level] = value;
        if (keyMode == 1) {
          lowerData[P_dco2_level] = upperData[P_dco2_level];
        }
      } else {
        lowerData[P_dco2_level] = value;
        if (keyMode == 2) {
          upperData[P_dco2_level] = lowerData[P_dco2_level];
        }
      }
      updatedco2_level(1);
      break;

    case CCdco2_mod:
      if (upperSW) {
        upperData[P_dco2_mod] = value;
        if (keyMode == 1) {
          lowerData[P_dco2_mod] = upperData[P_dco2_mod];
        }
      } else {
        lowerData[P_dco2_mod] = value;
        if (keyMode == 2) {
          upperData[P_dco2_mod] = lowerData[P_dco2_mod];
        }
      }
      updatedco2_mod(1);
      break;

    case CCvcf_hpf:
      if (upperSW) {
        upperData[P_vcf_hpf] = map(value, 0, 127, 0, 3);
        if (keyMode == 1) {
          lowerData[P_vcf_hpf] = upperData[P_vcf_hpf];
        }
      } else {
        lowerData[P_vcf_hpf] = map(value, 0, 127, 0, 3);
        if (keyMode == 2) {
          upperData[P_vcf_hpf] = lowerData[P_vcf_hpf];
        }
      }
      updatevcf_hpf(1);
      break;

    case CCvcf_cutoff:
      if (upperSW) {
        upperData[P_vcf_cutoff] = value;
        if (keyMode == 1) {
          lowerData[P_vcf_cutoff] = upperData[P_vcf_cutoff];
        }
      } else {
        lowerData[P_vcf_cutoff] = value;
        if (keyMode == 2) {
          upperData[P_vcf_cutoff] = lowerData[P_vcf_cutoff];
        }
      }
      updatevcf_cutoff(1);
      break;

    case CCvcf_res:
      if (upperSW) {
        upperData[P_vcf_res] = value;
        if (keyMode == 1) {
          lowerData[P_vcf_res] = upperData[P_vcf_res];
        }
      } else {
        lowerData[P_vcf_res] = value;
        if (keyMode == 2) {
          upperData[P_vcf_res] = lowerData[P_vcf_res];
        }
      }
      updatevcf_res(1);
      break;

    case CCvcf_kb:
      if (upperSW) {
        upperData[P_vcf_kb] = value;
        if (keyMode == 1) {
          lowerData[P_vcf_kb] = upperData[P_vcf_kb];
        }
      } else {
        lowerData[P_vcf_kb] = value;
        if (keyMode == 2) {
          upperData[P_vcf_kb] = lowerData[P_vcf_kb];
        }
      }
      updatevcf_kb(1);
      break;

    case CCvcf_env:
      if (upperSW) {
        upperData[P_vcf_env] = value;
        if (keyMode == 1) {
          lowerData[P_vcf_env] = upperData[P_vcf_env];
        }
      } else {
        lowerData[P_vcf_env] = value;
        if (keyMode == 2) {
          upperData[P_vcf_env] = lowerData[P_vcf_env];
        }
      }
      updatevcf_env(1);
      break;

    case CCvcf_lfo1:
      if (upperSW) {
        upperData[P_vcf_lfo1] = value;
        if (keyMode == 1) {
          lowerData[P_vcf_lfo1] = upperData[P_vcf_lfo1];
        }
      } else {
        lowerData[P_vcf_lfo1] = value;
        if (keyMode == 2) {
          upperData[P_vcf_lfo1] = lowerData[P_vcf_lfo1];
        }
      }
      updatevcf_lfo1(1);
      break;

    case CCvcf_lfo2:
      if (upperSW) {
        upperData[P_vcf_lfo2] = value;
        if (keyMode == 1) {
          lowerData[P_vcf_lfo2] = upperData[P_vcf_lfo2];
        }
      } else {
        lowerData[P_vcf_lfo2] = value;
        if (keyMode == 2) {
          upperData[P_vcf_lfo2] = lowerData[P_vcf_lfo2];
        }
      }
      updatevcf_lfo2(1);
      break;

    case CCvca_mod:
      if (upperSW) {
        upperData[P_vca_mod] = value;
        if (keyMode == 1) {
          lowerData[P_vca_mod] = upperData[P_vca_mod];
        }
      } else {
        lowerData[P_vca_mod] = value;
        if (keyMode == 2) {
          upperData[P_vca_mod] = lowerData[P_vca_mod];
        }
      }
      updatevca_mod(1);
      break;

    case CCat_vib:
      at_vib = value;
      at_vib_str = value;
      updateat_vib(1);
      break;

    case CCat_bri:
      at_bri = value;
      at_bri_str = value;
      updateat_bri(1);
      break;

    case CCat_vol:
      at_vol = value;
      at_vol_str = value;
      updateat_vol(1);
      break;

    case CCbalance:
      balance = value;
      balance_str = value;
      updatebalance(1);
      break;

    case CCvolume:
      volume = value;
      volume_str = value;
      updatevolume(1);
      break;

    case CCportamento:
      portamento = value;
      portamento_str = value;
      updateportamento(1);
      break;

    case CCtime1:
      if (upperSW) {
        upperData[P_time1] = value;
        if (keyMode == 1) {
          lowerData[P_time1] = upperData[P_time1];
        }
      } else {
        lowerData[P_time1] = value;
        if (keyMode == 2) {
          upperData[P_time1] = lowerData[P_time1];
        }
      }
      updatetime1(1);
      break;

    case CClevel1:
      if (upperSW) {
        upperData[P_level1] = value;
        if (keyMode == 1) {
          lowerData[P_level1] = upperData[P_level1];
        }
      } else {
        lowerData[P_level1] = value;
        if (keyMode == 2) {
          upperData[P_level1] = lowerData[P_level1];
        }
      }
      updatelevel1(1);
      break;

    case CCtime2:
      if (upperSW) {
        upperData[P_time2] = value;
        if (keyMode == 1) {
          lowerData[P_time2] = upperData[P_time2];
        }
      } else {
        lowerData[P_time2] = value;
        if (keyMode == 2) {
          upperData[P_time2] = lowerData[P_time2];
        }
      }
      updatetime2(1);
      break;

    case CClevel2:
      if (upperSW) {
        upperData[P_level2] = value;
        if (keyMode == 1) {
          lowerData[P_level2] = upperData[P_level2];
        }
      } else {
        lowerData[P_level2] = value;
        if (keyMode == 2) {
          upperData[P_level2] = lowerData[P_level2];
        }
      }
      updatelevel2(1);
      break;

    case CCtime3:
      if (upperSW) {
        upperData[P_time3] = value;
        if (keyMode == 1) {
          lowerData[P_time3] = upperData[P_time3];
        }
      } else {
        lowerData[P_time3] = value;
        if (keyMode == 2) {
          upperData[P_time3] = lowerData[P_time3];
        }
      }
      updatetime3(1);
      break;

    case CClevel3:
      if (upperSW) {
        upperData[P_level3] = value;
        if (keyMode == 1) {
          lowerData[P_level3] = upperData[P_level3];
        }
      } else {
        lowerData[P_level3] = value;
        if (keyMode == 2) {
          upperData[P_level3] = lowerData[P_level3];
        }
      }
      updatelevel3(1);
      break;

    case CCtime4:
      if (upperSW) {
        upperData[P_time4] = value;
        if (keyMode == 1) {
          lowerData[P_time4] = upperData[P_time4];
        }
      } else {
        lowerData[P_time4] = value;
        if (keyMode == 2) {
          upperData[P_time4] = lowerData[P_time4];
        }
      }
      updatetime4(1);
      break;

    case CC5stage_mode:
      if (upperSW) {
        upperData[P_env5stage_mode] = map(value, 0, 127, 0, 7);
        if (keyMode == 1) {
          lowerData[P_env5stage_mode] = upperData[P_env5stage_mode];
        }
      } else {
        lowerData[P_env5stage_mode] = map(value, 0, 127, 0, 7);
        if (keyMode == 2) {
          upperData[P_env5stage_mode] = lowerData[P_env5stage_mode];
        }
      }
      updateenv5stage_mode(1);
      break;

    case CC2time1:
      if (upperSW) {
        upperData[P_env2_time1] = value;
        if (keyMode == 1) {
          lowerData[P_env2_time1] = upperData[P_env2_time1];
        }
      } else {
        lowerData[P_env2_time1] = value;
        if (keyMode == 2) {
          upperData[P_env2_time1] = lowerData[P_env2_time1];
        }
      }
      updateenv2_time1(1);
      break;

    case CC2level1:
      if (upperSW) {
        upperData[P_env2_level1] = value;
        if (keyMode == 1) {
          lowerData[P_env2_level1] = upperData[P_env2_level1];
        }
      } else {
        lowerData[P_env2_level1] = value;
        if (keyMode == 2) {
          upperData[P_env2_level1] = lowerData[P_env2_level1];
        }
      }
      updateenv2_level1(1);
      break;

    case CC2time2:
      if (upperSW) {
        upperData[P_env2_time2] = value;
        if (keyMode == 1) {
          lowerData[P_env2_time2] = upperData[P_env2_time2];
        }
      } else {
        lowerData[P_env2_time2] = value;
        if (keyMode == 2) {
          upperData[P_env2_time2] = lowerData[P_env2_time2];
        }
      }
      updateenv2_time2(1);
      break;

    case CC2level2:
      if (upperSW) {
        upperData[P_env2_level2] = value;
        if (keyMode == 1) {
          lowerData[P_env2_level2] = upperData[P_env2_level2];
        }
      } else {
        lowerData[P_env2_level2] = value;
        if (keyMode == 2) {
          upperData[P_env2_level2] = lowerData[P_env2_level2];
        }
      }
      updateenv2_level2(1);
      break;

    case CC2time3:
      if (upperSW) {
        upperData[P_env2_time3] = value;
        if (keyMode == 1) {
          lowerData[P_env2_time3] = upperData[P_env2_time3];
        }
      } else {
        lowerData[P_env2_time3] = value;
        if (keyMode == 2) {
          upperData[P_env2_time3] = lowerData[P_env2_time3];
        }
      }
      updateenv2_time3(1);
      break;

    case CC2level3:
      if (upperSW) {
        upperData[P_env2_level3] = value;
        if (keyMode == 1) {
          lowerData[P_env2_level3] = upperData[P_env2_level3];
        }
      } else {
        lowerData[P_env2_level3] = value;
        if (keyMode == 2) {
          upperData[P_env2_level3] = lowerData[P_env2_level3];
        }
      }
      updateenv2_level3(1);
      break;

    case CC2time4:
      if (upperSW) {
        upperData[P_env2_time4] = value;
        if (keyMode == 1) {
          lowerData[P_env2_time4] = upperData[P_env2_time4];
        }
      } else {
        lowerData[P_env2_time4] = value;
        if (keyMode == 2) {
          upperData[P_env2_time4] = lowerData[P_env2_time4];
        }
      }
      updateenv2_time4(1);
      break;

    case CC25stage_mode:
      if (upperSW) {
        upperData[P_env2_5stage_mode] = map(value, 0, 127, 0, 7);
        if (keyMode == 1) {
          lowerData[P_env2_5stage_mode] = upperData[P_env2_5stage_mode];
        }
      } else {
        lowerData[P_env2_5stage_mode] = map(value, 0, 127, 0, 7);
        if (keyMode == 2) {
          upperData[P_env2_5stage_mode] = lowerData[P_env2_5stage_mode];
        }
      }
      updateenv2_env5stage_mode(1);
      break;

    case CCattack:
      if (upperSW) {
        upperData[P_attack] = value;
        if (keyMode == 1) {
          lowerData[P_attack] = upperData[P_attack];
        }
      } else {
        lowerData[P_attack] = value;
        if (keyMode == 2) {
          upperData[P_attack] = lowerData[P_attack];
        }
      }
      updateattack(1);
      break;

    case CCdecay:
      if (upperSW) {
        upperData[P_decay] = value;
        if (keyMode == 1) {
          lowerData[P_decay] = upperData[P_decay];
        }
      } else {
        lowerData[P_decay] = value;
        if (keyMode == 2) {
          upperData[P_decay] = lowerData[P_decay];
        }
      }
      updatedecay(1);
      break;

    case CCsustain:
      if (upperSW) {
        upperData[P_sustain] = value;
        if (keyMode == 1) {
          lowerData[P_sustain] = upperData[P_sustain];
        }
      } else {
        lowerData[P_sustain] = value;
        if (keyMode == 2) {
          upperData[P_sustain] = lowerData[P_sustain];
        }
      }
      updatesustain(1);
      break;

    case CCrelease:
      if (upperSW) {
        upperData[P_release] = value;
        if (keyMode == 1) {
          lowerData[P_release] = upperData[P_release];
        }
      } else {
        lowerData[P_release] = value;
        if (keyMode == 2) {
          upperData[P_release] = lowerData[P_release];
        }
      }
      updaterelease(1);
      break;

    case CCadsr_mode:
      if (upperSW) {
        upperData[P_adsr_mode] = map(value, 0, 127, 0, 7);
        if (keyMode == 1) {
          lowerData[P_adsr_mode] = upperData[P_adsr_mode];
        }
      } else {
        lowerData[P_adsr_mode] = map(value, 0, 127, 0, 7);
        if (keyMode == 2) {
          upperData[P_adsr_mode] = lowerData[P_adsr_mode];
        }
      }
      updateadsr_mode(1);
      break;

    case CC4attack:
      if (upperSW) {
        upperData[P_env4_attack] = value;
        if (keyMode == 1) {
          lowerData[P_env4_attack] = upperData[P_env4_attack];
        }
      } else {
        lowerData[P_env4_attack] = value;
        if (keyMode == 2) {
          upperData[P_env4_attack] = lowerData[P_env4_attack];
        }
      }
      updateenv4_attack(1);
      break;

    case CC4decay:
      if (upperSW) {
        upperData[P_env4_decay] = value;
        if (keyMode == 1) {
          lowerData[P_env4_decay] = upperData[P_env4_decay];
        }
      } else {
        lowerData[P_env4_decay] = value;
        if (keyMode == 2) {
          upperData[P_env4_decay] = lowerData[P_env4_decay];
        }
      }
      updateenv4_decay(1);
      break;

    case CC4sustain:
      if (upperSW) {
        upperData[P_env4_sustain] = value;
        if (keyMode == 1) {
          lowerData[P_env4_sustain] = upperData[P_env4_sustain];
        }
      } else {
        lowerData[P_env4_sustain] = value;
        if (keyMode == 2) {
          upperData[P_env4_sustain] = lowerData[P_env4_sustain];
        }
      }
      updateenv4_sustain(1);
      break;

    case CC4release:
      if (upperSW) {
        upperData[P_env4_release] = value;
        if (keyMode == 1) {
          lowerData[P_env4_release] = upperData[P_env4_release];
        }
      } else {
        lowerData[P_env4_release] = value;
        if (keyMode == 2) {
          upperData[P_env4_release] = lowerData[P_env4_release];
        }
      }
      updateenv4_release(1);
      break;

    case CC4adsr_mode:
      if (upperSW) {
        upperData[P_env4_adsr_mode] = map(value, 0, 127, 0, 7);
        if (keyMode == 1) {
          lowerData[P_env4_adsr_mode] = upperData[P_env4_adsr_mode];
        }
      } else {
        lowerData[P_env4_adsr_mode] = map(value, 0, 127, 0, 7);
        if (keyMode == 2) {
          upperData[P_env4_adsr_mode] = lowerData[P_env4_adsr_mode];
        }
      }
      updateenv4_adsr_mode(1);
      break;

    case CCdualdetune:
      dualdetune = value;
      dualdetune_str = value;
      updatedualdetune(1);
      break;

      // case CCunisondetune:
      //   unisondetune = value;
      //   updateunisondetune(1);
      //   break;

      //   // Buttons

    case CCoctave_down:
      if (upperSW) {
        upperChromaticSW = (upperChromaticSW - 1);
        if (upperChromaticSW < -24) {
          upperChromaticSW = -24;
        }
      } else {
        lowerChromaticSW = (lowerChromaticSW - 1);
        if (lowerChromaticSW < -24) {
          lowerChromaticSW = -24;
        }
      }
      updateoctave_send(1);
      break;

    case CCoctave_up:
      if (upperSW) {
        upperChromaticSW = upperChromaticSW + 1;
        if (upperChromaticSW > 24) {
          upperChromaticSW = 24;
        }
      } else {
        lowerChromaticSW = lowerChromaticSW + 1;  // was: lowerChromatic
        if (lowerChromaticSW > 24) {
          lowerChromaticSW = 24;
        }
      }
      updateoctave_send(1);
      break;

    case CCbend_enable:
      switch (bend_enable) {
        case 0:
          upperbend_enable_SW = 0;
          lowerbend_enable_SW = 0;
          break;

        case 1:
          upperbend_enable_SW = 0;
          lowerbend_enable_SW = 1;
          break;

        case 2:
          upperbend_enable_SW = 1;
          lowerbend_enable_SW = 0;
          break;

        case 3:
          upperbend_enable_SW = 1;
          lowerbend_enable_SW = 1;
          break;
      }
      updatebend_enable_button(1);
      break;

    case CCat_vib_sw:
      updateafter_vib_enable_button(1);
      break;

    case CCat_bri_sw:
      updateafter_bri_enable_button(1);
      break;

    case CCat_vol_sw:
      updateafter_vol_enable_button(1);
      break;

    case CCdual_button:
      updatedual_button(1);
      break;

    case CCsplit_button:
      updatesplit_button(1);
      break;

    case CCsingle_button:
      updatesingle_button(1);
      break;

    case CCspecial_button:
      updatespecial_button(1);
      break;

      // case CCpoly_button:
      //   updatepoly_button(1);
      //   break;

      // case CCmono_button:
      //   updatemono_button(1);
      //   break;

      // case CCunison_button:
      //   updateunison_button(1);
      //   break;

    case CClfo1_sync:
      if (upperSW) {
        upperData[P_lfo1_sync] = value;
        if (keyMode == 1) {
          lowerData[P_lfo1_sync] = upperData[P_lfo1_sync];
        }
      } else {
        lowerData[P_lfo1_sync] = value;
        if (keyMode == 2) {
          upperData[P_lfo1_sync] = lowerData[P_lfo1_sync];
        }
      }
      updatelfo1_sync(1);
      break;

    case CClfo2_sync:
      if (upperSW) {
        upperData[P_lfo2_sync] = value;
        if (keyMode == 1) {
          lowerData[P_lfo2_sync] = upperData[P_lfo2_sync];
        }
      } else {
        lowerData[P_lfo2_sync] = value;
        if (keyMode == 2) {
          upperData[P_lfo2_sync] = lowerData[P_lfo2_sync];
        }
      }
      updatelfo2_sync(1);
      break;

    case CCdco1_PWM_dyn:
      if (upperSW) {
        upperData[P_dco1_PWM_dyn] = value;
        if (keyMode == 1) {
          lowerData[P_dco1_PWM_dyn] = upperData[P_dco1_PWM_dyn];
        }
      } else {
        lowerData[P_dco1_PWM_dyn] = value;
        if (keyMode == 2) {
          upperData[P_dco1_PWM_dyn] = lowerData[P_dco1_PWM_dyn];
        }
      }
      updatedco1_PWM_dyn(1);
      break;

    case CCdco2_PWM_dyn:
      if (upperSW) {
        upperData[P_dco2_PWM_dyn] = value;
        if (keyMode == 1) {
          lowerData[P_dco2_PWM_dyn] = upperData[P_dco2_PWM_dyn];
        }
      } else {
        lowerData[P_dco2_PWM_dyn] = value;
        if (keyMode == 2) {
          upperData[P_dco2_PWM_dyn] = lowerData[P_dco2_PWM_dyn];
        }
      }
      updatedco2_PWM_dyn(1);
      break;

    case CCdco1_PWM_env_source:
      if (upperSW) {
        upperData[P_dco1_PWM_env_source] = value;
        if (keyMode == 1) {
          lowerData[P_dco1_PWM_env_source] = upperData[P_dco1_PWM_env_source];
        }
      } else {
        lowerData[P_dco1_PWM_env_source] = value;
        if (keyMode == 2) {
          upperData[P_dco1_PWM_env_source] = lowerData[P_dco1_PWM_env_source];
        }
      }
      updatedco1_PWM_env_source(1);
      break;

    case CCdco2_PWM_env_source:
      if (upperSW) {
        upperData[P_dco2_PWM_env_source] = value;
        if (keyMode == 1) {
          lowerData[P_dco2_PWM_env_source] = upperData[P_dco2_PWM_env_source];
        }
      } else {
        lowerData[P_dco2_PWM_env_source] = value;
        if (keyMode == 2) {
          upperData[P_dco2_PWM_env_source] = lowerData[P_dco2_PWM_env_source];
        }
      }
      updatedco2_PWM_env_source(1);
      break;

    case CCdco1_PWM_lfo_source:
      if (upperSW) {
        upperData[P_dco1_PWM_lfo_source] = value;
        if (keyMode == 1) {
          lowerData[P_dco1_PWM_lfo_source] = upperData[P_dco1_PWM_lfo_source];
        }
      } else {
        lowerData[P_dco1_PWM_lfo_source] = value;
        if (keyMode == 2) {
          upperData[P_dco1_PWM_lfo_source] = lowerData[P_dco1_PWM_lfo_source];
        }
      }
      updatedco1_PWM_lfo_source(1);
      break;

    case CCdco2_PWM_lfo_source:
      if (upperSW) {
        upperData[P_dco2_PWM_lfo_source] = value;
        if (keyMode == 1) {
          lowerData[P_dco2_PWM_lfo_source] = upperData[P_dco2_PWM_lfo_source];
        }
      } else {
        lowerData[P_dco2_PWM_lfo_source] = value;
        if (keyMode == 2) {
          upperData[P_dco2_PWM_lfo_source] = lowerData[P_dco2_PWM_lfo_source];
        }
      }
      updatedco2_PWM_lfo_source(1);
      break;

    case CCdco1_pitch_dyn:
      if (upperSW) {
        upperData[P_dco1_pitch_dyn] = value;
        if (keyMode == 1) {
          lowerData[P_dco1_pitch_dyn] = upperData[P_dco1_pitch_dyn];
        }
      } else {
        lowerData[P_dco1_pitch_dyn] = value;
        if (keyMode == 2) {
          upperData[P_dco1_pitch_dyn] = lowerData[P_dco1_pitch_dyn];
        }
      }
      updatedco1_pitch_dyn(1);
      break;

    case CCdco2_pitch_dyn:
      if (upperSW) {
        upperData[P_dco2_pitch_dyn] = value;
        if (keyMode == 1) {
          lowerData[P_dco2_pitch_dyn] = upperData[P_dco2_pitch_dyn];
        }
      } else {
        lowerData[P_dco2_pitch_dyn] = value;
        if (keyMode == 2) {
          upperData[P_dco2_pitch_dyn] = lowerData[P_dco2_pitch_dyn];
        }
      }
      updatedco2_pitch_dyn(1);
      break;

    case CCdco1_pitch_lfo_source:
      if (upperSW) {
        upperData[P_dco1_pitch_lfo_source] = value;
        if (keyMode == 1) {
          lowerData[P_dco1_pitch_lfo_source] = upperData[P_dco1_pitch_lfo_source];
        }
      } else {
        lowerData[P_dco1_pitch_lfo_source] = value;
        if (keyMode == 2) {
          upperData[P_dco1_pitch_lfo_source] = lowerData[P_dco1_pitch_lfo_source];
        }
      }
      updatedco1_pitch_lfo_source(1);
      break;

    case CCdco2_pitch_lfo_source:
      if (upperSW) {
        upperData[P_dco2_pitch_lfo_source] = value;
        if (keyMode == 1) {
          lowerData[P_dco2_pitch_lfo_source] = upperData[P_dco2_pitch_lfo_source];
        }
      } else {
        lowerData[P_dco2_pitch_lfo_source] = value;
        if (keyMode == 2) {
          upperData[P_dco2_pitch_lfo_source] = lowerData[P_dco2_pitch_lfo_source];
        }
      }
      updatedco2_pitch_lfo_source(1);
      break;

    case CCdco1_pitch_env_source:
      if (upperSW) {
        upperData[P_dco1_pitch_env_source] = value;
        if (keyMode == 1) {
          lowerData[P_dco1_pitch_env_source] = upperData[P_dco1_pitch_env_source];
        }
      } else {
        lowerData[P_dco1_pitch_env_source] = value;
        if (keyMode == 2) {
          upperData[P_dco1_pitch_env_source] = lowerData[P_dco1_pitch_env_source];
        }
      }
      updatedco1_pitch_env_source(1);
      break;

    case CCdco2_pitch_env_source:
      if (upperSW) {
        upperData[P_dco2_pitch_env_source] = value;
        if (keyMode == 1) {
          lowerData[P_dco2_pitch_env_source] = upperData[P_dco2_pitch_env_source];
        }
      } else {
        lowerData[P_dco2_pitch_env_source] = value;
        if (keyMode == 2) {
          upperData[P_dco2_pitch_env_source] = lowerData[P_dco2_pitch_env_source];
        }
      }
      updatedco2_pitch_env_source(1);
      break;

    case CCeditMode:
      updateeditMode(1);
      break;

    case CCdco_mix_env_source:
      if (upperSW) {
        upperData[P_dco_mix_env_source] = value;
        if (keyMode == 1) {
          lowerData[P_dco_mix_env_source] = upperData[P_dco_mix_env_source];
        }
      } else {
        lowerData[P_dco_mix_env_source] = value;
        if (keyMode == 2) {
          upperData[P_dco_mix_env_source] = lowerData[P_dco_mix_env_source];
        }
      }
      updatedco_mix_env_source(1);
      break;

    case CCdco_mix_dyn:
      if (upperSW) {
        upperData[P_dco_mix_dyn] = value;
        if (keyMode == 1) {
          lowerData[P_dco_mix_dyn] = upperData[P_dco_mix_dyn];
        }
      } else {
        lowerData[P_dco_mix_dyn] = value;
        if (keyMode == 2) {
          upperData[P_dco_mix_dyn] = lowerData[P_dco_mix_dyn];
        }
      }
      updatedco_mix_dyn(1);
      break;

    case CCvcf_env_source:
      if (upperSW) {
        upperData[P_vcf_env_source] = value;
        if (keyMode == 1) {
          lowerData[P_vcf_env_source] = upperData[P_vcf_env_source];
        }
      } else {
        lowerData[P_vcf_env_source] = value;
        if (keyMode == 2) {
          upperData[P_vcf_env_source] = lowerData[P_vcf_env_source];
        }
      }
      updatevcf_env_source(1);
      break;

    case CCvcf_dyn:
      if (upperSW) {
        upperData[P_vcf_dyn] = value;
        if (keyMode == 1) {
          lowerData[P_vcf_dyn] = upperData[P_vcf_dyn];
        }
      } else {
        lowerData[P_vcf_dyn] = value;
        if (keyMode == 2) {
          upperData[P_vcf_dyn] = lowerData[P_vcf_dyn];
        }
      }
      updatevcf_dyn(1);
      break;

    case CCvca_env_source:
      if (upperSW) {
        upperData[P_vca_env_source] = value;
        if (keyMode == 1) {
          lowerData[P_vca_env_source] = upperData[P_vca_env_source];
        }
      } else {
        lowerData[P_vca_env_source] = value;
        if (keyMode == 2) {
          upperData[P_vca_env_source] = lowerData[P_vca_env_source];
        }
      }
      updatevca_env_source(1);
      break;

    case CCvca_dyn:
      if (upperSW) {
        upperData[P_vca_dyn] = value;
        if (keyMode == 1) {
          lowerData[P_vca_dyn] = upperData[P_vca_dyn];
        }
      } else {
        lowerData[P_vca_dyn] = value;
        if (keyMode == 2) {
          upperData[P_vca_dyn] = lowerData[P_vca_dyn];
        }
      }
      updatevca_dyn(1);
      break;

    case CCchorus_sw:
      if (upperSW) {
        upperData[P_chorus] = value;
        if (keyMode == 1) {
          lowerData[P_chorus] = upperData[P_chorus];
        }
      } else {
        lowerData[P_chorus] = value;
        if (keyMode == 2) {
          upperData[P_chorus] = lowerData[P_chorus];
        }
      }
      updatechorus(1);
      break;


    case CCportamento_sw:
      switch (value) {
        case 0:
          upperPortamento_SW = 0;
          lowerPortamento_SW = 0;
          break;

        case 1:
          upperPortamento_SW = 0;
          lowerPortamento_SW = 1;
          break;

        case 2:
          upperPortamento_SW = 1;
          lowerPortamento_SW = 0;
          break;

        case 3:
          upperPortamento_SW = 1;
          lowerPortamento_SW = 1;
          break;
      }
      updateportamento_sw(1);
      break;

    case CCenv5stage:
      updateenv5stage(1);
      break;

    case CCadsr:
      updateadsr(1);
      break;
  }
}

// For continuous params (scaled 0..127 → 0..99)
static void flashToneScale(const char *label, int pIndex, int N = 100) {
  int u = unpackScaleN(upperData[pIndex], N);
  int l = unpackScaleN(lowerData[pIndex], N);
  showCurrentTonePage(label, String(u), String(l));
}

// For switch params (stepped, string labels)
static void flashToneStep(const char *label, int pIndex, const char **labels,
                          uint8_t step, uint8_t maxIndex) {
  const char *u = labels[unpackStep(upperData[pIndex], step, maxIndex)];
  const char *l = labels[unpackStep(lowerData[pIndex], step, maxIndex)];
  showCurrentTonePage(label, String(u), String(l));
}

FLASHMEM void updatelfo1_wave(bool announce) {

  const uint8_t newStep = upperSW ? upperData[P_lfo1_wave] : lowerData[P_lfo1_wave];

  static uint8_t lastStep = 0xFF;
  const bool changed = (newStep != lastStep);
  const bool firstTouch = (lastStepParam != CClfo1_wave);
  if (firstTouch) lastStepParam = CClfo1_wave;


  if (announce && !suppressParamAnnounce) {
    if (firstTouch || changed) {
      displayMode = 0;
      showCurrentTonePage("91 LFO1 WF",
                          lfowaveStepToLabel(upperData[P_lfo1_wave]),
                          lfowaveStepToLabel(lowerData[P_lfo1_wave]));
      startParameterDisplay();
    }
  }

  if (changed || recallPatchFlag) {
    if (upperSW) {
      sendVoiceParam(kBoardUpperPrefix, kBoardOffset0Prefix, 0xA1, lfowaveStepToValue(newStep));
      sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x3B, lfowaveStepToValue(newStep));
    } else {
      sendVoiceParam(kBoardLowerPrefix, kBoardOffset0Prefix, 0xA1, lfowaveStepToValue(newStep));
      sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x3B, lfowaveStepToValue(newStep));
    }
    lastStep = newStep;
  }
}

FLASHMEM void updatemod_lfo(bool announce) {
  const uint8_t displayVal = (uint8_t)map(
    upperSW ? upperLFOModDepth : lowerLFOModDepth,
    0, 127, 0, 99);

  if (announce && !suppressParamAnnounce) {
    displayMode = 1;
    if (upperSW) {
      showCurrentParameterPage("36 UPPER LFO MOD DEPTH", String(displayVal));
    } else {
      showCurrentParameterPage("46 LOWER LFO MOD DEPTH", String(displayVal));
    }
    startParameterDisplay();
  }

  if (upperSW) {
    sendModToBoards(upperLFOModDepth);
    sendCustomSysEx((upperChannel - 1), 0x22, upperLFOModDepth);
  } else {
    sendModToBoards(lowerLFOModDepth);
    sendCustomSysEx((lowerChannel - 1), 0x2B, lowerLFOModDepth);
  }
}

FLASHMEM void updateuppermod_lfo(bool announce) {
  sendCustomSysEx((upperChannel - 1), 0x22, upperLFOModDepth);
  sendModToBoards(upperLFOModDepth);
}

FLASHMEM void updatelowermod_lfo(bool announce) {
  sendCustomSysEx((lowerChannel - 1), 0x2B, lowerLFOModDepth);
  sendModToBoards(lowerLFOModDepth);
}

FLASHMEM void updatebend_range(bool announce) {

  const uint8_t newStep = static_cast<uint8_t>(map(bend_range, 0, 127, 0, 4));

  static uint8_t lastStep = 0xFF;
  const bool changed = (newStep != lastStep);
  const bool firstTouch = (lastStepParam != CCbend_range);
  if (firstTouch) lastStepParam = CCbend_range;


  bend_range_str = newStep;

  if (announce && !suppressParamAnnounce) {
    if (firstTouch || changed) {
      displayMode = 1;
      showCurrentParameterPage("16 BEND RANGE", bendStepToLabel(newStep));
      startParameterDisplay();
    }
  }

  if (changed || recallPatchFlag) {
    sendVoiceParam(kBoardBothPrefix, NO_OFFSET, 0xB7, bendStepToValue(newStep));
    if (bend_range_str == 4) {
      sendCustomSysEx((controlChannel - 1), 0x34, 0x01);
    } else {
      sendCustomSysEx((controlChannel - 1), 0x34, 0x00);
      sendCustomSysEx((controlChannel - 1), 0x17, bendStepToValue(newStep));
    }
    lastStep = newStep;
  }
}

FLASHMEM void updatelfo1_rate(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("93 LFO1 RATE", P_lfo1_rate);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset0Prefix, 0xA3, (uint8_t)upperData[P_lfo1_rate]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x3D, (uint8_t)upperData[P_lfo1_rate]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset0Prefix, 0xA3, (uint8_t)lowerData[P_lfo1_rate]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x3D, (uint8_t)lowerData[P_lfo1_rate]);
  }
}

FLASHMEM void updatelfo1_delay(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("92 LFO1 DELAY", P_lfo1_delay);
    startParameterDisplay();
  }
  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset0Prefix, 0xA2, (uint8_t)upperData[P_lfo1_delay]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x3C, (uint8_t)upperData[P_lfo1_delay]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset0Prefix, 0xA2, (uint8_t)lowerData[P_lfo1_delay]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x3C, (uint8_t)lowerData[P_lfo1_delay]);
  }
}

FLASHMEM void updatelfo1_lfo2(bool announce) {
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("94 LFO1 LFO", P_lfo1_lfo2);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset0Prefix, 0xA4, (uint8_t)upperData[P_lfo1_lfo2]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x3E, (uint8_t)upperData[P_lfo1_lfo2]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset0Prefix, 0xA4, (uint8_t)lowerData[P_lfo1_lfo2]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x3E, (uint8_t)lowerData[P_lfo1_lfo2]);
  }
}

FLASHMEM void updatedco1_PW(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("41 PWM1 WIDTH", P_dco1_PW);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset0Prefix, 0x8A, (uint8_t)upperData[P_dco1_PW]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x1F, (uint8_t)upperData[P_dco1_PW]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset0Prefix, 0x8A, (uint8_t)lowerData[P_dco1_PW]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x1F, (uint8_t)lowerData[P_dco1_PW]);
  }
}

FLASHMEM void updatedco1_PWM_env(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("42 PWM1 ENV", P_dco1_PWM_env);
    startParameterDisplay();
  }
  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset0Prefix, 0x8B, (uint8_t)upperData[P_dco1_PWM_env]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x20, (uint8_t)upperData[P_dco1_PWM_env]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset0Prefix, 0x8B, (uint8_t)lowerData[P_dco1_PWM_env]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x20, (uint8_t)lowerData[P_dco1_PWM_env]);
  }
}

FLASHMEM void updatedco1_PWM_lfo(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("43 PWM1 LFO", P_dco1_PWM_lfo);
    startParameterDisplay();
  }
  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset0Prefix, 0x8C, (uint8_t)upperData[P_dco1_PWM_lfo]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x21, (uint8_t)upperData[P_dco1_PWM_lfo]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset0Prefix, 0x8C, (uint8_t)lowerData[P_dco1_PWM_lfo]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x21, (uint8_t)lowerData[P_dco1_PWM_lfo]);
  }
}

FLASHMEM void updatedco1_pitch_env(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("16 DCO1 ENV", P_dco1_pitch_env);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset0Prefix, 0x85, (uint8_t)upperData[P_dco1_pitch_env]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x10, (uint8_t)upperData[P_dco1_pitch_env]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset0Prefix, 0x85, (uint8_t)lowerData[P_dco1_pitch_env]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x10, (uint8_t)lowerData[P_dco1_pitch_env]);
  }
}

FLASHMEM void updatedco1_pitch_lfo(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("14 DCO1 LFO", P_dco1_pitch_lfo);
    startParameterDisplay();
  }
  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset0Prefix, 0x83, (uint8_t)upperData[P_dco1_pitch_lfo]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x0E, (uint8_t)upperData[P_dco1_pitch_lfo]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset0Prefix, 0x83, (uint8_t)lowerData[P_dco1_pitch_lfo]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x0E, (uint8_t)lowerData[P_dco1_pitch_lfo]);
  }
}

FLASHMEM void updatedco1_wave(bool announce) {
  const uint8_t newStep = upperSW ? upperData[P_dco1_wave] : lowerData[P_dco1_wave];

  static uint8_t lastStep = 0xFF;
  const bool changed = (newStep != lastStep);
  const bool firstTouch = (lastStepParam != CCdco1_wave);
  if (firstTouch) lastStepParam = CCdco1_wave;

  if (announce && !suppressParamAnnounce) {
    if (firstTouch || changed) {
      displayMode = DM_TONE_FLASH;
      showCurrentTonePage("12 DCO1 WF",
                          dco1waveStepToLabel(upperData[P_dco1_wave]),
                          dco1waveStepToLabel(lowerData[P_dco1_wave]));
      startParameterDisplay();
    }
  }

  if (changed || recallPatchFlag) {
    if (upperSW) {
      sendVoiceParam(kBoardUpperPrefix, kBoardOffset0Prefix, 0x81, dco1waveStepToValue(newStep));
      sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x0C, dco1waveStepToValue(newStep));
    } else {
      sendVoiceParam(kBoardLowerPrefix, kBoardOffset0Prefix, 0x81, dco1waveStepToValue(newStep));
      sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x0C, dco1waveStepToValue(newStep));
    }
    lastStep = newStep;
  }
}

FLASHMEM void updatedco1_range(bool announce) {
  const uint8_t newStep = upperSW ? upperData[P_dco1_range] : lowerData[P_dco1_range];

  static uint8_t lastStep = 0xFF;
  const bool changed = (newStep != lastStep);
  const bool firstTouch = (lastStepParam != CCdco1_range);
  if (firstTouch) lastStepParam = CCdco1_range;

  if (announce && !suppressParamAnnounce) {
    if (firstTouch || changed) {
      displayMode = DM_TONE_FLASH;
      showCurrentTonePage("11 DCO1 RANG",
                          dco1rangeStepToLabel(upperData[P_dco1_range]),
                          dco1rangeStepToLabel(lowerData[P_dco1_range]));
      startParameterDisplay();
    }
  }

  if (changed || recallPatchFlag) {
    if (upperSW) {
      sendVoiceParam(kBoardUpperPrefix, kBoardOffset0Prefix, 0x80, dco1rangeStepToValue(newStep));
      sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x0B, dco1rangeStepToValue(newStep));
    } else {
      sendVoiceParam(kBoardLowerPrefix, kBoardOffset0Prefix, 0x80, dco1rangeStepToValue(newStep));
      sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x0B, dco1rangeStepToValue(newStep));
    }
    lastStep = newStep;
  }
}

FLASHMEM void updatedco1_tune(bool announce) {
  lastStepParam = 0xFF;
  const int activeStored = upperSW ? upperData[P_dco1_tune] : lowerData[P_dco1_tune];

  if (announce && !suppressParamAnnounce) {
    int upperIdx = unpackSplitTune(upperData[P_dco1_tune]);
    int lowerIdx = unpackSplitTune(lowerData[P_dco1_tune]);
    displayMode = DM_TONE_FLASH;
    showCurrentTonePage("13 DCO1 TUNE",
                        toneTunePtrs[upperIdx],
                        toneTunePtrs[lowerIdx]);
    startParameterDisplay();
  }
  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset0Prefix, 0x82, (uint8_t)upperData[P_dco1_tune]);
    sendToneSysEx(kBoardUpperPrefix, (upperChannel - 1), 0x0D, (uint8_t)activeStored);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset0Prefix, 0x82, (uint8_t)lowerData[P_dco1_tune]);
    sendToneSysEx(kBoardLowerPrefix, (lowerChannel - 1), 0x0D, (uint8_t)activeStored);
  }
}

FLASHMEM void updatedco1_xmod(bool announce) {
  const uint8_t newStep = upperSW ? upperData[P_dco1_mode] : lowerData[P_dco1_mode];

  static uint8_t lastStep = 0xFF;
  const bool changed = (newStep != lastStep);
  const bool firstTouch = (lastStepParam != CCdco1_xmod);
  if (firstTouch) lastStepParam = CCdco1_xmod;


  if (announce && !suppressParamAnnounce) {
    if (firstTouch || changed) {
      displayMode = 0;
      showCurrentTonePage("31 DCO X-MOD",
                          dco1xmodStepToLabel(upperData[P_dco1_mode]),
                          dco1xmodStepToLabel(lowerData[P_dco1_mode]));
      startParameterDisplay();
    }
  }

  if (changed || recallPatchFlag) {
    if (upperSW) {
      sendVoiceParam(kBoardUpperPrefix, NO_OFFSET, 0x88, dco1xmodStepToValue(newStep));
      sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x1B, dco1xmodStepToValue(newStep));
    } else {
      sendVoiceParam(kBoardLowerPrefix, NO_OFFSET, 0x88, dco1xmodStepToValue(newStep));
      sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x1B, dco1xmodStepToValue(newStep));
    }
    lastStep = newStep;
  }
}

FLASHMEM void updatelfo2_wave(bool announce) {
  const uint8_t newStep = upperSW ? upperData[P_lfo2_wave] : lowerData[P_lfo2_wave];

  static uint8_t lastStep = 0xFF;
  const bool changed = (newStep != lastStep);
  const bool firstTouch = (lastStepParam != CClfo2_wave);
  if (firstTouch) lastStepParam = CClfo2_wave;


  if (announce && !suppressParamAnnounce) {
    if (firstTouch || changed) {
      displayMode = 0;
      showCurrentTonePage("A1 LFO2 WF",
                          lfowaveStepToLabel(upperData[P_lfo2_wave]),
                          lfowaveStepToLabel(lowerData[P_lfo2_wave]));
      startParameterDisplay();
    }
  }

  if (changed || recallPatchFlag) {
    if (upperSW) {
      sendVoiceParam(kBoardUpperPrefix, kBoardOffset1Prefix, 0xA1, lfowaveStepToValue(newStep));
      sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x40, lfowaveStepToValue(newStep));
    } else {
      sendVoiceParam(kBoardLowerPrefix, kBoardOffset1Prefix, 0xA1, lfowaveStepToValue(newStep));
      sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x40, lfowaveStepToValue(newStep));
    }
    lastStep = newStep;
  }
}

FLASHMEM void updatelfo2_rate(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("A3 LFO2 RATE", P_lfo2_rate);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset1Prefix, 0xA3, (uint8_t)upperData[P_lfo2_rate]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x42, (uint8_t)upperData[P_lfo2_rate]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset1Prefix, 0xA3, (uint8_t)lowerData[P_lfo2_rate]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x42, (uint8_t)lowerData[P_lfo2_rate]);
  }
}

FLASHMEM void updatelfo2_delay(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("A2 LFO2 DELAY", P_lfo2_delay);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset1Prefix, 0xA2, (uint8_t)upperData[P_lfo2_delay]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x41, (uint8_t)upperData[P_lfo2_delay]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset1Prefix, 0xA2, (uint8_t)lowerData[P_lfo2_delay]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x41, (uint8_t)lowerData[P_lfo2_delay]);
  }
}

FLASHMEM void updatelfo2_lfo1(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("A4 LFO2 LFO", P_lfo2_lfo1);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset1Prefix, 0xA4, (uint8_t)upperData[P_lfo2_lfo1]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x43, (uint8_t)upperData[P_lfo2_lfo1]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset1Prefix, 0xA4, (uint8_t)lowerData[P_lfo2_lfo1]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x43, (uint8_t)lowerData[P_lfo2_lfo1]);
  }
}

FLASHMEM void updatedco2_PW(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("51 PWM2 WIDTH", P_dco2_PW);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset1Prefix, 0x8A, (uint8_t)upperData[P_dco2_PW]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x25, (uint8_t)upperData[P_dco2_PW]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset1Prefix, 0x8A, (uint8_t)lowerData[P_dco2_PW]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x25, (uint8_t)lowerData[P_dco2_PW]);
  }
}

FLASHMEM void updatedco2_PWM_env(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("52 PWM2 ENV", P_dco2_PWM_env);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset1Prefix, 0x8B, (uint8_t)upperData[P_dco2_PWM_env]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x26, (uint8_t)upperData[P_dco2_PWM_env]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset1Prefix, 0x8B, (uint8_t)lowerData[P_dco2_PWM_env]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x26, (uint8_t)lowerData[P_dco2_PWM_env]);
  }
}

FLASHMEM void updatedco2_PWM_lfo(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("53 PWM2 LFO", P_dco2_PWM_lfo);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset1Prefix, 0x8C, (uint8_t)upperData[P_dco2_PWM_lfo]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x27, (uint8_t)upperData[P_dco2_PWM_lfo]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset1Prefix, 0x8C, (uint8_t)lowerData[P_dco2_PWM_lfo]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x27, (uint8_t)lowerData[P_dco2_PWM_lfo]);
  }
}

FLASHMEM void updatedco2_pitch_env(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("26 DCO2 ENV", P_dco2_pitch_env);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset1Prefix, 0x85, (uint8_t)upperData[P_dco2_pitch_env]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x18, (uint8_t)upperData[P_dco2_pitch_env]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset1Prefix, 0x85, (uint8_t)lowerData[P_dco2_pitch_env]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x18, (uint8_t)lowerData[P_dco2_pitch_env]);
  }
}

FLASHMEM void updatedco2_pitch_lfo(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("24 DCO2 LFO", P_dco2_pitch_lfo);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset1Prefix, 0x83, (uint8_t)upperData[P_dco2_pitch_lfo]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x16, (uint8_t)upperData[P_dco2_pitch_lfo]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset1Prefix, 0x83, (uint8_t)lowerData[P_dco2_pitch_lfo]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x16, (uint8_t)lowerData[P_dco2_pitch_lfo]);
  }
}

FLASHMEM void updatedco2_wave(bool announce) {
  const uint8_t newStep = upperSW ? upperData[P_dco2_wave] : lowerData[P_dco2_wave];

  static uint8_t lastStep = 0xFF;
  const bool changed = (newStep != lastStep);
  const bool firstTouch = (lastStepParam != CCdco2_wave);
  if (firstTouch) lastStepParam = CCdco2_wave;


  if (announce && !suppressParamAnnounce) {
    if (firstTouch || changed) {
      displayMode = DM_TONE_FLASH;
      showCurrentTonePage("22 DCO2 WF",
                          dco1waveStepToLabel(upperData[P_dco2_wave]),
                          dco1waveStepToLabel(lowerData[P_dco2_wave]));
      startParameterDisplay();
    }
  }

  if (changed || recallPatchFlag) {
    if (upperSW) {
      sendVoiceParam(kBoardUpperPrefix, kBoardOffset1Prefix, 0x81, dco1waveStepToValue(newStep));
      sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x14, dco1waveStepToValue(newStep));
    } else {
      sendVoiceParam(kBoardLowerPrefix, kBoardOffset1Prefix, 0x81, dco1waveStepToValue(newStep));
      sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x14, dco1waveStepToValue(newStep));
    }
    lastStep = newStep;
  }
}

FLASHMEM void updatedco2_range(bool announce) {
  const uint8_t newStep = upperSW ? upperData[P_dco2_range] : lowerData[P_dco2_range];

  static uint8_t lastStep = 0xFF;
  const bool changed = (newStep != lastStep);
  const bool firstTouch = (lastStepParam != CCdco2_range);
  if (firstTouch) lastStepParam = CCdco2_range;


  if (announce && !suppressParamAnnounce) {
    if (firstTouch || changed) {
      displayMode = DM_TONE_FLASH;
      showCurrentTonePage("21 DCO2 RANG",
                          dco1rangeStepToLabel(upperData[P_dco2_range]),
                          dco1rangeStepToLabel(lowerData[P_dco2_range]));
      startParameterDisplay();
    }
  }

  if (changed || recallPatchFlag) {
    if (upperSW) {
      sendVoiceParam(kBoardUpperPrefix, kBoardOffset1Prefix, 0x80, dco1rangeStepToValue(newStep));
      sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x13, dco1rangeStepToValue(newStep));
    } else {
      sendVoiceParam(kBoardLowerPrefix, kBoardOffset1Prefix, 0x80, dco1rangeStepToValue(newStep));
      sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x13, dco1rangeStepToValue(newStep));
    }
    lastStep = newStep;
  }
}

FLASHMEM void updatedco2_tune(bool announce) {
  lastStepParam = 0xFF;
  const int activeStored = upperSW ? upperData[P_dco2_tune] : lowerData[P_dco2_tune];

  if (announce && !suppressParamAnnounce) {
    int upperIdx = unpackSplitTune(upperData[P_dco2_tune]);
    int lowerIdx = unpackSplitTune(lowerData[P_dco2_tune]);
    displayMode = DM_TONE_FLASH;
    showCurrentTonePage("23 DCO2 TUNE",
                        toneTunePtrs[upperIdx],
                        toneTunePtrs[lowerIdx]);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset1Prefix, 0x82, (uint8_t)upperData[P_dco2_tune]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x15, (uint8_t)activeStored);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset1Prefix, 0x82, (uint8_t)lowerData[P_dco2_tune]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x15, (uint8_t)activeStored);
  }
}

static inline int unpackSplit(int stored) {
  int v = stored & 0x7F;
  if (v < 0x40) {
    // Negative half: 0x00 -> 0, 0x3F -> 50
    return (v * 50 + 0x1F) / 0x3F;
  } else {
    // Positive half: 0x40 -> 51, 0x7F -> 101
    return 51 + ((v - 0x40) * 50 + 0x1F) / 0x3F;
  }
}

FLASHMEM void updatedco2_fine(bool announce) {
  lastStepParam = 0xFF;

  const int activeStored = upperSW ? upperData[P_dco2_fine] : lowerData[P_dco2_fine];

  if (announce && !suppressParamAnnounce) {
    int upperIdx = unpackSplitFine(upperData[P_dco2_fine]);
    int lowerIdx = unpackSplitFine(lowerData[P_dco2_fine]);
    displayMode = DM_TONE_FLASH;
    showCurrentTonePage("32 DCO2 FTUN",
                        toneFinePtrs[upperIdx],
                        toneFinePtrs[lowerIdx]);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, NO_OFFSET, 0x89, (uint8_t)upperData[P_dco2_fine]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x1C, (uint8_t)activeStored);
  } else {
    sendVoiceParam(kBoardLowerPrefix, NO_OFFSET, 0x89, (uint8_t)lowerData[P_dco2_fine]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x1C, (uint8_t)activeStored);
  }
}

FLASHMEM void updatedco1_level(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("61 MIX DCO1", P_dco1_level);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, NO_OFFSET, 0x90, (uint8_t)upperData[P_dco1_level]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x2B, (uint8_t)upperData[P_dco1_level]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, NO_OFFSET, 0x90, (uint8_t)lowerData[P_dco1_level]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x2B, (uint8_t)lowerData[P_dco1_level]);
  }
}

FLASHMEM void updatedco2_level(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("62 MIX DCO2", P_dco2_level);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, NO_OFFSET, 0x91, (uint8_t)upperData[P_dco2_level]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x2C, (uint8_t)upperData[P_dco2_level]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, NO_OFFSET, 0x91, (uint8_t)lowerData[P_dco2_level]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x2C, (uint8_t)lowerData[P_dco2_level]);
  }
}

FLASHMEM void updatedco2_mod(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("63 MIX ENV", P_dco2_mod);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, NO_OFFSET, 0x92, (uint8_t)upperData[P_dco2_mod]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x2D, (uint8_t)upperData[P_dco2_mod]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, NO_OFFSET, 0x92, (uint8_t)lowerData[P_dco2_mod]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x2D, (uint8_t)lowerData[P_dco2_mod]);
  }
}

FLASHMEM void updatevcf_hpf(bool announce) {
  const uint8_t newStep = upperSW ? upperData[P_vcf_hpf] : lowerData[P_vcf_hpf];

  static uint8_t lastStep = 0xFF;
  const bool changed = (newStep != lastStep);
  const bool firstTouch = (lastStepParam != CCvcf_hpf);
  if (firstTouch) lastStepParam = CCvcf_hpf;


  if (announce && !suppressParamAnnounce) {
    if (firstTouch || changed) {
      displayMode = 0;
      showCurrentTonePage("33 VCF HPF",
                          vcfrangeStepToLabel(upperData[P_vcf_hpf]),
                          vcfrangeStepToLabel(lowerData[P_vcf_hpf]));
      startParameterDisplay();
    }
  }

  if (changed || recallPatchFlag) {
    if (upperSW) {
      sendVoiceParam(kBoardUpperPrefix, NO_OFFSET, 0x95, vcfrangeStepToValue(newStep));
      sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x1D, vcfrangeStepToValue(newStep));
    } else {
      sendVoiceParam(kBoardLowerPrefix, NO_OFFSET, 0x95, vcfrangeStepToValue(newStep));
      sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x1D, vcfrangeStepToValue(newStep));
    }
    lastStep = newStep;
  }
}

FLASHMEM void updatevcf_cutoff(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("71 VCF FREQ", P_vcf_cutoff);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, NO_OFFSET, 0x96, (uint8_t)upperData[P_vcf_cutoff]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x30, (uint8_t)upperData[P_vcf_cutoff]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, NO_OFFSET, 0x96, (uint8_t)lowerData[P_vcf_cutoff]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x30, (uint8_t)lowerData[P_vcf_cutoff]);
  }
}

FLASHMEM void updatevcf_res(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("72 VCF RES", P_vcf_res);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, NO_OFFSET, 0x97, (uint8_t)upperData[P_vcf_res]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x31, (uint8_t)upperData[P_vcf_res]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, NO_OFFSET, 0x97, (uint8_t)lowerData[P_vcf_res]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x31, (uint8_t)lowerData[P_vcf_res]);
  }
}

FLASHMEM void updatevcf_kb(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("76 VCF KEY", P_vcf_kb);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, NO_OFFSET, 0x9B, (uint8_t)upperData[P_vcf_kb]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x35, (uint8_t)upperData[P_vcf_kb]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, NO_OFFSET, 0x9B, (uint8_t)lowerData[P_vcf_kb]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x35, (uint8_t)lowerData[P_vcf_kb]);
  }
}

FLASHMEM void updatevcf_env(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("75 VCF ENV", P_vcf_env);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, NO_OFFSET, 0x9A, (uint8_t)upperData[P_vcf_env]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x34, (uint8_t)upperData[P_vcf_env]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, NO_OFFSET, 0x9A, (uint8_t)lowerData[P_vcf_env]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x34, (uint8_t)lowerData[P_vcf_env]);
  }
}

FLASHMEM void updatevcf_lfo1(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("73 VCF LFO1", P_vcf_lfo1);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, NO_OFFSET, 0x98, (uint8_t)upperData[P_vcf_lfo1]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x32, (uint8_t)upperData[P_vcf_lfo1]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, NO_OFFSET, 0x98, (uint8_t)lowerData[P_vcf_lfo1]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x32, (uint8_t)lowerData[P_vcf_lfo1]);
  }
}

FLASHMEM void updatevcf_lfo2(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("74 VCF LFO2", P_vcf_lfo2);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, NO_OFFSET, 0x99, (uint8_t)upperData[P_vcf_lfo2]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x33, (uint8_t)upperData[P_vcf_lfo2]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, NO_OFFSET, 0x99, (uint8_t)lowerData[P_vcf_lfo2]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x33, (uint8_t)lowerData[P_vcf_lfo2]);
  }
}

FLASHMEM void updateat_vib(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 1;
    showCurrentParameterPage("21 AFTER TOUCH VIB", String(at_vib_str));
    startParameterDisplay();
  }
  sendVoiceParam(0xFD, 0x01, 0xB8, (uint8_t)at_vib);
  sendCustomSysEx((controlChannel - 1), 0x1A, (uint8_t)at_vib);
}

FLASHMEM void updateat_bri(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 1;
    showCurrentParameterPage("22 AFTER TOUCH BRI", String(at_bri_str));
    startParameterDisplay();
  }
  sendVoiceParam(0xFD, 0x03, 0xB9, (uint8_t)at_bri);
  sendCustomSysEx((controlChannel - 1), 0x1B, (uint8_t)at_bri);
}

FLASHMEM void updateat_vol(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 1;
    showCurrentParameterPage("23 AFTER TOUCH VOL", String(at_vol_str));
    startParameterDisplay();
  }
  sendVoiceParam(0xFD, 0x05, 0xBA, (uint8_t)at_vol);
  sendCustomSysEx((controlChannel - 1), 0x1C, (uint8_t)at_vol);
}

static inline uint8_t clamp_u8(uint16_t v, uint8_t lo, uint8_t hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return static_cast<uint8_t>(v);
}

static inline uint8_t map_u8(uint8_t x, uint8_t in_min, uint8_t in_max, uint8_t out_min, uint8_t out_max) {
  if (in_max == in_min) return out_min;
  const uint16_t num = static_cast<uint16_t>(x - in_min) * static_cast<uint16_t>(out_max - out_min);
  const uint16_t den = static_cast<uint16_t>(in_max - in_min);
  const uint16_t y = static_cast<uint16_t>(out_min) + (num + den / 2) / den;  // rounded
  return clamp_u8(y, out_min, out_max);
}

// Single function that always computes both boards correctly
void sendVCABalance() {
  const uint8_t km = (uint8_t)keyMode;
  const uint8_t bal = (uint8_t)balance;
  const uint8_t lowerLevel = (uint8_t)lowerData[P_vca_mod];
  const uint8_t upperLevel = (uint8_t)upperData[P_vca_mod];

  if (km == 1) {
    // Whole mode - balance ignored, each board gets its full scaled level
    lowerVCASend = map_u8(lowerLevel, 0, 127, 0, kMaxLevel);
    sendVoiceParam(kBoardBothPrefix, NO_OFFSET, kBalanceParam, lowerVCASend);
  }

  if (km == 2) {
    // Whole mode - balance ignored, each board gets its full scaled level
    upperVCASend = map_u8(upperLevel, 0, 127, 0, kMaxLevel);
    sendVoiceParam(kBoardBothPrefix, NO_OFFSET, kBalanceParam, upperVCASend);
  }

  if (km == 0 || km == 3 || km == 4 || km == 5) {
    // Equal-power crossfade approximation
    uint8_t lowerGain, upperGain;

    if (bal <= 64) {
      upperGain = map_u8(bal, 0, 64, 0, 127);
      lowerGain = 127;
    } else {
      lowerGain = map_u8(127 - bal, 0, 63, 0, 127);
      upperGain = 127;
    }

    uint8_t balancedLower = map_u8(lowerGain, 0, 127, 0, lowerLevel);
    uint8_t balancedUpper = map_u8(upperGain, 0, 127, 0, upperLevel);

    // Chase Play attenuates the Lower side while active
    if (chasePlay) {
      balancedLower = (uint8_t)(((uint16_t)balancedLower * (uint16_t)chaseLevel) / 127);
    }

    lowerVCASend = map_u8(balancedLower, 0, 127, 0, kMaxLevel);
    upperVCASend = map_u8(balancedUpper, 0, 127, 0, kMaxLevel);
    sendVoiceParam(kBoardLowerPrefix, NO_OFFSET, kBalanceParam, lowerVCASend);
    sendVoiceParam(kBoardUpperPrefix, NO_OFFSET, kBalanceParam, upperVCASend);
  }
}

FLASHMEM void updatevca_mod(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("81 VCA LEVEL", P_vca_mod);
    startParameterDisplay();
  }
  sendVCABalance();
  if (upperSW) {
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x38, (uint8_t)upperData[P_vca_mod]);
  } else {
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x38, (uint8_t)lowerData[P_vca_mod]);
  }
}

FLASHMEM void updatebalance(bool announce) {
  lastStepParam = 0xFF;
  balance_str = map(balance, 0, 127, 0, 99);
  if (announce && !suppressParamAnnounce) {
    displayMode = 1;
    showCurrentParameterPage("11 U/L BALANCE", String(balance_str));
    startParameterDisplay();
  }
  sendVCABalance();
  sendCustomSysEx((controlChannel - 1), 0x12, balance);
}

FLASHMEM void updateportamento(bool announce) {
  lastStepParam = 0xFF;
  portamento_str = map(portamento_str, 0, 127, 0, 99);
  if (announce && !suppressParamAnnounce) {
    displayMode = 1;
    showCurrentParameterPage("15 PORTAMENTO TIME", String(portamento_str));
    startParameterDisplay();
  }
  sendVoiceParam(kBoardBothPrefix, NO_OFFSET, 0xB0, portamento);
  sendCustomSysEx((controlChannel - 1), 0x16, portamento);
}

FLASHMEM void updatevolume(bool announce) {

  lastStepParam = 0xFF;
  volume_str = map(volume_str, 0, 127, 0, 99);
  if (announce && !suppressParamAnnounce) {
    displayMode = 1;
    showCurrentParameterPage("18 TOTAL VOLUME", String(volume_str));
    startParameterDisplay();
  }

  sendVoiceParam(kBoardBothPrefix, NO_OFFSET, 0xB1, volume);
  sendCustomSysEx((controlChannel - 1), 0x19, volume);
}

FLASHMEM void updatetime1(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("B1 ENV1 T1", P_time1);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset0Prefix, 0xA6, (uint8_t)upperData[P_time1]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x45, (uint8_t)upperData[P_time1]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset0Prefix, 0xA6, (uint8_t)lowerData[P_time1]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x45, (uint8_t)lowerData[P_time1]);
  }
}

FLASHMEM void updatelevel1(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("B2 ENV1 L1", P_level1);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset0Prefix, 0xA7, (uint8_t)upperData[P_level1]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x46, (uint8_t)upperData[P_level1]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset0Prefix, 0xA7, (uint8_t)lowerData[P_level1]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x46, (uint8_t)lowerData[P_level1]);
  }
}

FLASHMEM void updatetime2(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("B3 ENV1 T2", P_time2);
    startParameterDisplay();
  }
  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset0Prefix, 0xA8, (uint8_t)upperData[P_time2]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x47, (uint8_t)upperData[P_time2]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset0Prefix, 0xA8, (uint8_t)lowerData[P_time2]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x47, (uint8_t)lowerData[P_time2]);
  }
}

FLASHMEM void updatelevel2(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("B4 ENV1 L2", P_level2);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset0Prefix, 0xA9, (uint8_t)upperData[P_level2]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x48, (uint8_t)upperData[P_level2]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset0Prefix, 0xA9, (uint8_t)lowerData[P_level2]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x48, (uint8_t)lowerData[P_level2]);
  }
}

FLASHMEM void updatetime3(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("B5 ENV1 T3", P_time3);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset0Prefix, 0xAA, (uint8_t)upperData[P_time3]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x49, (uint8_t)upperData[P_time3]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset0Prefix, 0xAA, (uint8_t)lowerData[P_time3]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x49, (uint8_t)lowerData[P_time3]);
  }
}

FLASHMEM void updatelevel3(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("B6 ENV1 L3", P_level3);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset0Prefix, 0xAB, (uint8_t)upperData[P_level3]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x4A, (uint8_t)upperData[P_level3]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset0Prefix, 0xAB, (uint8_t)lowerData[P_level3]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x4A, (uint8_t)lowerData[P_level3]);
  }
}

FLASHMEM void updatetime4(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("B7 ENV1 T4", P_time4);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset0Prefix, 0xAC, (uint8_t)upperData[P_time4]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x4B, (uint8_t)upperData[P_time4]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset0Prefix, 0xAC, (uint8_t)lowerData[P_time4]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x4B, (uint8_t)lowerData[P_time4]);
  }
}

FLASHMEM void updateenv5stage_mode(bool announce) {
  const uint8_t newStep = upperSW ? upperData[P_env5stage_mode] : lowerData[P_env5stage_mode];

  static uint8_t lastStep = 0xFF;
  const bool changed = (newStep != lastStep);
  const bool firstTouch = (lastStepParam != CC5stage_mode);
  if (firstTouch) lastStepParam = CC5stage_mode;


  if (announce && !suppressParamAnnounce) {
    if (firstTouch || changed) {
      displayMode = 0;
      showCurrentTonePage("B8 ENV1 KEY",
                          env5stageModeStepToLabel(upperData[P_env5stage_mode]),
                          env5stageModeStepToLabel(lowerData[P_env5stage_mode]));
      startParameterDisplay();
    }
  }

  if (changed || recallPatchFlag) {
    if (upperSW) {
      sendVoiceParam(kBoardUpperPrefix, kBoardOffset0Prefix, 0xAD, env5stageModeStepToValue(newStep));
      sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x4C, env5stageModeStepToValue(newStep));
    } else {
      sendVoiceParam(kBoardLowerPrefix, kBoardOffset0Prefix, 0xAD, env5stageModeStepToValue(newStep));
      sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x4C, env5stageModeStepToValue(newStep));
    }
    lastStep = newStep;
  }
}

FLASHMEM void updateenv2_time1(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("C1 ENV2 T1", P_env2_time1);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset1Prefix, 0xA6, (uint8_t)upperData[P_env2_time1]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x4D, (uint8_t)upperData[P_env2_time1]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset1Prefix, 0xA6, (uint8_t)lowerData[P_env2_time1]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x4D, (uint8_t)lowerData[P_env2_time1]);
  }
}

FLASHMEM void updateenv2_level1(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("C2 ENV2 L1", P_env2_level1);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset1Prefix, 0xA7, (uint8_t)upperData[P_env2_level1]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x4E, (uint8_t)upperData[P_env2_level1]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset1Prefix, 0xA7, (uint8_t)lowerData[P_env2_level1]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x4E, (uint8_t)lowerData[P_env2_level1]);
  }
}

FLASHMEM void updateenv2_time2(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("C3 ENV2 T2", P_env2_time2);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset1Prefix, 0xA8, (uint8_t)upperData[P_env2_time2]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x4F, (uint8_t)upperData[P_env2_time2]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset1Prefix, 0xA8, (uint8_t)lowerData[P_env2_time2]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x4F, (uint8_t)lowerData[P_env2_time2]);
  }
}

FLASHMEM void updateenv2_level2(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("C4 ENV2 L2", P_env2_level2);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset1Prefix, 0xA9, (uint8_t)upperData[P_env2_level2]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x50, (uint8_t)upperData[P_env2_level2]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset1Prefix, 0xA9, (uint8_t)lowerData[P_env2_level2]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x50, (uint8_t)lowerData[P_env2_level2]);
  }
}

FLASHMEM void updateenv2_time3(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("C5 ENV2 T3", P_env2_time3);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset1Prefix, 0xAA, (uint8_t)upperData[P_env2_time3]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x51, (uint8_t)upperData[P_env2_time3]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset1Prefix, 0xAA, (uint8_t)lowerData[P_env2_time3]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x51, (uint8_t)lowerData[P_env2_time3]);
  }
}

FLASHMEM void updateenv2_level3(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("C6 ENV2 L3", P_env2_level3);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset1Prefix, 0xAB, (uint8_t)upperData[P_env2_level3]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x52, (uint8_t)upperData[P_env2_level3]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset1Prefix, 0xAB, (uint8_t)lowerData[P_env2_level3]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x52, (uint8_t)lowerData[P_env2_level3]);
  }
}

FLASHMEM void updateenv2_time4(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("C7 ENV2 T4", P_env2_time4);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset1Prefix, 0xAC, (uint8_t)upperData[P_env2_time4]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x53, (uint8_t)upperData[P_env2_time4]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset1Prefix, 0xAC, (uint8_t)lowerData[P_env2_time4]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x53, (uint8_t)lowerData[P_env2_time4]);
  }
}

FLASHMEM void updateenv2_env5stage_mode(bool announce) {
  const uint8_t newStep = upperSW ? upperData[P_env2_5stage_mode] : lowerData[P_env2_5stage_mode];

  static uint8_t lastStep = 0xFF;
  const bool changed = (newStep != lastStep);
  const bool firstTouch = (lastStepParam != CC25stage_mode);
  if (firstTouch) lastStepParam = CC25stage_mode;


  if (announce && !suppressParamAnnounce) {
    if (firstTouch || changed) {
      displayMode = 0;
      showCurrentTonePage("C8 ENV2 KEY",
                          env5stageModeStepToLabel(upperData[P_env2_5stage_mode]),
                          env5stageModeStepToLabel(lowerData[P_env2_5stage_mode]));
      startParameterDisplay();
    }
  }

  if (changed || recallPatchFlag) {
    if (upperSW) {
      sendVoiceParam(kBoardUpperPrefix, kBoardOffset1Prefix, 0xAD, env5stageModeStepToValue(newStep));
      sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x54, env5stageModeStepToValue(newStep));
    } else {
      sendVoiceParam(kBoardLowerPrefix, kBoardOffset1Prefix, 0xAD, env5stageModeStepToValue(newStep));
      sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x54, env5stageModeStepToValue(newStep));
    }
    lastStep = newStep;
  }
}

FLASHMEM void updateattack(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("D1 ENV3 ATT", P_attack);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset2Prefix, 0xA6, (uint8_t)upperData[P_attack]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x55, (uint8_t)upperData[P_attack]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset2Prefix, 0xA6, (uint8_t)lowerData[P_attack]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x55, (uint8_t)lowerData[P_attack]);
  }
}

FLASHMEM void updatedecay(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("D2 ENV3 DECY", P_decay);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset2Prefix, 0xAA, (uint8_t)upperData[P_decay]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x56, (uint8_t)upperData[P_decay]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset2Prefix, 0xAA, (uint8_t)lowerData[P_decay]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x56, (uint8_t)lowerData[P_decay]);
  }
}

FLASHMEM void updatesustain(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("D3 ENV3 SUST", P_sustain);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset2Prefix, 0xAB, (uint8_t)upperData[P_sustain]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x57, (uint8_t)upperData[P_sustain]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset2Prefix, 0xAB, (uint8_t)lowerData[P_sustain]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x57, (uint8_t)lowerData[P_sustain]);
  }
}

FLASHMEM void updaterelease(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("D4 ENV3 REL", P_release);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset2Prefix, 0xAC, (uint8_t)upperData[P_release]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x58, (uint8_t)upperData[P_release]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset2Prefix, 0xAC, (uint8_t)lowerData[P_release]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x58, (uint8_t)lowerData[P_release]);
  }
}

FLASHMEM void updateadsr_mode(bool announce) {
  const uint8_t newStep = upperSW ? upperData[P_adsr_mode] : lowerData[P_adsr_mode];

  static uint8_t lastStep = 0xFF;
  const bool changed = (newStep != lastStep);
  const bool firstTouch = (lastStepParam != CCadsr_mode);
  if (firstTouch) lastStepParam = CCadsr_mode;


  if (announce && !suppressParamAnnounce) {
    if (firstTouch || changed) {
      displayMode = 0;
      showCurrentTonePage("D5 ENV2 KEY",
                          env5stageModeStepToLabel(upperData[P_adsr_mode]),
                          env5stageModeStepToLabel(lowerData[P_adsr_mode]));
      startParameterDisplay();
    }
  }

  if (changed || recallPatchFlag) {
    if (upperSW) {
      sendVoiceParam(kBoardUpperPrefix, kBoardOffset2Prefix, 0xAD, env5stageModeStepToValue(newStep));
      sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x59, env5stageModeStepToValue(newStep));
    } else {
      sendVoiceParam(kBoardLowerPrefix, kBoardOffset2Prefix, 0xAD, env5stageModeStepToValue(newStep));
      sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x59, env5stageModeStepToValue(newStep));
    }
    lastStep = newStep;
  }
}

FLASHMEM void updateenv4_attack(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("E1 ENV4 ATT", P_env4_attack);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset3Prefix, 0xA6, (uint8_t)upperData[P_env4_attack]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x5A, (uint8_t)upperData[P_env4_attack]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset3Prefix, 0xA6, (uint8_t)lowerData[P_env4_attack]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x5A, (uint8_t)lowerData[P_env4_attack]);
  }
}

FLASHMEM void updateenv4_decay(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("E2 ENV4 DECY", P_env4_decay);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset3Prefix, 0xAA, (uint8_t)upperData[P_env4_decay]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x5B, (uint8_t)upperData[P_env4_decay]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset3Prefix, 0xAA, (uint8_t)lowerData[P_env4_decay]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x5B, (uint8_t)lowerData[P_env4_decay]);
  }
}

FLASHMEM void updateenv4_sustain(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("E3 ENV4 SUST", P_env4_sustain);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset3Prefix, 0xAB, (uint8_t)upperData[P_env4_sustain]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x5C, (uint8_t)upperData[P_env4_sustain]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset3Prefix, 0xAB, (uint8_t)lowerData[P_env4_sustain]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x5C, (uint8_t)lowerData[P_env4_sustain]);
  }
}

FLASHMEM void updateenv4_release(bool announce) {
  lastStepParam = 0xFF;
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    flashToneScale("E4 ENV3 REL", P_env4_release);
    startParameterDisplay();
  }

  if (upperSW) {
    sendVoiceParam(kBoardUpperPrefix, kBoardOffset3Prefix, 0xAC, (uint8_t)upperData[P_env4_release]);
    sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x5D, (uint8_t)upperData[P_env4_release]);
  } else {
    sendVoiceParam(kBoardLowerPrefix, kBoardOffset3Prefix, 0xAC, (uint8_t)lowerData[P_env4_release]);
    sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x5D, (uint8_t)lowerData[P_env4_release]);
  }
}

FLASHMEM void updateenv4_adsr_mode(bool announce) {
  const uint8_t newStep = upperSW ? upperData[P_env4_adsr_mode] : lowerData[P_env4_adsr_mode];

  static uint8_t lastStep = 0xFF;
  const bool changed = (newStep != lastStep);
  const bool firstTouch = (lastStepParam != CC4adsr_mode);
  if (firstTouch) lastStepParam = CC4adsr_mode;


  if (announce && !suppressParamAnnounce) {
    if (firstTouch || changed) {
      displayMode = 0;
      showCurrentTonePage("E5 ENV2 KEY",
                          env5stageModeStepToLabel(upperData[P_env4_adsr_mode]),
                          env5stageModeStepToLabel(lowerData[P_env4_adsr_mode]));
      startParameterDisplay();
    }
  }

  if (changed || recallPatchFlag) {
    if (upperSW) {
      sendVoiceParam(kBoardUpperPrefix, kBoardOffset3Prefix, 0xAD, env5stageModeStepToValue(newStep));
      sendToneSysEx(kBoardUpperMIDIPrefix, (upperChannel - 1), 0x5E, env5stageModeStepToValue(newStep));
    } else {
      sendVoiceParam(kBoardLowerPrefix, kBoardOffset3Prefix, 0xAD, env5stageModeStepToValue(newStep));
      sendToneSysEx(kBoardLowerMIDIPrefix, (lowerChannel - 1), 0x5E, env5stageModeStepToValue(newStep));
    }
    lastStep = newStep;
  }
}

static inline uint8_t map_u8_round(uint8_t x, uint8_t in_min, uint8_t in_max, uint8_t out_min, uint8_t out_max) {
  if (in_max == in_min) return out_min;
  const uint16_t num = static_cast<uint16_t>(x - in_min) * static_cast<uint16_t>(out_max - out_min);
  const uint16_t den = static_cast<uint16_t>(in_max - in_min);
  const uint16_t y = static_cast<uint16_t>(out_min) + (num + den / 2) / den;
  return clamp_u8(y, out_min, out_max);
}

static inline int16_t map_i16_round(uint8_t x, uint8_t in_min, uint8_t in_max, int16_t out_min, int16_t out_max) {
  if (in_max == in_min) return out_min;
  const int32_t num = static_cast<int32_t>(x - in_min) * static_cast<int32_t>(out_max - out_min);
  const int32_t den = static_cast<int32_t>(in_max - in_min);
  const int32_t y = static_cast<int32_t>(out_min) + (num + den / 2) / den;
  if (y < out_min) return out_min;
  if (y > out_max) return out_max;
  return static_cast<int16_t>(y);
}

// Slider 0..127 -> raw 0x00..0x6B, but at slider 63 raw must be 0x2C.
static uint8_t dualDetuneSliderToRaw(uint8_t slider) {
  slider = clamp_u8(slider, 0, 127);

  if (slider <= 63) {
    // 0..63 => 0x00..0x2C (inclusive, hit 0x2C at 63)
    return map_u8_round(slider, 0, 63, 0x00, kDetuneZeroPos);
  }

  // 64..127 => 0x2C..0x6B (inclusive)
  return map_u8_round(slider, 64, 127, kDetuneZeroPos, kDetuneMax);
}

// Raw 0x00..0x6B -> display value -50..+50,
// with distinct "-00" at 0x2B and "00" at 0x2C.
static String dualDetuneRawToDisplayString(uint8_t raw) {
  raw = clamp_u8(raw, 0x00, kDetuneMax);

  if (raw == kDetuneNegZero) return String("-00");
  if (raw == kDetuneZeroPos) return String("00");

  if (raw < kDetuneZeroPos) {
    // 0x00..0x2B => -50..-0
    const int16_t v = map_i16_round(raw, 0x00, kDetuneNegZero, -50, 0);
    // format "-02", "-01", "-00" handled above
    char buf[5];
    snprintf(buf, sizeof(buf), "%+03d", (int)v);  // yields "-02", "-01", "+00" etc
    // force minus sign style without '+'
    if (v == 0) return String("-00");
    if (buf[0] == '+') memmove(buf, buf + 1, strlen(buf));  // drop '+'
    return String(buf);
  } else {
    // 0x2D..0x6B => +1..+50 (0x2C handled above as "00")
    const int16_t v = map_i16_round(raw, kDetuneZeroPos, kDetuneMax, 0, 50);
    if (v == 0) return String("00");
    char buf[5];
    snprintf(buf, sizeof(buf), "%+03d", (int)v);  // "+01", "+02", ...
    return String(buf);
  }
}

// Example update routine
FLASHMEM void updatedualdetune(bool announce) {
  lastStepParam = 0xFF;

  const uint8_t raw = dualDetuneSliderToRaw(static_cast<uint8_t>(dualdetune));
  const String disp = dualDetuneRawToDisplayString(raw);

  if (announce && !suppressParamAnnounce) {
    displayMode = 1;
    showCurrentParameterPage("12 DUAL DETUNE", disp);
    startParameterDisplay();
  }

  if (keyMode == 0 || keyMode == 4 || keyMode == 5) {
    // Calculate detune send values without touching lowerMT1/MT2
    lowerSend = calcLowerDetune(masterTune, dualdetune);

    sendVoiceParam(kBoardLowerPrefix, NO_OFFSET, kDualDetuneParam, lowerSend);
    sendVoiceParam(kBoardLowerPrefix, NO_OFFSET, kDualDetuneParam2, lowerSend);

    sendVoiceParam(kBoardUpperPrefix, NO_OFFSET, kDualDetuneParam, upperMT1);
    sendVoiceParam(kBoardUpperPrefix, NO_OFFSET, kDualDetuneParam2, upperMT2);

  } else {
    // Single or Split — all boards get master tune, detune ignored
    sendVoiceParam(kBoardLowerPrefix, NO_OFFSET, kDualDetuneParam, lowerMT1);
    sendVoiceParam(kBoardLowerPrefix, NO_OFFSET, kDualDetuneParam2, lowerMT2);

    sendVoiceParam(kBoardUpperPrefix, NO_OFFSET, kDualDetuneParam, upperMT1);
    sendVoiceParam(kBoardUpperPrefix, NO_OFFSET, kDualDetuneParam2, upperMT2);
  }
}

// FLASHMEM void updateunisondetune(bool announce) {
//   unisondetune_str = map(unisondetune, 0, 127, -50, 50);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 1;
//     showCurrentParameterPage("UNISON DETUNE", String(unisondetune_str));
//     startParameterDisplay();
//   }
//   sendCustomSysEx((midiOutCh - 1), 0x13, unisondetune);
// }

// // Buttons

FLASHMEM void updateupperunisonDetune(bool announce) {
  sendCustomSysEx((upperChannel - 1), 0x20, upperUnisonDetune);
}

FLASHMEM void updatelowerunisonDetune(bool announce) {
  sendCustomSysEx((lowerChannel - 1), 0x29, lowerUnisonDetune);
}

// Encode a signed semitone shift (-24..+24) to the MKS-70 byte value.
// 0..+24  -> 0x00..0x18
// -24..-1 -> 0x68..0x7F   (i.e. 0x80 + shift)
static inline uint8_t encodeChromaticShift(int8_t shift) {
  if (shift < -24) shift = -24;
  if (shift > 24) shift = 24;
  return (shift >= 0) ? (uint8_t)shift : (uint8_t)(0x80 + shift);
}

FLASHMEM void updateoctave_send(bool announce) {

  // Pick which side we're updating based on the upper/lower selector
  int8_t shift = upperSW ? upperChromaticSW : lowerChromaticSW;

  if (announce && !suppressParamAnnounce) {
    displayMode = 1;

    // Build "-24 Semitones", "0 Semitones", "+7 Semitones", etc.
    String label;
    if (shift == 0) {
      label = "00";
    } else {
      label = String(shift > 0 ? "+" : "") + String(shift);
    }

    showCurrentParameterPage(upperSW ? "32 UP CHROMATIC SHIFT" : "42 LO CHROMATIC SHIFT", label);
    startParameterDisplay();

    upperChromatic = encodeChromaticShift(upperChromaticSW);
    lowerChromatic = encodeChromaticShift(lowerChromaticSW);
  }

  if (!switchLEDS) {
    switch (keyMode) {
      case 0:
      case 3:
      case 4:
      case 5:
        if (upperSW) {
          sendCustomSysEx((upperChannel - 1), 0x1E, upperChromatic);
        } else {
          sendCustomSysEx((lowerChannel - 1), 0x27, lowerChromatic);
        }
        break;

      case 1:
        sendCustomSysEx((upperChannel - 1), 0x1E, upperChromatic);
        break;

      case 2:
        sendCustomSysEx((lowerChannel - 1), 0x27, lowerChromatic);
        break;
    }
  }

  if (upperSW) {
    switch (upperChromatic) {

      case 0:
        mcp7.digitalWrite(OCTAVE_UP_RED, LOW);
        mcp7.digitalWrite(OCTAVE_UP_GREEN, LOW);
        mcp7.digitalWrite(OCTAVE_DOWN_RED, LOW);
        mcp7.digitalWrite(OCTAVE_DOWN_GREEN, LOW);
        break;

      case 1 ... 12:
        mcp7.digitalWrite(OCTAVE_UP_RED, HIGH);
        mcp7.digitalWrite(OCTAVE_UP_GREEN, LOW);
        mcp7.digitalWrite(OCTAVE_DOWN_RED, LOW);
        mcp7.digitalWrite(OCTAVE_DOWN_GREEN, LOW);
        break;

      case 13 ... 24:
        mcp7.digitalWrite(OCTAVE_UP_RED, LOW);
        mcp7.digitalWrite(OCTAVE_UP_GREEN, HIGH);
        mcp7.digitalWrite(OCTAVE_DOWN_RED, LOW);
        mcp7.digitalWrite(OCTAVE_DOWN_GREEN, LOW);
        break;

      case 116 ... 127:
        mcp7.digitalWrite(OCTAVE_UP_RED, LOW);
        mcp7.digitalWrite(OCTAVE_UP_GREEN, LOW);
        mcp7.digitalWrite(OCTAVE_DOWN_RED, LOW);
        mcp7.digitalWrite(OCTAVE_DOWN_GREEN, HIGH);
        break;

      case 103 ... 115:
        mcp7.digitalWrite(OCTAVE_UP_RED, LOW);
        mcp7.digitalWrite(OCTAVE_UP_GREEN, LOW);
        mcp7.digitalWrite(OCTAVE_DOWN_RED, HIGH);
        mcp7.digitalWrite(OCTAVE_DOWN_GREEN, LOW);
        break;
    }
  } else {
    switch (lowerChromatic) {

      case 0:
        mcp7.digitalWrite(OCTAVE_UP_RED, LOW);
        mcp7.digitalWrite(OCTAVE_UP_GREEN, LOW);
        mcp7.digitalWrite(OCTAVE_DOWN_RED, LOW);
        mcp7.digitalWrite(OCTAVE_DOWN_GREEN, LOW);
        break;

      case 1 ... 12:
        mcp7.digitalWrite(OCTAVE_UP_RED, HIGH);
        mcp7.digitalWrite(OCTAVE_UP_GREEN, LOW);
        mcp7.digitalWrite(OCTAVE_DOWN_RED, LOW);
        mcp7.digitalWrite(OCTAVE_DOWN_GREEN, LOW);
        break;

      case 13 ... 24:
        mcp7.digitalWrite(OCTAVE_UP_RED, LOW);
        mcp7.digitalWrite(OCTAVE_UP_GREEN, HIGH);
        mcp7.digitalWrite(OCTAVE_DOWN_RED, LOW);
        mcp7.digitalWrite(OCTAVE_DOWN_GREEN, LOW);
        break;

      case 116 ... 127:
        mcp7.digitalWrite(OCTAVE_UP_RED, LOW);
        mcp7.digitalWrite(OCTAVE_UP_GREEN, LOW);
        mcp7.digitalWrite(OCTAVE_DOWN_RED, LOW);
        mcp7.digitalWrite(OCTAVE_DOWN_GREEN, HIGH);
        break;

      case 103 ... 115:
        mcp7.digitalWrite(OCTAVE_UP_RED, LOW);
        mcp7.digitalWrite(OCTAVE_UP_GREEN, LOW);
        mcp7.digitalWrite(OCTAVE_DOWN_RED, HIGH);
        mcp7.digitalWrite(OCTAVE_DOWN_GREEN, LOW);
        break;
    }
  }
}

// Keymode buttons

FLASHMEM void updateassignMode(bool announce) {
}

FLASHMEM void updatekeyMode(bool announce) {


  switch (keyMode) {
    case 0:
      updatedual_button(0);
      break;

    case 3:
      updatesplit_button(0);
      break;

    case 1:
    case 2:
      updatesingle_button(0);
      break;

    case 4:
    case 5:
      updatespecial_button(0);
      break;
  }
}

FLASHMEM void updatedual_button(bool announce) {
  if (announce && !suppressParamAnnounce) {
    displayMode = 1;
    //showCurrentParameterPage("17 KEY  MODE", "DUAL");
    //startParameterDisplay();
  }
  mcp8.digitalWrite(KEY_DUAL_RED, HIGH);
  mcp8.digitalWrite(KEY_SPLIT_RED, LOW);
  mcp8.digitalWrite(KEY_SPECIAL_RED, LOW);
  mcp8.digitalWrite(KEY_SPECIAL_GREEN, LOW);
  mcp10.digitalWrite(KEY_SINGLE_RED, LOW);
  mcp10.digitalWrite(KEY_SINGLE_GREEN, LOW);
  if (upperSW) {
    mcp10.digitalWrite(UPPER_SELECT, HIGH);
    mcp10.digitalWrite(LOWER_SELECT, LOW);
  } else {
    mcp10.digitalWrite(LOWER_SELECT, HIGH);
    mcp10.digitalWrite(UPPER_SELECT, LOW);
  }

  allNotesOff();
  sendCustomSysEx((controlChannel - 1), 0x18, 0x00);
  sendCustomSysEx((controlChannel - 1), 0x33, 0x00);
  Serial3.write(kBoardBothPrefix);
  sendKeyModeStartInit();
  if (keyModeUpper) {
    Serial3.write(kBoardLowerPrefix);
    oldupperSW = upperSW;
    upperSW = false;
    updateLowerToneData();
    upperSW = oldupperSW;
  }
  if (keyModeLower) {
    Serial3.write(kBoardUpperPrefix);
    oldupperSW = upperSW;
    upperSW = true;
    updateUpperToneData();
    upperSW = oldupperSW;
  }
  sendKeyModeEndInit();
  keyModeUpper = false;
  keyModeLower = false;
  updateAssignLeds();
  sendAssignSysEx();
  renderCurrentPatchPage();
}

FLASHMEM void updatesplit_button(bool announce) {
  if (announce && !suppressParamAnnounce) {
    displayMode = 1;
    //showCurrentParameterPage("KEY MODE", "SPLIT");
    //startParameterDisplay();
  }
  mcp8.digitalWrite(KEY_DUAL_RED, LOW);
  mcp8.digitalWrite(KEY_SPLIT_RED, HIGH);
  mcp8.digitalWrite(KEY_SPECIAL_RED, LOW);
  mcp8.digitalWrite(KEY_SPECIAL_GREEN, LOW);
  mcp10.digitalWrite(KEY_SINGLE_RED, LOW);
  mcp10.digitalWrite(KEY_SINGLE_GREEN, LOW);
  if (upperSW) {
    mcp10.digitalWrite(UPPER_SELECT, HIGH);
    mcp10.digitalWrite(LOWER_SELECT, LOW);
  } else {
    mcp10.digitalWrite(LOWER_SELECT, HIGH);
    mcp10.digitalWrite(UPPER_SELECT, LOW);
  }
  allNotesOff();
  sendCustomSysEx((controlChannel - 1), 0x18, 0x01);
  sendCustomSysEx((controlChannel - 1), 0x33, 0x00);
  sendKeyModeStartInit();
  if (keyModeUpper) {
    Serial3.write(kBoardLowerPrefix);
    oldupperSW = upperSW;
    upperSW = false;
    updateLowerToneData();
    upperSW = oldupperSW;
  }
  if (keyModeLower) {
    Serial3.write(kBoardUpperPrefix);
    oldupperSW = upperSW;
    upperSW = true;
    updateUpperToneData();
    upperSW = oldupperSW;
  }
  sendKeyModeEndInit();
  keyModeUpper = false;
  keyModeLower = false;
  updateAssignLeds();
  sendAssignSysEx();
  renderCurrentPatchPage();
}

FLASHMEM void updatesingle_button(bool announce) {
  if (announce && !suppressParamAnnounce) {
    displayMode = 1;
    if (!single_button) {
      //showCurrentParameterPage("KEY MODE", "LO WHOLE");
    } else {
      //showCurrentParameterPage("KEY MODE", "UP WHOLE");
    }
    //startParameterDisplay();
  }
  if (keyMode == 1) {
    mcp10.digitalWrite(KEY_SINGLE_RED, LOW);
    mcp10.digitalWrite(KEY_SINGLE_GREEN, HIGH);
    mcp10.digitalWrite(LOWER_SELECT, LOW);
    mcp10.digitalWrite(UPPER_SELECT, HIGH);
    Serial3.write(kBoardBothPrefix);
    sendKeyModeStartInit();
    updateUpperToneData();
    keyModeUpper = true;
    keyModeLower = false;
    upperSW = true;
    updateAssignLeds();
    sendAssignSysEx();
    renderCurrentPatchPage();
  } else {
    mcp10.digitalWrite(KEY_SINGLE_RED, HIGH);
    mcp10.digitalWrite(KEY_SINGLE_GREEN, LOW);
    mcp10.digitalWrite(LOWER_SELECT, HIGH);
    mcp10.digitalWrite(UPPER_SELECT, LOW);
    Serial3.write(kBoardBothPrefix);
    sendKeyModeStartInit();
    updateLowerToneData();
    keyModeUpper = false;
    keyModeLower = true;
    upperSW = false;
    updateAssignLeds();
    sendAssignSysEx();
    renderCurrentPatchPage();
  }
  sendKeyModeEndInit();
  mcp8.digitalWrite(KEY_DUAL_RED, LOW);
  mcp8.digitalWrite(KEY_SPLIT_RED, LOW);
  mcp8.digitalWrite(KEY_SPECIAL_RED, LOW);
  mcp8.digitalWrite(KEY_SPECIAL_GREEN, LOW);

  allNotesOff();
  if (keyMode == 1) {
    sendCustomSysEx((controlChannel - 1), 0x18, 0x02);
    sendCustomSysEx((controlChannel - 1), 0x33, 0x00);
  } else {
    sendCustomSysEx((controlChannel - 1), 0x18, 0x03);
    sendCustomSysEx((controlChannel - 1), 0x33, 0x00);
  }
}

FLASHMEM void updatespecial_button(bool announce) {
  if (announce && !suppressParamAnnounce) {
    displayMode = 1;
    if (!special_button) {
      //showCurrentParameterPage("KEY MODE", "X-FADE");
    } else {
      //showCurrentParameterPage("KEY MODE", "T-VOICE");
    }
    //startParameterDisplay();
  }
  if (keyMode == 5) {
    mcp8.digitalWrite(KEY_SPECIAL_RED, LOW);
    mcp8.digitalWrite(KEY_SPECIAL_GREEN, HIGH);
  } else {
    mcp8.digitalWrite(KEY_SPECIAL_RED, HIGH);
    mcp8.digitalWrite(KEY_SPECIAL_GREEN, LOW);
  }
  mcp8.digitalWrite(KEY_DUAL_RED, LOW);
  mcp8.digitalWrite(KEY_SPLIT_RED, LOW);
  mcp10.digitalWrite(KEY_SINGLE_RED, LOW);
  mcp10.digitalWrite(KEY_SINGLE_GREEN, LOW);
  if (upperSW) {
    mcp10.digitalWrite(UPPER_SELECT, HIGH);
    mcp10.digitalWrite(LOWER_SELECT, LOW);
  } else {
    mcp10.digitalWrite(LOWER_SELECT, HIGH);
    mcp10.digitalWrite(UPPER_SELECT, LOW);
  }

  allNotesOff();
  if (keyMode == 5) {
    sendCustomSysEx((controlChannel - 1), 0x18, 0x00);
    sendCustomSysEx((controlChannel - 1), 0x33, 0x01);
  } else {
    sendCustomSysEx((controlChannel - 1), 0x18, 0x00);
    sendCustomSysEx((controlChannel - 1), 0x33, 0x02);
  }
  sendKeyModeStartInit();
  if (keyModeUpper) {
    Serial3.write(kBoardLowerPrefix);
    oldupperSW = upperSW;
    upperSW = false;
    updateLowerToneData();
    upperSW = oldupperSW;
  }
  if (keyModeLower) {
    Serial3.write(kBoardUpperPrefix);
    oldupperSW = upperSW;
    upperSW = true;
    updateUpperToneData();
    upperSW = oldupperSW;
  }
  sendKeyModeEndInit();
  keyModeUpper = false;
  keyModeLower = false;
  updateAssignLeds();
  sendAssignSysEx();
  renderCurrentPatchPage();
}

// Assigner buttons

FLASHMEM void updateAssignLeds() {
  int active = upperSW ? upperAssign : lowerAssign;
  int cat = assignCat(active);
  int variant = assignVariant(active);

  // Clear all six LEDs
  mcp8.digitalWrite(ASSIGN_POLY_RED, LOW);
  mcp8.digitalWrite(ASSIGN_POLY_GREEN, LOW);
  mcp8.digitalWrite(ASSIGN_UNI_RED, LOW);
  mcp8.digitalWrite(ASSIGN_UNI_GREEN, LOW);
  mcp8.digitalWrite(ASSIGN_MONO_RED, LOW);
  mcp8.digitalWrite(ASSIGN_MONO_GREEN, LOW);

  // Light active category's LED: RED for variant 1, GREEN for variant 2
  uint8_t redPin, greenPin;
  switch (cat) {
    case CAT_POLY:
      redPin = ASSIGN_POLY_RED;
      greenPin = ASSIGN_POLY_GREEN;
      break;
    case CAT_UNI:
      redPin = ASSIGN_UNI_RED;
      greenPin = ASSIGN_UNI_GREEN;
      break;
    case CAT_MONO:
      redPin = ASSIGN_MONO_RED;
      greenPin = ASSIGN_MONO_GREEN;
      break;
    default: return;
  }
  mcp8.digitalWrite(variant == 0 ? redPin : greenPin, HIGH);
}

static inline uint8_t wireToInternal(uint8_t w) {
  switch (w) {
    case 0: return 0;  // POLY 1
    case 4: return 1;  // POLY 2
    case 2: return 2;  // MONO 1
    case 6: return 3;  // MONO 2
    case 1: return 4;  // UNISON 1
    case 5: return 5;  // UNISON 2
    default: return 0;
  }
}

static inline void refreshRealAssigns() {
  upperRealAssign = wireToInternal((uint8_t)upperAssign);
  lowerRealAssign = wireToInternal((uint8_t)lowerAssign);
}

FLASHMEM void sendAssignSysEx() {
  uint8_t addr = upperSW ? 0x1F : 0x28;
  uint8_t val = upperSW ? (uint8_t)upperAssign : (uint8_t)lowerAssign;
  refreshRealAssigns();  // keep playback-side in sync

  // Serial.print("Pre Upper Assign ");
  // Serial.println(upperRealAssign);
  // Serial.print("Pre Lower Assign ");
  // Serial.println(lowerRealAssign);

  // Serial.print("KEyMODE ");
  // Serial.println(keyMode);

  if (keyMode == 1) {
    updateUpperPatchData();
  }
  if (keyMode == 2) {
    updateLowerPatchData();
  }

  uint8_t channelMIDI = upperSW ? (uint8_t)upperChannel : (uint8_t)lowerChannel;
  sendCustomSysEx((channelMIDI - 1), addr, val);

  // Serial.print("Upper Assign ");
  // Serial.println(upperRealAssign);
  // Serial.print("Lower Assign ");
  // Serial.println(lowerRealAssign);
}

FLASHMEM void announceAssign() {
  if (suppressParamAnnounce) return;
  int active = upperSW ? upperAssign : lowerAssign;
  displayMode = DM_PATCH_FLASH;  // or 1 if enum not added
  showCurrentParameterPage("ASSIGN MODE", assignLabels[active]);
  startParameterDisplay();
}

FLASHMEM void pressAssign(int requestedCat) {
  int &active = upperSW ? upperAssign : lowerAssign;

  if (assignCat(active) == requestedCat) {
    // Same category pressed — toggle variant 1 ↔ 2
    active = assignMake(requestedCat, assignVariant(active) ^ 1);
  } else {
    // Different category — reset to variant 1
    active = assignMake(requestedCat, 0);
  }

  updateAssignLeds();
  sendAssignSysEx();
  announceAssign();
}

FLASHMEM void updatechase(bool announce) {
  // chase level
  sendCustomSysEx((controlChannel - 1), 0x2F, chaseLevel);
  // chase mode
  sendCustomSysEx((controlChannel - 1), 0x30, chaseMode);
  // chase time
  sendCustomSysEx((controlChannel - 1), 0x31, chaseTime);
  // chase play
  sendCustomSysEx((controlChannel - 1), 0x32, chasePlay);
  switch (chasePlay) {
    case 0:
      mcp10.digitalWrite(CHASE_LED_RED, LOW);
      break;

    case 1:
      mcp10.digitalWrite(CHASE_LED_RED, HIGH);
      break;
  }
}

FLASHMEM void updatechaseLevel(bool announce) {
  sendCustomSysEx((controlChannel - 1), 0x2F, chaseLevel);
  sendVCABalance();
}

FLASHMEM void updatechaseMode(bool announce) {
  sendCustomSysEx((controlChannel - 1), 0x30, chaseMode);
}

FLASHMEM void updatechaseTime(bool announce) {
  sendCustomSysEx((controlChannel - 1), 0x31, chaseTime);
}

FLASHMEM void updatechasePlay(bool announce) {
  sendCustomSysEx((controlChannel - 1), 0x32, chasePlay);
  switch (chasePlay) {
    case 0:
      mcp10.digitalWrite(CHASE_LED_RED, LOW);
      break;

    case 1:
      mcp10.digitalWrite(CHASE_LED_RED, HIGH);
      break;
  }
}

FLASHMEM void updateupperHold() {
  switch (holdManualUpper) {
    case 0:
      sendVoiceParam(kBoardUpperPrefix, NO_OFFSET, 0xBB, 0x00);
      sendCustomSysEx((upperChannel - 1), 0x21, 0x00);

      break;

    case 1:
      sendVoiceParam(kBoardUpperPrefix, NO_OFFSET, 0xBB, 0x7F);
      sendCustomSysEx((upperChannel - 1), 0x21, 0x7F);
      break;
  }
}

FLASHMEM void updatelowerHold() {
  switch (holdManualLower) {
    case 0:
      sendVoiceParam(kBoardLowerPrefix, NO_OFFSET, 0xBB, 0x00);
      sendCustomSysEx((lowerChannel - 1), 0x2A, 0x00);
      break;

    case 1:
      sendVoiceParam(kBoardLowerPrefix, NO_OFFSET, 0xBB, 0x7F);
      sendCustomSysEx((lowerChannel - 1), 0x2A, 0x7F);
      break;
  }
}


FLASHMEM void updateupperchromaticshift() {
  sendCustomSysEx((upperChannel - 1), 0x1E, upperChromatic);
}

FLASHMEM void updatelowerchromaticshift() {
  sendCustomSysEx((lowerChannel - 1), 0x27, lowerChromatic);
}

FLASHMEM void updateuppersplitPoints(bool announce) {
  // upper
  sendCustomSysEx((upperChannel - 1), 0x14, upperSplitPoint);
}

FLASHMEM void updatelowersplitPoints(bool announce) {
  // lower
  sendCustomSysEx((lowerChannel - 1), 0x15, lowerSplitPoint);
}

FLASHMEM void updatebend_enable_button(bool announce) {
  if (announce && !suppressParamAnnounce) {
    displayMode = 1;
    switch (bend_enable) {
      case 0:
        showCurrentParameterPage("BEND ENABLE", "OFF");
        break;
      case 1:
        showCurrentParameterPage("BEND ENABLE", "LOWER");
        break;
      case 2:
        showCurrentParameterPage("BEND ENABLE", "UPPER");
        break;
      case 3:
        showCurrentParameterPage("BEND ENABLE", "BOTH");
        break;
    }
    startParameterDisplay();
  }

  if (!lowerbend_enable_SW && !upperbend_enable_SW) {
    mcp7.digitalWrite(BEND_ENABLE_RED, LOW);
    mcp7.digitalWrite(BEND_ENABLE_GREEN, LOW);
    bend_enable = 0;
  }

  if (lowerbend_enable_SW && !upperbend_enable_SW) {
    mcp7.digitalWrite(BEND_ENABLE_RED, HIGH);
    mcp7.digitalWrite(BEND_ENABLE_GREEN, LOW);
    bend_enable = 1;
  }

  if (!lowerbend_enable_SW && upperbend_enable_SW) {
    mcp7.digitalWrite(BEND_ENABLE_RED, LOW);
    mcp7.digitalWrite(BEND_ENABLE_GREEN, HIGH);
    bend_enable = 2;
  }

  if (lowerbend_enable_SW && upperbend_enable_SW) {
    mcp7.digitalWrite(BEND_ENABLE_RED, HIGH);
    mcp7.digitalWrite(BEND_ENABLE_GREEN, HIGH);
    bend_enable = 3;
  }
}

FLASHMEM void updateafter_vib_enable_button(bool announce) {
  if (announce && !suppressParamAnnounce) {
    displayMode = 1;
    switch (at_vib_sw) {
      case 0:
        showCurrentParameterPage("AT VIB", "OFF");
        break;
      case 1:
        at_vib_str = map(at_vib, 0, 127, 0, 99);
        showCurrentParameterPage("AT VIB", at_vib_str);
        break;
    }
    startParameterDisplay();
  }
  switch (at_vib_sw) {
    case 0:
      mcp12.digitalWrite(AFTER_VIB_RED, LOW);
      sendVoiceParam(0xFD, 0x00, 0xB8, 0x00);
      break;
    case 1:
      mcp12.digitalWrite(AFTER_VIB_RED, HIGH);
      sendVoiceParam(0xFD, 0x01, 0xB8, at_vib);
      break;
  }
}

FLASHMEM void updateafter_bri_enable_button(bool announce) {
  if (announce && !suppressParamAnnounce) {
    displayMode = 1;
    switch (at_bri_sw) {
      case 0:
        showCurrentParameterPage("AT BRI", "OFF");
        break;
      case 1:
        at_bri_str = map(at_bri, 0, 127, 0, 99);
        showCurrentParameterPage("AT BRI", at_bri_str);
        break;
    }
    startParameterDisplay();
  }
  switch (at_bri_sw) {
    case 0:
      mcp12.digitalWrite(AFTER_BRI_RED, LOW);
      sendVoiceParam(0xFD, 0x02, 0xB9, 0x00);
      break;
    case 1:
      mcp12.digitalWrite(AFTER_BRI_RED, HIGH);
      sendVoiceParam(0xFD, 0x03, 0xB9, (uint8_t)at_bri);
      break;
  }
}

FLASHMEM void updateafter_vol_enable_button(bool announce) {
  if (announce && !suppressParamAnnounce) {
    displayMode = 1;
    switch (at_vol_sw) {
      case 0:
        showCurrentParameterPage("AT VOL", "OFF");
        break;
      case 1:
        at_vol_str = map(at_vol, 0, 127, 0, 99);
        showCurrentParameterPage("AT VOL", at_vol_str);
        break;
    }
    startParameterDisplay();
  }
  switch (at_vol_sw) {
    case 0:
      mcp12.digitalWrite(AFTER_VOL_RED, LOW);
      sendVoiceParam(0xFD, 0x04, 0xB9, 0x00);
      break;
    case 1:
      mcp12.digitalWrite(AFTER_VOL_RED, HIGH);
      sendVoiceParam(0xFD, 0x05, 0xB9, (uint8_t)at_vol);
      break;
  }
}

FLASHMEM void updatelfo1_sync(bool announce) {
  lastStepParam = 0xFF;
  const uint8_t stored = upperSW ? upperData[P_lfo1_sync] : lowerData[P_lfo1_sync];

  // --- Display ---
  if (announce && !suppressParamAnnounce) {
    displayMode = DM_TONE_FLASH;
    flashToneStep("95 LFO1 SYNC", P_lfo1_sync, toneSyncValues, 0x20, 2);
    startParameterDisplay();
  }

  // --- SYX send ---
  if (!switchLEDS) {
    uint8_t prefix = upperSW ? kBoardUpperPrefix : kBoardLowerPrefix;
    uint8_t prefixMIDI = upperSW ? kBoardUpperMIDIPrefix : kBoardLowerMIDIPrefix;
    uint8_t channelMIDI = upperSW ? upperChannel : lowerChannel;
    sendVoiceParam(prefix, kBoardOffset0Prefix, 0xA5, stored);
    sendToneSysEx(prefixMIDI, (channelMIDI - 1), 0x3F, stored);
  }

  // --- LED update ---
  bool red = (stored == 0x20);    // ON  -> red
  bool green = (stored == 0x40);  // KEY -> green
  mcp1.digitalWrite(LFO1_SYNC_RED, red ? HIGH : LOW);
  mcp1.digitalWrite(LFO1_SYNC_GREEN, green ? HIGH : LOW);
}

FLASHMEM void updatelfo2_sync(bool announce) {
  lastStepParam = 0xFF;
  const uint8_t stored = upperSW ? upperData[P_lfo2_sync] : lowerData[P_lfo2_sync];

  // --- Display ---
  if (announce && !suppressParamAnnounce) {
    displayMode = DM_TONE_FLASH;
    flashToneStep("A5 LFO2 SYNC", P_lfo2_sync, toneSyncValues, 0x20, 2);
    startParameterDisplay();
  }

  // --- SYX send ---
  if (!switchLEDS) {
    uint8_t prefix = upperSW ? kBoardUpperPrefix : kBoardLowerPrefix;
    uint8_t prefixMIDI = upperSW ? kBoardUpperMIDIPrefix : kBoardLowerMIDIPrefix;
    uint8_t channelMIDI = upperSW ? upperChannel : lowerChannel;
    sendVoiceParam(prefix, kBoardOffset1Prefix, 0xA5, stored);
    sendToneSysEx(prefixMIDI, (channelMIDI - 1), 0x44, stored);
  }

  // --- LED update ---
  bool red = (stored == 0x20);    // ON  -> red
  bool green = (stored == 0x40);  // KEY -> green
  mcp2.digitalWrite(LFO2_SYNC_RED, red ? HIGH : LOW);
  mcp2.digitalWrite(LFO2_SYNC_GREEN, green ? HIGH : LOW);
}

FLASHMEM void updatedco1_PWM_dyn(bool announce) {
  lastStepParam = 0xFF;
  const uint8_t stored = upperSW ? upperData[P_dco1_PWM_dyn] : lowerData[P_dco1_PWM_dyn];

  // --- Display ---
  if (announce && !suppressParamAnnounce) {
    displayMode = DM_TONE_FLASH;
    flashToneStep("45 PWM1 DYNA", P_dco1_PWM_dyn, toneDynValues, 0x20, 3);
    startParameterDisplay();
  }

  // --- SYX send ---
  if (!switchLEDS) {
    uint8_t prefix = upperSW ? kBoardUpperPrefix : kBoardLowerPrefix;
    uint8_t prefixMIDI = upperSW ? kBoardUpperMIDIPrefix : kBoardLowerMIDIPrefix;
    uint8_t channelMIDI = upperSW ? upperChannel : lowerChannel;
    sendVoiceParam(prefix, kBoardOffset0Prefix, 0x8E, stored);
    sendToneSysEx(prefixMIDI, (channelMIDI - 1), 0x23, stored);
  }
  // --- LED update (binary-coded dual LED) ---
  int state = unpackStep(stored, 0x20, 3);  // 0/1/2/3
  mcp1.digitalWrite(DCO1_PWM_DYN_RED, (state & 1) ? HIGH : LOW);
  mcp1.digitalWrite(DCO1_PWM_DYN_GREEN, (state & 2) ? HIGH : LOW);
}

FLASHMEM void updatedco2_PWM_dyn(bool announce) {
  lastStepParam = 0xFF;
  const uint8_t stored = upperSW ? upperData[P_dco2_PWM_dyn] : lowerData[P_dco2_PWM_dyn];

  // --- Display ---
  if (announce && !suppressParamAnnounce) {
    displayMode = DM_TONE_FLASH;
    flashToneStep("55 PWM2 DYNA", P_dco2_PWM_dyn, toneDynValues, 0x20, 3);
    startParameterDisplay();
  }

  // --- SYX send ---
  if (!switchLEDS) {
    uint8_t prefix = upperSW ? kBoardUpperPrefix : kBoardLowerPrefix;
    uint8_t prefixMIDI = upperSW ? kBoardUpperMIDIPrefix : kBoardLowerMIDIPrefix;
    uint8_t channelMIDI = upperSW ? upperChannel : lowerChannel;
    sendVoiceParam(prefix, kBoardOffset1Prefix, 0x8E, stored);
    sendToneSysEx(prefixMIDI, (channelMIDI - 1), 0x29, stored);
  }

  // --- LED update (binary-coded dual LED) ---
  int state = unpackStep(stored, 0x20, 3);  // 0/1/2/3
  mcp2.digitalWrite(DCO1_PWM_DYN_RED, (state & 1) ? HIGH : LOW);
  mcp2.digitalWrite(DCO1_PWM_DYN_GREEN, (state & 2) ? HIGH : LOW);
}

FLASHMEM void updatedco1_PWM_env_source(bool announce) {
  lastStepParam = 0xFF;
  const uint8_t stored = upperSW ? upperData[P_dco1_PWM_env_source] : lowerData[P_dco1_PWM_env_source];

  // --- Display ---
  if (announce && !suppressParamAnnounce) {
    displayMode = DM_TONE_FLASH;
    flashToneStep("46 PWM1 MODE", P_dco1_PWM_env_source, toneEnvValues, 0x10, 7);
    startParameterDisplay();
  }

  // --- SYX send ---
  if (!switchLEDS) {
    uint8_t prefix = upperSW ? kBoardUpperPrefix : kBoardLowerPrefix;
    uint8_t prefixMIDI = upperSW ? kBoardUpperMIDIPrefix : kBoardLowerMIDIPrefix;
    uint8_t channelMIDI = upperSW ? upperChannel : lowerChannel;
    sendVoiceParam(prefix, kBoardOffset0Prefix, 0x8F, stored);
    sendToneSysEx(prefixMIDI, (channelMIDI - 1), 0x24, stored);
  }

  // --- LED update ---
  int state = unpackStep(stored, 0x10, 7);  // 0..7
  int env = (state >> 1) & 3;               // 0..3 (ENV1..ENV4)
  bool pos = state & 1;                     // 1 = positive polarity

  // SOURCE: binary-coded (env 0..3 -> bits on red/green)
  mcp1.digitalWrite(DCO1_PWM_ENV_SOURCE_RED, (env & 1) ? HIGH : LOW);
  mcp1.digitalWrite(DCO1_PWM_ENV_SOURCE_GREEN, (env & 2) ? HIGH : LOW);

  // POLARITY: exactly one on at a time
  mcp1.digitalWrite(DCO1_ENV_POL_RED, pos ? LOW : HIGH);
  mcp1.digitalWrite(DCO1_ENV_POL_GREEN, pos ? HIGH : LOW);
}

FLASHMEM void updatedco2_PWM_env_source(bool announce) {
  lastStepParam = 0xFF;
  const uint8_t stored = upperSW ? upperData[P_dco2_PWM_env_source] : lowerData[P_dco2_PWM_env_source];

  // --- Display ---
  if (announce && !suppressParamAnnounce) {
    displayMode = DM_TONE_FLASH;
    flashToneStep("56 PWM2 MODE", P_dco2_PWM_env_source, toneEnvValues, 0x10, 7);
    startParameterDisplay();
  }

  // --- SYX send ---
  if (!switchLEDS) {
    uint8_t prefix = upperSW ? kBoardUpperPrefix : kBoardLowerPrefix;
    uint8_t prefixMIDI = upperSW ? kBoardUpperMIDIPrefix : kBoardLowerMIDIPrefix;
    uint8_t channelMIDI = upperSW ? upperChannel : lowerChannel;
    sendVoiceParam(prefix, kBoardOffset1Prefix, 0x8F, stored);
    sendToneSysEx(prefixMIDI, (channelMIDI - 1), 0x2A, stored);
  }

  // --- LED update ---
  int state = unpackStep(stored, 0x10, 7);  // 0..7
  int env = (state >> 1) & 3;               // 0..3 (ENV1..ENV4)
  bool pos = state & 1;                     // 1 = positive polarity

  // SOURCE: binary-coded (env 0..3 -> bits on red/green)
  mcp2.digitalWrite(DCO2_PWM_ENV_SOURCE_RED, (env & 1) ? HIGH : LOW);
  mcp2.digitalWrite(DCO2_PWM_ENV_SOURCE_GREEN, (env & 2) ? HIGH : LOW);

  // POLARITY: exactly one on at a time
  mcp2.digitalWrite(DCO2_ENV_POL_RED, pos ? LOW : HIGH);
  mcp2.digitalWrite(DCO2_ENV_POL_GREEN, pos ? HIGH : LOW);
}

FLASHMEM void updatedco1_PWM_lfo_source(bool announce) {
  lastStepParam = 0xFF;
  const uint8_t stored = upperSW ? upperData[P_dco1_PWM_lfo_source] : lowerData[P_dco1_PWM_lfo_source];

  // --- Display ---
  if (announce && !suppressParamAnnounce) {
    displayMode = DM_TONE_FLASH;
    flashToneStep("44 PWM1 LFO", P_dco1_PWM_lfo_source, toneDcoLFOValues, 0x20, 3);
    startParameterDisplay();
  }

  // --- SYX send ---
  if (!switchLEDS) {
    uint8_t prefix = upperSW ? kBoardUpperPrefix : kBoardLowerPrefix;
    uint8_t prefixMIDI = upperSW ? kBoardUpperMIDIPrefix : kBoardLowerMIDIPrefix;
    uint8_t channelMIDI = upperSW ? upperChannel : lowerChannel;
    sendVoiceParam(prefix, kBoardOffset0Prefix, 0x8D, stored);
    sendToneSysEx(prefixMIDI, (channelMIDI - 1), 0x22, stored);
  }

  // --- LED update (binary-coded dual LED) ---
  int state = unpackStep(stored, 0x20, 3);  // 0/1/2/3
  mcp4.digitalWrite(DCO1_PWM_LFO_SEL_RED, (state & 1) ? HIGH : LOW);
  mcp4.digitalWrite(DCO1_PWM_LFO_SEL_GREEN, (state & 2) ? HIGH : LOW);
}

FLASHMEM void updatedco2_PWM_lfo_source(bool announce) {
  lastStepParam = 0xFF;
  const uint8_t stored = upperSW ? upperData[P_dco2_PWM_lfo_source] : lowerData[P_dco2_PWM_lfo_source];

  // --- Display ---
  if (announce && !suppressParamAnnounce) {
    displayMode = DM_TONE_FLASH;
    flashToneStep("54 PWM2 LFO", P_dco2_PWM_lfo_source, toneDcoLFOValues, 0x20, 3);
    startParameterDisplay();
  }

  // --- SYX send ---
  if (!switchLEDS) {
    uint8_t prefix = upperSW ? kBoardUpperPrefix : kBoardLowerPrefix;
    uint8_t prefixMIDI = upperSW ? kBoardUpperMIDIPrefix : kBoardLowerMIDIPrefix;
    uint8_t channelMIDI = upperSW ? upperChannel : lowerChannel;
    sendVoiceParam(prefix, kBoardOffset1Prefix, 0x8D, stored);
    sendToneSysEx(prefixMIDI, (channelMIDI - 1), 0x28, stored);
  }

  // --- LED update (binary-coded dual LED) ---
  int state = unpackStep(stored, 0x20, 3);  // 0/1/2/3
  mcp3.digitalWrite(DCO2_PWM_LFO_SEL_RED, (state & 1) ? HIGH : LOW);
  mcp3.digitalWrite(DCO2_PWM_LFO_SEL_GREEN, (state & 2) ? HIGH : LOW);
}

FLASHMEM void updatedco1_pitch_dyn(bool announce) {
  lastStepParam = 0xFF;
  const uint8_t stored = upperSW ? upperData[P_dco1_pitch_dyn] : lowerData[P_dco1_pitch_dyn];

  // --- Display ---
  if (announce && !suppressParamAnnounce) {
    displayMode = DM_TONE_FLASH;
    flashToneStep("17 DCO1 DYNA", P_dco1_pitch_dyn, toneDynValues, 0x20, 3);
    startParameterDisplay();
  }

  // --- SYX send ---
  if (!switchLEDS) {
    uint8_t prefix = upperSW ? kBoardUpperPrefix : kBoardLowerPrefix;
    uint8_t prefixMIDI = upperSW ? kBoardUpperMIDIPrefix : kBoardLowerMIDIPrefix;
    uint8_t channelMIDI = upperSW ? upperChannel : lowerChannel;
    sendVoiceParam(prefix, kBoardOffset0Prefix, 0x86, stored);
    sendToneSysEx(prefixMIDI, (channelMIDI - 1), 0x11, stored);
  }

  // --- LED update (binary-coded dual LED) ---
  int state = unpackStep(stored, 0x20, 3);  // 0/1/2/3
  mcp4.digitalWrite(DCO1_PITCH_DYN_RED, (state & 1) ? HIGH : LOW);
  mcp4.digitalWrite(DCO1_PITCH_DYN_GREEN, (state & 2) ? HIGH : LOW);
}

FLASHMEM void updatedco2_pitch_dyn(bool announce) {
  lastStepParam = 0xFF;
  const uint8_t stored = upperSW ? upperData[P_dco2_pitch_dyn] : lowerData[P_dco2_pitch_dyn];

  // --- Display ---
  if (announce && !suppressParamAnnounce) {
    displayMode = DM_TONE_FLASH;
    flashToneStep("27 DCO2 DYNA", P_dco2_pitch_dyn, toneDynValues, 0x20, 3);
    startParameterDisplay();
  }

  // --- SYX send ---
  if (!switchLEDS) {
    uint8_t prefix = upperSW ? kBoardUpperPrefix : kBoardLowerPrefix;
    uint8_t prefixMIDI = upperSW ? kBoardUpperMIDIPrefix : kBoardLowerMIDIPrefix;
    uint8_t channelMIDI = upperSW ? upperChannel : lowerChannel;
    sendVoiceParam(prefix, kBoardOffset1Prefix, 0x86, stored);
    sendToneSysEx(prefixMIDI, (channelMIDI - 1), 0x19, stored);
  }

  // --- LED update (binary-coded dual LED) ---
  int state = unpackStep(stored, 0x20, 3);  // 0/1/2/3
  mcp3.digitalWrite(DCO2_PITCH_DYN_RED, (state & 1) ? HIGH : LOW);
  mcp3.digitalWrite(DCO2_PITCH_DYN_GREEN, (state & 2) ? HIGH : LOW);
}

FLASHMEM void updatedco1_pitch_lfo_source(bool announce) {
  lastStepParam = 0xFF;
  const uint8_t stored = upperSW ? upperData[P_dco1_pitch_lfo_source] : lowerData[P_dco1_pitch_lfo_source];

  // --- Display ---
  if (announce && !suppressParamAnnounce) {
    displayMode = DM_TONE_FLASH;
    flashToneStep("15 DCO1 LFO", P_dco1_pitch_lfo_source, toneDcoLFOValues, 0x20, 3);
    startParameterDisplay();
  }

  // --- SYX send ---
  if (!switchLEDS) {
    uint8_t prefix = upperSW ? kBoardUpperPrefix : kBoardLowerPrefix;
    uint8_t prefixMIDI = upperSW ? kBoardUpperMIDIPrefix : kBoardLowerMIDIPrefix;
    uint8_t channelMIDI = upperSW ? upperChannel : lowerChannel;
    sendVoiceParam(prefix, kBoardOffset0Prefix, 0x84, stored);
    sendToneSysEx(prefixMIDI, (channelMIDI - 1), 0x0F, stored);
  }

  // --- LED update (binary-coded dual LED) ---
  int state = unpackStep(stored, 0x20, 3);  // 0/1/2/3
  mcp4.digitalWrite(DCO1_PITCH_LFO_SEL_RED, (state & 1) ? HIGH : LOW);
  mcp4.digitalWrite(DCO1_PITCH_LFO_SEL_GREEN, (state & 2) ? HIGH : LOW);
}

FLASHMEM void updatedco2_pitch_lfo_source(bool announce) {
  lastStepParam = 0xFF;
  const uint8_t stored = upperSW ? upperData[P_dco2_pitch_lfo_source] : lowerData[P_dco2_pitch_lfo_source];

  // --- Display ---
  if (announce && !suppressParamAnnounce) {
    displayMode = DM_TONE_FLASH;
    flashToneStep("25 DCO2 LFO", P_dco2_pitch_lfo_source, toneDcoLFOValues, 0x20, 3);
    startParameterDisplay();
  }

  // --- SYX send ---
  if (!switchLEDS) {
    uint8_t prefix = upperSW ? kBoardUpperPrefix : kBoardLowerPrefix;
    uint8_t prefixMIDI = upperSW ? kBoardUpperMIDIPrefix : kBoardLowerMIDIPrefix;
    uint8_t channelMIDI = upperSW ? upperChannel : lowerChannel;
    sendVoiceParam(prefix, kBoardOffset1Prefix, 0x84, stored);
    sendToneSysEx(prefixMIDI, (channelMIDI - 1), 0x17, stored);
  }

  // --- LED update (binary-coded dual LED) ---
  int state = unpackStep(stored, 0x20, 3);  // 0/1/2/3
  mcp3.digitalWrite(DCO2_PITCH_LFO_SEL_RED, (state & 1) ? HIGH : LOW);
  mcp3.digitalWrite(DCO2_PITCH_LFO_SEL_GREEN, (state & 2) ? HIGH : LOW);
}

FLASHMEM void updatedco1_pitch_env_source(bool announce) {
  lastStepParam = 0xFF;
  const uint8_t stored = upperSW ? upperData[P_dco1_pitch_env_source] : lowerData[P_dco1_pitch_env_source];

  // --- Display ---
  if (announce && !suppressParamAnnounce) {
    displayMode = DM_TONE_FLASH;
    flashToneStep("18 DCO1 MODE", P_dco1_pitch_env_source, toneEnvValues, 0x10, 7);
    startParameterDisplay();
  }

  // --- SYX send ---
  if (!switchLEDS) {
    uint8_t prefix = upperSW ? kBoardUpperPrefix : kBoardLowerPrefix;
    uint8_t prefixMIDI = upperSW ? kBoardUpperMIDIPrefix : kBoardLowerMIDIPrefix;
    uint8_t channelMIDI = upperSW ? upperChannel : lowerChannel;
    sendVoiceParam(prefix, kBoardOffset0Prefix, 0x87, stored);
    sendToneSysEx(prefixMIDI, (channelMIDI - 1), 0x12, stored);
  }

  // --- LED update ---
  int state = unpackStep(stored, 0x10, 7);  // 0..7
  int env = (state >> 1) & 3;               // 0..3 (ENV1..ENV4)
  bool pos = state & 1;                     // 1 = positive polarity

  // SOURCE: binary-coded (env 0..3 -> bits on red/green)
  mcp4.digitalWrite(DCO1_PITCH_ENV_SOURCE_RED, (env & 1) ? HIGH : LOW);
  mcp4.digitalWrite(DCO1_PITCH_ENV_SOURCE_GREEN, (env & 2) ? HIGH : LOW);

  // POLARITY: exactly one on at a time
  mcp4.digitalWrite(DCO1_PITCH_ENV_POL_RED, pos ? LOW : HIGH);
  mcp4.digitalWrite(DCO1_PITCH_ENV_POL_GREEN, pos ? HIGH : LOW);
}

FLASHMEM void updatedco2_pitch_env_source(bool announce) {
  lastStepParam = 0xFF;
  const uint8_t stored = upperSW ? upperData[P_dco2_pitch_env_source] : lowerData[P_dco2_pitch_env_source];

  // --- Display ---
  if (announce && !suppressParamAnnounce) {
    displayMode = DM_TONE_FLASH;
    flashToneStep("28 DCO2 MODE", P_dco2_pitch_env_source, toneEnvValues, 0x10, 7);
    startParameterDisplay();
  }

  // --- SYX send ---
  if (!switchLEDS) {
    uint8_t prefix = upperSW ? kBoardUpperPrefix : kBoardLowerPrefix;
    uint8_t prefixMIDI = upperSW ? kBoardUpperMIDIPrefix : kBoardLowerMIDIPrefix;
    uint8_t channelMIDI = upperSW ? upperChannel : lowerChannel;
    sendVoiceParam(prefix, kBoardOffset1Prefix, 0x87, stored);
    sendToneSysEx(prefixMIDI, (channelMIDI - 1), 0x1A, stored);
  }

  // --- LED update ---
  int state = unpackStep(stored, 0x10, 7);  // 0..7
  int env = (state >> 1) & 3;               // 0..3 (ENV1..ENV4)
  bool pos = state & 1;                     // 1 = positive polarity

  // SOURCE: binary-coded (env 0..3 -> bits on red/green)
  mcp3.digitalWrite(DCO2_PITCH_ENV_SOURCE_RED, (env & 1) ? HIGH : LOW);
  mcp3.digitalWrite(DCO2_PITCH_ENV_SOURCE_GREEN, (env & 2) ? HIGH : LOW);

  // POLARITY: exactly one on at a time
  mcp3.digitalWrite(DCO2_PITCH_ENV_POL_RED, pos ? LOW : HIGH);
  mcp3.digitalWrite(DCO2_PITCH_ENV_POL_GREEN, pos ? HIGH : LOW);
}

FLASHMEM void updateeditMode(bool announce) {
  if (announce && !suppressParamAnnounce) {
    // displayMode = 2;
    // switch (editMode) {
    //   case 0:
    //     showCurrentParameterPage("EDITING", "LOWER TONE");
    //     break;
    //   case 1:
    //     showCurrentParameterPage("EDITING", "UPPER TONE");
    //     break;
    //   case 2:
    //     showCurrentParameterPage("EDITING", "BOTH TONES");
    //     break;
    // }
    //startParameterDisplay();
  }
  Serial.println(keyMode);
  if (keyMode != 1 && keyMode != 2) {
    switch (editMode) {
      case 0:
        upperSW = false;
        mcp10.digitalWrite(LOWER_SELECT, HIGH);
        mcp10.digitalWrite(UPPER_SELECT, LOW);
        break;
      case 1:
        upperSW = true;
        mcp10.digitalWrite(LOWER_SELECT, LOW);
        mcp10.digitalWrite(UPPER_SELECT, HIGH);
        break;
      case 2:
        mcp10.digitalWrite(LOWER_SELECT, HIGH);
        mcp10.digitalWrite(UPPER_SELECT, HIGH);
        break;
    }
  }

  if (state == TONE_EDIT || state == TONE_EDITVALUE) {
    tonemenu::refresh_value();
    showToneEditPage(tonemenu::current_setting(),
                     tonemenu::current_setting_value(),
                     currentTonePart);
    updateScreen();
  }

  if (state == PARAMETER && displayMode == DM_TONE_FLASH) {
    renderToneFlashPage();
    startParameterDisplay();  // reset timeout so user can see the change
  }

  if (keyMode != 1 && keyMode != 2) {
    updateAssignLeds();  // Repaint for the newly-active tone
    switchLEDS = true;
    updateButtons();
    switchLEDS = false;
  }
}

FLASHMEM void updatedco_mix_env_source(bool announce) {
  lastStepParam = 0xFF;
  const uint8_t stored = upperSW ? upperData[P_dco_mix_env_source] : lowerData[P_dco_mix_env_source];

  // --- Display ---
  if (announce && !suppressParamAnnounce) {
    displayMode = DM_TONE_FLASH;
    flashToneStep("65 MIX MODE", P_dco_mix_env_source, toneEnvValues, 0x10, 7);
    startParameterDisplay();
  }

  // --- SYX send ---
  if (!switchLEDS) {
    uint8_t prefix = upperSW ? kBoardUpperPrefix : kBoardLowerPrefix;
    uint8_t prefixMIDI = upperSW ? kBoardUpperMIDIPrefix : kBoardLowerMIDIPrefix;
    uint8_t channelMIDI = upperSW ? upperChannel : lowerChannel;
    sendVoiceParam(prefix, NO_OFFSET, 0x94, stored);
    sendToneSysEx(prefixMIDI, (channelMIDI - 1), 0x2F, stored);
  }

  // --- LED update ---
  int state = unpackStep(stored, 0x10, 7);  // 0..7
  int env = (state >> 1) & 3;               // 0..3 (ENV1..ENV4)
  bool pos = state & 1;                     // 1 = positive polarity

  // SOURCE: binary-coded (env 0..3 -> bits on red/green)
  mcp5.digitalWrite(DCO_MIX_ENV_SOURCE_RED, (env & 1) ? HIGH : LOW);
  mcp5.digitalWrite(DCO_MIX_ENV_SOURCE_GREEN, (env & 2) ? HIGH : LOW);

  // POLARITY: exactly one on at a time
  mcp5.digitalWrite(DCO_MIX_ENV_POL_RED, pos ? LOW : HIGH);
  mcp5.digitalWrite(DCO_MIX_ENV_POL_GREEN, pos ? HIGH : LOW);
}

FLASHMEM void updatedco_mix_dyn(bool announce) {
  lastStepParam = 0xFF;
  const uint8_t stored = upperSW ? upperData[P_dco_mix_dyn] : lowerData[P_dco_mix_dyn];

  // --- Display ---
  if (announce && !suppressParamAnnounce) {
    displayMode = DM_TONE_FLASH;
    flashToneStep("64 MIX DYNA", P_dco_mix_dyn, toneDynValues, 0x20, 3);
    startParameterDisplay();
  }

  // --- SYX send ---
  if (!switchLEDS) {
    uint8_t prefix = upperSW ? kBoardUpperPrefix : kBoardLowerPrefix;
    uint8_t prefixMIDI = upperSW ? kBoardUpperMIDIPrefix : kBoardLowerMIDIPrefix;
    uint8_t channelMIDI = upperSW ? upperChannel : lowerChannel;
    sendVoiceParam(prefix, NO_OFFSET, 0x93, stored);
    sendToneSysEx(prefixMIDI, (channelMIDI - 1), 0x2E, stored);
  }

  // --- LED update (binary-coded dual LED) ---
  int state = unpackStep(stored, 0x20, 3);  // 0/1/2/3
  mcp5.digitalWrite(DCO_MIX_DYN_RED, (state & 1) ? HIGH : LOW);
  mcp5.digitalWrite(DCO_MIX_DYN_GREEN, (state & 2) ? HIGH : LOW);
}

FLASHMEM void updatevcf_env_source(bool announce) {
  lastStepParam = 0xFF;
  const uint8_t stored = upperSW ? upperData[P_vcf_env_source] : lowerData[P_vcf_env_source];

  // --- Display ---
  if (announce && !suppressParamAnnounce) {
    displayMode = DM_TONE_FLASH;
    flashToneStep("78 VCF MODE", P_vcf_env_source, toneEnvValues, 0x10, 7);
    startParameterDisplay();
  }

  // --- SYX send ---
  if (!switchLEDS) {
    uint8_t prefix = upperSW ? kBoardUpperPrefix : kBoardLowerPrefix;
    uint8_t prefixMIDI = upperSW ? kBoardUpperMIDIPrefix : kBoardLowerMIDIPrefix;
    uint8_t channelMIDI = upperSW ? upperChannel : lowerChannel;
    sendVoiceParam(prefix, NO_OFFSET, 0x9D, stored);
    sendToneSysEx(prefixMIDI, (channelMIDI - 1), 0x37, stored);
  }

  // --- LED update ---
  int state = unpackStep(stored, 0x10, 7);  // 0..7
  int env = (state >> 1) & 3;               // 0..3 (ENV1..ENV4)
  bool pos = state & 1;                     // 1 = positive polarity

  // SOURCE: binary-coded (env 0..3 -> bits on red/green)
  mcp5.digitalWrite(VCF_ENV_SOURCE_RED, (env & 1) ? HIGH : LOW);
  mcp5.digitalWrite(VCF_ENV_SOURCE_GREEN, (env & 2) ? HIGH : LOW);

  // POLARITY: exactly one on at a time
  mcp5.digitalWrite(VCF_ENV_POL_RED, pos ? LOW : HIGH);
  mcp5.digitalWrite(VCF_ENV_POL_GREEN, pos ? HIGH : LOW);
}

FLASHMEM void updatevcf_dyn(bool announce) {
  lastStepParam = 0xFF;
  const uint8_t stored = upperSW ? upperData[P_vcf_dyn] : lowerData[P_vcf_dyn];

  // --- Display ---
  if (announce && !suppressParamAnnounce) {
    displayMode = DM_TONE_FLASH;
    flashToneStep("77 VCF DYNA", P_vcf_dyn, toneDynValues, 0x20, 3);
    startParameterDisplay();
  }

  // --- SYX send ---
  if (!switchLEDS) {
    uint8_t prefix = upperSW ? kBoardUpperPrefix : kBoardLowerPrefix;
    uint8_t prefixMIDI = upperSW ? kBoardUpperMIDIPrefix : kBoardLowerMIDIPrefix;
    uint8_t channelMIDI = upperSW ? upperChannel : lowerChannel;
    sendVoiceParam(prefix, NO_OFFSET, 0x9C, stored);
    sendToneSysEx(prefixMIDI, (channelMIDI - 1), 0x36, stored);
  }

  // --- LED update (binary-coded dual LED) ---
  int state = unpackStep(stored, 0x20, 3);  // 0/1/2/3
  mcp6.digitalWrite(VCF_DYN_RED, (state & 1) ? HIGH : LOW);
  mcp6.digitalWrite(VCF_DYN_GREEN, (state & 2) ? HIGH : LOW);
}

FLASHMEM void updatevca_env_source(bool announce) {
  lastStepParam = 0xFF;
  const uint8_t stored = upperSW ? upperData[P_vca_env_source] : lowerData[P_vca_env_source];

  // --- Display ---
  if (announce && !suppressParamAnnounce) {
    displayMode = DM_TONE_FLASH;
    flashToneStep("82 VCA MODE", P_vca_env_source, toneVcaEnvValues, 0x20, 3);
    startParameterDisplay();
  }

  // --- SYX send ---
  if (!switchLEDS) {
    uint8_t prefix = upperSW ? kBoardUpperPrefix : kBoardLowerPrefix;
    uint8_t prefixMIDI = upperSW ? kBoardUpperMIDIPrefix : kBoardLowerMIDIPrefix;
    uint8_t channelMIDI = upperSW ? upperChannel : lowerChannel;
    sendVoiceParam(prefix, NO_OFFSET, 0xAE, stored);
    sendToneSysEx(prefixMIDI, (channelMIDI - 1), 0x39, stored);
  }

  // --- LED update (binary-coded dual LED) ---
  int state = unpackStep(stored, 0x20, 3);  // 0/1/2/3
  mcp6.digitalWrite(VCA_ENV_SOURCE_RED, (state & 1) ? HIGH : LOW);
  mcp6.digitalWrite(VCA_ENV_SOURCE_GREEN, (state & 2) ? HIGH : LOW);
}

FLASHMEM void updatevca_dyn(bool announce) {
  lastStepParam = 0xFF;
  const uint8_t stored = upperSW ? upperData[P_vca_dyn] : lowerData[P_vca_dyn];

  // --- Display ---
  if (announce && !suppressParamAnnounce) {
    displayMode = DM_TONE_FLASH;
    flashToneStep("83 VCA DYNA", P_vca_dyn, toneDynValues, 0x20, 3);
    startParameterDisplay();
  }

  // --- SYX send ---
  if (!switchLEDS) {
    uint8_t prefix = upperSW ? kBoardUpperPrefix : kBoardLowerPrefix;
    uint8_t prefixMIDI = upperSW ? kBoardUpperMIDIPrefix : kBoardLowerMIDIPrefix;
    uint8_t channelMIDI = upperSW ? upperChannel : lowerChannel;
    sendVoiceParam(prefix, NO_OFFSET, 0x9F, stored);
    sendToneSysEx(prefixMIDI, (channelMIDI - 1), 0x3A, stored);
  }

  // --- LED update (binary-coded dual LED) ---
  int state = unpackStep(stored, 0x20, 3);  // 0/1/2/3
  mcp6.digitalWrite(VCA_DYN_RED, (state & 1) ? HIGH : LOW);
  mcp6.digitalWrite(VCA_DYN_GREEN, (state & 2) ? HIGH : LOW);
}

FLASHMEM void updatechorus(bool announce) {
  lastStepParam = 0xFF;
  const uint8_t stored = upperSW ? upperData[P_chorus] : lowerData[P_chorus];

  // --- Display ---
  if (announce && !suppressParamAnnounce) {
    displayMode = DM_TONE_FLASH;
    flashToneStep("34 CHORUS", P_chorus, ChorusValues, 0x20, 2);
    startParameterDisplay();
  }

  // --- SYX send ---
  if (!switchLEDS) {
    uint8_t prefix = upperSW ? kBoardUpperPrefix : kBoardLowerPrefix;
    uint8_t prefixMIDI = upperSW ? kBoardUpperMIDIPrefix : kBoardLowerMIDIPrefix;
    uint8_t channelMIDI = upperSW ? upperChannel : lowerChannel;
    sendVoiceParam(prefix, NO_OFFSET, 0xA0, stored);
    sendToneSysEx(prefixMIDI, (channelMIDI - 1), 0x1E, stored);
  }

  // --- LED update ---
  bool red = (stored == 0x20);    // ON  -> red
  bool green = (stored == 0x40);  // KEY -> green
  mcp6.digitalWrite(CHORUS_SELECT_RED, red ? HIGH : LOW);
  mcp6.digitalWrite(CHORUS_SELECT_GREEN, green ? HIGH : LOW);
}


FLASHMEM void updateportamento_sw(bool announce) {
  if (announce && !suppressParamAnnounce) {
    displayMode = 1;
    switch (portamento_sw) {
      case 0:
        showCurrentParameterPage("37 & 47 PORTAMENTO", "OFF");
        break;
      case 1:
        showCurrentParameterPage("47 LOWER PORTAMENTO", "ON");
        break;
      case 2:
        showCurrentParameterPage("37 UPPER PORTAMENTO", "ON");
        break;
      case 3:
        showCurrentParameterPage("37 & 47 PORTAMENTO", "ON");
        break;
    }
    startParameterDisplay();
  }

  if (!lowerPortamento_SW && !upperPortamento_SW) {
    sendVoiceParam(kBoardLowerPrefix, NO_OFFSET, 0xB5, 0x00);
    sendVoiceParam(kBoardUpperPrefix, NO_OFFSET, 0xB5, 0x00);
    sendCustomSysEx((lowerChannel - 1), 0x2C, 0x00);
    sendCustomSysEx((upperChannel - 1), 0x23, 0x00);
    mcp2.digitalWrite(PORTAMENTO_LOWER_RED, LOW);
    mcp2.digitalWrite(PORTAMENTO_UPPER_GREEN, LOW);
    portamento_sw = 0;
  }

  if (lowerPortamento_SW && !upperPortamento_SW) {
    sendVoiceParam(kBoardLowerPrefix, NO_OFFSET, 0xB5, 0x7F);
    sendVoiceParam(kBoardUpperPrefix, NO_OFFSET, 0xB5, 0x00);
    sendCustomSysEx((lowerChannel - 1), 0x2C, 0x01);
    sendCustomSysEx((upperChannel - 1), 0x23, 0x00);
    mcp2.digitalWrite(PORTAMENTO_LOWER_RED, HIGH);
    mcp2.digitalWrite(PORTAMENTO_UPPER_GREEN, LOW);
    portamento_sw = 1;
  }

  if (!lowerPortamento_SW && upperPortamento_SW) {
    sendVoiceParam(kBoardLowerPrefix, NO_OFFSET, 0xB5, 0x00);
    sendVoiceParam(kBoardUpperPrefix, NO_OFFSET, 0xB5, 0x7F);
    sendCustomSysEx((lowerChannel - 1), 0x2C, 0x00);
    sendCustomSysEx((upperChannel - 1), 0x23, 0x01);
    mcp2.digitalWrite(PORTAMENTO_LOWER_RED, LOW);
    mcp2.digitalWrite(PORTAMENTO_UPPER_GREEN, HIGH);
    portamento_sw = 2;
  }

  if (lowerPortamento_SW && upperPortamento_SW) {
    sendVoiceParam(kBoardLowerPrefix, NO_OFFSET, 0xB5, 0x7F);
    sendVoiceParam(kBoardUpperPrefix, NO_OFFSET, 0xB5, 0x7F);
    sendCustomSysEx((lowerChannel - 1), 0x2C, 0x01);
    sendCustomSysEx((upperChannel - 1), 0x23, 0x01);
    mcp2.digitalWrite(PORTAMENTO_LOWER_RED, HIGH);
    mcp2.digitalWrite(PORTAMENTO_UPPER_GREEN, HIGH);
    portamento_sw = 3;
  }
}

FLASHMEM void updateenv5stage(bool announce) {
  if (announce && !suppressParamAnnounce) {
    displayMode = 2;
    switch (env5stage) {
      case 0:
        showCurrentParameterPage("5 STAGE", "ENVELOPE 1");
        break;
      case 1:
        showCurrentParameterPage("5 STAGE", "ENVELOPE 2");
        break;
    }
    startParameterDisplay();
  }
  switch (env5stage) {
    case 0:
      mcp5.digitalWrite(ENV5STAGE_SELECT_RED, HIGH);
      mcp5.digitalWrite(ENV5STAGE_SELECT_GREEN, LOW);
      break;
    case 1:
      mcp5.digitalWrite(ENV5STAGE_SELECT_RED, LOW);
      mcp5.digitalWrite(ENV5STAGE_SELECT_GREEN, HIGH);
      break;
  }
}

FLASHMEM void updateadsr(bool announce) {
  if (announce && !suppressParamAnnounce) {
    displayMode = 2;
    switch (adsr) {
      case 0:
        showCurrentParameterPage("ADSR", "ENVELOPE 3");
        break;
      case 1:
        showCurrentParameterPage("ADSR", "ENVELOPE 4");
        break;
    }
    startParameterDisplay();
  }
  switch (adsr) {
    case 0:
      mcp6.digitalWrite(ADSR_SELECT_RED, HIGH);
      mcp6.digitalWrite(ADSR_SELECT_GREEN, LOW);
      break;
    case 1:
      mcp6.digitalWrite(ADSR_SELECT_RED, LOW);
      mcp6.digitalWrite(ADSR_SELECT_GREEN, HIGH);
      break;
  }
}

void initRotaryEncoders() {
  for (auto &rotaryEncoder : rotaryEncoders) {
    rotaryEncoder.init();
  }
}

void initButtons() {
  for (auto &button : allButtons) {
    button->begin();
  }
}

void pollAllMCPs() {

  for (int j = 0; j < numMCPs; j++) {
    uint16_t gpioAB = allMCPs[j]->readGPIOAB();

    for (auto &button : allButtons) {
      if (button->getMcp() == allMCPs[j]) {
        button->feedInput(gpioAB);
      }
    }
  }
}

int getLowerSplitVoice(byte note) {
  // Try round-robin for a free voice first (Poly1 behaviour)
  for (int i = 0; i < 6; i++) {
    int idx = (lowerSplitVoicePointer + i) % 6;
    if (!voiceOn[idx]) {
      lowerSplitVoicePointer = (idx + 1) % 6;
      return idx;
    }
  }

  // No free voice: steal oldest, but prefer not physically held (JP-8 Hold behaviour)
  int oldest = oldestVoicePreferNotPhysHeld(0, 5);
  lowerSplitVoicePointer = (oldest + 1) % 6;
  return oldest;
}

int getUpperSplitVoice(byte note) {
  // Try round-robin for a free voice first (Poly1 behaviour)
  for (int i = 0; i < 6; i++) {
    int idx = 6 + (upperSplitVoicePointer + i) % 6;
    if (!voiceOn[idx]) {
      upperSplitVoicePointer = (idx - 6 + 1) % 6;  // pointer is 0..3
      return idx;
    }
  }

  // No free voice: steal oldest, but prefer not physically held (JP-8 Hold behaviour)
  int oldest = oldestVoicePreferNotPhysHeld(6, 11);
  upperSplitVoicePointer = ((oldest - 6) + 1) % 6;
  return oldest;
}

int getLowerSplitVoicePoly2(byte note) {
  // Poly2: pick the lowest-numbered free voice if any
  for (int i = 0; i < 6; i++) {
    if (!voiceOn[i]) return i;
  }

  // No free voice: steal oldest, but prefer not physically held (JP-8 Hold behaviour)
  return oldestVoicePreferNotPhysHeld(0, 5);
}

int getUpperSplitVoicePoly2(byte note) {
  // Poly2: pick the lowest-numbered free voice if any
  for (int i = 6; i < 12; i++) {
    if (!voiceOn[i]) return i;
  }

  // No free voice: steal oldest, but prefer not physically held (JP-8 Hold behaviour)
  return oldestVoicePreferNotPhysHeld(6, 11);
}


inline void sendVoiceNoteOn(int voiceIdx, byte note, byte vel) {
  if (voiceIdx < 6) {
    // Lower board: board select F1, note slot C0-C5
    Serial3.write(0xF1);
    Serial3.write(0xC0 | voiceIdx);  // C0–C5
    Serial3.write(note & 0x7F);
    Serial3.write(vel & 0x7F);
  } else {
    // Upper board: board select F9, note slot C0-C5
    Serial3.write(0xF9);
    Serial3.write(0xC0 | (voiceIdx - 6));  // C0–C5
    Serial3.write(note & 0x7F);
    Serial3.write(vel & 0x7F);
  }
}

inline void sendVoiceNoteOff(int voiceIdx, byte note) {
  if (voiceIdx < 6) {
    // Lower board: board select F1, note slot D0-D5
    Serial3.write(0xF1);
    Serial3.write(0xD0 | voiceIdx);  // D0–D5
    Serial3.write(note & 0x7F);
    Serial3.write(0x00);  // zero velocity on note off
  } else {
    // Upper board: board select F9, note slot D0-D5
    Serial3.write(0xF9);
    Serial3.write(0xD0 | (voiceIdx - 6));  // D0–D5
    Serial3.write(note & 0x7F);
    Serial3.write(0x00);
  }
}

void assignVoice(byte note, byte velocity, int voiceIdx) {
  if (voiceIdx < 0 || voiceIdx >= 12) return;

  // If voice is already sounding a different note, release it properly (state + LED + mappings)
  if (voices[voiceIdx].noteOn && voices[voiceIdx].note >= 0 && voices[voiceIdx].note != note) {
    releaseVoice((byte)voices[voiceIdx].note, voiceIdx);
  }

  voices[voiceIdx].note = note;
  voices[voiceIdx].velocity = velocity;
  voices[voiceIdx].timeOn = millis();
  voices[voiceIdx].noteOn = true;
  voiceOn[voiceIdx] = true;

  sendVoiceNoteOn(voiceIdx, note, velocity);
}

void releaseVoice(byte note, int voiceIdx) {
  if (voiceIdx < 0 || voiceIdx >= 12) return;

  if (voices[voiceIdx].noteOn && voices[voiceIdx].note == note) {
    sendVoiceNoteOff(voiceIdx, note);

    voices[voiceIdx].note = -1;
    voices[voiceIdx].noteOn = false;
    voiceOn[voiceIdx] = false;


    if (voiceIdx < 6) {
      voiceAssignmentLower[note] = -1;
      voiceToNoteLower[voiceIdx] = -1;
    } else {
      voiceAssignmentUpper[note] = -1;
      voiceToNoteUpper[voiceIdx - 6] = -1;
    }
  }
}

int getVoiceNoPoly2(int note) {
  voiceToReturn = -1;       // Initialize to 'null'
  earliestTime = millis();  // Initialize to now

  if (note == -1) {
    // NoteOn() - Get the oldest free voice (recent voices may still be on the release stage)
    if (voices[lastUsedVoice].note == -1) {
      return lastUsedVoice + 1;
    }

    // If the last used voice is not free or doesn't exist, check if the first voice is free
    if (voices[0].note == -1) {
      return 1;
    }

    // Find the lowest available voice for the new note
    for (int i = 0; i < NO_OF_VOICES; i++) {
      if (voices[i].note == -1) {
        return i + 1;
      }
    }

    // If no voice is available, release the oldest note
    int oldestVoice = 0;
    for (int i = 1; i < NO_OF_VOICES; i++) {
      if (voices[i].timeOn < voices[oldestVoice].timeOn) {
        oldestVoice = i;
      }
    }
    return oldestVoice + 1;
  } else {
    // NoteOff() - Get the voice number from the note
    for (int i = 0; i < NO_OF_VOICES; i++) {
      if (voices[i].note == note) {
        return i + 1;
      }
    }
  }

  // Shouldn't get here, return voice 1
  return 1;
}

int getVoiceNo(int note) {
  voiceToReturn = -1;       //Initialise to 'null'
  earliestTime = millis();  //Initialise to now
  if (note == -1) {
    //NoteOn() - Get the oldest free voice (recent voices may be still on release stage)
    for (int i = 0; i < NO_OF_VOICES; i++) {
      if (voices[i].note == -1) {
        if (voices[i].timeOn < earliestTime) {
          earliestTime = voices[i].timeOn;
          voiceToReturn = i;
        }
      }
    }
    if (voiceToReturn == -1) {
      //No free voices, need to steal oldest sounding voice
      earliestTime = millis();  //Reinitialise
      for (int i = 0; i < NO_OF_VOICES; i++) {
        if (voices[i].timeOn < earliestTime) {
          earliestTime = voices[i].timeOn;
          voiceToReturn = i;
        }
      }
    }
    return voiceToReturn + 1;
  } else {
    //NoteOff() - Get voice number from note
    for (int i = 0; i < NO_OF_VOICES; i++) {
      if (voices[i].note == note) {
        return i + 1;
      }
    }
  }
  //Shouldn't get here, return voice 1
  return 1;
}

int mod(int a, int b) {
  int r = a % b;
  return r < 0 ? r + b : r;
}

// Hold functions

inline bool pedalAffectsLower() {
  return true;
}

inline bool pedalAffectsUpper() {
  return (keyMode != 2);
}

inline bool holdEffectiveLower() {
  if (keyMode == 3) return (holdManualLower || holdPedal);   // SPLIT
  return (holdManualLower || holdManualUpper || holdPedal);  // WHOLE/DUAL
}

inline bool holdEffectiveUpper() {
  if (keyMode == 3) return holdManualUpper;                  // SPLIT (no pedal)
  return (holdManualLower || holdManualUpper || holdPedal);  // WHOLE/DUAL
}

void reconcileHoldReleases() {

  // PEDAL: sustain acts as global hold in WHOLE/DUAL, and LOWER-only in SPLIT
  auto holdEffectiveLower = [&]() -> bool {
    if (keyMode == 3) return (holdManualLower || holdPedal);   // SPLIT lower + pedal
    return (holdManualLower || holdManualUpper || holdPedal);  // WHOLE/DUAL global
  };
  auto holdEffectiveUpper = [&]() -> bool {
    if (keyMode == 3) return (holdManualUpper);                // SPLIT upper (no pedal)
    return (holdManualLower || holdManualUpper || holdPedal);  // WHOLE/DUAL global
  };

  // -------------------------
  // WHOLE or DUAL: hold is global
  // -------------------------
  if (keyMode != 3) {

    // Only reconcile when hold is effectively OFF (manual + pedal)
    if (!(holdManualLower || holdManualUpper || holdPedal)) {

      for (int n = 0; n < 128; n++) {

        bool latched = holdLatchedLower[n] || holdLatchedUpper[n];
        if (!latched) continue;

        bool phys = keyDownLower[n] || keyDownUpper[n] || keyDownWhole[n];
        if (phys) continue;

        // Release ALL voices currently playing this note
        for (int v = 0; v < 12; v++) {
          if (voices[v].noteOn && voices[v].note == n) {
            releaseVoice((byte)n, v);
          }
        }

        // Clear latch
        holdLatchedLower[n] = false;
        holdLatchedUpper[n] = false;
      }
    }

    return;
  }

  // -------------------------
  // SPLIT: Lower/Upper independent
  // -------------------------

  // LOWER (manual lower OR pedal)
  if (!holdEffectiveLower()) {
    for (int n = 0; n < 128; n++) {
      if (holdLatchedLower[n] && !keyDownLower[n]) {

        // Safer than voiceAssignmentLower[n] because voice stealing / mono/unison can change it.
        for (int v = 0; v <= 5; v++) {
          if (voices[v].noteOn && voices[v].note == n) {
            releaseVoice((byte)n, v);
          }
        }

        holdLatchedLower[n] = false;
      }
    }
  }

  // UPPER (manual upper only; pedal does not affect upper in split)
  if (!holdEffectiveUpper()) {
    for (int n = 0; n < 128; n++) {
      if (holdLatchedUpper[n] && !keyDownUpper[n]) {

        for (int v = 6; v <= 11; v++) {
          if (voices[v].noteOn && voices[v].note == n) {
            releaseVoice((byte)n, v);
          }
        }

        holdLatchedUpper[n] = false;
      }
    }
  }
}

inline bool isKeyPhysicallyDownForVoice(int voiceIdx) {
  int n = voices[voiceIdx].note;
  if (n < 0 || n > 127) return false;

  if (voiceIdx < 6) return keyDownLower[n];
  else return keyDownUpper[n];
}

int oldestVoicePreferNotPhysHeld(int vStart, int vEndInclusive) {
  int best = vStart;
  unsigned long bestTime = 0;
  bool found = false;

  // 1) oldest voice where key is NOT physically held
  for (int v = vStart; v <= vEndInclusive; v++) {
    if (!voiceOn[v]) continue;
    if (isKeyPhysicallyDownForVoice(v)) continue;
    if (!found || voices[v].timeOn < bestTime) {
      best = v;
      bestTime = voices[v].timeOn;
      found = true;
    }
  }
  if (found) return best;

  // 2) otherwise fall back to oldest regardless
  best = vStart;
  bestTime = voices[vStart].timeOn;
  for (int v = vStart + 1; v <= vEndInclusive; v++) {
    if (voices[v].timeOn < bestTime) {
      best = v;
      bestTime = voices[v].timeOn;
    }
  }
  return best;
}

// Mono lower & upper

void commandTopNoteLower() {
  int topNote = -1;
  for (int i = 0; i < 128; i++)
    if (notesLower[i]) topNote = i;

  if (topNote >= 0)
    assignVoice(topNote, noteVel, 0);
  else
    releaseVoice(noteMsg, 0);
}

void commandBottomNoteLower() {
  int bottomNote = -1;
  for (int i = 127; i >= 0; i--)
    if (notesLower[i]) bottomNote = i;

  if (bottomNote >= 0)
    assignVoice(bottomNote, noteVel, 0);
  else
    releaseVoice(noteMsg, 0);
}

void commandLastNoteLower() {
  for (int i = 0; i < 40; i++) {
    int8_t idx = noteOrderLower[mod(orderIndxLower - i, 40)];
    if (notesLower[idx]) {
      assignVoice(idx, noteVel, 0);
      return;
    }
  }
  releaseVoice(noteMsg, 0);
}

void commandTopNoteUpper() {
  int topNote = -1;
  for (int i = 0; i < 128; i++)
    if (notesUpper[i]) topNote = i;

  if (topNote >= 0)
    assignVoice(topNote, noteVel, 6);
  else
    releaseVoice(noteMsg, 4);
}

void commandBottomNoteUpper() {
  int bottomNote = -1;
  for (int i = 127; i >= 0; i--)
    if (notesUpper[i]) bottomNote = i;

  if (bottomNote >= 0)
    assignVoice(bottomNote, noteVel, 6);
  else
    releaseVoice(noteMsg, 4);
}

void commandLastNoteUpper() {
  for (int i = 0; i < 40; i++) {
    int8_t idx = noteOrderUpper[mod(orderIndxUpper - i, 40)];
    if (notesUpper[idx]) {
      assignVoice(idx, noteVel, 4);
      return;
    }
  }
  releaseVoice(noteMsg, 4);
}

// Unison lower and upper

void commandTopNoteUniLower() {
  int topNote = -1;
  for (int i = 0; i < 128; i++)
    if (notesLower[i]) topNote = i;

  if (topNote >= 0)
    for (int v = 0; v < 6; v++) assignVoice(topNote, noteVel, v);
  else
    for (int v = 0; v < 6; v++) releaseVoice(noteMsg, v);
}

void commandBottomNoteUniLower() {
  int bottomNote = -1;
  for (int i = 127; i >= 0; i--)
    if (notesLower[i]) bottomNote = i;

  if (bottomNote >= 0)
    for (int v = 0; v < 6; v++) assignVoice(bottomNote, noteVel, v);
  else
    for (int v = 0; v < 6; v++) releaseVoice(noteMsg, v);
}

void commandLastNoteUniLower() {
  for (int i = 0; i < 40; i++) {
    int8_t idx = noteOrderLower[mod(orderIndxLower - i, 40)];
    if (notesLower[idx]) {
      for (int v = 0; v < 6; v++) assignVoice(idx, noteVel, v);
      return;
    }
  }
  for (int v = 0; v < 6; v++) releaseVoice(noteMsg, v);
}

void commandTopNoteUniUpper() {
  int topNote = -1;
  for (int i = 0; i < 128; i++)
    if (notesUpper[i]) topNote = i;

  if (topNote >= 0)
    for (int v = 6; v < 12; v++) assignVoice(topNote, noteVel, v);
  else
    for (int v = 6; v < 12; v++) releaseVoice(noteMsg, v);
}

void commandBottomNoteUniUpper() {
  int bottomNote = -1;
  for (int i = 127; i >= 0; i--)
    if (notesUpper[i]) bottomNote = i;

  if (bottomNote >= 0)
    for (int v = 6; v < 12; v++) assignVoice(bottomNote, noteVel, v);
  else
    for (int v = 6; v < 12; v++) releaseVoice(noteMsg, v);
}

void commandLastNoteUniUpper() {
  for (int i = 0; i < 40; i++) {
    int8_t idx = noteOrderUpper[mod(orderIndxUpper - i, 40)];
    if (notesUpper[idx]) {
      for (int v = 6; v < 12; v++) assignVoice(idx, noteVel, v);
      return;
    }
  }
  for (int v = 6; v < 12; v++) releaseVoice(noteMsg, v);
}

void commandMonoNoteOnUpper(byte note, byte velocity) {
  notesUpper[note] = true;
  noteMsg = note;
  noteVel = velocity;
  orderIndxUpper = (orderIndxUpper + 1) % 40;
  noteOrderUpper[orderIndxUpper] = note;
  commandLastNoteUpper();
}

void commandMonoNoteOffUpper(byte note) {
  notesUpper[note] = false;
  noteMsg = note;
  commandLastNoteUpper();
}

void commandMonoNoteOnLower(byte note, byte velocity) {
  notesLower[note] = true;
  noteMsg = note;
  noteVel = velocity;
  orderIndxLower = (orderIndxLower + 1) % 40;
  noteOrderLower[orderIndxLower] = note;
  commandLastNoteLower();
}

void commandMonoNoteOffLower(byte note) {
  notesLower[note] = false;
  noteMsg = note;
  commandLastNoteLower();
}

void commandUnisonNoteOnUpper(byte note, byte velocity) {
  notesUpper[note] = true;
  noteMsg = note;      // explicitly set here
  noteVel = velocity;  // explicitly set here
  orderIndxWhole = (orderIndxWhole + 1) % 40;
  noteOrderUpper[orderIndxUpper] = note;
  commandLastNoteUniUpper();  // Last note priority
}

void commandUnisonNoteOffUpper(byte note) {
  notesUpper[note] = false;
  noteMsg = note;  // explicitly set here
  commandLastNoteUniUpper();
}

void commandUnisonNoteOnLower(byte note, byte velocity) {
  notesLower[note] = true;
  noteMsg = note;      // explicitly set here
  noteVel = velocity;  // explicitly set here
  orderIndxWhole = (orderIndxWhole + 1) % 40;
  noteOrderLower[orderIndxLower] = note;
  commandLastNoteUniLower();  // Last note priority
}

void commandUnisonNoteOffLower(byte note) {
  notesLower[note] = false;
  noteMsg = note;  // explicitly set here
  commandLastNoteUniLower();
}

int getLowerUnisonVoice() {
  // Find free pair starting from pointer
  for (int i = 0; i < 3; i++) {
    int idx = (lowerUnisonVoicePointer + i) % 3;
    if (!voiceOn[idx] && !voiceOn[idx + 3]) {
      lowerUnisonVoicePointer = (idx + 1) % 3;
      return idx;
    }
  }
  // No free pair: steal oldest pair
  int oldest = 0;
  unsigned long oldestTime = voices[0].timeOn;
  for (int i = 1; i < 3; i++) {
    if (voices[i].timeOn < oldestTime) {
      oldestTime = voices[i].timeOn;
      oldest = i;
    }
  }
  lowerUnisonVoicePointer = (oldest + 1) % 3;
  return oldest;
}

int getUpperUnisonVoice() {
  // Find free pair starting from pointer
  for (int i = 0; i < 3; i++) {
    int idx = (upperUnisonVoicePointer + i) % 3;
    if (!voiceOn[idx + 6] && !voiceOn[idx + 9]) {
      upperUnisonVoicePointer = (idx + 1) % 3;
      return idx + 6;
    }
  }
  // No free pair: steal oldest pair
  int oldest = 0;
  unsigned long oldestTime = voices[6].timeOn;
  for (int i = 1; i < 3; i++) {
    if (voices[i + 6].timeOn < oldestTime) {
      oldestTime = voices[i + 6].timeOn;
      oldest = i;
    }
  }
  upperUnisonVoicePointer = (oldest + 1) % 3;
  return oldest + 6;
}

void assignUnisonPairLower(byte note, byte velocity, bool octave) {
  int v = getLowerUnisonVoice();  // returns 0, 1, or 2
  int vPartner = v + 3;
  byte notePartner = octave ? (byte)max(0, (int)note - 12) : note;

  // Release both slots if they are sounding something different
  releaseVoice(voices[v].note, v);
  releaseVoice(voices[vPartner].note, vPartner);

  assignVoice(note, velocity, v);
  assignVoice(notePartner, velocity, vPartner);

  voiceAssignmentLower[note] = v;
  voiceToNoteLower[v] = note;
  voiceToNoteLower[vPartner] = note;  // track partner by same note for release
}

void releaseUnisonPairLower(byte note) {
  int v = voiceAssignmentLower[note];
  if (v >= 0 && v <= 2) {
    releaseVoice(note, v);
    releaseVoice(voices[v + 3].note, v + 3);
    voiceAssignmentLower[note] = -1;
    voiceToNoteLower[v] = -1;
    voiceToNoteLower[v + 3] = -1;
  }
}

void assignUnisonPairUpper(byte note, byte velocity, bool octave) {
  int v = getUpperUnisonVoice();  // returns 6, 7, or 8
  int vPartner = v + 3;
  byte notePartner = octave ? (byte)max(0, (int)note - 12) : note;

  releaseVoice(voices[v].note, v);
  releaseVoice(voices[vPartner].note, vPartner);

  assignVoice(note, velocity, v);
  assignVoice(notePartner, velocity, vPartner);

  voiceAssignmentUpper[note] = v;
  voiceToNoteUpper[v - 6] = note;
  voiceToNoteUpper[vPartner - 6] = note;  // track partner by same note for release
}

void releaseUnisonPairUpper(byte note) {
  int v = voiceAssignmentUpper[note];
  if (v >= 6 && v <= 8) {
    releaseVoice(note, v);
    releaseVoice(voices[v + 3].note, v + 3);
    voiceAssignmentUpper[note] = -1;
    voiceToNoteUpper[v - 6] = -1;
    voiceToNoteUpper[v + 3 - 6] = -1;
  }
}

int getWholeUnisonVoice() {
  // Pairs are: (0,3), (1,4), (2,5), (6,9), (7,10), (8,11)
  static const int pairA[6] = { 0, 1, 2, 6, 7, 8 };
  static const int pairB[6] = { 3, 4, 5, 9, 10, 11 };

  // Find free pair starting from pointer
  for (int i = 0; i < 6; i++) {
    int idx = (wholeUnisonVoicePointer + i) % 6;
    if (!voiceOn[pairA[idx]] && !voiceOn[pairB[idx]]) {
      wholeUnisonVoicePointer = (idx + 1) % 6;
      return idx;
    }
  }

  // No free pair: steal oldest
  int oldest = 0;
  unsigned long oldestTime = voices[pairA[0]].timeOn;
  for (int i = 1; i < 6; i++) {
    if (voices[pairA[i]].timeOn < oldestTime) {
      oldestTime = voices[pairA[i]].timeOn;
      oldest = i;
    }
  }
  wholeUnisonVoicePointer = (oldest + 1) % 6;
  return oldest;
}

void assignWholeUnisonPair(byte note, byte velocity, bool octave) {
  static const int pairA[6] = { 0, 1, 2, 6, 7, 8 };
  static const int pairB[6] = { 3, 4, 5, 9, 10, 11 };

  int idx = getWholeUnisonVoice();
  int vA = pairA[idx];
  int vB = pairB[idx];
  byte noteB = octave ? (byte)max(0, (int)note - 12) : note;

  releaseVoice(voices[vA].note, vA);
  releaseVoice(voices[vB].note, vB);

  assignVoice(note, velocity, vA);
  assignVoice(noteB, velocity, vB);

  voiceAssignmentLower[note] = idx;  // store pair index for release lookup
  voiceToNoteLower[vA] = note;
  voiceToNoteLower[vB] = note;
}

void releaseWholeUnisonPair(byte note) {
  static const int pairA[6] = { 0, 1, 2, 6, 7, 8 };
  static const int pairB[6] = { 3, 4, 5, 9, 10, 11 };

  int idx = voiceAssignmentLower[note];
  if (idx >= 0 && idx <= 5) {
    releaseVoice(note, pairA[idx]);
    releaseVoice(voices[pairB[idx]].note, pairB[idx]);
    voiceAssignmentLower[note] = -1;
    voiceToNoteLower[pairA[idx]] = -1;
    voiceToNoteLower[pairB[idx]] = -1;
  }
}

byte xfadeLowerVel(byte velocity) {
  if (velocity <= 80) return 0x60;
  if (velocity >= 112) return 0;
  // Ramp down from 96 to 3 over velocity range 81-111
  return (byte)map(velocity, 81, 111, 96, 3);
}

byte xfadeUpperVel(byte velocity) {
  if (velocity <= 48) return 0;
  // Ramp up from 2 to 127 over velocity range 49-127
  return (byte)map(velocity, 49, 127, 2, 127);
}

// ============================================================================
// myNoteOn / myNoteOff  (with Chase Play integrated)
// ----------------------------------------------------------------------------
// Chase Play is hooked in:
//   * myNoteOn  : after arp consume, before the keyMode switch
//   * myNoteOff : after arp consume and hold-latch return, before keyMode switch
//
// chasePlayNoteOn / chasePlayNoteOff return true when chase took ownership
// of the event — in that case we fall through past the normal Dual branch.
// ============================================================================

void myNoteOn(byte channel, byte note, byte velocity) {

  prevNote = note;

  // Per-patch split zones (JX-10 convention):
  //   note plays the LOWER section if note <= lowerSplitPoint
  //   note plays the UPPER section if note >= upperSplitPoint
  bool inLowerZone = (note <= lowerSplitPoint);
  bool inUpperZone = (note >= upperSplitPoint);

  // --- Key down tracking ---
  if (keyMode == 3) {
    if (inLowerZone) {
      keyDownLower[note] = true;
      holdLatchedLower[note] = false;
    }
    if (inUpperZone) {
      keyDownUpper[note] = true;
      holdLatchedUpper[note] = false;
    }
  } else if (keyMode == 0) {
    keyDownLower[note] = true;
    keyDownUpper[note] = true;
    holdLatchedLower[note] = false;
    holdLatchedUpper[note] = false;
  } else {
    keyDownWhole[note] = true;
    keyDownLower[note] = true;
    keyDownUpper[note] = true;
    holdLatchedLower[note] = false;
    holdLatchedUpper[note] = false;
  }

  // --- ARP: capture note ---
  bool arpConsumed = false;
  if (arpEnabled || arpLatch) {
    bool captureNote = false;
    if (keyMode == 3) {
      if (inLowerZone) captureNote = true;
    } else {
      captureNote = true;
    }
    if (captureNote) {
      arpAddNote(note);
      arpCurrentVel = velocity;
      arpBuildStepList();
      if (!arpRunning && arpLen > 0) {
        arpRunning = true;
        arpPos = -1;
        arpNextStepUs = micros();
        mcp9.digitalWrite(SEQ_START_STOP_LED_RED, HIGH);
      }
      arpConsumed = true;
    }
  }

  if (!arpConsumed) {

    // --- CHASE PLAY: intercepts Dual-mode notes when chasePlay is on ---
    if (chasePlayNoteOn(note, velocity)) {
      return;
    }

    int voiceNum = -1;

    switch (keyMode) {

      case 0:
        {
          if (lowerRealAssign == 1) {
            int lowerVoice = getLowerSplitVoicePoly2(note);
            int oldNote = voiceToNoteLower[lowerVoice];
            if (oldNote >= 0) {
              releaseVoice(oldNote, lowerVoice);
              voiceAssignmentLower[oldNote] = -1;
            }
            assignVoice(note, velocity, lowerVoice);
            voiceAssignmentLower[note] = lowerVoice;
            voiceToNoteLower[lowerVoice] = note;
          } else if (lowerRealAssign == 0) {
            int lowerVoice = getLowerSplitVoice(note);
            assignVoice(note, velocity, lowerVoice);
            voiceAssignmentLower[note] = lowerVoice;
            voiceToNoteLower[lowerVoice] = note;
          } else if (lowerRealAssign == 2) {
            commandMonoNoteOnLower(note, velocity);
          } else if (lowerRealAssign == 3) {
            commandUnisonNoteOnLower(note, velocity);
          } else if (lowerRealAssign == 4) {
            assignUnisonPairLower(note, velocity, false);
          } else if (lowerRealAssign == 5) {
            assignUnisonPairLower(note, velocity, true);
          }

          if (upperRealAssign == 1) {
            int upperVoice = getUpperSplitVoicePoly2(note);
            int oldNote = voiceToNoteUpper[upperVoice - 6];
            if (oldNote >= 0) {
              releaseVoice(oldNote, upperVoice);
              voiceAssignmentUpper[oldNote] = -1;
            }
            assignVoice(note, velocity, upperVoice);
            voiceAssignmentUpper[note] = upperVoice;
            voiceToNoteUpper[upperVoice - 6] = note;
          } else if (upperRealAssign == 0) {
            int upperVoice = getUpperSplitVoice(note);
            assignVoice(note, velocity, upperVoice);
            voiceAssignmentUpper[note] = upperVoice;
            voiceToNoteUpper[upperVoice - 6] = note;
          } else if (upperRealAssign == 2) {
            commandMonoNoteOnUpper(note, velocity);
          } else if (upperRealAssign == 3) {
            commandUnisonNoteOnUpper(note, velocity);
          } else if (upperRealAssign == 4) {
            assignUnisonPairUpper(note, velocity, false);
          } else if (upperRealAssign == 5) {
            assignUnisonPairUpper(note, velocity, true);
          }
        }
        break;

      case 1:
        switch (lowerRealAssign) {
          case 0:
            voiceNum = getVoiceNo(-1) - 1;
            assignVoice(note, velocity, voiceNum);
            break;
          case 1:
            voiceNum = getVoiceNoPoly2(-1) - 1;
            assignVoice(note, velocity, voiceNum);
            break;
          case 2: commandMonoNoteOn(note, velocity); break;
          case 3: commandUnisonNoteOn(note, velocity); break;
          case 4: assignWholeUnisonPair(note, velocity, false); break;
          case 5: assignWholeUnisonPair(note, velocity, true); break;
        }
        voiceAssignment[note] = voiceNum;
        break;

      case 2:
        switch (upperRealAssign) {
          case 0:
            voiceNum = getVoiceNo(-1) - 1;
            assignVoice(note, velocity, voiceNum);
            break;
          case 1:
            voiceNum = getVoiceNoPoly2(-1) - 1;
            assignVoice(note, velocity, voiceNum);
            break;
          case 2: commandMonoNoteOn(note, velocity); break;
          case 3: commandUnisonNoteOn(note, velocity); break;
          case 4: assignWholeUnisonPair(note, velocity, false); break;
          case 5: assignWholeUnisonPair(note, velocity, true); break;
        }
        voiceAssignment[note] = voiceNum;
        break;

      case 3:
        if (inLowerZone) {
          switch (lowerRealAssign) {
            case 0:
              voiceNum = getLowerSplitVoice(note);
              assignVoice(note, velocity, voiceNum);
              voiceAssignmentLower[note] = voiceNum;
              voiceToNoteLower[voiceNum] = note;
              break;
            case 1:
              voiceNum = getLowerSplitVoicePoly2(note);
              assignVoice(note, velocity, voiceNum);
              voiceAssignmentLower[note] = voiceNum;
              voiceToNoteLower[voiceNum] = note;
              break;
            case 2: commandMonoNoteOnLower(note, velocity); break;
            case 3: commandUnisonNoteOnLower(note, velocity); break;
            case 4: assignUnisonPairLower(note, velocity, false); break;
            case 5: assignUnisonPairLower(note, velocity, true); break;
          }
        }
        if (inUpperZone) {
          switch (upperRealAssign) {
            case 0:
              voiceNum = getUpperSplitVoice(note);
              assignVoice(note, velocity, voiceNum);
              voiceAssignmentUpper[note] = voiceNum;
              voiceToNoteUpper[voiceNum - 6] = note;
              break;
            case 1:
              voiceNum = getUpperSplitVoicePoly2(note);
              assignVoice(note, velocity, voiceNum);
              voiceAssignmentUpper[note] = voiceNum;
              voiceToNoteUpper[voiceNum - 6] = note;
              break;
            case 2: commandMonoNoteOnUpper(note, velocity); break;
            case 3: commandUnisonNoteOnUpper(note, velocity); break;
            case 4: assignUnisonPairUpper(note, velocity, false); break;
            case 5: assignUnisonPairUpper(note, velocity, true); break;
          }
        }
        break;

      case 4:
        if (velocity < upperSplitPoint) {
          switch (upperRealAssign) {
            case 0:
              voiceNum = getLowerSplitVoice(note);
              assignVoice(note, velocity, voiceNum);
              voiceAssignmentLower[note] = voiceNum;
              voiceToNoteLower[voiceNum] = note;
              break;
            case 1:
              voiceNum = getLowerSplitVoicePoly2(note);
              assignVoice(note, velocity, voiceNum);
              voiceAssignmentLower[note] = voiceNum;
              voiceToNoteLower[voiceNum] = note;
              break;
            case 2: commandMonoNoteOnLower(note, velocity); break;
            case 3: commandUnisonNoteOnLower(note, velocity); break;
            case 4: assignUnisonPairLower(note, velocity, false); break;
            case 5: assignUnisonPairLower(note, velocity, true); break;
          }
        } else {
          switch (upperRealAssign) {
            case 0:
              voiceNum = getUpperSplitVoice(note);
              assignVoice(note, velocity, voiceNum);
              voiceAssignmentUpper[note] = voiceNum;
              voiceToNoteUpper[voiceNum - 6] = note;
              break;
            case 1:
              voiceNum = getUpperSplitVoicePoly2(note);
              assignVoice(note, velocity, voiceNum);
              voiceAssignmentUpper[note] = voiceNum;
              voiceToNoteUpper[voiceNum - 6] = note;
              break;
            case 2: commandMonoNoteOnUpper(note, velocity); break;
            case 3: commandUnisonNoteOnUpper(note, velocity); break;
            case 4: assignUnisonPairUpper(note, velocity, false); break;
            case 5: assignUnisonPairUpper(note, velocity, true); break;
          }
        }
        voiceAssignment[note] = voiceNum;
        break;

      case 5:
        {
          byte lowerVel = xfadeLowerVel(velocity);
          byte upperVel = xfadeUpperVel(velocity);
          if (lowerVel > 0) {
            switch (upperRealAssign) {
              case 0:
                {
                  int v = getLowerSplitVoice(note);
                  assignVoice(note, lowerVel, v);
                  voiceAssignmentLower[note] = v;
                  voiceToNoteLower[v] = note;
                }
                break;
              case 1:
                {
                  int v = getLowerSplitVoicePoly2(note);
                  int old = voiceToNoteLower[v];
                  if (old >= 0) {
                    releaseVoice(old, v);
                    voiceAssignmentLower[old] = -1;
                  }
                  assignVoice(note, lowerVel, v);
                  voiceAssignmentLower[note] = v;
                  voiceToNoteLower[v] = note;
                }
                break;
              case 2: commandMonoNoteOnLower(note, lowerVel); break;
              case 3: commandUnisonNoteOnLower(note, lowerVel); break;
              case 4: assignUnisonPairLower(note, lowerVel, false); break;
              case 5: assignUnisonPairLower(note, lowerVel, true); break;
            }
          }
          if (upperVel > 0) {
            switch (upperRealAssign) {
              case 0:
                {
                  int v = getUpperSplitVoice(note);
                  assignVoice(note, upperVel, v);
                  voiceAssignmentUpper[note] = v;
                  voiceToNoteUpper[v - 6] = note;
                }
                break;
              case 1:
                {
                  int v = getUpperSplitVoicePoly2(note);
                  int old = voiceToNoteUpper[v - 6];
                  if (old >= 0) {
                    releaseVoice(old, v);
                    voiceAssignmentUpper[old] = -1;
                  }
                  assignVoice(note, upperVel, v);
                  voiceAssignmentUpper[note] = v;
                  voiceToNoteUpper[v - 6] = note;
                }
                break;
              case 2: commandMonoNoteOnUpper(note, upperVel); break;
              case 3: commandUnisonNoteOnUpper(note, upperVel); break;
              case 4: assignUnisonPairUpper(note, upperVel, false); break;
              case 5: assignUnisonPairUpper(note, upperVel, true); break;
            }
          }
        }
        voiceAssignment[note] = voiceNum;
        break;
    }
  }
}


void myNoteOff(byte channel, byte note, byte velocity) {

  auto holdEffectiveLower = [&]() -> bool {
    if (keyMode == 3) return (holdManualLower || holdPedal);
    return (holdManualLower || holdManualUpper || holdPedal);
  };
  auto holdEffectiveUpper = [&]() -> bool {
    if (keyMode == 3) return (holdManualUpper);
    return (holdManualLower || holdManualUpper || holdPedal);
  };

  bool inLowerZone = (note <= lowerSplitPoint);
  bool inUpperZone = (note >= upperSplitPoint);

  // Key down tracking
  if (keyMode == 3) {
    if (inLowerZone) keyDownLower[note] = false;
    if (inUpperZone) keyDownUpper[note] = false;
  } else if (keyMode == 0) {
    keyDownLower[note] = false;
    keyDownUpper[note] = false;
  } else {
    keyDownWhole[note] = false;
    keyDownLower[note] = false;
    keyDownUpper[note] = false;
  }

  // --- ARP: remove note ---
  bool arpConsumed = false;
  if (arpEnabled || arpLatch) {
    bool captureNote = false;
    if (keyMode == 3) {
      if (inLowerZone) captureNote = true;
    } else {
      captureNote = true;
    }
    if (captureNote) {
      arpRemoveNote(note);
      arpBuildStepList();
      if (!arpLatch && arpLen == 0) {
        arpStopCurrentNote();
        arpRunning = false;
        mcp9.digitalWrite(SEQ_START_STOP_LED_RED, LOW);
      }
      arpConsumed = true;
    }
  }

  if (arpConsumed) return;

  // Hold latching
  if (keyMode == 3) {
    bool latched = false;
    if (inLowerZone && holdEffectiveLower()) {
      holdLatchedLower[note] = true;
      latched = true;
    }
    if (inUpperZone && holdEffectiveUpper()) {
      holdLatchedUpper[note] = true;
      latched = true;
    }
    if (latched) return;
  } else {
    if (holdEffectiveLower()) {
      holdLatchedLower[note] = true;
      holdLatchedUpper[note] = true;
      return;
    }
  }

  // --- CHASE PLAY: intercepts Dual-mode releases when chasePlay is on ---
  if (chasePlayNoteOff(note)) {
    return;
  }

  int assignedVoice = voiceAssignment[note];

  switch (keyMode) {
    case 0:
      {
        if (lowerRealAssign == 2) commandMonoNoteOffLower(note);
        else if (lowerRealAssign == 3) commandUnisonNoteOffLower(note);
        else if (lowerRealAssign == 4 || lowerRealAssign == 5) releaseUnisonPairLower(note);
        else {
          int lowerVoice = voiceAssignmentLower[note];
          if (lowerVoice >= 0 && lowerVoice <= 5 && voiceToNoteLower[lowerVoice] == note) {
            releaseVoice(note, lowerVoice);
            voiceAssignmentLower[note] = -1;
            voiceToNoteLower[lowerVoice] = -1;
          }
        }
        if (upperRealAssign == 2) commandMonoNoteOffUpper(note);
        else if (upperRealAssign == 3) commandUnisonNoteOffUpper(note);
        else if (upperRealAssign == 4 || upperRealAssign == 5) releaseUnisonPairUpper(note);
        else {
          int upperVoice = voiceAssignmentUpper[note];
          if (upperVoice >= 6 && upperVoice <= 11 && voiceToNoteUpper[upperVoice - 6] == note) {
            releaseVoice(note, upperVoice);
            voiceAssignmentUpper[note] = -1;
            voiceToNoteUpper[upperVoice - 6] = -1;
          }
        }
      }
      break;

    case 1:
      switch (lowerRealAssign) {
        case 0:
          assignedVoice = getVoiceNo(note) - 1;
          releaseVoice(note, assignedVoice);
          break;
        case 1:
          assignedVoice = getVoiceNoPoly2(note) - 1;
          releaseVoice(note, assignedVoice);
          break;
        case 2: commandMonoNoteOff(note); break;
        case 3: commandUnisonNoteOff(note); break;
        case 4:
        case 5: releaseWholeUnisonPair(note); break;
      }
      break;

    case 2:
      switch (upperRealAssign) {
        case 0:
          assignedVoice = getVoiceNo(note) - 1;
          releaseVoice(note, assignedVoice);
          break;
        case 1:
          assignedVoice = getVoiceNoPoly2(note) - 1;
          releaseVoice(note, assignedVoice);
          break;
        case 2: commandMonoNoteOff(note); break;
        case 3: commandUnisonNoteOff(note); break;
        case 4:
        case 5: releaseWholeUnisonPair(note); break;
      }
      break;

    case 3:
      {
        if (inLowerZone) {
          if (lowerRealAssign == 2) commandMonoNoteOffLower(note);
          else if (lowerRealAssign == 3) commandUnisonNoteOffLower(note);
          else if (lowerRealAssign == 4 || lowerRealAssign == 5) releaseUnisonPairLower(note);
          else {
            int lowerVoice = voiceAssignmentLower[note];
            if (lowerVoice >= 0 && lowerVoice <= 5 && voiceToNoteLower[lowerVoice] == note) {
              releaseVoice(note, lowerVoice);
              voiceAssignmentLower[note] = -1;
              voiceToNoteLower[lowerVoice] = -1;
            }
          }
        }
        if (inUpperZone) {
          if (upperRealAssign == 2) commandMonoNoteOffUpper(note);
          else if (upperRealAssign == 3) commandUnisonNoteOffUpper(note);
          else if (upperRealAssign == 4 || upperRealAssign == 5) releaseUnisonPairUpper(note);
          else {
            int upperVoice = voiceAssignmentUpper[note];
            if (upperVoice >= 6 && upperVoice <= 11 && voiceToNoteUpper[upperVoice - 6] == note) {
              releaseVoice(note, upperVoice);
              voiceAssignmentUpper[note] = -1;
              voiceToNoteUpper[upperVoice - 6] = -1;
            }
          }
        }
      }
      break;

    case 4:
      switch (upperRealAssign) {
        case 2:
          commandMonoNoteOffLower(note);
          commandMonoNoteOffUpper(note);
          break;
        case 3:
          commandUnisonNoteOffLower(note);
          commandUnisonNoteOffUpper(note);
          break;
        case 4:
        case 5:
          releaseUnisonPairLower(note);
          releaseUnisonPairUpper(note);
          break;
        default:
          {
            int lowerVoice = voiceAssignmentLower[note];
            if (lowerVoice >= 0 && lowerVoice <= 5 && voiceToNoteLower[lowerVoice] == note) {
              releaseVoice(note, lowerVoice);
              voiceAssignmentLower[note] = -1;
              voiceToNoteLower[lowerVoice] = -1;
            }
            int upperVoice = voiceAssignmentUpper[note];
            if (upperVoice >= 6 && upperVoice <= 11 && voiceToNoteUpper[upperVoice - 6] == note) {
              releaseVoice(note, upperVoice);
              voiceAssignmentUpper[note] = -1;
              voiceToNoteUpper[upperVoice - 6] = -1;
            }
          }
          break;
      }
      break;

    case 5:
      {
        switch (upperRealAssign) {
          case 2: commandMonoNoteOffLower(note); break;
          case 3: commandUnisonNoteOffLower(note); break;
          case 4:
          case 5: releaseUnisonPairLower(note); break;
          default:
            {
              int lowerVoice = voiceAssignmentLower[note];
              if (lowerVoice >= 0 && lowerVoice <= 5 && voiceToNoteLower[lowerVoice] == note) {
                releaseVoice(note, lowerVoice);
                voiceAssignmentLower[note] = -1;
                voiceToNoteLower[lowerVoice] = -1;
              }
            }
            break;
        }
        switch (upperRealAssign) {
          case 2: commandMonoNoteOffUpper(note); break;
          case 3: commandUnisonNoteOffUpper(note); break;
          case 4:
          case 5: releaseUnisonPairUpper(note); break;
          default:
            {
              int upperVoice = voiceAssignmentUpper[note];
              if (upperVoice >= 6 && upperVoice <= 11 && voiceToNoteUpper[upperVoice - 6] == note) {
                releaseVoice(note, upperVoice);
                voiceAssignmentUpper[note] = -1;
                voiceToNoteUpper[upperVoice - 6] = -1;
              }
            }
            break;
        }
      }
      break;
  }
}

void commandMonoNoteOn(byte note, byte velocity) {
  notesWhole[note] = true;
  noteMsg = note;
  noteVel = velocity;
  orderIndxWhole = (orderIndxWhole + 1) % 40;
  noteOrderWhole[orderIndxWhole] = note;
  commandLastNoteWhole();
}

void commandMonoNoteOff(byte note) {
  notesWhole[note] = false;
  noteMsg = note;
  commandLastNoteWhole();
}

void commandLastNoteWhole() {
  for (int i = 0; i < 40; i++) {
    int8_t idx = noteOrderWhole[mod(orderIndxWhole - i, 40)];
    if (notesWhole[idx]) {
      assignVoice(idx, noteVel, 0);
      return;
    }
  }
  releaseVoice(noteMsg, 0);
}

void commandUnisonNoteOn(byte note, byte velocity) {
  notesWhole[note] = true;
  noteMsg = note;
  noteVel = velocity;
  orderIndxWhole = (orderIndxWhole + 1) % 40;
  noteOrderWhole[orderIndxWhole] = note;
  commandLastNoteUniWhole();
}

void commandUnisonNoteOff(byte note) {
  notesWhole[note] = false;
  noteMsg = note;
  commandLastNoteUniWhole();
}

void commandLastNoteUniWhole() {
  for (int i = 0; i < 40; i++) {
    int8_t idx = noteOrderWhole[mod(orderIndxWhole - i, 40)];
    if (notesWhole[idx]) {
      for (int v = 0; v < 12; v++) assignVoice(idx, noteVel, v);
      return;
    }
  }
  for (int v = 0; v < 12; v++) releaseVoice(noteMsg, v);
}

// ---------------------------------------------------------------------------
// Recall a patch from SD
// ---------------------------------------------------------------------------
void recallPatch(int bank, int group, int slot) {
  allNotesOff();
  delay(50);
  String path = getPatchPath(bank, group, slot);
  File patchFile = SD.open(path.c_str());
  if (!patchFile) {
    Serial.print(F("Patch not found: "));
    Serial.println(path);
    showPatchPage(getPatchLabel(group, slot), "(empty)");
    return;
  }
  String data[NO_OF_PARAMS];
  recallPatchData(patchFile, data);
  setCurrentPatchData(data);
  patchFile.close();

  // Reload tone library if bank has changed
  if (bank != currentBank) {
    loadToneLibrary(bank);
  }

  currentBank = bank;
  currentGroup = group;
  currentSlot = slot;
  showPatchPage(getPatchLabel(group, slot), patchName);
  updateAssignLeds();
}

// Recall using current position
void recallCurrentPatch() {
  recallPatch(currentBank, currentGroup, currentSlot);
}

// ---------------------------------------------------------------------------
// Boot: load first patch (bank 0, group A=0, slot 1)
// or restore from EEPROM if you add that later
// ---------------------------------------------------------------------------
void loadInitialPatch() {
  loadToneLibrary(0);
  recallPatch(0, 0, 1);
}

void allNotesOff() {

  sendVoiceParam(kPrefixBroadcast, NO_OFFSET, 0xB6, 0x7F);
  sendVoiceParam(kPrefixBroadcast, NO_OFFSET, 0xBB, 0x00);
  sendVoiceParam(kPrefixBroadcast, NO_OFFSET, 0xB5, 0x00);

  MIDI.sendControlChange(0x7B, 0x7F, controlChannel);
  MIDI.sendControlChange(0x7B, 0x00, controlChannel);

  voices[0].note = -1;
  voiceOn[0] = false;


  voices[1].note = -1;
  voiceOn[1] = false;


  voices[2].note = -1;
  voiceOn[2] = false;


  voices[3].note = -1;
  voiceOn[3] = false;


  voices[4].note = -1;
  voiceOn[4] = false;


  voices[5].note = -1;
  voiceOn[5] = false;


  voices[6].note = -1;
  voiceOn[6] = false;


  voices[7].note = -1;
  voiceOn[7] = false;

  voices[8].note = -1;
  voiceOn[8] = false;


  voices[9].note = -1;
  voiceOn[9] = false;


  voices[10].note = -1;
  voiceOn[10] = false;


  voices[11].note = -1;
  voiceOn[11] = false;
}

// -------------------------------------------------------------
// Tone handling
// -------------------------------------------------------------

// Add to loop()
void updateToneBlink() {
  if (!toneEntryActive) return;
  if (millis() - toneBlinkTimer > TONE_BLINK_INTERVAL) {
    toneBlinkState = !toneBlinkState;
    toneBlinkTimer = millis();

    uint8_t display = (toneEntryBuffer == 0) ? 100 : toneEntryBuffer;

    if (upperSW) {
      lcd.setCursor(36, 0);
    } else {
      lcd.setCursor(36, 1);
    }

    if (toneBlinkState) {
      if (display < 10) {
        lcd.print("  ");
        lcd.print(display);
      } else if (display < 100) {
        lcd.print(" ");
        lcd.print(display);
      } else {
        lcd.print(display);
      }
    } else {
      lcd.print("   ");  // blank - same width as "100"
    }
  }
}

void handleToneDigit(uint8_t digit) {
  toneEntryBuffer = (toneEntryBuffer % 10) * 10 + digit;
  toneEntryActive = true;
  toneBlinkTimer = 0;     // force immediate visible update on next blink cycle
  toneBlinkState = true;  // start on the visible phase

  // Write the number immediately so it shows before first blink interval
  uint8_t display = (toneEntryBuffer == 0) ? 100 : toneEntryBuffer;
  if (upperSW) {
    lcd.setCursor(36, 0);
  } else {
    lcd.setCursor(36, 1);
  }
  if (display < 10) {
    lcd.print("  ");
    lcd.print(display);
  } else if (display < 100) {
    lcd.print(" ");
    lcd.print(display);
  } else {
    lcd.print(display);
  }
}

// ----------------------------------------------------------------------
// Patch Data
// ----------------------------------------------------------------------

String getCurrentPatchData() {
  return patchName + "," + String(upperData[P_lfo1_wave]) + "," + String(lowerData[P_lfo1_wave]) + "," + String(upperData[P_lfo1_rate]) + "," + String(lowerData[P_lfo1_rate]) + "," + String(upperData[P_lfo1_delay]) + "," + String(lowerData[P_lfo1_delay])
         + "," + String(upperData[P_lfo1_lfo2]) + "," + String(lowerData[P_lfo1_lfo2]) + "," + String(upperData[P_lfo1_sync]) + "," + String(lowerData[P_lfo1_sync]) + "," + String(upperData[P_lfo2_wave]) + "," + String(lowerData[P_lfo2_wave])
         + "," + String(upperData[P_lfo2_rate]) + "," + String(lowerData[P_lfo2_rate]) + "," + String(upperData[P_lfo2_delay]) + "," + String(lowerData[P_lfo2_delay]) + "," + String(upperData[P_lfo2_lfo1]) + "," + String(lowerData[P_lfo2_lfo1])
         + "," + String(upperData[P_lfo2_sync]) + "," + String(lowerData[P_lfo2_sync]) + "," + String(upperData[P_dco1_PW]) + "," + String(lowerData[P_dco1_PW]) + "," + String(upperData[P_dco1_PWM_env]) + "," + String(lowerData[P_dco1_PWM_env])
         + "," + String(upperData[P_dco1_PWM_lfo]) + "," + String(lowerData[P_dco1_PWM_lfo]) + "," + String(upperData[P_dco1_PWM_dyn]) + "," + String(lowerData[P_dco1_PWM_dyn]) + "," + String(upperData[P_dco1_PWM_env_source]) + "," + String(lowerData[P_dco1_PWM_env_source])
         + "," + String(upperData[P_dco1_PWM_env_pol]) + "," + String(lowerData[P_dco1_PWM_env_pol]) + "," + String(upperData[P_dco1_PWM_lfo_source]) + "," + String(lowerData[P_dco1_PWM_lfo_source]) + "," + String(upperData[P_dco1_pitch_env]) + "," + String(lowerData[P_dco1_pitch_env])
         + "," + String(upperData[P_dco1_pitch_env_source]) + "," + String(lowerData[P_dco1_pitch_env_source]) + "," + String(upperData[P_dco1_pitch_env_pol]) + "," + String(lowerData[P_dco1_pitch_env_pol]) + "," + String(upperData[P_dco1_pitch_lfo]) + "," + String(lowerData[P_dco1_pitch_lfo])
         + "," + String(upperData[P_dco1_pitch_lfo_source]) + "," + String(lowerData[P_dco1_pitch_lfo_source]) + "," + String(upperData[P_dco1_pitch_dyn]) + "," + String(lowerData[P_dco1_pitch_dyn]) + "," + String(upperData[P_dco1_wave]) + "," + String(lowerData[P_dco1_wave])
         + "," + String(upperData[P_dco1_range]) + "," + String(lowerData[P_dco1_range]) + "," + String(upperData[P_dco1_tune]) + "," + String(lowerData[P_dco1_tune]) + "," + String(upperData[P_dco1_mode]) + "," + String(lowerData[P_dco1_mode])
         + "," + String(upperData[P_dco2_PW]) + "," + String(lowerData[P_dco2_PW]) + "," + String(upperData[P_dco2_PWM_env]) + "," + String(lowerData[P_dco2_PWM_env]) + "," + String(upperData[P_dco2_PWM_lfo]) + "," + String(lowerData[P_dco2_PWM_lfo])
         + "," + String(upperData[P_dco2_PWM_dyn]) + "," + String(lowerData[P_dco2_PWM_dyn]) + "," + String(upperData[P_dco2_PWM_env_source]) + "," + String(lowerData[P_dco2_PWM_env_source]) + "," + String(upperData[P_dco2_PWM_env_pol]) + "," + String(lowerData[P_dco2_PWM_env_pol])
         + "," + String(upperData[P_dco2_PWM_lfo_source]) + "," + String(lowerData[P_dco2_PWM_lfo_source]) + "," + String(upperData[P_dco2_pitch_env]) + "," + String(lowerData[P_dco2_pitch_env]) + "," + String(upperData[P_dco2_pitch_env_source]) + "," + String(lowerData[P_dco2_pitch_env_source])
         + "," + String(upperData[P_dco2_pitch_env_pol]) + "," + String(lowerData[P_dco2_pitch_env_pol]) + "," + String(upperData[P_dco2_pitch_lfo]) + "," + String(lowerData[P_dco2_pitch_lfo]) + "," + String(upperData[P_dco2_pitch_lfo_source]) + "," + String(lowerData[P_dco2_pitch_lfo_source])
         + "," + String(upperData[P_dco2_pitch_dyn]) + "," + String(lowerData[P_dco2_pitch_dyn]) + "," + String(upperData[P_dco2_wave]) + "," + String(lowerData[P_dco2_wave]) + "," + String(upperData[P_dco2_range]) + "," + String(lowerData[P_dco2_range])
         + "," + String(upperData[P_dco2_tune]) + "," + String(lowerData[P_dco2_tune]) + "," + String(upperData[P_dco2_fine]) + "," + String(lowerData[P_dco2_fine]) + "," + String(upperData[P_dco1_level]) + "," + String(lowerData[P_dco1_level])
         + "," + String(upperData[P_dco2_level]) + "," + String(lowerData[P_dco2_level]) + "," + String(upperData[P_dco2_mod]) + "," + String(lowerData[P_dco2_mod]) + "," + String(upperData[P_dco_mix_env_pol]) + "," + String(lowerData[P_dco_mix_env_pol])
         + "," + String(upperData[P_dco_mix_env_source]) + "," + String(lowerData[P_dco_mix_env_source]) + "," + String(upperData[P_dco_mix_dyn]) + "," + String(lowerData[P_dco_mix_dyn]) + "," + String(upperData[P_vcf_hpf]) + "," + String(lowerData[P_vcf_hpf])
         + "," + String(upperData[P_vcf_cutoff]) + "," + String(lowerData[P_vcf_cutoff]) + "," + String(upperData[P_vcf_res]) + "," + String(lowerData[P_vcf_res]) + "," + String(upperData[P_vcf_kb]) + "," + String(lowerData[P_vcf_kb])
         + "," + String(upperData[P_vcf_env]) + "," + String(lowerData[P_vcf_env]) + "," + String(upperData[P_vcf_lfo1]) + "," + String(lowerData[P_vcf_lfo1]) + "," + String(upperData[P_vcf_lfo2]) + "," + String(lowerData[P_vcf_lfo2])
         + "," + String(upperData[P_vcf_env_source]) + "," + String(lowerData[P_vcf_env_source]) + "," + String(upperData[P_vcf_env_pol]) + "," + String(lowerData[P_vcf_env_pol]) + "," + String(upperData[P_vcf_dyn]) + "," + String(lowerData[P_vcf_dyn])
         + "," + String(upperData[P_vca_mod]) + "," + String(lowerData[P_vca_mod]) + "," + String(upperData[P_vca_env_source]) + "," + String(lowerData[P_vca_env_source]) + "," + String(upperData[P_vca_dyn]) + "," + String(lowerData[P_vca_dyn])
         + "," + String(upperData[P_time1]) + "," + String(lowerData[P_time1]) + "," + String(upperData[P_level1]) + "," + String(lowerData[P_level1]) + "," + String(upperData[P_time2]) + "," + String(lowerData[P_time2])
         + "," + String(upperData[P_level2]) + "," + String(lowerData[P_level2]) + "," + String(upperData[P_time3]) + "," + String(lowerData[P_time3]) + "," + String(upperData[P_level3]) + "," + String(lowerData[P_level3])
         + "," + String(upperData[P_time4]) + "," + String(lowerData[P_time4]) + "," + String(upperData[P_env5stage_mode]) + "," + String(lowerData[P_env5stage_mode]) + "," + String(upperData[P_env2_time1]) + "," + String(lowerData[P_env2_time1])
         + "," + String(upperData[P_env2_level1]) + "," + String(lowerData[P_env2_level1]) + "," + String(upperData[P_env2_time2]) + "," + String(lowerData[P_env2_time2]) + "," + String(upperData[P_env2_level2]) + "," + String(lowerData[P_env2_level2])
         + "," + String(upperData[P_env2_time3]) + "," + String(lowerData[P_env2_time3]) + "," + String(upperData[P_env2_level3]) + "," + String(lowerData[P_env2_level3]) + "," + String(upperData[P_env2_time4]) + "," + String(lowerData[P_env2_time4])
         + "," + String(upperData[P_env2_5stage_mode]) + "," + String(lowerData[P_env2_5stage_mode]) + "," + String(upperData[P_attack]) + "," + String(lowerData[P_attack]) + "," + String(upperData[P_decay]) + "," + String(lowerData[P_decay])
         + "," + String(upperData[P_sustain]) + "," + String(lowerData[P_sustain]) + "," + String(upperData[P_release]) + "," + String(lowerData[P_release]) + "," + String(upperData[P_adsr_mode]) + "," + String(lowerData[P_adsr_mode])
         + "," + String(upperData[P_env4_attack]) + "," + String(lowerData[P_env4_attack]) + "," + String(upperData[P_env4_decay]) + "," + String(lowerData[P_env4_decay]) + "," + String(upperData[P_env4_sustain]) + "," + String(lowerData[P_env4_sustain])
         + "," + String(upperData[P_env4_release]) + "," + String(lowerData[P_env4_release]) + "," + String(upperData[P_env4_adsr_mode]) + "," + String(lowerData[P_env4_adsr_mode]) + "," + String(upperData[P_chorus]) + "," + String(lowerData[P_chorus])
         + "," + String(keyMode) + "," + String(dualdetune)
         + "," + String(at_vib) + "," + String(at_bri) + "," + String(at_vol) + "," + String(balance) + "," + String(portamento) + "," + String(volume)
         + "," + String(bend_range) + "," + String(chaseLevel) + "," + String(chaseMode) + "," + String(chaseTime) + "," + String(chasePlay)
         + "," + String(upperSplitPoint) + "," + String(lowerSplitPoint) + "," + String(upperToneNumber) + "," + String(lowerToneNumber) + "," + String(upperChromatic) + "," + String(lowerChromatic)
         + "," + String(upperAssign) + "," + String(lowerAssign) + "," + String(upperUnisonDetune) + "," + String(lowerUnisonDetune) + "," + String(upperHold) + "," + String(lowerHold)
         + "," + String(upperLFOModDepth) + "," + String(lowerLFOModDepth) + "," + String(upperPortamento_SW) + "," + String(lowerPortamento_SW) + "," + String(upperbend_enable_SW) + "," + String(lowerbend_enable_SW)
         + "," + String(keyMode2) + "," + String(bend_range2) + "," + String(at_vib_sw) + "," + String(at_bri_sw) + "," + String(at_vol_sw);
}

void setCurrentPatchData(String data[]) {
  patchName = data[0];

  // Tone data

  upperData[P_lfo1_wave] = data[1].toInt();
  lowerData[P_lfo1_wave] = data[2].toInt();
  upperData[P_lfo1_rate] = data[3].toInt();
  lowerData[P_lfo1_rate] = data[4].toInt();
  upperData[P_lfo1_delay] = data[5].toInt();
  lowerData[P_lfo1_delay] = data[6].toInt();
  upperData[P_lfo1_lfo2] = data[7].toInt();
  lowerData[P_lfo1_lfo2] = data[8].toInt();
  upperData[P_lfo1_sync] = data[9].toInt();
  lowerData[P_lfo1_sync] = data[10].toInt();
  upperData[P_lfo2_wave] = data[11].toInt();
  lowerData[P_lfo2_wave] = data[12].toInt();
  upperData[P_lfo2_rate] = data[13].toInt();
  lowerData[P_lfo2_rate] = data[14].toInt();
  upperData[P_lfo2_delay] = data[15].toInt();
  lowerData[P_lfo2_delay] = data[16].toInt();
  upperData[P_lfo2_lfo1] = data[17].toInt();
  lowerData[P_lfo2_lfo1] = data[18].toInt();
  upperData[P_lfo2_sync] = data[19].toInt();
  lowerData[P_lfo2_sync] = data[20].toInt();

  upperData[P_dco1_PW] = data[21].toInt();
  lowerData[P_dco1_PW] = data[22].toInt();
  upperData[P_dco1_PWM_env] = data[23].toInt();
  lowerData[P_dco1_PWM_env] = data[24].toInt();
  upperData[P_dco1_PWM_lfo] = data[25].toInt();
  lowerData[P_dco1_PWM_lfo] = data[26].toInt();
  upperData[P_dco1_PWM_dyn] = data[27].toInt();
  lowerData[P_dco1_PWM_dyn] = data[28].toInt();
  upperData[P_dco1_PWM_env_source] = data[29].toInt();
  lowerData[P_dco1_PWM_env_source] = data[30].toInt();
  upperData[P_dco1_PWM_env_pol] = data[31].toInt();
  lowerData[P_dco1_PWM_env_pol] = data[32].toInt();
  upperData[P_dco1_PWM_lfo_source] = data[33].toInt();
  lowerData[P_dco1_PWM_lfo_source] = data[34].toInt();
  upperData[P_dco1_pitch_env] = data[35].toInt();
  lowerData[P_dco1_pitch_env] = data[36].toInt();
  upperData[P_dco1_pitch_env_source] = data[37].toInt();
  lowerData[P_dco1_pitch_env_source] = data[38].toInt();
  upperData[P_dco1_pitch_env_pol] = data[39].toInt();
  lowerData[P_dco1_pitch_env_pol] = data[40].toInt();
  upperData[P_dco1_pitch_lfo] = data[41].toInt();
  lowerData[P_dco1_pitch_lfo] = data[42].toInt();
  upperData[P_dco1_pitch_lfo_source] = data[43].toInt();
  lowerData[P_dco1_pitch_lfo_source] = data[44].toInt();
  upperData[P_dco1_pitch_dyn] = data[45].toInt();
  lowerData[P_dco1_pitch_dyn] = data[46].toInt();
  upperData[P_dco1_wave] = data[47].toInt();
  lowerData[P_dco1_wave] = data[48].toInt();
  upperData[P_dco1_range] = data[49].toInt();
  lowerData[P_dco1_range] = data[50].toInt();
  upperData[P_dco1_tune] = data[51].toInt();
  lowerData[P_dco1_tune] = data[52].toInt();
  upperData[P_dco1_mode] = data[53].toInt();
  lowerData[P_dco1_mode] = data[54].toInt();

  upperData[P_dco2_PW] = data[55].toInt();
  lowerData[P_dco2_PW] = data[56].toInt();
  upperData[P_dco2_PWM_env] = data[57].toInt();
  lowerData[P_dco2_PWM_env] = data[58].toInt();
  upperData[P_dco2_PWM_lfo] = data[59].toInt();
  lowerData[P_dco2_PWM_lfo] = data[60].toInt();
  upperData[P_dco2_PWM_dyn] = data[61].toInt();
  lowerData[P_dco2_PWM_dyn] = data[62].toInt();
  upperData[P_dco2_PWM_env_source] = data[63].toInt();
  lowerData[P_dco2_PWM_env_source] = data[64].toInt();
  upperData[P_dco2_PWM_env_pol] = data[65].toInt();
  lowerData[P_dco2_PWM_env_pol] = data[66].toInt();
  upperData[P_dco2_PWM_lfo_source] = data[67].toInt();
  lowerData[P_dco2_PWM_lfo_source] = data[68].toInt();
  upperData[P_dco2_pitch_env] = data[69].toInt();
  lowerData[P_dco2_pitch_env] = data[70].toInt();
  upperData[P_dco2_pitch_env_source] = data[71].toInt();
  lowerData[P_dco2_pitch_env_source] = data[72].toInt();
  upperData[P_dco2_pitch_env_pol] = data[73].toInt();
  lowerData[P_dco2_pitch_env_pol] = data[74].toInt();
  upperData[P_dco2_pitch_lfo] = data[75].toInt();
  lowerData[P_dco2_pitch_lfo] = data[76].toInt();
  upperData[P_dco2_pitch_lfo_source] = data[77].toInt();
  lowerData[P_dco2_pitch_lfo_source] = data[78].toInt();
  upperData[P_dco2_pitch_dyn] = data[79].toInt();
  lowerData[P_dco2_pitch_dyn] = data[80].toInt();
  upperData[P_dco2_wave] = data[81].toInt();
  lowerData[P_dco2_wave] = data[82].toInt();
  upperData[P_dco2_range] = data[83].toInt();
  lowerData[P_dco2_range] = data[84].toInt();
  upperData[P_dco2_tune] = data[85].toInt();
  lowerData[P_dco2_tune] = data[86].toInt();
  upperData[P_dco2_fine] = data[87].toInt();
  lowerData[P_dco2_fine] = data[88].toInt();

  upperData[P_dco1_level] = data[89].toInt();
  lowerData[P_dco1_level] = data[90].toInt();
  upperData[P_dco2_level] = data[91].toInt();
  lowerData[P_dco2_level] = data[92].toInt();
  upperData[P_dco2_mod] = data[93].toInt();
  lowerData[P_dco2_mod] = data[94].toInt();
  upperData[P_dco_mix_env_pol] = data[95].toInt();
  lowerData[P_dco_mix_env_pol] = data[96].toInt();
  upperData[P_dco_mix_env_source] = data[97].toInt();
  lowerData[P_dco_mix_env_source] = data[98].toInt();
  upperData[P_dco_mix_dyn] = data[99].toInt();
  lowerData[P_dco_mix_dyn] = data[100].toInt();

  upperData[P_vcf_hpf] = data[101].toInt();
  lowerData[P_vcf_hpf] = data[102].toInt();
  upperData[P_vcf_cutoff] = data[103].toInt();
  lowerData[P_vcf_cutoff] = data[104].toInt();
  upperData[P_vcf_res] = data[105].toInt();
  lowerData[P_vcf_res] = data[106].toInt();
  upperData[P_vcf_kb] = data[107].toInt();
  lowerData[P_vcf_kb] = data[108].toInt();
  upperData[P_vcf_env] = data[109].toInt();
  lowerData[P_vcf_env] = data[110].toInt();
  upperData[P_vcf_lfo1] = data[111].toInt();
  lowerData[P_vcf_lfo1] = data[112].toInt();
  upperData[P_vcf_lfo2] = data[113].toInt();
  lowerData[P_vcf_lfo2] = data[114].toInt();
  upperData[P_vcf_env_source] = data[115].toInt();
  lowerData[P_vcf_env_source] = data[116].toInt();
  upperData[P_vcf_env_pol] = data[117].toInt();
  lowerData[P_vcf_env_pol] = data[118].toInt();
  upperData[P_vcf_dyn] = data[119].toInt();
  lowerData[P_vcf_dyn] = data[120].toInt();

  upperData[P_vca_mod] = data[121].toInt();
  lowerData[P_vca_mod] = data[122].toInt();
  upperData[P_vca_env_source] = data[123].toInt();
  lowerData[P_vca_env_source] = data[124].toInt();
  upperData[P_vca_dyn] = data[125].toInt();
  lowerData[P_vca_dyn] = data[126].toInt();

  upperData[P_time1] = data[127].toInt();
  lowerData[P_time1] = data[128].toInt();
  upperData[P_level1] = data[129].toInt();
  lowerData[P_level1] = data[130].toInt();
  upperData[P_time2] = data[131].toInt();
  lowerData[P_time2] = data[132].toInt();
  upperData[P_level2] = data[133].toInt();
  lowerData[P_level2] = data[134].toInt();
  upperData[P_time3] = data[135].toInt();
  lowerData[P_time3] = data[136].toInt();
  upperData[P_level3] = data[137].toInt();
  lowerData[P_level3] = data[138].toInt();
  upperData[P_time4] = data[139].toInt();
  lowerData[P_time4] = data[140].toInt();
  upperData[P_env5stage_mode] = data[141].toInt();
  lowerData[P_env5stage_mode] = data[142].toInt();

  upperData[P_env2_time1] = data[143].toInt();
  lowerData[P_env2_time1] = data[144].toInt();
  upperData[P_env2_level1] = data[145].toInt();
  lowerData[P_env2_level1] = data[146].toInt();
  upperData[P_env2_time2] = data[147].toInt();
  lowerData[P_env2_time2] = data[148].toInt();
  upperData[P_env2_level2] = data[149].toInt();
  lowerData[P_env2_level2] = data[150].toInt();
  upperData[P_env2_time3] = data[151].toInt();
  lowerData[P_env2_time3] = data[152].toInt();
  upperData[P_env2_level3] = data[153].toInt();
  lowerData[P_env2_level3] = data[154].toInt();
  upperData[P_env2_time4] = data[155].toInt();
  lowerData[P_env2_time4] = data[156].toInt();
  upperData[P_env2_5stage_mode] = data[157].toInt();
  lowerData[P_env2_5stage_mode] = data[158].toInt();

  upperData[P_attack] = data[159].toInt();
  lowerData[P_attack] = data[160].toInt();
  upperData[P_decay] = data[161].toInt();
  lowerData[P_decay] = data[162].toInt();
  upperData[P_sustain] = data[163].toInt();
  lowerData[P_sustain] = data[164].toInt();
  upperData[P_release] = data[165].toInt();
  lowerData[P_release] = data[166].toInt();
  upperData[P_adsr_mode] = data[167].toInt();
  lowerData[P_adsr_mode] = data[168].toInt();

  upperData[P_env4_attack] = data[169].toInt();
  lowerData[P_env4_attack] = data[170].toInt();
  upperData[P_env4_decay] = data[171].toInt();
  lowerData[P_env4_decay] = data[172].toInt();
  upperData[P_env4_sustain] = data[173].toInt();
  lowerData[P_env4_sustain] = data[174].toInt();
  upperData[P_env4_release] = data[175].toInt();
  lowerData[P_env4_release] = data[176].toInt();
  upperData[P_env4_adsr_mode] = data[177].toInt();
  lowerData[P_env4_adsr_mode] = data[178].toInt();

  upperData[P_chorus] = data[179].toInt();
  lowerData[P_chorus] = data[180].toInt();

  // Patch data
  keyMode = data[181].toInt();
  dualdetune = data[182].toInt();
  at_vib = data[183].toInt();
  at_bri = data[184].toInt();
  at_vol = data[185].toInt();
  balance = data[186].toInt();
  portamento = data[187].toInt();
  volume = data[188].toInt();
  bend_range = data[189].toInt();
  chaseLevel = data[190].toInt();
  chaseMode = data[191].toInt();
  chaseTime = data[192].toInt();
  chasePlay = data[193].toInt();
  upperSplitPoint = data[194].toInt();
  lowerSplitPoint = data[195].toInt();
  upperToneNumber = data[196].toInt();
  lowerToneNumber = data[197].toInt();
  upperChromatic = data[198].toInt();
  lowerChromatic = data[199].toInt();
  upperAssign = data[200].toInt();
  lowerAssign = data[201].toInt();
  upperUnisonDetune = data[202].toInt();
  lowerUnisonDetune = data[203].toInt();
  upperHold = data[204].toInt();
  lowerHold = data[205].toInt();
  upperLFOModDepth = data[206].toInt();
  lowerLFOModDepth = data[207].toInt();
  upperPortamento_SW = data[208].toInt();
  lowerPortamento_SW = data[209].toInt();
  upperbend_enable_SW = data[210].toInt();
  lowerbend_enable_SW = data[211].toInt();
  keyMode2 = data[212].toInt();
  bend_range2 = data[213].toInt();
  at_vib_sw = data[214].toInt();
  at_bri_sw = data[215].toInt();
  at_vol_sw = data[216].toInt();

  //Patchname
  updatePatchname();
  updateButtons();

  if (keyMode == 1) {
    updateLowerToneData();
  }
  if (keyMode == 2) {
    updateUpperToneData();
  }
  if (keyMode == 0 || keyMode == 3 || keyMode == 4 || keyMode == 5) {
    updateUpperToneData();
    updateLowerToneData();
  }
  updatePerformanceData();

  Serial.print("Set Patch: ");
  Serial.println(patchName);
}

void updateButtons() {

  updatelfo1_sync(0);
  updatelfo2_sync(0);
  updatedco1_PWM_lfo_source(0);
  updatedco1_PWM_env_source(0);
  updatedco1_PWM_dyn(0);
  updatedco2_PWM_lfo_source(0);
  updatedco2_PWM_env_source(0);
  updatedco2_PWM_dyn(0);
  updatedco1_pitch_lfo_source(0);
  updatedco1_pitch_dyn(0);
  updatedco1_pitch_env_source(0);
  updatedco2_pitch_lfo_source(0);
  updatedco2_pitch_dyn(0);
  updatedco2_pitch_env_source(0);
  updatechorus(0);
  updatedco_mix_env_source(0);
  updatedco_mix_dyn(0);
  updatevcf_dyn(0);         // 9C
  updatevcf_env_source(0);  // 9D
  updatevca_env_source(0);  // AE
  updatevca_dyn(0);         // 9F
  updateoctave_send(0);
  updateenv5stage(0);
  updateadsr(0);

  LAST_PARAM = 0x00;
}

void sendPatchInit() {
  sendVoiceParam(kBoardBothPrefix, NO_OFFSET, 0x9E, 0x00);
  sendVoiceParam(kBoardBothPrefix, kBoardOffset0Prefix, 0xA7, 0x00);
  sendVoiceParam(kBoardBothPrefix, kBoardOffset0Prefix, 0xA9, 0x00);
  sendVoiceParam(kBoardBothPrefix, kBoardOffset0Prefix, 0xAB, 0x00);
  sendVoiceParam(kBoardBothPrefix, kBoardOffset0Prefix, 0xAC, 0x00);

  sendVoiceParam(kBoardBothPrefix, kBoardOffset1Prefix, 0xA7, 0x00);
  sendVoiceParam(kBoardBothPrefix, kBoardOffset1Prefix, 0xA9, 0x00);
  sendVoiceParam(kBoardBothPrefix, kBoardOffset1Prefix, 0xAB, 0x00);
  sendVoiceParam(kBoardBothPrefix, kBoardOffset1Prefix, 0xAC, 0x00);

  sendVoiceParam(kBoardBothPrefix, kBoardOffset2Prefix, 0xA7, 0x00);
  sendVoiceParam(kBoardBothPrefix, kBoardOffset2Prefix, 0xA9, 0x00);
  sendVoiceParam(kBoardBothPrefix, kBoardOffset2Prefix, 0xAB, 0x00);
  sendVoiceParam(kBoardBothPrefix, kBoardOffset2Prefix, 0xAC, 0x00);

  sendVoiceParam(kBoardBothPrefix, kBoardOffset3Prefix, 0xA7, 0x00);
  sendVoiceParam(kBoardBothPrefix, kBoardOffset3Prefix, 0xA9, 0x00);
  sendVoiceParam(kBoardBothPrefix, kBoardOffset3Prefix, 0xAB, 0x00);
  sendVoiceParam(kBoardBothPrefix, kBoardOffset3Prefix, 0xAC, 0x00);
}

void sendKeyModeStartInit() {
  sendVoiceParam(kBoardBothPrefix, NO_OFFSET, 0xB6, 0x7F);
  sendVoiceParam(kBoardBothPrefix, NO_OFFSET, 0xBB, 0x00);
  sendVoiceParam(kBoardBothPrefix, NO_OFFSET, 0xB5, 0x00);
}

void sendKeyModeEndInit() {

  if (keyMode == 0 || keyMode == 3 || keyMode == 4 || keyMode == 5) {
    sendVoiceParam(kBoardLowerPrefix, NO_OFFSET, 0x9E, lowerVCASend);
    sendVoiceParam(kBoardUpperPrefix, NO_OFFSET, 0x9E, upperVCASend);
  }


  if (keyMode == 1 || keyMode == 2 || keyMode == 3) {
    sendVoiceParam(kBoardBothPrefix, NO_OFFSET, 0xB4, upperMT1);
    sendVoiceParam(kBoardBothPrefix, NO_OFFSET, 0xBE, upperMT1);
  } else {
    sendVoiceParam(kBoardLowerPrefix, NO_OFFSET, 0xB4, lowerSend);
    sendVoiceParam(kBoardLowerPrefix, NO_OFFSET, 0xBE, lowerSend);
    sendVoiceParam(kBoardUpperPrefix, NO_OFFSET, 0xB4, upperMT1);
    sendVoiceParam(kBoardUpperPrefix, NO_OFFSET, 0xBE, upperMT2);
  }

  sendVoiceParam(kBoardBothPrefix, NO_OFFSET, 0xB0, portamento);
  sendVoiceParam(kBoardLowerPrefix, NO_OFFSET, 0xB5, 0);
  sendVoiceParam(kBoardUpperPrefix, NO_OFFSET, 0xB5, 0);
}


void updateUpperToneData() {
  bool upperSWsafe = upperSW;
  upperSW = true;

  updatedco1_range(0);
  updatedco1_wave(0);
  updatedco1_tune(0);
  updatedco1_pitch_lfo(0);
  updatedco1_pitch_lfo_source(0);
  updatedco1_pitch_env(0);
  updatedco1_pitch_dyn(0);
  updatedco1_pitch_env_source(0);

  updatedco2_range(0);
  updatedco2_wave(0);
  updatedco2_tune(0);
  updatedco2_pitch_lfo(0);
  updatedco2_pitch_lfo_source(0);
  updatedco2_pitch_env(0);
  updatedco2_pitch_dyn(0);
  updatedco2_pitch_env_source(0);

  updatedco1_xmod(0);
  updatedco2_fine(0);
  updatevcf_hpf(0);
  updatechorus(0);

  updatedco1_PW(0);
  updatedco1_PWM_env(0);
  updatedco1_PWM_lfo(0);
  updatedco1_PWM_lfo_source(0);
  updatedco1_PWM_env_source(0);
  updatedco1_PWM_dyn(0);

  updatedco2_PW(0);
  updatedco2_PWM_env(0);
  updatedco2_PWM_lfo(0);
  updatedco2_PWM_lfo_source(0);
  updatedco2_PWM_env_source(0);
  updatedco2_PWM_dyn(0);

  updatedco1_level(0);
  updatedco2_level(0);
  updatedco_mix_env_source(0);
  updatedco_mix_dyn(0);
  updatedco2_mod(0);

  updatevcf_cutoff(0);      // 96
  updatevcf_res(0);         // 97
  updatevcf_lfo1(0);        // 98
  updatevcf_lfo2(0);        // 99
  updatevcf_env(0);         // 9A
  updatevcf_kb(0);          // 9B
  updatevcf_dyn(0);         // 9C
  updatevcf_env_source(0);  // 9D
  updatevca_mod(0);         // 9E
  updatevca_env_source(0);  // AE
  updatevca_dyn(0);         // 9F

  updatelfo1_wave(0);
  updatelfo1_delay(0);
  updatelfo1_rate(0);
  updatelfo1_lfo2(0);
  updatelfo1_sync(0);

  updatelfo2_wave(0);
  updatelfo2_rate(0);
  updatelfo2_delay(0);
  updatelfo2_lfo1(0);
  updatelfo2_sync(0);

  updatetime1(0);
  updatelevel1(0);
  updatetime2(0);
  updatelevel2(0);
  updatetime3(0);
  updatelevel3(0);
  updatetime4(0);
  updateenv5stage_mode(0);

  updateenv2_time1(0);
  updateenv2_level1(0);
  updateenv2_time2(0);
  updateenv2_level2(0);
  updateenv2_time3(0);
  updateenv2_level3(0);
  updateenv2_time4(0);
  updateenv2_env5stage_mode(0);

  updateattack(0);
  updatedecay(0);
  updatesustain(0);
  updaterelease(0);
  updateadsr_mode(0);

  updateenv4_attack(0);
  updateenv4_decay(0);
  updateenv4_sustain(0);
  updateenv4_release(0);
  updateenv4_adsr_mode(0);

  sendVoiceParam(kBoardUpperPrefix, kBoardOffset3Prefix, 0xA7, 0x7F);
  sendVoiceParam(kBoardUpperPrefix, kBoardOffset3Prefix, 0xA9, 0x7F);
  sendVoiceParam(kBoardUpperPrefix, kBoardOffset3Prefix, 0xA8, 0x00);

  sendVoiceParam(kBoardUpperPrefix, kBoardOffset2Prefix, 0xA7, 0x7F);
  sendVoiceParam(kBoardUpperPrefix, kBoardOffset2Prefix, 0xA9, 0x7F);
  sendVoiceParam(kBoardUpperPrefix, kBoardOffset2Prefix, 0xA8, 0x00);

  board = 0x00;
  EXTRA_OFFSET = 0xFF;
  LAST_PARAM = 0x00;
  upperSW = upperSWsafe;
}

void updateLowerToneData() {
  bool upperSWsafe = upperSW;
  upperSW = false;

  updatedco1_range(0);
  updatedco1_wave(0);
  updatedco1_tune(0);
  updatedco1_pitch_lfo(0);
  updatedco1_pitch_lfo_source(0);
  updatedco1_pitch_env(0);
  updatedco1_pitch_dyn(0);
  updatedco1_pitch_env_source(0);

  updatedco2_range(0);
  updatedco2_wave(0);
  updatedco2_tune(0);
  updatedco2_pitch_lfo(0);
  updatedco2_pitch_lfo_source(0);
  updatedco2_pitch_env(0);
  updatedco2_pitch_dyn(0);
  updatedco2_pitch_env_source(0);

  updatedco1_xmod(0);
  updatedco2_fine(0);
  updatevcf_hpf(0);
  updatechorus(0);

  updatedco1_PW(0);
  updatedco1_PWM_env(0);
  updatedco1_PWM_lfo(0);
  updatedco1_PWM_lfo_source(0);
  updatedco1_PWM_env_source(0);
  updatedco1_PWM_dyn(0);

  updatedco2_PW(0);
  updatedco2_PWM_env(0);
  updatedco2_PWM_lfo(0);
  updatedco2_PWM_lfo_source(0);
  updatedco2_PWM_env_source(0);
  updatedco2_PWM_dyn(0);

  updatedco1_level(0);
  updatedco2_level(0);
  updatedco_mix_env_source(0);
  updatedco_mix_dyn(0);
  updatedco2_mod(0);

  updatevcf_cutoff(0);      // 96
  updatevcf_res(0);         // 97
  updatevcf_lfo1(0);        // 98
  updatevcf_lfo2(0);        // 99
  updatevcf_env(0);         // 9A
  updatevcf_kb(0);          // 9B
  updatevcf_dyn(0);         // 9C
  updatevcf_env_source(0);  // 9D
  updatevca_mod(0);         // 9E
  updatevca_env_source(0);  // AE
  updatevca_dyn(0);         // 9F

  updatelfo1_wave(0);
  updatelfo1_delay(0);
  updatelfo1_rate(0);
  updatelfo1_lfo2(0);
  updatelfo1_sync(0);

  updatelfo2_wave(0);
  updatelfo2_rate(0);
  updatelfo2_delay(0);
  updatelfo2_lfo1(0);
  updatelfo2_sync(0);

  updatetime1(0);
  updatelevel1(0);
  updatetime2(0);
  updatelevel2(0);
  updatetime3(0);
  updatelevel3(0);
  updatetime4(0);
  updateenv5stage_mode(0);

  updateenv2_time1(0);
  updateenv2_level1(0);
  updateenv2_time2(0);
  updateenv2_level2(0);
  updateenv2_time3(0);
  updateenv2_level3(0);
  updateenv2_time4(0);
  updateenv2_env5stage_mode(0);

  updateattack(0);
  updatedecay(0);
  updatesustain(0);
  updaterelease(0);
  updateadsr_mode(0);

  updateenv4_attack(0);
  updateenv4_decay(0);
  updateenv4_sustain(0);
  updateenv4_release(0);
  updateenv4_adsr_mode(0);

  sendVoiceParam(kBoardLowerPrefix, kBoardOffset3Prefix, 0xA7, 0x7F);
  sendVoiceParam(kBoardLowerPrefix, kBoardOffset3Prefix, 0xA9, 0x7F);
  sendVoiceParam(kBoardLowerPrefix, kBoardOffset3Prefix, 0xA8, 0x00);

  sendVoiceParam(kBoardLowerPrefix, kBoardOffset2Prefix, 0xA7, 0x7F);
  sendVoiceParam(kBoardLowerPrefix, kBoardOffset2Prefix, 0xA9, 0x7F);
  sendVoiceParam(kBoardLowerPrefix, kBoardOffset2Prefix, 0xA8, 0x00);

  board = 0x00;
  EXTRA_OFFSET = 0xFF;
  LAST_PARAM = 0x00;
  upperSW = upperSWsafe;
}

void updatePerformanceData() {

  updatekeyMode(0);
  updateassignMode(0);
  updateat_vib(0);
  updateat_bri(0);
  updateat_vol(0);
  updatebalance(0);
  updatevolume(0);
  updateportamento(0);
  updatedualdetune(0);
  updatebend_enable_button(0);

  updatemod_lfo(0);

  oldupperSW = upperSW;
  upperSW = true;
  updatevca_mod(0);
  upperSW = false;
  updatevca_mod(0);
  upperSW = oldupperSW;

  updateafter_vib_enable_button(0);
  updateafter_bri_enable_button(0);
  updateafter_vol_enable_button(0);

  updatebend_range(0);

  sendVoiceParam(kBoardBothPrefix, NO_OFFSET, 0xB6, 0x7F);

  updateportamento_sw(0);
  at_vib_safe = at_vib;
  at_bri_safe = at_bri;
  at_vol_safe = at_vol;
  updatechasePlay(0);

  LAST_PARAM = 0x00;
}

void updateUpperPatchData() {

  lowerSavedAssign = lowerRealAssign;
  lowerRealAssign = upperRealAssign;
}

void updateLowerPatchData() {

  upperSavedAssign = upperRealAssign;
  upperRealAssign = lowerRealAssign;
}

void showSettingsPage() {
  showSettingsPage(settings::current_setting(), settings::current_setting_value(), state);
}

void showPatchEditPage() {
  showPatchEditPage(patchmenu::current_setting(), patchmenu::current_setting_value(), state);
}

void showToneEditPage() {
  showToneEditPage(tonemenu::current_setting(), tonemenu::current_setting_value(), state);
}

void showMIDIEditPage() {
  showMIDIEditPage(midimenu::current_setting(), midimenu::current_setting_value(), state);
}


void updatePatchname() {
  showPatchPage(String(patchNo), patchName);
}

void updateBankBlink() {
  if (!bankSelectMode) return;
  if (millis() - bankBlinkTimer > BANK_BLINK_INTERVAL) {
    bankBlinkState = !bankBlinkState;
    bankBlinkTimer = millis();
    // TODO: update your display/LED to show bank flashing here
    // e.g. showBankBlink(bankBlinkState);
    updateScreen();
  }
}

// ---------------------------------------------------------------------------
// checkSwitches  — replaces original
// ---------------------------------------------------------------------------
void checkSwitches() {

  // --- SAVE button ---
  saveButton.update();
  if (saveButton.held()) {
    // Held: enter patch rename mode
    if (state == PARAMETER) {
      enterPatchNameEdit();  // or the old PATCHNAMING flow if not migrated yet
      updateScreen();
    }
  } else if (saveButton.numClicks() == 1) {
    switch (state) {
      case PARAMETER:
        // 1st SAVE press — arm. Default target = currently loaded slot.
        saveTargetBank = currentBank;
        saveTargetGroup = currentGroup;
        saveTargetSlot = currentSlot;
        saveBankPicking = false;
        state = BANK_SELECT_SAVE;
        showSaveTargetPage();
        updateScreen();
        break;

      case BANK_SELECT_SAVE:
        // 2nd SAVE press — commit.
        if (renamedPatch.length() > 0) patchName = renamedPatch;
        savePatchTo(saveTargetBank, saveTargetGroup, saveTargetSlot);
        renamedPatch = "";
        saveBankPicking = false;
        state = PARAMETER;
        updateScreen();
        break;
    }
  }

  // --- SETTINGS button ---
  settingsButton.update();
  if (settingsButton.held()) {
    state = REINITIALISE;
    reinitialiseToPanel();
    updateScreen();
  } else if (settingsButton.numClicks() == 1) {
    switch (state) {
      case PARAMETER:
        state = SETTINGS;
        showSettingsPage();
        updateScreen();
        break;
      case SETTINGS:
        showSettingsPage();
        updateScreen();
        // fall through
      case SETTINGSVALUE:
        settings::save_current_value();
        state = SETTINGS;
        showSettingsPage();
        updateScreen();
        break;
    }
  }

  // --- BACK button ---
  backButton.update();
  if (backButton.held()) {
    allNotesOff();
    updateScreen();
  } else if (backButton.numClicks() == 1) {
    switch (state) {
      case BANK_SELECT:
      case BANK_SELECT_SAVE:
        bankSelectMode = false;
        saveBankPicking = false;
        state = PARAMETER;
        updateScreen();
        break;

      case SETTINGS:
        state = PARAMETER;
        updateScreen();
        break;

      case SETTINGSVALUE:
        state = SETTINGS;
        showSettingsPage();
        updateScreen();
        break;

      case PATCH_EDIT:
        state = PARAMETER;
        updateScreen();
        break;

      case PATCH_EDITVALUE:
        state = PARAMETER;
        updateScreen();
        break;

      case TONE_EDIT:
        state = PARAMETER;
        updateScreen();
        break;

      case TONE_EDITVALUE:
        state = PARAMETER;
        updateScreen();
        break;

      case MIDI_EDIT:
        state = PARAMETER;
        updateScreen();
        break;

      case MIDI_EDITVALUE:
        state = PARAMETER;
        updateScreen();
        break;

      case CHASE_EDIT:
        state = PARAMETER;
        updateScreen();
        break;
    }
  }

  // --- ENCODER BUTTON (Recall) ---
  recallButton.update();
  if (recallButton.held()) {
    // Held: revert patch to last saved state
    recallCurrentPatch();
    state = PARAMETER;
    updateScreen();
  } else if (recallButton.numClicks() == 1) {
    switch (state) {
      case PARAMETER:
        bankSelectMode = true;
        bankBlinkTimer = millis();
        state = BANK_SELECT;
        // Position cursor on the bank letter and blink it
        lcd.setCursor(2, 0);  // wherever your bank letter sits on line 0
        lcd.blink();
        updateScreen();
        break;

        // Back button cancel - restore the existing bank letter:
        bankSelectMode = false;
        lcd.noBlink();
        lcd.noCursor();
        lcd.setCursor(2, 0);
        lcd.print((char)(0 + currentBank));
        state = PARAMETER;
        updateScreen();

      case BANK_SELECT_SAVE:
        // Press RECALL while armed → enter bank-picking sub-flow
        saveBankPicking = true;
        showBankPickingPage();
        updateScreen();
        break;

      case SETTINGS:
        state = SETTINGSVALUE;
        showSettingsPage();
        updateScreen();
        break;

      case SETTINGSVALUE:
        settings::save_current_value();
        state = SETTINGS;
        showSettingsPage();
        updateScreen();
        break;

      case PATCH_EDIT:
        state = PATCH_EDITVALUE;
        showPatchEditPage();
        updateScreen();
        break;

      case PATCH_EDITVALUE:
        state = PATCH_EDIT;
        showPatchEditPage();
        updateScreen();
        break;

      case TONE_EDIT:
        state = TONE_EDITVALUE;
        showToneEditPage();
        updateScreen();
        break;

      case TONE_EDITVALUE:
        state = TONE_EDIT;
        showToneEditPage();
        updateScreen();
        break;

      case MIDI_EDIT:
        state = MIDI_EDITVALUE;
        showMIDIEditPage();
        updateScreen();
        break;

      case MIDI_EDITVALUE:
        state = MIDI_EDIT;
        showMIDIEditPage();
        updateScreen();
        break;

      case PEDAL_EDIT:
        pedalAssign = pedalAssignWorking;
        storePedalAssign(pedalAssign);
        break;

      case C1_EDIT:
        c1Assign = c1AssignWorking;
        storeC1Assign(c1Assign);
        break;

      case C2_EDIT:
        c2Assign = c2AssignWorking;
        storeC2Assign(c2Assign);
        break;
    }
  }
}

void handlePatchButton(int group, int slot) {

  switch (state) {

    // --- Normal play mode: load immediately ---
    case PARAMETER:
    case PATCH:
      if (group >= 0) currentGroup = group;
      if (slot >= 1) currentSlot = slot;
      recallPatch(currentBank, currentGroup, currentSlot);
      state = PARAMETER;
      updateScreen();
      break;

      // --- Bank-select mode (Recall pressed): A-H picks bank, no load ---
    case BANK_SELECT:
      if (group >= 0) {
        currentBank = group;  // A-H buttons = banks 0-7 (display 1-8)
        bankSelectMode = false;
        state = PARAMETER;
        updateScreen();
      } else if (slot >= 1) {
        currentBank = slot + 7;  // 1-8 buttons = banks 8-15 (display 9-16)
        bankSelectMode = false;
        state = PARAMETER;
        updateScreen();
      }
      break;

    // --- Save-destination picking: Recall + SAVE entered this state ---
    case BANK_SELECT_SAVE:
      if (group >= 0) {
        // A-H press picks destination bank, now wait for slot
        saveTargetBank = group;
        saveTargetGroup = currentGroup;  // will be overridden by slot press
        // Stay in BANK_SELECT_SAVE waiting for a 1-8 press
        showPatchPage("SAVE TO", String((char)(0 + group)) + "?");
        updateScreen();
      } else if (slot >= 1) {
        // 1-8 press picks destination slot — save now
        saveTargetSlot = slot;
        if (renamedPatch.length() > 0) patchName = renamedPatch;
        savePatchTo(saveTargetBank, currentGroup, saveTargetSlot);
        renamedPatch = "";
        bankSelectMode = false;
        saveToBankMode = false;
        state = PARAMETER;
        updateScreen();
      }
      break;

    // --- Patch naming: 1-8 and A-H scroll through characters ---
    case PATCHNAMING:
      if (slot >= 1) {
        // 1-8: step forward through characters
        if (charIndex >= TOTALCHARS) charIndex = 0;
        currentCharacter = CHARACTERS[charIndex++];
        showRenamingPage(renamedPatch + currentCharacter);
        updateScreen();
      } else if (group >= 0) {
        // A-H: step backward through characters
        if (charIndex <= 0) charIndex = TOTALCHARS - 1;
        currentCharacter = CHARACTERS[charIndex--];
        showRenamingPage(renamedPatch + currentCharacter);
        updateScreen();
      }
      break;

    default:
      break;
  }
}

void reinitialiseToPanel() {
  // //This sets the current patch to be the same as the current hardware panel state - all the pots
  // //The four button controls stay the same state
  // //This reinialises the previous hardware values to force a re-read
  // muxInput = 0;
  // for (int i = 0; i < MUXCHANNELS; i++) {
  // }
  // patchName = INITPATCHNAME;
  // showPatchPage("Initial", "Panel Settings");
}

// ---------------------------------------------------------------------------
// checkEncoder  — encoder now used only for patch renaming and settings
// ---------------------------------------------------------------------------
void checkEncoder() {
  long encRead = encoder.read();

  if ((encCW && encRead > encPrevious + 3) || (!encCW && encRead < encPrevious - 3)) {
    switch (state) {

      case PATCHNAMING:
        {
          char c = patchNameBuffer[patchNameCursor];
          if (c < 'A' || c > 'Z') c = 'A';
          else c = (c == 'Z') ? 'A' : c + 1;
          patchNameBuffer[patchNameCursor] = c;
          renderPatchNaming();
          updateScreen();
          break;
        }

      case SETTINGS:
        settings::increment_setting();
        showSettingsPage();
        updateScreen();
        break;

      case SETTINGSVALUE:
        settings::increment_setting_value();
        showSettingsPage();
        updateScreen();
        break;

      case PATCH_EDIT:
        patchmenu::increment_setting();
        showPatchEditPage(patchmenu::current_setting(),
                          patchmenu::current_setting_value(),
                          PATCH_EDIT);
        updateScreen();
        break;

      case PATCH_EDITVALUE:
        patchmenu::increment_setting_value();
        patchmenu::save_current_value();
        showPatchEditPage(patchmenu::current_setting(),
                          patchmenu::current_setting_value(),
                          PATCH_EDITVALUE);
        updateScreen();
        break;

      case TONE_EDIT:
        tonemenu::increment_setting();
        showToneEditPage(tonemenu::current_setting(),
                         tonemenu::current_setting_value(),
                         TONE_EDIT);
        updateScreen();
        break;

      case TONE_EDITVALUE:
        tonemenu::increment_setting_value();
        tonemenu::save_current_value();
        showToneEditPage(tonemenu::current_setting(),
                         tonemenu::current_setting_value(),
                         TONE_EDITVALUE);
        updateScreen();
        break;

      case MIDI_EDIT:
        midimenu::increment_setting();
        showMIDIEditPage(midimenu::current_setting(),
                         midimenu::current_setting_value(),
                         MIDI_EDIT);
        updateScreen();
        break;

      case MIDI_EDITVALUE:
        midimenu::increment_setting_value();
        midimenu::save_current_value();
        showMIDIEditPage(midimenu::current_setting(),
                         midimenu::current_setting_value(),
                         MIDI_EDITVALUE);
        updateScreen();
        break;

      case ARP_EDIT:
        arpParamIndex = (arpParamIndex + 1) % 8;
        showCurrentParameterPage(arpParamNames[arpParamIndex],
                                 arpParamValueString(arpParamIndex));
        updateScreen();
        break;

      case ARP_EDITVALUE:
        arpIncrementParam(arpParamIndex);
        showCurrentParameterPage(arpParamNames[arpParamIndex],
                                 arpParamValueString(arpParamIndex));
        updateScreen();
        break;

      case CHASE_EDIT:
        if (chaseEditField == 0) {
          if (chaseTime < 127) chaseTime++;
          updatechaseTime(0);
        } else if (chaseEditField == 1) {
          if (chaseLevel < 127) chaseLevel++;
          updatechaseLevel(0);
        } else {
          chaseMode = (chaseMode + 1) % 3;
          updatechaseMode(0);
        }
        updateScreen();
        break;

      case PEDAL_EDIT:
        pedalAssignWorking = (pedalAssignWorking + 1) % PEDAL_ASSIGN_COUNT;
        showPedalEditPage("PEDAL SW ASSIGN",
                             pedalAssignLabels[pedalAssignWorking]);
        updateScreen();
        break;

      case C1_EDIT:
        c1AssignWorking = (c1AssignWorking + 1) % CONTROLLER_ASSIGN_COUNT;
        showControlsEditPage("C1 PATCH",
                             controllerAssignLabels[c1AssignWorking]);
        updateScreen();
        break;

      case C2_EDIT:
        c2AssignWorking = (c2AssignWorking + 1) % CONTROLLER_ASSIGN_COUNT;
        showControlsEditPage("C2 PATCH",
                             controllerAssignLabels[c2AssignWorking]);
        updateScreen();
        break;
    }
    encPrevious = encRead;

  } else if ((encCW && encRead < encPrevious - 3) || (!encCW && encRead > encPrevious + 3)) {
    switch (state) {

      case PATCHNAMING:
        {
          char c = patchNameBuffer[patchNameCursor];
          if (c < 'A' || c > 'Z') c = 'Z';
          else c = (c == 'A') ? 'Z' : c - 1;
          patchNameBuffer[patchNameCursor] = c;
          renderPatchNaming();
          updateScreen();
          break;
        }

      case SETTINGS:
        settings::decrement_setting();
        showSettingsPage();
        updateScreen();
        break;

      case SETTINGSVALUE:
        settings::decrement_setting_value();
        showSettingsPage();
        updateScreen();
        break;

      case PATCH_EDIT:
        patchmenu::decrement_setting();
        showPatchEditPage(patchmenu::current_setting(),
                          patchmenu::current_setting_value(),
                          PATCH_EDIT);
        updateScreen();
        break;

      case PATCH_EDITVALUE:
        patchmenu::decrement_setting_value();
        patchmenu::save_current_value();
        showPatchEditPage(patchmenu::current_setting(),
                          patchmenu::current_setting_value(),
                          PATCH_EDITVALUE);
        updateScreen();
        break;

      case TONE_EDIT:
        tonemenu::decrement_setting();
        showToneEditPage(tonemenu::current_setting(),
                         tonemenu::current_setting_value(),
                         TONE_EDIT);
        updateScreen();
        break;

      case TONE_EDITVALUE:
        tonemenu::decrement_setting_value();
        tonemenu::save_current_value();
        showToneEditPage(tonemenu::current_setting(),
                         tonemenu::current_setting_value(),
                         TONE_EDITVALUE);
        updateScreen();
        break;

      case MIDI_EDIT:
        midimenu::decrement_setting();
        showMIDIEditPage(midimenu::current_setting(),
                         midimenu::current_setting_value(),
                         MIDI_EDIT);
        updateScreen();
        break;

      case MIDI_EDITVALUE:
        midimenu::decrement_setting_value();
        midimenu::save_current_value();
        showMIDIEditPage(midimenu::current_setting(),
                         midimenu::current_setting_value(),
                         MIDI_EDITVALUE);
        updateScreen();
        break;

      case ARP_EDIT:
        arpParamIndex = (arpParamIndex + 7) % 8;
        showCurrentParameterPage(arpParamNames[arpParamIndex],
                                 arpParamValueString(arpParamIndex));
        updateScreen();
        break;
      case ARP_EDITVALUE:
        arpDecrementParam(arpParamIndex);
        showCurrentParameterPage(arpParamNames[arpParamIndex],
                                 arpParamValueString(arpParamIndex));
        updateScreen();
        break;

      case CHASE_EDIT:
        if (chaseEditField == 0) {
          if (chaseTime > 0) chaseTime--;
          updatechaseTime(0);
        } else if (chaseEditField == 1) {
          if (chaseLevel > 0) chaseLevel--;
          updatechaseLevel(0);
        } else {
          chaseMode = (chaseMode + 2) % 3;
          updatechaseMode(0);
        }
        updateScreen();
        break;

      case PEDAL_EDIT:
        pedalAssignWorking = (pedalAssignWorking == 0)
                               ? (PEDAL_ASSIGN_COUNT - 1)
                               : (pedalAssignWorking - 1);
        showPedalEditPage("PEDAL SW ASSIGN",
                             pedalAssignLabels[pedalAssignWorking]);
        updateScreen();
        break;

      case C1_EDIT:
        c1AssignWorking = (c1AssignWorking == 0)
                            ? (CONTROLLER_ASSIGN_COUNT - 1)
                            : (c1AssignWorking - 1);
        showControlsEditPage("C1 PATCH",
                             controllerAssignLabels[c1AssignWorking]);
        updateScreen();
        break;

      case C2_EDIT:
        c2AssignWorking = (c2AssignWorking == 0)
                            ? (CONTROLLER_ASSIGN_COUNT - 1)
                            : (c2AssignWorking - 1);
        showControlsEditPage("C2 PATCH",
                             controllerAssignLabels[c2AssignWorking]);
        updateScreen();
        break;
    }
    encPrevious = encRead;
  }
}

void sendCustomSysEx(byte outChannel, byte parameter, byte value) {
  if (sysexExclusive >= 2) {
    const byte sysexData[] = {
      0xF0,
      0x41,
      0x39,  // Correct IPR opcode
      (byte)(outChannel & 0x0F),
      0x24,
      0x30,
      0x01,
      (byte)(parameter & 0x7F),
      (byte)(value & 0x7F),
      0xF7
    };

    MIDI.sendSysEx(sizeof(sysexData), sysexData, true);
  }
}

void sendToneSysEx(uint8_t boardprefix, byte outChannel, byte parameter, byte value) {
  if (sysexExclusive >= 2) {
    const byte sysexData[] = {
      0xF0,
      0x41,
      0x39,  // Correct IPR opcode
      (byte)(outChannel & 0x0F),
      0x24,
      0x20,
      boardprefix,
      (byte)(parameter & 0x7F),
      (byte)(value & 0x7F),
      0xF7
    };

    MIDI.sendSysEx(sizeof(sysexData), sysexData, true);
  }
}

void midiCCOut(int CC, int value) {
  switch (keyMode) {
    case 0:
      MIDI.sendControlChange(99, 1, controlChannel);     // NRPN MSB
      MIDI.sendControlChange(98, CC, controlChannel);    // NRPN LSB
      MIDI.sendControlChange(6, value, controlChannel);  // Data Entry MSB
      break;

    case 1:
      MIDI.sendControlChange(99, 0, controlChannel);     // NRPN MSB
      MIDI.sendControlChange(98, CC, controlChannel);    // NRPN LSB
      MIDI.sendControlChange(6, value, controlChannel);  // Data Entry MSB
      break;

    case 2:
      MIDI.sendControlChange(99, 1, controlChannel);     // NRPN MSB
      MIDI.sendControlChange(98, CC, controlChannel);    // NRPN LSB
      MIDI.sendControlChange(6, value, controlChannel);  // Data Entry MSB

      MIDI.sendControlChange(99, 0, controlChannel);     // NRPN MSB
      MIDI.sendControlChange(98, CC, controlChannel);    // NRPN LSB
      MIDI.sendControlChange(6, value, controlChannel);  // Data Entry MSB
      break;
  }
}

void pingPongStep(int &value, bool &goingUp) {
  if (goingUp) {
    value++;
    if (value >= 2) goingUp = false;  // hit 2 → reverse
  } else {
    value--;
    if (value <= 0) goingUp = true;  // hit 0 → reverse
  }
}

void mainButtonChanged(Button *btn, bool released) {

  switch (btn->id) {

    case NAME_BUTTON:
      if (!released) {
        if (state == PATCHNAMING) exitPatchNameEdit(false);  // cancel
        else enterPatchNameEdit();                           // enter
      }
      break;

    case PEDAL_BUTTON:
      if (!released) {
        if (state == PEDAL_EDIT) {
          // Re-press exits without committing
          state = PARAMETER;
        } else {
          // Exit any other menu first
          mcp9.digitalWrite(PATCH_LED_RED, LOW);
          mcp9.digitalWrite(TONE_LED_RED, LOW);
          mcp9.digitalWrite(MIDI_LED_RED, LOW);
          pedalAssignWorking = pedalAssign;  // start preview from stored
          state = PEDAL_EDIT;
          showPedalEditPage("PEDAL SW ASSIGN",
                               pedalAssignLabels[pedalAssignWorking]);
        }
        updateScreen();
      }
      break;

    case C1_BUTTON:
      if (!released) {
        if (state == C1_EDIT) {
          state = PARAMETER;
        } else {
          mcp9.digitalWrite(PATCH_LED_RED, LOW);
          mcp9.digitalWrite(TONE_LED_RED, LOW);
          mcp9.digitalWrite(MIDI_LED_RED, LOW);
          c1AssignWorking = c1Assign;
          state = C1_EDIT;
          showControlsEditPage("C1 PATCH",
                               controllerAssignLabels[c1AssignWorking]);
        }
        updateScreen();
      }
      break;

    case C2_BUTTON:
      if (!released) {
        if (state == C2_EDIT) {
          state = PARAMETER;
        } else {
          mcp9.digitalWrite(PATCH_LED_RED, LOW);
          mcp9.digitalWrite(TONE_LED_RED, LOW);
          mcp9.digitalWrite(MIDI_LED_RED, LOW);
          c2AssignWorking = c2Assign;
          state = C2_EDIT;
          showControlsEditPage("C2 PATCH",
                               controllerAssignLabels[c2AssignWorking]);
        }
        updateScreen();
      }
      break;

    case PATCH_BUTTON:
      if (!released) {
        if (state == PEDAL_EDIT || state == C1_EDIT || state == C2_EDIT) {
          state = PARAMETER;
        }
        if (state == PATCH_EDIT || state == PATCH_EDITVALUE) {
          // Already in PATCH menu — exit back to PARAMETER
          state = PARAMETER;
          mcp9.digitalWrite(PATCH_LED_RED, LOW);
        } else {
          // Enter PATCH menu (turn off any other menu LED that might be on)
          mcp9.digitalWrite(TONE_LED_RED, LOW);
          mcp9.digitalWrite(MIDI_LED_RED, LOW);
          state = PATCH_EDIT;
          mcp9.digitalWrite(PATCH_LED_RED, HIGH);
          patchmenu::refresh_value();  // ← add this
          showPatchEditPage(patchmenu::current_setting(),
                            patchmenu::current_setting_value(),
                            PATCH_EDIT);
        }
        updateScreen();
      }
      break;

    case TONE_BUTTON:
      if (!released) {
        if (state == PEDAL_EDIT || state == C1_EDIT || state == C2_EDIT) {
          state = PARAMETER;
        }
        if (state == TONE_EDIT || state == TONE_EDITVALUE) {
          state = PARAMETER;
          mcp9.digitalWrite(TONE_LED_RED, LOW);
        } else {
          mcp9.digitalWrite(PATCH_LED_RED, LOW);
          mcp9.digitalWrite(MIDI_LED_RED, LOW);
          state = TONE_EDIT;
          mcp9.digitalWrite(TONE_LED_RED, HIGH);
          tonemenu::refresh_value();  // ← add this
          showToneEditPage(tonemenu::current_setting(),
                           tonemenu::current_setting_value(),
                           TONE_EDIT);
        }
        updateScreen();
      }
      break;

    case MIDI_BUTTON:
      if (!released) {
        if (state == PEDAL_EDIT || state == C1_EDIT || state == C2_EDIT) {
          state = PARAMETER;
        }
        if (state == MIDI_EDIT || state == MIDI_EDITVALUE) {
          state = PARAMETER;
          mcp9.digitalWrite(MIDI_LED_RED, LOW);
        } else {
          mcp9.digitalWrite(PATCH_LED_RED, LOW);
          mcp9.digitalWrite(TONE_LED_RED, LOW);
          state = MIDI_EDIT;
          mcp9.digitalWrite(MIDI_LED_RED, HIGH);
          showMIDIEditPage(midimenu::current_setting(),
                           midimenu::current_setting_value(),
                           MIDI_EDIT);
        }
        updateScreen();
      }
      break;

    case PARAM_BUTTON:
      if (!released) {
        switch (state) {
          case PATCH_EDIT:
          case PATCH_EDITVALUE:
            state = PATCH_EDIT;
            showPatchEditPage();
            updateScreen();
            break;
          case TONE_EDIT:
          case TONE_EDITVALUE:
            state = TONE_EDIT;
            showToneEditPage();
            updateScreen();
            break;
          case MIDI_EDIT:
          case MIDI_EDITVALUE:
            state = MIDI_EDIT;
            showMIDIEditPage();
            updateScreen();
            break;
          default:
            // PARAM has no meaning outside an edit page — ignore
            break;
        }
      }
      break;

    case VALUE_BUTTON:
      if (!released) {
        switch (state) {
          case PATCH_EDIT:
          case PATCH_EDITVALUE:
            state = PATCH_EDITVALUE;
            showPatchEditPage();
            updateScreen();
            break;
          case TONE_EDIT:
          case TONE_EDITVALUE:
            state = TONE_EDITVALUE;
            showToneEditPage();
            updateScreen();
            break;
          case MIDI_EDIT:
          case MIDI_EDITVALUE:
            state = MIDI_EDITVALUE;
            showMIDIEditPage();
            updateScreen();
            break;
          default:
            break;
        }
      }
      break;

    case OCTAVE_DOWN_BUTTON:
      if (!released) {
        myControlChange(midiChannel, CCoctave_down, 1);
      }
      break;

    case OCTAVE_UP_BUTTON:
      if (!released) {
        myControlChange(midiChannel, CCoctave_up, 1);
      }
      break;


    case CHASE_ON_OFF_BUTTON:
      if (!released) {
        chasePlay = !chasePlay;
        updatechase(0);
      }
      break;

    case CHASE_TIME_BUTTON:
      if (!released) {
        if (state == CHASE_EDIT && chaseEditField == 0) {
          // Already on Time — exit
          state = PARAMETER;
        } else {
          chaseEditField = 0;
          state = CHASE_EDIT;
        }
        updateScreen();
      }
      break;

    case CHASE_FUNCTION_BUTTON:
      if (!released) {
        // Alternate between Level and Mode, per JX-10 spec
        chaseEditField = (chaseEditField == 1) ? 2 : 1;
        state = CHASE_EDIT;
        updateScreen();
      }
      break;

    case SEQ_FUNCTION_BUTTON:
      if (!released) {
        arpFunctionMode = !arpFunctionMode;
        if (arpFunctionMode) {
          state = ARP_EDIT;
          showCurrentParameterPage(arpParamNames[arpParamIndex],
                                   arpParamValueString(arpParamIndex));
        } else {
          state = PARAMETER;
        }
        updateScreen();
      }
      break;

    case SEQ_START_STOP_BUTTON:
      if (!released) {
        arpEnabled = !arpEnabled;
        if (arpEnabled) {
          arpPos = -1;
          arpDir = 1;
          arpNextStepUs = 0;
          arpMidiTickCount = 0;
          arpNoteActive = false;
          arpInsertCounter = 0;
          // Don't set arpRunning yet - it starts when first note is pressed
        } else {
          arpStopCurrentNote();
          arpRunning = false;
          arpEnabled = false;
          if (!arpLatch) {
            arpClearPattern();
            arpBuildStepList();
          }
          mcp9.digitalWrite(SEQ_START_STOP_LED_RED, LOW);
        }
      }
      break;

    case SEQ_RECORD_BUTTON:
      if (!released) {
        arpLatch = !arpLatch;
        if (!arpLatch) {
          // Latch turned off - clear held notes if nothing physically held
          arpClearNonHeldNotes();
        }
        showCurrentParameterPage("LATCH", arpLatch ? "ON" : "OFF");
        updateScreen();
      }
      break;

    case PATCH_1_BUTTON:
      if (!released) {
        if (state == PATCHNAMING) {
          patchNameBuffer[patchNameCursor] = patchSpecialChars[0];  // '-'
          renderPatchNaming();
          updateScreen();
          break;
        }
        if (arpFunctionMode) {
          arpParamIndex = ARP_P_BPM;
          state = ARP_EDITVALUE;
          showCurrentParameterPage(arpParamNames[arpParamIndex],
                                   arpParamValueString(arpParamIndex));
          updateScreen();
        } else {
          handlePatchButton(-1, 1);
        }
      }
      break;

    case PATCH_2_BUTTON:
      if (!released) {
        if (state == PATCHNAMING) {
          patchNameBuffer[patchNameCursor] = patchSpecialChars[1];  // '-'
          renderPatchNaming();
          updateScreen();
          break;
        }
        if (arpFunctionMode) {
          arpParamIndex = ARP_P_MODE;
          state = ARP_EDITVALUE;
          showCurrentParameterPage(arpParamNames[arpParamIndex],
                                   arpParamValueString(arpParamIndex));
          updateScreen();
        } else {
          handlePatchButton(-1, 2);
        }
      }
      break;

    case PATCH_3_BUTTON:
      if (!released) {
        if (state == PATCHNAMING) {
          patchNameBuffer[patchNameCursor] = patchSpecialChars[2];  // '-'
          renderPatchNaming();
          updateScreen();
          break;
        }
        if (arpFunctionMode) {
          arpParamIndex = ARP_P_OCTAVE;
          state = ARP_EDITVALUE;
          showCurrentParameterPage(arpParamNames[arpParamIndex],
                                   arpParamValueString(arpParamIndex));
          updateScreen();
        } else {
          handlePatchButton(-1, 3);
        }
      }
      break;

    case PATCH_4_BUTTON:
      if (!released) {
        if (state == PATCHNAMING) {
          patchNameBuffer[patchNameCursor] = patchSpecialChars[3];  // '-'
          renderPatchNaming();
          updateScreen();
          break;
        }
        if (arpFunctionMode) {
          arpParamIndex = ARP_P_INSERT;
          state = ARP_EDITVALUE;
          showCurrentParameterPage(arpParamNames[arpParamIndex],
                                   arpParamValueString(arpParamIndex));
          updateScreen();
        } else {
          handlePatchButton(-1, 4);
        }
      }
      break;

    case PATCH_5_BUTTON:
      if (!released) {
        if (state == PATCHNAMING) {
          patchNameBuffer[patchNameCursor] = patchSpecialChars[4];  // '-'
          renderPatchNaming();
          updateScreen();
          break;
        }
        if (arpFunctionMode) {
          arpParamIndex = ARP_P_RATE;
          state = ARP_EDITVALUE;
          showCurrentParameterPage(arpParamNames[arpParamIndex],
                                   arpParamValueString(arpParamIndex));
          updateScreen();
        } else {
          handlePatchButton(-1, 5);
        }
      }
      break;

    case PATCH_6_BUTTON:
      if (!released) {
        if (state == PATCHNAMING) {
          patchNameBuffer[patchNameCursor] = patchSpecialChars[5];  // '-'
          renderPatchNaming();
          updateScreen();
          break;
        }
        if (arpFunctionMode) {
          arpParamIndex = ARP_P_DURATION;
          state = ARP_EDITVALUE;
          showCurrentParameterPage(arpParamNames[arpParamIndex],
                                   arpParamValueString(arpParamIndex));
          updateScreen();
        } else {
          handlePatchButton(-1, 6);
        }
      }
      break;

    case PATCH_7_BUTTON:
      if (!released) {
        if (state == PATCHNAMING) {
          patchNameBuffer[patchNameCursor] = patchSpecialChars[6];  // '-'
          renderPatchNaming();
          updateScreen();
          break;
        }
        if (arpFunctionMode) {
          arpParamIndex = ARP_P_VELOCITY;
          state = ARP_EDITVALUE;
          showCurrentParameterPage(arpParamNames[arpParamIndex],
                                   arpParamValueString(arpParamIndex));
          updateScreen();
        } else {
          handlePatchButton(-1, 7);
        }
      }
      break;

    case PATCH_8_BUTTON:
      if (!released) {
        if (state == PATCHNAMING) {
          patchNameBuffer[patchNameCursor] = patchSpecialChars[7];  // '-'
          renderPatchNaming();
          updateScreen();
          break;
        }
        if (arpFunctionMode) {
          arpParamIndex = ARP_P_MIDI;
          state = ARP_EDITVALUE;
          showCurrentParameterPage(arpParamNames[arpParamIndex],
                                   arpParamValueString(arpParamIndex));
          updateScreen();
        } else {
          handlePatchButton(-1, 8);
        }
      }
      break;

    case BANK_A_BUTTON:
      if (!released) {
        if (arpFunctionMode) {
          arpMemoryIndex = 0;
          arp = arpMemories[0];
          showCurrentParameterPage("ARP MEMORY", "A");
          updateScreen();
        } else {
          handlePatchButton(0, -1);  // A pressed
        }
      }
      break;

    case BANK_B_BUTTON:
      if (!released) {
        if (arpFunctionMode) {
          arpMemoryIndex = 1;
          arp = arpMemories[1];
          showCurrentParameterPage("ARP MEMORY", "B");
          updateScreen();
        } else {
          handlePatchButton(1, -1);  // B pressed
        }
      }
      break;

    case BANK_C_BUTTON:
      if (!released) {
        if (arpFunctionMode) {
          arpMemoryIndex = 2;
          arp = arpMemories[2];
          showCurrentParameterPage("ARP MEMORY", "C");
          updateScreen();
        } else {
          handlePatchButton(2, -1);  // V pressed
        }
      }
      break;

    case BANK_D_BUTTON:
      if (!released) {
        if (arpFunctionMode) {
          arpMemoryIndex = 3;
          arp = arpMemories[3];
          showCurrentParameterPage("ARP MEMORY", "D");
          updateScreen();
        } else {
          handlePatchButton(3, -1);  // D pressed
        }
      }
      break;

    case BANK_E_BUTTON:
      if (!released) {
        if (arpFunctionMode) {
          arpMemoryIndex = 4;
          arp = arpMemories[4];
          showCurrentParameterPage("ARP MEMORY", "E");
          updateScreen();
        } else {
          handlePatchButton(4, -1);  // E pressed
        }
      }
      break;

    case BANK_F_BUTTON:
      if (!released) {
        if (arpFunctionMode) {
          arpMemoryIndex = 5;
          arp = arpMemories[5];
          showCurrentParameterPage("ARP MEMORY", "F");
          updateScreen();
        } else {
          handlePatchButton(5, -1);  // F pressed
        }
      }
      break;

    case BANK_G_BUTTON:
      if (!released) {
        if (arpFunctionMode) {
          arpMemoryIndex = 6;
          arp = arpMemories[6];
          showCurrentParameterPage("ARP MEMORY", "G");
          updateScreen();
        } else {
          handlePatchButton(6, -1);  // G pressed
        }
      }
      break;

    case BANK_H_BUTTON:
      if (!released) {
        if (arpFunctionMode) {
          arpMemoryIndex = 7;
          arp = arpMemories[7];
          showCurrentParameterPage("ARP MEMORY", "H");
          updateScreen();
        } else {
          handlePatchButton(7, -1);  // H pressed
        }
      }
      break;

    case TONE_0_BUTTON:
      if (!released) {
        if (state == PATCHNAMING) {
          patchNameBuffer[patchNameCursor] = '0';
          renderPatchNaming();
          updateScreen();
          break;
        }
        handleToneDigit(0);
      }
      break;

    case TONE_1_BUTTON:
      if (!released) {
        if (state == PATCHNAMING) {
          patchNameBuffer[patchNameCursor] = '1';
          renderPatchNaming();
          updateScreen();
          break;
        }
        handleToneDigit(1);
      }
      break;

    case TONE_2_BUTTON:
      if (!released) {
        if (state == PATCHNAMING) {
          patchNameBuffer[patchNameCursor] = '2';
          renderPatchNaming();
          updateScreen();
          break;
        }
        handleToneDigit(2);
      }
      break;

    case TONE_3_BUTTON:
      if (!released) {
        if (state == PATCHNAMING) {
          patchNameBuffer[patchNameCursor] = '3';
          renderPatchNaming();
          updateScreen();
          break;
        }
        handleToneDigit(3);
      }
      break;

    case TONE_4_BUTTON:
      if (!released) {
        if (state == PATCHNAMING) {
          patchNameBuffer[patchNameCursor] = '4';
          renderPatchNaming();
          updateScreen();
          break;
        }
        handleToneDigit(4);
      }
      break;

    case TONE_5_BUTTON:
      if (!released) {
        if (state == PATCHNAMING) {
          patchNameBuffer[patchNameCursor] = '5';
          renderPatchNaming();
          updateScreen();
          break;
        }
        handleToneDigit(5);
      }
      break;

    case TONE_6_BUTTON:
      if (!released) {
        if (state == PATCHNAMING) {
          patchNameBuffer[patchNameCursor] = '6';
          renderPatchNaming();
          updateScreen();
          break;
        }
        handleToneDigit(6);
      }
      break;

    case TONE_7_BUTTON:
      if (!released) {
        if (state == PATCHNAMING) {
          patchNameBuffer[patchNameCursor] = '7';
          renderPatchNaming();
          updateScreen();
          break;
        }
        handleToneDigit(7);
      }
      break;

    case TONE_8_BUTTON:
      if (!released) {
        if (state == PATCHNAMING) {
          patchNameBuffer[patchNameCursor] = '8';
          renderPatchNaming();
          updateScreen();
          break;
        }
        handleToneDigit(8);
      }
      break;

    case TONE_9_BUTTON:
      if (!released) {
        if (state == PATCHNAMING) {
          patchNameBuffer[patchNameCursor] = '9';
          renderPatchNaming();
          updateScreen();
          break;
        }
        handleToneDigit(9);
      }
      break;

    case TONE_ENTER_BUTTON:
      if (!released) {
        if (state == PATCHNAMING) {
          exitPatchNameEdit(true);  // commit
        } else if (arpFunctionMode) {
          arpMemories[arpMemoryIndex] = arp;
          arpSaveMemory(arpMemoryIndex);
          showCurrentParameterPage("ARP MEMORY", "SAVED");
          updateScreen();
        } else if (toneEntryActive) {
          uint8_t toneNumber = (toneEntryBuffer == 0) ? 100 : toneEntryBuffer;
          if (upperSW) {
            loadToneToSlot(toneNumber - 1, true);
          } else {
            loadToneToSlot(toneNumber - 1, false);
          }
          toneEntryBuffer = 0;
          toneEntryActive = false;
          startParameterDisplay();
        }
      }
      break;

    case BEND_ENABLE_BUTTON:
      if (!released) {
        bend_enable = bend_enable + 1;
        if (bend_enable > 3) {
          bend_enable = 0;
        }
        if (bend_enable != 0 && (keyMode == 1 || keyMode == 2)) {
          bend_enable = 3;
        }
        myControlChange(midiChannel, CCbend_enable, bend_enable);
      }
      break;

    case AFTER_VIB_BUTTON:
      if (!released) {
        at_vib_sw = !at_vib_sw;
        if (!at_vib_sw) {
          at_vib_safe = at_vib;
          at_vib = 0;
        } else {
          at_vib = at_vib_safe;
        }
        myControlChange(midiChannel, CCat_vib_sw, at_vib_sw);
      }
      break;

    case AFTER_BRI_BUTTON:
      if (!released) {
        at_bri_sw = !at_bri_sw;
        if (!at_bri_sw) {
          at_bri_safe = at_bri;
          at_bri = 0;
        } else {
          at_bri = at_bri_safe;
        }
        myControlChange(midiChannel, CCat_bri_sw, at_bri_sw);
      }
      break;

    case AFTER_VOL_BUTTON:
      if (!released) {
        at_vol_sw = !at_vol_sw;
        if (!at_vol_sw) {
          at_vol_safe = at_vol;
          at_vol = 0;
        } else {
          at_vol = at_vol_safe;
        }
        myControlChange(midiChannel, CCat_vol_sw, at_vol_sw);
      }
      break;

    case KEY_DUAL_BUTTON:
      if (!released) {
        dual_button = true;
        keyMode = 0;
        myControlChange(midiChannel, CCdual_button, dual_button);
      }
      break;

    case KEY_SPLIT_BUTTON:
      if (!released) {
        split_button = true;
        keyMode = 3;
        myControlChange(midiChannel, CCsplit_button, split_button);
      }
      break;

    case KEY_SINGLE_BUTTON:
      if (!released) {
        single_button = !single_button;
        switch (single_button) {
          case 0:
            keyMode = 1;
            break;

          case 1:
            keyMode = 2;
            break;
        }
        myControlChange(midiChannel, CCsingle_button, single_button);
      }
      break;

    case KEY_SPECIAL_BUTTON:
      if (!released) {
        special_button = !special_button;
        switch (special_button) {
          case 0:
            keyMode = 4;
            break;

          case 1:
            keyMode = 5;
            break;
        }
        myControlChange(midiChannel, CCspecial_button, special_button);
      }
      break;

      // Asssigner Buttons

    case ASSIGN_POLY_BUTTON:
      if (!released) pressAssign(CAT_POLY);
      break;

    case ASSIGN_UNI_BUTTON:
      if (!released) pressAssign(CAT_UNI);
      break;

    case ASSIGN_MONO_BUTTON:
      if (!released) pressAssign(CAT_MONO);
      break;

    case LFO1_SYNC_BUTTON:
      if (!released) {
        lfo1_sync = lfo1_sync + 32;
        if (lfo1_sync > 64) {
          lfo1_sync = 0;
        }
        myControlChange(midiChannel, CClfo1_sync, lfo1_sync);
      }
      break;

    case LFO2_SYNC_BUTTON:
      if (!released) {
        lfo2_sync = lfo2_sync + 32;
        if (lfo2_sync > 64) {
          lfo2_sync = 0;
        }
        myControlChange(midiChannel, CClfo2_sync, lfo2_sync);
      }
      break;

    case DCO1_PWM_DYN_BUTTON:
      if (!released) {
        dco1_PWM_dyn = dco1_PWM_dyn + 32;
        if (dco1_PWM_dyn > 96) {
          dco1_PWM_dyn = 0;
        }
        myControlChange(midiChannel, CCdco1_PWM_dyn, dco1_PWM_dyn);
      }
      break;

    case DCO2_PWM_DYN_BUTTON:
      if (!released) {
        dco2_PWM_dyn = dco2_PWM_dyn + 32;
        if (dco2_PWM_dyn > 96) {
          dco2_PWM_dyn = 0;
        }
        myControlChange(midiChannel, CCdco2_PWM_dyn, dco2_PWM_dyn);
      }
      break;

    case DCO1_PWM_ENV_SOURCE_BUTTON:
      if (!released) {
        dco1_PWM_env_source = dco1_PWM_env_source + 32;
        if (dco1_PWM_env_source > 112) {
          dco1_PWM_env_source = 0;
          dco1_PWM_env_pol = 0;
        }
        myControlChange(midiChannel, CCdco1_PWM_env_source, dco1_PWM_env_source);
      }
      break;

    case DCO2_PWM_ENV_SOURCE_BUTTON:
      if (!released) {
        dco2_PWM_env_source = dco2_PWM_env_source + 32;
        if (dco2_PWM_env_source > 112) {
          dco2_PWM_env_source = 0;
          dco2_PWM_env_pol = 0;
        }
        myControlChange(midiChannel, CCdco2_PWM_env_source, dco2_PWM_env_source);
      }
      break;

    case DCO1_PWM_ENV_POLARITY_BUTTON:
      if (!released) {
        dco1_PWM_env_pol = !dco1_PWM_env_pol;
        if (dco1_PWM_env_pol) {
          dco1_PWM_env_source = dco1_PWM_env_source + 16;
        }
        if (!dco1_PWM_env_pol) {
          dco1_PWM_env_source = dco1_PWM_env_source - 16;
        }
        myControlChange(midiChannel, CCdco1_PWM_env_source, dco1_PWM_env_source);
      }
      break;

    case DCO2_PWM_ENV_POLARITY_BUTTON:
      if (!released) {
        dco2_PWM_env_pol = !dco2_PWM_env_pol;
        if (dco2_PWM_env_pol) {
          dco2_PWM_env_source = dco2_PWM_env_source + 16;
        }
        if (!dco2_PWM_env_pol) {
          dco2_PWM_env_source = dco2_PWM_env_source - 16;
        }
        myControlChange(midiChannel, CCdco2_PWM_env_source, dco2_PWM_env_source);
      }
      break;

    case DCO1_PWM_LFO_SOURCE_BUTTON:
      if (!released) {
        dco1_PWM_lfo_source = dco1_PWM_lfo_source + 32;
        if (dco1_PWM_lfo_source > 96) {
          dco1_PWM_lfo_source = 0;
        }
        myControlChange(midiChannel, CCdco1_PWM_lfo_source, dco1_PWM_lfo_source);
      }
      break;

    case DCO2_PWM_LFO_SOURCE_BUTTON:
      if (!released) {
        dco2_PWM_lfo_source = dco2_PWM_lfo_source + 32;
        if (dco2_PWM_lfo_source > 96) {
          dco2_PWM_lfo_source = 0;
        }
        myControlChange(midiChannel, CCdco2_PWM_lfo_source, dco2_PWM_lfo_source);
      }
      break;

    case DCO1_PITCH_DYN_BUTTON:
      if (!released) {
        dco1_pitch_dyn = dco1_pitch_dyn + 32;
        if (dco1_pitch_dyn > 96) {
          dco1_pitch_dyn = 0;
        }
        myControlChange(midiChannel, CCdco1_pitch_dyn, dco1_pitch_dyn);
      }
      break;

    case DCO2_PITCH_DYN_BUTTON:
      if (!released) {
        dco2_pitch_dyn = dco2_pitch_dyn + 32;
        if (dco2_pitch_dyn > 96) {
          dco2_pitch_dyn = 0;
        }
        myControlChange(midiChannel, CCdco2_pitch_dyn, dco2_pitch_dyn);
      }
      break;

    case DCO1_PITCH_LFO_SOURCE_BUTTON:
      if (!released) {
        dco1_pitch_lfo_source = dco1_pitch_lfo_source + 32;
        if (dco1_pitch_lfo_source > 96) {
          dco1_pitch_lfo_source = 0;
        }
        myControlChange(midiChannel, CCdco1_pitch_lfo_source, dco1_pitch_lfo_source);
      }
      break;

    case DCO2_PITCH_LFO_SOURCE_BUTTON:
      if (!released) {
        dco2_pitch_lfo_source = dco2_pitch_lfo_source + 32;
        if (dco2_pitch_lfo_source > 96) {
          dco2_pitch_lfo_source = 0;
        }
        myControlChange(midiChannel, CCdco2_pitch_lfo_source, dco2_pitch_lfo_source);
      }
      break;

    case DCO1_PITCH_ENV_SOURCE_BUTTON:
      if (!released) {
        dco1_pitch_env_source = dco1_pitch_env_source + 32;
        if (dco1_pitch_env_source > 112) {
          dco1_pitch_env_source = 0;
          dco1_pitch_env_pol = 0;
        }
        myControlChange(midiChannel, CCdco1_pitch_env_source, dco1_pitch_env_source);
      }
      break;

    case DCO2_PITCH_ENV_SOURCE_BUTTON:
      if (!released) {
        dco2_pitch_env_source = dco2_pitch_env_source + 32;
        if (dco2_pitch_env_source > 112) {
          dco2_pitch_env_source = 0;
          dco2_pitch_env_pol = 0;
        }
        myControlChange(midiChannel, CCdco2_pitch_env_source, dco2_pitch_env_source);
      }
      break;

    case DCO1_PITCH_ENV_POLARITY_BUTTON:
      if (!released) {
        dco1_pitch_env_pol = !dco1_pitch_env_pol;
        if (dco1_pitch_env_pol) {
          dco1_pitch_env_source = dco1_pitch_env_source + 16;
        }
        if (!dco1_pitch_env_pol) {
          dco1_pitch_env_source = dco1_pitch_env_source - 16;
        }
        myControlChange(midiChannel, CCdco1_pitch_env_source, dco1_pitch_env_source);
      }
      break;

    case DCO2_PITCH_ENV_POLARITY_BUTTON:
      if (!released) {
        dco2_pitch_env_pol = !dco2_pitch_env_pol;
        if (dco2_pitch_env_pol) {
          dco2_pitch_env_source = dco2_pitch_env_source + 16;
        }
        if (!dco2_pitch_env_pol) {
          dco2_pitch_env_source = dco2_pitch_env_source - 16;
        }
        myControlChange(midiChannel, CCdco2_pitch_env_source, dco2_pitch_env_source);
      }
      break;

    case LOWER_BUTTON:
      if (!released) {
        if (state == PATCHNAMING) {
          if (patchNameCursor > 0) patchNameCursor--;
          renderPatchNaming();
          updateScreen();
          break;
        }
        editMode = 0;
        upperSW = false;
        myControlChange(midiChannel, CCeditMode, editMode);
      }
      break;

    case UPPER_BUTTON:
      if (!released) {
        if (state == PATCHNAMING) {
          if (patchNameCursor < 17) patchNameCursor++;
          renderPatchNaming();
          updateScreen();
          break;
        }
        editMode = 1;
        upperSW = true;
        myControlChange(midiChannel, CCeditMode, editMode);
      }
      break;

    case DCO_MIX_ENV_SOURCE_BUTTON:
      if (!released) {
        dco_mix_env_source = dco_mix_env_source + 32;
        if (dco_mix_env_source > 112) {
          dco_mix_env_source = 0;
          dco_mix_env_pol = 0;
        }
        myControlChange(midiChannel, CCdco_mix_env_source, dco_mix_env_source);
      }
      break;

    case DCO_MIX_ENV_POLARITY_BUTTON:
      if (!released) {
        dco_mix_env_pol = !dco_mix_env_pol;
        if (dco_mix_env_pol) {
          dco_mix_env_source = dco_mix_env_source + 16;
        }
        if (!dco_mix_env_pol) {
          dco_mix_env_source = dco_mix_env_source - 16;
        }
        myControlChange(midiChannel, CCdco_mix_env_source, dco_mix_env_source);
      }
      break;

    case DCO_MIX_DYN_BUTTON:
      if (!released) {
        dco_mix_dyn = dco_mix_dyn + 32;
        if (dco_mix_dyn > 96) {
          dco_mix_dyn = 0;
        }
        myControlChange(midiChannel, CCdco_mix_dyn, dco_mix_dyn);
      }
      break;

    case VCF_ENV_SOURCE_BUTTON:
      if (!released) {
        vcf_env_source = vcf_env_source + 32;
        if (vcf_env_source > 112) {
          vcf_env_source = 0;
          vcf_env_pol = 0;
        }
        myControlChange(midiChannel, CCvcf_env_source, vcf_env_source);
      }
      break;

    case VCF_ENV_POLARITY_BUTTON:
      if (!released) {
        vcf_env_pol = !vcf_env_pol;
        if (vcf_env_pol) {
          vcf_env_source = vcf_env_source + 16;
        }
        if (!vcf_env_pol) {
          vcf_env_source = vcf_env_source - 16;
        }
        myControlChange(midiChannel, CCvcf_env_source, vcf_env_source);
      }
      break;

    case VCF_DYN_BUTTON:
      if (!released) {
        vcf_dyn = vcf_dyn + 32;
        if (vcf_dyn > 96) {
          vcf_dyn = 0;
        }
        myControlChange(midiChannel, CCvcf_dyn, vcf_dyn);
      }
      break;

    case VCA_ENV_SOURCE_BUTTON:
      if (!released) {
        vca_env_source = vca_env_source + 32;
        if (vca_env_source > 96) {
          vca_env_source = 0;
        }
        myControlChange(midiChannel, CCvca_env_source, vca_env_source);
      }
      break;

    case VCA_DYN_BUTTON:
      if (!released) {
        vca_dyn = vca_dyn + 32;
        if (vca_dyn > 96) {
          vca_dyn = 0;
        }
        myControlChange(midiChannel, CCvca_dyn, vca_dyn);
      }
      break;

    case CHORUS_BUTTON:
      if (!released) {
        chorus = chorus + 32;
        if (chorus > 64) {
          chorus = 0;
        }
        myControlChange(midiChannel, CCchorus_sw, chorus);
      }
      break;

    case PORTAMENTO_BUTTON:
      if (!released) {
        portamento_sw = portamento_sw + 1;
        if (portamento_sw > 3) {
          portamento_sw = 0;
        }
        if (portamento_sw != 0 && (keyMode == 1 || keyMode == 2)) {
          portamento_sw = 3;
        }
        myControlChange(midiChannel, CCportamento_sw, portamento_sw);
      }
      break;

    case ENV5STAGE_SELECT_BUTTON:
      if (!released) {
        env5stage = !env5stage;
        myControlChange(midiChannel, CCenv5stage, env5stage);
      }
      break;

    case ADSR_SELECT_BUTTON:
      if (!released) {
        adsr = !adsr;
        myControlChange(midiChannel, CCadsr, adsr);
      }
      break;
  }
}

FLASHMEM void enterPatchNameEdit() {
  // Copy live patchName into buffer, padded to 18 chars.
  // Live patchName stays unchanged — it's our implicit backup for NAME cancel.
  String src = patchName;
  while (src.length() < 18) src += ' ';
  if (src.length() > 18) src = src.substring(0, 18);

  for (int i = 0; i < 18; i++) patchNameBuffer[i] = src.charAt(i);
  patchNameBuffer[18] = '\0';

  patchNameCursor = 0;
  state = PATCHNAMING;
  renderPatchNaming();
  updateScreen();
}

FLASHMEM void exitPatchNameEdit(bool commit) {
  if (commit) {
    patchName = String(patchNameBuffer);
  }
  state = PARAMETER;  // back to main screen
  renderCurrentParameterPage();
  updateScreen();
}


bool anyMuxNeedsReread() {
  for (int i = 0; i < MUXCHANNELS; i++) {
    if (mux1ValuesPrev[i] == RE_READ) return true;
    if (mux2ValuesPrev[i] == RE_READ) return true;
    if (mux3ValuesPrev[i] == RE_READ) return true;
    if (mux4ValuesPrev[i] == RE_READ) return true;
  }
  return false;
}

inline bool isRereadSentinel(int v) {
  return (v == RE_READ);
}

void checkMux() {

  if (bootInitInProgress) {
    muxInput++;
    if (muxInput >= MUXCHANNELS) muxInput = 0;
    return;
  }

  digitalWriteFast(MUX_0, muxInput & B0001);
  digitalWriteFast(MUX_1, muxInput & B0010);
  digitalWriteFast(MUX_2, muxInput & B0100);
  digitalWriteFast(MUX_3, muxInput & B1000);
  delayMicroseconds(2);

  mux1Read = adc->adc1->analogRead(MUX1_S);
  mux2Read = adc->adc1->analogRead(MUX2_S);
  mux3Read = adc->adc1->analogRead(MUX3_S);
  mux4Read = adc->adc1->analogRead(MUX4_S);

  bool reread1 = isRereadSentinel(mux1ValuesPrev[muxInput]);

  if (reread1 || mux1Read > (mux1ValuesPrev[muxInput] + QUANTISE_FACTOR) || mux1Read < (mux1ValuesPrev[muxInput] - QUANTISE_FACTOR)) {

    mux1ValuesPrev[muxInput] = mux1Read;
    mux1Read = (uint8_t)constrain(map(mux1Read, 0, 255, 0, 127), 0, 127);

    // During RE_READ pass: do not announce UI
    bool prevSuppress = suppressParamAnnounce;
    if (reread1) suppressParamAnnounce = true;

    switch (muxInput) {
      case MUX1_MOD_LFO:
        myControlChange(midiChannel, CCmod_lfo, mux1Read);
        break;
      case MUX1_LFO1_RATE:
        myControlChange(midiChannel, CClfo1_rate, mux1Read);
        break;
      case MUX1_LFO1_DELAY:
        myControlChange(midiChannel, CClfo1_delay, mux1Read);
        break;
      case MUX1_LFO1_LFO2_MOD:
        myControlChange(midiChannel, CClfo1_lfo2, mux1Read);
        break;
      case MUX1_DCO1_PW:
        myControlChange(midiChannel, CCdco1_PW, mux1Read);
        break;
      case MUX1_DCO1_PWM_ENV:
        myControlChange(midiChannel, CCdco1_PWM_env, mux1Read);
        break;
      case MUX1_DCO1_PWM_LFO:
        myControlChange(midiChannel, CCdco1_PWM_lfo, mux1Read);
        break;
      case MUX1_DCO1_PITCH_ENV:
        myControlChange(midiChannel, CCdco1_pitch_env, mux1Read);
        break;
      case MUX1_DCO1_PITCH_LFO:
        myControlChange(midiChannel, CCdco1_pitch_lfo, mux1Read);
        break;
      case MUX1_DCO1_WAVE:
        myControlChange(midiChannel, CCdco1_wave, mux1Read);
        break;
      case MUX1_DCO1_RANGE:
        myControlChange(midiChannel, CCdco1_range, mux1Read);
        break;
      case MUX1_DCO1_TUNE:
        myControlChange(midiChannel, CCdco1_tune, mux1Read);
        break;
      case MUX1_PORTAMENTO:
        myControlChange(midiChannel, CCportamento, mux1Read);
        break;
      case MUX1_LFO1_WAVE:
        myControlChange(midiChannel, CClfo1_wave, mux1Read);
        break;
    }
    suppressParamAnnounce = prevSuppress;
  }

  bool reread2 = isRereadSentinel(mux2ValuesPrev[muxInput]);

  if (reread2 || mux2Read > (mux2ValuesPrev[muxInput] + QUANTISE_FACTOR) || mux2Read < (mux2ValuesPrev[muxInput] - QUANTISE_FACTOR)) {

    mux2ValuesPrev[muxInput] = mux2Read;
    mux2Read = (uint8_t)constrain(map(mux2Read, 0, 255, 0, 127), 0, 127);

    // During RE_READ pass: do not announce UI
    bool prevSuppress = suppressParamAnnounce;
    if (reread2) suppressParamAnnounce = true;

    switch (muxInput) {
      case MUX2_BEND_RANGE:
        myControlChange(midiChannel, CCbend_range, mux2Read);
        break;
      case MUX2_LFO2_RATE:
        myControlChange(midiChannel, CClfo2_rate, mux2Read);
        break;
      case MUX2_LFO2_DELAY:
        myControlChange(midiChannel, CClfo2_delay, mux2Read);
        break;
      case MUX2_LFO2_LFO1_MOD:
        myControlChange(midiChannel, CClfo2_lfo1, mux2Read);
        break;
      case MUX2_DCO2_PW:
        myControlChange(midiChannel, CCdco2_PW, mux2Read);
        break;
      case MUX2_DCO2_PWM_ENV:
        myControlChange(midiChannel, CCdco2_PWM_env, mux2Read);
        break;
      case MUX2_DCO2_PWM_LFO:
        myControlChange(midiChannel, CCdco2_PWM_lfo, mux2Read);
        break;
      case MUX2_DCO2_PITCH_ENV:
        myControlChange(midiChannel, CCdco2_pitch_env, mux2Read);
        break;
      case MUX2_DCO2_PITCH_LFO:
        myControlChange(midiChannel, CCdco2_pitch_lfo, mux2Read);
        break;
      case MUX2_DCO2_WAVE:
        myControlChange(midiChannel, CCdco2_wave, mux2Read);
        break;
      case MUX2_DCO2_RANGE:
        myControlChange(midiChannel, CCdco2_range, mux2Read);
        break;
      case MUX2_DCO2_TUNE:
        myControlChange(midiChannel, CCdco2_tune, mux2Read);
        break;
      case MUX2_DCO2_FINE:
        myControlChange(midiChannel, CCdco2_fine, mux2Read);
        break;
      case MUX2_DCO1_MODE:
        myControlChange(midiChannel, CCdco1_xmod, mux2Read);
        break;
      case MUX2_LFO2_WAVE:
        myControlChange(midiChannel, CClfo2_wave, mux2Read);
        break;
    }
    suppressParamAnnounce = prevSuppress;
  }

  bool reread3 = isRereadSentinel(mux3ValuesPrev[muxInput]);

  if (reread3 || mux3Read > (mux3ValuesPrev[muxInput] + QUANTISE_FACTOR) || mux3Read < (mux3ValuesPrev[muxInput] - QUANTISE_FACTOR)) {

    mux3ValuesPrev[muxInput] = mux3Read;
    mux3Read = (uint8_t)constrain(map(mux3Read, 0, 255, 0, 127), 0, 127);

    // During RE_READ pass: do not announce UI
    bool prevSuppress = suppressParamAnnounce;
    if (reread3) suppressParamAnnounce = true;

    switch (muxInput) {
      case MUX3_DCO1_LEVEL:
        myControlChange(midiChannel, CCdco1_level, mux3Read);
        break;
      case MUX3_DCO2_LEVEL:
        myControlChange(midiChannel, CCdco2_level, mux3Read);
        break;
      case MUX3_DCO2_MOD:
        myControlChange(midiChannel, CCdco2_mod, mux3Read);
        break;
      case MUX3_VCF_HPF:
        myControlChange(midiChannel, CCvcf_hpf, mux3Read);
        break;
      case MUX3_VCF_CUTOFF:
        myControlChange(midiChannel, CCvcf_cutoff, mux3Read);
        break;
      case MUX3_VCF_RES:
        myControlChange(midiChannel, CCvcf_res, mux3Read);
        break;
      case MUX3_VCF_KB:
        myControlChange(midiChannel, CCvcf_kb, mux3Read);
        break;
      case MUX3_VCF_ENV:
        myControlChange(midiChannel, CCvcf_env, mux3Read);
        break;
      case MUX3_VCF_LFO1:
        myControlChange(midiChannel, CCvcf_lfo1, mux3Read);
        break;
      case MUX3_VCF_LFO2:
        myControlChange(midiChannel, CCvcf_lfo2, mux3Read);
        break;
      case MUX3_VCA_MOD:
        myControlChange(midiChannel, CCvca_mod, mux3Read);
        break;
      case MUX3_AT_VIB:
        myControlChange(midiChannel, CCat_vib, mux3Read);
        break;
      case MUX3_AT_BRI:
        myControlChange(midiChannel, CCat_bri, mux3Read);
        break;
      case MUX3_AT_VOL:
        myControlChange(midiChannel, CCat_vol, mux3Read);
        break;
      case MUX3_BALANCE:
        myControlChange(midiChannel, CCbalance, mux3Read);
        break;
      case MUX3_VOLUME:
        myControlChange(midiChannel, CCvolume, mux3Read);
        break;
    }
    suppressParamAnnounce = prevSuppress;
  }

  bool reread4 = isRereadSentinel(mux4ValuesPrev[muxInput]);

  if (reread4 || mux4Read > (mux4ValuesPrev[muxInput] + QUANTISE_FACTOR) || mux4Read < (mux4ValuesPrev[muxInput] - QUANTISE_FACTOR)) {

    mux4ValuesPrev[muxInput] = mux4Read;
    mux4Read = (uint8_t)constrain(map(mux4Read, 0, 255, 0, 127), 0, 127);

    // During RE_READ pass: do not announce UI
    bool prevSuppress = suppressParamAnnounce;
    if (reread4) suppressParamAnnounce = true;

    switch (muxInput) {
      case MUX4_T1:
        if (!env5stage) {
          myControlChange(midiChannel, CCtime1, mux4Read);
        } else {
          myControlChange(midiChannel, CC2time1, mux4Read);
        }
        break;
      case MUX4_L1:
        if (!env5stage) {
          myControlChange(midiChannel, CClevel1, mux4Read);
        } else {
          myControlChange(midiChannel, CC2level1, mux4Read);
        }
        break;
      case MUX4_T2:
        if (!env5stage) {
          myControlChange(midiChannel, CCtime2, mux4Read);
        } else {
          myControlChange(midiChannel, CC2time2, mux4Read);
        }
        break;
      case MUX4_L2:
        if (!env5stage) {
          myControlChange(midiChannel, CClevel2, mux4Read);
        } else {
          myControlChange(midiChannel, CC2level2, mux4Read);
        }
        break;
      case MUX4_T3:
        if (!env5stage) {
          myControlChange(midiChannel, CCtime3, mux4Read);
        } else {
          myControlChange(midiChannel, CC2time3, mux4Read);
        }
        break;
      case MUX4_L3:
        if (!env5stage) {
          myControlChange(midiChannel, CClevel3, mux4Read);
        } else {
          myControlChange(midiChannel, CC2level3, mux4Read);
        }
        break;
      case MUX4_T4:
        if (!env5stage) {
          myControlChange(midiChannel, CCtime4, mux4Read);
        } else {
          myControlChange(midiChannel, CC2time4, mux4Read);
        }
        break;
      case MUX4_5STAGE_MODE:
        if (!env5stage) {
          myControlChange(midiChannel, CC5stage_mode, mux4Read);
        } else {
          myControlChange(midiChannel, CC25stage_mode, mux4Read);
        }
        break;
      case MUX4_ATTACK:
        if (!adsr) {
          myControlChange(midiChannel, CCattack, mux4Read);
        } else {
          myControlChange(midiChannel, CC4attack, mux4Read);
        }
        break;
      case MUX4_DECAY:
        if (!adsr) {
          myControlChange(midiChannel, CCdecay, mux4Read);
        } else {
          myControlChange(midiChannel, CC4decay, mux4Read);
        }
        break;
      case MUX4_SUSTAIN:
        if (!adsr) {
          myControlChange(midiChannel, CCsustain, mux4Read);
        } else {
          myControlChange(midiChannel, CC4sustain, mux4Read);
        }
        break;
      case MUX4_RELEASE:
        if (!adsr) {
          myControlChange(midiChannel, CCrelease, mux4Read);
        } else {
          myControlChange(midiChannel, CC4release, mux4Read);
        }
        break;
      case MUX4_ADSR_MODE:
        if (!adsr) {
          myControlChange(midiChannel, CCadsr_mode, mux4Read);
        } else {
          myControlChange(midiChannel, CC4adsr_mode, mux4Read);
        }
        break;
      case MUX4_DUAL_DETUNE:
        myControlChange(midiChannel, CCdualdetune, mux4Read);
        break;
      case MUX4_UNISON_DETUNE:
        myControlChange(midiChannel, CCunisondetune, mux4Read);
        break;
    }
    suppressParamAnnounce = prevSuppress;
  }

  muxInput++;
  if (muxInput >= MUXCHANNELS) {
    muxInput = 0;
  }

  if (manualSyncInProgress && !anyMuxNeedsReread()) {
    manualSyncInProgress = false;
    suppressParamAnnounce = false;
  }
}

void loop() {

  myusb.Task();
  midi1.read();
  usbMIDI.read(midiChannel);
  MIDI.read(midiChannel);

  if (!receivingSysEx) {
    checkMux();
    checkEncoder();
    checkSwitches();
    pollAllMCPs();

    arpEngine();
    arpServiceLed();
    chaseService();
    updateToneBlink();
  }



  if (waitingToUpdate && (millis() - lastDisplayTriggerTime >= displayTimeout)) {
    updateScreen();  // retrigger
    waitingToUpdate = false;
  }

  // Handle VOICE1_RESET trigger
  if (voice1ResetTriggered) {
    voice1ResetTriggered = false;  // Clear flag first
    mcp9.digitalWrite(PATCH_LED_RED, LOW);
  }

  // Handle VOICE2_RESET trigger
  if (voice2ResetTriggered) {
    voice2ResetTriggered = false;  // Clear flag first
    mcp9.digitalWrite(TONE_LED_RED, LOW);
  }

  if (sysexComplete) {
    noInterrupts();
    sysexComplete = false;
    receivingSysEx = false;
    interrupts();
    decodeAndStoreDump();
    noInterrupts();
    dumpRawLen = 0;
    interrupts();
  }
}