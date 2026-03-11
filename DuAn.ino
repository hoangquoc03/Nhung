#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <Servo.h>
#include <SoftwareSerial.h>

#define ESP_RX 10
#define ESP_TX 11

// ===== PIN =====
#define SERVO_PIN 9
#define BUZZER_PIN 7
#define PIR_PIN 6
#define LIGHT_PIN 5
SoftwareSerial esp(ESP_RX, ESP_TX);

// ===== LCD =====
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ===== SERVO =====
Servo myServo;

// ===== KEYPAD =====
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {A0, A1, A2, A3};
byte colPins[COLS] = {4, 3, 2, A4};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ===== SYSTEM =====
String password = "1234";
String inputPassword = "";
String newPassword = "";

String adminPin = "9999";
String inputAdminPin = "";
bool waitAdminPin = false;

bool doorOpened = false;   // false = đang đóng, true = đang mở

int cardFailCount = 0;          // số lần quẹt thẻ sai
unsigned long dynamicLockTime = 30000; // thời gian khóa ban đầu (30s)

unsigned long lastDisplayUpdate = 0;

int failCount = 0;
bool systemLocked = false;
unsigned long lockTime = 0;
const unsigned long LOCK_DURATION = 30000;

unsigned long doorOpenTime = 0;
bool warningIssued = false;

const unsigned long DOOR_WARNING_TIME = 10000; // 10s

const unsigned long DOOR_WARNING_INTERVAL = 5000; // 5s kêu lại
const unsigned long DOOR_AUTO_CLOSE_TIME = 30000; // 30s tự đóng

unsigned long lastWarningTime = 0;

unsigned long lastDoorCountdownUpdate = 0;

// ===== MENU =====
enum Mode { NORMAL, MENU, CHANGE_PASS, RESET_PASS, ADD_CARD, DELETE_CARD };
Mode currentMode = NORMAL;

int menuIndex = 0;
bool menuShown = false;

const char* menuItems[] = {
  "Doi mat khau",
  "Reset mat khau",
  "Them the",
  "Xoa the",
  "Thoat"
};
const int MENU_SIZE = 5;


// ===== FUNCTIONS =====
void savePassword() {
  for (int i = 0; i < 4; i++) EEPROM.write(i, password[i]);
}

void loadPassword() {
  password = "";
  for (int i = 0; i < 4; i++) {
    char c = EEPROM.read(i);
    if (c >= '0' && c <= '9') password += c;
    else { password = "1234"; break; }
  }
}

void toggleDoor() {
  lcd.clear();

  if (!doorOpened) {
    lcd.print("Mo cua...");
    myServo.write(90);
    doorOpened = true;

    doorOpenTime = millis();
    lastWarningTime = doorOpenTime;
    warningIssued = false;
    esp.write('1');
  } else {
    lcd.print("Dong cua...");
    myServo.write(0);
    doorOpened = false;
    esp.write('0');
    warningIssued = false;
  }

  tone(BUZZER_PIN, 1200, 200);
  delay(800);

  lcd.clear();
  lcd.print("Nhap MK hoac the");
}



void lockSystem() {
  systemLocked = true;
  lockTime = millis();

  lcd.clear();
  lcd.print("HE THONG KHOA");
  lcd.setCursor(0,1);
  lcd.print(dynamicLockTime / 1000);
  lcd.print(" giay");

  tone(BUZZER_PIN, 1000, 1500);
  myServo.write(0);
  doorOpened = false;
  esp.write('L'); 
}

void showMenu() {
  lcd.clear();
  lcd.print("> ");
  lcd.print(menuItems[menuIndex]);
}

