#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// --- Configuration ---
LiquidCrystal_I2C lcd(0x27, 16, 2); 

// Pin Definitions
const int PIN_BTN1 = 32; // P1 / Decrease / Left
const int PIN_BTN2 = 33; // P2 / Increase / Right
const int PIN_BTN3 = 25; // Select / Next / Toggle Moves / Draw
const int PIN_BTN4 = 26; // Play / Pause
const int PIN_BTN5 = 27; // Back / Reset

const int LED_P1 = 13; // P1 LED Pin
const int LED_P2 = 12; // P2 LED Pin
const int BUZZER = 14;

// --- State Machine ---
enum State { 
  BOOT, 
  MENU_MAIN,
  SETUP_TIME, 
  SETUP_LEDS, 
  SETUP_SOUND, 
  READY,
  RUNNING, 
  PAUSED, 
  GAME_OVER 
};
State currentState = BOOT;

// --- Settings Variables ---
int setMins = 10;
int setSecs = 0;
int setInc = 0;

bool ledsEnabled = true;
bool sndPToggle = false;
bool sndAlert = true;
bool sndTimeout = true;

// --- Game Variables ---
long p1Seconds, p2Seconds;
int movesP1 = 0;
int movesP2 = 0;
bool isP1Turn = true;
unsigned long lastTickMillis = 0;

bool screenNeedsUpdate = true;
int setupCursor = 0;     // Tracks field inside menus
int mainMenuCursor = 0;  // 0: Time, 1: LEDs, 2: Sound
bool showMovesScreen = false; // Toggles clock vs moves display
int winState = 0; // 1 = P1, 2 = P2, 3 = Draw

