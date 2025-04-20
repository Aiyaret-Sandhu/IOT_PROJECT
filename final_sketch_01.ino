#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>
#include <ArduinoJson.h>
#include <WiFiServer.h>


#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C 

#define FINGERPRINT_DUPLICATE 0xEE  

const char* ssid = "wifi";
const char* password = "pass12345";
const char* serverName = "https://script.google.com/macros/s/AKfycbyHvPDI6xpPmoQ9zVZH3jX2xiLxXHyGit3ZNeJZbxAWvzXSvKztAeKUDdRCzj04cEn3SQ/exec";


WiFiServer server(23);
WiFiClient client;
bool clientConnected = false;
unsigned long previousMillis = 0;
const long heartbeatInterval = 3000; // Send heartbeat every 3 seconds when idle


Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


#if (defined(__AVR__) || defined(ESP8266)) && !defined(__AVR_ATmega2560__)

#else

#define mySerial Serial2

#endif

Adafruit_Fingerprint finger = Adafruit_Fingerprint(&Serial2);
uint8_t id;


#define BUTTON_REGISTER 19

#define BUTTON_AMOUNT_10 13
#define BUTTON_AMOUNT_20 5
#define BUTTON_AMOUNT_50 15
#define BUTTON_AMOUNT_100 26
#define BUTTON_CONFIRM 27

#define LED_REGISTER 18
#define LED_CONFIRM 23

#define SYSTEM_RUNNING_LED 14

#define BUZZER_PIN 12

// Helper function to send all serial output to TCP client
void sendToClient(String message) {
  Serial.print(message);
  if (clientConnected && client.connected()) {
    client.print(message);
  }
}

void sendToClientln(String message) {
  Serial.println(message);
  if (clientConnected && client.connected()) {
    client.println(message);
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  sendToClientln("");
  sendToClient("Connecting to ");
  sendToClientln(ssid);
  
  WiFi.begin(ssid, password);

  mySerial.begin(57600, SERIAL_8N1, 16, 17);
  finger.begin(57600);
  delay(2000);

  Wire.begin(21, 22);  

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    sendToClient(".");
  }
  sendToClientln("\nConnected to WiFi");
  sendToClient("IP Address: ");
  sendToClientln(WiFi.localIP().toString());
  
  server.begin();
  sendToClientln("TCP server started on port 23");

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    sendToClientln("SSD1306 allocation failed");
    for (;;);
  }

  digitalWrite(SYSTEM_RUNNING_LED, LOW);  // Quick blink
  delay(200);
  digitalWrite(SYSTEM_RUNNING_LED, HIGH); 

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 10);
  display.println("Mess \nManagement \nSystem");
  display.display();
  delay(2000);
  display.clearDisplay();

  pinMode(BUTTON_AMOUNT_10, INPUT_PULLUP);
  pinMode(BUTTON_AMOUNT_20, INPUT_PULLUP);
  pinMode(BUTTON_AMOUNT_50, INPUT_PULLUP);
  pinMode(BUTTON_AMOUNT_100, INPUT_PULLUP);
  pinMode(BUTTON_CONFIRM, INPUT_PULLUP);
  pinMode(BUTTON_REGISTER, INPUT_PULLUP);

  pinMode(LED_REGISTER, OUTPUT);
  pinMode(LED_CONFIRM, OUTPUT);
  digitalWrite(LED_REGISTER, LOW);
  digitalWrite(LED_CONFIRM, LOW);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);  
}



void beep(int duration = 200) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(duration);                 // Buzzer duration
  digitalWrite(BUZZER_PIN, LOW);
}

void clearFingerprintDatabase() {
  sendToClientln("Clearing all stored fingerprints...");
  updateDisplay("Clearing database...");
  
  for (int i = 0; i <= 250; i++) {
    if (finger.deleteModel(i) == FINGERPRINT_OK) {
      sendToClient("Deleted ID ");
      sendToClientln(String(i));
    } else {
      sendToClient("Failed to delete ID ");
      sendToClientln(String(i));
    }
    delay(50);  // Small delay between deletions
  }
  
  sendToClientln("All fingerprints deleted!");
  updateDisplay("Database cleared!");
  delay(2000);
}


