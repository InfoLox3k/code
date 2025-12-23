#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Настройки UART для связи с ESP8266
#define ESP_RX 2  // D2 -> TX ESP
#define ESP_TX 3  // D3 -> RX ESP
#include <SoftwareSerial.h>
SoftwareSerial espSerial(ESP_RX, ESP_TX); // RX, TX

// Настройки дисплея LCD 1602 (I2C)
LiquidCrystal_I2C lcd(0x27, 16, 2); // Адрес 0x27, 16 символов, 2 строки

// Пин джойстика
#define JOYSTICK_X A0    // X-ось
#define JOYSTICK_Y A1    // Y-ось
#define JOYSTICK_BTN 4   // Кнопка джойстика

// Константы игры
const int STEPS_INIT = 0;
const int MAX_STEPS = 100;

// Переменные игры
int steps = STEPS_INIT;
String currentHint = "Жду подсказку";
String lastHint = "";
int hintNumber = 0;
bool gameWon = false;
unsigned long lastUpdate = 0;

// Переменные для джойстика
int joystickX = 0;
int joystickY = 0;
bool joystickBtnPressed = false;
bool lastJoystickBtnState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

// Направления
enum Direction {
  DIR_NONE,
  DIR_FORWARD,
  DIR_BACKWARD,
  DIR_LEFT,
  DIR_RIGHT,
  DIR_STOP
};

Direction selectedDirection = DIR_NONE;
Direction lastSentDirection = DIR_NONE;

void setup() {
  // Инициализация Serial для отладки
  Serial.begin(115200);
  
  // Инициализация UART для связи с ESP8266
  espSerial.begin(9600);
  
  // Инициализация LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  
  // Настройка пинов джойстика
  pinMode(JOYSTICK_BTN, INPUT_PULLUP);
  
  // Приветственное сообщение
  showWelcomeMessage();
  
  // Отправляем запрос на инициализацию
  delay(1000);
  espSerial.println("ARDUINO:READY");
  Serial.println("🚀 Arduino готов к игре!");
  Serial.println("📡 Ожидание подсказок от штурмана...");
}

void loop() {
  // Читаем команды от ESP8266
  checkESPCommands();
  
  // Обрабатываем ввод с джойстика
  readJoystick();
  
  // Обновляем дисплей каждые 500 мс
  if (millis() - lastUpdate > 500) {
    updateDisplay();
    lastUpdate = millis();
  }
  
  // Проверяем, нужно ли отправить движение
  if (selectedDirection != DIR_NONE && selectedDirection != lastSentDirection) {
    sendMovement(selectedDirection);
    lastSentDirection = selectedDirection;
  }
  
  // Запрашиваем статус у ESP каждые 5 секунд
  static unsigned long lastStatusRequest = 0;
  if (millis() - lastStatusRequest > 5000) {
    espSerial.println("STATUS");
    lastStatusRequest = millis();
  }
}

// ========== КОММУНИКАЦИЯ С ESP8266 ==========

void checkESPCommands() {
  if (espSerial.available()) {
    String message = espSerial.readStringUntil('\n');
    message.trim();
    
    if (message.length() > 0) {
      Serial.println("⬅️ От ESP: " + message);
      processESPCommand(message);
    }
  }
}

void processESPCommand(String command) {
  if (command.startsWith("HINT:")) {
    // Получена подсказка от штурмана
    String hintType = command.substring(5);
    
    if (hintType == "FWD") {
      currentHint = "ВПЕРЕД!";
      setDirection(DIR_FORWARD);
    } else if (hintType == "BCK") {
      currentHint = "НАЗАД!";
      setDirection(DIR_BACKWARD);
    } else if (hintType == "LFT") {
      currentHint = "НАЛЕВО!";
      setDirection(DIR_LEFT);
    } else if (hintType == "RGT") {
      currentHint = "НАПРАВО!";
      setDirection(DIR_RIGHT);
    } else if (hintType == "STP") {
      currentHint = "СТОП!";
      setDirection(DIR_STOP);
    } else if (hintType == "TRN") {
      currentHint = "ПОВЕРНИ!";
      // Для поворота не устанавливаем конкретное направление
      hintNumber++;
    }
    
    lastHint = currentHint;
    hintNumber++;
    
    Serial.println("📢 Подсказка: " + currentHint);
    
  } else if (command.startsWith("RESET")) {
    // Сброс игры
    resetGame();
    Serial.println("🔄 Игра сброшена");
    
  } else if (command.startsWith("WIN:YES")) {
    // Победа!
    gameWon = true;
    currentHint = "ПОБЕДА!";
    showWinAnimation();
    Serial.println("🎉 ПОБЕДА ДОСТИГНУТА!");
    
  } else if (command.startsWith("STATUS:")) {
    // Получен статус игры (для отладки)
    Serial.println("📊 Статус от ESP: " + command);
    
  } else if (command.startsWith("INIT:START")) {
    // Начало игры
    currentHint = "Начинаем!";
    resetGame();
    Serial.println("🎮 Начало игры!");
  }
}

