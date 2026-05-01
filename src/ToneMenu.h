#include "ToneService.h"

// ============================================================================
// Tone.h — TONE menu
//
// All handlers read/write the currently-selected tone based on upperSW.
// upperSW == true  -> edit upperData[]
// upperSW == false -> edit lowerData[]
//
// When the user presses UPPER or LOWER while inside the TONE menu, the
// main.ino button handler should call tonemenu::refresh_value() so the
// displayed value re-reads from the newly active tone while keeping the
// same parameter index selected.
//
// STORAGE CONVENTION
// ------------------
// upperData[] / lowerData[] hold the packed voice-board byte (matches the
// Vecoven SYX wire format). The TONE menu passes/receives a compact UI
// index (0..N-1). The packStep()/unpackStep() helpers translate at the
// boundary so the menu never sees packed bytes, and the voice-board I/O
// never sees UI indices.
//
//   Switch options   Step    Stored byte sequence
//   ---------------  ------  --------------------------------
//   2                0x40    0x00, 0x40
//   3                0x20    0x00, 0x20, 0x40
//   4                0x20    0x00, 0x20, 0x40, 0x60
//   5                0x10    0x00, 0x10, 0x20, 0x30, 0x40
//   8                0x10    0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70
//   128 (continuous) 1       0x00..0x7F (identity)
//
// Nothing ever exceeds 0x7F by construction.
// ============================================================================

extern bool upperSW;  // declared in Parameters.h

void updatedco1_range(bool announce);
void updatedco1_wave(bool announce);
void updatedco1_tune(bool announce);
void updatedco1_pitch_lfo(bool announce);
void updatedco1_pitch_lfo_source(bool announce);
void updatedco1_pitch_env(bool announce);
void updatedco1_pitch_env_source(bool announce);
void updatedco1_pitch_dyn(bool announce);

void updatedco2_range(bool announce);
void updatedco2_wave(bool announce);
void updatedco2_tune(bool announce);
void updatedco2_pitch_lfo(bool announce);
void updatedco2_pitch_lfo_source(bool announce);
void updatedco2_pitch_env(bool announce);
void updatedco2_pitch_env_source(bool announce);
void updatedco2_pitch_dyn(bool announce);

void updatedco1_xmod(bool announce);
void updatedco2_fine(bool announce);
void updatevcf_hpf(bool announce);
void updatechorus(bool announce);

void updatedco1_PW(bool announce);
void updatedco1_PWM_env(bool announce);
void updatedco1_PWM_lfo(bool announce);
void updatedco1_PWM_lfo_source(bool announce);
void updatedco1_PWM_env_source(bool announce);
void updatedco1_PWM_dyn(bool announce);

void updatedco2_PW(bool announce);
void updatedco2_PWM_env(bool announce);
void updatedco2_PWM_lfo(bool announce);
void updatedco2_PWM_lfo_source(bool announce);
void updatedco2_PWM_env_source(bool announce);
void updatedco2_PWM_dyn(bool announce);

void updatedco1_level(bool announce);
void updatedco2_level(bool announce);
void updatedco_mix_env_source(bool announce);
void updatedco_mix_dyn(bool announce);
void updatedco2_mod(bool announce);

void updatevcf_cutoff(bool announce);      // 96
void updatevcf_res(bool announce);         // 97
void updatevcf_lfo1(bool announce);        // 98
void updatevcf_lfo2(bool announce);        // 99
void updatevcf_env(bool announce);         // 9A
void updatevcf_kb(bool announce);          // 9B
void updatevcf_dyn(bool announce);         // 9C
void updatevcf_env_source(bool announce);  // 9D
void updatevca_mod(bool announce);         // 9E
void updatevca_env_source(bool announce);  // AE
void updatevca_dyn(bool announce);         // 9F

void updatelfo1_wave(bool announce);
void updatelfo1_delay(bool announce);
void updatelfo1_rate(bool announce);
void updatelfo1_lfo2(bool announce);
void updatelfo1_sync(bool announce);

void updatelfo2_wave(bool announce);
void updatelfo2_rate(bool announce);
void updatelfo2_delay(bool announce);
void updatelfo2_lfo1(bool announce);
void updatelfo2_sync(bool announce);

void updatetime1(bool announce);
void updatelevel1(bool announce);
void updatetime2(bool announce);
void updatelevel2(bool announce);
void updatetime3(bool announce);
void updatelevel3(bool announce);
void updatetime4(bool announce);
void updateenv5stage_mode(bool announce);

void updateenv2_time1(bool announce);
void updateenv2_level1(bool announce);
void updateenv2_time2(bool announce);
void updateenv2_level2(bool announce);
void updateenv2_time3(bool announce);
void updateenv2_level3(bool announce);
void updateenv2_time4(bool announce);
void updateenv2_env5stage_mode(bool announce);

void updateattack(bool announce);
void updatedecay(bool announce);
void updatesustain(bool announce);
void updaterelease(bool announce);
void updateadsr_mode(bool announce);

void updateenv4_attack(bool announce);
void updateenv4_decay(bool announce);
void updateenv4_sustain(bool announce);
void updateenv4_release(bool announce);
void updateenv4_adsr_mode(bool announce);

// ---- Raw storage accessors --------------------------------------------------
static inline int toneRead(int pIndex) {
  return upperSW ? upperData[pIndex] : lowerData[pIndex];
}
static inline void toneWrite(int pIndex, int value) {
  if (upperSW) upperData[pIndex] = value;
  else lowerData[pIndex] = value;
}

// ---- Switch codec: stored = index * step ------------------------------------
static inline int packStep(int index, uint8_t step, uint8_t maxIndex) {
  if (index < 0) index = 0;
  if (index > maxIndex) index = maxIndex;
  return (index * step) & 0x7F;
}
static inline int unpackStep(int stored, uint8_t step, uint8_t maxIndex) {
  int i = (stored & 0x7F) / step;
  if (i > maxIndex) i = maxIndex;
  return i;
}

// ---- Linear-scaled continuous: UI index 0..(N-1) <-> stored 0..127 ---------
// Used by every plain 0..99 continuous tone parameter (N=100).
// Rounded-nearest both directions so min/mid/max UI hit 0/~63/127.
static inline int packScaleN(int idx, int N) {
  if (idx < 0) idx = 0;
  if (idx > N - 1) idx = N - 1;
  return ((idx * 127) + ((N - 1) / 2)) / (N - 1);
}
static inline int unpackScaleN(int stored, int N) {
  int v = stored & 0x7F;
  return ((v * (N - 1)) + 63) / 127;
}

