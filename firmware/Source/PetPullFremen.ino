// PET plastic bottle recycler
//
// Version 1.0 release

#include <Arduino.h>
#include "BasicStepperDriver.h"

// Motor steps per revolution. Most steppers are 200 steps or 1.8 degrees/step
#define MOTOR_STEPS 200
#define RPM 120

// Since microstepping is set externally, make sure this matches the selected mode
// If it doesn't, the motor will move at a different RPM than chosen
// 1=full step, 2=half step etc.
#define MICROSTEPS 1

// All the wires needed for full functionality
#define DIR 12
#define STEP 13

//Uncomment line to use enable/disable functionality
//#define SLEEP 13

// 2-wire basic config, microstepping is hardwired on the driver
BasicStepperDriver stepper(MOTOR_STEPS, DIR, STEP);

#include <EEPROM.h>

// Size of the configuration data structure
#define DATA_SIZE sizeof(DataConfig)

// Address of the beginning of structure storage in EEPROM
#define DATA_ADDR 0

// Structure for data storage
struct DataConfig {
  byte numberpet; // Configuration number
  
  int temperature; // Base heating temperature [°C]
  // Отклонение температуры
  byte deviation; // [°C]
  // Начальная скорость двигателя
  int mspeed; // [об/мин]
  // Счетчик витков
  int turns;
};

// Переменные для хранения данных
DataConfig dataConfig;

// NTC Termistor
// подключение: GND --- термистор --- A1 --- 10к --- 5V

// серый 4300, проводной 3950
#include <thermistor.h>

thermistor therm(A1,998);  // Датчик температуры подключается к ноге А1
double tempNTC;
int dtempNTC = 0; // Корректирующая температура при настройке датчика

#define fadePin 11  // Нога управляющая Мосфетом. Управление нагревателем.

//

// SpinTimer library, https://github.com/dniklaus/spin-timer,
//   add it by using the Arduino IDE Library Manager (search for spin-timer)
#include <SpinTimer.h>

// LcdKeypad, https://github.com/dniklaus/arduino-display-lcdkeypad,
//   add it by using the Arduino IDE Library Manager (search for arduino-display-lcdkeypad)
#include <LcdKeypad.h>

LcdKeypad* myLcdKeypad = 0;

void softReset() {
  asm volatile ("jmp 0");
}

bool testModeActive = false; // Режим запуска рабочих процедур
bool workModeActive = false; // Режим запуска работы .... пока не выключат.

// Запуск режима работы механизма
void runPetPull()
{
  testModeActive = true;
  workModeActive = true;
  myLcdKeypad->setCursor(0, 0);  // 
  myLcdKeypad->print("BT000 DT00 SM00");       // 
  myLcdKeypad->setCursor(2, 0);   
  myLcdKeypad->print(dataConfig.temperature);     // Базова температура
  myLcdKeypad->setCursor(8, 0);  
  myLcdKeypad->print(dataConfig.deviation);       // Дельта температуры
  myLcdKeypad->setCursor(13, 0);  
  myLcdKeypad->print(dataConfig.mspeed);          // Скорость дигателя
  stepper.rotate(1);                              // Угол поворота двигателя
   
  int maxTemp = dataConfig.temperature + dataConfig.deviation;
  int avgTemp = dataConfig.temperature + (dataConfig.deviation/2);
  int womTemp = dataConfig.temperature - (dataConfig.deviation/2);

  while ( workModeActive == true){
    tempNTC = therm.analog2temp()-dtempNTC;  // Чтение температуры
    myLcdKeypad->setCursor(2, 1);            // 
    myLcdKeypad->print((String)tempNTC);     // Выводим температуру на экран

    // Проверяем достиг ли нагрев предельной температуры 
    if (tempNTC < maxTemp) {  
      digitalWrite(fadePin, HIGH);  // Включили нагрев
      myLcdKeypad->setCursor(0, 1);
      myLcdKeypad->print("H");
    } 
    // Работа двиателя лишь при температуре до базовой минус половина разброса...
    if (tempNTC > womTemp)     
        stepper.move(-MOTOR_STEPS*MICROSTEPS);   // Запустить работу мотора

    tempNTC = therm.analog2temp()-dtempNTC; // Чтение температуры
    // Отключаем нагрев при температура выше базовой и выше базовой и плюс половина разброса
    if (tempNTC > avgTemp) {
      digitalWrite(fadePin, LOW);  // Выключили нагрев
      myLcdKeypad->setCursor(0, 1);
      myLcdKeypad->print(" ");
    }
  }
  testModeActive = false;
}

