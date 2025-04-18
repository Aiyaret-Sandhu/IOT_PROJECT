#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>
#include <ArduinoJson.h>


#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C 

#define FINGERPRINT_DUPLICATE 0xEE  

const char* ssid = "wifi";
const char* password = "pass12345";
const char* serverName = "https://script.google.com/macros/s/AKfycbyHvPDI6xpPmoQ9zVZH3jX2xiLxXHyGit3ZNeJZbxAWvzXSvKztAeKUDdRCzj04cEn3SQ/exec";


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

void setup() {
  Serial.begin(115200);
  delay(2000);
  WiFi.begin(ssid, password);
  mySerial.begin(57600, SERIAL_8N1, 16, 17);
  finger.begin(57600);
  delay(2000);

  Wire.begin(21, 22);  

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");


  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  digitalWrite(SYSTEM_RUNNING_LED, LOW);  // Quick blink
  delay(200);
  digitalWrite(SYSTEM_RUNNING_LED, HIGH); 

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 10);
  display.println("Mess Management System");
  display.display();
  delay(2000);
  display.clearDisplay();

  // pinMode(BUTTON_LED, OUTPUT);
  // digitalWrite(BUTTON_LED, LOW); 

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

  while(true) {  // Infinite loop instead of ESP.restart()
    updateDisplay("Press D19 to Register");
    Serial.println("Press D19 within 5 sec to Register...");

    unsigned long startTime = millis();
    bool registerSelected = false;
    
    while (millis() - startTime < 5000) {
      if (digitalRead(BUTTON_REGISTER) == LOW) {
        registerSelected = true;

        digitalWrite(LED_REGISTER, HIGH);  // Turn on register LED
      Serial.println("Register button pressed - switching to registration mode");
      updateDisplay("Switching to register...");
      delay(1000);
      digitalWrite(LED_REGISTER, LOW);
      
        break;
      }
    }


    if (registerSelected) {
      Serial.println("Register Mode Selected!");
      updateDisplay("Register Mode...");
      delay(1000);
      registerFingerprint();
      // After registration completes, it will loop back here
    } else {
      Serial.println("Payment Mode Selected!");
      updateDisplay("Payment Mode...");
      delay(1000);
      paymentMode();  // We'll add this function
    }
  }
}



void beep(int duration = 200) {
  digitalWrite(BUZZER_PIN, HIGH);
  // digitalWrite(BUTTON_LED, HIGH);  // Turn LED ON
  delay(duration);                 // Buzzer duration
  digitalWrite(BUZZER_PIN, LOW);
  // delay(2000 - duration);          // Keep LED on for 2s total
  // digitalWrite(BUTTON_LED, LOW);   // Turn LED OFF
}

void clearFingerprintDatabase() {
  Serial.println("Clearing all stored fingerprints...");
  updateDisplay("Clearing database...");
  
  for (int i = 0; i <= 250; i++) {
    if (finger.deleteModel(i) == FINGERPRINT_OK) {
      Serial.print("Deleted ID ");
      Serial.println(i);
    } else {
      Serial.print("Failed to delete ID ");
      Serial.println(i);
    }
    delay(50);  // Small delay between deletions
  }
  
  Serial.println("All fingerprints deleted!");
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

  Serial.print("User: ");
  Serial.print(userName);
  Serial.print(" | Roll No: ");
  Serial.print(rollNo);
  Serial.print(" | Total Fees: Rs");
  Serial.println(totalFees);
  
  return userName;
}


void sendToGoogleSheets(String jsonData) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverName);
    http.addHeader("Content-Type", "application/json; charset=UTF-8");
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    Serial.print("Sending JSON: ");  
    Serial.println(jsonData);

    int httpResponseCode = http.POST(jsonData);
  
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);

    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
}



uint8_t readnumber(void) {
  uint8_t num = 0;

  while (num == 0) {
    while (! Serial.available());
    num = Serial.parseInt();
  }
  return num;
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
  Serial.println("Enter the name for this fingerprint:");
  while (name.length() == 0) {
    while (Serial.available() == 0);
    name = Serial.readStringUntil('\n');  
    name.trim(); 
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
    Serial.println("Found matching fingerprint with ID #" + String(finger.fingerID));
    return true;
  }
  return false;
}


bool isIdAvailableInSheets(uint8_t id) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected - cannot check ID availability");
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