// ---- Centred codec for DCO TUNE / DCO2 FTUN --------------------------------
// The voice board has a "0" deadzone around 0x3F (e.g. 0x36..0x47 all mean
// "no detune" for DCO tune). We snap any stored value inside that band to
// the exact centre UI index so the UI never shows -1 for a patch whose
// stored byte happens to land in the deadzone.
//
// `deadHalf` is the half-width of the deadzone in stored units, measured
// from 0x40. For DCO tune (dead 0x36..0x47) that's 10.
//
// Pack is plain linear — when the user leaves "+0" by one click, the stored
// byte lands outside the deadzone on the correct side.
static inline int unpackScaleCentred(int stored, int N, int deadHalf) {
  int v = stored & 0x7F;
  int centreUi = (N - 1) / 2;
  int centreStored = 0x40;
  if (v >= centreStored - deadHalf && v < centreStored + deadHalf) {
    return centreUi;
  }
  return ((v * (N - 1)) + 63) / 127;
}

// ---- DCO TUNE codec (26 UI positions, measured from real JX) ---------------
// UI 0..12   -> "-12".."-00"    (stored 0x00..0x3F)
// UI 13..25  -> "+00".."+12"    (stored 0x40..0x7F)
// Most bands are 4 stored units wide; ±7, ±12 and the deadzone halves are 8.

struct ToneTuneBand {
  uint8_t ui;
  uint8_t lo;
  uint8_t hi;
};
static const ToneTuneBand toneTuneBands[26] = {
  { 0, 0x00, 0x07 },   //  -12
  { 1, 0x08, 0x0B },   //  -11
  { 2, 0x0C, 0x0F },   //  -10
  { 3, 0x10, 0x13 },   //   -9
  { 4, 0x14, 0x17 },   //   -8
  { 5, 0x18, 0x1F },   //   -7
  { 6, 0x20, 0x23 },   //   -6
  { 7, 0x24, 0x27 },   //   -5
  { 8, 0x28, 0x2B },   //   -4
  { 9, 0x2C, 0x2F },   //   -3
  { 10, 0x30, 0x33 },  //   -2
  { 11, 0x34, 0x37 },  //   -1
  { 12, 0x38, 0x3F },  //  -00
  { 13, 0x40, 0x47 },  //  +00
  { 14, 0x48, 0x4B },  //   +1
  { 15, 0x4C, 0x4F },  //   +2
  { 16, 0x50, 0x53 },  //   +3
  { 17, 0x54, 0x57 },  //   +4
  { 18, 0x58, 0x5B },  //   +5
  { 19, 0x5C, 0x5F },  //   +6
  { 20, 0x60, 0x67 },  //   +7
  { 21, 0x68, 0x6B },  //   +8
  { 22, 0x6C, 0x6F },  //   +9
  { 23, 0x70, 0x73 },  //  +10
  { 24, 0x74, 0x77 },  //  +11
  { 25, 0x78, 0x7F },  //  +12
};

// Representative byte to send per UI position (midpoint of each band).
static const uint8_t toneTuneReps[26] = {
  0x03,
  0x09,
  0x0D,
  0x11,
  0x15,
  0x1B,
  0x21,
  0x25,
  0x29,
  0x2D,
  0x31,
  0x35,
  0x3B,
  0x43,
  0x49,
  0x4D,
  0x51,
  0x55,
  0x59,
  0x5D,
  0x63,
  0x69,
  0x6D,
  0x71,
  0x75,
  0x7B,
};

static inline int packSplitTune(int idx) {
  if (idx < 0) idx = 0;
  if (idx > 25) idx = 25;
  return toneTuneReps[idx];
}

static inline int unpackSplitTune(int stored) {
  int v = stored & 0x7F;
  for (int i = 0; i < 26; i++) {
    if (v >= toneTuneBands[i].lo && v <= toneTuneBands[i].hi) {
      return toneTuneBands[i].ui;
    }
  }
  return 13;  // unreachable (full coverage); fallback to +00
}

// ---- Split codec for DCO2 FTUN (102 positions with -00 / +00) --------------
// UI 0..50   (-50..-00) -> stored 0x00..0x3F  (linear on 64 values)
// UI 51..101 (+00..+50) -> stored 0x40..0x7F  (linear on 64 values)
// The two halves are independent so -00 and +00 are distinct UI positions
// sharing no stored bytes.
static inline int packSplitFine(int idx) {
  if (idx < 0) idx = 0;
  if (idx > 101) idx = 101;
  if (idx <= 50) {
    // Negative half: idx 0 -> 0x00, idx 50 -> 0x3F
    return (idx * 0x3F + 25) / 50;
  } else {
    // Positive half: idx 51 -> 0x40, idx 101 -> 0x7F
    return 0x40 + ((idx - 51) * 0x3F + 25) / 50;
  }
}
static inline int unpackSplitFine(int stored) {
  int v = stored & 0x7F;
  if (v < 0x40) {
    // Negative half: 0x00 -> 0, 0x3F -> 50
    return (v * 50 + 0x1F) / 0x3F;
  } else {
    // Positive half: 0x40 -> 51, 0x7F -> 101
    return 51 + ((v - 0x40) * 50 + 0x1F) / 0x3F;
  }
}

// Shared 0..127 label table
static char tone128Labels[128][4];
static const char *tone128Ptrs[129];

static void buildTone128Labels() {
  for (int i = 0; i < 128; i++) {
    snprintf(tone128Labels[i], 4, "%d", i);
    tone128Ptrs[i] = tone128Labels[i];
  }
  tone128Ptrs[128] = "\0";
}

// ---- "0..99" display for 0..127-stored continuous params (100 UI positions) ----
static char tone100Labels[100][4];
static const char *tone100Ptrs[101];

static void buildTone100Labels() {
  for (int i = 0; i < 100; i++) {
    snprintf(tone100Labels[i], 4, "%d", i);
    tone100Ptrs[i] = tone100Labels[i];
  }
  tone100Ptrs[100] = "\0";
}

// ---- "-12..+12" display for DCO coarse tune (25 UI positions) ----
static char toneTuneLabels[26][5];
static const char *toneTunePtrs[27];

static void buildToneTuneLabels() {
  for (int i = 0; i < 12; i++) {
    // negative side: -12 at i=0, -01 at i=11
    snprintf(toneTuneLabels[i], 5, "-%02d", 12 - i);
    toneTunePtrs[i] = toneTuneLabels[i];
  }
  // Both centre positions display as "00" — the codec still keeps them
  // as distinct UI positions (stored 0x3F vs 0x40), but the label is unsigned.
  snprintf(toneTuneLabels[12], 5, " 00");
  snprintf(toneTuneLabels[13], 5, " 00");
  toneTunePtrs[12] = toneTuneLabels[12];
  toneTunePtrs[13] = toneTuneLabels[13];
  for (int i = 14; i < 26; i++) {
    // positive side: +01 at i=14, +12 at i=25
    snprintf(toneTuneLabels[i], 5, "+%02d", i - 13);
    toneTunePtrs[i] = toneTuneLabels[i];
  }
  toneTunePtrs[26] = "\0";
}

