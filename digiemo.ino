//Device 123 username - Buyer - abcde, Owner - abcd

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <SPI.h>
#include <ezButton.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "Arduino.h"
#include "Audio.h"
#include <DigiemoIcons.h>
// #include <List.hpp>
#include "ListLib.h"


// Digital I/O used
#define I2S_DOUT 25
#define I2S_BCLK 27
#define I2S_LRC 26

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define DEBOUNCE_TIME 50              //50 millisecond
#define SHORT_PRESS_TIME 1000         // 1500 milliseconds
#define LONG_PRESS_TIME 1500          // 1500 milliseconds
#define LONG_PRESS_TIME_TO_HOME 3000  //4000 miliseconds

ezButton down_Button(15);   // create ezButton object that attach to pin GIOP15;
ezButton select_Button(4);  // create ezButton object that attach to pin GIOP4;
ezButton up_Button(5);      // create ezButton object that attach to pin GIOP5;

const char *ssid = "SLT-ADSL-90B57";
const char *password = "3osni8gjsk";

Audio audio;

String serverName = "http://52.14.102.194/api/";
// String emojiApi = "emoji/emoji/";
String emojiApi = "attachment/123/emoji?";
String voiceApi = "attachment/123/voice?";

const char *mqtt_broker = "52.14.102.194";
const char *topic = "digiemo/client/abcd/123";
const char *mqtt_username = "digiemo";
const char *mqtt_password = "DigiemoAPIMqttAdmin@321";
const int mqtt_port = 1883;

WiFiClient wifi_1;
WiFiClient wifi_2;
PubSubClient mqttClient(wifi_1);

bool notificationStatus = false;


unsigned long pressedTime = 0;   //pressed time will be stored
unsigned long releasedTime = 0;  //release time will be stored

int sound = 3;
int screenTimeout = 30;
int contrast = 0;

int menuitem = 1;
int page = 1;
bool startToPlay = false;

int upButton = 5;
int downButton = 15;
int selectButton = 4;

volatile boolean up = false;
volatile boolean down = false;
volatile boolean middle = false;

int downButtonState = 0;
int upButtonState = 0;
int selectButtonState = 0;
int lastDownButtonState = 0;
int lastSelectButtonState = 0;
int lastUpButtonState = 0;

struct message {
  String token;
  String emoji;
  String voiceId;
};

List<message> messageList = List<message>(5);

bool playComplete = true;
int selectedMsg = 0;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void setup() {

  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connected to the WiFi network!");
  //Serial.println(WiFi.localIP());

  mqttClient.setServer(mqtt_broker, mqtt_port);
  mqttClient.setCallback(callback);
  while (!mqttClient.connected()) {
    String client_id = String(WiFi.macAddress());

    if (mqttClient.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("Connected to Public MQTT Broker");
    } else {
      Serial.print("Failed to connect with MQTT Broker");
      Serial.print(mqttClient.state());
      delay(2000);
    }
  }

  mqttClient.publish("digiemo/master", "\{username:\"abcd\", deviceId:\"123\", status:\"Connected\"\}");
  mqttClient.subscribe(topic);

  pinMode(upButton, INPUT_PULLUP);
  pinMode(downButton, INPUT_PULLUP);
  pinMode(selectButton, INPUT_PULLUP);

  select_Button.setDebounceTime(DEBOUNCE_TIME);  // set debounce time to 50 milliseconds
  down_Button.setDebounceTime(DEBOUNCE_TIME);    // set debounce time to 50 milliseconds
  up_Button.setDebounceTime(DEBOUNCE_TIME);      // set debounce time to 50 milliseconds

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  //start the display
  display.clearDisplay();                     //clear display

  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(20);  // 0...21
}

