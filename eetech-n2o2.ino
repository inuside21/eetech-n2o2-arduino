// EEPROM
#include <EEPROM.h>
int addrHumiTrigger = 0;
int addrHumiOffset = 21;
int addrWifiSSID = 41;
int addrWifiPass = 61;
int addrDeviceID = 81;
int addrTotalOff = 91;

// WIFI
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
ESP8266WiFiMulti WiFiMulti;
String deviceID = "1";
String serverAddress = "http://192.168.11.74/";   // put slash at end
String wifiSSID = "EETechAP-N2S";
String wifiPass = "12345678";
String wifiConnected = "DC";
bool isWifiConnected = false;

// LCD
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Temp
//#include "DHT.h"
//#define DHTPIN 14
//#define DHTTYPE DHT11
//DHT dht(DHTPIN, DHTTYPE);
String dTemp = "0";                               // Data Temp
String dHumi = "0";                               // Data Humi
String dHumiTrigger = "60";                       // >= 60 trigger solenoid
String dHumiTriggerEeprom = "";
String dHumiOffset = "0";
String dTriggered = "0";
unsigned long dTotalOff = 0;                      // Internal tracking for total seconds turned off

float tempSensorOxygen = 0;
bool isSensorOxygenReading = false;

// O2
String dOxygen = "0";
int oxygenPin = A0;

// Input
int button1 = D6;                                 // up
int button2 = D5;                                 // down

// Output
int pinSolenoid = D7;                             // solenoid relay
int pinOxygen = D8;                               // oxygen relay

// Screen
int currentScreen = 0;                            // 0 - main
                                                  // 1 - set humi

// Mode
int localMode = 0;                                // 0 - offline
                                                  // 1 -

// Debounce
unsigned long timerfunction1 = 0;
unsigned long timerSerial = 0;
unsigned long timerWifi = 0;
unsigned long timerDelayDisplay = 0;
unsigned long timerButton1 = 0;
unsigned long timerSensorOxygen = 0;
unsigned long timerSensorOxygenOff = 0;           

unsigned long debouncer025 = 250;
unsigned long debouncer05 = 500;
unsigned long debouncer1 = 1000;
unsigned long debouncer5 = 5000;
unsigned long debouncer10 = 10000;
unsigned long debouncer15 = 15000;
unsigned long debouncer30 = 30000;
unsigned long debouncer60 = 60000;

