#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include <Servo.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Provide the token generation process info.
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// =====================================================================
// WIFI & FIREBASE CONFIGURATION
// =====================================================================
#define WIFI_SSID "Clickys"
#define WIFI_PASSWORD "Dollagetta#2003"
#define API_KEY "AIzaSyCFH_hNAqgKBvbAXpD9vJ5zYOAXD_J3CPQ"
#define DATABASE_URL "smartparking-3f3b9-default-rtdb.asia-southeast1.firebasedatabase.app"

// =====================================================================
// PIN CONFIGURATION
// =====================================================================
#define IR1 D0
#define IR2 D1
#define IR3 D2
#define IR4 D3

#define TRIG_PIN D5
#define ECHO_PIN D6
#define SERVO_PIN D7

// =====================================================================
// FIREBASE OBJECTS & NTP
// =====================================================================
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
bool signupOK = false;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

Servo gateServo;

// =====================================================================
// SYSTEM STATE VARIABLES
// =====================================================================
int nextCarNumber = 1;
int currentEntranceCarNumber = 0;
String currentEvent = "WAITING_FOR_CAR";

// IR status
bool bay1Occupied = false;
bool bay2Occupied = false;
bool bay3Occupied = false;
bool bay4Occupied = false;

bool lastBay1 = false;
bool lastBay2 = false;
bool lastBay3 = false;
bool lastBay4 = false;

unsigned long systemUpdateTimer = 0;
const long updateInterval = 500;

// Entrance sequence state machine tracking
unsigned long stateWaitTimer = 0;

void setup() {
  Serial.begin(115200);
  delay(100);

  // Initialize Pins
  pinMode(IR1, INPUT);
  pinMode(IR2, INPUT);
  pinMode(IR3, INPUT);
  pinMode(IR4, INPUT);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  gateServo.attach(SERVO_PIN);
  gateServo.write(0); // GATE CLOSED

  // Connect to Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
  }
  timeClient.begin();

  // Configure Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  if (Firebase.signUp(&config, &auth, "", "")) {
    signupOK = true;
  }
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Read initial states to populate db
  bay1Occupied = (digitalRead(IR1) == LOW);
  bay2Occupied = (digitalRead(IR2) == LOW);
  bay3Occupied = (digitalRead(IR3) == LOW);
  bay4Occupied = (digitalRead(IR4) == LOW);
  lastBay1 = bay1Occupied;
  lastBay2 = bay2Occupied;
  lastBay3 = bay3Occupied;
  lastBay4 = bay4Occupied;
}

// Function to measure ultrasonic distance
long readDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // 30ms timeout
  if (duration == 0) return 999;
  return duration * 0.034 / 2;
}

void updateFirebaseEvent(String eventName) {
  currentEvent = eventName;
  Firebase.RTDB.setString(&fbdo, "/events/currentEvent", eventName);
}

