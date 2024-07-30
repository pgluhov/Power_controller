
#include "Defines.h"
#include "driver/uart.h"

#pragma pack(push, 1) // используем принудительное выравнивание
struct Tx_buff{       // Структура передатчик на основной контроллер
  int Row;
  int Column;
  int RawBits;
  bool statPress;
  int enc_step=0;
  int enc_stepH = 0;
  int enc_click=0;
  int enc_held=0;
  byte crc;
};
#pragma pack(pop)
Tx_buff TxBuff;

#pragma pack(push, 1) // используем принудительное выравнивание
struct Rx_buff{       // Структура приемник от основного блока
  int x;
  uint8_t y;  
  byte crc;
};
#pragma pack(pop)
Rx_buff RxBuff;

//--------------------------------------------------------------------------------------------

TaskHandle_t Task1;
TaskHandle_t Task2;
TaskHandle_t Task3;
TaskHandle_t Task4;
void Task1code(void* pvParameters);
void Init_Task1();
void Task2code(void* pvParameters);
void Init_Task2();
void Task3code(void* pvParameters);
void Init_Task3();
void Task4code(void* pvParameters);
void Init_Task4();

SemaphoreHandle_t recive_uart1_mutex;

QueueHandle_t QueueHandleKeyboard; // Определить дескриптор очереди для клавиатуры
const int QueueElementSizeBtn = 50;
typedef struct{
  int activeRow;
  int activeColumn;
  int statusColumn;
  bool statPress;   
  int enc_step=0;
  int enc_stepH=0; 
  int enc_click=0;
  int enc_held=0;
} btn_message_t;

QueueHandle_t QueueHandleUart; // Определить дескриптор очереди для приема от основного контроллера
const int QueueElementSizeUart = 10;
typedef struct{
  int x;
  uint8_t y;
} message_uart;


//--------------------------------------------------------------------------------------------

#include <EEPROM.h>
struct valueEEprom {  // структура с переменными   
  bool SW_Slave1;
  bool SW_Slave2; 
  bool SW_Slave3;
}; 
valueEEprom EE_VALUE;

//--------------------------------------------------------------------------------------------

#define EB_FAST 60     // таймаут быстрого поворота, мс
#define EB_DEB 40      // дебаунс кнопки, мс
#define EB_CLICK 200   // таймаут накликивания, мс
#include <EncButton2.h>
EncButton2<EB_ENCBTN> enc(INPUT, ENCODER_A, ENCODER_B, BTN_ENC);  // энкодер с кнопкой
hw_timer_t *timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

//--------------------------------------------------------------------------------------------
/*Прототипы функций*/
byte crc8_bytes(byte *buffer, byte size);
void IRAM_ATTR serialEvent1();

void IRAM_ATTR onTimer(){ // Опрос энкодера
  portENTER_CRITICAL_ISR(&timerMux);
  enc.tick();    // опрос энкодера  
  portEXIT_CRITICAL_ISR(&timerMux);
}

