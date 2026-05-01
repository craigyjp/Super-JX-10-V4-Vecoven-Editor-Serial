#include "PatchService.h"

// ============================================================================
// Patch.h — PATCH menu handlers and parameter table
//
// All handlers write to the live in-memory patch data (globals in
// Parameters.h and upperData[]/lowerData[] arrays).  A Save button press
// elsewhere in the code commits these to SD.
//
// At the bottom of each handler there is a // TODO: send-to-voice-boards
// comment where you should invoke the existing sender function(s) you
// already use elsewhere when these variables change.
// ============================================================================
void sendCustomSysEx(byte outChannel, byte parameter, byte value);


// Menu index 0..5  ↔  storage value 0,1,2,4,5,6 (bit 2 skips 3)
static inline int packAssign(int menuIdx) {
  // 0→0, 1→1, 2→2, 3→4, 4→5, 5→6
  return (menuIdx < 3) ? menuIdx : (menuIdx + 1);
}
static inline int unpackAssign(int stored) {
  // 0→0, 1→1, 2→2, 4→3, 5→4, 6→5
  return (stored < 3) ? stored : (stored - 1);
}

static void sendOneAssign(bool upper) {
  uint8_t addr = upper ? 0x1F : 0x28;
  uint8_t val  = upper ? (uint8_t)upperAssign : (uint8_t)lowerAssign;
  sendCustomSysEx((midiOutCh - 1), addr, val);
}

static inline int packScaleP(int idx, int N) {
  if (idx < 0)       idx = 0;
  if (idx > N - 1)   idx = N - 1;
  return ((idx * 127) + ((N - 1) / 2)) / (N - 1);
}
static inline int unpackScaleP(int stored, int N) {
  int v = stored & 0x7F;
  return ((v * (N - 1)) + 63) / 127;
}

static inline int packSplitDetune(int idx) {
  if (idx < 0)   idx = 0;
  if (idx > 101) idx = 101;
  if (idx <= 50) {
    // Negative half: idx 0 -> 0x00, idx 50 -> 0x3F
    return (idx * 0x3F + 25) / 50;
  } else {
    // Positive half: idx 51 -> 0x40, idx 101 -> 0x7F
    return 0x40 + ((idx - 51) * 0x3F + 25) / 50;
  }
}
static inline int unpackSplitDetune(int stored) {
  int v = stored & 0x7F;
  if (v < 0x40) {
    // Negative half: 0x00 -> 0, 0x3F -> 50
    return (v * 50 + 0x1F) / 0x3F;
  } else {
    // Positive half: 0x40 -> 51, 0x7F -> 101
    return 51 + ((v - 0x40) * 50 + 0x1F) / 0x3F;
  }
}

// ---- "-50..-00, +00..+50" display for DCO2 fine tune (102 UI positions) ----
// UI 0..50   -> "-50".."-00"  (stored 0x00..0x3F)
// UI 51..101 -> "+00".."+50"  (stored 0x40..0x7F)
static char patchDetuneLabels[102][5];
static const char *patchDetunePtrs[103];

static void buildPatchDetuneLabels() {
  for (int i = 0; i < 51; i++) {
    // negative side: -50 at i=0, -00 at i=50
    snprintf(patchDetuneLabels[i], 5, "-%02d", 50 - i);
    patchDetunePtrs[i] = patchDetuneLabels[i];
  }
  for (int i = 51; i < 102; i++) {
    // positive side: +00 at i=51, +50 at i=101
    snprintf(patchDetuneLabels[i], 5, "+%02d", i - 51);
    patchDetunePtrs[i] = patchDetuneLabels[i];
  }
  patchDetunePtrs[102] = "\0";
}

// ─────────────────────────────────────────────
// Shared 0..127 decimal label table
// Many MKS-70 parameters are 0..127; one table serves them all.
// ─────────────────────────────────────────────
static char patch128Labels[128][4];
static const char *patch128Ptrs[129];  // 128 + sentinel

