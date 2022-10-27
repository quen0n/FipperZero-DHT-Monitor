#ifndef QUENON_DHT_MON
#define QUENON_DHT_MON

#include <stdlib.h>
#include <stdio.h>

#include <furi.h>
#include <furi_hal_power.h>

#include <gui/gui.h>
#include <gui/modules/submenu.h>
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

    char txtbuff[30]; //Буффер для печати строк на экране
    bool last_OTG_State; //Состояние OTG до запуска приложения
    Storage* storage; //Хранилище датчиков
    Stream* file_stream; //Поток файла с датчиками
    int8_t sensors_count; // Количество загруженных датчиков
    DHT_sensor sensors[8]; //Сохранённые датчики
    DHT_data data; //Инфа из датчика

} PluginData;

void scene_main(Canvas* const canvas, PluginData* app);

#endif