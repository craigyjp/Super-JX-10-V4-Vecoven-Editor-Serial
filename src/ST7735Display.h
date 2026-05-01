#define DISPLAYTIMEOUT 1500

#include <LiquidCrystal_PCF8574.h>
#include "ToneService.h"

#define PULSE 1
#define VAR_TRI 2
#define FILTER_ENV 3
#define AMP_ENV 4

enum DisplayMode {
  DM_TONE_FLASH = 0,   // LOWER/UPPER param flash with arrow markers
  DM_PATCH_FLASH = 1,  // patch-param flash
  DM_EDIT_FLASH = 2,   // generic "editing parameter" flash
  DM_SYSEX_BANNER = 5  // sysex dump banner
};

LiquidCrystal_PCF8574 lcd(0x27);  // set the LCD address to 0x27 for a 16 chars and 2 line display

String currentParameter = "";
String currentValue = "";
float currentFloatValue = 0.0;
String currentPgmNum = "";
String currentPatchName = "";
String newPatchName = "";
const char *currentSettingsOption = "";
const char *currentSettingsValue = "";
int currentSettingsPart = SETTINGS;

String currentToneParameter = "";
String currentToneUpper = "";
String currentToneLower = "";

const char *currentPatchOption = "";
const char *currentPatchValue = "";
int currentPatchPart = PATCH_EDIT;

const char *currentToneOption = "";
const char *currentToneValue = "";
int currentTonePart = TONE_EDIT;

const char *currentMIDIOption = "";
const char *currentMIDIValue = "";
int currentMIDIPart = MIDI_EDIT;

int paramType = PARAMETER;

boolean MIDIClkSignal = false;
int Patchnumber = 0;
unsigned long timeout = 0;

char buf[7];  // 8 chars + null terminator
char bankStr[12];

static void renderToneFlashPage();    // displayMode 0
static void renderPatchFlashPage();   // displayMode 1
static void renderEditFlashPage();    // displayMode 2
static void renderSysexBannerPage();  // displayMode 5

void startTimer() {
  if (state == PARAMETER) {
    timeout = millis();
  }
}

static void drawBankSlotHeader() {
  lcd.setCursor(0, 0);
  lcd.print("I:");
  lcd.setCursor(2, 0);
  lcd.print(currentBank + 1);
  lcd.setCursor(0, 1);
  lcd.print((char)('A' + currentGroup));
  lcd.print(currentSlot);
}

static void clearTheToneMarkers() {
  // arrows (clear 8, paint 4)
  lcd.setCursor(26, 0);
  lcd.print(" ");
  lcd.setCursor(32, 0);
  lcd.print(" ");
  lcd.setCursor(33, 0);
  lcd.print(" ");
  lcd.setCursor(39, 0);
  lcd.print(" ");
  lcd.setCursor(26, 1);
  lcd.print(" ");
  lcd.setCursor(32, 1);
  lcd.print(" ");
  lcd.setCursor(33, 1);
  lcd.print(" ");
  lcd.setCursor(39, 1);
  lcd.print(" ");

  if (upperSW) {
    lcd.setCursor(33, 0);
    lcd.write(byte(0));
    lcd.setCursor(39, 0);
    lcd.write(byte(0));
    lcd.setCursor(33, 1);
    lcd.write(byte(0));
    lcd.setCursor(39, 1);
    lcd.write(byte(0));
  } else {
    lcd.setCursor(26, 0);
    lcd.write(byte(0));
    lcd.setCursor(32, 0);
    lcd.write(byte(0));
    lcd.setCursor(26, 1);
    lcd.write(byte(0));
    lcd.setCursor(32, 1);
    lcd.write(byte(0));
  }
}

void renderBootUpPage() {
  lcd.home();
  lcd.clear();
  lcd.setCursor(7, 0);
  lcd.print("*****  ROLAND  JX-10  *****");
  lcd.setCursor(0, 1);
  lcd.print("VECOVEN ENHANCED MODE   ");
  lcd.print("VERSION ");
  lcd.print(VERSION);
}