void setup() {
  pinMode(PIN_BTN1, INPUT_PULLUP);
  pinMode(PIN_BTN2, INPUT_PULLUP);
  pinMode(PIN_BTN3, INPUT_PULLUP);
  pinMode(PIN_BTN4, INPUT_PULLUP);
  pinMode(PIN_BTN5, INPUT_PULLUP);
  
  pinMode(LED_P1, OUTPUT);
  pinMode(LED_P2, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  digitalWrite(LED_P1, LOW);
  digitalWrite(LED_P2, LOW);

  lcd.init();
  lcd.backlight();
  drawBootScreen();
}

void loop() {
  int btn = readButtons();

  switch (currentState) {
    case BOOT:        handleBoot(btn); break;
    case MENU_MAIN:   handleMainMenu(btn); break;
    case SETUP_TIME:  handleSetupTime(btn); break;
    case SETUP_LEDS:  handleSetupLEDs(btn); break;
    case SETUP_SOUND: handleSetupSound(btn); break;
    case READY:       handleReady(btn); break;
    case RUNNING:     handleRunning(btn); break;
    case PAUSED:      handlePaused(btn); break;
    case GAME_OVER:   handleGameOver(btn); break;
  }
}

// ==========================================
// STATE HANDLERS
// ==========================================

void handleBoot(int btn) {
  if (btn == PIN_BTN1) {
    currentState = READY;
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print(" Ready to Play! ");
    lcd.setCursor(0, 1); lcd.print(" Press P1 or P2 ");
    digitalWrite(LED_P1, HIGH); // Both LEDs on to signal ready
    digitalWrite(LED_P2, HIGH);
  } else if (btn == PIN_BTN2) {
    currentState = MENU_MAIN;
    mainMenuCursor = 0;
    screenNeedsUpdate = true;
    lcd.clear();
  }
}

void handleMainMenu(int btn) {
  if (screenNeedsUpdate) {
    lcd.setCursor(0, 0);
    if (mainMenuCursor == 0) lcd.print("> Time Setup    ");
    else lcd.print("  Time Setup    ");

    lcd.setCursor(0, 1);
    if (mainMenuCursor == 1) lcd.print("> LEDs   Sound  ");
    else if (mainMenuCursor == 2) lcd.print("  LEDs > Sound  ");
    else lcd.print("  LEDs   Sound  ");
    screenNeedsUpdate = false;
  }

  if (btn == PIN_BTN1 || btn == PIN_BTN2) {
    mainMenuCursor++;
    if (mainMenuCursor > 2) mainMenuCursor = 0;
    screenNeedsUpdate = true;
  }
  
  if (btn == PIN_BTN3) {
    setupCursor = 0;
    screenNeedsUpdate = true;
    lcd.clear();
    if (mainMenuCursor == 0) currentState = SETUP_TIME;
    if (mainMenuCursor == 1) currentState = SETUP_LEDS;
    if (mainMenuCursor == 2) currentState = SETUP_SOUND;
  }
  
  if (btn == PIN_BTN5) drawBootScreen();
}

void handleSetupTime(int btn) {
  if (screenNeedsUpdate) {
    lcd.setCursor(0, 0); lcd.print("Mins  Secs  Inc ");
    char buf[17];
    sprintf(buf, " %02d    %02d   +%02d ", setMins, setSecs, setInc);
    lcd.setCursor(0, 1); lcd.print(buf);
    
    if (setupCursor == 0) lcd.setCursor(2, 1);
    else if (setupCursor == 1) lcd.setCursor(8, 1);
    else if (setupCursor == 2) lcd.setCursor(14, 1);
    lcd.blink();
    screenNeedsUpdate = false;
  }

  if (btn == PIN_BTN1) { 
    if (setupCursor == 0 && setMins > 0) setMins--;
    if (setupCursor == 1 && setSecs > 0) setSecs--;
    if (setupCursor == 2 && setInc > 0) setInc--;
    screenNeedsUpdate = true;
  }
  if (btn == PIN_BTN2) { 
    if (setupCursor == 0 && setMins < 99) setMins++;
    if (setupCursor == 1 && setSecs < 59) setSecs++;
    if (setupCursor == 2 && setInc < 99) setInc++;
    screenNeedsUpdate = true;
  }
  if (btn == PIN_BTN3) { 
    setupCursor++;
    if (setupCursor > 2) {
      exitToMainMenu();
      return;
    }
    screenNeedsUpdate = true;
  }
  if (btn == PIN_BTN5) exitToMainMenu();
}

void handleSetupLEDs(int btn) {
  if (screenNeedsUpdate) {
    lcd.setCursor(0, 0); lcd.print("LEDs Setup:     ");
    lcd.setCursor(0, 1);
    if (ledsEnabled) lcd.print("     < ON >     ");
    else lcd.print("     < OFF >    ");
    lcd.noBlink();
    screenNeedsUpdate = false;
  }

  if (btn == PIN_BTN1 || btn == PIN_BTN2) {
    ledsEnabled = !ledsEnabled;
    screenNeedsUpdate = true;
  }
  if (btn == PIN_BTN3 || btn == PIN_BTN5) exitToMainMenu();
}

void handleSetupSound(int btn) {
  if (screenNeedsUpdate) {
    lcd.setCursor(0, 0); lcd.print("P-TGL Alrt T-OUT");
    lcd.setCursor(0, 1);
    lcd.print(sndPToggle ? " ON   " : " OFF  ");
    lcd.print(sndAlert ? " ON   " : " OFF  ");
    lcd.print(sndTimeout ? " ON  " : " OFF ");

    if (setupCursor == 0) lcd.setCursor(1, 1);
    else if (setupCursor == 1) lcd.setCursor(7, 1);
    else if (setupCursor == 2) lcd.setCursor(12, 1);
    lcd.blink();
    screenNeedsUpdate = false;
  }

  if (btn == PIN_BTN1 || btn == PIN_BTN2) {
    if (setupCursor == 0) sndPToggle = !sndPToggle;
    if (setupCursor == 1) sndAlert = !sndAlert;
    if (setupCursor == 2) sndTimeout = !sndTimeout;
    screenNeedsUpdate = true;
  }
  if (btn == PIN_BTN3) {
    setupCursor++;
    if (setupCursor > 2) {
      exitToMainMenu();
      return;
    }
    screenNeedsUpdate = true;
  }
  if (btn == PIN_BTN5) exitToMainMenu();
}

void handleReady(int btn) {
  if (btn == PIN_BTN1) {
    // P1 pressed their button, so P2 starts thinking
    isP1Turn = false;
    startGameReal();
  } else if (btn == PIN_BTN2) {
    // P2 pressed their button, so P1 starts thinking
    isP1Turn = true;
    startGameReal();
  } else if (btn == PIN_BTN5) {
    drawBootScreen();
  }
}

void handleRunning(int btn) {
  unsigned long currentMillis = millis();

  // Timer Tick
  if (currentMillis - lastTickMillis >= 1000) {
    lastTickMillis = currentMillis;
    if (isP1Turn) p1Seconds--; else p2Seconds--;
    
    // 10-Second Alert
    long activeTime = isP1Turn ? p1Seconds : p2Seconds;
    if (sndAlert && activeTime > 0 && activeTime <= 10) tone(BUZZER, 2000, 50);

    drawGameScreen();

    // Timeout Check
    if (p1Seconds <= 0 || p2Seconds <= 0) {
      winState = (p1Seconds <= 0) ? 2 : 1; 
      currentState = GAME_OVER;
      if (sndTimeout) tone(BUZZER, 500, 2000); // Long timeout beep
      digitalWrite(LED_P1, LOW); digitalWrite(LED_P2, LOW);
      screenNeedsUpdate = true;
    }
  }

  // Player Moves
  if (btn == PIN_BTN1 && isP1Turn) {
    isP1Turn = false;
    p1Seconds += setInc;
    movesP1++;
    triggerTurnFeedbacks();
  } 
  else if (btn == PIN_BTN2 && !isP1Turn) {
    isP1Turn = true;
    p2Seconds += setInc;
    movesP2++;
    triggerTurnFeedbacks();
  }
  
  // Toggle Moves Screen
  if (btn == PIN_BTN3) {
    showMovesScreen = !showMovesScreen;
    drawGameScreen();
  }

  // Pause
  if (btn == PIN_BTN4) {
    currentState = PAUSED;
    screenNeedsUpdate = true;
  }
}

void handlePaused(int btn) {
  if (screenNeedsUpdate) {
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("PAUSED   B3:Draw");
    lcd.setCursor(0, 1); lcd.print("B4:Play  B5:Quit");
    screenNeedsUpdate = false;
  }

  if (btn == PIN_BTN4) { // Resume
    currentState = RUNNING;
    lcd.clear();
    drawGameScreen();
    lastTickMillis = millis(); 
  }
  
  if (btn == PIN_BTN3) { // Draw
    winState = 3;
    currentState = GAME_OVER;
    triggerDrawFeedback();
    screenNeedsUpdate = true;
  }
  
  if (btn == PIN_BTN5) drawBootScreen(); // Quit
}

void handleGameOver(int btn) {
  if (screenNeedsUpdate) {
    lcd.clear();
    lcd.setCursor(0, 0);
    if (winState == 1) lcd.print(" Player 1 Wins! ");
    else if (winState == 2) lcd.print(" Player 2 Wins! ");
    else if (winState == 3) lcd.print("   Game Drawn   ");
    screenNeedsUpdate = false;
  }
  if (btn == PIN_BTN5 || btn == PIN_BTN4) drawBootScreen();
}

// ==========================================
// HELPER FUNCTIONS
// ==========================================

void startGameReal() {
  p1Seconds = (setMins * 60) + setSecs;
  p2Seconds = p1Seconds;
  movesP1 = 0;
  movesP2 = 0;
  showMovesScreen = false;
  winState = 0;
  currentState = RUNNING;
  
  updateLEDs();
  lcd.clear();
  drawGameScreen();
  lastTickMillis = millis();
}

void drawBootScreen() {
  currentState = BOOT;
  lcd.noBlink();
  digitalWrite(LED_P1, LOW); digitalWrite(LED_P2, LOW);
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("1:Start  2:Setup");
  lcd.setCursor(0, 1); lcd.print("Ready...        ");
}

void exitToMainMenu() {
  lcd.noBlink();
  currentState = MENU_MAIN;
  screenNeedsUpdate = true;
  lcd.clear();
}

void drawGameScreen() {
  if (showMovesScreen) {
    lcd.setCursor(0, 0);
    lcd.print("   Move Count   ");
    char buf[17];
    sprintf(buf, " P1:%03d  P2:%03d ", movesP1, movesP2);
    lcd.setCursor(0, 1);
    lcd.print(buf);
  } else {
    char buf[17];
    sprintf(buf, "  %02d:%02d  %02d:%02d  ", p1Seconds/60, p1Seconds%60, p2Seconds/60, p2Seconds%60);
    
    lcd.setCursor(0, 0);
    if (isP1Turn) lcd.print(" [P1]      P2   ");
    else lcd.print("  P1      [P2]  ");
    
    lcd.setCursor(0, 1);
    lcd.print(buf);
  }
}

void triggerTurnFeedbacks() {
  if (sndPToggle) tone(BUZZER, 1000, 50);
  updateLEDs();
  drawGameScreen();
}

void triggerDrawFeedback() {
  digitalWrite(LED_P1, HIGH);
  digitalWrite(LED_P2, HIGH);
  
  // Distinct 3-beep sound for Draw
  tone(BUZZER, 800, 150); delay(200);
  tone(BUZZER, 800, 150); delay(200);
  tone(BUZZER, 800, 400);
}

void updateLEDs() {
  if (!ledsEnabled) {
    digitalWrite(LED_P1, LOW); digitalWrite(LED_P2, LOW);
    return;
  }
  // The LED for the currently active player goes HIGH
  if (isP1Turn) {
    digitalWrite(LED_P1, HIGH);
    digitalWrite(LED_P2, LOW);
  } else {
    digitalWrite(LED_P1, LOW);
    digitalWrite(LED_P2, HIGH);
  }
}

int readButtons() {
  static unsigned long lastDebounceTime = 0;
  if (millis() - lastDebounceTime < 200) return -1;

  if (digitalRead(PIN_BTN1) == LOW) { lastDebounceTime = millis(); return PIN_BTN1; }
  if (digitalRead(PIN_BTN2) == LOW) { lastDebounceTime = millis(); return PIN_BTN2; }
  if (digitalRead(PIN_BTN3) == LOW) { lastDebounceTime = millis(); return PIN_BTN3; }
  if (digitalRead(PIN_BTN4) == LOW) { lastDebounceTime = millis(); return PIN_BTN4; }
  if (digitalRead(PIN_BTN5) == LOW) { lastDebounceTime = millis(); return PIN_BTN5; }
  
  return -1;
}