void Task1code(void* pvParameters) {  // Опрос клавиатуры
  #if (ENABLE_DEBUG_TASK == 1)
  Serial.print("Task1code running on core ");
  Serial.println(xPortGetCoreID());
  #endif 
  
  btn_message_t message;
  bool statPin = 1;
  int activeRow = 0;           // Номер активного ряда 
  bool statPress = 0;          // 1 - это нажата кнопка
  uint8_t columnPress = 0;     // на каком столбце зафиксирована кнопка
  uint8_t statusColumn = 255;  // Каждый бит это статус соответствующей кнопки
  uint8_t activeColumn = 0;    // Номер активного столбца
  //uint8_t oldStatusColumn = 0; // Старое значение 
  //uint8_t oldActiveColumn = 0; // Старое значение   
  bool activeMask[8] = {0,1,1,1,1,1,1,1};
  int countPins = sizeof(rowArray) / sizeof(rowArray[0]);
  int countColums = sizeof(columnArray) / sizeof(columnArray[0]);
  for (int i=0; i<countColums; i++){    
    digitalWrite(columnArray[i], activeMask[i]); 
    }
     

  for (;;) {
    statusColumn = 255;
    activeRow = -1;
    for(int i=0; i<countPins; i++){ // Опрос всего столбца в statusColumn
      statPin = digitalRead(rowArray[i]);
      if (statPin == 0){activeRow = i;}
      bitWrite(statusColumn, i, statPin);
    } 
       
    if (statusColumn == 255 && statPress==1 && activeColumn==columnPress){ // Отправка когда кнопка отпущена с флагом 0
      message.statPress = statPress = 0;
      message.activeRow = activeRow;
      message.statusColumn = statusColumn;
      if(QueueHandleKeyboard != NULL && uxQueueSpacesAvailable(QueueHandleKeyboard) > 0){ // проверьте, существует ли очередь И есть ли в ней свободное место
        int ret = xQueueSend(QueueHandleKeyboard, (void*) &message, 0);
        if(ret == pdTRUE){
          #if (ENABLE_DEBUG_KEYB_TASK1 == 1)
          Serial.print("Task1 Отправлены данные в очередь "); 
          Serial.println(statusColumn, BIN);
          Serial.print("Task1 номер активного ряда   " );
          Serial.println(message.activeRow); 
          Serial.print("Task1 номер активного столбца   " );
          Serial.println(message.activeColumn);
          Serial.print("Task1 статус нажатия   " );
          Serial.println(message.statPress); 
          Serial.println(); 
          #endif    
        }
        else if(ret == errQUEUE_FULL){Serial.println("Не удалось отправить данные в очередь из Task1code");
        } 
    }
    else if (QueueHandleKeyboard != NULL && uxQueueSpacesAvailable(QueueHandleKeyboard) == 0){Serial.println("Очередь отсутствует или нет свободного места");}
    } 
    
    
    //if (statusColumn != 255 && statPress == 0 && (activeColumn != oldActiveColumn || statusColumn != oldStatusColumn)){ // Отправка только если что-то нажато с флагом 1
    if (statusColumn != 255 && statPress == 0 ){ // Отправка только если что-то нажато с флагом 1
      //oldActiveColumn = activeColumn;
      //oldStatusColumn = statusColumn; 
      columnPress = activeColumn;
      message.statPress = statPress = 1;
      message.activeRow = activeRow; 
      message.activeColumn = activeColumn;
      message.statusColumn = statusColumn;
      if(QueueHandleKeyboard != NULL && uxQueueSpacesAvailable(QueueHandleKeyboard) > 0){ // проверьте, существует ли очередь И есть ли в ней свободное место
        int ret = xQueueSend(QueueHandleKeyboard, (void*) &message, 0);
        if(ret == pdTRUE){
          #if (ENABLE_DEBUG_KEYB_TASK1 == 1)
          Serial.print("Task1 Отправлены данные в очередь "); 
          Serial.println(statusColumn, BIN); 
          Serial.print("Task1 номер активныого ряда   " );
          Serial.println(message.activeRow); 
          Serial.print("Task1 номер активныого столбца   " );
          Serial.println(message.activeColumn);
          Serial.print("Task1 статус нажатия   " );
          Serial.println(message.statPress); 
          Serial.println(); 
          #endif    
        }
        else if(ret == errQUEUE_FULL){Serial.println("Не удалось отправить данные в очередь из Task1code");
        } 
    }
    else if (QueueHandleKeyboard != NULL && uxQueueSpacesAvailable(QueueHandleKeyboard) == 0){Serial.println("Очередь отсутствует или нет свободного места");}
    } 
    
    activeMask[activeColumn] = 1; 
    activeColumn ++;    
    if (activeColumn == countColums){activeColumn = 0; activeMask[activeColumn] = 0;}
    if (activeColumn < countColums){activeMask[activeColumn] = 0;}
    for (int i=0; i<countColums; i++){    
      digitalWrite(columnArray[i], activeMask[i]);
      }
     
    vTaskDelay(15);               
   }
}

void Init_Task1() {  //создаем задачу
  xTaskCreatePinnedToCore(
    Task1code, /* Функция задачи. */
    "Task1",   /* Ее имя. */
    4096,      /* Размер стека функции */
    NULL,      /* Параметры */
    1,         /* Приоритет */
    &Task1,    /* Дескриптор задачи для отслеживания */
    0);        /* Указываем пин для данного ядра */
  delay(500);
}