void registerFingerprint() {
  // clearFingerprintDatabase();
  Serial.println("Ready to enroll a fingerprint!");
  updateDisplay("Ready to begin .....");
  
  while (true) {
    Serial.println("Please type in the ID # (from 1 to 250) you want to save this finger as...");
    id = readnumber();
    if (id == 0) return;

    // Check if ID exists in Google Sheets
    if (!isIdAvailableInSheets(id)) {
      Serial.println("ID " + String(id) + " is already registered in the system. Please choose a different ID.");
      updateDisplay("ID " + String(id) + " taken!");
      delay(2000);
      continue;
    }
    break;
  }

  Serial.println("Place finger to check for duplicates...");
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
    Serial.println("ERROR: This fingerprint is already registered under ID #" + String(finger.fingerID));
    beep(1000);
    updateDisplay("Fingerprint exists!\nID: " + String(finger.fingerID));
    
    // Clear the fingerprint buffer
    while(finger.getImage() != FINGERPRINT_NOFINGER);
    
    // Wait for user to remove finger and confirm
    Serial.println("Press D27 ( CONFIRM BUTTON ) to continue...");
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
  String rollNo = readField("Enter Roll Number: ");
  
  String email = readField("Enter Email: ");

  String otp = String(random(100000, 999999));
  
  // Send OTP email (but don't wait for response)
  String otpRequest = "{\"action\":\"sendOTP\",\"email\":\"" + email + "\",\"otp\":\"" + otp + "\",\"fingerprintID\":\"" + String(id) + "\"}";

  HTTPClient http;
  http.begin(serverName);
  http.addHeader("Content-Type", "application/json");
  http.POST(otpRequest); // Fire and forget
  http.end();

  Serial.println("OTP has been sent to your email");
  updateDisplay("Check email for OTP");

  // Get OTP from user
  String userOTP = readField("Enter OTP from email: ");

  // Simple client-side verification
  if (userOTP != otp) {
    Serial.println("OTP verification failed");
    updateDisplay("Invalid OTP!");
    delay(2000);
    return;
  }


  String phoneNo = readField("Enter Phone Number: ");

  int hostel = 0;
  while (hostel < 1 || hostel > 5) {
    Serial.println("Select Hostel:");
    Serial.println("1: Arch A");
    Serial.println("2: Arch B");
    Serial.println("3: Arch C");
    Serial.println("4: Arch D");
    Serial.println("5: Other");
    hostel = readnumber();
    
    if (hostel < 1 || hostel > 5) {
      Serial.println("Invalid selection! Please choose 1-5");
    }
  }

  String roomNo = readField("Enter Room Number: ");



  Serial.print("Enrolling ID #");
  Serial.println(id);
  updateDisplay("Enrolling...");

  // Check for duplicate fingerprint before enrolling
  if (!getFingerprintEnroll(name, rollNo, phoneNo, hostel, email, roomNo)) {
    Serial.println("Failed to enroll fingerprint");
    updateDisplay("Enroll failed!");
    delay(2000);
    return;
  }

  beep(1000);
  Serial.println("Registration Complete !");
  updateDisplay("Registered !");
  delay(2000);
  paymentMode();
}

String readField(const char* prompt) {
  Serial.println(prompt);
  String input = "";
  while (input.length() == 0) {
    while (Serial.available() == 0);
    input = Serial.readStringUntil('\n');  
    input.trim();
    
    // For OTP field, ensure it's 6 digits
    if (strstr(prompt, "OTP") != NULL && input.length() != 6) {
      Serial.println("OTP must be 6 digits");
      input = "";
      continue;
    }
  }
  return input;
}

bool sendOTPRequest(String jsonData) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected");
    return false;
  }

  HTTPClient http;
  http.begin(serverName);
  http.addHeader("Content-Type", "application/json");
  
  Serial.println("Sending OTP request:");
  Serial.println(jsonData);
  
  int httpCode = http.POST(jsonData);
  String response = http.getString();
  
  Serial.print("HTTP Response code: ");
  Serial.println(httpCode);
  Serial.print("Response: ");
  Serial.println(response);
  
  bool success = (httpCode == 200);
  
  http.end();
  return success;
}

bool verifyOTP(String jsonData) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected for OTP verification");
    return false;
  }

  HTTPClient http;
  http.begin(serverName);
  http.addHeader("Content-Type", "application/json");
  
  Serial.println("Sending OTP verification:");
  Serial.println(jsonData);
  
  int httpCode = http.POST(jsonData);
  
  if (httpCode == 200) {
    String response = http.getString();
    Serial.print("OTP verification response: ");
    Serial.println(response);
    
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, response);
    
    if (!error && doc["success"] == true && doc["verified"] == true) {
      http.end();
      Serial.println("OTP verification successful");
      return true;
    } else {
      Serial.println("OTP verification failed in response parsing");
    }
  } else {
    Serial.print("OTP verification HTTP error: ");
    Serial.println(httpCode);
  }
  
  http.end();
  return false;
}

