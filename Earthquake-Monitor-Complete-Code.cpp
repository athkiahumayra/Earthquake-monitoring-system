#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ============================================================
// WIFI
// ============================================================

const char* WIFI_SSID     = "ACL-2";
const char* WIFI_PASSWORD = "luacl27@";

// ============================================================
// SMS API - SMSinBD
// ============================================================

const char* SMS_API_URL =
  "https://login.smsinbd.com/api/external/v1/sms/send";

const char* SMS_API_KEY =
  "smsk_your_api_key_here";

// SMS receiver
const char* SMS_PHONE =
  "+8801711000000";

// Enable / Disable SMS
bool SMS_ENABLED = true;

// ============================================================
// WHATSAPP API
// ============================================================

const char* WHATSAPP_API_URL =
  "https://whatsapp-api-w0jb.onrender.com/send";

// Put your WhatsApp API key here
const char* WHATSAPP_API_KEY =
  "YOUR_API_KEY";

// WhatsApp receiver
//
// Use the format expected by your WhatsApp API.
// Multiple numbers can be separated by comma.
const char* WHATSAPP_NUMBER =
  "8801711000000";

// Enable / Disable WhatsApp
bool WHATSAPP_ENABLED = true;

// ============================================================
// HARDWARE PINS
// ============================================================

#define BUZZER_PIN 25
#define LED_PIN    26

// ============================================================
// I2C
// ============================================================

#define SDA_PIN 21
#define SCL_PIN 22

// ============================================================
// LCD
// ============================================================

#define LCD_ADDRESS 0x27
#define LCD_COLUMNS 16
#define LCD_ROWS    2

LiquidCrystal_I2C lcd(
  LCD_ADDRESS,
  LCD_COLUMNS,
  LCD_ROWS
);

// ============================================================
// ADXL345
// ============================================================

#define ADXL345_ADDRESS 0x53

#define ADXL_REG_DEVID     0x00
#define ADXL_REG_DATA_FMT  0x31
#define ADXL_REG_POWER_CTL 0x2D
#define ADXL_REG_DATA_X0   0x32

// ============================================================
// SHAKE SETTINGS
// ============================================================

// Total acceleration threshold
//
// Normal stationary ADXL345 should be around:
//
// X/Y = depending on orientation
// Z   = approximately 1g
// Acc = approximately 1g
//
// A shake will produce rapidly changing acceleration.
//
// Increase this if false alarms occur.
const float SHAKE_THRESHOLD = 1.50;

// Number of consecutive detections
const int SHAKE_CONFIRM_COUNT = 3;

// ============================================================
// ALERT SETTINGS
// ============================================================

// Minimum time between alert messages
const unsigned long ALERT_COOLDOWN = 60000;

// Alarm duration
const unsigned long ALARM_DURATION = 5000;

// ============================================================
// VARIABLES
// ============================================================

float ax = 0.0;
float ay = 0.0;
float az = 0.0;

float acceleration = 0.0;

int shakeCount = 0;

bool shakeDetected = false;

unsigned long lastAlertTime = 0;

// ============================================================
// ADXL345 WRITE
// ============================================================

void writeADXLRegister(byte reg, byte value)
{
  Wire.beginTransmission(ADXL345_ADDRESS);

  Wire.write(reg);
  Wire.write(value);

  byte error = Wire.endTransmission();

  if (error != 0)
  {
    Serial.print("ADXL write error: ");
    Serial.println(error);
  }
}

// ============================================================
// ADXL345 READ
// ============================================================

byte readADXLRegister(byte reg)
{
  Wire.beginTransmission(ADXL345_ADDRESS);

  Wire.write(reg);

  byte error = Wire.endTransmission(false);

  if (error != 0)
  {
    Serial.print("ADXL read request error: ");
    Serial.println(error);

    return 0;
  }

  Wire.requestFrom(
    ADXL345_ADDRESS,
    (byte)1
  );

  if (Wire.available())
  {
    return Wire.read();
  }

  return 0;
}

// ============================================================
// INITIALIZE ADXL345
// ============================================================