static void buildPatch128Labels() {
  for (int i = 0; i < 128; i++) {
    snprintf(patch128Labels[i], 4, "%d", i);
    patch128Ptrs[i] = patch128Labels[i];
  }
  patch128Ptrs[128] = "\0";
}

// ─────────────────────────────────────────────
// Shared 0..99 decimal label table (tone numbers)
// ─────────────────────────────────────────────
static char patch100Labels[100][4];
static const char *patch100Ptrs[101];

static void buildPatch100Labels() {
  for (int i = 0; i < 100; i++) {
    snprintf(patch100Labels[i], 4, "%d", i);
    patch100Ptrs[i] = patch100Labels[i];
  }
  patch100Ptrs[100] = "\0";
}

// ─────────────────────────────────────────────
// Note-name label table for split points
// Anchor: stored 38 = B3, stored 39 = C4 (one semitone per unit).
// Derived:  octave = (stored + 21) / 12 - 1
//           pitchClass = (stored + 21) % 12
// Valid keyboard range is stored 7..82 (E1..G7) but we label all 128
// positions so loaded patches never display blank.
// ─────────────────────────────────────────────
static char patchNoteLabels[128][5];
static const char *patchNotePtrs[129];

static void buildPatchNoteLabels() {
  static const char * const pc[] = {
    "C","C#","D","D#","E","F","F#","G","G#","A","A#","B"
  };
  for (int s = 0; s < 128; s++) {
    int n    = s + 21;           // shift so stored 39 -> n=60 -> C4
    int oct  = (n / 12) - 1;
    int pidx = n % 12;
    snprintf(patchNoteLabels[s], 5, "%s%d", pc[pidx], oct);
    patchNotePtrs[s] = patchNoteLabels[s];
  }
  patchNotePtrs[128] = "\0";
}

// ─────────────────────────────────────────────
// Chromatic shift label table (-24..+24 semitones)
// Stored value:  0x00..0x18 = 0..+24,  0x68..0x7F = -24..-1
// Menu index runs 0..48  →  -24..+24 in order.
// ─────────────────────────────────────────────
static char patchShiftLabels[49][6];
static const char *patchShiftPtrs[50];

static void buildPatchShiftLabels() {
  for (int i = 0; i < 49; i++) {
    int semis = i - 24;
    snprintf(patchShiftLabels[i], 6, "%+d", semis);
    patchShiftPtrs[i] = patchShiftLabels[i];
  }
  patchShiftPtrs[49] = "\0";
}

// Convert chromatic-shift menu index (0..48) → raw hex value.
static inline uint8_t shiftIndexToRaw(int idx) {
  int semis = idx - 24;
  if (semis >= 0) return (uint8_t)semis;           // 0..+24  →  0x00..0x18
  return (uint8_t)(0x80 + semis);                  // -24..-1 →  0x68..0x7F
}

// Convert raw hex → menu index.
static inline int shiftRawToIndex(uint8_t raw) {
  if (raw <= 0x18) return raw + 24;                // 0..+24
  if (raw >= 0x68) return (int)raw - 0x80 + 24;    // -24..-1
  return 24;                                        // fallback = 0
}

// ─────────────────────────────────────────────
// Bend range labels (5 steps)
// Stored in bend_range param upper nibble:
// 0x00=2  0x20=3  0x40=4  0x60=7;   12 semitones = separate 34H flag 0x01
// ─────────────────────────────────────────────
static const char *patchBendRangeValues[] = { "2", "3", "4", "7", "12", "\0" };

// ─────────────────────────────────────────────
// Key mode labels (aa = 18H)
// Table per MKS-70 spec. Adjust labels if your conversion table differs.
// ─────────────────────────────────────────────
static const char *patchKeyModeValues[] = {
  "DUAL", "UP WHOLE", "LO WHOLE", "SPLIT", "T-VOICE", "X-FADE", "\0"
};

