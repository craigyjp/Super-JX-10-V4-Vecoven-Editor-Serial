//Values below are just for initialising and will be changed when synth is initialised to current panel controls & EEPROM settings
byte midiChannel = 1;  //(EEPROM)
byte midiOutCh = 1; 

enum AssignCat { CAT_POLY = 0, CAT_UNI = 1, CAT_MONO = 2 };

static inline int assignCat(int v)     { return v & 3; }
static inline int assignVariant(int v) { return (v >> 2) & 1; }
static inline int assignMake(int cat, int variant) { return (variant << 2) | cat; }

static const char *assignLabels[7] = {
  "POLY 1", "UNI 1", "MONO 1", "",    // 0, 1, 2, 3-unused
  "POLY 2", "UNI 2", "MONO 2"         // 4, 5, 6
};

bool patchNameEditMode = false;
int  patchNameCursor   = 0;
char patchNameBuffer[19];   // 18 chars + null

static const char patchSpecialChars[8] = {
  '-', '*', '/', '\x03', '<', '>', ',', ' '
};

enum {
  NAMING_DIGIT = 0,
  NAMING_SPECIAL,
  NAMING_CURSOR_LEFT,
  NAMING_CURSOR_RIGHT,
  NAMING_ENCODER_UP,
  NAMING_ENCODER_DOWN,
};

struct VoiceAndNote {
  int note;
  int velocity;
  unsigned long timeOn;
  bool sustained;  // Sustain flag
  bool keyDown;
  double noteFreq;  // Note frequency
  int position;
  bool noteOn;
};

struct VoiceAndNote voices[NO_OF_VOICES] = {
  { -1, -1, 0, false, false, 0, -1, false },
  { -1, -1, 0, false, false, 0, -1, false },
  { -1, -1, 0, false, false, 0, -1, false },
  { -1, -1, 0, false, false, 0, -1, false },
  { -1, -1, 0, false, false, 0, -1, false },
  { -1, -1, 0, false, false, 0, -1, false },
  { -1, -1, 0, false, false, 0, -1, false },
  { -1, -1, 0, false, false, 0, -1, false },
  { -1, -1, 0, false, false, 0, -1, false },
  { -1, -1, 0, false, false, 0, -1, false },
  { -1, -1, 0, false, false, 0, -1, false },
  { -1, -1, 0, false, false, 0, -1, false }
};

// Tracks exactly which note each voice currently plays
int voiceToNoteLower[6] = { -1, -1, -1, -1, -1, -1 };
int voiceToNoteUpper[6] = { -1, -1, -1, -1, -1, -1 };

bool voiceOn[NO_OF_VOICES] = { false, false, false, false, false, false, false, false, false, false, false, false };

int noteMsg;
int prevNote = 0;  //Initialised to middle value
bool notes[128] = { 0 }, initial_loop = 1;
int8_t noteOrder[40] = { 0 }, orderIndx = { 0 };

bool notesWhole[128], notesLower[128], notesUpper[128];
byte noteOrderWhole[40], noteOrderLower[40], noteOrderUpper[40];
int orderIndxWhole = 0, orderIndxLower = 0, orderIndxUpper = 0;

int voiceAssignmentLower[128];
int voiceAssignmentUpper[128];

int noteVel;
int lastPlayedNote = -1;  // Track the last note played
int lastPlayedVoice = 0;  // Track the voice of the last note played
int lastUsedVoice = 0;    // Global variable to store the last used voice

int patchNo = 1;               //Current patch no
int voiceToReturn = -1;        //Initialise
unsigned long earliestTime = millis();  //For voice allocation - initialise to now

int resolutionFrig = 1;

uint32_t noteAgeCounter = 1;
int nextVoiceRR = 0;
uint8_t voiceNote[9];  // per-voice note index (0..127 or into noteFreqs)

// JP8 Hold
// -------------------- HOLD CONFIG --------------------

bool keyDownLower[128] = {0};
bool keyDownUpper[128] = {0};
bool keyDownWhole[128] = {0}; // optional (whole mode convenience)

bool holdManualLower = false;
bool holdManualUpper = false;
bool holdPedal = false;     // DP-2 pressed

bool holdLatchedLower[128] = {0}; // notes sustaining because Hold caught their note-off
bool holdLatchedUpper[128] = {0};

bool sustainPedalDown = false;     // CCsustain >= 64

// Encoders - is it needed?

// adding encoders
bool rotaryEncoderChanged(int id, bool clockwise, int speed);
#define NUM_ENCODERS 51
unsigned long lastTransition[NUM_ENCODERS + 1];
unsigned long lastDisplayTriggerTime = 0;
bool waitingToUpdate = false;
const unsigned long displayTimeout = 5000;  // e.g. 5 seconds

