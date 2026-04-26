static inline uint8_t bendStepToValue(uint8_t step) {
  switch (step) {
    case 0: return 0x00;  // 2
    case 1: return 0x10;  // 3
    case 2: return 0x20;  // 4
    case 3: return 0x30;  // 7
    case 4: return 0x70;  // 12
    default: return 0x00;
  }
}

static inline const char *bendStepToLabel(uint8_t step) {
  switch (step) {
    case 0: return "2 SEMITONES";
    case 1: return "3 SEMITONES";
    case 2: return "4 SEMITONES";
    case 3: return "7 SEMITONES";
    case 4: return "12 SEMITONES";
    default: return "2 SEMITONES";
  }
}

static inline uint8_t lfowaveStepToValue(uint8_t step) {
  switch (step) {
    case 0: return 0x00;  // 2
    case 1: return 0x10;  // 3
    case 2: return 0x20;  // 4
    case 3: return 0x30;  // 7
    case 4: return 0x40;  // 12
    default: return 0x10;  // 2
  }
}

static inline const char *lfowaveStepToLabel(uint8_t step) {
  switch (step) {
    case 0: return "RAND";
    case 1: return "SQR";
    case 2: return "SAW-";
    case 3: return "SAW+";
    default: return "SINE";
  }
}

static inline uint8_t dco1waveStepToValue(uint8_t step) {
  switch (step) {
    case 0: return 0x00;  // NOIS
    case 1: return 0x20;  // SQR
    case 2: return 0x40;  // PWM
    case 3: return 0x60;  // SAW
    default: return 0x00;
  }
}

static inline const char *dco1waveStepToLabel(uint8_t step) {
  switch (step) {
    case 0: return "NOIS";
    case 1: return "SQR";
    case 2: return "PWM";
    case 3: return "SAW";
    default: return "NOIS";
  }
}

static inline uint8_t dco1rangeStepToValue(uint8_t step) {
  switch (step) {
    case 0: return 0x00;  // 16
    case 1: return 0x20;  // 8
    case 2: return 0x40;  // 4
    case 3: return 0x60;  // 2
    default: return 0x00;
  }
}

static inline const char *dco1rangeStepToLabel(uint8_t step) {
  switch (step) {
    case 0: return "16";
    case 1: return "8";
    case 2: return "4";
    case 3: return "2";
    default: return "16";
  }
}

static inline uint8_t dco1xmodStepToValue(uint8_t step) {
  switch (step) {
    case 0: return 0x00;  // OFF
    case 1: return 0x20;  // SYNC1
    case 2: return 0x40;  // SYNC2
    case 3: return 0x60;  // X-MOD
    default: return 0x00;
  }
}

static inline const char *dco1xmodStepToLabel(uint8_t step) {
  switch (step) {
    case 0: return "OFF";
    case 1: return "SYNC1";
    case 2: return "SYNC2";
    case 3: return "X-MOD";
    default: return "OFF";
  }
}

static inline uint8_t env5stageModeStepToValue(uint8_t step) {
  return step <= 7 ? (step * 0x10) : 0x00;  // 0x00, 0x10, 0x20...0x70
}

static inline const char *env5stageModeStepToLabel(uint8_t step) {
  switch (step) {
    case 0: return "OFF";
    case 1: return "KEY1";
    case 2: return "KEY2";
    case 3: return "KEY3";
    case 4: return "LOOP0";
    case 5: return "LOOP1";
    case 6: return "LOOP2";
    case 7: return "LOOP3";
    default: return "OFF";
  }
}
