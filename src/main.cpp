#include <M5Unified.h>
#include <Adafruit_Fingerprint.h>
#include <AudioFileSourceSD.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>
#include <AudioFileSourcePROGMEM.h>
#include "jiang_mp3.h"

#define FINGERPRINT_TX 33
#define FINGERPRINT_RX 32
#define MAX_FINGERPRINTS 50
#define LONG_TOUCH_DURATION 1000 

#define YELLOW 0xFFE0
#define CYAN 0x07FF
#define MAGENTA 0xF81F

Adafruit_Fingerprint finger = Adafruit_Fingerprint(&Serial2);

AudioGeneratorMP3 *mp3;
AudioFileSourceSD *file;
AudioOutputI2S *out;

enum ProgramState {
    MAIN_MENU,
    ADMIN_REGISTRATION,
    FINGERPRINT_VERIFICATION,
    ENROLLMENT
};

ProgramState currentState = MAIN_MENU;

// Function prototypes
uint8_t getFingerprintID();
void enrollFingerprint(bool isAdmin);
uint8_t getFingerprintEnroll(int id);
void showSplashScreen();
void scanEnrolledFingerprints();
void showMainMenu();
void flushFingerprints();
bool checkAdminFingerprint();
void printWithDelay(const char* message, int delayMs);
void playWelcomeMessage();
void testSpeaker();
void debugPrintAdminStatus();
void handleEnrollment();
void showAdminRegistrationMenu();
void handleFingerprint();
void debugPrintFingerprint(uint8_t result);

// Global variables
bool isAdmin = false;
bool adminRegistrationMode = false;
bool isFingerPrintAdmin[MAX_FINGERPRINTS] = {false};
unsigned long touchStartTime = 0;

void testSpeaker() {
    M5.Speaker.tone(440, 1000);  
    delay(1000);
    M5.Speaker.stop();  
}

void setup() {
  auto cfg = M5.config();
  cfg.internal_spk = true;  
  cfg.internal_mic = true;  
  M5.begin(cfg);
  
  Serial2.begin(57600, SERIAL_8N1, FINGERPRINT_RX, FINGERPRINT_TX);
  
  M5.Display.setTextSize(2);
  M5.Display.setCursor(0, 0);

  M5.Speaker.setVolume(128);
  
  M5.Display.println("Testing speaker...");
  testSpeaker();
  M5.Display.println("Speaker test complete");
  delay(2000);

  showSplashScreen();
  
  if (finger.verifyPassword()) {
    M5.Display.println("Fingerprint sensor found!");
  } else {
    M5.Display.println("Fingerprint sensor not found :(");
    while (1);
  }

  
  out = new AudioOutputI2S();
  out->SetPinout(12, 0, 2);
  out->SetOutputModeMono(true);
  
  delay(2000);
  showMainMenu();
}

void printWithDelay(const char* message, int delayMs = 1000) {
  M5.Display.println(message);
  delay(delayMs);
}

void loop() {
    M5.update();

    
    if (M5.Touch.getCount() && touchStartTime == 0) {
        touchStartTime = millis();
    } else if (!M5.Touch.getCount() && touchStartTime > 0) {
        if (millis() - touchStartTime >= LONG_TOUCH_DURATION) {
            if (currentState == MAIN_MENU) {
                currentState = ADMIN_REGISTRATION;
                showAdminRegistrationMenu();
            } else if (currentState == ADMIN_REGISTRATION) {
                currentState = MAIN_MENU;
                isAdmin = false;  
                showMainMenu();
            }
        }
        touchStartTime = 0;
    }

    
    switch (currentState) {
        case MAIN_MENU:
            if (M5.BtnA.wasPressed()) {
                handleFingerprint();
            } else if (M5.BtnB.wasPressed()) {
                handleEnrollment();
            } else if (M5.BtnC.wasPressed()) {
                debugPrintAdminStatus();
            }
            break;

        case ADMIN_REGISTRATION:
            if (M5.BtnA.wasPressed()) {
                enrollFingerprint(true);  
                showAdminRegistrationMenu();
            } else if (M5.BtnB.wasPressed()) {
                flushFingerprints();
                showAdminRegistrationMenu();
            } else if (M5.BtnC.wasPressed()) {
                currentState = MAIN_MENU;
                isAdmin = false;  
                showMainMenu();
            }
            break;

        default:
            
            currentState = MAIN_MENU;
            isAdmin = false;
            showMainMenu();
            break;
    }

    delay(100);
}
void scanEnrolledFingerprints() {
  M5.Display.fillScreen(BLACK);
  M5.Display.setCursor(0, 0);
  M5.Display.println("Scanning enrolled fingerprints...");
  
  for (int i = 1; i < MAX_FINGERPRINTS; i++) {
    if (finger.loadModel(i) == FINGERPRINT_OK) {
      M5.Display.printf("ID %d: Enrolled", i);
      if (isFingerPrintAdmin[i]) {
        M5.Display.println(" (Admin)");
      } else {
        M5.Display.println(" (User)");
      }
    }
  }
  
  M5.Display.println("Scan complete. Press any button.");
  while (!M5.BtnA.wasPressed() && !M5.BtnB.wasPressed() && !M5.BtnC.wasPressed()) {
    M5.update();
    delay(100);
  }
}