// ---- "-50..-00, +00..+50" display for DCO2 fine tune (102 UI positions) ----
// UI 0..50   -> "-50".."-00"  (stored 0x00..0x3F)
// UI 51..101 -> "+00".."+50"  (stored 0x40..0x7F)
static char toneFineLabels[102][5];
static const char *toneFinePtrs[103];

static void buildToneFineLabels() {
  for (int i = 0; i < 51; i++) {
    // negative side: -50 at i=0, -00 at i=50
    snprintf(toneFineLabels[i], 5, "-%02d", 50 - i);
    toneFinePtrs[i] = toneFineLabels[i];
  }
  for (int i = 51; i < 102; i++) {
    // positive side: +00 at i=51, +50 at i=101
    snprintf(toneFineLabels[i], 5, "+%02d", i - 51);
    toneFinePtrs[i] = toneFineLabels[i];
  }
  toneFinePtrs[102] = "\0";
}

// ─────────────────────────────────────────────
// Label tables for categorical params
// ─────────────────────────────────────────────
static const char *toneDcoRangeValues[] = { " 16'", " 8'", " 4'", " 2'", "\0" };   // 4 opts, step 0x20
static const char *toneDcoWaveValues[] = { "NOIS", "SQR", "PULS", "SAWT", "\0" };  // 4 opts, step 0x20
static const char *toneDcoLFOValues[] = { "1NEG", "1POS", "2NEG", "2POS", "\0" };  // 4 opts, step 0x20
static const char *toneDynValues[] = { "OFF", "DYN1", "DYN2", "DYN3", "\0" };      // 4 opts, step 0x20
static const char *toneEnvValues[] = { "ENV1-", "ENV1+", "ENV2-", "ENV2+",
                                       "ENV3-", "ENV3+", "ENV4-", "ENV4+", "\0" };     // 8 opts, step 0x10
static const char *toneVcaEnvValues[] = { "ENV1", "ENV2", "ENV3", "ENV4", "\0" };      // 4 opts, step 0x20
static const char *toneLFOValues[] = { "RAND", "SQR", "SAW-", "SAW+", "SINE", "\0" };  // 5 opts, step 0x10
static const char *toneSyncValues[] = { "OFF", "ON", "KEY", "\0" };                    // 3 opts, step 0x20
static const char *toneEnvKeyValues[] = { "OFF", "KEY 1", "KEY 2", "KEY 3",
                                          "LOOP0", "LOOP1", "LOOP2", "LOOP3", "\0" };  // 8 opts, step 0x10
static const char *toneDcoXMODValues[] = { "OFF", "SYNC1", "SYNC2", "SYNC3", "\0" };   // 4 opts, step 0x20
static const char *VCFHPFValues[] = { "HPF0", "HPF1", "HPF2", "HPF3", "\0" };          // 4 opts, step 0x20
static const char *ChorusValues[] = { "OFF", " 1", " 2", "\0" };                       // 3 opts, step 0x20

// ============================================================================
// Handlers
//
// Each handler pair has a "b3=$XX" comment showing the voice-board SYX
// parameter address taken from the V4 ROM tone-edit table ($CB40..$CDEA).
// When you replace the "TODO: send" lines, use this b3 with your existing
// voice-board send function.
//
// b3 = ? means the parameter isn't in the V4 ROM tone-edit menu; check the
// Vecoven bit-layout doc for the address if/when you need to send it.
// ============================================================================

// ───── DCO1 ─────────────────────────────────────────────── b3 from ROM ─────
void toneDco1Range(int index, const char *value) {  // b3=$42
  toneWrite(P_dco1_range, index);
  updatedco1_range(0);
}
int idxToneDco1Range() {
  return toneRead(P_dco1_range);
}

void toneDco1Wave(int index, const char *value) {  // b3=$41
  toneWrite(P_dco1_wave, index);
  updatedco1_wave(0);
}
int idxToneDco1Wave() {
  return toneRead(P_dco1_wave);
}

void toneDco1Tune(int index, const char *value) {  // b3=$0B
  toneWrite(P_dco1_tune, packSplitTune(index));
  updatedco1_tune(0);
}
int idxToneDco1Tune() {
  return unpackSplitTune(toneRead(P_dco1_tune));
}

void toneDco1LFO(int index, const char *value) {  // b3=$0C
  toneWrite(P_dco1_pitch_lfo, packScaleN(index, 100));
  updatedco1_pitch_lfo(0);
}
int idxToneDco1LFO() {
  return unpackScaleN(toneRead(P_dco1_pitch_lfo), 100);
}

void toneDco1LFOSrc(int index, const char *value) {  // b3=$48
  toneWrite(P_dco1_pitch_lfo_source, packStep(index, 0x20, 3));
  updatedco1_pitch_lfo_source(0);
}
int idxToneDco1LFOSrc() {
  return unpackStep(toneRead(P_dco1_pitch_lfo_source), 0x20, 3);
}

void toneDco1Env(int index, const char *value) {  // b3=$0D
  toneWrite(P_dco1_pitch_env, packScaleN(index, 100));
  updatedco1_pitch_env(0);
}
int idxToneDco1Env() {
  return unpackScaleN(toneRead(P_dco1_pitch_env), 100);
}

void toneDco1Dyn(int index, const char *value) {  // b3=$45
  toneWrite(P_dco1_pitch_dyn, packStep(index, 0x20, 3));
  updatedco1_pitch_dyn(0);
}
int idxToneDco1Dyn() {
  return unpackStep(toneRead(P_dco1_pitch_dyn), 0x20, 3);
}

void toneDco1EnvSrc(int index, const char *value) {  // b3=$53
  toneWrite(P_dco1_pitch_env_source, packStep(index, 0x10, 7));
  updatedco1_pitch_env_source(0);
}
int idxToneDco1EnvSrc() {
  return unpackStep(toneRead(P_dco1_pitch_env_source), 0x10, 7);
}

// ───── DCO2 ─────────────────────────────────────────────────────────────────
void toneDco2Range(int index, const char *value) {  // b3=?  (not in V4 tone-edit menu)
  toneWrite(P_dco2_range, index);
  updatedco2_range(0);
}
int idxToneDco2Range() {
  return toneRead(0);
}

void toneDco2Wave(int index, const char *value) {  // b3=$43
  toneWrite(P_dco2_wave, index);
  updatedco2_wave(0);
}
int idxToneDco2Wave() {
  return toneRead(0);
}

void toneDco2Tune(int index, const char *value) {  // b3=$0E
  toneWrite(P_dco2_tune, packSplitTune(index));
  updatedco2_tune(0);
}
int idxToneDco2Tune() {
  return unpackSplitTune(toneRead(P_dco2_tune));
}

void toneDco2LFO(int index, const char *value) {  // b3=$10
  toneWrite(P_dco2_pitch_lfo, packScaleN(index, 100));
  updatedco2_pitch_lfo(0);
}
int idxToneDco2LFO() {
  return unpackScaleN(toneRead(P_dco2_pitch_lfo), 100);
}

