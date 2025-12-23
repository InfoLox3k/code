#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SoftwareSerial.h>

// Настройки Wi-Fi
const char* ssid = "POCOPHONE F1";
const char* password = "babl1248";

// Настройки UART для связи с Arduino
#define ARDUINO_RX D5  // GPIO14 -> TX Arduino
#define ARDUINO_TX D6  // GPIO12 -> RX Arduino
SoftwareSerial arduinoSerial(ARDUINO_RX, ARDUINO_TX); // RX, TX

// Создаем веб-сервер на порту 80
ESP8266WebServer server(80);

// Константы игры
const int MAZE_WIDTH = 8;
const int MAZE_HEIGHT = 8;

// Лабиринт (1 - стена, 0 - путь, 2 - игрок, 3 - выход, 4 - пройденный путь)
int maze[MAZE_HEIGHT][MAZE_WIDTH] = {
  {1, 1, 1, 1, 1, 1, 1, 1},
  {1, 2, 0, 1, 0, 0, 0, 1},
  {1, 1, 0, 1, 0, 1, 0, 1},
  {1, 0, 0, 0, 0, 1, 0, 1},
  {1, 0, 1, 1, 1, 1, 0, 1},
  {1, 0, 0, 0, 0, 0, 0, 1},
  {1, 0, 1, 1, 1, 1, 0, 1},
  {1, 3, 1, 1, 1, 1, 1, 1}
};

// Позиция игрока
int playerX = 1;
int playerY = 1;

// Позиция выхода
int exitX = 7;
int exitY = 1;

// Последняя подсказка
String lastHint = "Двигайтесь к выходу!";
String currentDirection = "";
bool gameWon = false;

// Переменные для управления с Arduino
struct GameCommand {
  char type;      // 'M' - движение, 'S' - статус
  char direction; // 'U' - вверх, 'D' - вниз, 'L' - влево, 'R' - вправо
  bool valid;
};

GameCommand arduinoCommand = {' ', ' ', false};