// ===== ESP COMMAND =====
void handleESP(char c) {
  if (c == 'O') {                 // ✅ THẺ ĐÚNG
    failCount = 0;
    cardFailCount = 0;
    dynamicLockTime = 30000;      // reset về 30s
    systemLocked = false;
    toggleDoor();

  }

  else if (c == 'A') {            // 👑 ADMIN
    waitAdminPin = true;
    inputAdminPin = "";
    lcd.clear();
    lcd.print("Nhap PIN ADMIN");
  }

  else if (c == 'F') {            // ❌ THẺ SAI
    cardFailCount++;
    tone(BUZZER_PIN, 1500, 300);

    lcd.clear();
    lcd.print("The khong hop le");
    delay(1000);

    if (cardFailCount >= 3) {
      lockSystem();
      dynamicLockTime *= 2;       // ⏫ nhân đôi thời gian khóa
      cardFailCount = 0;
    } else {
      lcd.clear();
      lcd.print("Nhap MK hoac the");
    }
  }

  else if (c == 'S') {            // ESP báo OK (add/delete)
    lcd.clear();
    lcd.print("Thanh cong");
    delay(1500);
    currentMode = NORMAL;
    lcd.clear();
    lcd.print("Nhap MK hoac the");
  }
  else if (c == 'T') {   // 🌐 WEB TOGGLE
    if (systemLocked) return;   // không cho web khi khóa
    toggleDoor();
  }

}


// ===== SETUP =====
void setup() {
  Serial.begin(9600);
  esp.begin(9600);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(PIR_PIN, INPUT);
  pinMode(LIGHT_PIN, OUTPUT);

  myServo.attach(SERVO_PIN);
  myServo.write(0);

  lcd.init();
  lcd.backlight();

  loadPassword();
  lcd.print("Nhap MK hoac the");
}