void toneDco2LFOSrc(int index, const char *value) {  // b3=$49
  toneWrite(P_dco2_pitch_lfo_source, packStep(index, 0x20, 3));
  updatedco2_pitch_lfo_source(0);
}
int idxToneDco2LFOSrc() {
  return unpackStep(toneRead(P_dco2_pitch_lfo_source), 0x20, 3);
}

void toneDco2Env(int index, const char *value) {  // b3=$11
  toneWrite(P_dco2_pitch_env, packScaleN(index, 100));
  updatedco2_pitch_env(0);
}
int idxToneDco2Env() {
  return unpackScaleN(toneRead(P_dco2_pitch_env), 100);
}

void toneDco2Dyn(int index, const char *value) {  // b3=$46
  toneWrite(P_dco2_pitch_dyn, packStep(index, 0x20, 3));
  updatedco2_pitch_dyn(0);
}
int idxToneDco2Dyn() {
  return unpackStep(toneRead(P_dco2_pitch_dyn), 0x20, 3);
}

void toneDco2EnvSrc(int index, const char *value) {  // b3=$54
  toneWrite(P_dco2_pitch_env_source, packStep(index, 0x10, 7));
  updatedco2_pitch_env_source(0);
}
int idxToneDco2EnvSrc() {
  return unpackStep(toneRead(P_dco2_pitch_env_source), 0x10, 7);
}

void toneDcoXMOD(int index, const char *value) {  // b3=$44
  toneWrite(P_dco1_mode, packStep(index, 0x20, 3));
  updatedco1_xmod(0);
}
int idxToneDcoXMOD() {
  return unpackStep(toneRead(P_dco1_mode), 0x20, 3);
}

void toneDco2FTun(int index, const char *value) {  // b3=$0F
  toneWrite(P_dco2_fine, packSplitFine(index));
  updatedco2_fine(0);
}
int idxToneDco2FTun() {
  return unpackSplitFine(toneRead(P_dco2_fine));
}

// ───── VCF HPF / CHORUS ─────────────────────────────────────────────────────
void toneVcfHPF(int index, const char *value) {  // b3=$52
  toneWrite(P_vcf_hpf, packStep(index, 0x20, 3));
  updatevcf_hpf(0);
}
int idxToneVcfHPF() {
  return unpackStep(toneRead(P_vcf_hpf), 0x20, 3);
}

void toneChorus(int index, const char *value) {  // b3=$5B
  toneWrite(P_chorus, packStep(index, 0x20, 2));
  updatechorus(0);
}
int idxToneChorus() {
  return unpackStep(toneRead(P_chorus), 0x20, 2);
}

// ───── PWM1 ─────────────────────────────────────────────────────────────────
void tonePWM1Width(int index, const char *value) {  // b3=$15
  toneWrite(P_dco1_PW, packScaleN(index, 100));
  updatedco1_PW(0);
}
int idxTonePWM1Width() {
  return unpackScaleN(toneRead(P_dco1_PW), 100);
}

void tonePWM1Env(int index, const char *value) {  // b3=$13
  toneWrite(P_dco1_PWM_env, packScaleN(index, 100));
  updatedco1_PWM_env(0);
}
int idxTonePWM1Env() {
  return unpackScaleN(toneRead(P_dco1_PWM_env), 100);
}

void tonePWM1LFO(int index, const char *value) {  // b3=$14
  toneWrite(P_dco1_PWM_lfo, packScaleN(index, 100));
  updatedco1_PWM_lfo(0);
}
int idxTonePWM1LFO() {
  return unpackScaleN(toneRead(P_dco1_PWM_lfo), 100);
}

void tonePWM1LFOSrc(int index, const char *value) {  // b3=$4A
  toneWrite(P_dco1_PWM_lfo_source, packStep(index, 0x20, 3));
  updatedco1_PWM_lfo_source(0);
}
int idxTonePWM1LFOSrc() {
  return unpackStep(toneRead(P_dco1_PWM_lfo_source), 0x20, 3);
}

void tonePWM1Dyn(int index, const char *value) {  // b3=$4C
  toneWrite(P_dco1_PWM_dyn, packStep(index, 0x20, 3));
  updatedco1_PWM_dyn(0);
}
int idxTonePWM1Dyn() {
  return unpackStep(toneRead(P_dco1_PWM_dyn), 0x20, 3);
}

void tonePWM1EnvSrc(int index, const char *value) {  // b3=$5F
  toneWrite(P_dco1_PWM_env_source, packStep(index, 0x10, 7));
  updatedco1_PWM_env_source(0);
}
int idxTonePWM1EnvSrc() {
  return unpackStep(toneRead(P_dco1_PWM_env_source), 0x10, 7);
}

// ───── PWM2 ─────────────────────────────────────────────────────────────────
void tonePWM2Width(int index, const char *value) {  // b3=$16
  toneWrite(P_dco2_PW, packScaleN(index, 100));
  updatedco2_PW(0);
}
int idxTonePWM2Width() {
  return unpackScaleN(toneRead(P_dco2_PW), 100);
}

void tonePWM2Env(int index, const char *value) {  // b3=$17
  toneWrite(P_dco2_PWM_env, packScaleN(index, 100));
  updatedco2_PWM_env(0);
}
int idxTonePWM2Env() {
  return unpackScaleN(toneRead(P_dco2_PWM_env), 100);
}

void tonePWM2LFO(int index, const char *value) {  // b3=$4B
  toneWrite(P_dco2_PWM_lfo, packScaleN(index, 100));
  updatedco2_PWM_lfo(0);
}
int idxTonePWM2LFO() {
  return unpackScaleN(toneRead(P_dco2_PWM_lfo), 100);
}

void tonePWM2LFOSrc(int index, const char *value) {  // b3=$4D
  toneWrite(P_dco2_PWM_lfo_source, packStep(index, 0x20, 3));
  updatedco2_PWM_lfo_source(0);
}
int idxTonePWM2LFOSrc() {
  return unpackStep(toneRead(P_dco2_PWM_lfo_source), 0x20, 3);
}

void tonePWM2Dyn(int index, const char *value) {  // b3=$4D (*)
  toneWrite(P_dco2_PWM_dyn, packStep(index, 0x20, 3));
  updatedco2_PWM_dyn(0);
}
int idxTonePWM2Dyn() {
  return unpackStep(toneRead(P_dco2_PWM_dyn), 0x20, 3);
}

void tonePWM2EnvSrc(int index, const char *value) {  // b3=$60
  toneWrite(P_dco2_PWM_env_source, packStep(index, 0x10, 7));
  updatedco2_PWM_env_source(0);
}
int idxTonePWM2EnvSrc() {
  return unpackStep(toneRead(P_dco2_PWM_env_source), 0x10, 7);
}

