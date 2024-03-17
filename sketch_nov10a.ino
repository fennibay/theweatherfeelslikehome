#include <stdint.h>
#include <string.h>

#include "ArduinoGraphics.h"
#include "Arduino_LED_Matrix.h"
#include "RTC.h"

#define DELAY_UNIT  250   // ms

// the hours are [start;stop), i.e. stop exclusive
int DAYTIME_START = 9;
int NIGHTTIME_START = 16;
int STOP = 22;

ArduinoLEDMatrix matrix;

const char *morseTable[] = {
    ".-",   // A
    "-...", // B
    "-.-.", // C
    "-..",  // D
    ".",    // E
    "..-.", // F
    "--.",  // G
    "....", // H
    "..",   // I
    ".---", // J
    "-.-",  // K
    ".-..", // L
    "--",   // M
    "-.",   // N
    "---",  // O
    ".--.", // P
    "--.-", // Q
    ".-.",  // R
    "...",  // S
    "-",    // T
    "..-",  // U
    "...-", // V
    ".--",  // W
    "-..-", // X
    "-.--", // Y
    "--.."  // Z
};

const char *messages[] = {
  "The weather feels like home",
  "The weather doesnt feel like home",
  ""
};
int messageIdx = 0;

char const * const convertCharToMorse(char const c) {
  return (c == ' ' ? " " : morseTable[toupper(c) - 'A']);
}

void printToMtx(char const * const c, int len, bool scroll = false) {
  char mtxText[100] = {0};

  strncpy(mtxText, c, len);
  mtxText[len] = '\0';

  matrix.beginDraw();
  matrix.clear();
  matrix.stroke(0xFFFFFFFF);
  matrix.textScrollSpeed(150);

  matrix.textFont(Font_4x6);
  matrix.beginText(0, 1, 0xFFFFFF);
  matrix.println(mtxText);
  matrix.endText(scroll ? SCROLL_LEFT : NO_SCROLL);

  matrix.endDraw();

  if (scroll) {
    matrix.play();
    while (!matrix.sequenceDone()) {
      matrix.play();
    }
    delay(500);
    clearMtx();
  }
}

void clearMtx() {
  matrix.beginDraw();
  matrix.clear();
  matrix.endDraw();
  delay(DELAY_UNIT);
}

void fadeInOut() {
  for (int i = 0; i <= 200; ++i) {
    outputLED(i);
    delay(25);
  }
  delay(1000);
  for (int i = 200; i >= 0; --i) {
    outputLED(i);
    delay(25);
  }
  delay(1000);
}

void outputLED(unsigned value) {
  analogWrite(13, value); // onboard LED
  analogWrite(6, value);  // PWM
} 

void outputMessageInMorse(char const * msg, int pin) {
  for (;*msg != '\0'; ++msg) {
    printToMtx(msg, 1);
    outputMorse(convertCharToMorse(*msg), pin);
    clearMtx();
  }
}

void outputMorse(char const * const morseString, int pin) {
    int const dim_level = getDimLevel();
    int i;
    for (i = 0; morseString[i] != '\0'; i++) {
        if (morseString[i] == '.') {
            outputLED(dim_level);
            delay(DELAY_UNIT * 1);
            outputLED(0);
            delay(DELAY_UNIT * 1);
        } else if (morseString[i] == '-') {
            outputLED(dim_level);
            delay(DELAY_UNIT * 3);
            outputLED(0);
            delay(DELAY_UNIT * 1);
        } else if (morseString[i] == ' ') {
            delay(DELAY_UNIT * 7);
        }
    }
}

void printCurTime() {
  RTCTime currentTime;
  RTC.getTime(currentTime);
  Serial.println(currentTime.toString());
}

void printCurTimeToMtx() {
  RTCTime currentTime;
  RTC.getTime(currentTime);
  int const hour = currentTime.getHour();
  int const min = currentTime.getMinutes();
  char text[5] = {0};
  if (hour < 10) {
    text[0] = '0';
    itoa(hour, text + 1, 10);
  } else {
    itoa(hour, text + 0, 10);
  }
  if (min < 10) {
    text[2] = '0';
    itoa(min, text + 3, 10);
  } else {
    itoa(min, text + 2, 10);
  }
  text[4] = '\0';
  printToMtx(text, 5, true);
}

void checkSerial() {
  // Messageformat: settime 20231118143400 (~22 characters)
  if (Serial.available()) {
    char buf[100];
    size_t size = Serial.readBytes(buf, 100);
    if (size >= 22 && strncmp(buf, "settime ", 8) == 0) {
      struct tm newTime;
      newTime.tm_sec = atoi(buf + 20);
      buf[20] = '\0';
      newTime.tm_min = atoi(buf + 18);
      buf[18] = '\0';
      newTime.tm_hour = atoi(buf + 16);
      buf[16] = '\0';
      newTime.tm_mday = atoi(buf + 14);
      buf[14] = '\0';
      newTime.tm_mon = atoi(buf + 12) - 1;
      buf[12] = '\0';
      newTime.tm_year = atoi(buf + 8);
      RTCTime newTimeRtc;
      newTimeRtc.setTM(newTime);
      RTC.setTime(newTimeRtc);
      Serial.println("Time set");
      printCurTime();
    } else if (size >= 7 && strncmp(buf, "gettime", 7) == 0) {
      printCurTime();
    } else if (size >= 16 && strncmp(buf, "acthours ", 8) == 0) {
      // syntax: acthours 09 17 21
      int stop = atoi(buf + 15);
      buf[14] = '\0';
      int night_start = atoi(buf + 12);
      buf[11] = '\0';
      int day_start = atoi(buf + 9);
      DAYTIME_START = day_start;
      NIGHTTIME_START = night_start;
      STOP = stop;
      Serial.print("Active hours ");
      Serial.print(DAYTIME_START, 10);
      Serial.print(" ");
      Serial.print(NIGHTTIME_START, 10);
      Serial.print(" ");
      Serial.print(STOP, 10);
      Serial.println("");
    } else {
      Serial.println("Unknown command");
    }
  }
}

void rtcSetup() {
  RTC.begin();

  RTCTime savedTime;
  RTC.getTime(savedTime);

  if (!RTC.isRunning()) {
    // this means the RTC is waking up "as new"
    RTC.setTime(savedTime);
  }

  printCurTime();
  printCurTimeToMtx();
}

bool inActiveTime() {
  RTCTime curtime;
  RTC.getTime(curtime);
  int curhour = curtime.getHour();
  return curhour >= DAYTIME_START && curhour < STOP;
}

int getDimLevel() {
  RTCTime curtime;
  RTC.getTime(curtime);
  int curhour = curtime.getHour();
  if (curhour >= DAYTIME_START && curhour < NIGHTTIME_START) {
    return 200;
  } else if (curhour >= NIGHTTIME_START && curhour < STOP) {
    return 30;
  } else {
    return 0;
  }
}

void setup() {
  Serial.begin(9600);

  Serial.println("Started");

  matrix.begin();

  rtcSetup();
  
  Serial.println("Starting fadeInOut");
  fadeInOut();
  Serial.println("Completed fadeInOut");
}

void loop() {
    if (inActiveTime()) {
      const char *message = messages[messageIdx];

      if (message[0] == '\0') {
        messageIdx = 0;
        return;
      } else {
        ++messageIdx;
      }
      outputMessageInMorse(message, 13);
    } else {
      outputLED(30);
      delay(DELAY_UNIT * 20);
    }

    checkSerial();
    printCurTimeToMtx();

    return;
}