void Task2code(void* pvParameters) {  // Отправка команд через Serial1 на основной контроллер
  #if (ENABLE_DEBUG_TASK == 1)
  Serial.print("Task2code running on core ");
  Serial.println(xPortGetCoreID()); 
  #endif  
  btn_message_t message;
  
  for (;;) {
      if(QueueHandleKeyboard != NULL){ // Проверка работоспособности просто для того, чтобы убедиться, что очередь действительно существует
      int ret = xQueueReceive(QueueHandleKeyboard, &message, portMAX_DELAY);
      if(ret == pdPASS){
        #if (ENABLE_DEBUG_KEYB == 1)
        Serial.print("Task2 получены данные из очереди  " );  
        Serial.println(message.statusColumn,BIN); 
        Serial.print("Task2 номер активныого ряда   " );
        Serial.println(message.activeRow); 
        Serial.print("Task2 номер активныого столбца   " );
        Serial.println(message.activeColumn); 
        Serial.print("Task2 статус нажатия   " );
        Serial.println(message.statPress);  
        Serial.print("enc_step значение " );
        Serial.println(message.enc_step);         
        Serial.print("enc_click значение " );
        Serial.println(message.enc_click);         
        Serial.print("enc_held значение " );
        Serial.println(message.enc_held); 
        Serial.println(); 
        #endif

      #if (ENABLE_DEBUG_TERMINAL == 1)  
        //if (enc.left()) Serial.println("left");     // поворот налево
        //if (enc.right()) Serial.println("right");   // поворот направо
        //if (enc.leftH()) Serial.println("leftH");   // нажатый поворот налево
        //if (enc.rightH()) Serial.println("rightH"); // нажатый поворот направо
        //if (enc.press()) Serial.println("press");
        //if (enc.click()) Serial.println("click");
        //if (enc.release()) Serial.println("release"); 
        //if (enc.held()) Serial.println("held");      // однократно вернёт true при удержании 

        Serial.print("номер ряда ");
        Serial.println(message.activeRow); 
        Serial.print("номер столбца ");
        Serial.println(message.activeColumn); 
        Serial.print("статус нажатия ");
        Serial.println(message.statPress); 
        Serial.println(); 
      #endif
        

        TxBuff.Row = message.activeRow;        // Номер строки
        TxBuff.Column = message.activeColumn;  // Номер столбца
        TxBuff.RawBits = message.statusColumn; // Байт с битами всего столбца
        TxBuff.statPress = message.statPress;  // Статус нажата или отпущена кнопка         
        TxBuff.enc_step = message.enc_step; 
        TxBuff.enc_stepH = message.enc_stepH; 
        TxBuff.enc_click = message.enc_click;
        TxBuff.enc_held = message.enc_held;               
        TxBuff.crc = crc8_bytes((byte*)&TxBuff, sizeof(TxBuff) - 1);

        Serial1.write((byte*)&TxBuff, sizeof(TxBuff));       
        }
      }
   }
}

void Init_Task2() {  //создаем задачу
  xTaskCreatePinnedToCore(
    Task2code, /* Функция задачи. */
    "Task2",   /* Ее имя. */
    4096,      /* Размер стека функции */
    NULL,      /* Параметры */
    1,         /* Приоритет */
    &Task2,    /* Дескриптор задачи для отслеживания */
    1);        /* Указываем пин для данного ядра */
  delay(500);
}

void Task3code(void* pvParameters) {  // Обработка приема данных от Serial1
  #if (ENABLE_DEBUG_TASK == 1)
  Serial.print("Task3code running on core ");
  Serial.println(xPortGetCoreID()); 
  #endif 
  message_uart message;
  
    for (;;) { 
      if(QueueHandleUart != NULL){ // Проверка работоспособности просто для того, чтобы убедиться, что очередь действительно существует
      int ret = xQueueReceive(QueueHandleUart, &message, portMAX_DELAY);
      if(ret == pdPASS){ 
         #if (ENABLE_DEBUG_UART == 1)
        Serial.println("Task3 получены данные от  serialEvent1" );         
        Serial.print("Task3 переменная 1: " );
        Serial.println(message.x); 
        Serial.print("Task3 переменная 2: " );
        Serial.println(message.y);        
        Serial.println(); 
        #endif 
      }
    }
  }  
}

void Init_Task3() {  //создаем задачу
  xTaskCreatePinnedToCore(
    Task3code, /* Функция задачи. */
    "Task3",   /* Ее имя. */
    4096,      /* Размер стека функции */
    NULL,      /* Параметры */
    1,         /* Приоритет */
    &Task3,    /* Дескриптор задачи для отслеживания */
    1);        /* Указываем пин для данного ядра */
  delay(500);
}