void setGate(String state) {
  if (state == "OPEN") {
    gateServo.write(90);
  } else {
    gateServo.write(0);
  }
  Firebase.RTDB.setString(&fbdo, "/entrance/gate", state);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) return;
  timeClient.update();

  if (Firebase.ready() && signupOK) {
    long dist = readDistance();
    
    // Process Entrance Sequence
    if (currentEvent == "WAITING_FOR_CAR") {
      int freeSpaces = 4 - ((lastBay1?1:0) + (lastBay2?1:0) + (lastBay3?1:0) + (lastBay4?1:0));
      if (dist <= 10 && freeSpaces > 0) {
        updateFirebaseEvent("CAR_APPROACHING");
        setGate("OPEN");
        stateWaitTimer = millis();
        updateFirebaseEvent("GATE_OPEN");
      }
    } 
    else if (currentEvent == "GATE_OPEN") {
      if (millis() - stateWaitTimer > 3000) { // Wait for car to enter
        updateFirebaseEvent("CAR_ENTERED");
        setGate("CLOSED");
        stateWaitTimer = millis();
        updateFirebaseEvent("GATE_CLOSED");
      }
    }
    else if (currentEvent == "GATE_CLOSED") {
      if (millis() - stateWaitTimer > 1000) {
        updateFirebaseEvent("WAITING_FOR_PAYMENT");
        stateWaitTimer = millis();
      }
    }
    else if (currentEvent == "WAITING_FOR_PAYMENT") {
      if (millis() - stateWaitTimer > 3000) { // Simulate payment delay
        updateFirebaseEvent("PAYMENT_DONE");
        stateWaitTimer = millis();
      }
    }
    else if (currentEvent == "PAYMENT_DONE") {
      if (millis() - stateWaitTimer > 1000) {
        updateFirebaseEvent("ASSIGNING_SPACE");
        currentEntranceCarNumber = nextCarNumber;
        nextCarNumber++;
        Firebase.RTDB.setInt(&fbdo, "/system/nextCarNumber", nextCarNumber);
        // Stays here until a bay is occupied
      }
    }

    // Every 500ms update sensors to DB
    if (millis() - systemUpdateTimer > updateInterval) {
      systemUpdateTimer = millis();
      Firebase.RTDB.setInt(&fbdo, "/entrance/distance", dist);

      bay1Occupied = digitalRead(IR1) == LOW;
      bay2Occupied = digitalRead(IR2) == LOW;
      bay3Occupied = digitalRead(IR3) == LOW;
      bay4Occupied = digitalRead(IR4) == LOW;

      // Check state changes
      if (bay1Occupied != lastBay1) { handleBayChange("bay1", bay1Occupied); lastBay1 = bay1Occupied; }
      if (bay2Occupied != lastBay2) { handleBayChange("bay2", bay2Occupied); lastBay2 = bay2Occupied; }
      if (bay3Occupied != lastBay3) { handleBayChange("bay3", bay3Occupied); lastBay3 = bay3Occupied; }
      if (bay4Occupied != lastBay4) { handleBayChange("bay4", bay4Occupied); lastBay4 = bay4Occupied; }
      
      // Update system stats
      int occupied = (lastBay1?1:0) + (lastBay2?1:0) + (lastBay3?1:0) + (lastBay4?1:0);
      int free = 4 - occupied;
      FirebaseJson sysJson;
      sysJson.set("totalSpaces", 4);
      sysJson.set("occupiedSpaces", occupied);
      sysJson.set("freeSpaces", free);
      sysJson.set("parkingStatus", free > 0 ? "AVAILABLE" : "FULL");
      Firebase.RTDB.updateNode(&fbdo, "/system", &sysJson);
    }
  }
}

void handleBayChange(String bay, bool isOccupied) {
  FirebaseJson json;
  if (isOccupied) {
    json.set("status", "OCCUPIED");
    json.set("timeLimit", 30);
    // If we were assigning a space, assign this one to the car
    if (currentEvent == "ASSIGNING_SPACE") {
      json.set("carNumber", currentEntranceCarNumber);
      json.set("paymentStatus", "PAYMENT_DONE");
      json.set("parkingStartTime", (double)timeClient.getEpochTime() * 1000.0);
      updateFirebaseEvent("CAR_PARKED");
      delay(100);
      updateFirebaseEvent("PARKING_TIMER_RUNNING");
      delay(100);
      updateFirebaseEvent("WAITING_FOR_CAR"); // reset entrance for next car
    } else {
      // Just an unassigned car parked
      json.set("carNumber", nextCarNumber++);
      json.set("paymentStatus", "WAITING_FOR_PAYMENT");
      json.set("parkingStartTime", (double)timeClient.getEpochTime() * 1000.0);
    }
  } else {
    // Car removed
    json.set("status", "AVAILABLE");
    json.set("carNumber", (int)0);
    json.set("paymentStatus", "null");
    json.set("parkingStartTime", (int)0);
    updateFirebaseEvent("CAR_REMOVED");
    delay(500);
    updateFirebaseEvent("WAITING_FOR_CAR");
  }
  Firebase.RTDB.updateNode(&fbdo, String("/parking/") + bay, &json);
}
