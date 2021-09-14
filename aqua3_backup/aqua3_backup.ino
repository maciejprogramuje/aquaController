#include <LCD5110_Graph.h>
#include <ClickEncoder.h>
#include <TimerOne.h>
#include <virtuabotixRTC.h>
#include <avr/eeprom.h>
#include <OneWire.h>
#include <DallasTemperature.h>

LCD5110 disp(8, 9, 10, 12, 11);
extern uint8_t SmallFont[];
extern unsigned char TinyFont[];
extern uint8_t BigNumbers[];

ClickEncoder encoder(4, 5, 7, 4);
int16_t oldEncPos, encPos;
uint8_t buttonState;

virtuabotixRTC rtClock(A0, 2, 3);

OneWire oneWire(A5); // termometr
DallasTemperature tempSensor(&oneWire);

int lightDelay = 100;
int tLightDelay = 0;
int lightDelayImpulse = 20;
int tLightDelayImpulse = 0;
int lightDelayImpulseSecond = 0;

int menu = 0;
String menuOne[] = {"[..]", "Blue sunrise", "Mix sunrise", "Sunrise", "Mix sunset", "Blue sunset", "Sunset", "Set clock", "Temp. alarm"};
int menuOnePos = 0;

int hh = 0;
int mm = 0;

float temp = 0;
char temperature[6];
float tempAlarm = 0.0;
char temperatureAlarm[6];

//adresacja pamieci eeprom (nieulotnej): nazwa/początkowy bajt/ilosc zajmowanych bajtow
//blueSunriseHh 0,2    mixSunriseHh 4,2    sunriseHh 8,2    mixSunsetHh 12,2   blueSunsetHh 16,2    sunsetHh 20,2
//blueSunriseMm 2,2    mixSunriseMn 6,2    sunriseMm 10,2   mixSunsetMm 14,2   blueSunsetMm 18,2    sunsetMm 22,2
//tempAlarm 24,4

int lightMode = 0;
int relay1 = 13;
int relayHh = 0;
int relayMm = 0;
boolean relayFactor;

void setup() {
  //Serial.begin(9600);

  Timer1.initialize(1000);
  Timer1.attachInterrupt(timerIsr);
  encoder.setAccelerationEnabled(true);
  oldEncPos = -1;

  disp.InitLCD();
  disp.setFont(SmallFont);
  analogWrite(6, 255); // podświetlenie
  disp.clrScr();

  eeprom_read_block(&tempAlarm, 24, 4);
  if (isnan(tempAlarm)) {
    tempAlarm = 30.0;
  }

  tempSensor.begin();
  tempSensor.setWaitForConversion(false);

  //                      seconds, minutes, hours, day of the week, day of the month, month, year
  //rtClock.setDS1302Time(      0,      50,     8,               6,               28,     8, 2021);

  pinMode(relay1, OUTPUT);
  digitalWrite(relay1, HIGH);
  relayFactor = true;

  tLightDelayImpulse = lightDelayImpulse;
}