// ─────────────────────────────────────────────
// Key assign labels (1FH / 28H)
// 00 Poly1, 01 Uni1, 02 Mono1, 04 Poly2, 05 Uni2, 06 Mono2
// Menu index 0..5  ↔  stored value via tables below.
// ─────────────────────────────────────────────
static const char *patchKeyAssignValues[] = {
  "POLY 1", "UNI 1", "MONO 1", "POLY 2", "UNI 2", "MONO 2", "\0"
};
static const uint8_t patchKeyAssignRaw[] = { 0x00, 0x01, 0x02, 0x04, 0x05, 0x06 };

static inline int assignRawToIndex(uint8_t raw) {
  for (int i = 0; i < 6; i++) if (patchKeyAssignRaw[i] == raw) return i;
  return 0;
}

// ─────────────────────────────────────────────
// On/Off (hold, portamento, bender per tone, chase play)
// ─────────────────────────────────────────────
static const char *patchOnOffValues[] = { "OFF", "ON", "\0" };

// ─────────────────────────────────────────────
// Chase mode (30H): 0=U-L, 1=U-L-L-, 2=U-L-U-
// ─────────────────────────────────────────────
static const char *patchChaseModeValues[] = { "U-L", "U-L-L-", "U-L-U-", "\0" };

// ============================================================================
// Forward declarations — handlers and currentIndex callbacks
// ============================================================================

// Patch-scope handlers
void patchBalance(int index, const char *value);
void patchDualDetune(int index, const char *value);
void patchUpperSplitPoint(int index, const char *value);
void patchLowerSplitPoint(int index, const char *value);
void patchPortamentoTime(int index, const char *value);
void patchBendRange(int index, const char *value);
void patchKeyModeAA(int index, const char *value);
void patchKeyModeBB(int index, const char *value);
void patchVolume(int index, const char *value);
void patchAftertouchVib(int index, const char *value);
void patchAftertouchLPF(int index, const char *value);
void patchAftertouchVol(int index, const char *value);

// Upper-tone-scope (per-patch) handlers
void patchUpperToneNumber(int index, const char *value);
void patchUpperShift(int index, const char *value);
void patchUpperAssign(int index, const char *value);
void patchUpperUnisonDet(int index, const char *value);
void patchUpperHold(int index, const char *value);
void patchUpperLFOMod(int index, const char *value);
void patchUpperPortamento(int index, const char *value);
void patchUpperBender(int index, const char *value);

// Lower-tone-scope (per-patch) handlers
void patchLowerToneNumber(int index, const char *value);
void patchLowerShift(int index, const char *value);
void patchLowerAssign(int index, const char *value);
void patchLowerUnisonDet(int index, const char *value);
void patchLowerHold(int index, const char *value);
void patchLowerLFOMod(int index, const char *value);
void patchLowerPortamento(int index, const char *value);
void patchLowerBender(int index, const char *value);

// Chase handlers
void patchChaseLevel(int index, const char *value);
void patchChaseMode(int index, const char *value);
void patchChaseTime(int index, const char *value);
void patchChasePlay(int index, const char *value);

// currentIndex callbacks — all declared together
int idxBalance();
int idxDualDetune();
int idxUpperSplitPoint();
int idxLowerSplitPoint();
int idxPortamentoTime();
int idxBendRange();
int idxKeyModeAA();
int idxKeyModeBB();
int idxVolume();
int idxAftertouchVib();
int idxAftertouchLPF();
int idxAftertouchVol();
int idxUpperToneNumber();
int idxUpperShift();
int idxUpperAssign();
int idxUpperUnisonDet();
int idxUpperHold();
int idxUpperLFOMod();
int idxUpperPortamento();
int idxUpperBender();
int idxLowerToneNumber();
int idxLowerShift();
int idxLowerAssign();
int idxLowerUnisonDet();
int idxLowerHold();
int idxLowerLFOMod();
int idxLowerPortamento();
int idxLowerBender();
int idxChaseLevel();
int idxChaseMode();
int idxChaseTime();
int idxChasePlay();

