#ifndef CHASEPLAY_H
#define CHASEPLAY_H

// Forward declarations for off-side / pair-release functions used below.
// (Their on-side counterparts are already declared in Arp.h.)
void commandMonoNoteOffUpper(byte note);
void commandUnisonNoteOffUpper(byte note);
void commandMonoNoteOffLower(byte note);
void commandUnisonNoteOffLower(byte note);
void releaseUnisonPairLower(byte note);
void releaseUnisonPairUpper(byte note);

// ============================================================================
// Chase Play  (JX-10)
// ----------------------------------------------------------------------------
// Only active when keyMode == 0 (DUAL) and the global `chasePlay` is on.
//
//   Mode 0  "U-L"    : Upper at t=0, Lower at t=Time
//   Mode 1  "U-L-U"  : Upper at t=0, Lower at t=Time, Upper at t=2*Time
//   Mode 2  "U-L-L"  : Upper at t=0, Lower at t=Time, Lower at t=2*Time
//
// chaseTime  : 0..127  underlying value (GUI shows 0..99); maps to 10ms..3s
//              via chaseTimeToMs() — currently linear, easily replaced
// chaseLevel : 0..127  underlying value (GUI shows 0..99); attenuates VCA of
//              delayed Lower hits, passed straight to the Lower board
// chasePlay  : global on/off (int)
// chaseMode  : 0/1/2 as above
//
// Behaviour:
//   * Each chase event is dispatched through the SAME assign-mode dispatcher
//     used by myNoteOn (Poly1 / Poly2 / Mono / Unison / Pair / PairOctave) so
//     the chase respects the patch's current Upper/Lower assign settings.
//   * If the user releases the key before the sequence finishes, any pending
//     ON events still fire (per spec). Each pending ON gets a tail OFF
//     scheduled 50 ms after it fires.
//   * If a chased event has already fired by the time the key is released,
//     it's released immediately like a normal note.
// ============================================================================

// ---- Tunables ---------------------------------------------------------------
// chaseTime maps to a delay in ms via chaseTimeToMs() below.
// Spec: 10 ms (min) to 3000 ms (max) across the underlying 0..127 value.
// Currently a linear mapping with a 10 ms floor; swap the body of
// chaseTimeToMs() for an exponential curve later if a hardware capture
// suggests one.
#ifndef CHASE_TIME_MIN_MS
#define CHASE_TIME_MIN_MS         10
#endif
#ifndef CHASE_TIME_MAX_MS
#define CHASE_TIME_MAX_MS       3000
#endif

static inline uint32_t chaseTimeToMs(uint8_t t) {
  // Linear: t=0 -> ~min, t=127 -> ~max
  uint32_t ms = ((uint32_t)t * (uint32_t)CHASE_TIME_MAX_MS) / 127u;
  if (ms < (uint32_t)CHASE_TIME_MIN_MS) ms = (uint32_t)CHASE_TIME_MIN_MS;
  return ms;
}

#ifndef CHASE_EARLY_RELEASE_MS
#define CHASE_EARLY_RELEASE_MS   50   // tail when key released before chased ON fires
#endif
#ifndef CHASE_QUEUE_SIZE
#define CHASE_QUEUE_SIZE         24
#endif

enum ChaseKind : uint8_t {
  CHASE_NONE      = 0,
  CHASE_UPPER_ON  = 1,
  CHASE_LOWER_ON  = 2,
  CHASE_UPPER_OFF = 3,
  CHASE_LOWER_OFF = 4
};

struct ChaseEvent {
  bool      active;
  uint32_t  fireAtMs;
  uint8_t   note;
  uint8_t   velocity;
  ChaseKind kind;
};

static ChaseEvent chaseQueue[CHASE_QUEUE_SIZE];



// ---- Queue helpers ----------------------------------------------------------
// ---- Stub: send chase Level to the just-assigned Lower voice ----------------
// `chaseLevel` is the raw underlying value (0..127). The GUI shows 0..99 but
// the stored/used value is on the native 0..127 scale, so pass it straight
// through to whatever VCA-level send your Lower board expects.
// TODO: replace body with the correct per-voice VCA-level send for your rig.
static inline void applyChaseLevelToLowerVoice(int voiceIdx, uint8_t level) {
  (void)voiceIdx; (void)level;
  // e.g. Serial3.write(kBoardLowerPrefix); Serial3.write(kChaseLevelParam); Serial3.write(level);
}