void loop() {
  rtClock.updateTime();

  setTemperature();

  lightOnOff();

  if (tLightDelayImpulse < lightDelayImpulse && tLightDelayImpulse > 0) {
    tLightDelayImpulse--;
    if (lightDelayImpulseSecond > 1) {
      tLightDelay = lightDelay;
    }
  } else {
    tLightDelayImpulse = lightDelayImpulse;
    lightDelayImpulseSecond = 0;
  }
  //Serial.println("tLightDelayImpulse=" + String(tLightDelay) + ", tLightDelayImpulse=" + String(tLightDelayImpulse) + ", lightDelayImpulseSecond=" + String(lightDelayImpulseSecond));


  if (menu == 0) {
    showTempAndClock();
  } else if (menu == 1) {
    showMenuOne(menuOnePos);
  } else if (menu >= 11 && menu <= 17) {
    showTimeSetting();
  } else if (menu == 18) {
    showTempAlarmSetting();
  } else if (menu == 19) {
    showInvalidTimeAlert();
  }

  encPos += encoder.getValue();
  if (encPos != oldEncPos) {
    setLightDelayImpulse();

    if (menu == 1) {
      if (oldEncPos > encPos) {
        menuOnePos--;
        if (menuOnePos < 0) menuOnePos = 0;
      } else {
        menuOnePos++;
        if (menuOnePos > 8) menuOnePos = 8;
      }
      showMenuOne(menuOnePos);
    } else if (menu >= 11 && menu <= 17) {
      setHhMmByEncPos();
    } else if (menu == 18) {
      setTempAlarmByEncPos();
    }

    oldEncPos = encPos;
  }

  buttonState = encoder.getButton();
  if (buttonState != 0) {
    setLightDelayImpulse();

    if (menu == 0) {
      menu = 1;
      menuOnePos = 0;
    } else if (menu == 1) {
      if (menuOnePos == 0) {
        menu = 0;
      } else if (menuOnePos >= 1 && menuOnePos <= 6) {
        setMenuAndSetHhMmFromEeprom((10 + menuOnePos), ((menuOnePos - 1) * 4), (menuOnePos * 4 - 2));
      } else if (menuOnePos == 7) {
        menu = 17;
        hh = int(rtClock.hours);
        mm = int(rtClock.minutes);
      } else if (menuOnePos == 8) {
        menu = 18;
      }
    } else if (menu >= 11 && menu <= 16) {
      backToMenuOneWithPosAndSetEepromFromHhMm((menu - 10), ((menuOnePos - 1) * 4), (menuOnePos * 4 - 2));
    } else if (menu == 17) {
      rtClock.setDS1302Time(0, mm, hh, int(rtClock.dayofweek), int(rtClock.dayofmonth), int(rtClock.month), int(rtClock.year));
      backToMenuOne(7);
    } else if (menu == 18) {
      eeprom_write_block(&tempAlarm, 24, 4);
      backToMenuOne(8);
    } else if (menu == 19) {
      backToMenuOne(menuOnePos);
    }
  }

  manageRelay();
}

void lightOnOff() {
  if (tLightDelay > 0 || temp >= tempAlarm) {
    analogWrite(6, 50);
  } else {
    analogWrite(6, 255);
    tLightDelay = 0;
  }
  tLightDelay--;
}

void setLightDelayImpulse() {
  tLightDelayImpulse--;
  if (tLightDelayImpulse < lightDelayImpulse) {
    lightDelayImpulseSecond++;
  }
}

void setRelayStateAndFactor(boolean value) {
  digitalWrite(relay1, value);
  relayFactor = false;
}

void setRelayHhMmFromEeprom(int tHh, int tMm) {
  eeprom_read_block(&relayHh, tHh, 2);
  eeprom_read_block(&relayMm, tMm, 2);
}

void timerIsr() {
  encoder.service();
}

void showMenuOne(int invLine) {
  disp.clrScr();
  disp.setFont(SmallFont);
  for (int i = 0; i < 9; i++) {
    if (invLine == i) disp.invertText(true);
    if (invLine > 5) {
      disp.print(menuOne[i], LEFT, (i - 4) * 9);
    } else if (invLine > 3) {
      disp.print(menuOne[i], LEFT, (i - 2) * 9);
    } else {
      disp.print(menuOne[i], LEFT, i * 9);
    }
    disp.invertText(false);
  }
  disp.update();
}

void showTempAndClock() {
  disp.clrScr();
  disp.setFont(TinyFont);
  disp.print(leadZero(String(rtClock.dayofmonth)) + "/" + leadZero(String(rtClock.month)) + "/" + leadZero(String(rtClock.year)), 9, 2);
  disp.drawLine(0, 8, 83, 8);
  disp.drawLine(0, 0, 83, 0);
  hh = int(rtClock.hours);
  mm = int(rtClock.minutes);
  disp.print(leadZero(String(hh)) + ":" + leadZero(String(mm)) + ":" + leadZero(String(rtClock.seconds)), RIGHT, 2);
  disp.setFont(BigNumbers);
  disp.print(temperature, 9, 15);
  disp.setFont(SmallFont);
  disp.print("o", 66, 14);
  disp.print("C", 73, 16);
  disp.drawLine(0, 47, 84, 47);
  drawLightModeRect();
  disp.update();
}

void showTimeSetting() {
  disp.clrScr();
  disp.setFont(BigNumbers);
  disp.print(leadZero(String(hh)), 7, 20);
  disp.print(".", CENTER, 14);
  disp.print(".", CENTER, 8);
  disp.print(leadZero(String(mm)), 49, 20);
  disp.setFont(TinyFont);
  disp.invertText(true);
  disp.drawLine(0, 0, 84, 0);
  disp.print(" left  - set hours    ", 0, 1);
  disp.drawLine(0, 8, 84, 8);
  disp.print(" right - set minutes  ", 0, 9);
  disp.invertText(false);
  disp.setFont(SmallFont);
  disp.update();
}