int MIDIThru = midi::Thru::Off;  //(EEPROM)
String patchName = INITPATCHNAME;
String toneNameU = INITTONEU;
String toneNameL = INITTONEL;
bool encCW = true;  //This is to set the encoder to increment when turned CW - Settings Option
bool announce = true;
bool recallPatchFlag = true;
byte accelerate = 1;
int speed = 1;
bool updateParams = false;  //(EEPROM)
int old_value = 0;
int old_param_offset = 0;
int displayMode = 0;
int editMode = 0;
bool switchLEDS = false;;

bool manualSyncInProgress = false;
bool suppressParamAnnounce = true;
bool bootInitInProgress = true;

// ---- UI controls (0..255 unless noted) ----
uint8_t uiCutoff      = 128;  // base cutoff
uint8_t uiKeytrackAmt = 128;  // keytrack depth
uint8_t uiEnvAmt      = 128;  // filter env depth
uint8_t uiLfoAmt      = 0;    // LFO depth to filter

// Dirty flag + lightweight throttle
volatile bool pitchDirty = true;
elapsedMillis msSincePitchUpdate;

// ---------------------------------------------------------------------------
// Bank select blink state (for Recall-button bank-select mode)
// ---------------------------------------------------------------------------

bool bankSelectMode    = false;   // true while waiting for A-H bank choice
bool saveToBankMode    = false;   // true when bank-select is for save destination
int  saveTargetBank    = 0;
int  saveTargetGroup   = 0;
int  saveTargetSlot    = 1;


bool saveBankPicking  = false;  // true = in "choose different bank" sub-flow

unsigned long bankBlinkTimer = 0;
bool bankBlinkState = false;
#define BANK_BLINK_INTERVAL 300  // ms

// ---------------------------------------------------------------------------------
// --- Tone entry state ---
// ---------------------------------------------------------------------------------

unsigned long toneBlinkTimer = 0;
bool toneBlinkState = false;
#define TONE_BLINK_INTERVAL 300

int8_t  toneDigit1     = -1;   // -1 = no digit entered yet
int8_t  toneEntryBuffer = 0;
bool    toneEntryActive = false;

// Sysex reception
// ---------------------Sysex variables



// New JX parameters

int upperData[102];
int lowerData[102];
int upperswapData[102];
int lowerswapData[102];
int panelData[102];
int patchData[30];

#define P_sysex 0
#define P_lfo1_wave 1
#define P_lfo1_rate 2
#define P_lfo1_delay 3
#define P_lfo1_lfo2 4
#define P_lfo1_sync 5
#define P_lfo2_wave 6
#define P_lfo2_rate 7
#define P_lfo2_delay 8
#define P_lfo2_lfo1 9
#define P_lfo2_sync 10
#define P_dco1_PW 11
#define P_dco1_PWM_env 12
#define P_dco1_PWM_lfo 13
#define P_dco1_PWM_dyn 14
#define P_dco1_PWM_env_source 15
#define P_dco1_PWM_env_pol 16
#define P_dco1_PWM_lfo_source 17
#define P_dco1_pitch_env 18
#define P_dco1_pitch_env_source 19
#define P_dco1_pitch_env_pol 20
#define P_dco1_pitch_lfo 21
#define P_dco1_pitch_lfo_source 22
#define P_dco1_pitch_dyn 23
#define P_dco1_wave 24
#define P_dco1_range 25
#define P_dco1_tune 26
#define P_dco1_mode 27
#define P_dco2_PW 28
#define P_dco2_PWM_env 29
#define P_dco2_PWM_lfo 30
#define P_dco2_PWM_dyn 31
#define P_dco2_PWM_env_source 32
#define P_dco2_PWM_env_pol 33
#define P_dco2_PWM_lfo_source 34
#define P_dco2_pitch_env 35
#define P_dco2_pitch_env_source 36
#define P_dco2_pitch_env_pol 37
#define P_dco2_pitch_lfo 38
#define P_dco2_pitch_lfo_source 39
#define P_dco2_pitch_dyn 40
#define P_dco2_wave 41
#define P_dco2_range 42
#define P_dco2_tune 43
#define P_dco2_fine 44
#define P_dco1_level 45
#define P_dco2_level 46
#define P_dco2_mod 47
#define P_dco_mix_env_pol 48
#define P_dco_mix_env_source 49
#define P_dco_mix_dyn 50
#define P_toneName 51
#define P_vcf_hpf 52
#define P_vcf_cutoff 53
#define P_vcf_res 54
#define P_vcf_kb 55
#define P_vcf_env 56
#define P_vcf_lfo1 57
#define P_vcf_lfo2 58
#define P_vcf_env_source 59
#define P_vcf_env_pol 60
#define P_vcf_dyn 61
#define P_vca_mod 62
#define P_vca_env_source 63
#define P_vca_dyn 64
#define P_time1 65
#define P_level1 66
#define P_time2 67
#define P_level2 68
#define P_time3 69
#define P_level3 70
#define P_time4 71
#define P_env5stage_mode 72
#define P_env2_time1 73
#define P_env2_level1 74
#define P_env2_time2 75
#define P_env2_level2 76
#define P_env2_time3 77
#define P_env2_level3 78
#define P_env2_time4 79
#define P_env2_5stage_mode 80
#define P_attack 81
#define P_decay 82
#define P_sustain 83
#define P_release 84
#define P_adsr_mode 85
#define P_env4_attack 86
#define P_env4_decay 87
#define P_env4_sustain 88
#define P_env4_release 89
#define P_env4_adsr_mode 90
#define P_chorus 91