//callback
void callback(char *topic, byte *payload, unsigned int length) {
  Serial.print("Message arrived in topic: ");
  Serial.println(topic);
  Serial.print("Message:");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println("");

  //payload parsing
  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, payload);
  JsonObject obj = doc.as<JsonObject>();
  // Test if parsing succeeds.
  if (error) {
    Serial.println("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }

  String type = obj["type"];
  notificationStatus = obj["status"];

  if (type != "null") {
    // Serial.println(type);
    // Serial.println(token);
    if (type == "NOTIFICATION") {

      if (notificationStatus == true) {
        display.clearDisplay();
        display.drawBitmap(0, 0, notification1, 128, 64, 1);
        display.display();
      }
    }
    if (type == "MESSAGE") {
      notificationStatus = true;
      String emoji = obj["emoji"];
      String voiceId = obj["voiceId"];
      String token = obj["token"];

      display.clearDisplay();
      display.drawBitmap(0, 0, notification1, 128, 64, 1);
      display.display();

      messageQueue(emoji, voiceId, token);
    }
  }
}
//message queue
void messageQueue(String e, String v, String t) {
  message msg = { t, e, v };
  messageList.Add(msg);
  if (messageList.Count() > 3) {
    messageList.Remove(0);
  }
  for (int i = 0; i < messageList.Count(); i++) {
    message m = messageList[i];
    Serial.println("Message " + String(i) + ", token - " + m.token);
  }
}
//message playing
void playMessages(message msg) {
  if (msg.emoji != "null") {
    //Serial.println("emo " + msg.emoji);
    getEmoji(msg.emoji, msg.token);
  }
  if (msg.voiceId != "null") {
    //Serial.println("voice " + msg.voiceId);
    getVoice(msg.voiceId, msg.token);
  }
}

void loop() {

  mqttClient.loop();
  audio.loop();
  select_Button.loop();
  up_Button.loop();
  down_Button.loop();


  downButtonState = digitalRead(downButton);
  selectButtonState = digitalRead(selectButton);
  upButtonState = digitalRead(upButton);

  checkIfDownButtonIsPressed();
  checkIfUpButtonIsPressed();
  checkIfSelectButtonIsPressed();


  if (notificationStatus == false) {

    drawMenu();

    if (up && page == 2) {
      up = false;
      menuitem--;
      if (menuitem == 0) {
        menuitem = 2;
      }
    } else if (up && page == 3) {
      up = false;
      menuitem--;
      if (menuitem == 0) {
        menuitem = 4;
      }
    } else if (up && page == 4) {
      up = false;
      menuitem--;
      if (menuitem == 0) {
        menuitem = 2;
      }
    } else if (up && page == 5) {
      up = false;
      menuitem--;
      if (menuitem == 0) {
        menuitem = 2;
      }
    } else if (up && page == 6) {
      up = false;
      sound--;
      if (sound == -1) {
        sound = 0;
      }
    } else if (up && page == 7) {
      up = false;
      menuitem--;
      if (menuitem == 0) {
        menuitem = 2;
      }
    } else if (up && page == 8) {
      up = false;
      screenTimeout -= 10;
      if (screenTimeout == -10) {
        screenTimeout = 0;
      }
    } else if (up && page == 9) {
      up = false;
      contrast--;
      if (contrast == -6) {
        contrast = -5;
      }
    }
    if (down && page == 2) {
      down = false;
      menuitem++;
      if (menuitem == 3) {
        menuitem = 1;
      }
    } else if (down && page == 3) {
      down = false;
      menuitem++;
      if (menuitem == 5) {
        menuitem = 0;
      }
    } else if (down && page == 4) {
      down = false;
      menuitem++;
      if (menuitem == 3) {
        menuitem = 1;
      }
    } else if (down && page == 5) {
      down = false;
      menuitem++;
      if (menuitem == 3) {
        menuitem = 1;
      }
    } else if (down && page == 6) {
      down = false;
      sound++;
      if (sound == 4) {
        sound = 3;
      }
    } else if (down && page == 7) {
      down = false;
      menuitem++;
      if (menuitem == 3) {
        menuitem = 1;
      }
    } else if (down && page == 8) {
      down = false;
      screenTimeout += 10;
      if (screenTimeout == 70) {
        screenTimeout = 60;
      }
    } else if (down && page == 9) {
      down = false;
      contrast++;
      if (contrast == +6) {
        contrast = 5;
      }
    }

    if (page == 3 && menuitem == 2) {
      selectedMsg = 0;
    }
    if (page == 3 && menuitem == 3) {
      selectedMsg = 1;
    }
    if (page == 3 && menuitem == 4) {
      selectedMsg = 2;
    }

    if (select_Button.isPressed()) {
      pressedTime = millis();
    }
    if (select_Button.isReleased()) {
      releasedTime = millis();

      long pressDuration = releasedTime - pressedTime;

      if (middle) {
        middle = false;

        if (page == 1 && pressDuration < SHORT_PRESS_TIME) {
          page = 2;
        } else if (page == 2 && pressDuration > LONG_PRESS_TIME) {
          page = 1;
        } else if (page == 2 && menuitem == 1 && pressDuration < SHORT_PRESS_TIME) {
          page = 3;
        } else if (page == 3 && pressDuration > LONG_PRESS_TIME && pressDuration < LONG_PRESS_TIME_TO_HOME) {
          page = 2;
        } else if (page == 3 && pressDuration > LONG_PRESS_TIME_TO_HOME) {
          page = 1;
        } else if (page == 2 && menuitem == 2 && pressDuration < SHORT_PRESS_TIME) {
          page = 4;
        } else if (page == 4 && pressDuration > LONG_PRESS_TIME && pressDuration < LONG_PRESS_TIME_TO_HOME) {
          page = 2;
        } else if (page == 4 && pressDuration > LONG_PRESS_TIME_TO_HOME) {
          page = 1;
        } else if (page == 3 && menuitem == 2 && pressDuration < SHORT_PRESS_TIME) {
          page = 5;
          // selectedMsg = 0;
          Serial.println("Now at - " + String(selectedMsg));

        } else if (page == 3 && menuitem == 3 && pressDuration < SHORT_PRESS_TIME) {
          page = 5;
          // selectedMsg = 1;
          Serial.println("Now at - " + String(selectedMsg));

        } else if (page == 3 && menuitem == 4 && pressDuration < SHORT_PRESS_TIME) {
          page = 5;
          // selectedMsg = 2;
          Serial.println("Now at - " + String(selectedMsg));
        } else if (page == 5 && menuitem == 1 && pressDuration < SHORT_PRESS_TIME) {
          page = 10;
        } else if (page == 5 && menuitem == 2 && pressDuration < SHORT_PRESS_TIME) {
          page = 11;
        } else if (page == 5 && pressDuration > LONG_PRESS_TIME && pressDuration < LONG_PRESS_TIME_TO_HOME) {
          page = 3;
        } else if (page == 5 && pressDuration > LONG_PRESS_TIME_TO_HOME) {
          page = 1;
        } else if (page == 4 && menuitem == 1 && pressDuration < SHORT_PRESS_TIME) {
          page = 6;
        } else if (page == 6 && pressDuration > LONG_PRESS_TIME && pressDuration < LONG_PRESS_TIME_TO_HOME) {
          page = 4;
        } else if (page == 6 && pressDuration > LONG_PRESS_TIME_TO_HOME) {
          page = 1;
        } else if (page == 4 && menuitem == 2 && pressDuration < SHORT_PRESS_TIME) {
          page = 7;
        } else if (page == 7 && pressDuration > LONG_PRESS_TIME && pressDuration < LONG_PRESS_TIME_TO_HOME) {
          page = 4;
        } else if (page == 7 && pressDuration > LONG_PRESS_TIME_TO_HOME) {
          page = 1;
        } else if (page == 7 && menuitem == 1 && pressDuration < SHORT_PRESS_TIME) {
          page = 8;
        } else if (page == 8 && pressDuration > LONG_PRESS_TIME && pressDuration < LONG_PRESS_TIME_TO_HOME) {
          page = 7;
        } else if (page == 8 && pressDuration > LONG_PRESS_TIME_TO_HOME) {
          page = 1;
        } else if (page == 7 && menuitem == 2 && pressDuration < SHORT_PRESS_TIME) {
          page = 9;
        } else if (page == 9 && pressDuration > LONG_PRESS_TIME && pressDuration < LONG_PRESS_TIME_TO_HOME) {
          page = 7;
        } else if (page == 9 && pressDuration > LONG_PRESS_TIME_TO_HOME) {
          page = 1;
        } else if (page == 10 && pressDuration > LONG_PRESS_TIME) {
          page = 5;
          playComplete = true;
        } else if (page == 11 && pressDuration > LONG_PRESS_TIME) {
          page = 5;
        }
      }
    }
  }
}

//get audio from the server
void getVoice(String voice, String token) {
  if (WiFi.status() == WL_CONNECTED) {
    String serverPath = serverName + voiceApi + "voiceId=" + voice + "&token=" + token;
    //String serverPath = "http://52.14.102.194/api/attachment/123/voice?voiceId=abcduser.mp3&token=5a0437f6-dd09-451c-9938-ae9d8e34ab85";
    audio.stopSong();
    serverPath.trim();
    Serial.println(serverPath);
    audio.connecttohost(serverPath.c_str());
    playComplete = false;
  }
}

// // optional
void audio_info(const char *info) {
  Serial.print("info        ");
  Serial.println(info);
}
void audio_id3data(const char *info) {  //id3 metadata
  Serial.print("id3data     ");
  Serial.println(info);
}
void audio_eof_mp3(const char *info) {  //end of file
  Serial.print("eof_mp3     ");
  Serial.println(info);
  playComplete = true;
  page = 5;
  Serial.println("Playing complete");
}

// void audio_eof_stream(const char *info) {
//   Serial.print("eof_mp3     ");
//   Serial.println(info);
//   playComplete = true;
//   page = 5;
//   Serial.println("play complete");
// }
void audio_showstation(const char *info) {
  Serial.print("station     ");
  Serial.println(info);
}
void audio_showstreamtitle(const char *info) {
  Serial.print("streamtitle ");
  Serial.println(info);
}
void audio_bitrate(const char *info) {
  Serial.print("bitrate     ");
  Serial.println(info);
}
void audio_commercial(const char *info) {  //duration in sec
  Serial.print("commercial  ");
  Serial.println(info);
}
void audio_icyurl(const char *info) {  //homepage
  Serial.print("icyurl      ");
  Serial.println(info);
}
void audio_lasthost(const char *info) {  //stream URL played
  Serial.print("lasthost    ");
  Serial.println(info);
}



void getEmoji(String emojiName, String token) {
  //String serverPath = "http://52.14.102.194/api/attachment/123/emoji?emoName=smile&token=5a0437f6-dd09-451c-9938-ae9d8e34ab85";
  String serverPath = serverName + emojiApi + "emoName=" + emojiName + "&token=" + token;
  Serial.println("Server path - " + serverPath);
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    // Your Domain name with URL path or IP address with path
    http.begin(wifi_2, serverPath.c_str());

    // Send HTTP GET request
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      String payload = http.getString();


      DynamicJsonDocument doc(28000);
      //Serial.println(ESP.getMaxAllocHeap());
      // DynamicJsonDocument doc(ESP.getMaxAllocHeap());
      DeserializationError error = deserializeJson(doc, payload);

      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
      }

      String emo = doc["data"]["emoName"];
      // JsonArray intArr = doc["data"]["value"];
      uint8_t intArr[1024];
      copyArray(doc["data"]["value"], intArr);
      Serial.println("Playing id - " + String(selectedMsg));

      display.clearDisplay();
      display.drawBitmap(0, 0, intArr, 128, 64, 1);
      display.display();
      playComplete = false;
    } else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
      delay(3000);
    }
    // Free resources
    http.end();
  } else {
    Serial.println("WiFi Disconnected");
    // delay(2000);
  }
}

