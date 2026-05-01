#pragma once

// ---------------------------------------------------------------------------
// Roland MKS-70 / JX-10 Bulk Dump SysEx Handler
//
// Receives a bulk dump consisting of:
//   64 x patch messages (level 0x30, 106 bytes, nibble encoded)
//   50 x tone messages  (level 0x20, 69 bytes, plain bytes)
//
// All messages are buffered in EXTMEM (PSRAM) during reception.
// Only after the complete dump is received are files written to SD.
// On completion: reloads tone library and recalls first patch.
// ---------------------------------------------------------------------------

uint8_t patchBuffer[64][106];
uint8_t toneBuffer[50][69];
EXTMEM uint8_t dumpRawBuffer[11000];

uint16_t dumpRawLen = 0;

// Forward declarations
void startParameterDisplay();
void loadToneLibrary(uint8_t bank);
void loadToneToSlotData(uint8_t toneIndex, bool upper);
void loadInitialPatch();
void savePatch(const char *path, String data);
void showCurrentParameterPage(const char *param, String value);

// --- SysEx reception state ---

static uint8_t patchCount = 0;
static uint8_t toneCount = 0;
static bool dumpInProgress = false;
bool sysexComplete = false;
bool receivingSysEx = false;  // Flag to indicate if a SysEx message is in progress
uint16_t byteIndex = 0;
uint8_t currentBlock = 0;

// ---------------------------------------------------------------------------
// Decode all buffered patches and write to SD
// ---------------------------------------------------------------------------

// Key mode conversion table (Roland -> Vecoven)
static const uint8_t key_mode_conv[] = { 3, 2, 0, 1, 5, 4 };

// Key assign conversion table (Roland -> Vecoven)
static const uint8_t key_assign_conv[] = { 0, 2, 4, 0, 1, 3, 5 };

// Unpack using clrb/rola/rolb/lsra pattern (fresh carry each time)
static void rolaRolb_fresh(uint8_t in, uint8_t &a_out, uint8_t &b_out) {
  uint8_t a = in, b = 0, carry = 0;
  uint8_t nc = (a >> 7) & 1;
  a = ((a << 1) | carry) & 0xFF;
  carry = nc;
  nc = (b >> 7) & 1;
  b = ((b << 1) | carry) & 0xFF;
  a >>= 1;
  a_out = a;
  b_out = b;
}

// Unpack two chained bytes using rola/rolb/lsra pattern
static void rolaRolb_pair(uint8_t in1, uint8_t in2,
                          uint8_t &a1, uint8_t &b_mid, uint8_t &a2) {
  uint8_t a = in1, b = 0, carry = 0, nc;
  nc = (a >> 7) & 1;
  a = ((a << 1) | carry) & 0xFF;
  carry = nc;
  nc = (b >> 7) & 1;
  b = ((b << 1) | carry) & 0xFF;
  carry = nc;
  a1 = a >> 1;
  a = in2;
  nc = (a >> 7) & 1;
  a = ((a << 1) | carry) & 0xFF;
  carry = nc;
  nc = (b >> 7) & 1;
  b = ((b << 1) | carry) & 0xFF;
  carry = nc;
  b_mid = b;
  a2 = a >> 1;
}

// Unpack two chained bytes using rola/rorb pattern
static void rolaRorb_pair(uint8_t in1, uint8_t in2,
                          uint8_t &a1, uint8_t &b2, uint8_t &a2) {
  uint8_t a = in1, b = 0, carry = 0, nc;
  nc = (a >> 7) & 1;
  a = ((a << 1) | carry) & 0xFF;
  carry = nc;
  b = ((carry << 7) | (b >> 1)) & 0xFF;
  a1 = a >> 1;
  a = in2;
  nc = (a >> 7) & 1;
  a = ((a << 1) | carry) & 0xFF;
  carry = nc;
  b = ((carry << 7) | (b >> 1)) & 0xFF;
  b >>= 1;
  b2 = b;
  a2 = a >> 1;
}