// ───── MIX ──────────────────────────────────────────────────────────────────
void toneMIXDco1(int index, const char *value) {  // b3=$18
  toneWrite(P_dco1_level, packScaleN(index, 100));
  updatedco1_level(0);
}
int idxToneMIXDco1() {
  return unpackScaleN(toneRead(P_dco1_level), 100);
}

void toneMIXDco2(int index, const char *value) {  // b3=$19
  toneWrite(P_dco2_level, packScaleN(index, 100));
  updatedco2_level(0);
}
int idxToneMIXDco2() {
  return unpackScaleN(toneRead(P_dco2_level), 100);
}

void toneMIXEnv(int index, const char *value) {  // b3=$1A
  toneWrite(P_dco2_mod, packScaleN(index, 100));
  updatedco2_mod(0);
}
int idxToneMIXEnv() {
  return unpackScaleN(toneRead(P_dco2_mod), 100);
}

void toneMIXDyn(int index, const char *value) {  // b3=$47
  toneWrite(P_dco_mix_dyn, packStep(index, 0x20, 3));
  updatedco_mix_dyn(0);
}
int idxToneMixDyn() {
  return unpackStep(toneRead(P_dco_mix_dyn), 0x20, 3);
}

void toneMIXEnvSrc(int index, const char *value) {  // b3=$56
  toneWrite(P_dco_mix_env_source, packStep(index, 0x10, 7));
  updatedco_mix_env_source(0);
}
int idxToneMIXEnvSrc() {
  return unpackStep(toneRead(P_dco_mix_env_source), 0x10, 7);
}

// ───── VCF ──────────────────────────────────────────────────────────────────
void toneVcfCutoff(int index, const char *value) {  // b3=$1B
  toneWrite(P_vcf_cutoff, packScaleN(index, 100));
  updatevcf_cutoff(0);
}
int idxToneVcfCutoff() {
  return unpackScaleN(toneRead(P_vcf_cutoff), 100);
}

void toneVcfRes(int index, const char *value) {  // b3=$1C
  toneWrite(P_vcf_res, packScaleN(index, 100));
  updatevcf_res(0);
}
int idxToneVcfRes() {
  return unpackScaleN(toneRead(P_vcf_res), 100);
}

void toneVcfLFO1(int index, const char *value) {  // b3=$1D
  toneWrite(P_vcf_lfo1, packScaleN(index, 100));
  updatevcf_lfo1(0);
}
int idxToneVcfLFO1() {
  return unpackScaleN(toneRead(P_vcf_lfo1), 100);
}

void toneVcfLFO2(int index, const char *value) {  // b3=$1E
  toneWrite(P_vcf_lfo2, packScaleN(index, 100));
  updatevcf_lfo2(0);
}
int idxToneVcfLFO2() {
  return unpackScaleN(toneRead(P_vcf_lfo2), 100);
}

void toneVcfEnv(int index, const char *value) {  // b3=$1F
  toneWrite(P_vcf_env, packScaleN(index, 100));
  updatevcf_env(0);
}
int idxToneVcfEnv() {
  return unpackScaleN(toneRead(P_vcf_env), 100);
}

void toneVcfKey(int index, const char *value) {  // b3=$20
  toneWrite(P_vcf_kb, packScaleN(index, 100));
  updatevcf_kb(0);
}
int idxToneVcfKey() {
  return unpackScaleN(toneRead(P_vcf_kb), 100);
}

void toneVcfDyn(int index, const char *value) {  // b3=$58
  toneWrite(P_vcf_dyn, packStep(index, 0x20, 3));
  updatevcf_dyn(0);
}
int idxToneVcfDyn() {
  return unpackStep(toneRead(P_vcf_dyn), 0x20, 3);
}

void toneVcfEnvSrc(int index, const char *value) {  // b3=$57
  toneWrite(P_vcf_env_source, packStep(index, 0x10, 7));
  updatevcf_env_source(0);
}
int idxToneVcfEnvSrc() {
  return unpackStep(toneRead(P_vcf_env_source), 0x10, 7);
}

// ───── VCA ──────────────────────────────────────────────────────────────────
void toneVcaLevel(int index, const char *value) {  // b3=$21
  toneWrite(P_vca_mod, packScaleN(index, 100));
  updatevca_mod(0);
}
int idxToneVcaLevel() {
  return unpackScaleN(toneRead(P_vca_mod), 100);
}

void toneVcaEnvSrc(int index, const char *value) {  // b3=$59
  toneWrite(P_vca_env_source, packStep(index, 0x20, 3));
  updatevca_env_source(0);
}
int idxToneVcaEnvSrc() {
  return unpackStep(toneRead(P_vca_env_source), 0x20, 3);
}

void toneVcaDyn(int index, const char *value) {  // b3=$55
  toneWrite(P_vca_dyn, packStep(index, 0x20, 3));
  updatevca_dyn(0);
}
int idxToneVcaDyn() {
  return unpackStep(toneRead(P_vca_dyn), 0x20, 3);
}

// ───── LFO1 ─────────────────────────────────────────────────────────────────
void toneLFO1WF(int index, const char *value) {  // b3=$5C
  toneWrite(P_lfo1_wave, index);
  updatelfo1_wave(0);
}
int idxToneLFO1WF() {
  return toneRead(P_lfo1_wave);
}

void toneLFO1Delay(int index, const char *value) {  // b3=$22
  toneWrite(P_lfo1_delay, packScaleN(index, 100));
  updatelfo1_delay(0);
}
int idxToneLFO1Delay() {
  return unpackScaleN(toneRead(P_lfo1_delay), 100);
}

void toneLFO1Rate(int index, const char *value) {  // b3=$23
  toneWrite(P_lfo1_rate, packScaleN(index, 100));
  updatelfo1_rate(0);
}
int idxToneLFO1Rate() {
  return unpackScaleN(toneRead(P_lfo1_rate), 100);
}

void toneLFO1LFO(int index, const char *value) {  // b3=$24
  toneWrite(P_lfo1_lfo2, packScaleN(index, 100));
  updatelfo1_lfo2(0);
}
int idxToneLFO1LFO() {
  return unpackScaleN(toneRead(P_lfo1_lfo2), 100);
}

void toneLFO1Sync(int index, const char *value) {  // b3=$4E
  toneWrite(P_lfo1_sync, packStep(index, 0x20, 2));
  updatelfo1_sync(0);
}
int idxToneLFO1Sync() {
  return unpackStep(toneRead(P_lfo1_sync), 0x20, 2);
}

// ───── LFO2 ─────────────────────────────────────────────────────────────────
void toneLFO2WF(int index, const char *value) {  // b3=$5A
  toneWrite(P_lfo2_wave, index);
  updatelfo2_wave(0);
}
int idxToneLFO2WF() {
  return toneRead(P_lfo2_wave);
}

void toneLFO2Delay(int index, const char *value) {  // b3=$25
  toneWrite(P_lfo2_delay, packScaleN(index, 100));
  updatelfo2_delay(0);
}
int idxToneLFO2Delay() {
  return unpackScaleN(toneRead(P_lfo2_delay), 100);
}

