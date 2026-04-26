// Arp parameter indices
#define ARP_P_BPM 0
#define ARP_P_MODE 1
#define ARP_P_OCTAVE 2
#define ARP_P_INSERT 3
#define ARP_P_RATE 4
#define ARP_P_DURATION 5
#define ARP_P_VELOCITY 6
#define ARP_P_MIDI 7

// Arp modes
#define ARP_MODE_UP 0
#define ARP_MODE_DOWN 1
#define ARP_MODE_UPDOWN 2
#define ARP_MODE_PLAYED 3
#define ARP_MODE_RANDOM 4

// Arp insert modes
#define ARP_INS_NONE 0
#define ARP_INS_HIGHEST 1
#define ARP_INS_LOWEST 2
#define ARP_INS_UP3DOWN1 3
#define ARP_INS_UP4DOWN2 4

// Arp MIDI routing
#define ARP_MIDI_KEYBOARD 0
#define ARP_MIDI_MIDI 1
#define ARP_MIDI_ALL 2
#define ARP_MIDI_KEYB_OUT 3
#define ARP_MIDI_MIDI_OUT 4
#define ARP_MIDI_ALL_OUT 5

#define ARP_EEPROM_BASE 256  // 0-255 reserved for system variables

// Rate table - numerator/denominator pairs relative to one beat
// A beat = one quarter note
const uint8_t arpRateNum[] =  { 4, 6, 2, 3, 4, 1, 3, 2, 1, 3, 2, 1, 2, 1 };
const uint8_t arpRateDen[] =  { 1, 1, 1, 1, 3, 1, 2, 3, 1, 4, 3, 2, 3, 4 };
const char* arpRateNames[] = {
  "1", "1/2D", "1/2", "1/4D", "1/2T",
  "1/4", "1/8D", "1/4T", "1/8", "1/16D",
  "1/8T", "1/16", "1/16T", "1/32"
};
#define ARP_RATE_COUNT 14

const char* arpModeNames[]   = { "UP", "DOWN", "UP-DOWN", "PLAYED", "RANDOM" };
const char* arpInsertNames[] = { "NONE", "HIGHEST", "LOWEST", "UP3 DN1", "UP4 DN2" };
const char* arpMidiNames[]   = { "KEYBOARD", "MIDI", "ALL", "KEYB+OUT", "MIDI+OUT", "ALL+OUT" };

// Arp memory structure - 8 bytes per slot, 8 slots = 64 bytes EEPROM
struct ArpMemory {
  uint8_t bpm;       // 0-19 = EXTERNAL, 20-250 = internal BPM
  uint8_t mode;      // 0-4 ARP_MODE_*
  uint8_t octave;    // 1-4
  uint8_t insert;    // 0-4 ARP_INS_*
  uint8_t rate;      // 0-13 index into rate table
  uint8_t duration;  // 1-16 (16 = legato)
  uint8_t velocity;  // 0 = played, 1-127 = fixed
  uint8_t midiRoute; // 0-5 ARP_MIDI_*
};

ArpMemory arpMemories[8];
ArpMemory arp;  // working copy

// Runtime arp state
uint8_t arpParamIndex = 0;   // selected parameter 0-7
uint8_t arpMemoryIndex = 0;  // selected memory slot 0-7
bool arpRunning = false;
bool arpLatch = false;
bool arpFunctionMode = false;
bool arpKeysPhysicallyHeld = false;  // true if any key is currently down
uint32_t arpNoteOnTimeUs = 0;  // timestamp of last note on for gate timing
uint32_t arpLedOffTimeMs = 0;
bool arpLedOn = false;
uint8_t arpInsertCounter = 0;  // tracks position within insert pattern
bool arpEnabled = false;  // true when START has been pressed, even if pattern is empty

// Pattern storage
#define ARP_MAX_NOTES 32
uint8_t arpPattern[ARP_MAX_NOTES];
uint8_t arpLen = 0;
int16_t arpPos = -1;
int8_t arpDir = 1;
bool arpNoteActive = false;
uint8_t arpCurrentNote = 0;
uint8_t arpCurrentVel = 0;