void updatekeyMode(bool announce);
void updateassignMode(bool announce);
void updateupperchromaticshift();
void updatelowerchromaticshift();
void updateuppersplitPoints(bool announce);
void updatelowersplitPoints(bool announce);
void updateat_vib(bool announce);
void updateat_bri(bool announce);
void updateat_vol(bool announce);
void updatebalance(bool announce);
void updatevolume(bool announce);
void updateportamento(bool announce);
void updatedualdetune(bool announce);
void updateupperunisonDetune(bool announce);
void updatelowerunisonDetune(bool announce);
void updatebend_enable_button(bool announce);
void updatebend_range(bool announce);
void updateuppermod_lfo(bool announce);
void updatelowermod_lfo(bool announce);
void updateupperHold();
void updatelowerHold();
void updateafter_vib_enable_button(bool announce);
void updateafter_bri_enable_button(bool announce);
void updateafter_vol_enable_button(bool announce);
void updateportamento_sw(bool announce);
void updatechaseLevel(bool announce);
void updatechaseMode(bool announce);
void updatechaseTime(bool announce);
void updatechasePlay(bool announce);
void updateAssignLeds();

// ============================================================================
// Handlers — each writes to the live global, then yields so main.ino
// can send to hardware.  The TODO comments mark the exact spot.
// ============================================================================

// -------- 12H U/L Balance --------
void patchBalance(int index, const char *value) {
  balance = packScaleP(index, 100);
  updatebalance(0);
}
int idxBalance() { return unpackScaleP(balance, 100); }

void patchDualDetune(int index, const char *value) {
  dualdetune = packSplitDetune(index);
  updatedualdetune(0);
}
int idxDualDetune() { return unpackSplitDetune(dualdetune); }


// -------- 14H Upper Split Point --------
void patchUpperSplitPoint(int index, const char *value) {
  // MKS-70 splits are stored in a single splitPoint byte in your code.
  // If you need upper/lower split separately, add a second variable.
  upperSplitPoint = (byte)index;
  updateuppersplitPoints(0);
}
int idxUpperSplitPoint() { return upperSplitPoint & 0x7F; }

// -------- 15H Lower Split Point --------
void patchLowerSplitPoint(int index, const char *value) {
  lowerSplitPoint = (byte)index;  // placeholder — rename/rewire if you have a
                             // dedicated lower-split variable
  updatelowersplitPoints(0);
}
int idxLowerSplitPoint() { return lowerSplitPoint & 0x7F; }

// -------- 16H Portamento Time --------
void patchPortamentoTime(int index, const char *value) {
  portamento = packScaleP(index, 100);
  updateportamento(0);
}
int idxPortamentoTime() { return unpackScaleP(portamento, 100); }

// -------- 17H Bend Range (+ 34H 12-semi flag) --------
void patchBendRange(int index, const char *value) {
  static const uint8_t bendRangeStored[5] = { 0, 32, 64, 96, 127 };
  if (index < 0) index = 0;
  if (index > 4) index = 4;
  bend_range = bendRangeStored[index];
  updatebend_range(0);
}
int idxBendRange() {
  return map(bend_range, 0, 127, 0, 4);
}

// -------- 18H Key mode aa --------
void patchKeyModeAA(int index, const char *value) {
  keyMode = index;
  updatekeyMode(0);
}
int idxKeyModeAA() {
  if (keyMode < 0) return 0;
  if (keyMode > 5) return 5;
  return keyMode;
}

// -------- 33H Key mode bb --------
// Placeholder — you'll need a variable for this if you're not tracking it yet.
static int keyModeBB = 0;
void patchKeyModeBB(int index, const char *value) {
  keyModeBB = index;
  // TODO: send
}
int idxKeyModeBB() { return keyModeBB & 0x07; }