void toneLFO2Rate(int index, const char *value) {  // b3=$26
  toneWrite(P_lfo2_rate, packScaleN(index, 100));
  updatelfo2_rate(0);
}
int idxToneLFO2Rate() {
  return unpackScaleN(toneRead(P_lfo2_rate), 100);
}

void toneLFO2LFO(int index, const char *value) {  // b3=$27
  toneWrite(P_lfo2_lfo1, packScaleN(index, 100));
  updatelfo2_lfo1(0);
}
int idxToneLFO2LFO() {
  return unpackScaleN(toneRead(P_lfo2_lfo1), 100);
}

void toneLFO2Sync(int index, const char *value) {  // b3=$4F
  toneWrite(P_lfo2_sync, packStep(index, 0x20, 2));
  updatelfo2_sync(0);
}
int idxToneLFO2Sync() {
  return unpackStep(toneRead(P_lfo2_sync), 0x20, 2);
}

// ───── ENV1 (5-stage envelope for tone 1) ───────────────────────────────────
void toneEnv1T1(int index, const char *value) {
  toneWrite(P_time1, packScaleN(index, 100));
  updatetime1(0);
}  // b3=$29
int idxToneEnv1T1() {
  return unpackScaleN(toneRead(P_time1), 100);
}
void toneEnv1L1(int index, const char *value) {
  toneWrite(P_level1, packScaleN(index, 100));
  updatelevel1(0);
}  // b3=$2A
int idxToneEnv1L1() {
  return unpackScaleN(toneRead(P_level1), 100);
}
void toneEnv1T2(int index, const char *value) {
  toneWrite(P_time2, packScaleN(index, 100));
  updatetime2(0);
}  // b3=$2B
int idxToneEnv1T2() {
  return unpackScaleN(toneRead(P_time2), 100);
}
void toneEnv1L2(int index, const char *value) {
  toneWrite(P_level2, packScaleN(index, 100));
  updatelevel2(0);
}  // b3=$2C
int idxToneEnv1L2() {
  return unpackScaleN(toneRead(P_level2), 100);
}
void toneEnv1T3(int index, const char *value) {
  toneWrite(P_time3, packScaleN(index, 100));
  updatetime3(0);
}  // b3=$2D
int idxToneEnv1T3() {
  return unpackScaleN(toneRead(P_time3), 100);
}
void toneEnv1L3(int index, const char *value) {
  toneWrite(P_level3, packScaleN(index, 100));
  updatelevel3(0);
}  // b3=$2E
int idxToneEnv1L3() {
  return unpackScaleN(toneRead(P_level3), 100);
}
void toneEnv1T4(int index, const char *value) {
  toneWrite(P_time4, packScaleN(index, 100));
  updatetime4(0);
}  // b3=$2F
int idxToneEnv1T4() {
  return unpackScaleN(toneRead(P_time4), 100);
}
void toneEnv1Key(int index, const char *value) {
  toneWrite(P_env5stage_mode, packStep(index, 0x10, 7));
  updateenv5stage_mode(0);
}  // b3=$50
int idxToneEnv1Key() {
  return unpackStep(toneRead(P_env5stage_mode), 0x10, 7);
}

// ───── ENV2 (5-stage envelope for tone 2) ───────────────────────────────────
void toneEnv2T1(int index, const char *value) {
  toneWrite(P_env2_time1, packScaleN(index, 100));
  updateenv2_time1(0);
}  // b3=$30
int idxToneEnv2T1() {
  return unpackScaleN(toneRead(P_env2_time1), 100);
}
void toneEnv2L1(int index, const char *value) {
  toneWrite(P_env2_level1, packScaleN(index, 100));
  updateenv2_level1(0);
}  // b3=$31
int idxToneEnv2L1() {
  return unpackScaleN(toneRead(P_env2_level1), 100);
}
void toneEnv2T2(int index, const char *value) {
  toneWrite(P_env2_time2, packScaleN(index, 100));
  updateenv2_time2(0);
}  // b3=$32
int idxToneEnv2T2() {
  return unpackScaleN(toneRead(P_env2_time2), 100);
}
void toneEnv2L2(int index, const char *value) {
  toneWrite(P_env2_level2, packScaleN(index, 100));
  updateenv2_level2(0);
}  // b3=$33
int idxToneEnv2L2() {
  return unpackScaleN(toneRead(P_env2_level2), 100);
}
void toneEnv2T3(int index, const char *value) {
  toneWrite(P_env2_time3, packScaleN(index, 100));
  updateenv2_time3(0);
}  // b3=$34
int idxToneEnv2T3() {
  return unpackScaleN(toneRead(P_env2_time3), 100);
}
void toneEnv2L3(int index, const char *value) {
  toneWrite(P_env2_level3, packScaleN(index, 100));
  updateenv2_level3(0);
}  // b3=$35
int idxToneEnv2L3() {
  return unpackScaleN(toneRead(P_env2_level3), 100);
}
void toneEnv2T4(int index, const char *value) {
  toneWrite(P_env2_time4, packScaleN(index, 100));
  updateenv2_time4(0);
}  // b3=$36
int idxToneEnv2T4() {
  return unpackScaleN(toneRead(P_env2_time4), 100);
}
void toneEnv2Key(int index, const char *value) {
  toneWrite(P_env2_5stage_mode, packStep(index, 0x10, 7));
  updateenv2_env5stage_mode(0);
}  // b3=$51
int idxToneEnv2Key() {
  return unpackStep(toneRead(P_env2_5stage_mode), 0x10, 7);
}

// ───── ENV3 (ADSR for tone 1) ───────────────────────────────────────────────
void toneEnv3Att(int index, const char *value) {
  toneWrite(P_attack, packScaleN(index, 100));
  updateattack(0);
}  // b3=$37
int idxToneEnv3Att() {
  return unpackScaleN(toneRead(P_attack), 100);
}
void toneEnv3Dec(int index, const char *value) {
  toneWrite(P_decay, packScaleN(index, 100));
  updatedecay(0);
}  // b3=$38
int idxToneEnv3Dec() {
  return unpackScaleN(toneRead(P_decay), 100);
}
void toneEnv3Sust(int index, const char *value) {
  toneWrite(P_sustain, packScaleN(index, 100));
  updatesustain(0);
}  // b3=$39
int idxToneEnv3Sust() {
  return unpackScaleN(toneRead(P_sustain), 100);
}
void toneEnv3Rel(int index, const char *value) {
  toneWrite(P_release, packScaleN(index, 100));
  updaterelease(0);
}  // b3=$17 (check)
int idxToneEnv3Rel() {
  return unpackScaleN(toneRead(P_release), 100);
}
void toneEnv3Key(int index, const char *value) {
  toneWrite(P_adsr_mode, packStep(index, 0x10, 7));
  updateadsr_mode(0);
}  // b3=$5D
int idxToneEnv3Key() {
  return unpackStep(toneRead(P_adsr_mode), 0x10, 7);
}