void flushFingerprints() {
    M5.Display.fillScreen(BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.println("Flushing all fingerprints...");

    for (int i = 1; i < MAX_FINGERPRINTS; i++) {
        if (finger.deleteModel(i) == FINGERPRINT_OK) {
            isFingerPrintAdmin[i] = false;
        }
    }

    M5.Display.println("All fingerprints deleted.");
    delay(2000);
    
    
    if (currentState == ADMIN_REGISTRATION) {
        showAdminRegistrationMenu();
    } else {
        currentState = MAIN_MENU;
        showMainMenu();
    }
}

void handleFingerprint() {
    M5.Display.fillScreen(BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.println("Place finger on sensor");

    uint8_t result = getFingerprintID();
    debugPrintFingerprint(result);

    if (result == FINGERPRINT_OK) {
        M5.Display.println("Fingerprint matched!");
        M5.Display.printf("ID #%d\n", finger.fingerID);
        if (isFingerPrintAdmin[finger.fingerID]) {
            isAdmin = true;
            M5.Display.println("Admin rights granted");
            playWelcomeMessage();
        } else {
            isAdmin = false;
            M5.Display.println("User verified");
        }
    } else if (result == FINGERPRINT_NOTFOUND) {
        M5.Display.println("Fingerprint not found");
        isAdmin = false;
    } else {
        M5.Display.println("Error reading fingerprint");
        isAdmin = false;
    }
    delay(2000);
    currentState = MAIN_MENU;
    showMainMenu();
}
void handleEnrollment() {
    if (!isAdmin) {
        if (!checkAdminFingerprint()) {
            M5.Display.fillScreen(BLACK);
            M5.Display.setCursor(0, 0);
            printWithDelay("Admin rights required");
            printWithDelay("Returning to main menu");
            currentState = MAIN_MENU;
            showMainMenu();
            return;
        }
    }

    while (true) {
        M5.Display.fillScreen(BLACK);
        M5.Display.setCursor(0, 0);
        M5.Display.println("User Enrollment Menu");
        M5.Display.println("A: Enroll new user");
        M5.Display.println("B: Back to main menu");

        while (!M5.BtnA.wasPressed() && !M5.BtnB.wasPressed()) {
            M5.update();
            delay(100);
        }

        if (M5.BtnA.wasPressed()) {
            enrollFingerprint(false);  
        } else if (M5.BtnB.wasPressed()) {
            break;
        }
    }
    
    currentState = MAIN_MENU;
    showMainMenu();
}
void showSplashScreen() {
  M5.Display.fillScreen(BLACK);
  M5.Display.setTextColor(WHITE);
  
  
  for (int i = 0; i <= 100; i += 5) {
    M5.Display.fillScreen(BLACK);
    M5.Display.setTextSize(3);
    M5.Display.setCursor(20, i);
    M5.Display.setTextColor(BLUE);
    M5.Display.println("Fingerprint");
    M5.Display.setCursor(80, i + 40);
    M5.Display.setTextColor(RED);
    M5.Display.println("Sensor");
    M5.Display.setCursor(50, i + 80);
    M5.Display.setTextColor(GREEN);
    M5.Display.println("App v1.6");
    delay(50);
  }
  
  
  for (int j = 0; j < 3; j++) {
    for (int s = 2; s <= 4; s++) {
      M5.Display.fillScreen(BLACK);
      M5.Display.setTextSize(s);
      M5.Display.setCursor(20, 100);
      M5.Display.setTextColor(BLUE);
      M5.Display.println("Fingerprint");
      M5.Display.setCursor(80, 140);
      M5.Display.setTextColor(RED);
      M5.Display.println("Sensor");
      M5.Display.setCursor(50, 180);
      M5.Display.setTextColor(GREEN);
      M5.Display.println("App v1.6");
      delay(100);
    }
  }
  
  delay(1000);
}

void showMainMenu() {
  M5.Display.fillScreen(BLACK);
  M5.Display.setTextSize(2);
  
  M5.Display.setCursor(0, 20);
  M5.Display.setTextColor(YELLOW);
  M5.Display.println("A: Verify fingerprint");
  
  M5.Display.setCursor(0, 60);
  M5.Display.setTextColor(CYAN);
  M5.Display.println("B: Enrollment menu");
  
  M5.Display.setCursor(0, 100);
  M5.Display.setTextColor(MAGENTA);
  M5.Display.println("C: Debug admin status");
  
  M5.Display.setCursor(0, 140);
  M5.Display.setTextColor(GREEN);
  M5.Display.println("Long touch: Admin Mode");
}

void showAdminRegistrationMenu() {
  M5.Display.fillScreen(BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(0, 0);
  M5.Display.setTextColor(YELLOW);
  M5.Display.println("Admin Registration Mode");
  M5.Display.setTextColor(CYAN);
  M5.Display.println("A: Enroll admin");
  M5.Display.setTextColor(MAGENTA);
  M5.Display.println("B: Flush fingerprints");
  M5.Display.setTextColor(GREEN);
  M5.Display.println("C: Exit admin mode");
}

void enrollFingerprint(bool asAdmin) {
  scanEnrolledFingerprints();  

  int id = 1;
  while (id < MAX_FINGERPRINTS && finger.loadModel(id) == FINGERPRINT_OK) {
    id++;
  }
  
  if (id >= MAX_FINGERPRINTS) {
    printWithDelay("No free slot for new fingerprint");
    return;
  }

  M5.Display.fillScreen(BLACK);
  M5.Display.setCursor(0, 0);
  if (currentState == ADMIN_REGISTRATION) {
    printWithDelay("Enrolling admin fingerprint");
    asAdmin = true;  
  } else {
    printWithDelay("Enrolling user fingerprint");
    asAdmin = false;  
  }
  printWithDelay("Please wait...");

  uint8_t result = getFingerprintEnroll(id);
  
  if (result == id) {
    isFingerPrintAdmin[id] = asAdmin;
    M5.Display.fillScreen(BLACK);
    M5.Display.setCursor(0, 0);
    printWithDelay(asAdmin ? "Admin fingerprint enrolled" : "User fingerprint enrolled");
    printWithDelay("Enrollment successful!");
    char buffer[50];
    snprintf(buffer, sizeof(buffer), "Enrolled ID: %d", id);
    printWithDelay(buffer);
    snprintf(buffer, sizeof(buffer), "Admin status: %s", isFingerPrintAdmin[id] ? "true" : "false");
    printWithDelay(buffer);
  } else if (result == FINGERPRINT_ENROLLMISMATCH) {
    printWithDelay("Enrollment cancelled or failed");
  } else {
    printWithDelay("Enrollment failed");
    char buffer[50];
    snprintf(buffer, sizeof(buffer), "Error code: %d", result);
    printWithDelay(buffer);
  }
  
  printWithDelay("Press any button to continue");
  while (!M5.BtnA.wasPressed() && !M5.BtnB.wasPressed() && !M5.BtnC.wasPressed()) {
    M5.update();
    delay(100);
  }

  scanEnrolledFingerprints();  
}


bool checkAdminFingerprint() {
  M5.Display.fillScreen(BLACK);
  M5.Display.setCursor(0, 0);
  printWithDelay("Admin verification required");
  printWithDelay("Place admin finger on sensor");
  
  uint8_t result = getFingerprintID();
  debugPrintFingerprint(result);
  
  if (result == FINGERPRINT_OK) {
    char buffer[50];
    snprintf(buffer, sizeof(buffer), "Fingerprint ID: %d", finger.fingerID);
    printWithDelay(buffer);
    if (finger.fingerID > 0 && finger.fingerID < MAX_FINGERPRINTS && isFingerPrintAdmin[finger.fingerID]) {
      isAdmin = true;
      printWithDelay("Admin verified");
      snprintf(buffer, sizeof(buffer), "Admin ID: %d", finger.fingerID);
      printWithDelay(buffer);
      playWelcomeMessage();
      return true;
    } else {
      printWithDelay("Not an admin fingerprint");
      snprintf(buffer, sizeof(buffer), "Admin status: %s", isFingerPrintAdmin[finger.fingerID] ? "true" : "false");
      printWithDelay(buffer);
    }
  } else {
    printWithDelay("Verification failed");
  }
  
  isAdmin = false;
  printWithDelay("Returning to main menu");
  return false;
}

void debugPrintFingerprint(uint8_t result) {
  M5.Display.println("Debug: Fingerprint Result");
  M5.Display.printf("Result code: %d\n", result);
  switch (result) {
    case FINGERPRINT_OK:
      M5.Display.println("FINGERPRINT_OK");
      break;
    case FINGERPRINT_NOTFOUND:
      M5.Display.println("FINGERPRINT_NOTFOUND");
      break;
    case FINGERPRINT_NOFINGER:
      M5.Display.println("FINGERPRINT_NOFINGER");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      M5.Display.println("FINGERPRINT_PACKETRECIEVEERR");
      break;
    case FINGERPRINT_IMAGEFAIL:
      M5.Display.println("FINGERPRINT_IMAGEFAIL");
      break;
    case FINGERPRINT_IMAGEMESS:
      M5.Display.println("FINGERPRINT_IMAGEMESS");
      break;
    case FINGERPRINT_FEATUREFAIL:
      M5.Display.println("FINGERPRINT_FEATUREFAIL");
      break;
    case FINGERPRINT_INVALIDIMAGE:
      M5.Display.println("FINGERPRINT_INVALIDIMAGE");
      break;
    case FINGERPRINT_ENROLLMISMATCH:
      M5.Display.println("FINGERPRINT_ENROLLMISMATCH");
      break;
    default:
      M5.Display.println("UNKNOWN ERROR");
      break;
  }
  delay(2000);  
}

void debugPrintAdminStatus() {
  M5.Display.fillScreen(BLACK);
  M5.Display.setCursor(0, 0);
  M5.Display.println("Debug: Admin Status");
  M5.Display.printf("isAdmin: %s\n", isAdmin ? "true" : "false");
  M5.Display.println("Enrolled fingerprints:");
  for (int i = 1; i < MAX_FINGERPRINTS; i++) {
    if (finger.loadModel(i) == FINGERPRINT_OK) {
      M5.Display.printf("ID %d: %s\n", i, isFingerPrintAdmin[i] ? "Admin" : "User");
    }
  }
  M5.Display.println("Press any button to return");
  while (!M5.BtnA.wasPressed() && !M5.BtnB.wasPressed() && !M5.BtnC.wasPressed()) {
    M5.update();
    delay(100);
  }
  
  
  if (currentState == ADMIN_REGISTRATION) {
    showAdminRegistrationMenu();
  } else {
    currentState = MAIN_MENU;
    showMainMenu();
  }
}


uint8_t getFingerprintID() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK)  return p;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK)  return p;

  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK)  return p;
  

  M5.Display.print("Found ID #"); M5.Display.print(finger.fingerID); 
  M5.Display.print(" with confidence of "); M5.Display.println(finger.confidence);
  return FINGERPRINT_OK;
}