// Expanded step list built from pattern + octave + insert settings
#define ARP_MAX_STEPS 128
uint8_t arpSteps[ARP_MAX_STEPS];
uint8_t arpStepCount = 0;

// Timing
uint32_t arpNextStepUs = 0;
uint32_t arpLastMidiTickUs = 0;
uint8_t arpMidiTickCount = 0;
bool arpMidiClockRunning = false;

void releaseVoice(byte note, int voiceIdx);
int getLowerSplitVoice(byte note);
int getUpperSplitVoice(byte note);
void assignVoice(byte note, byte velocity, int voiceIdx);
int getLowerSplitVoicePoly2(byte note);
int getUpperSplitVoicePoly2(byte note);
void commandMonoNoteOnLower(byte note, byte velocity);
void commandUnisonNoteOnLower(byte note, byte velocity);
void assignUnisonPairLower(byte note, byte velocity, bool octave);
void assignUnisonPairUpper(byte note, byte velocity, bool octave);
int getVoiceNo(int note);
int getVoiceNoPoly2(int note);
void commandMonoNoteOnUpper(byte note, byte velocity);
void commandMonoNoteOnLower(byte note, byte velocity);
void commandUnisonNoteOnUpper(byte note, byte velocity);
void commandUnisonNoteOnLower(byte note, byte velocity);
void commandMonoNoteOn(byte note, byte velocity);
void commandUnisonNoteOn(byte note, byte velocity);
void assignWholeUnisonPair(byte note, byte velocity, bool octave);

// MIDI clock ticks per beat = 24
// So ticks per step = 24 * rateNum / rateDen
inline uint32_t arpStepIntervalUs() {
  if (arp.bpm < 20) return 0;  // external clock handles its own timing
  uint32_t beatUs = 60000000UL / arp.bpm;
  return (beatUs * arpRateNum[arp.rate]) / arpRateDen[arp.rate];
}

inline uint8_t arpMidiTicksPerStep() {
  return (24 * arpRateNum[arp.rate]) / arpRateDen[arp.rate];
}

// Helper to get display string for current parameter value
String arpParamValueString(uint8_t param) {
  switch (param) {
    case ARP_P_BPM:
      return arp.bpm < 20 ? "EXTERNAL" : String(arp.bpm);
    case ARP_P_MODE:
      return arpModeNames[arp.mode];
    case ARP_P_OCTAVE:
      return String(arp.octave);
    case ARP_P_INSERT:
      return arpInsertNames[arp.insert];
    case ARP_P_RATE:
      return arpRateNames[arp.rate];
    case ARP_P_DURATION:
      return arp.duration == 16 ? "LEGATO" : String(arp.duration);
    case ARP_P_VELOCITY:
      return arp.velocity == 0 ? "PLAYED" : String(arp.velocity);
    case ARP_P_MIDI:
      return arpMidiNames[arp.midiRoute];
    default:
      return "";
  }
}

const char* arpParamNames[] = {
  "BPM", "MODE", "OCTAVE", "INSERT",
  "PLAY RATE", "DURATION", "VELOCITY", "MIDI"
};

// Default arp settings
void arpSetDefaults(ArpMemory &a) {
  a.bpm = 100;
  a.mode = ARP_MODE_UP;
  a.octave = 1;
  a.insert = ARP_INS_NONE;
  a.rate = 5;   // 1/4 note
  a.duration = 16;  // legato
  a.velocity = 0;   // played
  a.midiRoute = ARP_MIDI_KEYBOARD;
}

void arpSaveMemory(uint8_t slot) {
  if (slot > 7) return;
  EEPROM.put(ARP_EEPROM_BASE + (slot * sizeof(ArpMemory)), arpMemories[slot]);
}