void setup() {
  // Инициализация Serial для отладки
  Serial.begin(115200);
  delay(1000);
  
  // Инициализация UART для связи с Arduino
  arduinoSerial.begin(9600);
  Serial.println("📡 UART с Arduino инициализирован");
  
  // Подключаемся к Wi-Fi
  Serial.println();
  Serial.print("Подключаемся к ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  // Ждем подключения
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  // Выводим информацию о подключении
  Serial.println("");
  Serial.println("✅ Wi-Fi подключен!");
  Serial.print("🌐 IP-адрес: ");
  Serial.println(WiFi.localIP());
  
  // Настраиваем обработчики веб-сервера
  server.on("/", HTTP_GET, handleRoot);           // Главная страница игры
  server.on("/move", HTTP_POST, handleMove);      // Отправка подсказки
  server.on("/status", HTTP_GET, handleStatus);   // Статус игры
  server.on("/reset", HTTP_POST, handleReset);    // Сброс игры
  server.on("/arduino", HTTP_GET, handleArduino); // Интерфейс для Arduino
  
  // Запускаем сервер
  server.begin();
  Serial.println("🚀 HTTP сервер запущен");
  Serial.println("🎮 Игра 'Слепой лабиринт' готова!");
  Serial.println("📌 Откройте в браузере: http://" + WiFi.localIP().toString());
  
  // Отправляем начальное состояние Arduino
  sendToArduino("INIT:START");
}

void loop() {
  // Обрабатываем входящие запросы
  server.handleClient();
  
  // Проверяем команды от Arduino
  checkArduinoCommands();
  
  // Обрабатываем команды от Arduino
  processArduinoCommands();
  
  // Проверяем победу
  checkWinCondition();
}

// ========== ВЕБ-ИНТЕРФЕЙС ==========

void handleRoot() {
  String html = "<!DOCTYPE html>";
  html += "<html>";
  html += "<head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>🎮 Слепой лабиринт</title>";
  html += "<style>";
  html += "body { font-family: 'Arial', sans-serif; margin: 0; padding: 20px; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; }";
  html += ".container { max-width: 1000px; margin: 0 auto; }";
  html += ".game-container { display: flex; flex-wrap: wrap; gap: 30px; justify-content: center; }";
  html += ".panel { background: white; border-radius: 15px; padding: 25px; box-shadow: 0 10px 30px rgba(0,0,0,0.2); flex: 1; min-width: 300px; }";
  html += "h1 { color: white; text-align: center; margin-bottom: 30px; font-size: 2.5em; text-shadow: 2px 2px 4px rgba(0,0,0,0.3); }";
  html += "h2 { color: #333; margin-top: 0; border-bottom: 2px solid #667eea; padding-bottom: 10px; }";
  html += ".maze { border-collapse: collapse; margin: 20px auto; }";
  html += ".maze td { width: 40px; height: 40px; text-align: center; font-size: 20px; border: 1px solid #ddd; }";
  html += ".wall { background: #2c3e50; color: white; }";
  html += ".path { background: #ecf0f1; }";
  html += ".player { background: #e74c3c; color: white; border-radius: 50%; font-weight: bold; }";
  html += ".exit { background: #2ecc71; color: white; font-weight: bold; }";
  html += ".visited { background: #3498db; color: white; }";
  html += ".controls { display: grid; grid-template-columns: 1fr 1fr; gap: 15px; margin-top: 20px; }";
  html += ".control-btn { padding: 15px; font-size: 18px; border: none; border-radius: 10px; cursor: pointer; transition: all 0.3s; font-weight: bold; }";
  html += ".control-btn:hover { transform: translateY(-2px); box-shadow: 0 5px 15px rgba(0,0,0,0.2); }";
  html += ".up { background: #3498db; color: white; grid-column: 1 / span 2; }";
  html += ".down { background: #3498db; color: white; grid-column: 1 / span 2; }";
  html += ".left { background: #f39c12; color: white; }";
  html += ".right { background: #f39c12; color: white; }";
  html += ".hint-box { background: #f8f9fa; border: 2px dashed #667eea; border-radius: 10px; padding: 15px; margin: 20px 0; min-height: 60px; }";
  html += ".status { padding: 15px; border-radius: 10px; margin: 10px 0; text-align: center; font-weight: bold; }";
  html += ".connected { background: #d4edda; color: #155724; }";
  html += ".disconnected { background: #f8d7da; color: #721c24; }";
  html += ".message { padding: 10px; border-radius: 5px; margin: 10px 0; }";
  html += ".success { background: #d4edda; color: #155724; }";
  html += ".error { background: #f8d7da; color: #721c24; }";
  html += ".reset-btn { background: #e74c3c; color: white; padding: 12px 25px; border: none; border-radius: 10px; cursor: pointer; font-size: 16px; margin-top: 20px; }";
  html += ".arduino-info { background: #f1f1f1; padding: 15px; border-radius: 10px; margin-top: 20px; }";
  html += "</style>";
  html += "</head>";
  html += "<body>";
  html += "<div class='container'>";
  html += "<h1>🎮 СЛЕПОЙ ЛАБИРИНТ</h1>";
  html += "<div class='game-container'>";
  
  // Левая панель - карта и управление
  html += "<div class='panel'>";
  html += "<h2>🗺️ Карта лабиринта</h2>";
  html += "<div class='maze'>";
  html += generateMazeHTML();
  html += "</div>";
  html += "<div class='legend' style='text-align: center; margin: 15px 0;'>";
  html += "<span style='display: inline-block; width: 20px; height: 20px; background: #e74c3c; border-radius: 50%; margin-right: 10px;'></span> Игрок";
  html += "<span style='display: inline-block; width: 20px; height: 20px; background: #2ecc71; margin: 0 10px;'></span> Выход";
  html += "<span style='display: inline-block; width: 20px; height: 20px; background: #2c3e50; margin-left: 10px;'></span> Стена";
  html += "</div>";
  html += "</div>";
  
  // Правая панель - управление и информация
  html += "<div class='panel'>";
  html += "<h2>🎯 Управление штурмана</h2>";
  
  // Статус игры
  html += "<div class='status " + String(gameWon ? "connected" : "disconnected") + "'>";
  html += gameWon ? "🎉 ПОБЕДА! Игрок достиг выхода!" : "🕹️ Игра в процессе...";
  html += "</div>";
  
  // Текущая подсказка
  html += "<div class='hint-box'>";
  html += "<strong>📢 Текущая подсказка:</strong><br>";
  html += lastHint;
  html += "</div>";
  
  // Кнопки управления
  html += "<h3>📡 Отправить подсказку пилоту:</h3>";
  html += "<div class='controls'>";
  html += "<button class='control-btn up' onclick=\"sendHint('ВПЕРЕД')\">↑ ВПЕРЕД</button>";
  html += "<button class='control-btn left' onclick=\"sendHint('НАЛЕВО')\">← НАЛЕВО</button>";
  html += "<button class='control-btn right' onclick=\"sendHint('НАПРАВО')\">→ НАПРАВО</button>";
  html += "<button class='control-btn down' onclick=\"sendHint('НАЗАД')\">↓ НАЗАД</button>";
  html += "</div>";
  
  // Дополнительные команды
  html += "<div style='margin-top: 20px;'>";
  html += "<button class='control-btn' style='background: #9b59b6; width: 100%;' onclick=\"sendHint('СТОП')\">⏹️ СТОП</button>";
  html += "<button class='control-btn' style='background: #1abc9c; width: 100%; margin-top: 10px;' onclick=\"sendHint('ПОВЕРНИ')\">🔄 ПОВЕРНИ</button>";
  html += "</div>";
  
  // Информация об Arduino
  html += "<div class='arduino-info'>";
  html += "<h3>📱 Связь с пилотом (Arduino)</h3>";
  html += "<p><strong>Статус:</strong> <span id='arduinoStatus'>🟢 Связь установлена</span></p>";
  html += "<p><strong>Последняя команда:</strong> <span id='lastCommand'>" + currentDirection + "</span></p>";
  html += "<p><strong>Позиция игрока:</strong> X=" + String(playerX) + ", Y=" + String(playerY) + "</p>";
  html += "</div>";
  
  // Кнопка сброса
  html += "<button class='reset-btn' onclick=\"resetGame()\">🔄 Сбросить игру</button>";
  html += "</div>";
  html += "</div>"; // game-container
  
  // JavaScript
  html += "<script>";
  html += "function sendHint(direction) {";
  html += "  fetch('/move', {";
  html += "    method: 'POST',";
  html += "    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },";
  html += "    body: 'direction=' + direction";
  html += "  })";
  html += "  .then(response => response.text())";
  html += "  .then(data => {";
  html += "    alert('Подсказка отправлена: ' + direction);";
  html += "    location.reload();";
  html += "  });";
  html += "}";
  html += "function resetGame() {";
  html += "  if(confirm('Сбросить игру?')) {";
  html += "    fetch('/reset', { method: 'POST' })";
  html += "    .then(() => location.reload());";
  html += "  }";
  html += "}";
  html += "// Автообновление каждые 3 секунды";
  html += "setTimeout(() => location.reload(), 3000);";
  html += "</script>";
  html += "</div>";
  html += "</body>";
  html += "</html>";
  
  server.send(200, "text/html", html);
}

void handleMove() {
  if (server.hasArg("direction")) {
    currentDirection = server.arg("direction");
    
    // Формируем подсказку на основе направления
    if (currentDirection == "ВПЕРЕД") {
      lastHint = "Двигайтесь ВПЕРЕД к выходу!";
      sendToArduino("HINT:FWD");
    } else if (currentDirection == "НАЗАД") {
      lastHint = "Отходите НАЗАД!";
      sendToArduino("HINT:BCK");
    } else if (currentDirection == "НАЛЕВО") {
      lastHint = "Поверните НАЛЕВО!";
      sendToArduino("HINT:LFT");
    } else if (currentDirection == "НАПРАВО") {
      lastHint = "Поверните НАПРАВО!";
      sendToArduino("HINT:RGT");
    } else if (currentDirection == "СТОП") {
      lastHint = "СТОП! Остановитесь!";
      sendToArduino("HINT:STP");
    } else if (currentDirection == "ПОВЕРНИ") {
      lastHint = "ПОВЕРНИТЕ и осмотритесь!";
      sendToArduino("HINT:TRN");
    }
    
    server.send(200, "text/plain", "Подсказка отправлена: " + currentDirection);
  } else {
    server.send(400, "text/plain", "Ошибка: не указано направление");
  }
}

void handleStatus() {
  String json = "{";
  json += "\"playerX\":" + String(playerX) + ",";
  json += "\"playerY\":" + String(playerY) + ",";
  json += "\"exitX\":" + String(exitX) + ",";
  json += "\"exitY\":" + String(exitY) + ",";
  json += "\"gameWon\":" + String(gameWon ? "true" : "false") + ",";
  json += "\"lastHint\":\"" + lastHint + "\",";
  json += "\"currentDirection\":\"" + currentDirection + "\"";
  json += "}";
  
  server.send(200, "application/json", json);
}

void handleReset() {
  // Сбрасываем позицию игрока
  playerX = 1;
  playerY = 1;
  gameWon = false;
  lastHint = "Двигайтесь к выходу!";
  currentDirection = "";
  
  // Сбрасываем лабиринт
  for (int y = 0; y < MAZE_HEIGHT; y++) {
    for (int x = 0; x < MAZE_WIDTH; x++) {
      if (maze[y][x] == 4) { // Очищаем пройденный путь
        maze[y][x] = 0;
      }
    }
  }
  
  // Обновляем позицию игрока в лабиринте
  maze[1][1] = 2;
  
  // Отправляем команду сброса на Arduino
  sendToArduino("RESET");
  
  server.send(200, "text/plain", "Игра сброшена");
}

void handleArduino() {
  // Простой интерфейс для отладки Arduino
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
  html += "<title>Arduino Debug</title></head><body>";
  html += "<h1>Arduino Debug Interface</h1>";
  html += "<p>Отправка команд на Arduino:</p>";
  html += "<button onclick=\"sendCmd('FWD')\">Вперед</button>";
  html += "<button onclick=\"sendCmd('BCK')\">Назад</button>";
  html += "<button onclick=\"sendCmd('LFT')\">Влево</button>";
  html += "<button onclick=\"sendCmd('RGT')\">Вправо</button>";
  html += "<script>";
  html += "function sendCmd(cmd) {";
  html += "  fetch('/arduino?cmd=' + cmd);";
  html += "  alert('Отправлено: ' + cmd);";
  html += "}";
  html += "</script>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

// ========== ЛОГИКА ИГРЫ ==========

String generateMazeHTML() {
  String html = "";
  for (int y = 0; y < MAZE_HEIGHT; y++) {
    html += "<tr>";
    for (int x = 0; x < MAZE_WIDTH; x++) {
      String cellClass = "";
      String cellContent = "";
      
      if (x == playerX && y == playerY) {
        cellClass = "player";
        cellContent = "P";
      } else if (x == exitX && y == exitY) {
        cellClass = "exit";
        cellContent = "E";
      } else if (maze[y][x] == 1) {
        cellClass = "wall";
        cellContent = "█";
      } else if (maze[y][x] == 4) {
        cellClass = "visited";
        cellContent = "·";
      } else {
        cellClass = "path";
        cellContent = " ";
      }
      
      html += "<td class='" + cellClass + "'>" + cellContent + "</td>";
    }
    html += "</tr>";
  }
  return html;
}

bool isValidMove(int x, int y) {
  return x >= 0 && x < MAZE_WIDTH && 
         y >= 0 && y < MAZE_HEIGHT && 
         maze[y][x] != 1; // Не стена
}

void movePlayer(int dx, int dy) {
  int newX = playerX + dx;
  int newY = playerY + dy;
  
  if (isValidMove(newX, newY)) {
    // Помечаем старую позицию как посещенную
    if (maze[playerY][playerX] == 2) {
      maze[playerY][playerX] = 4;
    }
    
    // Обновляем позицию
    playerX = newX;
    playerY = newY;
    
    // Устанавливаем игрока в новой позиции
    maze[playerY][playerX] = 2;
    
    Serial.println("Игрок перемещен: X=" + String(playerX) + ", Y=" + String(playerY));
  } else {
    Serial.println("Невозможно переместиться: X=" + String(newX) + ", Y=" + String(newY));
  }
}

void checkWinCondition() {
  if (playerX == exitX && playerY == exitY && !gameWon) {
    gameWon = true;
    lastHint = "🎉 ПОБЕДА! Вы достигли выхода!";
    sendToArduino("WIN:YES");
    Serial.println("🎉 Игрок достиг выхода!");
  }
}

// ========== КОММУНИКАЦИЯ С ARDUINO ==========

void sendToArduino(String message) {
  arduinoSerial.println(message);
  Serial.println("➡️ Отправлено Arduino: " + message);
}

void checkArduinoCommands() {
  if (arduinoSerial.available()) {
    String received = arduinoSerial.readStringUntil('\n');
    received.trim();
    
    if (received.length() > 0) {
      Serial.println("⬅️ Получено от Arduino: " + received);
      
      // Обработка команд от Arduino
      if (received.startsWith("MOVE:")) {
        String dir = received.substring(5);
        arduinoCommand.type = 'M';
        arduinoCommand.direction = dir.charAt(0);
        arduinoCommand.valid = true;
      } else if (received.startsWith("STATUS")) {
        // Отправляем статус игры
        String status = "STATUS:P" + String(playerX) + "," + String(playerY) + 
                       ":E" + String(exitX) + "," + String(exitY) + 
                       ":W" + (gameWon ? "1" : "0");
        sendToArduino(status);
      }
    }
  }
}

void processArduinoCommands() {
  if (arduinoCommand.valid) {
    if (arduinoCommand.type == 'M') {
      // Движение от Arduino (пилот сам двигается)
      switch (arduinoCommand.direction) {
        case 'U': // Up/Forward
          movePlayer(0, -1);
          lastHint = "Пилот движется ВПЕРЕД";
          break;
        case 'D': // Down/Back
          movePlayer(0, 1);
          lastHint = "Пилот движется НАЗАД";
          break;
        case 'L': // Left
          movePlayer(-1, 0);
          lastHint = "Пилот движется ВЛЕВО";
          break;
        case 'R': // Right
          movePlayer(1, 0);
          lastHint = "Пилот движется ВПРАВО";
          break;
      }
    }
    
    arduinoCommand.valid = false;
  }
}
