#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <EncButton.h>
#include <MsTimer2.h>
#include <OneWire.h>
#include <EEPROM.h>

#define POWER_MODE 0       // режим питания, 0 - внешнее, 1 - паразитное
#define MEASURE_PERIOD 500 // время измерения, * 2 мс

#define CLK 2
#define DT 3
#define SW 4
#define PIN_DS18B20 5
#define PIN_PELTE 12
#define PIN_OUT_VENT 11
#define PIN_IN_VENT 10

byte celciy[8] = {
    B00111,
    B00101,
    B00111,
    B00000,
    B00000,
    B00000,
    B00000,
    B00000};

byte histeresys[8] = {
    B01111,
    B01010,
    B01010,
    B01010,
    B01010,
    B01010,
    B01010,
    B11110};

byte cursor[8] = {
    B11000,
    B11100,
    B01110,
    B00111,
    B00111,
    B01110,
    B11100,
    B11000};

LiquidCrystal_I2C lcd(0x27, 16, 2);

EncButton<EB_TICK, 2, 3, 4> enc;

OneWire sensDs(5); // датчик подключен к выводу 14

int timeCount;         // счетчик времени измерения
boolean flagSensReady; // признак готовности данных с датчика
byte bufData[9];       // буфер данных
float temperature;     // измеренная температура

float current_temp, set_temp, prev_temp;
float set_his = 2;

bool pelte_ON, in_vent_ON, out_vent_ON;

byte setting_number = 1;
bool set_mode = 0;

uint32_t in_vent_timer, out_vent_timer;

void isrCLK()
{
  enc.tick(); // отработка в прерывании
}
void isrDT()
{
  enc.tick(); // отработка в прерывании
}

void timerInterrupt()
{

  // управление датчиком DS18B20 паралллельным процессом
  timeCount++;
  if (timeCount >= MEASURE_PERIOD)
  {
    timeCount = 0;
    flagSensReady = true;
  }

  if (timeCount == 0)
    sensDs.reset(); // сброс шины
  if (timeCount == 1)
    sensDs.write(0xCC, POWER_MODE); // пропуск ROM
  if (timeCount == 2)
    sensDs.write(0x44, POWER_MODE); // инициализация измерения

  if (timeCount == 480)
    sensDs.reset(); // сброс шины
  if (timeCount == 481)
    sensDs.write(0xCC, POWER_MODE); // пропуск ROM
  if (timeCount == 482)
    sensDs.write(0xBE, POWER_MODE); // команда чтения памяти датчика

  if (timeCount == 483)
    bufData[0] = sensDs.read(); // чтение памяти датчика
  if (timeCount == 484)
    bufData[1] = sensDs.read(); // чтение памяти датчика
  if (timeCount == 485)
    bufData[2] = sensDs.read(); // чтение памяти датчика
  if (timeCount == 486)
    bufData[3] = sensDs.read(); // чтение памяти датчика
  if (timeCount == 487)
    bufData[4] = sensDs.read(); // чтение памяти датчика
  if (timeCount == 488)
    bufData[5] = sensDs.read(); // чтение памяти датчика
  if (timeCount == 489)
    bufData[6] = sensDs.read(); // чтение памяти датчика
  if (timeCount == 490)
    bufData[7] = sensDs.read(); // чтение памяти датчика
  if (timeCount == 491)
    bufData[8] = sensDs.read(); // чтение памяти датчика
}

void settings()
{
  if (enc.held()) // вхождение в меню настроек
    set_mode = !set_mode;

  if (setting_number == 3)
    setting_number = 1;
  if (setting_number == 0)
    setting_number = 2;

  if (set_mode)
  {
    if (enc.rightH())
      setting_number++;
    if (enc.leftH())
      setting_number--;

    lcd.setCursor(12, 0);
    lcd.print("set");

    switch (setting_number)
    {
    case 1:

      if (enc.right()) // настройка уставки
        set_temp = set_temp - 0.5;
      if (enc.left())
        set_temp = set_temp + 0.5;

      EEPROM.put(0, set_temp);

      lcd.setCursor(0, 1);
      lcd.write(3);
      lcd.setCursor(9, 1);
      lcd.print(" ");
      break;

    case 2:

      if (enc.right()) // настройка гистерезиса
        set_his = set_his - 0.5;
      if (enc.left())
        set_his = set_his + 0.5;

      EEPROM.put(4, set_his);

      lcd.setCursor(9, 1);
      lcd.write(3);
      lcd.setCursor(0, 1);
      lcd.print(" ");
      break;
    }
  }
  else
  {
    setting_number = 1;
    lcd.setCursor(12, 0);
    lcd.print("   ");
    lcd.setCursor(0, 1);
    lcd.print(" ");
    lcd.setCursor(9, 1);
    lcd.print(" ");
  }
}