int arpRangeU;
int arpRangeL;
int arpModeU;
int arpModeL;

int at_vib;
int at_bri;
int at_vol;
int balance;
int portamento;
int portamento_sw;
int volume;
int dualdetune = 0x2C;
int bend_range;
int bend_range2;
int bend_enable;
int after_enable;
int keyMode = 0;
int keyMode2 = 0;
int chaseLevel;
int chaseMode;
int chaseTime;
int chasePlay;
int upperSplitPoint;
int lowerSplitPoint;
int upperToneNumber;
int lowerToneNumber;
int upperChromatic;
int lowerChromatic;
int upperAssign;
int lowerAssign;
int upperUnisonDetune = 0x2C;
int lowerUnisonDetune = 0x2C;
int upperHold;
int lowerHold;
int upperLFOModDepth;
int lowerLFOModDepth;
int upperPortamento_SW;
int lowerPortamento_SW;
int upperbend_enable_SW;
int lowerbend_enable_SW;
int at_vib_sw = 0;
int at_bri_sw = 0;
int at_vol_sw = 0;
int midiSplitPoint = 60;

// MIDI menu globals — loaded from EEPROM at boot via loadMidiSettings()
byte upperLocal       = 1;
byte lowerLocal       = 1;
byte upperChannel     = 1;
byte lowerChannel     = 1;
byte controlChannel   = 1;
byte patchProgChange  = 3;
byte sysexExclusive   = 3;
byte sysexApr         = 3;
byte realTime         = 1;
byte upperProgChange  = 3;
byte upperAfterTouch  = 3;
byte upperBender      = 3;
byte upperModulation  = 3;
byte upperPortamento  = 3;
byte upperMIDIHold    = 3;
byte upperVolume      = 3;
byte lowerProgChange  = 3;
byte lowerAfterTouch  = 3;
byte lowerBender      = 3;
byte lowerModulation  = 3;
byte lowerPortamento  = 3;
byte lowerMIDIHold    = 3;
byte lowerVolume      = 3;
byte c1c2ToneEdit     = 0;

uint8_t chaseEditField = 0;   // 0=Time, 1=Level, 2=Mode

int unisondetune_str;
int mod_lfo_str;
int at_vib_str;
int at_bri_str;
int at_vol_str;
int balance_str;
int portamento_str;
int volume_str;
int dualdetune_str;
int bend_range_str;

bool set10ctave = false;
bool octave_down_upwards = true;  // true = going up, false = going down
int upperChromaticSW = 0;
int lowerChromaticSW = 0;
uint8_t upperByte = 0;
uint8_t lowerByte = 0;
int octave_upU = 0;
int octave_upL = 0;
bool octave_up_upwards = true;
bool gatedEnv = false;

// Balance variables

static constexpr uint8_t kBalanceParam = 0x9E;
static constexpr uint8_t kBoardLowerPrefix = 0xF1;
static constexpr uint8_t kBoardUpperPrefix = 0xF9;
static constexpr uint8_t kBoardBothPrefix  = 0xF4; // keyMode 1/2 broadcast
static constexpr uint8_t kBoardOffset0Prefix  = 0x0C;
static constexpr uint8_t kBoardOffset1Prefix  = 0x0D;
static constexpr uint8_t kBoardOffset2Prefix  = 0x0E;
static constexpr uint8_t kBoardOffset3Prefix  = 0x0F;
static constexpr uint8_t kMaxLevel = 0x69;         // 96
static constexpr uint8_t kBoardLowerMIDIPrefix = 0x02;
static constexpr uint8_t kBoardUpperMIDIPrefix = 0x01;


// Dual detune
static constexpr uint8_t kDualDetuneParam = 0xB4; // TODO: set your actual param
static constexpr uint8_t kDualDetuneParam2 = 0xBE; // TODO: set your actual param
static constexpr uint8_t kDetuneZeroPos   = 0x2C; // "00"
static constexpr uint8_t kDetuneNegZero   = 0x2B; // "-00"
static constexpr uint8_t kDetuneMax       = 0x6B; // "+50"
constexpr uint8_t TUNE_442HZ = 0x41;  // 442.0 Hz threshold