void arpLoadMemory(uint8_t slot) {
  if (slot > 7) return;
  EEPROM.get(ARP_EEPROM_BASE + (slot * sizeof(ArpMemory)), arpMemories[slot]);
  // Sanity check - if data looks uninitialised, apply defaults
  if (arpMemories[slot].bpm > 250 || 
     (arpMemories[slot].bpm > 0 && arpMemories[slot].bpm < 20) ||
      arpMemories[slot].mode > 4 ||
      arpMemories[slot].octave < 1 || arpMemories[slot].octave > 4 ||
      arpMemories[slot].insert > 4 ||
      arpMemories[slot].rate >= ARP_RATE_COUNT ||
      arpMemories[slot].duration < 1 || arpMemories[slot].duration > 16 ||
      arpMemories[slot].velocity > 127 ||
      arpMemories[slot].midiRoute > 5) {
    arpSetDefaults(arpMemories[slot]);
  }
}

void arpLoadAllMemories() {
  for (uint8_t i = 0; i < 8; i++) {
    arpLoadMemory(i);
  }
}

void arpSaveAllMemories() {
  for (uint8_t i = 0; i < 8; i++) {
    arpSaveMemory(i);
  }
}

void arpIncrementParam(uint8_t param) {
  switch (param) {
    case ARP_P_BPM:
      if (arp.bpm < 20) arp.bpm = 20;
      else if (arp.bpm < 250) arp.bpm++;
      break;
    case ARP_P_MODE:
      if (arp.mode < 4) arp.mode++;
      break;
    case ARP_P_OCTAVE:
      if (arp.octave < 4) arp.octave++;
      break;
    case ARP_P_INSERT:
      if (arp.insert < 4) arp.insert++;
      break;
    case ARP_P_RATE:
      if (arp.rate < ARP_RATE_COUNT - 1) arp.rate++;
      break;
    case ARP_P_DURATION:
      if (arp.duration < 16) arp.duration++;
      break;
    case ARP_P_VELOCITY:
      if (arp.velocity < 127) arp.velocity++;
      break;
    case ARP_P_MIDI:
      if (arp.midiRoute < 5) arp.midiRoute++;
      break;
  }
}

void arpDecrementParam(uint8_t param) {
  switch (param) {
    case ARP_P_BPM:
      if (arp.bpm == 20) arp.bpm = 0;  // drop to external
      else if (arp.bpm > 20) arp.bpm--;
      break;
    case ARP_P_MODE:
      if (arp.mode > 0) arp.mode--;
      break;
    case ARP_P_OCTAVE:
      if (arp.octave > 1) arp.octave--;
      break;
    case ARP_P_INSERT:
      if (arp.insert > 0) arp.insert--;
      break;
    case ARP_P_RATE:
      if (arp.rate > 0) arp.rate--;
      break;
    case ARP_P_DURATION:
      if (arp.duration > 1) arp.duration--;
      break;
    case ARP_P_VELOCITY:
      if (arp.velocity > 0) arp.velocity--;
      break;
    case ARP_P_MIDI:
      if (arp.midiRoute > 0) arp.midiRoute--;
      break;
  }
}

void arpClearPattern() {
  arpLen = 0;
  arpPos = -1;
  arpDir = 1;
  arpStepCount = 0;
  arpInsertCounter = 0;
}

bool arpPatternContains(uint8_t note) {
  for (uint8_t i = 0; i < arpLen; i++) {
    if (arpPattern[i] == note) return true;
  }
  return false;
}

void arpInit() {
  arpLoadAllMemories();
  // Load memory slot 0 as default working copy
  arp = arpMemories[0];
  arpMemoryIndex = 0;
  arpParamIndex = 0;
  arpRunning = false;
  arpLatch = false;
  arpFunctionMode = false;
  arpLen = 0;
  arpPos = -1;
  arpDir = 1;
  arpNoteActive = false;
  arpNextStepUs = 0;
  arpMidiTickCount = 0;
  arpMidiClockRunning = false;
}

void arpStopCurrentNote() {
  if (!arpNoteActive) return;

  // Release across all voices that are playing arpCurrentNote
  for (int v = 0; v < 12; v++) {
    if (voices[v].noteOn && voices[v].note == arpCurrentNote) {
      releaseVoice((byte)arpCurrentNote, v);
    }
  }
  arpNoteActive = false;
}

