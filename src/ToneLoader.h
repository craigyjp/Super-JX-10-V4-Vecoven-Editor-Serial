#pragma once


void updateUpperToneData();
void updateLowerToneData();

// ---------------------------------------------------------------------------
// Tone library - 100 tones loaded into RAM on bank select
// Tones 0-49  = programmable (1-50),  file: programmable.jsonl (per bank)
// Tones 50-99 = factory presets (51-100), file: factory.jsonl (per bank)
// ---------------------------------------------------------------------------

#define TONE_PARAM_COUNT 49
#define TONE_NAME_LEN 11  // 10 chars + null
#define TONE_COUNT 100

struct Tone {
  char name[TONE_NAME_LEN];
  uint8_t params[TONE_PARAM_COUNT];
};

Tone toneLibrary[TONE_COUNT];

// ---------------------------------------------------------------------------
// Indices into tone params[] array (matches factory JSON parameter order)
// ---------------------------------------------------------------------------
#define TP_UNDEF_10 0
#define TP_DCO1_RANGE 1
#define TP_DCO1_WAVE 2
#define TP_DCO1_TUNE 3
#define TP_DCO1_LFO_MOD 4
#define TP_DCO1_ENV_MOD 5
#define TP_DCO2_RANGE 6
#define TP_DCO2_WAVE 7
#define TP_DCO2_CROSSMOD 8  // → P_dco1_mode
#define TP_DCO2_TUNE 9
#define TP_DCO2_FINE 10
#define TP_DCO2_LFO_MOD 11
#define TP_DCO2_ENV_MOD 12
#define TP_UNDEF_23 13
#define TP_UNDEF_24 14
#define TP_UNDEF_25 15
#define TP_DCO_DYNAMICS 16  // → P_dco1_pitch_dyn + P_dco2_pitch_dyn
#define TP_DCO_ENV_MODE 17  // → decoded into source+pol for dco1 and dco2
#define TP_MIXER_DCO1_LEVEL 18
#define TP_MIXER_DCO2_LEVEL 19
#define TP_MIXER_ENV_MOD 20  // → P_dco2_mod
#define TP_MIXER_DYNAMICS 21
#define TP_MIXER_ENV_MODE 22  // → decoded into P_dco_mix_env_source + pol
#define TP_HPF_CUTOFF 23
#define TP_VCF_CUTOFF 24
#define TP_VCF_RES 25
#define TP_VCF_LFO_MOD 26
#define TP_VCF_ENV_MOD 27
#define TP_VCF_KEY_FOLLOW 28
#define TP_VCF_DYNAMICS 29
#define TP_VCF_ENV_MODE 30  // → decoded into P_vcf_env_source + pol
#define TP_VCA_LEVEL 31
#define TP_VCA_DYNAMICS 32
#define TP_CHORUS 33
#define TP_LFO_WAVE 34
#define TP_LFO_DELAY 35
#define TP_LFO_RATE 36
#define TP_ENV1_ATTACK 37      // → P_time1
#define TP_ENV1_DECAY 38       // → P_time3
#define TP_ENV1_SUSTAIN 39     // → P_level3
#define TP_ENV1_RELEASE 40     // → P_time4
#define TP_ENV1_KEY_FOLLOW 41  // → P_env5stage_mode (0-3 direct)
#define TP_ENV2_ATTACK 42      // → P_attack
#define TP_ENV2_DECAY 43       // → P_decay
#define TP_ENV2_SUSTAIN 44     // → P_sustain
#define TP_ENV2_RELEASE 45     // → P_release
#define TP_ENV2_KEY_FOLLOW 46  // → P_adsr_mode (0-3 direct)
#define TP_UNDEF_57 47
#define TP_VCA_ENV_MODE 48  // → P_vca_env_source