void writeAllPatches() {
  for (uint8_t idx = 0; idx < 64; idx++) {
    uint8_t *src = patchBuffer[idx];

    // Decode 48 bytes from nibble pairs starting at byte 9
    uint8_t dec[48];
    int ni = 9;
    for (int i = 0; i < 48; i++) {
      uint8_t hi = src[ni++] & 0x0F;
      uint8_t lo = src[ni++] & 0x0F;
      dec[i] = (hi << 4) | lo;
    }

    // Patch name (dec[0-17])
    patchName = "";
    for (int i = 0; i < 18; i++) {
      uint8_t b = dec[i];
      if (b >= 0x20 && b < 0x7F) patchName += (char)b;
      else if (b >= 0x01 && b <= 0x09) patchName += (char)('0' + b);
      else patchName += ' ';
    }

    // x[1]=dec[18], x[2]=dec[19] ... matching assembly offset convention
    uint8_t *x = &dec[17];

    // Temporaries for intermediate values
    uint8_t b;
    uint8_t bend_range_R, key_mode_R, key_mode_hi_R;
    uint8_t A_shift_R, B_shift_R, A_key_assign_R, B_key_assign_R;
    uint8_t bend_range_hi_R;
    uint8_t t1, t2, t3;

    // AB_bal + bend_range_R + after_vol
    rolaRorb_pair(x[1], x[9], t1, bend_range_R, t2);
    balance = t1;
    at_vol = t2;

    // dual_detune: plain copy
    dualdetune = x[2];

    // upper_split + key_mode_R + lower_split
    rolaRolb_pair(x[3], x[4], t1, key_mode_R, t2);
    upperSplitPoint = t1;
    lowerSplitPoint = t2;

    // porta_time + total_vol
    rolaRolb_pair(x[5], x[6], t1, b, t2);
    portamento = t1;
    volume = t2;

    // after_vib + after_bri + key_mode_hi_R
    rolaRolb_pair(x[7], x[8], t1, key_mode_hi_R, t2);
    at_vib = t1;
    at_bri = t2;

    // A_tone + A_hold_sw
    rolaRolb_fresh(x[0xA], t1, t2);
    upperToneNumber = t1 + 1;
    upperHold = t2;

    // A_shift_R: plain
    A_shift_R = x[0xB];

    // A_detune: plain
    upperUnisonDetune = x[0xC];

    // A_lfo + A_porta_sw
    rolaRolb_fresh(x[0xD], t1, t2);
    upperLFOModDepth = t1;
    upperPortamento_SW = t2;

    // A_bend_sw
    upperbend_enable_SW = x[0xE] & 1;

    // B_tone + B_hold_sw
    rolaRolb_fresh(x[0xF], t1, t2);
    lowerToneNumber = t1 + 1;
    lowerHold = t2;

    // B_shift_R: plain
    B_shift_R = x[0x10];

    // B_detune: plain
    lowerUnisonDetune = x[0x11];

    // B_lfo + B_porta_sw
    rolaRolb_fresh(x[0x12], t1, t2);
    lowerLFOModDepth = t1;
    lowerPortamento_SW = t2;

    // B_bend_sw
    lowerbend_enable_SW = x[0x13] & 1;

    // chase_level + chase_mode + chase_time
    rolaRolb_pair(x[0x14], x[0x15], t1, t2, t3);
    chaseLevel = t1;
    chaseMode = t2;
    chaseTime = t3;

    // A_key_assign_R: tab/clra/lsld x3
    {
      uint8_t bb = x[0x16], aa = 0;
      for (int i = 0; i < 3; i++) {
        uint16_t d = ((uint16_t)aa << 8) | bb;
        d <<= 1;
        aa = (d >> 8) & 0xFF;
        bb = d & 0xFF;
      }
      A_key_assign_R = aa;
    }

    // B_key_assign_R: tab/clra/lsld x3
    {
      uint8_t bb = x[0x17], aa = 0;
      for (int i = 0; i < 3; i++) {
        uint16_t d = ((uint16_t)aa << 8) | bb;
        d <<= 1;
        aa = (d >> 8) & 0xFF;
        bb = d & 0xFF;
      }
      B_key_assign_R = aa;
    }

    // chase_sw (chasePlay)
    rolaRolb_fresh(x[0x1C], t1, t2);
    midiSplitPoint = t1;   // 0-127 key code, JX-10 split point
    chasePlay = t2;

    // bend_range_hi_R
    bend_range_hi_R = x[0x1E] & 1;

    // --- Adapt parameters ---

    bend_range = bend_range_hi_R ? 0x40 : (bend_range_R >> 1);
    // Convert bend_range to 0-4 steps (2,3,4,7,12 semitones)
    switch (bend_range) {
      case 0: bend_range = 0; break;   // 2 semitones
      case 16: bend_range = 1; break;  // 3 semitones
      case 32: bend_range = 2; break;  // 4 semitones
      case 48: bend_range = 3; break;  // 7 semitones
      case 64: bend_range = 4; break;  // 12 semitones
      default: bend_range = 0; break;
    }

    // key_mode
    {
      uint8_t km_idx = key_mode_hi_R ? (3 + key_mode_hi_R) : key_mode_R;
      keyMode = (km_idx < 6) ? key_mode_conv[km_idx] : 0;
    }

    switch (keyMode) {
      case 0: keyMode = 2; break;
      case 1: keyMode = 1; break;
      case 2: keyMode = 3; break;
      case 3: keyMode = 0; break;
      case 4: keyMode = 5; break;
      case 5: keyMode = 4; break;
      default: keyMode = 0; break;
    }

    // chromatic shift: Roland E8..00..18 -> Vecoven 00..30

    upperChromatic = A_shift_R & 0x7F;
    lowerChromatic = B_shift_R & 0x7F;

    // key_assign
    upperAssign = (A_key_assign_R < 7) ? key_assign_conv[A_key_assign_R] : 0;
    lowerAssign = (B_key_assign_R < 7) ? key_assign_conv[B_key_assign_R] : 0;

    switch (upperAssign) {
      case 0: upperAssign = 0; break;
      case 1: upperAssign = 1; break;
      case 2: upperAssign = 4; break;
      case 3: upperAssign = 5; break;
      case 4: upperAssign = 2; break;
      case 5: upperAssign = 3; break;
      default: upperAssign = 0; break;
    }

    switch (lowerAssign) {
      case 0: lowerAssign = 0; break;
      case 1: lowerAssign = 1; break;
      case 2: lowerAssign = 4; break;
      case 3: lowerAssign = 5; break;
      case 4: lowerAssign = 2; break;
      case 5: lowerAssign = 3; break;
      default: lowerAssign = 0; break;
    }

    // --- Load tones ---
    loadToneToSlotData(upperToneNumber - 1, true);
    loadToneToSlotData(lowerToneNumber - 1, false);

    // --- Set group and slot from patch number ---
    uint8_t patchNum = src[8];
    currentGroup = patchNum / 8;
    currentSlot = (patchNum % 8) + 1;

    // --- Save to SD ---
    saveCurrentPatch();

    Serial.print(F("Patch written: "));
    Serial.print((char)('A' + currentGroup));
    Serial.print(currentSlot);
    Serial.print(F(" "));
    Serial.println(patchName);
  }
}