void drawUI()
{
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.setCursor(2, 0);
  lcd.print(current_temp, 1);
  lcd.setCursor(6, 0);
  lcd.write(1);
  lcd.setCursor(7, 0);
  lcd.print("C");

  lcd.setCursor(1, 1);
  lcd.print("t:");
  lcd.setCursor(3, 1);
  lcd.print(set_temp, 1);
  // lcd.setCursor(7, 1);
  // lcd.write(1);
  lcd.setCursor(7, 1);
  lcd.print("C");

  lcd.setCursor(10, 1);
  lcd.write(2);
  lcd.setCursor(11, 1);
  lcd.print(":");
  lcd.setCursor(12, 1);
  lcd.print(set_his, 1);
  // lcd.setCursor(14, 1);
  // lcd.write(1);
  lcd.setCursor(15, 1);
  lcd.print("C");
}

void getTemp()
{
  if (flagSensReady == true)
  {
    flagSensReady = false;
    // данные готовы
    if (OneWire::crc8(bufData, 8) == bufData[8])
    { // проверка CRC
      // данные правильные
      temperature = (float)((int)bufData[0] | (((int)bufData[1]) << 8)) * 0.0625 + 0.03125;

      current_temp = temperature;
    }
  }
}

void setDoer()
{
  if (pelte_ON)
    digitalWrite(PIN_PELTE, 0);
  if (!pelte_ON)
    digitalWrite(PIN_PELTE, 1);

  if (in_vent_ON)
    digitalWrite(PIN_IN_VENT, 1);
  if (!in_vent_ON)
    digitalWrite(PIN_IN_VENT, 0);

  if (pelte_ON)
    digitalWrite(PIN_OUT_VENT, 1); // ---------------------
  if (!pelte_ON)
    digitalWrite(PIN_OUT_VENT, 0); // ---------------------
}

void setPelte()
{
  if (current_temp > prev_temp)
  {

    if (current_temp > set_temp + set_his)
    {
      pelte_ON = 1;
      // in_vent_ON = 1;
    }
  }
  else if (current_temp < prev_temp)
  {
    if (current_temp <= set_temp)
    {
      pelte_ON = 0;
      // out_vent_ON = 0;
    }
  }

  prev_temp = current_temp;
  setDoer();
}

void setInVent()
{
  if (pelte_ON)
  {
    if (millis() - in_vent_timer >= 20 * 1000)
    {
      in_vent_timer = millis(); // сброс таймера

      in_vent_ON = !in_vent_ON;
    }
  }
  else
  {
    in_vent_ON = 0;
  }
  setDoer();
}

void setOutVent()
{
  if (pelte_ON)
  {
    if (millis() - out_vent_timer >= 1 * 1000)
    {
      out_vent_timer = millis(); // сброс таймера

      out_vent_ON = !out_vent_ON;
    }
  }
  else
  {
    out_vent_ON = 0;
  }
  setDoer();
}

void setup()
{
  lcd.init();
  lcd.backlight(); // Включаем подсветку дисплея

  attachInterrupt(0, isrCLK, CHANGE); // прерывание на 2 пине! CLK у энка
  attachInterrupt(1, isrDT, CHANGE);  // прерывание на 3 пине! DT у энка

  lcd.createChar(1, celciy);
  lcd.createChar(2, histeresys);
  lcd.createChar(3, cursor);

  EEPROM.get(0, set_temp);
  EEPROM.get(4, set_his);

  MsTimer2::set(2, timerInterrupt); // задаем период прерывания по таймеру 2 мс
  MsTimer2::start();                // разрешаем прерывание по таймеру

  pinMode(PIN_PELTE, OUTPUT);
  digitalWrite(PIN_PELTE, 0);
  pinMode(PIN_OUT_VENT, OUTPUT);
  digitalWrite(PIN_OUT_VENT, 0);
  pinMode(PIN_IN_VENT, OUTPUT);
  digitalWrite(PIN_IN_VENT, 0);
}

void loop()
{
  enc.tick();
  if (enc.turn())
    lcd.clear();

  setPelte();
  setInVent();
  // setOutVent();
  getTemp();
  drawUI();
  settings();
}