int getFingerprintID() {
  int p = finger.getImage();
  if (p != FINGERPRINT_OK) return -1;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return -1;

  p = finger.fingerSearch();
  if (p != FINGERPRINT_OK) return -1;

  beep(300);

  return finger.fingerID;
}

String getUserName(int id, String &rollNo) {  // Added rollNo as reference parameter
  if (WiFi.status() != WL_CONNECTED) {
    rollNo = "";
    return "Unknown";
  }

  HTTPClient http;
  String requestUrl = String(serverName) + "?id=" + String(id);
  http.begin(requestUrl);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  String userName = "Unknown";
  rollNo = "";  // Initialize rollNo
  int totalFees = 0;

  int httpCode = http.GET();
  if (httpCode == 200) {
    String response = http.getString();
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, response);
    
    if (!error) {
      userName = doc["username"].as<String>();
      rollNo = doc["rollNo"].as<String>();  // Get roll number
      totalFees = doc["totalFees"].as<int>();
    }
  }
  http.end();

  // Display formatted information on OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("User Details:");
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.print("Roll: ");
  display.println(rollNo);
  display.setCursor(0, 35);
  display.print("Name: ");
  display.println(userName);
  display.setCursor(0, 50);
  display.print("Fees : Rs");
  display.println(totalFees);
  display.display();

  sendToClient("User      : ");
  sendToClientln(userName);

  sendToClient("Roll No   : ");
  sendToClientln(rollNo);

  sendToClient("Total Fees: Rs");
  sendToClientln(String(totalFees));


  
  return userName;
}


void sendToGoogleSheets(String jsonData) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverName);
    http.addHeader("Content-Type", "application/json; charset=UTF-8");
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    sendToClient("Sending JSON: ");  
    sendToClientln(jsonData);

    int httpResponseCode = http.POST(jsonData);
  
    sendToClient("HTTP Response code: ");
    sendToClientln(String(httpResponseCode));

    http.end();
  } else {
    sendToClientln("WiFi Disconnected");
  }
}



void updateDisplay(String message) {
  display.clearDisplay();
  display.setTextSize(1);  
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(5, 0);
  display.println(message);
  display.display();
}

String readName() {
  String name = "";
  sendToClientln("Enter the name for this fingerprint:");
  while (name.length() == 0) {
    name = readInputFromAllSources();
    name.trim();
    delay(100);
  }
  return name;
}

bool isFingerprintDuplicate() {
  // First, try to read the fingerprint
  int p = finger.getImage();
  if (p != FINGERPRINT_OK) return false;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return false;

  p = finger.fingerFastSearch();
  
  if (p == FINGERPRINT_OK) {
    sendToClientln("Found matching fingerprint with ID #" + String(finger.fingerID));
    return true;
  }
  return false;
}


bool isIdAvailableInSheets(uint8_t id) {
  if (WiFi.status() != WL_CONNECTED) {
    sendToClientln("WiFi not connected - cannot check ID availability");
    return false;
  }

  HTTPClient http;
  String requestUrl = String(serverName) + "?id=" + String(id);
  http.begin(requestUrl);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  int httpResponseCode = http.GET();
  if (httpResponseCode == 200) {
    String response = http.getString();
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, response);
    if (!error) {
      String userName = doc["username"].as<String>();
      http.end();
      return userName == "Unknown"; // ID is available if username is "Unknown"
    }
  }
  http.end();
  return false; // Assume ID is not available if there's any error
}

