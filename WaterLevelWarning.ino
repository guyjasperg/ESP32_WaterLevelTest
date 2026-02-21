#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>          //WiFiManager by tzapu
#include <UniversalTelegramBot.h> //UniversalTelegramBot by Brian Lough
#include <ArduinoJson.h>          //ArduinoJson by Benoit Blanchon
#include <Wire.h>
#include <Adafruit_GFX.h>         //Adafruit-GFX-Library by Adafruit
#include <Adafruit_SSD1306.h>     //Adafruit SSD1306 by Adafruit

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Credentials
// const char* ssid = "PLDT_Home_CE0A6";
// const char* password = "xxxxxxxxx";
#define BOTtoken "8022492830:AAGL-ZU95qbJUUg1YJRUxGEAdxQAkpTIPAQ"
#define CHANNEL_ID "@Raine_Broadcast"
#define ADMIN_ID "8430837982" // Your personal ID for secure polling

#define WATER_LEVEL_SENSOR A0   //Analog Pin where sensor is connected [ADC1_0 / A0 / GPIO36 / VP]
#define SENSOR_POWER_PIN 14   // power for the sensor

#define LED_RED     4
#define LED_YELLOW  16
#define LED_GREEN   17
#define BUZZER_PIN  23

#define LEVEL_LOW 15
#define LEVEL_MED 30

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

// Global Variables for 1-minute averaging
long intervalAccumulator = 0;
int readingCount = 0;
int lastIntervalAverage = 0;
unsigned long lastIntervalTimestamp = 0;
unsigned long lastTelegramNotif = 0;

void setup() {
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(SENSOR_POWER_PIN, OUTPUT);
  digitalWrite(SENSOR_POWER_PIN, LOW); // Keep it off by default
  TurnOffLED();

  Serial.begin(115200);
  
  // Initialize OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // 0x3C is common for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("System Booting...");
  display.display();

  // --- WiFiManager Logic ---
  WiFiManager wm;

  // Optional: Wipe settings for testing (Uncomment to reset saved WiFi)
  //wm.resetSettings();
  
  // REGISTER THE CALLBACK HERE
  wm.setAPCallback(configModeCallback);
 
  // This creates an AP named "ESP32_Config_Bot"
  // It blocks here until you configure the WiFi via your phone
  if(!wm.autoConnect("ESP32_Config_Bot")) {
      Serial.println("Failed to connect and hit timeout");
      ESP.restart();
  }

  Serial.println("WiFi Connected Successfully!");
  client.setInsecure();

  display.clearDisplay();
  display.setCursor(0,0);
  display.println("WiFi: Connected");
  display.println("Bot: Online");
  display.display();

  bot.sendMessage(CHANNEL_ID, "Bot Online.", "");
}

void loop() {
  // 1. Get the current stabilized raw reading (which already averages 20-30 samples)
  int rawValue = GetWaterLevel(); 

  /*
    - The Input (rawValue): This is the actual voltage your ESP32 is reading from its Analog-to-Digital Converter (ADC). 
      Since the ESP32 has a 12-bit ADC, it sees the world in values from 0 to 4095.
    - The Source Range (0, 4095): You are telling the code: "Expect the input to be somewhere between 0 and 4095."
    - The Target Range (0, 100): You are saying: "I want you to turn that big number into a percentage (0 to 100%)."  
  */
  int currentPercent = map(rawValue, 0, 4095, 0, 100);

  // 2. Add to our "Minute Accumulator"
  intervalAccumulator += currentPercent;
  readingCount++;

  // 3. Check if 10sec (10,000 ms) has passed
  if (millis() - lastIntervalTimestamp >= 10000) {
    if (readingCount > 0) {
      lastIntervalAverage = intervalAccumulator / readingCount;
    }

    Serial.printf("--- [Avg Interval Level] : %d ---\n", lastIntervalAverage);
    displayCenteredValue(lastIntervalAverage, currentPercent);           //update OLED display

    // 4. Reset for the next minute
    intervalAccumulator = 0;
    readingCount = 0;
    lastIntervalTimestamp = millis();

    //Interpret data
    TurnOffLED();
    if(lastIntervalAverage <= LEVEL_LOW)
    {
      //water level is low
      digitalWrite(LED_GREEN,HIGH);
    }
    else if(lastIntervalAverage <= LEVEL_MED)
    {
      //water level medium
      digitalWrite(LED_YELLOW,HIGH);
    }
    else
    {
      //above LEVEL_MED, water level is high
      digitalWrite(LED_RED,HIGH);
      
      //Send alert message through telegram!  
      SendAlertMessage();
      
      //sound Buzzer
      playAlarmSound();
    }
  }
  else
  {
    // Local debug every second
    Serial.printf("Current percent: %d | Samples collected: %d\n", currentPercent, readingCount);
    displayCenteredValue(lastIntervalAverage, currentPercent);
  }

  delay(1000);
}