// ───── ENV4 (ADSR for tone 2) ───────────────────────────────────────────────
void toneEnv4Att(int index, const char *value) {
  toneWrite(P_env4_attack, packScaleN(index, 100));
  updateenv4_attack(0);
}  // b3=$3A
int idxToneEnv4Att() {
  return unpackScaleN(toneRead(P_env4_attack), 100);
}
void toneEnv4Dec(int index, const char *value) {
  toneWrite(P_env4_decay, packScaleN(index, 100));
  updateenv4_decay(0);
}  // b3=$3B
int idxToneEnv4Dec() {
  return unpackScaleN(toneRead(P_env4_decay), 100);
}
void toneEnv4Sust(int index, const char *value) {
  toneWrite(P_env4_sustain, packScaleN(index, 100));
  updateenv4_sustain(0);
}  // b3=$3C
int idxToneEnv4Sust() {
  return unpackScaleN(toneRead(P_env4_sustain), 100);
}
void toneEnv4Rel(int index, const char *value) {
  toneWrite(P_env4_release, packScaleN(index, 100));
  updateenv4_release(0);
}  // b3=$3D
int idxToneEnv4Rel() {
  return unpackScaleN(toneRead(P_env4_release), 100);
}
void toneEnv4Key(int index, const char *value) {
  toneWrite(P_env4_adsr_mode, packStep(index, 0x10, 7));
  updateenv4_adsr_mode(0);
}  // b3=$62
int idxToneEnv4Key() {
  return unpackStep(toneRead(P_env4_adsr_mode), 0x10, 7);
}

// ============================================================================
// setUpTone()
// Call once at boot. Order of appends = order of scrolling.
// ============================================================================