// START
// ==============================
void setup()
{
  //
  EEPROM.begin(512);

  // Serial
  Serial.begin(9600);
  
  // LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("     EETECH     ");
  lcd.setCursor(0, 1);
  lcd.print("   INNOVATION   ");
  delay(1000);
  Serial.println("Type dcmd to view all commands");
  Serial.println("BOOT - OK");
  
  // EEPROM
  {
    //
    bool isSetupOK = true;

    // DEVICE INFO
    {
      deviceID = readStringFromEEPROM(addrDeviceID, 20).c_str();

      // Default
      if (deviceID.length() <= 0 || deviceID.length() > 3)
      {
        deviceID = "1";
        writeStringToEEPROM(addrDeviceID, deviceID);

        isSetupOK = false;
        Serial.println("EEPROM - DEVICE LOAD FAILED");
      }
      else
      {
        Serial.println("EEPROM - DEVICE LOAD OK");
      }
    }

    // WIFI
    {
      wifiSSID = readStringFromEEPROM(addrWifiSSID, 20).c_str();
      wifiPass = readStringFromEEPROM(addrWifiPass, 20).c_str();

      // Default
      if (wifiSSID.length() < 8 || wifiPass.length() < 8)
      {
        wifiSSID = "EETechAP-N2S";
        wifiPass = "12345678";
        writeStringToEEPROM(addrWifiSSID, wifiSSID);
        writeStringToEEPROM(addrWifiPass, wifiPass);

        isSetupOK = false;
        Serial.println("EEPROM - WIFI LOAD FAILED");
      }
      else
      {
        Serial.println("EEPROM - WIFI LOAD OK");
      }
    }

    // Temp / Humi
    {
      // Humi Trigger
      if (!checkIfNumber(readStringFromEEPROM(addrHumiTrigger, 20).c_str()))
      {
        dHumiTrigger = "45";
        dHumiTriggerEeprom = "45";
        writeStringToEEPROM(addrHumiTrigger, dHumiTrigger);

        isSetupOK = false;
        Serial.println("EEPROM - TEMP/HUMI TRIGGER LOAD FAILED");
      }
      else
      {
        dHumiTrigger = readStringFromEEPROM(addrHumiTrigger, 20).c_str();
        dHumiTriggerEeprom = dHumiTrigger;

        Serial.println("EEPROM - TEMP/HUMI TRIGGER LOAD OK");
      }

      // Humi Offset
      if (!checkIfNumber(readStringFromEEPROM(addrHumiOffset, 20).c_str()))
      {
        dHumiOffset = "0";
        writeStringToEEPROM(addrHumiOffset, dHumiOffset);

        isSetupOK = false;
        Serial.println("EEPROM - TEMP/HUMI OFFSET LOAD FAILED");
      }
      else
      {
        dHumiOffset = readStringFromEEPROM(addrHumiOffset, 20).c_str();

        Serial.println("EEPROM - TEMP/HUMI OFFSET LOAD OK");
      }
    }

    // Total
    {
      // Default
      if (!checkIfNumber(readStringFromEEPROM(addrTotalOff, 20).c_str()))
      {
        dTotalOff = 0;
        writeStringToEEPROM(addrTotalOff, String(dTotalOff));

        isSetupOK = false;
        Serial.println("EEPROM - DATA TOTAL OFF LOAD FAILED");
      }
      else
      {
        String tempDataTotalOff = readStringFromEEPROM(addrTotalOff, 20).c_str();
        dTotalOff = tempDataTotalOff.toInt();

        Serial.println("EEPROM - DATA TOTAL OFF LOAD OK");
      }
    }

    // Complete
    if (isSetupOK)
    {
      //
      lcd.setCursor(0, 0);
      lcd.print("    N2 SETUP    ");
      lcd.setCursor(0, 1);
      lcd.print("     LOADED     ");
    }

    // Failed
    else
    {
      //
      lcd.setCursor(0, 0);
      lcd.print("    N2 SETUP    ");
      lcd.setCursor(0, 1);
      lcd.print("  LOAD DEFAULT  ");
    }
  }

  // Wifi
  {
    WiFi.mode(WIFI_STA);
    WiFiMulti.addAP(wifiSSID.c_str(), wifiPass.c_str());
    int timeOut = 0;
    while (WiFiMulti.run() != WL_CONNECTED)
    {
      lcd.setCursor(0, 0);
      lcd.print("   CONNECTING   ");
      lcd.setCursor(0, 1);
      lcd.print("  " + wifiSSID + "  ");
      timeOut = timeOut + 1;
      Serial.println("WIFI - CONNECTING TO " + wifiSSID);

      if (timeOut >= 1)
      {
        Serial.println("WIFI - CONNECT TIMEOUT");
        break;
      }
    }

    if (WiFiMulti.run() == WL_CONNECTED)
    {
      lcd.setCursor(0, 0);
      lcd.print("   CONNECTION   ");
      lcd.setCursor(0, 1);
      lcd.print("    SUCCESS!    ");

      isWifiConnected = true;
      Serial.println("WIFI - CONNECT OK @ " + WiFi.localIP().toString());
      delay(2000);
    }
    else
    {
      lcd.setCursor(0, 0);
      lcd.print("   CONNECTION   ");
      lcd.setCursor(0, 1);
      lcd.print("     FAILED     ");

      isWifiConnected = false;
      Serial.println("WIFI - CONNECT FAILED");
      delay(2000);
    }
  }

  // Input
  pinMode(button1, INPUT_PULLUP);
  pinMode(button2, INPUT_PULLUP);

  // Output
  pinMode(pinSolenoid, OUTPUT);
  pinMode(pinOxygen, OUTPUT);

  //
  Serial.println("Type dcmd to view all commands");
}


