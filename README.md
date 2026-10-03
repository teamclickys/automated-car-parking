# Automated Smart Car Parking System Setup Guide

This guide will walk you through setting up your Firebase database, uploading the ESP8266 code, and deploying your web dashboard.

## 1. How to create the Firebase project

1. Go to the [Firebase Console](https://console.firebase.google.com/).
2. Click **"Add project"**.
3. Enter a project name (e.g., "SmartParking").
4. Disable Google Analytics (you won't need it for this project) and click **"Create project"**.
5. Wait for the project to be created and click **"Continue"**.

## 2. How to create Realtime Database

1. In your Firebase project dashboard, look at the left sidebar and click on **Build** -> **Realtime Database**.
2. Click **"Create Database"**.
3. Choose a location closest to you and click **"Next"**.
4. Start in **"Test mode"** (this allows reading and writing without authentication for up to 30 days, which is perfect for testing). Click **"Enable"**.

## 3. Where to paste Firebase configuration

You need to paste Firebase credentials into two places: the **Web Dashboard** and the **ESP8266 Code**.

### For the Web Dashboard (`script.js`):
1. In Firebase Console, go to **Project Overview** (gear icon) -> **Project settings**.
2. Scroll down to the "Your apps" section and click the **Web icon (`</>`)**.
3. Register the app with a nickname (e.g., "Parking Dashboard"). (Do not check Firebase Hosting for now). Click **"Register app"**.
4. You will see a `firebaseConfig` object. Copy the values inside it.
5. Open `script.js` in your project folder.
6. Replace the placeholder values in the `firebaseConfig` object at the top of the file with your actual values:
   ```javascript
   const firebaseConfig = {
     apiKey: "AIzaSy...",
     authDomain: "your-project.firebaseapp.com",
     databaseURL: "https://your-project-default-rtdb.firebaseio.com",
     projectId: "your-project",
     storageBucket: "your-project.appspot.com",
     messagingSenderId: "123456789",
     appId: "1:123456789:web:abcdef"
   };
   ```

### For the ESP8266 Code (`ESP8266_Parking.ino`):
1. Open `ESP8266_Parking.ino` in the Arduino IDE.
2. Locate the `FIREBASE CONFIGURATION` section near the top.
3. For `API_KEY`, use the `apiKey` from the step above.
4. For `DATABASE_URL`, copy your database URL from the Realtime Database tab. **IMPORTANT**: Remove `https://` and the trailing `/`. It should look like this: `your-project-default-rtdb.firebaseio.com` (or similar depending on your region).

## 4. How to configure Firebase Database Rules for initial testing

Because you selected "Test mode" during creation, your rules should already be set to true. However, to ensure they don't expire immediately while you develop:

1. In the Firebase Console, go to **Realtime Database** -> **Rules** tab.
2. Replace the rules with the following to allow open access:
   ```json
   {
     "rules": {
       ".read": true,
       ".write": true
     }
   }
   ```
3. Click **"Publish"**. 
*(Note: This is insecure for a production application, but perfectly fine for a local IoT university/personal project.)*

## 5. How to upload the ESP8266 code

1. Open `ESP8266_Parking.ino` in the **Arduino IDE**.
2. Go to **File** -> **Preferences** and ensure you have the ESP8266 board manager URL added: `http://arduino.esp8266.com/stable/package_esp8266com_index.json`.
3. Go to **Tools** -> **Board** -> **Boards Manager**, search for "esp8266" and install it.
4. Go to **Tools** -> **Board** -> **ESP8266 Boards** and select **"NodeMCU 1.0 (ESP-12E Module)"**.
5. You need the Firebase library. Go to **Sketch** -> **Include Library** -> **Manage Libraries**. Search for **"Firebase ESP Client"** by Mobizt and install it.
6. Verify your Wi-Fi credentials in the code (`WIFI_SSID` and `WIFI_PASSWORD`).
7. Connect your ESP8266 to your computer via USB. Select the correct Port in **Tools** -> **Port**.
8. Click the **"Upload"** button (Right arrow icon).

## 6. How to deploy the dashboard

Since the dashboard uses vanilla HTML, CSS, and JS, you have several easy options:

- **Locally:** Just double-click the `index.html` file on your computer. It will open in your browser and connect to Firebase immediately.
- **GitHub Pages:** Create a GitHub repository, upload `index.html`, `style.css`, and `script.js`, and enable GitHub Pages in the repo settings.
- **Vercel / Netlify:** Drag and drop the folder containing your three files into Vercel or Netlify for free instant hosting.

## 7. How to test the system

1. After uploading the code to the ESP8266, open the **Serial Monitor** in Arduino IDE (Tools -> Serial Monitor). Set baud rate to **115200**.
2. Watch the Serial Monitor to ensure the ESP8266 connects to Wi-Fi ("Clickys") and then prints "Firebase Auth OK".
3. Open `index.html` in your web browser. You should see "Connecting to Firebase..." change to the actual data.
4. Trigger your IR sensors (place a hand or object in front of them).
5. The corresponding LED on the NodeMCU (D5-D8) should turn on or off immediately.
6. The Serial Monitor will print "=== Parking State Changed ===" and show the upload status.
7. Look at your Web Dashboard. The corresponding Bay card should instantly turn Red (OCCUPIED) or Green (AVAILABLE), and the total numbers should update automatically without refreshing the page!

### Troubleshooting IR Sensors
If your dashboard shows OCCUPIED when there is no car, and AVAILABLE when there is a car, your IR sensors use inverted logic.
Simply go to the ESP8266 code, find this line:
`#define IR_ACTIVE_STATE LOW`
Change it to:
`#define IR_ACTIVE_STATE HIGH`
Re-upload the code.
