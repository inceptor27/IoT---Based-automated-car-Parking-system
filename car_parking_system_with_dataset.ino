#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <FirebaseESP32.h>  // Firebase ESP32 library
#include <HTTPClient.h>

// Create an LCD object with I2C address 0x27, 16 columns, and 4 rows
LiquidCrystal_I2C lcd(0x27, 16, 4);

// Create a Servo object
Servo myservo;

// Define pin connections for sensors and servos
#define ir_enter 14
#define ir_back  4
#define ir_car1 5   // First car slot sensor
#define ir_car2 18  // Second car slot sensor
#define ir_car3 19  // Third car slot sensor
#define servo_pin 13  // Define the servo pin, adjust as per your connection

int S1 = 0, S2 = 0, S3 = 0;
int flag1 = 0, flag2 = 0;
int slot = 3;  // Total number of slots is now 3
const char* ssid = "Hrithik's iPhone";
const char* password = "12345677";
const char* firebaseDatabaseURL = "https://parking-8f5fc-default-rtdb.asia-southeast1.firebasedatabase.app/";  // Update with your Firebase URL

void setup() {
  Serial.begin(115200);  // Use 115200 baud rate for ESP32

  // Set up pin modes for sensors
  pinMode(ir_car1, INPUT_PULLUP);
  pinMode(ir_car2, INPUT_PULLUP);
  pinMode(ir_car3, INPUT_PULLUP);
  pinMode(ir_enter, INPUT_PULLUP);
  pinMode(ir_back, INPUT_PULLUP);

  // Attach servo to its pin
  myservo.attach(servo_pin);
  myservo.write(90);  // Initial position of the servo

  // Initialize the LCD with 16 columns and 4 rows
  lcd.begin();
  lcd.setCursor(0, 1);
  lcd.print(" Car Parking ");
  lcd.setCursor(0, 2);
  lcd.print("   System    ");
  delay(2000);
  lcd.clear();

  // Read initial sensor values
  Read_Sensor();
  updateSlotAvailability();
  connectToWiFi();
}

void loop() {
  Read_Sensor();
  updateSlotAvailability();

  // Display available slots on the first line
  lcd.setCursor(0, 0);
  lcd.print("Have Slot: ");
  lcd.print(slot);
  lcd.print("   ");

  // Display the status of each slot on the second line and scroll it
  String slotStatus = "S1:" + String(S1 ? "Fill " : "Empty ") + "S2:" + String(S2 ? "Fill " : "Empty ") + "S3:" + String(S3 ? "Fill " : "Empty ");
  lcd.setCursor(0, 1);
  lcd.print(slotStatus);

  // Scroll the status message
  for (int i = 0; i < slotStatus.length() - 15; i++) {
    lcd.scrollDisplayLeft();  // Scroll the text to the left
    delay(300);  // Delay for smooth scrolling
  }

  // Entry gate logic
  if (digitalRead(ir_enter) == LOW && flag1 == 0) {
    if (slot > 0) {  // Only allow entry if there are slots available
      flag1 = 1;
      if (flag2 == 0) {
        myservo.write(180);  // Open the gate
        slot = slot - 1;  // Decrease available slots
        sendToFirebase(S1 ? "ir1" : S2 ? "ir2" : "ir3", String(slot), slot);
      }
    } else {
      lcd.setCursor(0, 0);
      lcd.print(" Parking Full ");
      delay(1500);  // Display the "Parking Full" message
    }
  }

  // Exit gate logic
  if (digitalRead(ir_back) == LOW && flag2 == 0) {
    flag2 = 1;
    if (flag1 == 0) {
      myservo.write(180);  // Open the gate
      slot = slot + 1;  // Increase available slots
      sendToFirebase("Free", String(slot), slot);
    }
  }

  // Reset flags and close gate
  if (flag1 == 1 && flag2 == 1) {
    delay(1000);  // Delay to simulate gate operation
    myservo.write(90);  // Close the gate
    flag1 = 0;
    flag2 = 0;
  }

  delay(100);  // Short delay to avoid rapid looping
}

// Function to read sensor states
void Read_Sensor() {
  // Update sensor values based on readings
  S1 = (digitalRead(ir_car1) == LOW) ? 1 : 0;
  S2 = (digitalRead(ir_car2) == LOW) ? 1 : 0;
  S3 = (digitalRead(ir_car3) == LOW) ? 1 : 0;
}

// Update slot availability and send data to Firebase
void updateSlotAvailability() {
  if (S1 && S2 && S3) {
    slot = 0;
    sendToFirebase("All Full", String(slot), slot);
  } else if (!S1 && !S2 && !S3) {
    slot = 3;
    sendToFirebase("Free", String(slot), slot);
  } else {
    slot = 3 - (S1 + S2 + S3);  // Update the available slot count
    sendToFirebase(S1 ? "ir1" : S2 ? "ir2" : "ir3", String(slot), slot);
  }
}

// Connect to WiFi
void connectToWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi");
}

// Send data to Firebase Realtime Database
void sendToFirebase(String slot, String status, int availableSlots) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    // URL for the specific slot node
    String url = String(firebaseDatabaseURL) + "/slots/" + slot + ".json";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    // JSON payload for the slot status
    String jsonPayload = "{\"status\":\"" + status + "\",\"sensor\":\"" + slot + "\"}";
    int httpResponseCode = http.PATCH(jsonPayload);

    // Check response
    if (httpResponseCode > 0) {
      Serial.print("Slot update HTTP Response code: ");
      Serial.println(httpResponseCode);
    } else {
      Serial.print("Error on sending PATCH for slot: ");
      Serial.println(httpResponseCode);
    }
    http.end();

    // Update the availability node
    http.begin(String(firebaseDatabaseURL) + "/slots/availability.json");
    http.addHeader("Content-Type", "application/json");
    jsonPayload = "{\"availableSlots\":" + String(availableSlots) + "}";
    httpResponseCode = http.PATCH(jsonPayload);

    if (httpResponseCode > 0) {
      Serial.print("Availability update HTTP Response code: ");
      Serial.println(httpResponseCode);
    } else {
      Serial.print("Error on sending PATCH for availability: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("Error in WiFi connection");
  }
}