// ===== LOOP =====
void loop() {
  bool hasInteraction = false;
  digitalWrite(LIGHT_PIN, digitalRead(PIR_PIN));

  if (esp.available()) {
    handleESP(esp.read());
    hasInteraction = true;
  }


  if (systemLocked) {
    unsigned long elapsed = millis() - lockTime;
    unsigned long remaining = (dynamicLockTime > elapsed)
                                ? (dynamicLockTime - elapsed)
                                : 0;

    // cập nhật LCD mỗi 1 giây
    if (millis() - lastDisplayUpdate >= 1000) {
      lastDisplayUpdate = millis();
      lcd.setCursor(0, 1);
      lcd.print("Con lai: ");
      lcd.print(remaining / 1000);
      lcd.print("s   ");
    }

    if (remaining == 0) {
      systemLocked = false;
      esp.write('0'); 
      failCount = 0;
      cardFailCount = 0;
      lcd.clear();
      lcd.print("Nhap MK hoac the");
    }
    return;
  }



  char key = keypad.getKey();
  if (key) hasInteraction = true;

  // ===== NHAP PIN ADMIN =====
  if (waitAdminPin) {
    if (key == '*') {
      inputAdminPin = "";
      lcd.setCursor(0,1);
      lcd.print("    ");
    }
    else if (isDigit(key) && inputAdminPin.length() < 4) {
      inputAdminPin += key;
      lcd.setCursor(0,1);
      for (int i = 0; i < inputAdminPin.length(); i++) lcd.print('*');
    }

    if (inputAdminPin.length() == 4) {
      if (inputAdminPin == adminPin) {
        waitAdminPin = false;
        currentMode = MENU;
        menuShown = false;
        lcd.clear();
        lcd.print("ADMIN MENU");
        delay(800);
      } else {
        lcd.clear();
        lcd.print("Sai PIN ADMIN");
        tone(BUZZER_PIN, 1000, 500);
        delay(1500);
        waitAdminPin = false;
        lcd.clear();
        lcd.print("Nhap MK hoac the");
      }
      inputAdminPin = "";
    }
    return;   // ⚠️ rất quan trọng
  }
  // ===== MENU =====
  if (currentMode == MENU) {
    if (!menuShown) {
      showMenu();
      menuShown = true;
    }

    if (key == '2') {
      menuIndex = (menuIndex + 1) % MENU_SIZE;
      showMenu();
    }
    else if (key == '8') {
      menuIndex = (menuIndex - 1 + MENU_SIZE) % MENU_SIZE;
      showMenu();
    }
    else if (key == '#') {
      if (menuIndex == 0) {
        currentMode = CHANGE_PASS;
        newPassword = "";
        lcd.clear();
        lcd.print("MK moi:");
      }
      else if (menuIndex == 1) {
        password = "1234";
        savePassword();
        lcd.clear();
        lcd.print("Da reset MK");
        delay(1500);
        currentMode = NORMAL;
      }
      else if (menuIndex == 2) {   // ✅ THÊM THẺ
        currentMode = ADD_CARD;
        lcd.clear();
        lcd.print("Quet the moi");
        esp.write('T');             // 👉 báo ESP vào chế độ thêm thẻ
      }
      else if (menuIndex == 3) {   // ✅ XÓA THẺ
        currentMode = DELETE_CARD;
        lcd.clear();
        lcd.print("Quet the xoa");
        esp.write('X');             // 👉 báo ESP vào chế độ xóa thẻ
      }
      else {
        currentMode = NORMAL;
        lcd.clear();
        lcd.print("Nhap MK hoac the");
      }
      menuShown = false;
    }
    return;
  }
  


  // ===== CHANGE PASS =====
  if (currentMode == CHANGE_PASS) {
    if (isDigit(key) && newPassword.length() < 4) {
      newPassword += key;
      lcd.setCursor(0,1);
      for (int i = 0; i < newPassword.length(); i++) lcd.print('*');
    }

    if (newPassword.length() == 4) {
      password = newPassword;
      savePassword();
      lcd.clear();
      lcd.print("Doi MK OK");
      delay(1500);
      currentMode = NORMAL;
      lcd.clear();
      lcd.print("Nhap MK hoac the");
    }
    return;
  }

  // ===== NORMAL MODE =====
  if (key == '*') {
    inputPassword = "";
    lcd.clear();
    lcd.print("Nhap MK hoac the");
  }
  else if (isDigit(key)) {
    inputPassword += key;
    lcd.setCursor(0,1);
    for (int i = 0; i < inputPassword.length(); i++) lcd.print('*');

    if (inputPassword.length() == 4) {
      if (inputPassword == password) {
        failCount = 0;
        dynamicLockTime = 30000;
        toggleDoor();
      } else {
        failCount++;
        lcd.clear();
        lcd.print("Sai MK ");
        tone(BUZZER_PIN, 1000, 500);
        delay(1000);
        if (failCount >= 3) {
          lockSystem();
          dynamicLockTime *= 2;   // ⏫ nhân đôi thời gian khóa
          failCount = 0;
        }
        else lcd.print("Nhap lai");
      }
      inputPassword = "";
    }
  }
  if (doorOpened) {
    unsigned long openDuration = millis() - doorOpenTime;
    unsigned long remaining = (DOOR_AUTO_CLOSE_TIME > openDuration)
                                ? (DOOR_AUTO_CLOSE_TIME - openDuration)
                                : 0;

    // ⏱ HIỂN THỊ ĐẾM NGƯỢC MỖI 1S
    if (millis() - lastDoorCountdownUpdate >= 1000) {
      lastDoorCountdownUpdate = millis();
      lcd.setCursor(0,0);
      lcd.print("Tu dong dong sau");
      lcd.setCursor(0,1);
      lcd.print(remaining / 1000);
      lcd.print(" s      ");
    }

    // 🔔 CẢNH BÁO MỖI 5 GIÂY SAU 10S
    if (openDuration >= DOOR_WARNING_TIME) {
      if (millis() - lastWarningTime >= DOOR_WARNING_INTERVAL) {
        lastWarningTime = millis();
        tone(BUZZER_PIN, 2000, 300);
      }
    }

    // 🔒 TỰ ĐÓNG CỬA SAU 30S
    if (remaining == 0) {
      lcd.clear();
      lcd.print("Dang dong cua");
      myServo.write(0);
      doorOpened = false;

      tone(BUZZER_PIN, 1500, 800);
      delay(500);

      lcd.clear();
      lcd.print("Nhap MK hoac the");
    }
  }
}