// Pitch bend

static constexpr uint8_t kPrefixBroadcast = 0xF4;
static constexpr uint8_t kPitchSignParam  = 0xBF; // 0x00=negative, 0x7F=positive
static constexpr uint8_t kPitchValueParam = 0xB2; // 0..0x7F magnitude
static constexpr bool kInvertPb14 = true;

// None saved variables

int at_vib_safe = 0;
int at_bri_safe = 0;
int at_vol_safe = 0;
int lfo1_sync = 0;
int lfo2_sync = 0;
int chorus = 0;
int vca_env_source = 0;
int vca_dyn = 0;
int vcf_dyn = 0;
int vcf_env_source = 0;
int vcf_env_pol = 1;
int dco_mix_dyn = 0;
int dco_mix_env_source = 0;
int dco_mix_env_pol = 1;
int dco1_pitch_lfo_source = 0;
int dco2_pitch_lfo_source = 0;
int dco1_pitch_env_source = 0;
int dco2_pitch_env_source = 0;
int dco1_pitch_env_pol = 1;
int dco2_pitch_env_pol = 1;
int dco1_pitch_dyn = 0;
int dco2_pitch_dyn = 0;
int dco1_PWM_lfo_source = 0;
int dco2_PWM_lfo_source = 0;
int dco1_PWM_env_source = 0;
int dco2_PWM_env_source = 0;
int dco1_PWM_env_pol = 1;
int dco2_PWM_env_pol = 1;
int dco1_PWM_dyn = 0;
int dco2_PWM_dyn = 0;
int masterTune = 0;

int oldkeyMode = -1;
bool keyModeUpper = false;
bool keyModeLower = false;
int adsr = 0;
int env5stage = 0;
bool upperSW = false;
bool oldupperSW = false;


uint8_t lowerMT1 = 0x2C;
uint8_t lowerMT2 = 0x2C;
uint8_t upperMT1 = 0x2C;
uint8_t upperMT2 = 0x2C;
uint8_t lowerSend = 0;
uint8_t lowerVCASend, upperVCASend;
constexpr uint8_t TUNE_CENTER   = 0x2C;  // A 440 Hz / 0 detune
constexpr uint8_t TUNE_MIN      = 0x00;
constexpr uint8_t TUNE_MAX      = 0x7F;
constexpr uint8_t DETUNE_MAX    = 0x6B;  // physical slider max
constexpr uint8_t DETUNE_CENTER = 64;  // slider center, display shows "00"
uint8_t lastStepParam = 0xFF;
uint8_t lastModWheelValue = 0;

uint8_t LAST_PARAM = 0x00;
uint8_t EXTRA_OFFSET = 0x00;
uint8_t board = 0xF4;
#define NO_OFFSET 0xFE
#define NO_BOARD 0xFE

boolean dual_button;
boolean split_button;
boolean single_button;
boolean special_button;


byte oldsplitPoint = 0;
byte newsplitPoint = 0;
byte splitTrans = 0;
byte oldsplitTrans = 0;
int lowerTranspose = 0;
int lowerSplitVoicePointer = 0;
int upperSplitVoicePointer = 0;
int lowerUnisonVoicePointer = 0;
int upperUnisonVoicePointer = 0;
int wholeUnisonVoicePointer = 0;

// Voice board setup

volatile bool voice1ResetTriggered = false;
volatile bool voice2ResetTriggered = false;

// 5x8 custom chars (each row uses bits 0..4)

byte midBar2[] = {
  0b00110,
  0b00110,
  0b00110,
  0b00110,  // center bar row 1
  0b00110,  // center bar row 2
  0b00110,
  0b00110,
  0b00110
};

// TM — dotted top line
byte frameTopMid[8] = {
  B00000,
  B00000,
  B10101,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000
};

byte frameBotMid[8] = {
  B00000,
  B00000,
  B00000,
  B00000,
  B10101,
  B00000,
  B00000,
  B00000
};

byte frameTopLeft[8] = {
  B00000,     // top row dotted
  B00000,     // left column dotted
  B10101,
  B00000,
  B10000,
  B00000,
  B10000,
  B00000
};

byte frameTopRight[8] = {
  B00000,
  B00000,
  B10101,
  B00000,
  B00001,
  B00000,
  B00001,
  B00000
};

byte frameBotLeft[8] = {
  B10000,
  B00000,
  B10000,
  B00000,
  B10101,
  B00000,
  B00000,
  B00000      // bottom row dotted
};

byte frameBotRight[8] = {
  B00001,
  B00000,
  B00001,
  B00000,
  B10101,
  B00000,
  B00000,
  B00000
};

byte backslashGlyph[] = {
  B00000,
  B10000,
  B01000,
  B00100,
  B00010,
  B00001,
  B00000,
  B00000
};