// Запуск режима работы тестирования механизма

void testPetPull()
{
    testModeActive = true;
    tempNTC = therm.analog2temp()-dtempNTC;
    myLcdKeypad->setCursor(0, 1);
    myLcdKeypad->print((String)tempNTC);
    digitalWrite(fadePin, LOW);
    myLcdKeypad->setCursor(0, 0);    // Визуализация тестирования
    myLcdKeypad->print("Start Test");
    delay(500); 
    stepper.rotate(360);
    stepper.move(MOTOR_STEPS*MICROSTEPS);  // Мотор вперед
    stepper.move(-MOTOR_STEPS*MICROSTEPS); // Мотор назад
    digitalWrite(fadePin, LOW);
    while (tempNTC<100){
      digitalWrite(fadePin, HIGH); 
      tempNTC = therm.analog2temp()-dtempNTC;
      myLcdKeypad->setCursor(0, 1);
      myLcdKeypad->print((String)tempNTC);
    }
    digitalWrite(fadePin, LOW);
    myLcdKeypad->setCursor(0, 0);    // Визуализация тестирования
    myLcdKeypad->print("End Test    ");
    delay(2000);
    testModeActive = false;
}

// Implement specific LcdKeypadAdapter in order to allow receiving key press events
class MyLcdKeypadAdapter : public LcdKeypadAdapter
{
  private:
    LcdKeypad* m_lcdKeypad;
    unsigned char m_value;
    unsigned char m_currentMenu;
    unsigned char m_subMenu;
  public:
    MyLcdKeypadAdapter(LcdKeypad* lcdKeypad)
      : m_lcdKeypad(lcdKeypad)
      , m_value(7)
      , m_currentMenu(0)
      , m_subMenu(0)
    { }

    // Отображение температуры
    void showTemLCD() {
      double cTemp = therm.analog2temp()-dtempNTC;
      m_lcdKeypad->setCursor(3, 1);
      m_lcdKeypad->print(cTemp);
      m_lcdKeypad->setCursor(6, 1);
      m_lcdKeypad->print("*C");
      delay(100);
    }