void checkIfDownButtonIsPressed() {
  if (downButtonState != lastDownButtonState) {
    if (downButtonState == 0) {
      if (notificationStatus == true) {
        notificationStatus = false;
        //set page & message
      } else {
        down = true;
      }
    }
    delay(50);
  }
  lastDownButtonState = downButtonState;
}

void checkIfUpButtonIsPressed() {
  if (upButtonState != lastUpButtonState) {
    if (upButtonState == 0) {
      if (notificationStatus == true) {
        notificationStatus = false;
        //set page & message
      } else {
        up = true;
      }
    }
    delay(50);
  }
  lastUpButtonState = upButtonState;
}

void checkIfSelectButtonIsPressed() {

  if (selectButtonState != lastSelectButtonState) {
    if (selectButtonState == 0) {
      if (notificationStatus == true) {
        notificationStatus = false;
      } else {
        middle = true;
      }
    }
    delay(50);
  }
  lastSelectButtonState = selectButtonState;
}



void drawMenu() {

  //Starting interface
  if (page == 1) {
    display.clearDisplay();
    display.drawBitmap(0, 0, digiemoLogo, 128, 64, 1);
    display.display();


    //main menu interface
  } else if (page == 2) {

    //messages
    if (menuitem == 1) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(BLACK, WHITE);
      display.setCursor(40, 0);
      display.print("MESSAGES");
      display.drawBitmap(0, 16, message_icon, 128, 64, 1);
      display.display();
    }

    //Settings
    if (menuitem == 2) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(BLACK, WHITE);
      display.setCursor(40, 0);
      display.print("SETTINGS");
      display.drawBitmap(0, 16, setting_icon, 128, 64, 1);
      display.display();
    }

    //message queue
  } else if (page == 3) {

    //No messages
    if (messageList.Count() == 0 && menuitem == 1) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(BLACK, WHITE);
      display.setCursor(40, 0);
      display.print("No Messages");
      display.drawBitmap(0, 16, message_icon, 128, 64, 1);
      display.display();
      //message 1
    } else if (messageList.Count() >= 1 && menuitem == 2) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(BLACK, WHITE);
      display.setCursor(40, 0);
      display.print("MESSAGE 1");
      display.drawBitmap(0, 16, message_icon, 128, 64, 1);
      display.display();
      //message 2
    } else if (messageList.Count() >= 2 && menuitem == 3) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(BLACK, WHITE);
      display.setCursor(40, 0);
      display.print("MESSAGE 2");
      display.drawBitmap(0, 16, message_icon, 128, 64, 1);
      display.display();
    }
    //message 3
    else if (messageList.Count() >= 3 && menuitem == 4) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(BLACK, WHITE);
      display.setCursor(40, 0);
      display.print("MESSAGE 3");
      display.drawBitmap(0, 16, message_icon, 128, 64, 1);
      display.display();
    }
    //setting menu
  } else if (page == 4) {
    //sound settings
    if (menuitem == 1) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(BLACK, WHITE);
      display.setCursor(25, 0);
      display.print("SOUNDS SETTING");
      display.drawBitmap(0, 16, sound_icon, 128, 64, 1);
      display.display();
    }
    //display settings
    if (menuitem == 2) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(BLACK, WHITE);
      display.setCursor(25, 0);
      display.print("DISPLAY SETTING");
      display.drawBitmap(0, 16, display_setting_icon, 128, 64, 1);
      display.display();
    }
    //message open interface
  } else if (page == 5) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(BLACK, WHITE);
    display.setCursor(15, 0);
    display.print("DO YOU WANT PLAY?");
    display.setCursor(0, 25);
    //Play option
    if (menuitem == 1) {
      display.setTextColor(BLACK, WHITE);
    } else {
      display.setTextColor(WHITE, BLACK);
    }
    display.println(">PLAY");

    //delet optition
    if (menuitem == 2) {
      display.setTextColor(BLACK, WHITE);
    } else {
      display.setTextColor(WHITE, BLACK);
    }
    display.println(">DELETE");
    display.display();
    //vloume setting
  } else if (page == 6) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(BLACK, WHITE);
    display.setCursor(15, 0);
    display.print("SOUNDS");
    display.drawFastHLine(0, 10, 83, BLACK);
    display.setCursor(5, 20);
    display.print("VOLUME");
    display.setTextSize(3);
    display.setCursor(10, 30);
    display.print(sound);
    display.display();
    //display setting
  } else if (page == 7) {
    if (menuitem == 1) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(BLACK, WHITE);
      display.setCursor(15, 0);
      display.print("SCREEN OFF TIMEOUT");
      display.drawBitmap(0, 16, screen_timeout_icon, 128, 64, 1);
      display.display();
    }
    if (menuitem == 2) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(BLACK, WHITE);
      display.setCursor(25, 0);
      display.print("CONTRAST");
      display.drawBitmap(0, 16, contrast_icon, 128, 64, 1);
      display.display();
    }
    //screen timeout setting
  } else if (page == 8) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(BLACK, WHITE);
    display.setCursor(15, 0);
    display.print("SCREEN OFF TIMEOUT");
    display.drawFastHLine(0, 10, 83, BLACK);
    display.setCursor(5, 20);
    display.print("SECONDS");
    display.setTextSize(3);
    display.setCursor(10, 30);
    display.print(screenTimeout);
    display.display();
    //Contrast setting
  } else if (page == 9) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(BLACK, WHITE);
    display.setCursor(15, 0);
    display.print("CONTRAST");
    display.drawFastHLine(0, 10, 83, BLACK);
    display.setCursor(5, 20);
    display.print("LEVEL");
    display.setTextSize(3);
    display.setCursor(10, 30);
    display.print(contrast);
    display.display();

    //play option
  } else if (page == 10) {
    if (playComplete == true) {
      playMessages(messageList[selectedMsg]);
    }

    // display.clearDisplay();
    // display.setTextSize(1);
    // display.setTextColor(WHITE, BLACK);
    // display.setCursor(0, 20);;
    // display.print("Opening Message.....");
    // display.display();
    // display.clearDisplay();



    //delet option
  } else if (page == 11) {
    Serial.println("Now deleting - " + String(selectedMsg) + " from count - " + String(messageList.Count()));
    if (messageList.Count() > 0) {
      messageList.Remove(selectedMsg);
    }
    //     if (selectedMsg >= 1) {
    //   selectedMsg = selectedMsg - 1;
    // }
    Serial.println("Now available - " + String(messageList.Count()));
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE, BLACK);
    display.setCursor(0, 20);
    display.print("Message Deleted.");
    display.display();
    delay(1500);
    if (messageList.Count() > 0) {
      page = 3;
    } else {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE, BLACK);
      display.setCursor(0, 20);
      display.print("No Message.");
      display.display();
      page = 1;
    }
  }
}