String readInputFromAllSources() {
  String input = "";
  unsigned long startTime = millis();
  const unsigned long timeout = 30000; // 30-second timeout
  
  while (millis() - startTime < timeout) {
    // Check Serial (for debugging)
    while (Serial.available()) {
      char c = Serial.read();
      if (c == '\n') {
        input.trim();
        if (input.length() > 0) {
          sendToClientln("From Serial: " + input);
          return input;
        }
        input = "";
      } else {
        input += c;
      }
    }
    
    // Check TCP Client (from app)
    if (clientConnected && client.available()) {
      while (client.available()) {
        char c = client.read();
        if (c == '\n') {
          input.trim();
          if (input.length() > 0) {
            sendToClientln("From Client: " + input);
            return input;
          }
          input = "";
        } else {
          input += c;
        }
      }
    }
    
    // Small delay to prevent busy waiting
    delay(50);
    
    // Check for registration cancellation
    if (digitalRead(BUTTON_REGISTER) == LOW) {
      sendToClientln("Registration cancelled by button press");
      return ""; // Return empty string to indicate cancellation
    }
  }
  
  sendToClientln("Input timeout reached");
  return ""; // Return empty string on timeout
}


void registerFingerprint() {
  // clearFingerprintDatabase();
  sendToClientln("Ready to enroll a fingerprint!");
  updateDisplay("Ready to begin .....");

  // TEST: Add a confirmation prompt
  sendToClientln("TEST: Please enter '1' to confirm you can see this message");
  uint8_t confirm = 0;
  while (confirm != 1) {
    confirm = readnumber();
    if (confirm != 1) {
      sendToClientln("Please enter exactly '1' to confirm");
    }
  }
  sendToClientln("NEUMERIC TEST SUCCESS: Input received correctly!");
  beep(200);
  delay(1000);

  // TEST: String input confirmation
  sendToClientln("TEST: Please enter 'hello' to confirm string input works");
  String testString = "";
  while (testString != "hello") {
    testString = readInputFromAllSources();
    if (testString != "hello") {
      sendToClientln("Please enter exactly 'hello' to confirm");
    }
  }
  sendToClientln("STRING TEST SUCCESS: Input received correctly!");
  beep(200);
  delay(1000);
  
  while (true) {
    sendToClientln("Please type in the ID # (from 1 to 250) you want to save this finger as...");
    id = readnumber();
    if (id == 0) return;

    // Check if ID exists in Google Sheets
    if (!isIdAvailableInSheets(id)) {
      sendToClientln("ID " + String(id) + " is already registered in the system. Please choose a different ID.");
      updateDisplay("ID " + String(id) + " taken!");
      delay(2000);
      continue;
    }
    break;
  }

  sendToClientln("Place finger to check for duplicates...");
  updateDisplay("Checking for duplicates...");
  
  unsigned long startTime = millis();
  bool duplicateFound = false;

  beep(200);
  
  while (millis() - startTime < 10000) { // 10-second timeout
    if (isFingerprintDuplicate()) {
      duplicateFound = true;
      break;
    }
    delay(200);
  }

  if (duplicateFound) {
    sendToClientln("ERROR: This fingerprint is already registered under ID #" + String(finger.fingerID));
    beep(1000);
    updateDisplay("Fingerprint exists!\nID: " + String(finger.fingerID));
    
    // Clear the fingerprint buffer
    while(finger.getImage() != FINGERPRINT_NOFINGER);
    
    // Wait for user to remove finger and confirm
    sendToClientln("Press D27 ( CONFIRM BUTTON ) to continue...");
    while(digitalRead(BUTTON_CONFIRM) == HIGH) {
      delay(100);
    }

    digitalWrite(LED_CONFIRM, HIGH);  // Flash confirm LED
    delay(200);
    digitalWrite(LED_CONFIRM, LOW);

    return;
  }

  beep();

  String name = readName();
  if (name.length() == 0) {
    sendToClientln("Registration cancelled");
    return;
  }

  String rollNo = readField("Enter Roll Number: ");
  if (rollNo.length() == 0) {
    sendToClientln("Registration cancelled");
    return;
  }
  
  String email = readField("Enter Email: ");

  String otp = String(random(100000, 999999));
  
  // Send OTP email (but don't wait for response)
  String otpRequest = "{\"action\":\"sendOTP\",\"email\":\"" + email + "\",\"otp\":\"" + otp + "\",\"fingerprintID\":\"" + String(id) + "\"}";

  HTTPClient http;
  http.begin(serverName);
  http.addHeader("Content-Type", "application/json");
  http.POST(otpRequest); // Fire and forget
  http.end();

  sendToClientln("OTP has been sent to your email");
  updateDisplay("Check email for OTP");

  // Get OTP from user
  String userOTP = readField("Enter OTP from email: ");

  // Simple client-side verification
  if (userOTP != otp) {
    sendToClientln("OTP verification failed");
    updateDisplay("Invalid OTP!");
    delay(2000);
    return;
  }


  String phoneNo = readField("Enter Phone Number: ");

  int hostel = 0;
  while (hostel < 1 || hostel > 5) {
    sendToClientln("Select Hostel:");
    sendToClientln("1: Arch A");
    sendToClientln("2: Arch B");
    sendToClientln("3: Arch C");
    sendToClientln("4: Arch D");
    sendToClientln("5: Other");
    hostel = readnumber();
    
    if (hostel < 1 || hostel > 5) {
      sendToClientln("Invalid selection! Please choose 1-5");
    }
  }

  String roomNo = readField("Enter Room Number: ");



  sendToClient("Enrolling ID #");
  sendToClientln(String(id));
  updateDisplay("Enrolling...");

  // Check for duplicate fingerprint before enrolling
  if (!getFingerprintEnroll(name, rollNo, phoneNo, hostel, email, roomNo)) {
    sendToClientln("Failed to enroll fingerprint");
    updateDisplay("Enroll failed!");
    delay(2000);
    return;
  }

  beep(1000);
  sendToClientln("Registration Complete !");
  updateDisplay("Registered !");
  delay(2000);
  paymentMode();
}

