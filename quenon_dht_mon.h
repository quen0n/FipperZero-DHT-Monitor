#ifndef QUENON_DHT_MON
#define QUENON_DHT_MON

#include <stdlib.h>
#include <stdio.h>

#include <furi.h>
#include <furi_hal_power.h>

#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/text_input.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>

#include <notification/notification.h>
#include <notification/notification_messages.h>

#include <toolbox/stream/file_stream.h>
#include <input/input.h>

#include "DHT.h"

#define APP_NAME "DHT monitor"
#define APP_PATH_FOLDER "/ext/DHT monitor"
#define APP_FILENAME "sensors.txt"

// //Виды менюшек
typedef enum {
    MAIN_MENU_VIEW,
    ADDSENSOR_MENU_VIEW,
    TEXTINPUT_VIEW,
    SENSOR_ACTIONS_VIEW
} MENU_VIEWS;

typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} PluginEvent;

typedef struct {
    const uint8_t name;
    const GpioPin* pin;
} GpioItem;

//Структура с данными плагина
typedef struct {
    //Очередь сообщений
    FuriMessageQueue* event_queue;
    //Мутекс
    ValueMutex state_mutex;
    //Вьюпорт
    ViewPort* view_port;
    //GUI
    Gui* gui;
    NotificationApp* notifications;
    Submenu* submenu;
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    View* view;
    TextInput* text_input;
    VariableItem* item;
    VariableItemList* variable_item_list;

    char txtbuff[30]; //Буффер для печати строк на экране
    bool last_OTG_State; //Состояние OTG до запуска приложения
    Storage* storage; //Хранилище датчиков
    Stream* file_stream; //Поток файла с датчиками
    int8_t sensors_count; // Количество загруженных датчиков
    DHT_sensor sensors[8]; //Сохранённые датчики
    DHT_data data; //Инфа из датчика
    DHT_sensor* currentSensorEdit; //Указатель на редактируемый датчик

} PluginData;

const GpioPin* index_to_gpio(uint8_t index);

bool DHT_sensors_reload(void);
uint8_t DHT_sensors_save(void);

void scene_main(Canvas* const canvas, PluginData* app);
void mainMenu_scene(PluginData* app);
void addSensor_scene(PluginData* app);
void sensorActions_scene(PluginData* app);
#endif