void playWelcomeMessage() {
    M5.Display.fillScreen(BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(YELLOW);
    M5.Display.setCursor(10, 10);
    M5.Display.println("Felicitations ");

    AudioFileSourcePROGMEM* file = new AudioFileSourcePROGMEM(jiang_mp3, jiang_mp3_len);
    AudioGeneratorMP3* mp3 = new AudioGeneratorMP3();
    AudioOutputI2S* out = new AudioOutputI2S();
    
    out->SetPinout(12, 0, 2);
    out->SetOutputModeMono(true);
    out->SetGain(0.5);

    if (mp3->begin(file, out)) {
        int animFrame = 0;
        unsigned long lastUpdate = 0;
        while (mp3->isRunning()) {
            if (mp3->loop()) {
                unsigned long now = millis();
                if (now - lastUpdate > 500) {  
                    M5.Display.fillRect(0, 100, 320, 40, BLACK);
                    M5.Display.setCursor(10, 100);
                    M5.Display.setTextColor(CYAN);
                    M5.Display.print("Playing");
                    for (int i = 0; i < animFrame; i++) {
                        M5.Display.print(".");
                    }
                    animFrame = (animFrame + 1) % 4;
                    lastUpdate = now;
                }
            } else {
                mp3->stop();
            }
            M5.update();
        }

        M5.Display.fillScreen(BLACK);
        M5.Display.setCursor(10, 10);
        M5.Display.setTextColor(GREEN);
        M5.Display.println("Welcome message finished");
    } else {
        M5.Display.setTextColor(RED);
        M5.Display.println("Failed to start MP3 playback");
    }

    delay(2000);

    delete file;
    delete mp3;
    delete out;
}
uint8_t getFingerprintEnroll(int id) {
  int p = -1;
  int attemptCount = 0;
  const int maxAttempts = 3;
  unsigned long startTime;
  const unsigned long timeout = 15000; 

  M5.Display.fillScreen(BLACK);
  M5.Display.setCursor(0, 0);
  M5.Display.println("New Fingerprint Enrollment");
  M5.Display.println("Place your finger on the sensor");
  M5.Display.println("Press C to cancel");

  while (attemptCount < maxAttempts) {
    p = -1;
    startTime = millis();
    while (p != FINGERPRINT_OK && millis() - startTime < timeout) {
      M5.update();
      if (M5.BtnC.wasPressed()) {
        M5.Display.println("Enrollment cancelled");
        return FINGERPRINT_ENROLLMISMATCH;
      }
      p = finger.getImage();
      switch (p) {
        case FINGERPRINT_OK:
          M5.Display.println("Image taken");
          break;
        case FINGERPRINT_NOFINGER:
          M5.Display.print(".");
          break;
        case FINGERPRINT_PACKETRECIEVEERR:
          M5.Display.println("Communication error");
          break;
        case FINGERPRINT_IMAGEFAIL:
          M5.Display.println("Imaging error");
          break;
        default:
          M5.Display.println("Unknown error");
          break;
      }
      delay(100);
    }

    if (p != FINGERPRINT_OK) {
      M5.Display.println("Timeout or error occurred");
      return p;
    }

    p = finger.image2Tz(1);
    if (p != FINGERPRINT_OK) {
      M5.Display.println("Image conversion failed");
      continue;
    }

    M5.Display.println("Remove finger");
    delay(2000);
    p = 0;
    while (p != FINGERPRINT_NOFINGER && millis() - startTime < timeout) {
      p = finger.getImage();
      delay(100);
    }

    M5.Display.println("Place same finger again");
    p = -1;
    startTime = millis();
    while (p != FINGERPRINT_OK && millis() - startTime < timeout) {
      M5.update();
      if (M5.BtnC.wasPressed()) {
        M5.Display.println("Enrollment cancelled");
        return FINGERPRINT_ENROLLMISMATCH;
      }
      p = finger.getImage();
      switch (p) {
        case FINGERPRINT_OK:
          M5.Display.println("Image taken");
          break;
        case FINGERPRINT_NOFINGER:
          M5.Display.print(".");
          break;
        case FINGERPRINT_PACKETRECIEVEERR:
          M5.Display.println("Communication error");
          break;
        case FINGERPRINT_IMAGEFAIL:
          M5.Display.println("Imaging error");
          break;
        default:
          M5.Display.println("Unknown error");
          break;
      }
      delay(100);
    }

    if (p != FINGERPRINT_OK) {
      M5.Display.println("Timeout or error occurred");
      return p;
    }

    p = finger.image2Tz(2);
    if (p != FINGERPRINT_OK) {
      M5.Display.println("Image conversion failed");
      continue;
    }

    p = finger.createModel();
    if (p == FINGERPRINT_OK) {
      M5.Display.println("Prints matched!");
      p = finger.storeModel(id);
      if (p == FINGERPRINT_OK) {
        M5.Display.printf("Stored as ID %d\n", id);
        return id;
      } else {
        M5.Display.println("Error storing model");
      }
      return p;
    } else if (p == FINGERPRINT_ENROLLMISMATCH) {
      M5.Display.println("Fingerprints did not match");
      M5.Display.println("Please try again");
      attemptCount++;
    } else {
      M5.Display.println("Unknown error");
      return p;
    }
  }

  if (attemptCount >= maxAttempts) {
    M5.Display.println("Enrollment failed after maximum attempts");
    return FINGERPRINT_ENROLLMISMATCH;
  }

  return p;
}