// -------- 19H Total Volume --------
void patchVolume(int index, const char *value) {
  volume = packScaleP(index, 100);
  updatevolume(0);
}
int idxVolume() { return unpackScaleP(volume, 100); }

// -------- 1AH Aftertouch Vibrato --------
void patchAftertouchVib(int index, const char *value) {
  at_vib = packScaleP(index, 100);
  updateat_vib(0);
}
int idxAftertouchVib() { return unpackScaleP(at_vib, 100); }

// -------- 1BH Aftertouch Brilliance --------
void patchAftertouchLPF(int index, const char *value) {
  at_bri = packScaleP(index, 100);
  updateat_bri(0);
}
int idxAftertouchLPF() { return unpackScaleP(at_bri, 100); }

// -------- 1CH Aftertouch Volume --------
void patchAftertouchVol(int index, const char *value) {
  at_vol = packScaleP(index, 100);
  updateat_vol(0);
}
int idxAftertouchVol() { return unpackScaleP(at_vol, 100); }

// ============================================================================
// Upper tone (per-patch) fields — stored in upperData[]
// Note: these are per-PATCH mappings of upper-section behaviour (key assign,
// tone number, etc.) — NOT the tone synth parameters, which live in the
// TONE menu (ToneService).
// ============================================================================

// -------- 1DH Upper Tone Number --------
void patchUpperToneNumber(int index, const char *value) {
  upperToneNumber = index;
  // TODO: load the tone from memory and send the full tone to the upper board
}
int idxUpperToneNumber() {
  int v = upperToneNumber;
  return (v < 0) ? 0 : (v > 99 ? 99 : v);
}

// -------- 1EH Upper Chromatic Shift --------
void patchUpperShift(int index, const char *value) {
  upperChromatic = shiftIndexToRaw(index);
  // You likely have upperTranspose / upperShift somewhere — wire into it:
  updateupperchromaticshift();
}
int idxUpperShift() {
  return shiftRawToIndex(upperChromatic);
}

// -------- 1FH Upper Key Assign --------
void patchUpperAssign(int index, const char *value) {
  upperAssign = packAssign(index);
  // Send SYX + refresh LEDs
  sendOneAssign(true);
  updateAssignLeds();
}
int idxUpperAssign() { return unpackAssign(upperAssign); }

// -------- 20H Upper Unison Detune --------
void patchUpperUnisonDet(int index, const char *value) {
  upperUnisonDetune = packSplitDetune(index);
  updateupperunisonDetune(0);
}
int idxUpperUnisonDet() { return unpackSplitDetune(upperUnisonDetune); }

// -------- 21H Upper Hold --------
void patchUpperHold(int index, const char *value) {
  holdManualUpper = (index == 1);
  updateupperHold();
}
int idxUpperHold() { return holdManualUpper ? 1 : 0; }

// -------- 22H Upper LFO mod depth --------
void patchUpperLFOMod(int index, const char *value) {
  upperLFOModDepth = packScaleP(index, 100);
  updateuppermod_lfo(0);
}
int idxUpperLFOMod() { return unpackScaleP(upperLFOModDepth, 100); }

// -------- 23H Upper Portamento ON/OFF --------
void patchUpperPortamento(int index, const char *value) {
  upperPortamento_SW = index;
  updateportamento_sw(0);
}
int idxUpperPortamento() { return upperPortamento_SW ? 1 : 0; }

// -------- 24H Upper Bender --------
void patchUpperBender(int index, const char *value) {
  upperbend_enable_SW = index;
  updatebend_enable_button(0);
}
int idxUpperBender() { return upperbend_enable_SW ? 1 : 0; }

// ============================================================================
// Lower tone (per-patch) fields — stored in lowerData[]
// ============================================================================