// ---------------------------------------------------------------------------
// Decode packed env mode (0-3) into Vecoven source and polarity
// 0 = Env1 positive → source=0, pol=1
// 1 = Env1 negative → source=0, pol=0
// 2 = Env3 positive → source=2, pol=1
// 3 = Env3 negative → source=2, pol=0
// ---------------------------------------------------------------------------
static inline void decodeEnvMode(uint8_t raw, uint8_t &source, uint8_t &pol) {
  switch (raw) {
    case 0 ... 31: // ENV1 Pos to Env1 pos
      source = 64;
      pol = 0;
    break;

    case 32 ... 63: // ENV1 Neg to Env1 neg
      source = 80;
      pol = 0;
    break;

    case 64 ... 95: // Env2 Pos to Env3 Pos
      source = 0;
      pol = 0;
    break;

    case 96 ... 127: // Env2 Neg to Env3 Neg
      source = 16;
      pol = 0;
    break;

    default: source = 16; pol = 0; break;  // safe fallback

  }
}

static inline void decodeDynamics(uint8_t raw, uint8_t &source) {
  switch (raw) {
    case 0 ... 31: // Dynamics Off
      source = 0;
    break;

    case 32 ... 63: // Dynamics 1
      source = 32;
    break;

    case 64 ... 95: // Dynamics 2
      source = 64;
    break;

    case 96 ... 127: // Dynamics 3
      source = 96;
    break;

    default: source = 96; break;  // safe fallback

  }
}

static inline void decodeVCAEnvMode(uint8_t raw, uint8_t &source) {
  switch (raw) {
    case 0 ... 63:
      source = 64;
      gatedEnv = true;
      break;

    case 64 ... 127: 
      source = 64; 
      gatedEnv = false;
      break;

    default:         source = 64; break;
  }
}

static inline void decodeDCORange(uint8_t raw, uint8_t &source) {
  switch (raw) {
    case 0 ... 31: source = 0; break;
    case 32 ... 63: source = 1; break;
    case 64 ... 95: source = 2; break;
    case 96 ... 127: source = 3; break;
    default: source = 0; break;  // safe fallback
  }
}

static inline void decodeLFOWave(uint8_t raw, uint8_t &source) {
  switch (raw) {
    case 0 ... 31: source = 0; break;
    case 32 ... 63: source = 1; break;
    case 64 ... 127: source = 4; break;
    default: source = 4; break;  // safe fallback
  }
}

static inline void decodeChorusType(uint8_t raw, uint8_t &source) {
  switch (raw) {
    case 0 ... 31: source = 0; break;
    case 32 ... 63: source = 32; break;
    case 64 ... 127: source = 64; break;
    default: source = 0; break;  // safe fallback
  }
}

static inline void decodeKeyFollow(uint8_t raw, uint8_t &source) {
  switch (raw) {
    case 0 ... 31: source = 0; break;
    case 32 ... 63: source = 1; break;
    case 64 ... 95: source = 2; break;
    case 96 ... 127: source = 3; break;
    default: source = 0; break;  // safe fallback
  }
}

// ---------------------------------------------------------------------------
// Parse a single compact JSONL line into a Tone struct
// Expected format: {"i":1,"n":"NAME","p":[v0,v1,...,v48]}
// ---------------------------------------------------------------------------
bool parseToneLine(const char *line, Tone &tone) {
  const char *np = strstr(line, "\"n\":\"");
  if (!np) return false;
  np += 5;
  const char *ne = strchr(np, '"');
  if (!ne) return false;
  uint8_t nlen = (uint8_t)min((int)(ne - np), TONE_NAME_LEN - 1);
  memcpy(tone.name, np, nlen);
  tone.name[nlen] = '\0';

  const char *pp = strchr(line, '[');
  if (!pp) return false;
  pp++;

  for (int i = 0; i < TONE_PARAM_COUNT; i++) {
    tone.params[i] = (uint8_t)atoi(pp);
    pp = strchr(pp, ',');
    if (!pp) break;
    pp++;
  }
  return true;
}