void renderCurrentPatchPage() {
  lcd.clear();
  lcd.noBlink();
  drawBankSlotHeader();

  lcd.setCursor(6, 0);
  switch (keyMode) {
    case 0:
      lcd.print("DUAL");
      break;

    case 1:
      lcd.print("WH-L");
      break;

    case 2:
      lcd.print("WH-U");
      break;

    case 3:
      lcd.print("SPLT");
      break;

    case 4:
      lcd.print("X-FD");
      break;

    case 5:
      lcd.print("T-VC");
      break;
  }
  lcd.setCursor(12, 0);
  lcd.print(patchName);

  if (upperSW) {
    lcd.setCursor(35, 0);
    lcd.write(byte(0));
  } else {
    lcd.setCursor(35, 1);
    lcd.write(byte(0));
  }
  lcd.setCursor(36, 0);
  lcd.print(upperToneNumber);
  if (upperSW) {
    lcd.setCursor(39, 0);
    lcd.write(byte(0));
  } else {
    lcd.setCursor(39, 1);
    lcd.write(byte(0));
  }
  // lower section
  lcd.setCursor(36, 1);
  lcd.print(lowerToneNumber);
}

void renderCurrentParameterPage() {
  if (state != PARAMETER) return;

  switch (displayMode) {
    case DM_TONE_FLASH: renderToneFlashPage(); break;
    case DM_PATCH_FLASH: renderPatchFlashPage(); break;
    case DM_EDIT_FLASH: renderEditFlashPage(); break;
    case DM_SYSEX_BANNER:
      renderSysexBannerPage();
      break;
      // no default — unknown mode = no-op, same as original silently did
  }
}

static void renderToneFlashPage() {
  lcd.clear();
  drawBankSlotHeader();

  lcd.setCursor(5, 0);
  lcd.print(upperSW ? "UPPER  PARAMETER" : "LOWER  PARAMETER");
  lcd.setCursor(27, 0);
  lcd.print("LOWER");
  lcd.setCursor(34, 0);
  lcd.print("UPPER");

  lcd.setCursor(5, 1);
  lcd.print("  ");
  lcd.setCursor(5, 1);
  lcd.print(upperSW ? upperToneNumber : lowerToneNumber);

  clearTheToneMarkers();

  lcd.setCursor(12, 1);
  lcd.print(currentToneParameter);

  // Clear value columns then paint both
  lcd.setCursor(27, 1);
  lcd.print("     ");
  lcd.setCursor(34, 1);
  lcd.print("     ");
  lcd.setCursor(27, 1);
  lcd.print(currentToneLower);
  lcd.setCursor(34, 1);
  lcd.print(currentToneUpper);
}

static void renderPatchFlashPage() {
  lcd.clear();

  drawBankSlotHeader();

  lcd.setCursor(5, 0);
  lcd.print("PATCH  PARAMETER");
  lcd.setCursor(12, 1);
  lcd.print(currentParameter);

  lcd.setCursor(34, 1);
  String valueStr = String(currentValue);
  if (valueStr.length() > 27) valueStr = valueStr.substring(0, 27);

  snprintf(buf, sizeof(buf), "%6s", valueStr.c_str());
  lcd.print(buf);
}

static void renderEditFlashPage() {
  lcd.clear();

  drawBankSlotHeader();

  lcd.setCursor(6, 0);
  lcd.print("EDITING PARAMETER");
  lcd.setCursor(15, 1);
  lcd.print(currentParameter);
  lcd.setCursor(28, 1);
  lcd.print(currentValue);
}

static void renderSysexBannerPage() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("**********");
  lcd.setCursor(12, 0);
  lcd.print(currentParameter);
  lcd.setCursor(18, 0);
  lcd.print(currentValue);
  lcd.setCursor(30, 0);
  lcd.print("**********");
}

FLASHMEM void showSaveTargetPage() {
  lcd.noBlink();
  lcd.setCursor(0, 0);  lcd.write(byte(1));
  for (int i = 1; i < 39; i++) {
    lcd.setCursor(i, 0); lcd.write(byte(2));
  }
  lcd.setCursor(39, 0); lcd.write(byte(4));

  // Frame — bottom row
  lcd.setCursor(0, 1);  lcd.write(byte(5));
  for (int i = 1; i < 39; i++) {
    lcd.setCursor(i, 1); lcd.write(byte(6));
  }
  lcd.setCursor(39, 1); lcd.write(byte(7));

  snprintf(bankStr, sizeof(bankStr), "%02d", saveTargetBank + 1);

  // Message — overlays dots in the frame
  lcd.setCursor(6, 0); lcd.print("WRITE PATCH   ");

  String msg = "TO  ";
  msg += "I";
  msg += bankStr; 
  msg += ":";
  msg += (char)('A' + saveTargetGroup);
  msg += saveTargetSlot;
  msg += "   ?";
  lcd.setCursor(20, 0); lcd.print(msg);
}

FLASHMEM void showBankPickingPage() {
  lcd.setCursor(25, 0);
  lcd.blink();

}