void showTempAlarmSetting() {
  dtostrf(tempAlarm, 3, 1, temperatureAlarm);

  disp.clrScr();
  disp.setFont(BigNumbers);
  disp.print(temperatureAlarm, 9, 20);
  disp.setFont(SmallFont);
  disp.print("o", 66, 19);
  disp.print("C", 73, 21);
  disp.setFont(TinyFont);
  disp.invertText(true);
  disp.drawLine(0, 0, 84, 0);
  disp.print(" left  - lower temp.  ", 0, 1);
  disp.drawLine(0, 8, 84, 8);
  disp.print(" right - higher temp. ", 0, 9);
  disp.invertText(false);
  disp.setFont(SmallFont);
  disp.update();
}

void showInvalidTimeAlert() {
  disp.clrScr();
  disp.setFont(TinyFont);
  disp.print("The time must be", CENTER, 4);
  disp.print("greater than", CENTER, 12);
  disp.print("the previous step", CENTER, 20);
  disp.print("time and less than", CENTER, 28);
  disp.print("the next step time!", CENTER, 36);
  disp.drawRect(0, 0, 83, 47);
  disp.update();
}

void setTemperature() {
  if (tempSensor.isConversionComplete()) {
    tempSensor.requestTemperatures();
    temp = tempSensor.getTempCByIndex(0);
    dtostrf(temp, 3, 1, temperature);
  }
}

void setTempAlarmByEncPos() {
  if (oldEncPos > encPos) {
    tempAlarm = tempAlarm - 0.1;
    if (tempAlarm <= 0.0) tempAlarm = 0.0;
  } else {
    tempAlarm = tempAlarm + 0.1;
    if (tempAlarm >= 99.9) tempAlarm = 99.9;
  }
}

void setHhMmByEncPos() {
  if (oldEncPos > encPos) {
    hh++;
    if (hh > 23) hh = 0;
  } else {
    mm++;
    if (mm > 59) mm = 0;
  }
}

void backToMenuOne(int backToMenuOnePos) {
  menu = 1;
  menuOnePos = backToMenuOnePos;
  disp.invert(false);
}

void setMenuAndSetHhMmFromEeprom(int menuNum, int hhEepromFirstByte, int mmEepromFirstByte) {
  menu = menuNum;
  eeprom_read_block(&hh, hhEepromFirstByte, 2);
  eeprom_read_block(&mm, mmEepromFirstByte, 2);
}

void backToMenuOneWithPosAndSetEepromFromHhMm(int menuOnePosNum, int hhEepromFirstByte, int mmEepromFirstByte) {
  int prevHh = 0;
  int prevMm = -1;
  int nextHh = 23;
  int nextMm = 60;
  if (hhEepromFirstByte >= 4) {
    eeprom_read_block(&prevHh, hhEepromFirstByte - 4, 2);
    eeprom_read_block(&prevMm, mmEepromFirstByte - 4, 2);
  }
  if (hhEepromFirstByte <= 16) {
    eeprom_read_block(&nextHh, hhEepromFirstByte + 4, 2);
    eeprom_read_block(&nextMm, mmEepromFirstByte + 4, 2);
  }

  int prevTime = prevHh * 60 + prevMm;
  int nextTime = nextHh * 60 + nextMm;
  int nowTime = hh * 60 + mm;

  if (nowTime > prevTime && nowTime < nextTime) {
    eeprom_write_block(&hh, hhEepromFirstByte, 2);
    eeprom_write_block(&mm, mmEepromFirstByte, 2);
    backToMenuOne(menuOnePosNum);
  } else {
    menu = 19;
  }
}