bool initADXL345()
{
  Serial.println();
  Serial.println("Testing ADXL345...");

  byte deviceID =
    readADXLRegister(ADXL_REG_DEVID);

  Serial.print("ADXL345 Device ID: 0x");
  Serial.println(deviceID, HEX);

  if (deviceID != 0xE5)
  {
    Serial.println(
      "ERROR: ADXL345 not detected!"
    );

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("ADXL ERROR!");

    lcd.setCursor(0, 1);
    lcd.print("Check wiring");

    return false;
  }

  // Full resolution
  // +/- 16g
  writeADXLRegister(
    ADXL_REG_DATA_FMT,
    0x0B
  );

  // Measurement mode
  writeADXLRegister(
    ADXL_REG_POWER_CTL,
    0x08
  );

  Serial.println(
    "ADXL345 initialized successfully."
  );

  return true;
}

// ============================================================
// READ ADXL345
// ============================================================

bool readADXL345()
{
  Wire.beginTransmission(ADXL345_ADDRESS);

  Wire.write(ADXL_REG_DATA_X0);

  byte error =
    Wire.endTransmission(false);

  if (error != 0)
  {
    Serial.print(
      "ADXL data request error: "
    );

    Serial.println(error);

    return false;
  }

  Wire.requestFrom(
    ADXL345_ADDRESS,
    (byte)6
  );

  if (Wire.available() < 6)
  {
    Serial.println(
      "ERROR: ADXL345 data unavailable!"
    );

    return false;
  }

  int16_t x =
    Wire.read() |
    (Wire.read() << 8);

  int16_t y =
    Wire.read() |
    (Wire.read() << 8);

  int16_t z =
    Wire.read() |
    (Wire.read() << 8);

  // ADXL345 full-resolution:
  // approximately 3.9 mg/LSB

  ax = x * 0.0039;
  ay = y * 0.0039;
  az = z * 0.0039;

  // Total acceleration
  acceleration =
    sqrt(
      ax * ax +
      ay * ay +
      az * az
    );

  return true;
}

// ============================================================
// CONNECT WIFI
// ============================================================

void connectWiFi()
{
  Serial.println();
  Serial.println("Connecting to Wi-Fi...");

  WiFi.mode(WIFI_STA);

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );

  int attempts = 0;

  while (
    WiFi.status() != WL_CONNECTED &&
    attempts < 30
  )
  {
    delay(500);

    Serial.print(".");

    attempts++;
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println(
      "Wi-Fi connected!"
    );

    Serial.print(
      "IP Address: "
    );

    Serial.println(
      WiFi.localIP()
    );

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected");

    lcd.setCursor(0, 1);
    lcd.print("System Ready");

    delay(1500);
  }
  else
  {
    Serial.println(
      "ERROR: Wi-Fi connection failed!"
    );

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("WiFi ERROR!");

    lcd.setCursor(0, 1);
    lcd.print("No Internet");
  }
}

// ============================================================
// CHECK WIFI
// ============================================================

bool checkWiFi()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    return true;
  }

  Serial.println(
    "Wi-Fi disconnected."
  );

  Serial.println(
    "Attempting reconnect..."
  );

  WiFi.disconnect();

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );

  unsigned long start =
    millis();

  while (
    WiFi.status() != WL_CONNECTED &&
    millis() - start < 10000
  )
  {
    delay(250);

    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() ==
      WL_CONNECTED)
  {
    Serial.println(
      "Wi-Fi reconnected!"
    );

    return true;
  }

  Serial.println(
    "Wi-Fi reconnect failed."
  );

  return false;
}

// ============================================================
// CREATE ALERT MESSAGE
// ============================================================

String createAlertMessage()
{
  String message =
    "ALERT! Shake detected. ";

  message += "X=";
  message += String(ax, 2);
  message += "g ";

  message += "Y=";
  message += String(ay, 2);
  message += "g ";

  message += "Z=";
  message += String(az, 2);
  message += "g ";

  message += "Acc=";
  message += String(acceleration, 2);
  message += "g";

  return message;
}

// ============================================================
// SEND SMS
// ============================================================