uint8_t readnumber(void) {
  uint8_t num = 0;

  while (num == 0) {
    String input = readInputFromAllSources();
    if (input.length() > 0) {
      num = input.toInt();
    }
    delay(100);
  }
  return num;
}

String readField(const char* prompt) {
  sendToClientln(prompt);
  String input = "";
  while (input.length() == 0) {
    input = readInputFromAllSources();
    input.trim();
    
    // For OTP field, ensure it's 6 digits
    if (strstr(prompt, "OTP") != NULL && input.length() != 6) {
      if (input.length() > 0) { // Only show message if we got some input
        sendToClientln("OTP must be 6 digits");
      }
      input = "";
      continue;
    }
    delay(100); // Small delay to prevent busy waiting
  }
  return input;
}

bool sendOTPRequest(String jsonData) {
  if (WiFi.status() != WL_CONNECTED) {
    sendToClientln("WiFi not connected");
    return false;
  }

  HTTPClient http;
  http.begin(serverName);
  http.addHeader("Content-Type", "application/json");
  
  sendToClientln("Sending OTP request:");
  sendToClientln(jsonData);
  
  int httpCode = http.POST(jsonData);
  String response = http.getString();
  
  sendToClient("HTTP Response code: ");
  sendToClientln(String(httpCode));
  sendToClient("Response: ");
  sendToClientln(response);
  
  bool success = (httpCode == 200);
  
  http.end();
  return success;
}

bool verifyOTP(String jsonData) {
  if (WiFi.status() != WL_CONNECTED) {
    sendToClientln("WiFi not connected for OTP verification");
    return false;
  }

  HTTPClient http;
  http.begin(serverName);
  http.addHeader("Content-Type", "application/json");
  
  sendToClientln("Sending OTP verification:");
  sendToClientln(jsonData);
  
  int httpCode = http.POST(jsonData);
  
  if (httpCode == 200) {
    String response = http.getString();
    sendToClient("OTP verification response: ");
    sendToClientln(response);
    
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, response);
    
    if (!error && doc["success"] == true && doc["verified"] == true) {
      http.end();
      sendToClientln("OTP verification successful");
      return true;
    } else {
      sendToClientln("OTP verification failed in response parsing");
    }
  } else {
    sendToClient("OTP verification HTTP error: ");
    sendToClientln(String(httpCode));
  }
  
  http.end();
  return false;
}