void renderPatchEditPage() {
  lcd.clear();
  lcd.noBlink();
  drawBankSlotHeader();

  // upper section of screen

  lcd.setCursor(5, 0);
  lcd.print("PATCH  PARAMETER");

  // lower section

  lcd.setCursor(5, 1);
  lcd.print("SETUP");

  lcd.setCursor(12, 1);
  String optionName = currentPatchOption;
  if (optionName.length() > 27) optionName = optionName.substring(0, 27);
  lcd.print(optionName);

  lcd.setCursor(32, 1);
  String valueStr = String(currentPatchValue);
  if (valueStr.length() > 27) valueStr = valueStr.substring(0, 27);

  snprintf(buf, sizeof(buf), "%6s", valueStr.c_str());
  lcd.print(buf);
}

void renderToneEditPage() {
  lcd.clear();
  lcd.noBlink();
  drawBankSlotHeader();

  // upper section of screen

  lcd.setCursor(5, 0);
  lcd.print(upperSW ? "UPPER  PARAMETER" : "LOWER  PARAMETER");
  lcd.setCursor(27, 0);
  lcd.print("LOWER");
  lcd.setCursor(34, 0);
  lcd.print("UPPER");

  clearTheToneMarkers();

  // lower section

  lcd.setCursor(12, 1);
  String optionName = currentToneOption;
  if (optionName.length() > 30) optionName = optionName.substring(0, 30);
  lcd.print(optionName);

  // Print the currently-edited tone number in column 5
  lcd.setCursor(5, 1);
  lcd.print("  ");
  lcd.setCursor(5, 1);
  lcd.print(upperSW ? upperToneNumber : lowerToneNumber);


  // Both values — active side at its column, other side at the other column.
  String activeValueStr = String(currentToneValue);
  String otherValueStr = String(tonemenu::other_tone_value());

  if (activeValueStr.length() > 5) activeValueStr = activeValueStr.substring(0, 5);
  if (otherValueStr.length() > 5) otherValueStr = otherValueStr.substring(0, 5);

  // Clear the value columns (5 chars each at 27 and 34)
  lcd.setCursor(27, 1);
  lcd.print("     ");
  lcd.setCursor(34, 1);
  lcd.print("     ");

  if (upperSW) {
    lcd.setCursor(27, 1);
    lcd.print(otherValueStr);  // LOWER column = other
    lcd.setCursor(34, 1);
    lcd.print(activeValueStr);  // UPPER column = active
  } else {
    lcd.setCursor(27, 1);
    lcd.print(activeValueStr);  // LOWER column = active
    lcd.setCursor(34, 1);
    lcd.print(otherValueStr);  // UPPER column = other
  }
}

void renderMIDIEditPage() {
  lcd.clear();
  lcd.noBlink();
  drawBankSlotHeader();
  // upper section of screen
  lcd.setCursor(5, 0);
  lcd.print("MIDI   PARAMETER");

  // lower section
  lcd.setCursor(5, 1);
  lcd.print("SETUP");


  lcd.setCursor(12, 1);
  String optionName = currentMIDIOption;
  if (optionName.length() > 27) optionName = optionName.substring(0, 27);
  lcd.print(optionName);

  lcd.setCursor(34, 1);
  String valueStr = String(currentMIDIValue);
  if (valueStr.length() > 27) valueStr = valueStr.substring(0, 27);

  snprintf(buf, sizeof(buf), "%6s", valueStr.c_str());
  lcd.print(buf);
}

void renderPatchNaming() {
  lcd.clear();
  drawBankSlotHeader();
  lcd.setCursor(5, 0);
  lcd.print("PATCH NAME");

  // Print the 18-char buffer starting at column 0 of line 1
  lcd.setCursor(20, 0);
  for (int i = 0; i < 18; i++) {
    lcd.print(patchNameBuffer[i]);
  }

  // Place hardware cursor at current edit position
  lcd.setCursor(20 + patchNameCursor, 0);
  lcd.blink();
}

void showRenamingPage(String newName) {
  newPatchName = newName;
}

void renderArpParameterPage() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ARP ");
  lcd.setCursor(4, 0);
  lcd.print(arpParamNames[arpParamIndex]);
  lcd.setCursor(0, 1);
  lcd.print("MEM:");
  lcd.setCursor(4, 1);
  lcd.print((char)('A' + arpMemoryIndex));
  lcd.setCursor(15, 1);
  lcd.print(arpParamValueString(arpParamIndex));
}