bool sendSMS(String message)
{
  if (!SMS_ENABLED)
  {
    Serial.println(
      "[SMS] DISABLED"
    );

    return false;
  }

  if (!checkWiFi())
  {
    Serial.println(
      "[SMS] ERROR: Wi-Fi unavailable"
    );

    return false;
  }

  HTTPClient http;

  Serial.println();
  Serial.println(
    "========== SMS =========="
  );

  Serial.println(
    "[SMS] Connecting to API..."
  );

  http.begin(SMS_API_URL);

  // SMSinBD authentication
  http.addHeader(
    "X-API-KEY",
    SMS_API_KEY
  );

  http.addHeader(
    "Content-Type",
    "application/json"
  );

  // JSON body
  String jsonBody =
    "{\"phone\":\"" +
    String(SMS_PHONE) +
    "\",\"message\":\"" +
    message +
    "\"}";

  Serial.print(
    "[SMS] Phone: "
  );

  Serial.println(
    SMS_PHONE
  );

  Serial.print(
    "[SMS] JSON: "
  );

  Serial.println(
    jsonBody
  );

  int httpCode =
    http.POST(jsonBody);

  Serial.print(
    "[SMS] HTTP Code: "
  );

  Serial.println(
    httpCode
  );

  if (httpCode > 0)
  {
    String response =
      http.getString();

    Serial.println(
      "[SMS] API Response:"
    );

    Serial.println(
      response
    );

    if (
      httpCode >= 200 &&
      httpCode < 300
    )
    {
      Serial.println(
        "[SMS] SUCCESS"
      );

      http.end();

      return true;
    }
    else
    {
      Serial.println(
        "[SMS] FAILED - API ERROR"
      );
    }
  }
  else
  {
    Serial.print(
      "[SMS] HTTP ERROR: "
    );

    Serial.println(
      http.errorToString(
        httpCode
      )
    );
  }

  http.end();

  return false;
}

// ============================================================
// SEND WHATSAPP
// ============================================================

bool sendWhatsApp(String message)
{
  if (!WHATSAPP_ENABLED)
  {
    Serial.println(
      "[WhatsApp] DISABLED"
    );

    return false;
  }

  if (!checkWiFi())
  {
    Serial.println(
      "[WhatsApp] ERROR: Wi-Fi unavailable"
    );

    return false;
  }

  HTTPClient http;

  Serial.println();
  Serial.println(
    "======= WHATSAPP ========"
  );

  Serial.println(
    "[WhatsApp] Connecting to API..."
  );

  http.begin(
    WHATSAPP_API_URL
  );

  // API key
  http.addHeader(
    "x-api-key",
    WHATSAPP_API_KEY
  );

  http.addHeader(
    "Content-Type",
    "application/json"
  );

  /*
    WhatsApp API format supplied earlier:

    [
      {
        "numbers": "12345678,87654321",
        "message": "Hello batch!"
      }
    ]
  */

  String jsonBody =
    "[{\"numbers\":\"" +
    String(WHATSAPP_NUMBER) +
    "\",\"message\":\"" +
    message +
    "\"}]";

  Serial.print(
    "[WhatsApp] Number: "
  );

  Serial.println(
    WHATSAPP_NUMBER
  );

  Serial.print(
    "[WhatsApp] JSON: "
  );

  Serial.println(
    jsonBody
  );

  int httpCode =
    http.POST(jsonBody);

  Serial.print(
    "[WhatsApp] HTTP Code: "
  );

  Serial.println(
    httpCode
  );

  if (httpCode > 0)
  {
    String response =
      http.getString();

    Serial.println(
      "[WhatsApp] API Response:"
    );

    Serial.println(
      response
    );

    if (
      httpCode >= 200 &&
      httpCode < 300
    )
    {
      Serial.println(
        "[WhatsApp] SUCCESS"
      );

      http.end();

      return true;
    }
    else
    {
      Serial.println(
        "[WhatsApp] FAILED - API ERROR"
      );
    }
  }
  else
  {
    Serial.print(
      "[WhatsApp] HTTP ERROR: "
    );

    Serial.println(
      http.errorToString(
        httpCode
      )
    );
  }

  http.end();

  return false;
}