void Task4code(void* pvParameters) {  // Функции энкодера
  #if (ENABLE_DEBUG_TASK == 1)
  Serial.print("Task4code running on core ");
  Serial.println(xPortGetCoreID());
  #endif 

  for (;;) {
  
  btn_message_t message;
    
  // =============== ЭНКОДЕР ===============  
  if (enc.left()) {message.enc_step   = -1; message.activeRow=-1;} // поворот налево 
  if (enc.right()){message.enc_step   =  1; message.activeRow=-1;} // поворот направо 
  if (enc.click()){message.enc_click  =  1; message.activeRow=-1;} 
  if (enc.held()) {message.enc_held   =  1; message.activeRow=-1;}
  if (enc.leftH()) {message.enc_stepH = -1; message.activeRow=-1;} // поворот налево нажатый
  if (enc.rightH()){message.enc_stepH =  1; message.activeRow=-1;} // поворот направо нажатый
        
  #if (ENABLE_DEBUG_ENC == 1)  
  if (enc.left()) Serial.println("left");     // поворот налево
  if (enc.right()) Serial.println("right");   // поворот направо
  if (enc.leftH()) Serial.println("leftH");   // нажатый поворот налево
  if (enc.rightH()) Serial.println("rightH"); // нажатый поворот направо
  if (enc.press()) Serial.println("press");
  if (enc.click()) Serial.println("click");
  if (enc.release()) Serial.println("release"); 
  if (enc.held()) Serial.println("held");      // однократно вернёт true при удержании 
  #endif

  if(message.enc_step!=0 || message.enc_click!=0 || message.enc_held!=0 || message.enc_stepH!=0){    
  if(QueueHandleKeyboard != NULL && uxQueueSpacesAvailable(QueueHandleKeyboard) > 0){ // проверьте, существует ли очередь И есть ли в ней свободное место
     int ret = xQueueSend(QueueHandleKeyboard, (void*) &message, 0);
     if(ret == pdTRUE){
        #if (ENABLE_DEBUG_ENC == 1)          
        Serial.print("Task4 enc_step " );
        Serial.println(message.enc_step); 
        Serial.print("Task4 enc_click " );
        Serial.println(message.enc_click); 
        Serial.print("Task4 enc_held " );
        Serial.println(message.enc_held); 
        Serial.print("Task4 enc_stepH " );
        Serial.println(message.enc_stepH); 
        Serial.println(); 
        #endif       
       }
     else if(ret == errQUEUE_FULL){Serial.println("Не удалось отправить данные в очередь из Task3code");}         
    }
    else if (QueueHandleKeyboard != NULL && uxQueueSpacesAvailable(QueueHandleKeyboard) == 0){Serial.println("Очередь отсутствует или нет свободного места");}
   } 
 
   enc.resetState();     
   vTaskDelay(10/portTICK_PERIOD_MS);    
  }
}

void Init_Task4() {  //создаем задачу
  xTaskCreatePinnedToCore(
    Task4code, /* Функция задачи. */
    "Task4",   /* Ее имя. */
    4096,      /* Размер стека функции */
    NULL,      /* Параметры */
    2,         /* Приоритет */
    &Task4,    /* Дескриптор задачи для отслеживания */
    0);        /* Указываем пин для данного ядра */
  delay(500);
}

void IRAM_ATTR serialEvent1(){   
  #if (ENABLE_DEBUG_UART == 1)  
  Serial.println("Есть данные в прерывании Serial1");  
  #endif
  if (Serial1.readBytes((byte*)&RxBuff, sizeof(RxBuff))) {
  byte crc = crc8_bytes((byte*)&RxBuff, sizeof(RxBuff));
  if (crc == 0) {
    #if (ENABLE_DEBUG_UART == 1)
    Serial.println("CRC PASSED");
    #endif       
      message_uart message; 
      message.x = RxBuff.x;
      message.y = RxBuff.y;
      if(QueueHandleUart != NULL && uxQueueSpacesAvailable(QueueHandleUart) > 0){ // проверьте, существует ли очередь И есть ли в ней свободное место
        int ret = xQueueSend(QueueHandleUart, (void*) &message, 0);
        if(ret == pdTRUE){
          #if (ENABLE_DEBUG_UART == 1)
          Serial.println("serialEvent1 Отправлены данные в очередь ");      
          #endif    
          }
        else if(ret == errQUEUE_FULL){Serial.println("Не удалось отправить данные в очередь из serialEvent1()");
        } 
      }
      else if (QueueHandleUart != NULL && uxQueueSpacesAvailable(QueueHandleUart) == 0){Serial.println("Очередь отсутствует или нет свободного места");}
      } 
   else {
      Serial.println("CRC ERROR");
     }
   }  
}

