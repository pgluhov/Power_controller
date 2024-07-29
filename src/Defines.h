
#ifndef DEFINES_H
#define DEFINES_H
#include <Arduino.h>

// ========== ДЕФАЙНЫ НАСТРОЕК ==========

#define ENABLE_DEBUG_TASK 0  // Если 1 то общая отладка в задачах включена
#define ENABLE_DEBUG_ALL  0  // Если 1 то общая отладка включена
#define ENABLE_DEBUG_KEYB_TASK1 0  // Если 1 то отладка клавиатуры включена 
#define ENABLE_DEBUG_KEYB_TASK2 0  // Если 1 то отладка клавиатуры включена 
#define ENABLE_DEBUG_UART 0  // Если 1 то отладка обмена по uart включена 
#define ENABLE_DEBUG_ENC  0  // Если 1 то отладка энкодера в Serial 

String DEVICE_NAME = "Power controller"; // Имя девайса
String CURRENT_VERSION_SW = "1.06";      // Текущая версиия прошивки 
String VERSION_SW = "Версия ПО 1.06";    // Текст для отображения
 
//--------номера IO-------------------

#define RXPIN  33  
#define TXPIN  32

#define BTN_ENC    34 
#define ENCODER_A  39 
#define ENCODER_B  36 

//--------номера IO-------------------

// счет столбца слева-направо
#define column_1  4   // Пин для столбеца 1
#define column_2  16  // Пин для столбеца 2
#define column_3  17  // Пин для столбеца 3
#define column_4  5   // Пин для столбеца 4
int columnArray[] = {column_1, column_2, column_3, column_4};
 

// счет ряда снизу-вверх
#define row_1  18     // Пин для ряда 1
#define row_2  15     // Пин для ряда 2
#define row_3  13     // Пин для ряда 3
#define row_4  14     // Пин для ряда 4
#define row_5  27     // Пин для ряда 5
#define row_6  25     // Пин для ряда 6
int rowArray[] = {row_1, row_2, row_3, row_4, row_5, row_6};

#endif //DEFINES_H