void arpClearNonHeldNotes() {
  // Called when latch is turned off
  // Remove any notes from pattern that are not physically held
  uint8_t newLen = 0;
  for (uint8_t i = 0; i < arpLen; i++) {
    if (keyDownLower[arpPattern[i]]) {
      arpPattern[newLen++] = arpPattern[i];
    }
  }
  arpLen = newLen;
  if (arpLen == 0) {
    arpStopCurrentNote();
    arpRunning = false;
    mcp9.digitalWrite(SEQ_START_STOP_LED_RED, LOW);
  }
}

void arpAddNote(uint8_t note) {
  if (arpLatch && !arpKeysPhysicallyHeld) {
    // First new key after all released - clear old pattern
    arpClearPattern();
  }
  
  // Mark that keys are now held
  arpKeysPhysicallyHeld = true;
  
  // Add note if not already in pattern
  if (!arpPatternContains(note)) {
    if (arpLen < ARP_MAX_NOTES) {
      arpPattern[arpLen++] = note;
    }
  }
}

void arpRemoveNote(uint8_t note) {
  // Update physically held flag
  arpKeysPhysicallyHeld = false;
  for (int i = 0; i < 128; i++) {
    if (keyDownLower[i] && i != note) {
      arpKeysPhysicallyHeld = true;
      break;
    }
  }

  // Only remove from pattern if latch is off
  if (!arpLatch) {
    for (uint8_t i = 0; i < arpLen; i++) {
      if (arpPattern[i] == note) {
        for (uint8_t j = i; j + 1 < arpLen; j++) {
          arpPattern[j] = arpPattern[j + 1];
        }
        arpLen--;
        break;
      }
    }
  }
}

void arpBuildStepList() {
  arpStepCount = 0;
  if (arpLen == 0) return;

  // First sort a working copy of the pattern for HIGHEST/LOWEST insert
  uint8_t sorted[ARP_MAX_NOTES];
  memcpy(sorted, arpPattern, arpLen);
  // Simple bubble sort
  for (uint8_t i = 0; i < arpLen - 1; i++) {
    for (uint8_t j = 0; j < arpLen - i - 1; j++) {
      if (sorted[j] > sorted[j + 1]) {
        uint8_t tmp = sorted[j];
        sorted[j] = sorted[j + 1];
        sorted[j + 1] = tmp;
      }
    }
  }
  uint8_t lowestNote = sorted[0];
  uint8_t highestNote = sorted[arpLen - 1];

  // Build base sequence according to mode
  // We'll build one octave first then replicate
  uint8_t base[ARP_MAX_NOTES];
  uint8_t baseLen = 0;

  switch (arp.mode) {
    case ARP_MODE_UP:
      // Sort ascending
      memcpy(base, sorted, arpLen);
      baseLen = arpLen;
      break;

    case ARP_MODE_DOWN:
      // Sort descending
      for (uint8_t i = 0; i < arpLen; i++) {
        base[i] = sorted[arpLen - 1 - i];
      }
      baseLen = arpLen;
      break;

    case ARP_MODE_UPDOWN:
      // Ascending then descending, no repeat of top/bottom
      memcpy(base, sorted, arpLen);
      baseLen = arpLen;
      if (arpLen > 1) {
        for (int8_t i = arpLen - 2; i > 0; i--) {
          base[baseLen++] = sorted[i];
        }
      }
      break;

    case ARP_MODE_PLAYED:
      // Use pattern in order played
      memcpy(base, arpPattern, arpLen);
      baseLen = arpLen;
      break;

    case ARP_MODE_RANDOM:
      // Random - just use sorted, engine will pick randomly at step time
      memcpy(base, sorted, arpLen);
      baseLen = arpLen;
      break;
  }

  // Now expand across octave span and apply insert mode
  for (uint8_t oct = 0; oct < arp.octave; oct++) {
    for (uint8_t i = 0; i < baseLen; i++) {
      int16_t n = (int16_t)base[i] + (oct * 12);
      if (n > 127) n = 127;

      switch (arp.insert) {
        case ARP_INS_NONE:
          if (arpStepCount < ARP_MAX_STEPS)
            arpSteps[arpStepCount++] = (uint8_t)n;
          break;

        case ARP_INS_HIGHEST:
          if (arpStepCount < ARP_MAX_STEPS)
            arpSteps[arpStepCount++] = (uint8_t)n;
          if (arpStepCount < ARP_MAX_STEPS) {
            int16_t hn = (int16_t)highestNote + (oct * 12);
            if (hn > 127) hn = 127;
            arpSteps[arpStepCount++] = (uint8_t)hn;
          }
          break;

        case ARP_INS_LOWEST:
          if (arpStepCount < ARP_MAX_STEPS)
            arpSteps[arpStepCount++] = (uint8_t)n;
          if (arpStepCount < ARP_MAX_STEPS) {
            int16_t ln = (int16_t)lowestNote + (oct * 12);
            if (ln > 127) ln = 127;
            arpSteps[arpStepCount++] = (uint8_t)ln;
          }
          break;

        case ARP_INS_UP3DOWN1:
          // Every 4th step goes back one
          if (arpStepCount < ARP_MAX_STEPS)
            arpSteps[arpStepCount++] = (uint8_t)n;
          // The down step is handled at engine level by tracking position
          break;

        case ARP_INS_UP4DOWN2:
          if (arpStepCount < ARP_MAX_STEPS)
            arpSteps[arpStepCount++] = (uint8_t)n;
          break;
      }
    }
  }
}