void paymentMode() {
  sendToClientln("Starting payment mode..."); 

  while(true) {
    sendToClientln("Waiting for fingerprint...");
    updateDisplay("Place Finger...");

    unsigned long startTime = millis();
    int userID = -1;
    
    while (userID == -1) {
      // Check for fingerprint
      userID = getFingerprintID();
      
      // Check for TCP client disconnection
      if (clientConnected && !client.connected()) {
        clientConnected = false;
        sendToClientln("Client disconnected");
      }
      
      // Check for new TCP client connections
      if (!clientConnected) {
        WiFiClient newClient = server.available();
        if (newClient) {
          client = newClient;
          clientConnected = true;
          sendToClientln("New client connected");
          sendToClientln("ESP32 Mess Management System");
          sendToClientln("Current mode: Payment Mode");
          sendToClientln("Waiting for fingerprint...");
        }
      }
      
      if (digitalRead(BUTTON_REGISTER) == LOW) {
        digitalWrite(LED_REGISTER, HIGH);
        sendToClientln("Register button pressed - switching to registration mode");
        updateDisplay("Switching to register...");
        delay(1000);
        digitalWrite(LED_REGISTER, LOW);
        registerFingerprint(); 
        return;  // Exit payment mode after registration completes
      }
      
      // Small delay to prevent busy-waiting
      delay(100);
      
      // Optional timeout (e.g., 5 seconds)
      if (millis() - startTime > 5000) {
        sendToClientln("Timeout waiting for fingerprint");
        updateDisplay("Timeout - Retrying...");
        delay(1000);
        break;  // Break out of the inner while loop
      }
    }

    if (userID == -1) {
      // Fingerprint not recognized or timeout
      sendToClientln("Fingerprint not recognized!");
      updateDisplay("Not recognized!");
      delay(2000);
      continue;
    }

    // Found user, get name
    String rollNo;
    String userName = getUserName(userID, rollNo);
    
    sendToClientln("Fingerprint detected, ID: " + String(userID));
    delay(3000);

    // Select amount using buttons
    int selectedAmount = 0;
    bool paymentDone = false;
    
    while (!paymentDone) {
      updateDisplay("Total: Rs : " + String(selectedAmount) + "\nPress Confirm Button to Confirm");
      
      if (digitalRead(BUTTON_AMOUNT_10) == LOW) {
        selectedAmount += 10;
        updateDisplay("Added 10...");
        sendToClientln("Added 10...");
        delay(300);
      } 
      else if (digitalRead(BUTTON_AMOUNT_20) == LOW) {
        selectedAmount += 20;
        updateDisplay("Added 20...");
        sendToClientln("Added 20...");
        delay(300);
      } 
      else if (digitalRead(BUTTON_AMOUNT_50) == LOW) {
        selectedAmount += 50;
        updateDisplay("Added 50...");
        sendToClientln("Added 50...");
        delay(300);
      }
      else if(digitalRead(BUTTON_AMOUNT_100) == LOW) {
        selectedAmount += 100;
        updateDisplay("Added 100...");
        sendToClientln("Added 100...");
        delay(300);
      }
      else if (digitalRead(BUTTON_CONFIRM) == LOW) {
        digitalWrite(LED_CONFIRM, HIGH);
        if (selectedAmount > 0) {
          updateDisplay("Processing...");
          sendToClientln("Processing...");
          String paymentData = "{\"fingerprintID\":\"" + String(userID) + 
                   "\",\"username\":\"" + userName + 
                   "\",\"status\":\"Paid " + String(selectedAmount) + "\"}";
          sendToGoogleSheets(paymentData);
          beep(300);
          updateDisplay("Paid: Rs : " + String(selectedAmount));
          sendToClientln("Paid: Rs : " + String(selectedAmount));
          delay(2000);
        }
        digitalWrite(LED_CONFIRM, LOW);
        paymentDone = true; // Mark payment as complete
      }
      
      // Check for TCP client disconnection/reconnection
      if (clientConnected && !client.connected()) {
        clientConnected = false;
        sendToClientln("Client disconnected during payment");
      }
      
      if (!clientConnected) {
        WiFiClient newClient = server.available();
        if (newClient) {
          client = newClient;
          clientConnected = true;
          sendToClientln("New client connected during payment");
          sendToClientln("Current amount: Rs " + String(selectedAmount));
        }
      }
      
      delay(50);
    }
    
    if (paymentDone) {
      return; // Exit payment mode after successful transaction
    }
  }
}