int GetWaterLevel()
{
  int numReadings = 10; // Increase this for more smoothing
  long sum = 0;         // Use 'long' to prevent overflow during summation

  digitalWrite(SENSOR_POWER_PIN, HIGH); // Turn sensor ON
  delay(50);                            // Wait for voltage to stabilize (CRITICAL)

  for (int i = 0; i < numReadings; i++) {
    sum += analogRead(WATER_LEVEL_SENSOR);
    delay(10); // Small delay to let the ADC stabilize between reads
  }
  digitalWrite(SENSOR_POWER_PIN, LOW);  // Turn sensor OFF

  return (int)(sum / numReadings);
}

void SendAlertMessage()
{
  //send alert only every minute to avoid spamming
  if(lastTelegramNotif == 0 || (millis() - lastTelegramNotif >= 60000))
  {
    String alert = "🚨 WATER LEVEL CRITICAL! 🚨\nPlease consider evacuating to a higher place.";
    bot.sendMessage(CHANNEL_ID, alert, "");
    lastTelegramNotif = millis();
  }
}

//Turn off all LEDs
void TurnOffLED()
{
  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_GREEN, LOW);
}

void playAlarmSound() {
  // Repeat the cycle 3 times
  for (int count = 0; count < 3; count++) {
    // Slide frequency up
    for (int freq = 500; freq < 1500; freq += 10) {
      tone(BUZZER_PIN, freq);
      delay(1); 
    }
    // Slide frequency down
    for (int freq = 1500; freq > 500; freq -= 10) {
      tone(BUZZER_PIN, freq);
      delay(1);
    }
  }
  noTone(BUZZER_PIN); // Always turn it off at the end!
}

// This function runs ONLY when WiFiManager enters Configuration Mode
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  
  display.println("WiFi Setup Mode");
  display.println("");
  display.println("Connect to WiFi:");
  display.setTextColor(SSD1306_BLACK, SSD1306_WHITE); // Invert text for emphasis
  display.println(" ESP32_Config_Bot ");
  display.setTextColor(SSD1306_WHITE);
  display.println("");
  display.print("IP: ");
  display.println(WiFi.softAPIP()); // Usually 192.168.4.1
  
  display.display();
}

//Display data to the OLED display
void displayCenteredValue(int value, int rawValue) {
  display.clearDisplay();
  
  // 1. Draw a small header
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  String header = "SENSOR VALUE";
  int16_t hx1, hy1;
  uint16_t hw, hh;
  display.getTextBounds(header, 0, 0, &hx1, &hy1, &hw, &hh);
  display.setCursor((SCREEN_WIDTH - hw) / 2, 5);
  display.println(header);

  // 2. Format the value string (0-999)
  String valStr = String(value);
  
  // 3. Calculate bounds for the large font (Size 3)
  display.setTextSize(3);
  int16_t x1, y1;
  uint16_t w, h;
  
  // This function calculates exactly how many pixels wide the text is
  display.getTextBounds(valStr, 0, 0, &x1, &y1, &w, &h);
  
  // 4. Set cursor to the calculated center
  int xPos = (SCREEN_WIDTH - w) / 2;
  int yPos = 25; 
  
  display.setCursor(xPos, yPos);
  display.print(valStr);

  // 5. Raw Data (Bottom Left Small)
  display.setTextSize(1);
  // SCREEN_HEIGHT is 64, so 54 gives us a 10px margin from the bottom
  display.setCursor(0, 54); 
  display.print("*: ");
  display.print(rawValue);
  
  display.display();
}