#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include <Servo.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

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
#define SERVO_PIN D8   // Servo on D8

// =====================================================================
// GATE TIMING (milliseconds)
// =====================================================================
#define GATE_OPEN_DURATION 10000   // All gate openings = 10 seconds

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

bool bay1Occupied = false;
bool bay2Occupied = false;
bool bay3Occupied = false;
bool bay4Occupied = false;

bool lastBay1 = false;
bool lastBay2 = false;
bool lastBay3 = false;
bool lastBay4 = false;

// Parking start times per bay (millis) for expired detection
unsigned long bay1StartMs = 0;
unsigned long bay2StartMs = 0;
unsigned long bay3StartMs = 0;
unsigned long bay4StartMs = 0;

// Whether gate was already opened for exit on this expiry
bool bay1ExitTriggered = false;
bool bay2ExitTriggered = false;
bool bay3ExitTriggered = false;
bool bay4ExitTriggered = false;

#define PARKING_DURATION_MS 30000  // 30 seconds

unsigned long systemUpdateTimer = 0;
const long updateInterval = 500;

unsigned long stateWaitTimer = 0;

// Forward declaration
void handleBayChange(String bay, bool isOccupied);

// =====================================================================
// SETUP
// =====================================================================
void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println();
  Serial.println("=================================");
  Serial.println(" SMART CAR PARKING SYSTEM");
  Serial.println("=================================");

  pinMode(IR1, INPUT);
  pinMode(IR2, INPUT);
  pinMode(IR3, INPUT);
  pinMode(IR4, INPUT);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  gateServo.attach(SERVO_PIN);
  gateServo.write(0);  // GATE CLOSED on boot
  Serial.println("Gate: CLOSED");

  // Connect to Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.println("WiFi connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // NTP time sync
  timeClient.begin();
  timeClient.update();

  // Configure Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase Auth OK");
    signupOK = true;
  } else {
    Serial.print("Firebase Auth Error: ");
    Serial.println(config.signer.signupError.message.c_str());
  }
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Wait for Firebase to be ready
  Serial.print("Waiting for Firebase");
  unsigned long fbWait = millis();
  while (!Firebase.ready() && millis() - fbWait < 10000) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();

  // Read physical sensor states
  bay1Occupied = (digitalRead(IR1) == LOW);
  bay2Occupied = (digitalRead(IR2) == LOW);
  bay3Occupied = (digitalRead(IR3) == LOW);
  bay4Occupied = (digitalRead(IR4) == LOW);
  lastBay1 = bay1Occupied;
  lastBay2 = bay2Occupied;
  lastBay3 = bay3Occupied;
  lastBay4 = bay4Occupied;

  // Force-sync ALL bays to Firebase on boot (fixes wiped DB)
  Serial.println("Syncing bay states to Firebase...");
  handleBayChange("bay1", bay1Occupied);
  handleBayChange("bay2", bay2Occupied);
  handleBayChange("bay3", bay3Occupied);
  handleBayChange("bay4", bay4Occupied);

  // Sync system stats
  int occupied = (bay1Occupied?1:0)+(bay2Occupied?1:0)+(bay3Occupied?1:0)+(bay4Occupied?1:0);
  int freeS = 4 - occupied;
  FirebaseJson sysJson;
  sysJson.set("totalSpaces", 4);
  sysJson.set("occupiedSpaces", occupied);
  sysJson.set("freeSpaces", freeS);
  sysJson.set("parkingStatus", freeS > 0 ? "AVAILABLE" : "FULL");
  Firebase.RTDB.updateNode(&fbdo, "/system", &sysJson);

  Firebase.RTDB.setString(&fbdo, "/entrance/gate", "CLOSED");
  Firebase.RTDB.setInt(&fbdo, "/entrance/distance", 999);
  Firebase.RTDB.setString(&fbdo, "/events/currentEvent", "WAITING_FOR_CAR");

  Serial.println("=================================");
  Serial.println(" SYSTEM READY");
  Serial.println("=================================");
}

// =====================================================================
// ULTRASONIC - NOISE FILTERED (3 averaged readings)
// =====================================================================
long readDistance() {
  long totalDist = 0;
  int validReads = 0;

  for (int i = 0; i < 3; i++) {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    long duration = pulseIn(ECHO_PIN, HIGH, 30000);
    if (duration > 0) {
      long d = duration * 0.034 / 2;
      if (d > 0 && d < 400) {
        totalDist += d;
        validReads++;
      }
    }
    delay(10);
  }

  if (validReads == 0) return 999;
  return totalDist / validReads;
}