// ============================================================
// DISPLAY NORMAL
// ============================================================

void displayNormal()
{
  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print("X:");
  lcd.print(ax, 1);

  lcd.print(" Y:");
  lcd.print(ay, 1);

  lcd.setCursor(0, 1);

  lcd.print("Z:");
  lcd.print(az, 1);

  lcd.print(" A:");
  lcd.print(acceleration, 1);
}

// ============================================================
// DISPLAY SHAKE
// ============================================================

void displayShake()
{
  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print("!!! SHAKE !!!");

  lcd.setCursor(0, 1);

  lcd.print("Acc:");
  lcd.print(acceleration, 2);
  lcd.print("g");
}

// ============================================================
// ALARM ON
// ============================================================

void alarmOn()
{
  digitalWrite(
    LED_PIN,
    HIGH
  );

  digitalWrite(
    BUZZER_PIN,
    HIGH
  );
}

// ============================================================
// ALARM OFF
// ============================================================

void alarmOff()
{
  digitalWrite(
    LED_PIN,
    LOW
  );

  digitalWrite(
    BUZZER_PIN,
    LOW
  );
}

// ============================================================
// SEND ALL ALERTS
// ============================================================

void sendAllAlerts()
{
  unsigned long now =
    millis();

  // Cooldown
  if (
    lastAlertTime != 0 &&
    now - lastAlertTime <
      ALERT_COOLDOWN
  )
  {
    unsigned long remaining =
      (ALERT_COOLDOWN -
       (now - lastAlertTime)) /
      1000;

    Serial.println();
    Serial.println(
      "Alert cooldown active."
    );

    Serial.print(
      "Remaining: "
    );

    Serial.print(
      remaining
    );

    Serial.println(
      " seconds"
    );

    return;
  }

  String message =
    createAlertMessage();

  Serial.println();
  Serial.println(
    "********************************"
  );

  Serial.println(
    "SENDING ALERTS"
  );

  Serial.print(
    "Message: "
  );

  Serial.println(
    message
  );

  Serial.println(
    "********************************"
  );

  // ----------------------------------------------------------
  // SMS
  // ----------------------------------------------------------

  bool smsResult =
    sendSMS(message);

  // ----------------------------------------------------------
  // WhatsApp
  // ----------------------------------------------------------

  bool whatsappResult =
    sendWhatsApp(message);

  // ----------------------------------------------------------
  // Results
  // ----------------------------------------------------------

  Serial.println();
  Serial.println(
    "========= ALERT RESULT ========="
  );

  Serial.print(
    "SMS: "
  );

  if (smsResult)
  {
    Serial.println("SUCCESS");
  }
  else
  {
    if (SMS_ENABLED)
      Serial.println("FAILED");
    else
      Serial.println("DISABLED");
  }

  Serial.print(
    "WhatsApp: "
  );

  if (whatsappResult)
  {
    Serial.println("SUCCESS");
  }
  else
  {
    if (WHATSAPP_ENABLED)
      Serial.println("FAILED");
    else
      Serial.println("DISABLED");
  }

  Serial.println(
    "================================"
  );

  // Start cooldown regardless
  // of individual API result.
  lastAlertTime =
    millis();
}

// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println(
    "======================================"
  );

  Serial.println(
    "ESP32 SHAKE ALERT SYSTEM"
  );

  Serial.println(
    "SMS + WHATSAPP"
  );

  Serial.println(
    "======================================"
  );

  // ----------------------------------------------------------
  // GPIO
  // ----------------------------------------------------------

  pinMode(
    BUZZER_PIN,
    OUTPUT
  );

  pinMode(
    LED_PIN,
    OUTPUT
  );

  alarmOff();

  // ----------------------------------------------------------
  // I2C
  // ----------------------------------------------------------

  Wire.begin(
    SDA_PIN,
    SCL_PIN
  );

  // ----------------------------------------------------------
  // LCD
  // ----------------------------------------------------------

  lcd.init();

  lcd.backlight();

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Shake Detector");

  lcd.setCursor(0, 1);
  lcd.print("Starting...");

  delay(1500);

  // ----------------------------------------------------------
  // ADXL345
  // ----------------------------------------------------------

  if (!initADXL345())
  {
    Serial.println(
      "ADXL345 initialization failed."
    );

    while (true)
    {
      digitalWrite(
        LED_PIN,
        !digitalRead(LED_PIN)
      );

      delay(500);
    }
  }

  // ----------------------------------------------------------
  // Wi-Fi
  // ----------------------------------------------------------

  connectWiFi();

  // ----------------------------------------------------------
  // Configuration
  // ----------------------------------------------------------

  Serial.println();
  Serial.println(
    "========== CONFIGURATION =========="
  );

  Serial.print(
    "SMS: "
  );

  Serial.println(
    SMS_ENABLED ? "ON" : "OFF"
  );

  Serial.print(
    "WhatsApp: "
  );

  Serial.println(
    WHATSAPP_ENABLED ? "ON" : "OFF"
  );

  Serial.print(
    "Shake threshold: "
  );

  Serial.print(
    SHAKE_THRESHOLD
  );

  Serial.println(
    " g"
  );

  Serial.print(
    "Confirmation count: "
  );

  Serial.println(
    SHAKE_CONFIRM_COUNT
  );

  Serial.println(
    "==================================="
  );

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("System Ready");

  lcd.setCursor(0, 1);

  if (
    SMS_ENABLED &&
    WHATSAPP_ENABLED
  )
  {
    lcd.print("SMS+WA: ON");
  }
  else if (SMS_ENABLED)
  {
    lcd.print("SMS: ON");
  }
  else if (WHATSAPP_ENABLED)
  {
    lcd.print("WA: ON");
  }
  else
  {
    lcd.print("Alerts OFF");
  }

  delay(1500);

  Serial.println();
  Serial.println(
    "System ready."
  );
}

// ============================================================
// LOOP
// ============================================================

void loop()
{
  // ----------------------------------------------------------
  // Read ADXL345
  // ----------------------------------------------------------

  if (!readADXL345())
  {
    Serial.println(
      "ERROR: Failed to read ADXL345."
    );

    delay(500);

    return;
  }

  // ----------------------------------------------------------
  // Serial readings
  // ----------------------------------------------------------

  Serial.print("X: ");
  Serial.print(ax, 2);

  Serial.print("   Y: ");
  Serial.print(ay, 2);

  Serial.print("   Z: ");
  Serial.print(az, 2);

  Serial.print("   Acceleration: ");
  Serial.print(acceleration, 2);

  Serial.println(" g");

  // ----------------------------------------------------------
  // SHAKE DETECTION
  // ----------------------------------------------------------

  if (
    acceleration >=
    SHAKE_THRESHOLD
  )
  {
    shakeCount++;

    Serial.print(
      "Shake confirmation: "
    );

    Serial.print(
      shakeCount
    );

    Serial.print("/");
    Serial.println(
      SHAKE_CONFIRM_COUNT
    );

    if (
      shakeCount >=
      SHAKE_CONFIRM_COUNT
    )
    {
      shakeDetected = true;

      shakeCount = 0;
    }
  }
  else
  {
    // Gradually reduce count
    // to avoid false triggering.

    if (shakeCount > 0)
    {
      shakeCount--;
    }
  }

  // ----------------------------------------------------------
  // SHAKE EVENT
  // ----------------------------------------------------------

  if (shakeDetected)
  {
    Serial.println();
    Serial.println(
      "**************************************"
    );

    Serial.println(
      "*** SHAKE ALARM TRIGGERED! ***"
    );

    Serial.println(
      "**************************************"
    );

    // Alarm
    alarmOn();

    // LCD
    displayShake();

    // Send SMS + WhatsApp
    sendAllAlerts();

    // Keep alarm active
    delay(ALARM_DURATION);

    // Turn alarm off
    alarmOff();

    shakeDetected = false;

    Serial.println(
      "Alarm cleared."
    );

    Serial.println();
  }
  else
  {
    // Normal LCD
    displayNormal();
  }

  delay(100);
}