void loop() {
  // Check for TCP client disconnection
  if (clientConnected && !client.connected()) {
    clientConnected = false;
    sendToClientln("Client disconnected");
  }
  
  // Check for new TCP client connections
  if (!clientConnected) {
    WiFiClient newClient = server.available();
    if (newClient) {
      client = newClient;
      clientConnected = true;
      sendToClientln("New client connected");
      sendToClientln("ESP32 Mess Management System");
      sendToClientln("System ready");
    }
  }
  
  // Check for incoming data from client
  if (clientConnected && client.available()) {
    String receivedData = client.readStringUntil('\n');
    receivedData.trim();
    
    if (receivedData.length() > 0) {
      sendToClient("Received command: ");
      sendToClientln(receivedData);
      
      // Process commands here if needed
      client.println("Command acknowledged: " + receivedData);
    }
  }
  
  // Send heartbeat if it's time and we have a client
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= heartbeatInterval) {
    previousMillis = currentMillis;
    if (clientConnected) {
      client.println("System running... Press the Register button to enter registration mode");
    }
  }
  
  // Main program flow
  updateDisplay("Press \nRegister Button \nto Register");
  sendToClientln("Press \nRegister Button \nwithin 5 sec to Register...");

  unsigned long startTime = millis();
  bool registerSelected = false;
  
  while (millis() - startTime < 5000) {
    // Handle client connections while waiting
    if (clientConnected && !client.connected()) {
      clientConnected = false;
      sendToClientln("Client disconnected during button wait");
    }
    
    if (!clientConnected) {
      WiFiClient newClient = server.available();
      if (newClient) {
        client = newClient;
        clientConnected = true;
        sendToClientln("New client connected");
        sendToClientln("ESP32 Mess Management System");
        sendToClientln("Press D19 button to enter registration mode");
      }
    }
    
    if (digitalRead(BUTTON_REGISTER) == LOW) {
      registerSelected = true;
      digitalWrite(LED_REGISTER, HIGH);
      sendToClientln("Register button pressed - switching to registration mode");
      updateDisplay("Switching to register...");
      delay(1000);
      digitalWrite(LED_REGISTER, LOW);
      break;
    }
    
    delay(100); // Small delay to prevent busy waiting
  }

  if (registerSelected) {
    sendToClientln("Register Mode Selected!");
    updateDisplay("Register Mode...");
    delay(1000);
    registerFingerprint();
  } else {
    sendToClientln("Payment Mode Selected!");
    updateDisplay("Payment Mode...");
    delay(1000);
    paymentMode();
  }
}