// LOOP
// ==============================
void loop()
{
  // Wifi
  //WiFiMulti.run();

  // Temp
  float readTemp = 0; //dht.readTemperature();
  float readHumi = 0; //dht.readHumidity();

  // O2
  unsigned long readOxygen = analogRead(A0); 
  if (isSensorOxygenReading)
  {
    //
    if (tempSensorOxygen < readOxygen)
    {
      tempSensorOxygen = readOxygen;
    }

    //float readOxygen = analogRead(A0); 
    //readOxygen = readOxygen / 1023.0;
    //Serial.println("read: " + String(readOxygen) + " tempo:" + String(tempSensorOxygen));
  }
  else
  {
    
  }

  lcd.setCursor(0, 0);
  lcd.print(String(readOxygen));
  lcd.setCursor(0, 1);
  lcd.print(String(tempSensorOxygen));

  delay(1000);

  // OFFSET
  readHumi = readHumi + dHumiOffset.toFloat();

  // OK?
  if (readHumi < 100)
  {
    dTemp = ConvertNumber(readTemp);
    dHumi = ConvertNumber(readHumi);
  }
  else
  {
    readTemp = dTemp.toFloat();
    readHumi = dHumi.toFloat();
  }


  // Button
  int button1Val = digitalRead(button1);
  int button2Val = digitalRead(button2);

  // Screen - Main
  if (currentScreen == 0)
  {
    // LCD
    {
      // Active Saving
      if (readHumi < dHumiTrigger.toFloat())
      {
        lcd.setCursor(0, 0);
        lcd.print("N2:OFF SR:" + ConvertNumber(dHumiTrigger.toFloat()) + "%");
        lcd.setCursor(0, 1);
        lcd.print("TH:" + ConvertNumber3(int((dTotalOff / 60) / 60)) + " CR:" + dHumi + "%");
      }

      // Active N2
      if (readHumi >= dHumiTrigger.toFloat())
      {
        lcd.setCursor(0, 0);
        lcd.print("N2:ON  SR:" + ConvertNumber(dHumiTrigger.toFloat()) + "%");
        lcd.setCursor(0, 1);
        lcd.print("TH:" + ConvertNumber3(int((dTotalOff / 60) / 60)) + " CR:" + dHumi + "%");
      }
    }

    // Button
    {
      // up
      if (!button1Val)
      {
        timerDelayDisplay = millis();
        timerButton1 = millis();
        currentScreen = 1;
        return;
      }

      // down
      if (!button2Val)
      {
        timerDelayDisplay = millis();
        timerButton1 = millis();
        currentScreen = 1;
        return;
      }
    }
  }

  // Screen - Setup
  else
  {
    if ((millis() - timerDelayDisplay) > debouncer5)
    {
      // SAVE
      WriteHumiTrigger();

      timerDelayDisplay = millis();
      currentScreen = 0;
      return;
    }

    // LCD
    {
      lcd.setCursor(0, 0);
      lcd.print("SR:" + ConvertNumber2(dHumiTrigger) + " OFFSET:" + ConvertNumberSpace(dHumiOffset));
      lcd.setCursor(0, 1);
      lcd.print("WF:" + wifiConnected + "     ID:" + ConvertNumberSpace(deviceID));
    }

    // Button
    {
      // Up Button
      if (!button1Val)
      {
        if ((millis() - timerButton1) > debouncer025)
        {
          timerDelayDisplay = millis();
          timerButton1 = millis();

          int temp_dHumiTrigger = dHumiTrigger.toInt();

          if (temp_dHumiTrigger >= 99)
          {
            temp_dHumiTrigger = 20;
          }
          else
          {
            temp_dHumiTrigger = temp_dHumiTrigger + 1;
          }

          dHumiTrigger = String(temp_dHumiTrigger);
        }
      }
    }
  }

  // Sensor - Oxygen
  {
    // turn on
    if ((millis() - timerSensorOxygen) > debouncer60)
    {
      //
      timerSensorOxygen = millis();
      timerSensorOxygenOff = millis();

      //
      tempSensorOxygen = 0;
      isSensorOxygenReading = true;
      digitalWrite(pinOxygen, HIGH);

      //
      return;
    }

    // turn off
    if (isSensorOxygenReading)
    {
      //
      if ((millis() - timerSensorOxygenOff) > debouncer15)
      {
        //
        timerSensorOxygenOff = millis();

        //
        isSensorOxygenReading = false;
        digitalWrite(pinOxygen, LOW);

        //
        return;
      }
    }
  }

  // Relay - Solenoid
  {
    /*
    // Active Saving
    if (readHumi <= dHumiTrigger.toFloat() - 5)
    {
      dTriggered = "0";
      digitalWrite(pinSolenoid, LOW);
    }

    // Active N2
    if (readHumi >= dHumiTrigger.toFloat())
    {
      dTriggered = "1";
      digitalWrite(pinSolenoid, HIGH);
    }
    */
  }

  // Wifi Reconnect
  {
    /*
    if ((millis() - timerWifi) > debouncer60)
    {
      timerWifi = millis();

      if (WiFiMulti.run() == WL_CONNECTED)
      {
        wifiConnected = "OK";
        isWifiConnected = true;
      }
      else
      {
        wifiConnected = "DC";
        isWifiConnected = false;
      }
    }
    */
  }

  // Web
  {
    if (isWifiConnected)
    {
      WiFiClient client;
      HTTPClient http;
      String serverUrl = serverAddress + "";
      if (http.begin(client, serverUrl)) 
      {
        int httpCode = http.GET();
        if (httpCode > 0) {
          if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
            String payload = http.getString();
            Serial.println(payload);
          }
        }
      }

      http.end();
    }
  }

  // 1 Sec Loop
  {
    if ((millis() - timerfunction1) > debouncer1)
    {
      timerfunction1 = millis();

      // Saving
      {
        if (readHumi < dHumiTrigger.toFloat())
        {
          dTotalOff = dTotalOff + 1;

          if (dTotalOff % 3600 == 0)
          {
            writeStringToEEPROM(addrTotalOff, String(dTotalOff));
          }
        }
      }
    }
  }

  // Serial
  {
    // READ
    if (Serial.available() > 0)
    {
      //
      String incomingString = Serial.readString();

      //
      int wDeviceIdIndex = incomingString.indexOf("wifiid ");
      int wSSIDIndex = incomingString.indexOf("wifissid ");
      int wPassIndex = incomingString.indexOf("wifipass ");
      int tHumiReadIndex = incomingString.indexOf("humiread");
      int tHumiTriggerIndex = incomingString.indexOf("humitrigger ");
      int tHumiOffIndex = incomingString.indexOf("humioff ");
      int tTotalOffIndex = incomingString.indexOf("totaloff ");
      int tTotalOffClearIndex = incomingString.indexOf("totaloffclear");
      int dStatusIndex = incomingString.indexOf("dstatus");
      int dCommandIndex = incomingString.indexOf("dcmd");

      //
      if (wDeviceIdIndex != -1)
      {
        //
        String result = incomingString.substring(wDeviceIdIndex + 7);
        result.trim();

        if (result.length() > 0 && result.length() <= 3)
        {
          writeStringToEEPROM(addrDeviceID, result);
          Serial.println("DEVICE ID Updated - Please reset the device to take effect");
        }
        else
        {
          Serial.println("DEVICE ID must 1 to 3 char length");
        }

        //
        Serial.flush();
        return;
      }

      //
      if (wSSIDIndex != -1)
      {
        //
        String result = incomingString.substring(wSSIDIndex + 9);
        result.trim();

        if (result.length() == 12)
        {
          writeStringToEEPROM(addrWifiSSID, result);
          Serial.println("WIFI SSID Updated - Please reset the device to take effect");
        }
        else
        {
          Serial.println("WIFI SSID must 12 char length");
        }

        //
        Serial.flush();
        return;
      }

      //
      if (wPassIndex != -1)
      {
        //
        String result = incomingString.substring(wPassIndex + 9);
        result.trim();

        if (result.length() == 8)
        {
          writeStringToEEPROM(addrWifiPass, result);
          Serial.println("WIFI PASS Updated - Please reset the device to take effect");
        }
        else
        {
          Serial.println("WIFI PASS must 8 char length");
        }

        //
        Serial.flush();
        return;
      }

      //
      if (tHumiReadIndex != -1)
      {
        //
        Serial.println("HUMI READ " + dHumi);

        //
        Serial.flush();
        return;
      }

      //
      if (tHumiTriggerIndex != -1)
      {
        //
        String result = incomingString.substring(tHumiTriggerIndex + 12);
        result.trim();

        if (result.length() > 0 && result.length() <= 3 && checkIfNumber(result.c_str()) && result.toInt() >= 20 && result.toInt() <= 99)
        {
          writeStringToEEPROM(addrHumiTrigger, result);
          Serial.println("HUMI TRIGGER Updated - Please reset the device to take effect");
        }
        else
        {
          Serial.println("HUMI TRIGGER must between 20 and 99");
        }

        //
        Serial.flush();
        return;
      }

      //
      if (tHumiOffIndex != -1)
      {
        //
        String result = incomingString.substring(tHumiOffIndex + 8);
        result.trim();

        if (result.length() > 0 && result.length() <= 3 && checkIfNumber(result.c_str()))
        {
          writeStringToEEPROM(addrHumiOffset, result);
          Serial.println("HUMI OFFSET Updated - Please reset the device to take effect");
        }
        else
        {
          Serial.println("HUMI OFFSET must between -99 and 99 res " + result);
        }

        //
        Serial.flush();
        return;
      }

      //
      if (tTotalOffIndex != -1)
      {
        //
        String result = incomingString.substring(tTotalOffIndex + 9);
        result.trim();

        if (result.length() > 0 && result.length() <= 8 && checkIfNumber(result.c_str()))
        {
          writeStringToEEPROM(addrTotalOff, result);
          Serial.println("DATA TOTAL OFF Updated - Please reset the device to take effect");
        }
        else
        {
          Serial.println("DATA TOTAL OFF must 0 or more, 8 max characters");
        }

        //
        Serial.flush();
        return;
      }

      //
      if (tTotalOffClearIndex != -1)
      {
        String clearData = "0";
        writeStringToEEPROM(addrTotalOff, clearData);
        Serial.println("DATA TOTAL OFF Updated - Please reset the device to take effect");

        //
        Serial.flush();
        return;
      }

      //
      if (dStatusIndex != -1)
      {
        Serial.println("------------------------------------------");
        Serial.println("Device Status");
        Serial.println("------------------------------------------");
        Serial.println("DEVICE - ID - " + deviceID);
        Serial.print("DEVICE - RAM FREE "); Serial.print(ESP.getFreeHeap() / 1024.0) + Serial.println(" KB");
        Serial.println("DATA - TOTAL OFF SEC - " + String(dTotalOff));
        Serial.println("DATA - TOTAL OFF HRS - " + String(int((dTotalOff / 60) / 60)));
        Serial.println("TEMP/HUMI - LOOP TEMP - " + dTemp);
        Serial.println("TEMP/HUMI - LOOP HUMI - " + dHumi);
        Serial.println("TEMP/HUMI - LOOP HUMI TRIGGER - " + dHumiTrigger);
        Serial.println("TEMP/HUMI - LOOP HUMI OFFSET - " + dHumiOffset);
        Serial.println("TEMP/HUMI - TRIGGERED - " + dTriggered);
        Serial.println("WIFI - LOOP SSID - " + wifiSSID);
        Serial.println("WIFI - LOOP PASS - " + wifiPass);

        if (isWifiConnected)
        {
          Serial.println("WIFI - LOOP Connected - True");
          Serial.println("WIFI - LOOP IP - " + WiFi.localIP().toString());
        }
        else
        {
          Serial.println("WIFI - LOOP Connected - False");
          Serial.println("WIFI - LOOP IP - NA");
        }

        Serial.println("------------------------------------------");

        //
        Serial.flush();
        return;
      }

      //
      if (dCommandIndex != -1)
      {
        Serial.println("------------------------------------------");
        Serial.println("Serial Command List");
        Serial.println("------------------------------------------");
        Serial.println("wifiid ??? - Modify Device ID");
        Serial.println("wifissid ???????????? - Modify Device SSID");
        Serial.println("wifipass ???????? - Modify Device Password");
        Serial.println("humiread - Read humidity (w/o offset)");
        Serial.println("humitrigger ?? - Modify Trigger for humidity (20 to 99)");
        Serial.println("humioff ??? - Modify Offset of humidity (-99 to 99)");
        Serial.println("totaloff ???????? - Modify Total Hours OFF (8 digits)");
        Serial.println("totaloffclear - Clear Total Hours OFF");
        Serial.println("dstatus - Serial log");
        Serial.println("------------------------------------------");

        //
        Serial.flush();
        return;
      }
    }

    // DEBUG
    if ((millis() - timerSerial) > debouncer1)
    {
      timerSerial = millis();


    }
  }
}