// =====================================================================
// FIREBASE HELPERS
// =====================================================================
void updateFirebaseEvent(String eventName) {
  currentEvent = eventName;
  Serial.print("EVENT: ");
  Serial.println(eventName);
  Firebase.RTDB.setString(&fbdo, "/events/currentEvent", eventName);
}

void setGate(String state) {
  if (state == "OPEN") {
    gateServo.write(90);
    Serial.println(">>> GATE OPEN <<<");
  } else {
    gateServo.write(0);
    Serial.println(">>> GATE CLOSED <<<");
  }
  Firebase.RTDB.setString(&fbdo, "/entrance/gate", state);
}

// =====================================================================
// CHECK PARKING TIMERS - opens exit gate when time expires
// =====================================================================
void checkParkingTimers() {
  // Only check when idle (not in the middle of an entrance sequence)
  if (currentEvent != "WAITING_FOR_CAR" &&
      currentEvent != "PARKING_TIMER_RUNNING") return;

  unsigned long now = millis();

  // Helper lambda via macro for each bay
  #define CHECK_BAY_EXPIRY(bayId, bayOccupied, bayStartMs, bayExitTriggered) \
    if (bayOccupied && bayStartMs > 0) { \
      unsigned long elapsed = now - bayStartMs; \
      if (elapsed >= PARKING_DURATION_MS && !bayExitTriggered) { \
        Serial.println("TIME EXPIRED for " #bayId " - opening exit gate"); \
        bayExitTriggered = true; \
        updateFirebaseEvent("TIME_EXPIRED"); \
        delay(500); \
        updateFirebaseEvent("CAR_MUST_BE_REMOVED"); \
        setGate("OPEN"); \
        stateWaitTimer = millis(); \
        updateFirebaseEvent("EXIT_GATE_OPEN"); \
      } \
    }

  CHECK_BAY_EXPIRY(bay1, lastBay1, bay1StartMs, bay1ExitTriggered)
  CHECK_BAY_EXPIRY(bay2, lastBay2, bay2StartMs, bay2ExitTriggered)
  CHECK_BAY_EXPIRY(bay3, lastBay3, bay3StartMs, bay3ExitTriggered)
  CHECK_BAY_EXPIRY(bay4, lastBay4, bay4StartMs, bay4ExitTriggered)
}

// =====================================================================
// MAIN LOOP
// =====================================================================
void loop() {
  if (WiFi.status() != WL_CONNECTED) return;
  timeClient.update();

  if (Firebase.ready() && signupOK) {
    long dist = readDistance();

    // --- ENTRANCE / EXIT STATE MACHINE ---
    if (currentEvent == "WAITING_FOR_CAR") {
      int freeSpaces = 4 - ((lastBay1?1:0)+(lastBay2?1:0)+(lastBay3?1:0)+(lastBay4?1:0));
      // Only trigger on real reading: 3cm–10cm
      if (dist > 2 && dist <= 10 && freeSpaces > 0) {
        updateFirebaseEvent("CAR_APPROACHING");
        delay(200);
        setGate("OPEN");
        stateWaitTimer = millis();
        updateFirebaseEvent("GATE_OPEN");
      }
    }
    else if (currentEvent == "GATE_OPEN") {
      // Gate open for 10 seconds for car to enter
      if (millis() - stateWaitTimer > GATE_OPEN_DURATION) {
        updateFirebaseEvent("CAR_ENTERED");
        delay(200);
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
      if (millis() - stateWaitTimer > 3000) {
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
      }
    }
    else if (currentEvent == "EXIT_GATE_OPEN") {
      // Exit gate open for 10 seconds then close
      if (millis() - stateWaitTimer > GATE_OPEN_DURATION) {
        setGate("CLOSED");
        delay(300);
        updateFirebaseEvent("WAITING_FOR_CAR");
        Serial.println("Exit gate closed - ready for next car");
      }
    }

    // --- SENSOR POLLING EVERY 500ms ---
    if (millis() - systemUpdateTimer > updateInterval) {
      systemUpdateTimer = millis();

      Firebase.RTDB.setInt(&fbdo, "/entrance/distance", dist);

      bay1Occupied = digitalRead(IR1) == LOW;
      bay2Occupied = digitalRead(IR2) == LOW;
      bay3Occupied = digitalRead(IR3) == LOW;
      bay4Occupied = digitalRead(IR4) == LOW;

      // Check for expired timers and open exit gate
      checkParkingTimers();

      if (bay1Occupied != lastBay1) { handleBayChange("bay1", bay1Occupied); lastBay1 = bay1Occupied; }
      if (bay2Occupied != lastBay2) { handleBayChange("bay2", bay2Occupied); lastBay2 = bay2Occupied; }
      if (bay3Occupied != lastBay3) { handleBayChange("bay3", bay3Occupied); lastBay3 = bay3Occupied; }
      if (bay4Occupied != lastBay4) { handleBayChange("bay4", bay4Occupied); lastBay4 = bay4Occupied; }

      int occupied = (lastBay1?1:0)+(lastBay2?1:0)+(lastBay3?1:0)+(lastBay4?1:0);
      int freeS = 4 - occupied;
      FirebaseJson sysJson;
      sysJson.set("totalSpaces", 4);
      sysJson.set("occupiedSpaces", occupied);
      sysJson.set("freeSpaces", freeS);
      sysJson.set("parkingStatus", freeS > 0 ? "AVAILABLE" : "FULL");
      Firebase.RTDB.updateNode(&fbdo, "/system", &sysJson);
    }
  }
}

// =====================================================================
// BAY CHANGE HANDLER
// =====================================================================
void handleBayChange(String bay, bool isOccupied) {
  FirebaseJson json;

  if (isOccupied) {
    json.set("status", "OCCUPIED");
    json.set("timeLimit", 30);

    unsigned long nowMs = millis();

    if (currentEvent == "ASSIGNING_SPACE") {
      json.set("carNumber", currentEntranceCarNumber);
      json.set("paymentStatus", "PAYMENT_DONE");
      json.set("parkingStartTime", (double)timeClient.getEpochTime() * 1000.0);

      // Track local timer for exit gate trigger
      if (bay == "bay1") { bay1StartMs = nowMs; bay1ExitTriggered = false; }
      if (bay == "bay2") { bay2StartMs = nowMs; bay2ExitTriggered = false; }
      if (bay == "bay3") { bay3StartMs = nowMs; bay3ExitTriggered = false; }
      if (bay == "bay4") { bay4StartMs = nowMs; bay4ExitTriggered = false; }

      Serial.print("Car #"); Serial.print(currentEntranceCarNumber);
      Serial.print(" assigned to "); Serial.println(bay);

      updateFirebaseEvent("CAR_PARKED");
      delay(100);
      updateFirebaseEvent("PARKING_TIMER_RUNNING");
      delay(100);
      updateFirebaseEvent("WAITING_FOR_CAR");
    } else {
      // Boot sync - bay already occupied
      json.set("carNumber", 0);
      json.set("paymentStatus", "WAITING_FOR_PAYMENT");
      json.set("parkingStartTime", (double)timeClient.getEpochTime() * 1000.0);

      if (bay == "bay1") { bay1StartMs = nowMs; bay1ExitTriggered = false; }
      if (bay == "bay2") { bay2StartMs = nowMs; bay2ExitTriggered = false; }
      if (bay == "bay3") { bay3StartMs = nowMs; bay3ExitTriggered = false; }
      if (bay == "bay4") { bay4StartMs = nowMs; bay4ExitTriggered = false; }
    }
  } else {
    // Car removed - reset bay
    json.set("status", "AVAILABLE");
    json.set("carNumber", 0);
    json.set("paymentStatus", "NONE");
    json.set("parkingStartTime", 0);

    // Reset exit gate trigger for this bay
    if (bay == "bay1") { bay1StartMs = 0; bay1ExitTriggered = false; }
    if (bay == "bay2") { bay2StartMs = 0; bay2ExitTriggered = false; }
    if (bay == "bay3") { bay3StartMs = 0; bay3ExitTriggered = false; }
    if (bay == "bay4") { bay4StartMs = 0; bay4ExitTriggered = false; }

    Serial.print(bay); Serial.println(" → AVAILABLE");

    if (currentEvent != "WAITING_FOR_CAR" &&
        currentEvent != "GATE_OPEN" &&
        currentEvent != "EXIT_GATE_OPEN") {
      updateFirebaseEvent("CAR_REMOVED");
      delay(300);
      updateFirebaseEvent("WAITING_FOR_CAR");
    }
  }

  Firebase.RTDB.updateNode(&fbdo, String("/parking/") + bay, &json);
}