// ---------------------------------------------------------------------------
// Load one JSONL file into toneLibrary starting at slot offset
// ---------------------------------------------------------------------------
bool loadToneFile(const char *path, uint8_t offset) {
  File f = SD.open(path);
  if (!f) {
    Serial.print(F("Cannot open: "));
    Serial.println(path);
    return false;
  }

  uint8_t idx = offset;
  char lineBuf[220];

  while (f.available() && idx < (offset + 50)) {
    uint8_t pos = 0;
    while (f.available() && pos < sizeof(lineBuf) - 1) {
      char c = f.read();
      if (c == '\n') break;
      lineBuf[pos++] = c;
    }
    lineBuf[pos] = '\0';
    if (pos == 0) continue;

    if (parseToneLine(lineBuf, toneLibrary[idx])) {
      idx++;
    }
  }
  f.close();
  Serial.print(F("Loaded "));
  Serial.print(idx - offset);
  Serial.print(F(" tones from "));
  Serial.println(path);
  return true;
}



// ---------------------------------------------------------------------------
// Copy a source file to a destination path on SD
// Used during bank initialisation to write default tone files
// ---------------------------------------------------------------------------
bool copyFileToSD(const char *srcPath, const char *dstPath) {
  File src = SD.open(srcPath, FILE_READ);
  if (!src) {
    Serial.print(F("copyFileToSD: cannot open src: "));
    Serial.println(srcPath);
    return false;
  }
  File dst = SD.open(dstPath, FILE_WRITE);
  if (!dst) {
    src.close();
    Serial.print(F("copyFileToSD: cannot open dst: "));
    Serial.println(dstPath);
    return false;
  }
  uint8_t buf[64];
  while (src.available()) {
    int n = src.read(buf, sizeof(buf));
    if (n > 0) dst.write(buf, n);
  }
  src.close();
  dst.close();
  return true;
}

// ---------------------------------------------------------------------------
// Ensure tone files exist in a bank folder.
// programmable.jsonl is copied from /programmable.jsonl (master default)
// factory.jsonl is copied from /factory.jsonl (master, same for all banks)
// Call this from ensureJX10BankInitialized() after creating the folder.
// ---------------------------------------------------------------------------
void ensureToneFiles(uint8_t bank) {
  String base = "/bank" + String(bank) + "/";

  String progPath = base + "programmable.jsonl";
  if (!SD.exists(progPath.c_str())) {
    Serial.print(F("Creating "));
    Serial.println(progPath);
    copyFileToSD("/bank0/programmable.jsonl", progPath.c_str());
  }

  String factPath = base + "factory.jsonl";
  if (!SD.exists(factPath.c_str())) {
    Serial.print(F("Creating "));
    Serial.println(factPath);
    copyFileToSD("/factory.jsonl", factPath.c_str());
  }
}

// ---------------------------------------------------------------------------
// Load all 100 tones for a bank into RAM.
// Call this whenever the active bank changes.
// ---------------------------------------------------------------------------
void loadToneLibrary(uint8_t bank) {
  String base = "/bank" + String(bank) + "/";
  loadToneFile((base + "programmable.jsonl").c_str(), 0);  // slots 0-49
  loadToneFile((base + "factory.jsonl").c_str(), 50);      // slots 50-99
}