// WEB
// ==============================



// FUNCTIONS, FORMATS
// ==============================
void WriteHumiTrigger() {
  if (dHumiTrigger != dHumiTriggerEeprom)
  {
    writeStringToEEPROM(addrHumiTrigger, dHumiTrigger);
    dHumiTriggerEeprom = dHumiTrigger;
    delay(1000);
  }
}

String ConvertNumber(float x) {
  // result will be 00.00

  x = round(x * 100.0) / 100.0;
  String newx = String(x);

  if (x < 10)
  {
    newx = "0" + newx;
  }

  if (x <= 0)
  {
    newx = "00.00";
  }

  return newx;
}

String ConvertNumber2(String x) { // result will be 00

  int s = x.toInt();
  String newx = String(s);

  if (x.length() <= 0)
  {
    newx = "00";
  }

  return newx;
}

String ConvertNumber3(unsigned long x) { // result will be 000

  x = int(x);
  String newx = String(x);

  if (x < 100)
  {
    newx = "0" + newx;
  }

  if (x < 10)
  {
    newx = "0" + newx;
  }

  if (x <= 0)
  {
    newx = "000";
  }

  return newx;
}

String ConvertNumberSpace(String x) { // result will be "   "

  if (x.length() == 2)
  {
    x = " " + x;
  }

  if (x.length() == 1)
  {
    x = "  " + x;
  }

  if (x.length() <= 0)
  {
    x = "   ";
  }

  return x;
}

void writeStringToEEPROM(int address, const String &data) {
  for (int i = 0; i < data.length(); ++i) {
    EEPROM.write(address + i, data[i]);
  }

  // Write a null terminator at the end of the string
  EEPROM.write(address + data.length(), '\0');
  EEPROM.commit();
}

String readStringFromEEPROM(int address, int length) {
  String result = "";
  for (int i = 0; i < length; ++i) {
    char character = EEPROM.read(address + i);
    if (character == '\0') {
      break; // Stop reading if null terminator is encountered
    }
    result += character;
  }
  return result;
}

bool checkIfNumber(const char* str) {

  // use .c_str() to string

  // Check if the string is empty
  if (!str || *str == '\0') {
    return false;
  }

  bool isNumber = true;
  int i = 0;

  // Check for a minus sign at the beginning
  if (str[i] == '-') {
    ++i;  // Move to the next character if a minus sign is present
  }

  // Check the remaining characters
  for (; str[i] != '\0'; ++i) {
    if (!isdigit(str[i])) {
      isNumber = false;
      break;
    }
  }

  return isNumber;
}
