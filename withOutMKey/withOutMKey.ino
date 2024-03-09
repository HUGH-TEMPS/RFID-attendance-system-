#include <WiFi.h>
#include <HTTPClient.h>
#include <rdm6300.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <TFT_eFEX.h>
#include <PNGdec.h>
#include "aces240.h"  // Image is stored here in an 8-bit array
#include "cicte240.h"
#include "wlc240.h"
//#include "lordish.h" //font
#define MAX_IMAGE_WIDTH 240  // Adjust for your images
#define Buzzer 4
// configurables
const char* URL1 = "http://192.168.100.49/WLC/dist/_getUIDPhotoURL.php";
const char* URL2 = "http://192.168.100.49/WLC/dist/_getUID.php";
const char* SSID = "Tem369";
const char* PASSWORD = "Aquabend";

const int PIN_RFID = 16;
const int PIN_READ_LED = 33;
const int TFT_ORIENTATION = 135;
const int TFT_TEXTSIZE = 1;

const int LED_FREQ = 5000;
const int LED_CHANNEL = 15;
const int LED_RESOLUTION = 8;
const int LED_BRIGHTNESS = 20;

// Slideshow variables
int16_t xpos = 0;
int16_t ypos = 0;
int currentLogo = 1;
unsigned long previousTime = 0;
const unsigned long logoInterval = 2000;  // Interval in milliseconds for logo slideshow
// end Slideshow variables


TFT_eSPI tft = TFT_eSPI();
TFT_eFEX fex = TFT_eFEX(&tft);
Rdm6300 rdm6300;
PNG png;

void printSplitString(String text) {
  int wordStart = 0;
  int wordEnd = 0;
  while ((text.indexOf(' ', wordStart) >= 0) && (wordStart <= text.length())) {
    wordEnd = text.indexOf(' ', wordStart + 1);
    uint16_t len = tft.textWidth(text.substring(wordStart, wordEnd));
    if (tft.getCursorX() + len >= tft.width()) {
      tft.println();
      if (wordStart > 0) wordStart++;
    }
    tft.print(text.substring(wordStart, wordEnd));
    wordStart = wordEnd;
  }
}

void connectWiFiTFT() {


  // digitalWrite(PIN_READ_LED, LOW);
  ledcWrite(LED_CHANNEL, LED_BRIGHTNESS);     
  tft.fillScreen(TFT_WHITE);
  tft.print("Connecting to: ");
  printSplitString(SSID);

  WiFi.begin(SSID, PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    tft.print(".");
  }
  playNote(523, 200);
  delay(50);
  playNote(659, 200);
  delay(50);
  playNote(784, 200);
  delay(50);
  playNote(988, 300);
  delay(50);

  tft.println();
  tft.println();
  tft.println("WiFi connected.");
  tft.println();
  tft.println("IP address: ");
  tft.println(WiFi.localIP());
  tft.println();

  delay(5000);

  // digitalWrite(PIN_READ_LED, HIGH);
  ledcWrite(LED_CHANNEL, 0);

  tft.fillScreen(TFT_WHITE);
  tft.setCursor(0, 0);
}

void setup() {
  Serial.begin(115200);

  ledcSetup(LED_CHANNEL, LED_FREQ, LED_RESOLUTION);
  ledcAttachPin(PIN_READ_LED, LED_CHANNEL);
  pinMode(Buzzer, OUTPUT);
  rdm6300.begin(PIN_RFID);

  tft.begin();
  tft.setRotation(TFT_ORIENTATION);
  tft.fillScreen(TFT_WHITE);
  tft.setTextColor(TFT_BLACK);
  tft.setTextSize(TFT_TEXTSIZE);
  tft.setTextWrap(true);

  if (!LittleFS.begin()) {
    Serial.println(("LittleFS initialization failed!"));
    LittleFS.begin(true);
  }

  fex.listSPIFFS();
}