static inline int chaseFindFreeSlot() {
  for (int i = 0; i < CHASE_QUEUE_SIZE; i++) if (!chaseQueue[i].active) return i;
  return -1;
}

static inline void chasePush(uint32_t fireAtMs, uint8_t note, uint8_t vel, ChaseKind k) {
  int s = chaseFindFreeSlot();
  if (s < 0) return;
  chaseQueue[s].active   = true;
  chaseQueue[s].fireAtMs = fireAtMs;
  chaseQueue[s].note     = note;
  chaseQueue[s].velocity = vel;
  chaseQueue[s].kind     = k;
}

// ---- Assign-mode-aware fire helpers (mirror DUAL branch of myNoteOn) --------

static inline void chaseFireUpperOn(uint8_t note, uint8_t vel) {
  // If this note already has an Upper voice (re-trigger from U-L-U), release it first.
  int prev = voiceAssignmentUpper[note];
  if (prev >= 6 && prev <= 11 && voiceToNoteUpper[prev - 6] == note) {
    releaseVoice(note, prev);
    voiceAssignmentUpper[note] = -1;
    voiceToNoteUpper[prev - 6] = -1;
  }

  switch (upperAssign) {
    case 0: {
      int v = getUpperSplitVoice(note);
      assignVoice(note, vel, v);
      voiceAssignmentUpper[note] = v;
      voiceToNoteUpper[v - 6] = note;
    } break;
    case 1: {
      int v = getUpperSplitVoicePoly2(note);
      int old = voiceToNoteUpper[v - 6];
      if (old >= 0) { releaseVoice(old, v); voiceAssignmentUpper[old] = -1; }
      assignVoice(note, vel, v);
      voiceAssignmentUpper[note] = v;
      voiceToNoteUpper[v - 6] = note;
    } break;
    case 2: commandMonoNoteOnUpper(note, vel);          break;
    case 3: commandUnisonNoteOnUpper(note, vel);        break;
    case 4: assignUnisonPairUpper(note, vel, false);    break;
    case 5: assignUnisonPairUpper(note, vel, true);     break;
  }
}

static inline void chaseFireLowerOn(uint8_t note, uint8_t vel) {
  // If this note already has a Lower voice (re-trigger from U-L-L), release it first.
  int prev = voiceAssignmentLower[note];
  if (prev >= 0 && prev <= 5 && voiceToNoteLower[prev] == note) {
    releaseVoice(note, prev);
    voiceAssignmentLower[note] = -1;
    voiceToNoteLower[prev] = -1;
  }

  int trackedVoice = -1;
  switch (lowerAssign) {
    case 0: {
      int v = getLowerSplitVoice(note);
      assignVoice(note, vel, v);
      voiceAssignmentLower[note] = v;
      voiceToNoteLower[v] = note;
      trackedVoice = v;
    } break;
    case 1: {
      int v = getLowerSplitVoicePoly2(note);
      int old = voiceToNoteLower[v];
      if (old >= 0) { releaseVoice(old, v); voiceAssignmentLower[old] = -1; }
      assignVoice(note, vel, v);
      voiceAssignmentLower[note] = v;
      voiceToNoteLower[v] = note;
      trackedVoice = v;
    } break;
    case 2: commandMonoNoteOnLower(note, vel);          break;
    case 3: commandUnisonNoteOnLower(note, vel);        break;
    case 4: assignUnisonPairLower(note, vel, false);    break;
    case 5: assignUnisonPairLower(note, vel, true);     break;
  }
  if (trackedVoice >= 0) {
    // chaseLevel is on the native 0..127 scale (GUI shows 0..99)
    applyChaseLevelToLowerVoice(trackedVoice, (uint8_t)chaseLevel);
  }
}

static inline void chaseFireUpperOff(uint8_t note) {
  if (upperAssign == 2) { commandMonoNoteOffUpper(note); return; }
  if (upperAssign == 3) { commandUnisonNoteOffUpper(note); return; }
  if (upperAssign == 4 || upperAssign == 5) { releaseUnisonPairUpper(note); return; }
  int v = voiceAssignmentUpper[note];
  if (v >= 6 && v <= 11 && voiceToNoteUpper[v - 6] == note) {
    releaseVoice(note, v);
    voiceAssignmentUpper[note] = -1;
    voiceToNoteUpper[v - 6] = -1;
  }
}