void paymentMode() {

  // beep(10000);
  // delay(10000);  

  while(true) {
    Serial.println("Waiting for fingerprint...");
    updateDisplay("Place Finger...");

    unsigned long startTime = millis();
    int userID = -1;
    
    while (userID == -1) {
      // Check for fingerprint
      userID = getFingerprintID();
      
      
      if (digitalRead(BUTTON_REGISTER) == LOW) {
        digitalWrite(LED_REGISTER, HIGH);
        Serial.println("Register button pressed - switching to registration mode");
        updateDisplay("Switching to register...");
        delay(1000);
        digitalWrite(LED_REGISTER, LOW);
        registerFingerprint(); 
        return;  // Exit payment mode after registration completes
      }
      
      // Small delay to prevent busy-waiting
      delay(100);
      
      // Optional timeout (e.g., 30 seconds)
      if (millis() - startTime > 5000) {
        Serial.println("Timeout waiting for fingerprint");
        updateDisplay("Timeout - Retrying...");
        delay(1000);
        break;  // Break out of the inner while loop
      }
    }

    if (userID == -1) {
      // Fingerprint not recognized or timeout
      Serial.println("Fingerprint not recognized!");
      updateDisplay("Not recognized!");
      delay(2000);
      continue;
    }

    // Found user, get name
    String rollNo;
    String userName = getUserName(userID, rollNo);
    // updateDisplay("User: " + userName);
    delay(3000);

    // Select amount using buttons
    int selectedAmount = 0;
    bool paymentDone = false;
    
    while (!paymentDone) {
      updateDisplay("Total: Rs : " + String(selectedAmount) + "\nPress Confirm Button to Confirm");
      
      if (digitalRead(BUTTON_AMOUNT_10) == LOW) {
        selectedAmount += 10;
        updateDisplay("Added 10...");
        Serial.println("Added 10...");
        delay(300);
      } 
      else if (digitalRead(BUTTON_AMOUNT_20) == LOW) {
        selectedAmount += 20;
        updateDisplay("Added 20...");
        Serial.println("Added 20...");
        delay(300);
      } 
      else if (digitalRead(BUTTON_AMOUNT_50) == LOW) {
        selectedAmount += 50;
        updateDisplay("Added 50...");
        Serial.println("Added 50...");
        delay(300);
      }
      else if(digitalRead(BUTTON_AMOUNT_100) == LOW) {
        selectedAmount += 100;
        updateDisplay("Added 100...");
        Serial.println("Added 100...");
        delay(300);
      }
      else if (digitalRead(BUTTON_CONFIRM) == LOW) {
        digitalWrite(LED_CONFIRM, HIGH);
        if (selectedAmount > 0) {
          updateDisplay("Processing...");
          Serial.println("Processing...");
          String paymentData = "{\"fingerprintID\":\"" + String(userID) + 
                   "\",\"username\":\"" + userName + 
                   "\",\"status\":\"Paid " + String(selectedAmount, DEC) + "\"}";
          sendToGoogleSheets(paymentData);
          beep(300);
          updateDisplay("Paid: Rs : " + String(selectedAmount));
          Serial.println("Paid: Rs : " + String(selectedAmount));
          delay(2000);
        }
        digitalWrite(LED_CONFIRM, LOW);
        paymentDone = true; // Mark payment as complete
      }
      delay(50);
    }
    
    if (paymentDone) {
      return; // Exit payment mode after successful transaction
    }
  }
}


void loop() {
  
    
}


uint8_t getFingerprintEnroll(String name, String rollNo, String phoneNo, int hostel, String email, String roomNo) {

  int p = -1;
  Serial.print("Waiting for valid finger to enroll as #"); Serial.println(id);
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      // beep();
      break;
    case FINGERPRINT_NOFINGER:
      Serial.print(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      break;
    default:
      Serial.println("Unknown error");
      break;
    }
  }

  // OK success!

  p = finger.image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      beep();
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  Serial.println("Remove finger");
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }
  Serial.print("ID "); Serial.println(id);
  p = -1;
  Serial.println("Place same finger again");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      // beep();
      break;
    case FINGERPRINT_NOFINGER:
      Serial.print(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      break;
    default:
      Serial.println("Unknown error");
      break;
    }
  }

  // OK success!

  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      beep();
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK converted!
  Serial.print("Creating model for #");  Serial.println(id);

  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    Serial.println("Prints matched!");
    beep(1000);
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    Serial.println("Fingerprints did not match");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }

  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) {
    Serial.println("ERROR: Fingerprint matched existing template during enrollment!");
    return FINGERPRINT_DUPLICATE;
  }

  Serial.print("ID "); Serial.println(id);
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("Stored!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Could not store in that location");
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Error writing to flash");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }


  if (p == FINGERPRINT_OK) {
  Serial.println("Stored!");
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
    Serial.println("Error storing fingerprint");
    String jsonData = "{\"fingerprintID\":\"" + String(id) + 
                     "\",\"username\":\"" + name + 
                     "\",\"status\":\"Error\"}";
    sendToGoogleSheets(jsonData);
  }


  return true;
}