void sendMovement(Direction dir) {
  String command = "MOVE:";
  
  switch (dir) {
    case DIR_FORWARD:
      command += "U";
      steps++;
      break;
    case DIR_BACKWARD:
      command += "D";
      steps++;
      break;
    case DIR_LEFT:
      command += "L";
      steps++;
      break;
    case DIR_RIGHT:
      command += "R";
      steps++;
      break;
    case DIR_STOP:
      command += "S";
      break;
    default:
      return;
  }
  
  espSerial.println(command);
  Serial.println("➡️ Отправлено ESP: " + command);
  
  // Сбрасываем выбранное направление после отправки
  selectedDirection = DIR_NONE;
  lastSentDirection = dir;
  
  // Обновляем подсказку
  currentHint = "Отправлено!";
}

// ========== УПРАВЛЕНИЕ ДЖОЙСТИКОМ ==========

void readJoystick() {
  // Чтение осей джойстика
  joystickX = analogRead(JOYSTICK_X);
  joystickY = analogRead(JOYSTICK_Y);
  
  // Определение направления по осям
  Direction joystickDir = getJoystickDirection();
  
  // Обновляем светодиодную индикацию
  updateDirectionLEDs(joystickDir);
  
  // Обработка кнопки джойстика с защитой от дребезга
  bool reading = digitalRead(JOYSTICK_BTN);
  
  if (reading != lastJoystickBtnState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != joystickBtnPressed) {
      joystickBtnPressed = reading;
      
      if (joystickBtnPressed == LOW) { // Кнопка нажата
        onJoystickButtonPressed(joystickDir);
      }
    }
  }
  
  lastJoystickBtnState = reading;
}

Direction getJoystickDirection() {
  int deadZone = 100;
  int centerMin = 512 - deadZone;
  int centerMax = 512 + deadZone;
  
  // Проверяем X ось (лево/право)
  if (joystickX < centerMin - 200) {
    return DIR_LEFT;
  } else if (joystickX > centerMax + 200) {
    return DIR_RIGHT;
  }
  
  // Проверяем Y ось (вперед/назад)
  if (joystickY < centerMin - 200) {
    return DIR_FORWARD;
  } else if (joystickY > centerMax + 200) {
    return DIR_BACKWARD;
  }
  
  return DIR_NONE;
}

void onJoystickButtonPressed(Direction dir) {
  Serial.println("🎮 Кнопка джойстика нажата!");
  
  if (gameWon) {
    // Если игра выиграна, сброс по нажатию кнопки
    espSerial.println("RESET_REQUEST");
    resetGame();
    return;
  }
  
  if (dir != DIR_NONE) {
    // Подтверждение выбранного направления
    selectedDirection = dir;
    
    Serial.print("✅ Подтверждено направление: ");
    switch (dir) {
      case DIR_FORWARD: Serial.println("ВПЕРЕД"); break;
      case DIR_BACKWARD: Serial.println("НАЗАД"); break;
      case DIR_LEFT: Serial.println("ВЛЕВО"); break;
      case DIR_RIGHT: Serial.println("ВПРАВО"); break;
      default: break;
    }
  } else {
    // Если джойстик в центре - отправляем команду СТОП
    selectedDirection = DIR_STOP;
    Serial.println("⏹️ Команда СТОП");
  }
}

// ========== ДИСПЛЕЙ LCD ==========

void showWelcomeMessage() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("BLIND LABIRINT");
  lcd.setCursor(0, 1);
  lcd.print("PILOT IS READY!");
  
  // Анимация приветствия
  for (int i = 0; i < 3; i++) {
    lcd.backlight();
    delay(300);
    lcd.noBacklight();
    delay(300);
    lcd.backlight();
  }
}

void updateDisplay() {
  lcd.clear();
  
  // Верхняя строка: шаги и подсказка номер
  lcd.setCursor(0, 0);
  lcd.print("Steps:");
  lcd.print(steps);
  lcd.print("/");
  lcd.print(MAX_STEPS);
  
  lcd.setCursor(11, 0);
  lcd.print("#");
  lcd.print(hintNumber);
  
  // Нижняя строка: текущая подсказка
  lcd.setCursor(0, 1);
  
  if (gameWon) {
    lcd.print("WIN!");
    return;
  }
  
  // Обрезаем подсказку, если она слишком длинная
  if (currentHint.length() > 16) {
    String displayHint = currentHint.substring(0, 16);
    lcd.print(displayHint);
  } else {
    lcd.print(currentHint);
  }
  
  // Мигание курсора для индикации активности
  static bool cursorVisible = true;
  if (cursorVisible) {
    lcd.cursor();
  } else {
    lcd.noCursor();
  }
  cursorVisible = !cursorVisible;
}

void showWinAnimation() {
  // Включаем светодиод победы
  
  // Анимация на дисплее
  for (int i = 0; i < 5; i++) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WIN!");
    lcd.setCursor(0, 1);
    lcd.print("Steps: ");
    lcd.print(steps);
    delay(500);
    
    lcd.clear();
    lcd.setCursor(4, 0);
    lcd.print("HORRAY!");
    lcd.setCursor(2, 1);
    lcd.print("EXIT FOUND");
    delay(500);
  }
}

// ========== ИГРОВАЯ ЛОГИКА ==========

void resetGame() {
  steps = STEPS_INIT;
  hintNumber = 0;
  gameWon = false;
  currentHint = "Жду подсказку";
  lastHint = "";
  selectedDirection = DIR_NONE;
  lastSentDirection = DIR_NONE;

  Serial.println("🔄 Игра сброшена");
}