void patchLowerToneNumber(int index, const char *value) {
  lowerToneNumber = index;
  // TODO: load + send to lower board
}
int idxLowerToneNumber() {
  int v = lowerToneNumber;
  return (v < 0) ? 0 : (v > 99 ? 99 : v);
}

void patchLowerShift(int index, const char *value) {
  lowerChromatic = shiftIndexToRaw(index);
  updatelowerchromaticshift();
}
int idxLowerShift() { 
  return shiftRawToIndex(lowerChromatic); 
}  

void patchLowerAssign(int index, const char *value) {
  lowerAssign = packAssign(index);
  sendOneAssign(false);
  updateAssignLeds();
}
int idxLowerAssign() { return unpackAssign(lowerAssign); }

void patchLowerUnisonDet(int index, const char *value) {
  lowerUnisonDetune = packSplitDetune(index);
  updatelowerunisonDetune(0);
}
int idxLowerUnisonDet() { return unpackSplitDetune(lowerUnisonDetune); }

void patchLowerHold(int index, const char *value) {
  holdManualLower = (index == 1);
  updatelowerHold();
}
int idxLowerHold() { return holdManualLower ? 1 : 0; }

void patchLowerLFOMod(int index, const char *value) {
  lowerLFOModDepth = packScaleP(index, 100);
  updatelowermod_lfo(0);
}
int idxLowerLFOMod() { return unpackScaleP(lowerLFOModDepth, 100); }

void patchLowerPortamento(int index, const char *value) {
  lowerPortamento_SW = index;
  updateportamento_sw(0);
}
int idxLowerPortamento() { return lowerPortamento_SW ? 1 : 0; }

void patchLowerBender(int index, const char *value) {
  lowerbend_enable_SW = index;
  updatebend_enable_button(0);
}
int idxLowerBender() { return lowerbend_enable_SW ? 1 : 0; }

// ============================================================================
// Chase
// ============================================================================

void patchChaseLevel(int index, const char *value) {
  chaseLevel = packScaleP(index, 100);
  updatechaseLevel(0);
}
int idxChaseLevel() { return unpackScaleP(chaseLevel, 100); }

void patchChaseMode(int index, const char *value) {
  chaseMode = index;
  updatechaseMode(0);
}
int idxChaseMode() {
  if (chaseMode < 0) return 0;
  if (chaseMode > 2) return 2;
  return chaseMode;
}

void patchChaseTime(int index, const char *value) {
  chaseTime = packScaleP(index, 100);
  updatechaseTime(0);
}
int idxChaseTime() { return unpackScaleP(chaseTime, 100); }

void patchChasePlay(int index, const char *value) {
  chasePlay = index;
  updatechasePlay(0);
}
int idxChasePlay() { return chasePlay ? 1 : 0; }

// ============================================================================
// setUpPatch() — build label tables, then register every option.
// Order of append() calls determines encoder scroll order.
// ============================================================================