void loop() {
  // local variables
  HTTPClient http;
  uint32_t currentUserID;
  int httpResponseCode;
  int rotate = 0;
  int imgX = 0;
  int imgY = 0;
  String postData;
  String imgURL;
  // end local variables

  if (WiFi.status() != WL_CONNECTED) {
    connectWiFiTFT();
  }

  if (rdm6300.get_new_tag_id()) {
    currentUserID = rdm6300.get_tag_id();
    //tft.fillScreen(TFT_PINK);
  } else {
    slideshow();
    delay(50);
    return;
  }

  // digitalWrite(PIN_READ_LED, LOW);
  ledcWrite(LED_CHANNEL, LED_BRIGHTNESS);

  // start debug
  Serial.println(currentUserID);
  //tone buzzer
  digitalWrite(Buzzer, HIGH);
  delay(100);
  digitalWrite(Buzzer, LOW);
  // end debug

  if (TFT_ORIENTATION == 1 || TFT_ORIENTATION == 3)
    rotate = 1;

  postData = "uidResult=" + String(currentUserID) + "&orientation=" + String(rotate);

  http.begin(URL1);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  httpResponseCode = http.POST(postData);

  // start debug
  Serial.println(httpResponseCode);
  // end debug

  if (httpResponseCode != HTTP_CODE_OK) {
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0);
    tft.println("ERROR: \nCannot access URL.");
    // digitalWrite(PIN_READ_LED, HIGH);
    ledcWrite(LED_CHANNEL, 0);
    delay(2000);
    return;
  }

  imgURL = http.getString();

  http.end();

  if (imgURL.substring(0, 4) == "http") {
    http.begin(imgURL);
    // start debug
    Serial.println(imgURL);
    // end debug
    httpResponseCode = http.GET();
    // start debug
    Serial.println(httpResponseCode);
    // end debug
    if (httpResponseCode == HTTP_CODE_OK) {
      // if (httpResponseCode == HTTP_CODE_OK) {}
      fs::File file = LittleFS.open("/img.jpg", FILE_WRITE);
      int len = http.getSize();
      uint8_t buff[128] = { 0 };
      WiFiClient* stream = http.getStreamPtr();
      while (http.connected() && (len > 0 || len == -1)) {
        size_t size = stream->available();
        if (size) {
          int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
          file.write(buff, c);
          if (len > 0) {
            len -= c;
          }
        }
      }
      http.end();
      file.close();
      tft.fillScreen(TFT_WHITE);
      JpegDec.decodeFsFile("/img.jpg");
      imgX = JpegDec.width;
      imgY = JpegDec.height;
      bool isDrawn = fex.drawJpgFile(SPIFFS, "/img.jpg", (tft.width() - imgX) / 2 - 1, 0);
      JpegDec.abort();
      // start debug
      if (isDrawn) {
        Serial.println("Image drawn.");
      } else {
        Serial.println("Image failed to draw.");
      }
      // end debug
      LittleFS.remove("/img.jpg");
    }
  } else {
    tft.fillScreen(TFT_WHITE);
    tft.setCursor(0, 0);
    imgURL.replace("<br>", "\n");
    printSplitString(imgURL);
  }

  postData = "uidResult=" + String(currentUserID);

  http.begin(URL2);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  httpResponseCode = http.POST(postData);
  // start debug
  Serial.println(httpResponseCode);
  // end debug

  tft.setCursor(20, imgY);
  String result = http.getString();
  //result.replace("<br>", "\n");
  tft.setTextColor(tft.color565(139, 38, 53));
  //tft.loadFont(lordish);
  tft.println(result);
  
  http.end();
  tft.unloadFont();
  delay(2000);

  // digitalWrite(PIN_READ_LED, HIGH);
  ledcWrite(LED_CHANNEL, 0);
}

void playNote(int noteFrequency, int noteDuration) {
  tone(Buzzer, noteFrequency, noteDuration);
  delay(noteDuration);
  noTone(Buzzer);
}

void pngDraw(PNGDRAW* pDraw) {
  uint16_t lineBuffer[MAX_IMAGE_WIDTH];
  png.getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_BIG_ENDIAN, 0xffffffff);

  // Calculate the starting X position to center the image horizontally
  int16_t xpos = (tft.width() - pDraw->iWidth) / 2;

  tft.pushImage(xpos, ypos + pDraw->y, pDraw->iWidth, 1, lineBuffer);
}
void slideshow()  {
  unsigned long currentTime = millis();
  if (currentTime - previousTime >= logoInterval) {
    previousTime = currentTime;
    switch (currentLogo) {
      case 1:{
        int16_t wlc = png.openFLASH((uint8_t *)wlc240, sizeof(wlc240), pngDraw);
        if (wlc == PNG_SUCCESS) {
          tft.startWrite();
          wlc = png.decode(NULL, 0);
        }
        currentLogo = 2; 
        break;
      }
      case 2:{
        int16_t cicte = png.openFLASH((uint8_t *)cicte240, sizeof(cicte240), pngDraw);
        if (cicte == PNG_SUCCESS) {
          tft.startWrite();
          cicte = png.decode(NULL, 0);
        }
        currentLogo = 3; 
        break;
      }
      case 3:{
        int16_t aces = png.openFLASH((uint8_t *)aces240, sizeof(aces240), pngDraw);
        if (aces == PNG_SUCCESS) {
          tft.startWrite();
          aces = png.decode(NULL, 0);
        }
        currentLogo = 1;
        break;
      }
    }
    tft.endWrite();
  }
}