static inline void chaseFireLowerOff(uint8_t note) {
  if (lowerAssign == 2) { commandMonoNoteOffLower(note); return; }
  if (lowerAssign == 3) { commandUnisonNoteOffLower(note); return; }
  if (lowerAssign == 4 || lowerAssign == 5) { releaseUnisonPairLower(note); return; }
  int v = voiceAssignmentLower[note];
  if (v >= 0 && v <= 5 && voiceToNoteLower[v] == note) {
    releaseVoice(note, v);
    voiceAssignmentLower[note] = -1;
    voiceToNoteLower[v] = -1;
  }
}

// ---- Public API -------------------------------------------------------------

// Returns true if chase took ownership of this noteOn.
// Call from myNoteOn AFTER the arp-consume check, BEFORE the keyMode switch.
inline bool chasePlayNoteOn(uint8_t note, uint8_t vel) {
  if (keyMode != 0) return false;
  if (!chasePlay)   return false;

  // Drop any still-pending events from a previous chase for this same note —
  // they'd corrupt voice tracking for the new press.
  for (int i = 0; i < CHASE_QUEUE_SIZE; i++) {
    if (chaseQueue[i].active && chaseQueue[i].note == note) {
      chaseQueue[i].active = false;
    }
  }

  // Upper fires immediately
  chaseFireUpperOn(note, vel);

  uint32_t step = chaseTimeToMs((uint8_t)chaseTime);
  uint32_t now  = millis();

  // Lower at t = step
  chasePush(now + step, note, vel, CHASE_LOWER_ON);

  // Third hit at t = 2*step (modes 1, 2)
  if (chaseMode == 1) {
    chasePush(now + 2 * step, note, vel, CHASE_LOWER_ON);   // U-L-L
  } else if (chaseMode == 2) {
    chasePush(now + 2 * step, note, vel, CHASE_UPPER_ON);   // U-L-U
  }
  return true;
}

// Returns true if chase handled the release.
// Call from myNoteOff AFTER arp/hold-latch handling, BEFORE the keyMode switch.
inline bool chasePlayNoteOff(uint8_t note) {
  if (keyMode != 0) return false;
  if (!chasePlay)   return false;

  // Upper was triggered at t=0 — release now.
  chaseFireUpperOff(note);

  bool lowerStillPending = false;

  // For each pending ON event for this note, schedule a tail OFF after it.
  for (int i = 0; i < CHASE_QUEUE_SIZE; i++) {
    if (!chaseQueue[i].active)         continue;
    if (chaseQueue[i].note != note)    continue;

    if (chaseQueue[i].kind == CHASE_LOWER_ON) {
      lowerStillPending = true;
      chasePush(chaseQueue[i].fireAtMs + CHASE_EARLY_RELEASE_MS,
                note, 0, CHASE_LOWER_OFF);
    } else if (chaseQueue[i].kind == CHASE_UPPER_ON) {
      chasePush(chaseQueue[i].fireAtMs + CHASE_EARLY_RELEASE_MS,
                note, 0, CHASE_UPPER_OFF);
    }
  }

  // If Lower has already fired (no pending ON), release it immediately.
  if (!lowerStillPending) {
    chaseFireLowerOff(note);
  }
  return true;
}

// Service the queue. Call every loop().
inline void chaseService() {
  uint32_t now = millis();
  for (int i = 0; i < CHASE_QUEUE_SIZE; i++) {
    if (!chaseQueue[i].active) continue;
    if ((int32_t)(now - chaseQueue[i].fireAtMs) < 0) continue;

    uint8_t   n = chaseQueue[i].note;
    uint8_t   v = chaseQueue[i].velocity;
    ChaseKind k = chaseQueue[i].kind;
    chaseQueue[i].active = false;

    switch (k) {
      case CHASE_LOWER_ON:  chaseFireLowerOn(n, v);  break;
      case CHASE_UPPER_ON:  chaseFireUpperOn(n, v);  break;
      case CHASE_LOWER_OFF: chaseFireLowerOff(n);    break;
      case CHASE_UPPER_OFF: chaseFireUpperOff(n);    break;
      default: break;
    }
  }
}

inline void chaseFlushAll() {
  for (int i = 0; i < CHASE_QUEUE_SIZE; i++) chaseQueue[i].active = false;
}

inline void chaseInit() { chaseFlushAll(); }

#endif // CHASEPLAY_H