void drawLightModeRect() {
  if (lightMode == 1) {           //white
    disp.drawRect(0, 0, 6, 8);
  } else if (lightMode == 2) {    //mix
    disp.drawRect(0, 0, 6, 8);
    disp.drawLine(0, 2, 6, 2);
  } else if (lightMode == 3) {    //blue
    disp.drawRect(0, 0, 6, 8);
    disp.drawLine(2, 2, 3, 2);
    disp.drawLine(4, 2, 5, 2);
    disp.drawLine(2, 4, 3, 4);
    disp.drawLine(4, 4, 5, 4);
    disp.drawLine(2, 6, 3, 6);
    disp.drawLine(4, 6, 5, 6);
  } else if (lightMode == 4) {    //black
    disp.drawRect(0, 0, 5, 8);
    disp.drawRect(1, 1, 4, 7);
    disp.drawRect(2, 2, 3, 6);
  }
}

String leadZero(String s) {
  if (s.length() < 2) {
    return "0" + s;
  }
  return s;
}

void manageRelay() {
  int sec = rtClock.seconds;
  setRelayHhMmFromEeprom(0, 2); //blueSunrise
  if (rtClock.hours == relayHh && rtClock.minutes == relayMm) {
    if (sec == 0 && relayFactor) {
      setRelayStateAndFactor(HIGH);
    } else if (sec == 2 && relayFactor) {
      setRelayStateAndFactor(LOW); //white
    } else if (sec == 4 && relayFactor) {
      setRelayStateAndFactor(HIGH);
    } else if (sec == 6 && relayFactor) {
      setRelayStateAndFactor(LOW); //mix
    } else if (sec == 8 && relayFactor) {
      setRelayStateAndFactor(HIGH);
    } else if (sec == 10 && relayFactor) {
      setRelayStateAndFactor(LOW); //blue
      lightMode = 3;
    } else if (sec == 1 || sec == 3 || sec == 5 || sec == 7 || sec == 9 || sec == 11) {
      relayFactor = true;
    }
  }
  setRelayHhMmFromEeprom(4, 6); //mixSunrise
  if (rtClock.hours == relayHh && rtClock.minutes == relayMm) {
    if (sec == 0 && relayFactor) {
      setRelayStateAndFactor(HIGH); //blue off
    } else if (sec == 2 && relayFactor) {
      setRelayStateAndFactor(LOW); //white
    } else if (sec == 4 && relayFactor) {
      setRelayStateAndFactor(HIGH);
    } else if (sec == 6 && relayFactor) {
      setRelayStateAndFactor(LOW); //mix
      lightMode = 2;
    } else if (sec == 1 || sec == 3 || sec == 5 || sec == 7) {
      relayFactor = true;
    }
  }
  setRelayHhMmFromEeprom(8, 10); //sunrise
  if (rtClock.hours == relayHh && rtClock.minutes == relayMm) {
    if (sec == 0 && relayFactor) {
      setRelayStateAndFactor(HIGH); //mix off
    } else if (sec == 2 && relayFactor) {
      setRelayStateAndFactor(LOW); //blue
    } else if (sec == 4 && relayFactor) {
      setRelayStateAndFactor(HIGH);
    } else if (sec == 6 && relayFactor) {
      setRelayStateAndFactor(LOW); //white
      lightMode = 1;
    } else if (sec == 1 || sec == 3 || sec == 5 || sec == 7) {
      relayFactor = true;
    }
  }
  setRelayHhMmFromEeprom(12, 14); //mixSunset
  if (rtClock.hours == relayHh && rtClock.minutes == relayMm) {
    if (sec == 0 && relayFactor) {
      setRelayStateAndFactor(HIGH); //white off
    } else if (sec == 2 && relayFactor) {
      setRelayStateAndFactor(LOW); //mix
      lightMode = 2;
    } else if (sec == 1 || sec == 3) {
      relayFactor = true;
    }
  }
  setRelayHhMmFromEeprom(16, 18); //blueSunset
  if (rtClock.hours == relayHh && rtClock.minutes == relayMm) {
    if (sec == 0 && relayFactor) {
      setRelayStateAndFactor(HIGH); //mix off
    } else if (sec == 2 && relayFactor) {
      setRelayStateAndFactor(LOW); //blue
      lightMode = 3;
    } else if (sec == 1 || sec == 3) {
      relayFactor = true;
    }
  }
  setRelayHhMmFromEeprom(20, 22); //sunset
  if (rtClock.hours == relayHh && rtClock.minutes == relayMm) {
    if (sec == 0 && relayFactor) {
      setRelayStateAndFactor(HIGH); //blue off
      lightMode = 4;
    } else if (sec == 1) {
      relayFactor = true;
    }
  }
}