// ---------------------------------------------------------------------------
// Decode all buffered tones and write programmable.jsonl
// ---------------------------------------------------------------------------

void writeAllTones() {
  String tonePath = "/bank" + String(currentBank) + "/programmable.jsonl";
  SD.remove(tonePath.c_str());
  File f = SD.open(tonePath.c_str(), FILE_WRITE);
  if (!f) {
    Serial.println(F("Failed to open programmable.jsonl for writing"));
    return;
  }

  for (uint8_t idx = 0; idx < toneCount; idx++) {
    const uint8_t *msg = toneBuffer[idx];

    uint8_t toneNum = msg[8] + 1;  // 0-based → 1-based

    // Decode name - 10 chars plain ASCII starting at byte 9
    char name[11];
    for (uint8_t i = 0; i < 10; i++) {
      uint8_t b = msg[9 + i];
      if (b >= 0x20 && b < 0x7F) name[i] = (char)b;
      else if (b >= 0x01 && b <= 0x09) name[i] = '0' + b;
      else name[i] = ' ';
    }
    name[10] = '\0';
    for (int i = 9; i >= 0 && name[i] == ' '; i--) name[i] = '\0';

    // 49 param bytes starting at byte 19
    String line = "{\"i\":" + String(toneNum) + ",\"n\":\"" + String(name) + "\",\"p\":[";
    for (uint8_t i = 0; i < 49; i++) {
      line += String(msg[19 + i]);
      if (i < 48) line += ",";
    }
    line += "]}";
    f.println(line);
  }

  f.close();
  Serial.println(F("programmable.jsonl written"));
}

// ---------------------------------------------------------------------------
// Called when both patch and tone buffers are complete
// ---------------------------------------------------------------------------

void processDumpComplete() {
  displayMode = 5;
  showCurrentParameterPage("SYSEX", "Processing...");
  startParameterDisplay();
  writeAllTones();
  loadToneLibrary(currentBank);  // reload from newly written programmable.jsonl
  writeAllPatches();

  loadInitialPatch();

  dumpInProgress = false;
  receivingSysEx = false;
  patchCount = 0;
  toneCount = 0;

  displayMode = 5;
  showCurrentParameterPage("SYSEX", "Done!");
  startParameterDisplay();
  Serial.println(F("Bulk dump complete"));

}

void decodeAndStoreDump() {
  Serial.println(F("Decoding dump..."));
  Serial.print(F("Patches received: "));
  Serial.println(currentBlock >= 64 ? 64 : currentBlock);
  Serial.print(F("Tones received: "));
  Serial.println(currentBlock >= 114 ? 50 : currentBlock - 64);

  patchCount = 64;
  toneCount = 50;

  processDumpComplete();
}

// ---------------------------------------------------------------------------
// Main SysEx byte handler - replaces old handleSysexByte
// ---------------------------------------------------------------------------

void handleSysexByte(byte *data, unsigned length) {
  if (!receivingSysEx) {
    receivingSysEx = true;
    displayMode = 5;
    showCurrentParameterPage("SysEx", "Receiving");
    startParameterDisplay();
  }

  for (unsigned i = 0; i < length; i++) {
    if (currentBlock < 64) {
      // Filling patch buffer
      patchBuffer[currentBlock][byteIndex] = data[i];
      byteIndex++;
      if (byteIndex >= 106) {
        byteIndex = 0;
        showCurrentParameterPage("Patch", (currentBlock + 1));
        startParameterDisplay();
        currentBlock++;
      }
    } else if (currentBlock < 114) {
      // Filling tone buffer
      toneBuffer[currentBlock - 64][byteIndex] = data[i];

      byteIndex++;
      if (byteIndex >= 69) {
        byteIndex = 0;
        showCurrentParameterPage("Tone", (currentBlock -63));
        startParameterDisplay();
        currentBlock++;
      }
    }
  }

  //Serial.print(F("Current Block ")); Serial.println(currentBlock);

  if (currentBlock >= 114) {
    sysexComplete = true;
    receivingSysEx = false;
    Serial.println(F("SysEx Dump Complete"));
    currentBlock = 0;
    byteIndex = 0;
  }
}