// ---------------------------------------------------------------------------
// Populate upperData[] or lowerData[] from a tone and send to voice boards.
// toneIndex: 0-99 (array index; tone numbers 1-100 = toneIndex+1)
// upper: true = upperData[], false = lowerData[]
// ---------------------------------------------------------------------------
void loadToneToSlot(uint8_t toneIndex, bool upper) {
  if (toneIndex >= TONE_COUNT) return;

  const Tone &t = toneLibrary[toneIndex];
  int *data = upper ? upperData : lowerData;

  // --- Decode packed env mode values ---
  uint8_t dcoEnvSrc, dcoEnvPol;
  uint8_t mixEnvSrc, mixEnvPol;
  uint8_t vcfEnvSrc, vcfEnvPol;
  uint8_t vcaEnvSrc, vcfDynamics;
  uint8_t dco1Range, dco1Wave;
  uint8_t dco2Range, dco2Wave;
  uint8_t lfo1Wave, dcoMode, keyFollow1, keyFollow2;
  uint8_t decodeChorus;
  decodeEnvMode(t.params[TP_DCO_ENV_MODE], dcoEnvSrc, dcoEnvPol);
  decodeEnvMode(t.params[TP_MIXER_ENV_MODE], mixEnvSrc, mixEnvPol);
  decodeEnvMode(t.params[TP_VCF_ENV_MODE], vcfEnvSrc, vcfEnvPol);
  decodeVCAEnvMode(t.params[TP_VCA_ENV_MODE], vcaEnvSrc);
  decodeDCORange(t.params[TP_DCO1_RANGE], dco1Range);
  decodeDCORange(t.params[TP_DCO1_WAVE], dco1Wave);
  decodeDCORange(t.params[TP_DCO2_RANGE], dco2Range);
  decodeDCORange(t.params[TP_DCO2_WAVE], dco2Wave);
  decodeLFOWave(t.params[TP_LFO_WAVE], lfo1Wave);
  decodeDCORange(t.params[TP_DCO2_CROSSMOD], dcoMode);
  decodeKeyFollow(t.params[TP_ENV1_KEY_FOLLOW], keyFollow1);
  decodeKeyFollow(t.params[TP_ENV2_KEY_FOLLOW], keyFollow2);
  decodeDynamics(t.params[TP_VCF_DYNAMICS], vcfDynamics);
  decodeChorusType(t.params[TP_CHORUS], decodeChorus);

  // --- DCO1 ---
  data[P_dco1_range] = dco1Range;
  data[P_dco1_wave] = dco1Wave;
  data[P_dco1_tune] = t.params[TP_DCO1_TUNE];
  data[P_dco1_pitch_lfo] = t.params[TP_DCO1_LFO_MOD];
  data[P_dco1_pitch_env] = t.params[TP_DCO1_ENV_MOD];
  data[P_dco1_pitch_dyn] = t.params[TP_DCO_DYNAMICS];
  data[P_dco1_pitch_env_source] = dcoEnvSrc;
  data[P_dco1_pitch_env_pol] = dcoEnvPol;
  data[P_dco1_mode] = dcoMode;
  data[P_dco1_level] = t.params[TP_MIXER_DCO1_LEVEL];
  data[P_dco1_PW] = 0;
  data[P_dco1_PWM_env] = 0;
  data[P_dco1_PWM_lfo] = 0;
  data[P_dco1_PWM_dyn] = 0;
  data[P_dco1_PWM_env_source] = 16;
  data[P_dco1_PWM_lfo_source] = 0;

  // --- DCO2 ---
  data[P_dco2_range] = dco2Range;
  data[P_dco2_wave] = dco2Wave;
  data[P_dco2_tune] = t.params[TP_DCO2_TUNE];
  data[P_dco2_fine] = t.params[TP_DCO2_FINE];
  data[P_dco2_pitch_lfo] = t.params[TP_DCO2_LFO_MOD];
  data[P_dco2_pitch_env] = t.params[TP_DCO2_ENV_MOD];
  data[P_dco2_pitch_dyn] = t.params[TP_DCO_DYNAMICS];
  data[P_dco2_pitch_env_source] = dcoEnvSrc;
  data[P_dco2_pitch_env_pol] = dcoEnvPol;
  data[P_dco2_mod] = t.params[TP_MIXER_ENV_MOD];
  data[P_dco2_level] = t.params[TP_MIXER_DCO2_LEVEL];
  data[P_dco2_PW] = 0;
  data[P_dco2_PWM_env] = 0;
  data[P_dco2_PWM_lfo] = 0;
  data[P_dco2_PWM_dyn] = 0;
  data[P_dco2_PWM_env_source] = 16;
  data[P_dco2_PWM_env_pol] = 0;
  data[P_dco2_PWM_lfo_source] = 0;

  // --- Mixer ---
  data[P_dco_mix_dyn] = t.params[TP_MIXER_DYNAMICS];
  data[P_dco_mix_env_source] = mixEnvSrc;
  data[P_dco_mix_env_pol] = mixEnvPol;

  // --- LFO1 ---
  data[P_lfo1_wave] = lfo1Wave;
  data[P_lfo1_delay] = t.params[TP_LFO_DELAY];
  data[P_lfo1_rate] = t.params[TP_LFO_RATE];
  data[P_lfo1_sync] = 0;
  data[P_lfo1_lfo2] = 0;

  // --- LFO2 - zeroed ---
  data[P_lfo2_wave] = 0;
  data[P_lfo2_rate] = 0;
  data[P_lfo2_delay] = 0;
  data[P_lfo2_lfo1] = 0;
  data[P_lfo2_sync] = 0;

  // --- VCF ---
  data[P_vcf_hpf] = t.params[TP_HPF_CUTOFF];
  data[P_vcf_cutoff] = t.params[TP_VCF_CUTOFF];
  data[P_vcf_res] = t.params[TP_VCF_RES];
  data[P_vcf_lfo1] = t.params[TP_VCF_LFO_MOD];
  data[P_vcf_lfo2] = 0;
  data[P_vcf_env] = t.params[TP_VCF_ENV_MOD];
  data[P_vcf_kb] = t.params[TP_VCF_KEY_FOLLOW];
  data[P_vcf_dyn] = vcfDynamics;
  data[P_vcf_env_source] = vcfEnvSrc;
  data[P_vcf_env_pol] = vcfEnvPol;

  // --- VCA ---
  data[P_vca_mod] = t.params[TP_VCA_LEVEL];
  data[P_vca_dyn] = t.params[TP_VCA_DYNAMICS];
  data[P_vca_env_source] = vcaEnvSrc;

  // --- Chorus ---
  data[P_chorus] = decodeChorus;

  // --- Env1 ---
  data[P_time1] = t.params[TP_ENV1_ATTACK];
  data[P_level1] = 127;
  data[P_time2] = 0;
  data[P_level2] = 127;
  data[P_time3] = t.params[TP_ENV1_DECAY];
  data[P_level3] = t.params[TP_ENV1_SUSTAIN];
  data[P_time4] = t.params[TP_ENV1_RELEASE];
  data[P_env5stage_mode] = keyFollow1;

  // --- Env2 - zeroed ---
  data[P_env2_time1] = 0;
  data[P_env2_level1] = 0;
  data[P_env2_time2] = 0;
  data[P_env2_level2] = 0;
  data[P_env2_time3] = 0;
  data[P_env2_level3] = 0;
  data[P_env2_time4] = 0;
  data[P_env2_5stage_mode] = 0;

  // --- Env3 (VCA envelope) ---
  if (gatedEnv) {
    data[P_attack] = 0;
    data[P_decay] = 0;
    data[P_sustain] = 127;
    data[P_release] = 0;
  } else {
    data[P_attack] = t.params[TP_ENV2_ATTACK];
    data[P_decay] = t.params[TP_ENV2_DECAY];
    data[P_sustain] = t.params[TP_ENV2_SUSTAIN];
    data[P_release] = t.params[TP_ENV2_RELEASE];
  }
  data[P_adsr_mode] = keyFollow2;

  // --- Env4 - zeroed ---
  data[P_env4_attack] = 0;
  data[P_env4_decay] = 0;
  data[P_env4_sustain] = 0;
  data[P_env4_release] = 0;
  data[P_env4_adsr_mode] = 0;

  // --- Tone number (1-based) ---
  if (upper) {
    upperToneNumber = toneIndex + 1;
  } else {
    lowerToneNumber = toneIndex + 1;
  }

  // --- Send to voice boards and update LEDs ---
  if (upper) {
    updateUpperToneData();
  } else {
    updateLowerToneData();
  }

  Serial.print(F("Tone loaded: "));
  Serial.print(toneIndex + 1);
  Serial.print(F(" "));
  Serial.println(t.name);

}