int16_t arpNextStep() {
  if (arpStepCount == 0) return -1;

  switch (arp.mode) {
    case ARP_MODE_RANDOM:
      return (int16_t)random(arpStepCount);

    case ARP_MODE_UP:
    case ARP_MODE_PLAYED:
    case ARP_MODE_DOWN:
    case ARP_MODE_UPDOWN:
      {
        switch (arp.insert) {
          case ARP_INS_NONE:
            arpPos++;
            if (arpPos >= arpStepCount) arpPos = 0;
            return arpPos;

          case ARP_INS_HIGHEST:
          case ARP_INS_LOWEST:
            // Step list already has inserts baked in from arpBuildStepList
            arpPos++;
            if (arpPos >= arpStepCount) arpPos = 0;
            return arpPos;

          case ARP_INS_UP3DOWN1:
            {
              arpInsertCounter++;
              if (arpInsertCounter <= 2) {
                // Two steps forward
                arpPos++;
              } else {
                // One step back
                arpPos--;
                arpInsertCounter = 0;
              }
              // Clamp to valid range
              if (arpPos >= arpStepCount) arpPos = 0;
              if (arpPos < 0) arpPos = 0;
              return arpPos;
            }

          case ARP_INS_UP4DOWN2:
            {
              // Play full sequence up, back 2, up to top, repeat
              // Track phase: 0 = going up, 1 = going down, 2 = going up to top
              static uint8_t up4Phase = 0;
              static int16_t up4PeakPos = 0;

              switch (up4Phase) {
                case 0:  // going up
                  arpPos++;
                  if (arpPos >= arpStepCount) {
                    arpPos = arpStepCount - 1;
                    up4PeakPos = arpPos;
                    up4Phase = 1;
                    arpInsertCounter = 0;
                  }
                  break;
                case 1:  // going down 2
                  arpPos--;
                  arpInsertCounter++;
                  if (arpInsertCounter >= 2) {
                    up4Phase = 2;
                  }
                  if (arpPos < 0) arpPos = 0;
                  break;
                case 2:  // going back up to top
                  arpPos++;
                  if (arpPos >= arpStepCount) {
                    arpPos = 0;
                    up4Phase = 0;
                    arpInsertCounter = 0;
                  }
                  break;
              }
              return arpPos;
            }
        }
      }
      break;
  }
  return 0;
}