byte crc8_bytes(byte *buffer, byte size) {
  byte crc = 0;
  for (byte i = 0; i < size; i++) {
    byte data = buffer[i];
    for (int j = 8; j > 0; j--) {
      crc = ((crc ^ data) & 1) ? (crc >> 1) ^ 0x8C : (crc >> 1);
      data >>= 1;
    }
  }
  return crc;
}

void boardInfo() {
  Serial.println();
  Serial.println("ESP32 Info.");
  Serial.println("****************************************");
  //Serial.print("WiFi MAC:               ");
  //Serial.println(WiFi.macAddress());
  //Serial.print("WiFi MAC:               ");
  //Serial.println(EE_VALUE.Text_This_Mac_Dec);  
  uint64_t chipid;
  chipid = ESP.getEfuseMac();                                          // The chip ID is essentially its MAC address(length: 6 bytes).   
  Serial.printf("ESP32 Chip ID  \t\t%04X", (uint16_t)(chipid >> 32));  // print High 2 bytes
  Serial.printf("%08X\n", (uint32_t)chipid);                           // print Low 4bytes.
  Serial.printf("Flash chip frequency:\t%d (Hz)\n", ESP.getFlashChipSpeed());
  Serial.printf("Flash chip size:\t%d (bytes)\n", ESP.getFlashChipSize());
  Serial.printf("Free heap size:\t\t%d (bytes)\n", ESP.getFreeHeap());
  Serial.println();  
}

void INIT_DEFAULT_VALUE(){ // Заполняем переменные в EEPROM базовыми значениями 
    EEPROM.put(0, EE_VALUE);  // сохраняем
    EEPROM.commit();          // записываем
}

void INIT_IO(){
  #if (ENABLE_DEBUG_ALL == 1)
  Serial.println();
  #endif
  int quantity = sizeof(columnArray) / sizeof(columnArray[0]);
  for(int i=0; i<quantity; i++){
    pinMode(columnArray[i], OUTPUT);
    #if (ENABLE_DEBUG_ALL == 1)
    Serial.print("Инициализован пин как выход: ");
    Serial.println(columnArray[i]);
    #endif
  }  
  quantity = sizeof(rowArray) / sizeof(rowArray[0]);
  for(int i=0; i<quantity; i++){
    pinMode(rowArray[i], INPUT);
    #if (ENABLE_DEBUG_ALL == 1)
    Serial.print("Инициализован пин как вход: ");
    Serial.println(rowArray[i]);
    #endif
  }
  
}

void INIT_TIM_ENC(){
  timer = timerBegin(0, 80, true);    // 80 000 000 Hz тактирование шины
  timerAttachInterrupt(timer, &onTimer, false);
  timerAlarmWrite(timer, 8000, true); // 8000 делитель таймера
  timerAlarmEnable(timer); // Enable таймер f = 80000000/8000 => 10 kHz
}

void setup() {     
  Serial.setTimeout(5);
  Serial.begin(115200, SERIAL_8N1, 3, 1);  
  delay(10);   
  Serial1.setTimeout(5);
  Serial1.begin(115200, SERIAL_8N1, RXPIN, TXPIN);
  delay(10);
  
  INIT_IO(); 
  INIT_TIM_ENC();
  
  QueueHandleKeyboard = xQueueCreate(QueueElementSizeBtn, sizeof(btn_message_t)); // Создайте очередь, которая будет содержать <Размер элемента очереди> количество элементов, каждый из которых имеет размер `btn_message_t`, и передайте адрес в <QueueHandleKeyboard>.
  if(QueueHandleKeyboard == NULL){  // Проверьте, была ли успешно создана очередь
    Serial.println("QueueHandleKeyboard could not be created. Halt.");
    while(1) delay(1000);   // Halt at this point as is not possible to continue
  }

  QueueHandleUart = xQueueCreate(QueueElementSizeUart, sizeof(message_uart)); // Создайте очередь, которая будет содержать <Размер элемента очереди> количество элементов, каждый из которых имеет размер `message_t`, и передайте адрес в <QueueHandleKeyboard>.
  if(QueueHandleUart == NULL){  // Проверьте, была ли успешно создана очередь
    Serial.println("QueueHandleUart could not be created. Halt.");
    while(1) delay(1000);   // Halt at this point as is not possible to continue
  }
    
  EEPROM.begin(2048);
  EEPROM.get(0, EE_VALUE); //читаем всё из памяти 
  
  boardInfo();
  
  Init_Task1(); // Опрос клавиатуры
  Init_Task2(); // Отпрака результата по Serial1 
  Init_Task3(); // прием данных от Serial1
  Init_Task4(); // Обработка энкодера

}

void loop() {    
  
  }


  