void renderSettingsPage() {
  lcd.clear();

  // Line 1: Setting name with up/down indicators on the right
  lcd.setCursor(0, 0);
  lcd.print("PARAMETER ");
  lcd.setCursor(10, 0);
  String optionName = currentSettingsOption;
  if (optionName.length() > 27) {
    optionName = optionName.substring(0, 27);
  }
  lcd.print(optionName);

  // Up/down indicators for SETTINGS navigation (right side of line 1)
  lcd.setCursor(38, 0);
  if (currentSettingsPart == SETTINGS) {
    lcd.write(byte(1));  // Up triangle
  } else {
    lcd.print("  ");
  }

  // Line 2: Setting value with up/down indicators on the right
  lcd.setCursor(0, 1);
  lcd.print("VALUE ");
  lcd.setCursor(10, 1);
  String valueStr = String(currentSettingsValue);
  if (valueStr.length() > 27) {
    valueStr = valueStr.substring(0, 27);
  }
  lcd.print(valueStr);

  // Up/down indicators for SETTINGSVALUE navigation (right side of line 2)
  lcd.setCursor(38, 1);
  if (currentSettingsPart == SETTINGSVALUE) {
    lcd.write(byte(2));  // Down triangle
  } else {
    lcd.print("  ");
  }
}

void showPatchEditPage(const char *option, const char *value, int part) {
  currentPatchOption = option;
  currentPatchValue = value;
  currentPatchPart = part;
}

void showToneEditPage(const char *option, const char *value, int part) {
  currentToneOption = option;
  currentToneValue = value;
  currentTonePart = part;
}

void showMIDIEditPage(const char *option, const char *value, int part) {
  currentMIDIOption = option;
  currentMIDIValue = value;
  currentMIDIPart = part;
}

void showCurrentTonePage(const char *param, String upperVal, String lowerVal) {
  if (state == SETTINGS || state == SETTINGSVALUE) state = PARAMETER;
  currentToneParameter = param;
  currentToneUpper = upperVal;
  currentToneLower = lowerVal;
  displayMode = DM_TONE_FLASH;
  startTimer();
}

void showCurrentParameterPage(const char *param, float val, int pType) {
  currentParameter = param;
  currentValue = String(val);
  currentFloatValue = val;
  paramType = pType;
  startTimer();
}

void showCurrentParameterPage(const char *param, String val, int pType) {
  if (state == SETTINGS || state == SETTINGSVALUE) state = PARAMETER;  //Exit settings page if showing
  currentParameter = param;
  currentValue = val;
  paramType = pType;
  startTimer();
}

void showCurrentParameterPage(const char *param, String val) {
  showCurrentParameterPage(param, val, PARAMETER);
}


void showPatchPage(String number, String patchName) {
  currentPgmNum = number;
  currentPatchName = patchName;
}

void showSettingsPage(const char *option, const char *value, int settingsPart) {
  currentSettingsOption = option;
  currentSettingsValue = value;
  currentSettingsPart = settingsPart;
}

void updateScreen() {
  switch (state) {
    case PARAMETER:
      if ((millis() - timeout) > DISPLAYTIMEOUT) {
        renderCurrentPatchPage();
      } else {
        renderCurrentParameterPage();
      }
      break;
    // case SAVE:
    //   renderSavePage();
    //   break;
    case PATCHNAMING:
      renderPatchNaming();
      break;
    case PATCH:
      renderCurrentPatchPage();
      break;
    case SETTINGS:
    case SETTINGSVALUE:
      renderSettingsPage();
      break;
    case ARP_EDIT:
    case ARP_EDITVALUE:
      renderArpParameterPage();
      break;
    case PATCH_EDIT:
    case PATCH_EDITVALUE:
      renderPatchEditPage();
      break;
    case TONE_EDIT:
    case TONE_EDITVALUE:
      renderToneEditPage();
      break;
    case MIDI_EDIT:
    case MIDI_EDITVALUE:
      renderMIDIEditPage();
      break;
  }
  //tft.updateScreen();
}

void setupDisplay() {

  lcd.begin(40, 2, Wire2);  // initialize the lcd
  lcd.createChar(0, midBar2);
  lcd.createChar(1, frameTopLeft);
  lcd.createChar(2, frameTopMid);
  lcd.createChar(3, backslashGlyph);  // use slot 1 (slot 0 is your existing arrow)
  lcd.createChar(4, frameTopRight);
  lcd.createChar(5, frameBotLeft);
  lcd.createChar(6, frameBotMid);
  lcd.createChar(7, frameBotRight);
  renderBootUpPage();
}