void loadToneToSlotData(uint8_t toneIndex, bool upper) {
  if (toneIndex >= TONE_COUNT) return;

  const Tone &t = toneLibrary[toneIndex];
  int *data = upper ? upperData : lowerData;

  // --- Decode packed env mode values ---
  uint8_t dcoEnvSrc, dcoEnvPol;
  uint8_t mixEnvSrc, mixEnvPol;
  uint8_t vcfEnvSrc, vcfEnvPol;
  uint8_t vcaEnvSrc, vcfDynamics;
  uint8_t dco1Range, dco1Wave;
  uint8_t dco2Range, dco2Wave;
  uint8_t lfo1Wave, dcoMode, keyFollow1, keyFollow2;
  uint8_t decodeChorus;
  decodeEnvMode(t.params[TP_DCO_ENV_MODE], dcoEnvSrc, dcoEnvPol);
  decodeEnvMode(t.params[TP_MIXER_ENV_MODE], mixEnvSrc, mixEnvPol);
  decodeEnvMode(t.params[TP_VCF_ENV_MODE], vcfEnvSrc, vcfEnvPol);
  decodeVCAEnvMode(t.params[TP_VCA_ENV_MODE], vcaEnvSrc);
  decodeDCORange(t.params[TP_DCO1_RANGE], dco1Range);
  decodeDCORange(t.params[TP_DCO1_WAVE], dco1Wave);
  decodeDCORange(t.params[TP_DCO2_RANGE], dco2Range);
  decodeDCORange(t.params[TP_DCO2_WAVE], dco2Wave);
  decodeLFOWave(t.params[TP_LFO_WAVE], lfo1Wave);
  decodeDCORange(t.params[TP_DCO2_CROSSMOD], dcoMode);
  decodeKeyFollow(t.params[TP_ENV1_KEY_FOLLOW], keyFollow1);
  decodeKeyFollow(t.params[TP_ENV2_KEY_FOLLOW], keyFollow2);
  decodeDynamics(t.params[TP_VCF_DYNAMICS], vcfDynamics);
  decodeChorusType(t.params[TP_CHORUS], decodeChorus);

  // --- DCO1 ---
  data[P_dco1_range] = dco1Range;
  data[P_dco1_wave] = dco1Wave;
  data[P_dco1_tune] = t.params[TP_DCO1_TUNE];
  data[P_dco1_pitch_lfo] = t.params[TP_DCO1_LFO_MOD];
  data[P_dco1_pitch_env] = t.params[TP_DCO1_ENV_MOD];
  data[P_dco1_pitch_dyn] = t.params[TP_DCO_DYNAMICS];
  data[P_dco1_pitch_env_source] = dcoEnvSrc;
  data[P_dco1_pitch_env_pol] = dcoEnvPol;
  data[P_dco1_mode] = dcoMode;
  data[P_dco1_level] = t.params[TP_MIXER_DCO1_LEVEL];
  data[P_dco1_PW] = 0;
  data[P_dco1_PWM_env] = 0;
  data[P_dco1_PWM_lfo] = 0;
  data[P_dco1_PWM_dyn] = 0;
  data[P_dco1_PWM_env_source] = 16;
  data[P_dco1_PWM_lfo_source] = 0;

  // --- DCO2 ---
  data[P_dco2_range] = dco2Range;
  data[P_dco2_wave] = dco2Wave;
  data[P_dco2_tune] = t.params[TP_DCO2_TUNE];
  data[P_dco2_fine] = t.params[TP_DCO2_FINE];
  data[P_dco2_pitch_lfo] = t.params[TP_DCO2_LFO_MOD];
  data[P_dco2_pitch_env] = t.params[TP_DCO2_ENV_MOD];
  data[P_dco2_pitch_dyn] = t.params[TP_DCO_DYNAMICS];
  data[P_dco2_pitch_env_source] = dcoEnvSrc;
  data[P_dco2_pitch_env_pol] = dcoEnvPol;
  data[P_dco2_mod] = t.params[TP_MIXER_ENV_MOD];
  data[P_dco2_level] = t.params[TP_MIXER_DCO2_LEVEL];
  data[P_dco2_PW] = 0;
  data[P_dco2_PWM_env] = 0;
  data[P_dco2_PWM_lfo] = 0;
  data[P_dco2_PWM_dyn] = 0;
  data[P_dco2_PWM_env_source] = 16;
  data[P_dco2_PWM_env_pol] = 0;
  data[P_dco2_PWM_lfo_source] = 0;

  // --- Mixer ---
  data[P_dco_mix_dyn] = t.params[TP_MIXER_DYNAMICS];
  data[P_dco_mix_env_source] = mixEnvSrc;
  data[P_dco_mix_env_pol] = mixEnvPol;

  // --- LFO1 ---
  data[P_lfo1_wave] = lfo1Wave;
  data[P_lfo1_delay] = t.params[TP_LFO_DELAY];
  data[P_lfo1_rate] = t.params[TP_LFO_RATE];
  data[P_lfo1_sync] = 0;
  data[P_lfo1_lfo2] = 0;

  // --- LFO2 - zeroed ---
  data[P_lfo2_wave] = 0;
  data[P_lfo2_rate] = 0;
  data[P_lfo2_delay] = 0;
  data[P_lfo2_lfo1] = 0;
  data[P_lfo2_sync] = 0;

  // --- VCF ---
  data[P_vcf_hpf] = t.params[TP_HPF_CUTOFF];
  data[P_vcf_cutoff] = t.params[TP_VCF_CUTOFF];
  data[P_vcf_res] = t.params[TP_VCF_RES];
  data[P_vcf_lfo1] = t.params[TP_VCF_LFO_MOD];
  data[P_vcf_lfo2] = 0;
  data[P_vcf_env] = t.params[TP_VCF_ENV_MOD];
  data[P_vcf_kb] = t.params[TP_VCF_KEY_FOLLOW];
  data[P_vcf_dyn] = vcfDynamics;
  data[P_vcf_env_source] = vcfEnvSrc;
  data[P_vcf_env_pol] = vcfEnvPol;

  // --- VCA ---
  data[P_vca_mod] = t.params[TP_VCA_LEVEL];
  data[P_vca_dyn] = t.params[TP_VCA_DYNAMICS];
  data[P_vca_env_source] = vcaEnvSrc;

  // --- Chorus ---
  data[P_chorus] = decodeChorus;

  // --- Env1 ---
  data[P_time1] = t.params[TP_ENV1_ATTACK];
  data[P_level1] = 127;
  data[P_time2] = 0;
  data[P_level2] = 127;
  data[P_time3] = t.params[TP_ENV1_DECAY];
  data[P_level3] = t.params[TP_ENV1_SUSTAIN];
  data[P_time4] = t.params[TP_ENV1_RELEASE];
  data[P_env5stage_mode] = keyFollow1;

  // --- Env2 - zeroed ---
  data[P_env2_time1] = 0;
  data[P_env2_level1] = 0;
  data[P_env2_time2] = 0;
  data[P_env2_level2] = 0;
  data[P_env2_time3] = 0;
  data[P_env2_level3] = 0;
  data[P_env2_time4] = 0;
  data[P_env2_5stage_mode] = 0;

  // --- Env3 (VCA envelope) ---
  if (gatedEnv) {
    data[P_attack] = 0;
    data[P_decay] = 0;
    data[P_sustain] = 127;
    data[P_release] = 0;
  } else {
    data[P_attack] = t.params[TP_ENV2_ATTACK];
    data[P_decay] = t.params[TP_ENV2_DECAY];
    data[P_sustain] = t.params[TP_ENV2_SUSTAIN];
    data[P_release] = t.params[TP_ENV2_RELEASE];
  }
  data[P_adsr_mode] = keyFollow2;

  // --- Env4 - zeroed ---
  data[P_env4_attack] = 0;
  data[P_env4_decay] = 0;
  data[P_env4_sustain] = 0;
  data[P_env4_release] = 0;
  data[P_env4_adsr_mode] = 0;

  // --- Tone number (1-based) ---
  if (upper) {
    upperToneNumber = toneIndex + 1;
  } else {
    lowerToneNumber = toneIndex + 1;
  }

}