    // Specific handleKeyChanged() method implementation - define your actions here
    void handleKeyChanged(LcdKeypad::Key newKey)
    { 
      {
        m_lcdKeypad->clear();
        //showTemLCD();
        switch (newKey)
        {
          case LcdKeypad::UP_KEY:
            if (m_currentMenu < 4)
            {
              m_currentMenu++;
            }
            break;
          case LcdKeypad::DOWN_KEY:
            if (m_currentMenu > 0)
            {
              m_currentMenu--;
            }
            break;
          case LcdKeypad::LEFT_KEY:
            if (m_subMenu==1)
            {
              switch (m_currentMenu)
              {
                 case 0:
                   dataConfig.temperature--;
                   break;
                 case 1:
                   dataConfig.deviation--;
                   break;
                 case 2:
                   dataConfig.mspeed--;
                   break;
               } 
            }
            break;
          case LcdKeypad::RIGHT_KEY:
           if (m_subMenu==1)
            {
              switch (m_currentMenu)
              {
                 case 0:
                   dataConfig.temperature++;
                   break;
                 case 1:
                   dataConfig.deviation++;
                   break;
                 case 2:
                   dataConfig.mspeed++;
                   break;
               }  
            }
            break;    
          case LcdKeypad::SELECT_KEY:
            switch (m_currentMenu)
            {
              case 0:
                // Запуск
                runPetPull();
                break;
              case 1:
                // Настройки
                m_subMenu = 1;
                m_currentMenu = 0;  // Первый элемент
                break;
              case 2:
                // Тест системы
                m_subMenu = 2;
                break;
              case 3:
                // Выход
                // Завершение программы
                if (m_subMenu == 1) // Выход из меню CONFIG
                {
                  m_subMenu = 0;
                  m_currentMenu = 0;
                  // Сохранение данных Config в EEPROM
                  EEPROM.put(DATA_ADDR, dataConfig);
                }
                break;
            }
            case LcdKeypad::NO_KEY:
              //showTemLCD();
              break;

            break;
        }
        // Вывод текущего пункта меню
        switch (m_subMenu)
        {
          case 0:
            // Главное меню
            m_lcdKeypad->setCursor(0, 0);
            m_lcdKeypad->print(m_currentMenu == 0 ? "Start"  :
                               m_currentMenu == 1 ? "Config" :
                               m_currentMenu == 2 ? "Testing" : "Exit");
            break;
          case 1:
            // Меню Config
            m_lcdKeypad->setCursor(0, 0);
            m_lcdKeypad->print(m_currentMenu == 0 ? "Temperature" :
                               m_currentMenu == 1 ? "Deviation" :
                               m_currentMenu == 2 ? "Speed" : "Exit");
            m_lcdKeypad->setCursor(0, 1);
            m_lcdKeypad->print(m_currentMenu == 0 ? String(dataConfig.temperature) :
                               m_currentMenu == 1 ? String(dataConfig.deviation) :
                               m_currentMenu == 2 ? String(dataConfig.mspeed) : "<-");                   
            break;
           case 2: // Test
            testPetPull(); 
            softReset();
            break;  

        }
        m_lcdKeypad->setBacklight(static_cast<LcdKeypad::LcdBacklightColor>(LcdKeypad::LCDBL_WHITE & m_value));
      }
    }
};

void setup()
{
  //Serial.begin(9600); //initialize port serial at 9600 Bauds.
   // Чтение конфигурации данных из EEPROM
  EEPROM.get(DATA_ADDR, dataConfig);

  // Начальные параметры - чтобы на кнопки не клацать
  //dataConfig.numberpet = 1;
  //dataConfig.temperature =210;
  //dataConfig.deviation = 10;
  //dataConfig.mspeed = 40;
  //dataConfig.turns = 20;
  //
  //EEPROM.put(DATA_ADDR, dataConfig);
  
  stepper.begin(dataConfig.mspeed, MICROSTEPS);  // инициализация шагового двигателя
  
  pinMode(fadePin, OUTPUT);   // Нога для нагревателя установить режим.
  digitalWrite(fadePin, LOW); // Нагреватель выкл.
  myLcdKeypad = new LcdKeypad();  // instantiate an object of the LcdKeypad class, using default parameters

  // Attach the specific LcdKeypadAdapter implementation (dependency injection)
  myLcdKeypad->attachAdapter(new MyLcdKeypadAdapter(myLcdKeypad));
  myLcdKeypad->setCursor(0, 0);   // position the cursor at beginning of the first line
  myLcdKeypad->print("Pet Pull System");   // print a Value label on the first line of the display
  tempNTC = therm.analog2temp()-dtempNTC; // start read temperature
  myLcdKeypad->setCursor(0, 1);   // position the cursor at beginning of the first line
  myLcdKeypad->print(tempNTC);   // print a Value label on the first line of the display
  delay(500);
}

void loop()
{
  if (!testModeActive) {
    scheduleTimers();  // Get the timer(s) ticked, in particular the LcdKeypad dirver's keyPollTimer
  }
}
