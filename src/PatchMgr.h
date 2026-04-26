// JX-10 style patch management
// SD structure: /bank0/ .. /bank7/  containing files 11-88
// where tens digit = group (A=1..H=8), units digit = slot (1-8)
// e.g. bank2/35 = bank 2, group C (3), slot 5

// Forward declarations - defined in main.ino
String getCurrentPatchData();
void setCurrentPatchData(String data[]);
void showPatchPage(String number, String patchName);

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

#define NUM_BANKS   16   // banks 0-7, selected via Recall + A-H
#define NUM_GROUPS  8   // groups A-H (1-8), rows of patch buttons
#define NUM_SLOTS   8   // slots 1-8, columns of patch buttons

#define TOTALCHARS 63
const char CHARACTERS[TOTALCHARS] = {
  'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z',
  'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
  ' ','1','2','3','4','5','6','7','8','9','0'
};
int charIndex = 0;
char currentCharacter = 0;
String renamedPatch = "";

// ---------------------------------------------------------------------------
// Current position state
// group: 0-7 (A-H), slot: 1-8, bank: 0-7
// ---------------------------------------------------------------------------
int currentBank  = 0;   // active bank (0-7)
int currentGroup = 0;   // active group 0=A .. 7=H
int currentSlot  = 1;   // active slot 1-8

// ---------------------------------------------------------------------------
// Build SD file path:  /bankN/GS   e.g.  /bank2/35
// group is 0-based internally, file uses 1-based tens digit
// ---------------------------------------------------------------------------
String getPatchPath(int bank, int group, int slot) {
  // group 0-7 -> file tens digit 1-8
  int fileNum = (group + 1) * 10 + slot;
  return "/bank" + String(bank) + "/" + String(fileNum);
}

// Convenience overload using current position
String getCurrentPatchPath() {
  return getPatchPath(currentBank, currentGroup, currentSlot);
}

// Human-readable label e.g. "B3"
String getPatchLabel(int group, int slot) {
  return String((char)('A' + group)) + String(slot);
}

// ---------------------------------------------------------------------------
// Low-level file helpers  (unchanged from original)
// ---------------------------------------------------------------------------
size_t readField(File *file, char *str, size_t size, const char *delim) {
  char ch;
  size_t n = 0;
  while ((n + 1) < size && file->read(&ch, 1) == 1) {
    if (ch == '\r') continue;
    str[n++] = ch;
    if (strchr(delim, ch)) break;
  }
  str[n] = '\0';
  return n;
}

void recallPatchData(File patchFile, String data[]) {
  size_t n;
  char str[20];
  int i = 0;
  while (patchFile.available() && i < NO_OF_PARAMS) {
    n = readField(&patchFile, str, sizeof(str), ",\n");
    if (n == 0) break;
    if (str[n - 1] == ',' || str[n - 1] == '\n') {
      str[n - 1] = 0;
    } else {
      Serial.print(patchFile.available() ? F("error: ") : F("eof:   "));
    }
    data[i++] = String(str);
  }
}

// ---------------------------------------------------------------------------
// Save / Delete
// ---------------------------------------------------------------------------
void savePatch(const char *path, String patchData) {
  if (SD.exists(path)) SD.remove(path);
  File patchFile = SD.open(path, FILE_WRITE);
  if (patchFile) {
    patchFile.println(patchData);
    patchFile.close();
  } else {
    Serial.print(F("Error writing patch: "));
    Serial.println(path);
  }
}

void savePatch(const char *path, String patchData[]) {
  String dataString = patchData[0];
  for (int i = 1; i < NO_OF_PARAMS; i++) {
    dataString += "," + patchData[i];
  }
  savePatch(path, dataString);
}

// Save current patch to current bank/group/slot
void saveCurrentPatch() {
  String path = getCurrentPatchPath();
  patchName = patchName.length() > 0 ? patchName : INITPATCHNAME;
  savePatch(path.c_str(), getCurrentPatchData());
  showPatchPage(getPatchLabel(currentGroup, currentSlot), patchName);
  Serial.print(F("Saved to "));
  Serial.println(path);
}

// Save to an explicit bank/group/slot (used by save-to-destination flow)
void savePatchTo(int bank, int group, int slot) {
  String path = getPatchPath(bank, group, slot);
  patchName = patchName.length() > 0 ? patchName : INITPATCHNAME;
  savePatch(path.c_str(), getCurrentPatchData());
  // Update current position to where we just saved
  currentBank  = bank;
  currentGroup = group;
  currentSlot  = slot;
  showPatchPage(getPatchLabel(currentGroup, currentSlot), patchName);
  Serial.print(F("Saved to "));
  Serial.println(path);
}