void setUpPatch() {
  buildPatch128Labels();
  buildPatch100Labels();
  buildPatchNoteLabels();
  buildPatchShiftLabels();
  buildPatchDetuneLabels();

  // 12H..1CH — patch-scope
  patchmenu::append({ "11 U/L BALANCE",   patch100Ptrs, 100, patchBalance,         idxBalance         });
  patchmenu::append({ "12 DUAL DETUNE",   patchDetunePtrs, 102, patchDualDetune,   idxDualDetune      });
  patchmenu::append({ "13 UPPER SPLIT POINT",   patchNotePtrs, 128, patchUpperSplitPoint, idxUpperSplitPoint });
  patchmenu::append({ "14 LOWER SPLIT POINT",   patchNotePtrs, 128, patchLowerSplitPoint, idxLowerSplitPoint });
  patchmenu::append({ "15 PORTAMENTO TIME",   patch100Ptrs, 100, patchPortamentoTime,  idxPortamentoTime  });
  patchmenu::append({ "16 BEND RANGE",    patchBendRangeValues, 5, patchBendRange, idxBendRange       });
  patchmenu::append({ "17 KEY  MODE",      patchKeyModeValues,   6, patchKeyModeAA, idxKeyModeAA       });
  patchmenu::append({ "18 TOTAL VOLUME",  patch100Ptrs, 100, patchVolume,          idxVolume          });
  patchmenu::append({ "21 AFTER TOUCH VIB",    patch100Ptrs, 100, patchAftertouchVib,   idxAftertouchVib   });
  patchmenu::append({ "22 AFTER TOUCH BRI", patch100Ptrs, 100, patchAftertouchLPF,   idxAftertouchLPF   });
  patchmenu::append({ "23 AFTER TOUCH VOL",     patch100Ptrs, 100, patchAftertouchVol,   idxAftertouchVol   });

  // 1DH..24H — upper
  patchmenu::append({ "31 UPPER TONE",  patch100Ptrs, 100, patchUpperToneNumber, idxUpperToneNumber });
  patchmenu::append({ "32 UP CHROMATIC SHIFT",   patchShiftPtrs, 49, patchUpperShift,     idxUpperShift      });
  patchmenu::append({ "33 UPPER KEY ASSIGN",  patchKeyAssignValues, 6, patchUpperAssign, idxUpperAssign   });
  patchmenu::append({ "34 UPPER UNISON DETUNE", patchDetunePtrs, 102, patchUpperUnisonDet,  idxUpperUnisonDet  });
  patchmenu::append({ "35 UPPER HOLD",    patchOnOffValues, 2, patchUpperHold,     idxUpperHold       });
  patchmenu::append({ "36 UPPER LFO MOD DEPTH", patch100Ptrs, 100, patchUpperLFOMod,     idxUpperLFOMod     });
  patchmenu::append({ "37 UPPER PORTAMENTO",  patchOnOffValues, 2, patchUpperPortamento, idxUpperPortamento });
  patchmenu::append({ "38 UPPER BENDER",  patchOnOffValues, 2, patchUpperBender,   idxUpperBender     });

  // 26H..2DH — lower
  patchmenu::append({ "41 LOWER TONE",  patch100Ptrs, 100, patchLowerToneNumber, idxLowerToneNumber });
  patchmenu::append({ "42 LO CHROMATIC SHIFT",   patchShiftPtrs, 49, patchLowerShift,     idxLowerShift      });
  patchmenu::append({ "43 LOWER KEY ASSIGN",  patchKeyAssignValues, 6, patchLowerAssign, idxLowerAssign   });
  patchmenu::append({ "44 LOWER UNISON DETUNE", patchDetunePtrs, 102, patchLowerUnisonDet,  idxLowerUnisonDet  });
  patchmenu::append({ "45 LOWER HOLD",    patchOnOffValues, 2, patchLowerHold,     idxLowerHold       });
  patchmenu::append({ "46 LOWER LFO MOD DEPTH", patch100Ptrs, 100, patchLowerLFOMod,     idxLowerLFOMod     });
  patchmenu::append({ "47 LOWER PORTAMENTO",  patchOnOffValues, 2, patchLowerPortamento, idxLowerPortamento });
  patchmenu::append({ "48 LOWER BENDER",  patchOnOffValues, 2, patchLowerBender,   idxLowerBender     });

  // 2FH..32H — chase
  patchmenu::append({ "51 CHASE PLAY LEVEL",   patch100Ptrs, 100, patchChaseLevel,      idxChaseLevel      });
  patchmenu::append({ "52 CHASE PLAY MODE",    patchChaseModeValues, 3, patchChaseMode, idxChaseMode       });
  patchmenu::append({ "53 CHASE PLAY TIME",    patch100Ptrs, 100, patchChaseTime,       idxChaseTime       });
  patchmenu::append({ "54 CHASE PLAY SWITCH",   patchOnOffValues, 2, patchChasePlay,     idxChasePlay       });
}