uint8_t getFingerprintEnroll(String name, String rollNo, String phoneNo, int hostel, String email, String roomNo) {
  int p = -1;
  sendToClient("Waiting for valid finger to enroll as #"); 
  sendToClientln(String(id));
  
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
    case FINGERPRINT_OK:
      sendToClientln("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      sendToClient(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      sendToClientln("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      sendToClientln("Imaging error");
      break;
    default:
      sendToClientln("Unknown error");
      break;
    }
    
    // Check for client connection/disconnection
    if (clientConnected && !client.connected()) {
      clientConnected = false;
      sendToClientln("Client disconnected during enrollment");
    }
    
    if (!clientConnected) {
      WiFiClient newClient = server.available();
      if (newClient) {
        client = newClient;
        clientConnected = true;
        sendToClientln("New client connected during enrollment");
        sendToClient("Enrolling finger as ID #"); 
        sendToClientln(String(id));
      }
    }
    
    delay(100); // Small delay to prevent busy waiting
  }

  // Rest of the fingerprint enrollment process
  p = finger.image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK:
      sendToClientln("Image converted");
      beep();
      break;
    case FINGERPRINT_IMAGEMESS:
      sendToClientln("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      sendToClientln("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      sendToClientln("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      sendToClientln("Could not find fingerprint features");
      return p;
    default:
      sendToClientln("Unknown error");
      return p;
  }

  sendToClientln("Remove finger");
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }
  
  sendToClient("ID "); 
  sendToClientln(String(id));
  p = -1;
  sendToClientln("Place same finger again");
  
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
    case FINGERPRINT_OK:
      sendToClientln("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      sendToClient(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      sendToClientln("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      sendToClientln("Imaging error");
      break;
    default:
      sendToClientln("Unknown error");
      break;
    }
    
    // Check for client connection/disconnection
    if (clientConnected && !client.connected()) {
      clientConnected = false;
      sendToClientln("Client disconnected during second finger scan");
    }
    
    if (!clientConnected) {
      WiFiClient newClient = server.available();
      if (newClient) {
        client = newClient;
        clientConnected = true;
        sendToClientln("New client connected during enrollment");
        sendToClientln("Place same finger again");
      }
    }
    
    delay(100); // Small delay to prevent busy waiting
  }

  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      sendToClientln("Image converted");
      beep();
      break;
    case FINGERPRINT_IMAGEMESS:
      sendToClientln("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      sendToClientln("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      sendToClientln("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      sendToClientln("Could not find fingerprint features");
      return p;
    default:
      sendToClientln("Unknown error");
      return p;
  }

  sendToClient("Creating model for #");  
  sendToClientln(String(id));

  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    sendToClientln("Prints matched!");
    beep(1000);
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    sendToClientln("Communication error");
    return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    sendToClientln("Fingerprints did not match");
    return p;
  } else {
    sendToClientln("Unknown error");
    return p;
  }

  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) {
    sendToClientln("ERROR: Fingerprint matched existing template during enrollment!");
    return FINGERPRINT_DUPLICATE;
  }

  sendToClient("ID "); 
  sendToClientln(String(id));
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    sendToClientln("Stored!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    sendToClientln("Communication error");
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    sendToClientln("Could not store in that location");
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    sendToClientln("Error writing to flash");
    return p;
  } else {
    sendToClientln("Unknown error");
    return p;
  }


  if (p == FINGERPRINT_OK) {
    sendToClientln("Stored!");
    beep(200);

    String hostelName;
    if (hostel == 1) {
      hostelName = "Arch A";
    } else if (hostel == 2) {
      hostelName = "Arch B";
    } else if (hostel == 3) {
      hostelName = "Arch C";
    } else if (hostel == 4) {
      hostelName = "Arch D";
    } else {
      hostelName = "Other";
    }

    String jsonData = "{\"fingerprintID\":\"" + String(id) + 
                      "\",\"username\":\"" + name + 
                      "\",\"rollNo\":\"" + rollNo + 
                      "\",\"phoneNo\":\"" + phoneNo + 
                      "\",\"hostel\":\"" + String(hostel) + 
                      "\",\"hostelName\":\"" + hostelName + 
                      "\",\"email\":\"" + email + 
                      "\",\"roomNo\":\"" + roomNo + 
                      "\",\"status\":\"Stored Successfully\"}";
    
    sendToGoogleSheets(jsonData);
  } else {
    sendToClientln("Error storing fingerprint");
    String jsonData = "{\"fingerprintID\":\"" + String(id) + 
                      "\",\"username\":\"" + name + 
                      "\",\"status\":\"Error\"}";
    sendToGoogleSheets(jsonData);
  }

  return true;
}