void setUpTone() {
  buildTone128Labels();
  buildTone100Labels();
  buildToneTuneLabels();
  buildToneFineLabels();

  tonemenu::append({ "11 DCO1 RANG", toneDcoRangeValues, 4, toneDco1Range, idxToneDco1Range });
  tonemenu::append({ "12 DCO1 WF", toneDcoWaveValues, 4, toneDco1Wave, idxToneDco1Wave });
  tonemenu::append({ "13 DCO1 TUNE", toneTunePtrs, 25, toneDco1Tune, idxToneDco1Tune });
  tonemenu::append({ "13 DCO1 TUNE", toneTunePtrs, 26, toneDco1Tune, idxToneDco1Tune });
  tonemenu::append({ "15 DCO1 LFO", toneDcoLFOValues, 4, toneDco1LFOSrc, idxToneDco1LFOSrc });
  tonemenu::append({ "16 DCO1 ENV", tone100Ptrs, 100, toneDco1Env, idxToneDco1Env });
  tonemenu::append({ "17 DCO1 DYNA", toneDynValues, 4, toneDco1Dyn, idxToneDco1Dyn });
  tonemenu::append({ "18 DCO1 MODE", toneEnvValues, 8, toneDco1EnvSrc, idxToneDco1EnvSrc });

  tonemenu::append({ "21 DCO2 RANG", toneDcoRangeValues, 4, toneDco2Range, idxToneDco2Range });
  tonemenu::append({ "22 DCO2 WF", toneDcoWaveValues, 4, toneDco2Wave, idxToneDco2Wave });
  tonemenu::append({ "23 DCO2 TUNE", toneTunePtrs, 26, toneDco2Tune, idxToneDco2Tune });
  tonemenu::append({ "24 DCO2 LFO", tone100Ptrs, 100, toneDco2LFO, idxToneDco2LFO });
  tonemenu::append({ "25 DCO2 LFO", toneDcoLFOValues, 4, toneDco2LFOSrc, idxToneDco2LFOSrc });
  tonemenu::append({ "26 DCO2 ENV", tone100Ptrs, 100, toneDco2Env, idxToneDco2Env });
  tonemenu::append({ "27 DCO2 DYNA", toneDynValues, 4, toneDco2Dyn, idxToneDco2Dyn });
  tonemenu::append({ "28 DCO2 MODE", toneEnvValues, 8, toneDco2EnvSrc, idxToneDco2EnvSrc });

  tonemenu::append({ "31 DCO X-MOD", toneDcoXMODValues, 4, toneDcoXMOD, idxToneDcoXMOD });
  tonemenu::append({ "32 DCO2 FTUN", toneFinePtrs, 102, toneDco2FTun, idxToneDco2FTun });
  tonemenu::append({ "33 VCF HPF", VCFHPFValues, 4, toneVcfHPF, idxToneVcfHPF });
  tonemenu::append({ "34 CHORUS", ChorusValues, 3, toneChorus, idxToneChorus });

  tonemenu::append({ "41 PWM1 WIDTH", tone100Ptrs, 100, tonePWM1Width, idxTonePWM1Width });
  tonemenu::append({ "42 PWM1 ENV", tone100Ptrs, 100, tonePWM1Env, idxTonePWM1Env });
  tonemenu::append({ "43 PWM1 LFO", tone100Ptrs, 100, tonePWM1LFO, idxTonePWM1LFO });
  tonemenu::append({ "44 PWM1 LFO", toneDcoLFOValues, 4, tonePWM1LFOSrc, idxTonePWM1LFOSrc });
  tonemenu::append({ "45 PWM1 DYNA", toneDynValues, 4, tonePWM1Dyn, idxTonePWM1Dyn });
  tonemenu::append({ "46 PWM1 MODE", toneEnvValues, 8, tonePWM1EnvSrc, idxTonePWM1EnvSrc });

  tonemenu::append({ "51 PWM2 WIDTH", tone100Ptrs, 100, tonePWM2Width, idxTonePWM2Width });
  tonemenu::append({ "52 PWM2 ENV", tone100Ptrs, 100, tonePWM2Env, idxTonePWM2Env });
  tonemenu::append({ "53 PWM2 LFO", tone100Ptrs, 100, tonePWM2LFO, idxTonePWM2LFO });
  tonemenu::append({ "54 PWM2 LFO", toneDcoLFOValues, 4, tonePWM2LFOSrc, idxTonePWM2LFOSrc });
  tonemenu::append({ "55 PWM2 DYNA", toneDynValues, 4, tonePWM2Dyn, idxTonePWM2Dyn });
  tonemenu::append({ "56 PWM2 MODE", toneEnvValues, 8, tonePWM2EnvSrc, idxTonePWM2EnvSrc });

  tonemenu::append({ "61 MIX DCO1", tone100Ptrs, 100, toneMIXDco1, idxToneMIXDco1 });
  tonemenu::append({ "62 MIX DCO2", tone100Ptrs, 100, toneMIXDco2, idxToneMIXDco2 });
  tonemenu::append({ "63 MIX ENV", tone100Ptrs, 100, toneMIXEnv, idxToneMIXEnv });
  tonemenu::append({ "64 MIX DYNA", toneDynValues, 4, toneMIXDyn, idxToneMixDyn });
  tonemenu::append({ "65 MIX MODE", toneEnvValues, 8, toneMIXEnvSrc, idxToneMIXEnvSrc });

  tonemenu::append({ "71 VCF FREQ", tone100Ptrs, 100, toneVcfCutoff, idxToneVcfCutoff });
  tonemenu::append({ "72 VCF RES", tone100Ptrs, 100, toneVcfRes, idxToneVcfRes });
  tonemenu::append({ "73 VCF LFO1", tone100Ptrs, 100, toneVcfLFO1, idxToneVcfLFO1 });
  tonemenu::append({ "74 VCF LFO2", tone100Ptrs, 100, toneVcfLFO2, idxToneVcfLFO2 });
  tonemenu::append({ "75 VCF ENV", tone100Ptrs, 100, toneVcfEnv, idxToneVcfEnv });
  tonemenu::append({ "76 VCF KEY", tone100Ptrs, 100, toneVcfKey, idxToneVcfKey });
  tonemenu::append({ "77 VCF DYNA", toneDynValues, 4, toneVcfDyn, idxToneVcfDyn });
  tonemenu::append({ "78 VCF MODE", toneEnvValues, 8, toneVcfEnvSrc, idxToneVcfEnvSrc });

  tonemenu::append({ "81 VCA LEVEL", tone100Ptrs, 100, toneVcaLevel, idxToneVcaLevel });
  tonemenu::append({ "82 VCA MODE", toneVcaEnvValues, 4, toneVcaEnvSrc, idxToneVcaEnvSrc });
  tonemenu::append({ "83 VCA DYNA", toneDynValues, 4, toneVcaDyn, idxToneVcaDyn });

  tonemenu::append({ "91 LFO1 WF", toneLFOValues, 5, toneLFO1WF, idxToneLFO1WF });
  tonemenu::append({ "92 LFO1 DELAY", tone100Ptrs, 100, toneLFO1Delay, idxToneLFO1Delay });
  tonemenu::append({ "93 LFO1 RATE", tone100Ptrs, 100, toneLFO1Rate, idxToneLFO1Rate });
  tonemenu::append({ "94 LFO1 LFO", tone100Ptrs, 100, toneLFO1LFO, idxToneLFO1LFO });
  tonemenu::append({ "95 LFO1 SYNC", toneSyncValues, 3, toneLFO1Sync, idxToneLFO1Sync });

  tonemenu::append({ "A1 LFO2 WF", toneLFOValues, 5, toneLFO2WF, idxToneLFO2WF });
  tonemenu::append({ "A2 LFO2 DELAY", tone100Ptrs, 100, toneLFO2Delay, idxToneLFO2Delay });
  tonemenu::append({ "A3 LFO2 RATE", tone100Ptrs, 100, toneLFO2Rate, idxToneLFO2Rate });
  tonemenu::append({ "A4 LFO2 LFO", tone100Ptrs, 100, toneLFO2LFO, idxToneLFO2LFO });
  tonemenu::append({ "A5 LFO2 SYNC", toneSyncValues, 3, toneLFO2Sync, idxToneLFO2Sync });

  tonemenu::append({ "B1 ENV1 T1", tone100Ptrs, 100, toneEnv1T1, idxToneEnv1T1 });
  tonemenu::append({ "B2 ENV1 L1", tone100Ptrs, 100, toneEnv1L1, idxToneEnv1L1 });
  tonemenu::append({ "B3 ENV1 T2", tone100Ptrs, 100, toneEnv1T2, idxToneEnv1T2 });
  tonemenu::append({ "B4 ENV1 L2", tone100Ptrs, 100, toneEnv1L2, idxToneEnv1L2 });
  tonemenu::append({ "B5 ENV1 T3", tone100Ptrs, 100, toneEnv1T3, idxToneEnv1T3 });
  tonemenu::append({ "B6 ENV1 L3", tone100Ptrs, 100, toneEnv1L3, idxToneEnv1L3 });
  tonemenu::append({ "B7 ENV1 T4", tone100Ptrs, 100, toneEnv1T4, idxToneEnv1T4 });
  tonemenu::append({ "B8 ENV1 KEY", toneEnvKeyValues, 8, toneEnv1Key, idxToneEnv1Key });

  tonemenu::append({ "C1 ENV2 T1", tone100Ptrs, 100, toneEnv2T1, idxToneEnv2T1 });
  tonemenu::append({ "C2 ENV2 L1", tone100Ptrs, 100, toneEnv2L1, idxToneEnv2L1 });
  tonemenu::append({ "C3 ENV2 T2", tone100Ptrs, 100, toneEnv2T2, idxToneEnv2T2 });
  tonemenu::append({ "C4 ENV2 L2", tone100Ptrs, 100, toneEnv2L2, idxToneEnv2L2 });
  tonemenu::append({ "C5 ENV2 T3", tone100Ptrs, 100, toneEnv2T3, idxToneEnv2T3 });
  tonemenu::append({ "C6 ENV2 L3", tone100Ptrs, 100, toneEnv2L3, idxToneEnv2L3 });
  tonemenu::append({ "C7 ENV2 T4", tone100Ptrs, 100, toneEnv2T4, idxToneEnv2T4 });
  tonemenu::append({ "C8 ENV2 KEY", toneEnvKeyValues, 8, toneEnv2Key, idxToneEnv2Key });

  tonemenu::append({ "D1 ENV3 ATT", tone100Ptrs, 100, toneEnv3Att, idxToneEnv3Att });
  tonemenu::append({ "D2 ENV3 DECY", tone100Ptrs, 100, toneEnv3Dec, idxToneEnv3Dec });
  tonemenu::append({ "D3 ENV3 SUST", tone100Ptrs, 100, toneEnv3Sust, idxToneEnv3Sust });
  tonemenu::append({ "D4 ENV3 REL", tone100Ptrs, 100, toneEnv3Rel, idxToneEnv3Rel });
  tonemenu::append({ "D5 ENV3 KEY", toneEnvKeyValues, 8, toneEnv3Key, idxToneEnv3Key });

  tonemenu::append({ "E1 ENV4 ATT", tone100Ptrs, 100, toneEnv4Att, idxToneEnv4Att });
  tonemenu::append({ "E2 ENV4 DECY", tone100Ptrs, 100, toneEnv4Dec, idxToneEnv4Dec });
  tonemenu::append({ "E3 ENV4 SUST", tone100Ptrs, 100, toneEnv4Sust, idxToneEnv4Sust });
  tonemenu::append({ "E4 ENV4 REL", tone100Ptrs, 100, toneEnv4Rel, idxToneEnv4Rel });
  tonemenu::append({ "E5 ENV4 KEY", toneEnvKeyValues, 8, toneEnv4Key, idxToneEnv4Key });
}