void arpPlayCurrentStep() {
  if (arpStepCount == 0) return;
  if (arpPos < 0 || arpPos >= arpStepCount) arpPos = 0;

  uint8_t note = arpSteps[arpPos];
  uint8_t vel = (arp.velocity == 0) ? arpCurrentVel : arp.velocity;

  // Route note through voice allocator based on keyMode
  // Arp always plays on lower section
  switch (keyMode) {
    case 0:  // DUAL - both boards
      {
        switch (upperAssign) {
          case 0:
            {
              int v = getLowerSplitVoice(note);
              assignVoice(note, vel, v);
              voiceAssignmentLower[note] = v;
              voiceToNoteLower[v] = note;
              v = getUpperSplitVoice(note);
              assignVoice(note, vel, v);
              voiceAssignmentUpper[note] = v;
              voiceToNoteUpper[v - 6] = note;
            }
            break;
          case 1:
            {
              int v = getLowerSplitVoicePoly2(note);
              int old = voiceToNoteLower[v];
              if (old >= 0) { releaseVoice(old, v); voiceAssignmentLower[old] = -1; }
              assignVoice(note, vel, v);
              voiceAssignmentLower[note] = v;
              voiceToNoteLower[v] = note;
              v = getUpperSplitVoicePoly2(note);
              old = voiceToNoteUpper[v - 6];
              if (old >= 0) { releaseVoice(old, v); voiceAssignmentUpper[old] = -1; }
              assignVoice(note, vel, v);
              voiceAssignmentUpper[note] = v;
              voiceToNoteUpper[v - 6] = note;
            }
            break;
          case 2: commandMonoNoteOnLower(note, vel); commandMonoNoteOnUpper(note, vel); break;
          case 3: commandUnisonNoteOnLower(note, vel); commandUnisonNoteOnUpper(note, vel); break;
          case 4: assignUnisonPairLower(note, vel, false); assignUnisonPairUpper(note, vel, false); break;
          case 5: assignUnisonPairLower(note, vel, true); assignUnisonPairUpper(note, vel, true); break;
        }
      }
      break;

    case 1:  // SINGLE LOWER - all 12 voices
    case 2:  // SINGLE UPPER - all 12 voices
      {
        byte assignData = (keyMode == 1) ? lowerAssign : upperAssign;
        switch (assignData) {
          case 0: { int v = getVoiceNo(-1) - 1; assignVoice(note, vel, v); } break;
          case 1: { int v = getVoiceNoPoly2(-1) - 1; assignVoice(note, vel, v); } break;
          case 2: commandMonoNoteOn(note, vel); break;
          case 3: commandUnisonNoteOn(note, vel); break;
          case 4: assignWholeUnisonPair(note, vel, false); break;
          case 5: assignWholeUnisonPair(note, vel, true); break;
        }
      }
      break;

    case 3:  // SPLIT - lower section only
      {
        switch (lowerAssign) {
          case 0:
            {
              int v = getLowerSplitVoice(note);
              assignVoice(note, vel, v);
              voiceAssignmentLower[note] = v;
              voiceToNoteLower[v] = note;
            }
            break;
          case 1:
            {
              int v = getLowerSplitVoicePoly2(note);
              int old = voiceToNoteLower[v];
              if (old >= 0) { releaseVoice(old, v); voiceAssignmentLower[old] = -1; }
              assignVoice(note, vel, v);
              voiceAssignmentLower[note] = v;
              voiceToNoteLower[v] = note;
            }
            break;
          case 2: commandMonoNoteOnLower(note, vel); break;
          case 3: commandUnisonNoteOnLower(note, vel); break;
          case 4: assignUnisonPairLower(note, vel, false); break;
          case 5: assignUnisonPairLower(note, vel, true); break;
        }
      }
      break;

    case 4:  // T.VOICE
    case 5:  // X-FADE - use lower board for arp
      {
        switch (upperAssign) {
          case 0:
            {
              int v = getLowerSplitVoice(note);
              assignVoice(note, vel, v);
              voiceAssignmentLower[note] = v;
              voiceToNoteLower[v] = note;
            }
            break;
          case 1:
            {
              int v = getLowerSplitVoicePoly2(note);
              int old = voiceToNoteLower[v];
              if (old >= 0) { releaseVoice(old, v); voiceAssignmentLower[old] = -1; }
              assignVoice(note, vel, v);
              voiceAssignmentLower[note] = v;
              voiceToNoteLower[v] = note;
            }
            break;
          case 2: commandMonoNoteOnLower(note, vel); break;
          case 3: commandUnisonNoteOnLower(note, vel); break;
          case 4: assignUnisonPairLower(note, vel, false); break;
          case 5: assignUnisonPairLower(note, vel, true); break;
        }
      }
      break;
  }

  arpCurrentNote = note;
  arpCurrentVel = vel;
  arpNoteActive = true;
}

void arpLedPulse() {
  // Flash LED on each step
  mcp9.digitalWrite(SEQ_START_STOP_LED_RED, HIGH);
  arpLedOffTimeMs = millis() + 30;  // 30ms pulse
  arpLedOn = true;
}

bool arpShouldStep() {
  if (arp.bpm < 20) {
    // External MIDI clock
    if (arpMidiTickCount >= arpMidiTicksPerStep()) {
      arpMidiTickCount = 0;
      return true;
    }
    return false;
  }

  // Internal clock
  uint32_t now = micros();
  if (arpNextStepUs == 0) {
    arpNextStepUs = now;
    return true;
  }
  if ((int32_t)(now - arpNextStepUs) < 0) return false;
  arpNextStepUs += arpStepIntervalUs();
  // Resync if fallen behind
  if ((int32_t)(micros() - arpNextStepUs) > (int32_t)arpStepIntervalUs()) {
    arpNextStepUs = micros() + arpStepIntervalUs();
  }
  return true;
}

void arpServiceLed() {
  if (arpLedOn && (int32_t)(millis() - arpLedOffTimeMs) >= 0) {
    mcp9.digitalWrite(SEQ_START_STOP_LED_RED, LOW);
    arpLedOn = false;
  }
}

void arpOnMidiClockTick() {
  if (!arpMidiClockRunning) return;
  arpMidiTickCount++;
}

void arpOnMidiStart() {
  arpMidiClockRunning = true;
  arpMidiTickCount = 0;
  arpPos = -1;
  arpDir = 1;
  if (arpNoteActive) arpStopCurrentNote();
}

void arpOnMidiStop() {
  arpMidiClockRunning = false;
  arpMidiTickCount = 0;
  if (arpNoteActive) arpStopCurrentNote();
}

void arpOnMidiContinue() {
  arpMidiClockRunning = true;
}

void arpEngine() {
  if (!arpRunning || arpLen == 0) {
    if (arpNoteActive) arpStopCurrentNote();
    return;
  }

  // Handle duration/gate - when to stop current note
  if (arpNoteActive && arp.duration < 16) {
    // duration 1-15: note off after fraction of step interval
    uint32_t stepUs = arpStepIntervalUs();
    uint32_t gateUs = (stepUs * arp.duration) / 16;
    if ((int32_t)(micros() - arpNoteOnTimeUs) >= (int32_t)gateUs) {
      arpStopCurrentNote();
    }
  }

  // Check if it's time for next step
  if (!arpShouldStep()) return;

  // Stop current note if still sounding (legato handles this differently)
  if (arpNoteActive && arp.duration == 16) {
    arpStopCurrentNote();  // legato: stop just before next note
  }

  // Rebuild step list in case pattern changed
  arpBuildStepList();
  if (arpStepCount == 0) return;

  // Advance to next step
  int16_t nextPos = arpNextStep();
  if (nextPos < 0) return;
  arpPos = nextPos;

  // Play the note
  arpPlayCurrentStep();

  // Record note on time for gate/duration
  arpNoteOnTimeUs = micros();

  // Pulse LED on each step
  arpLedPulse();
}

