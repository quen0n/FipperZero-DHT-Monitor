#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <stdio.h>
#include "furi_hal_power.h"

#include "DHT.h"
#include <toolbox/stream/file_stream.h>

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

static const GpioItem gpio_item[] = {
    {2, &gpio_ext_pa7},
    {3, &gpio_ext_pa6},
    {4, &gpio_ext_pa4},
    {5, &gpio_ext_pb3},
    {6, &gpio_ext_pb2},
    {7, &gpio_ext_pc3},
    {15, &gpio_ext_pc1},
    {16, &gpio_ext_pc0},
};

//Структура с данными плагина
typedef struct {
    char txtbuff[25]; //Буффер для печати строк на экране
    bool last_OTG_State; //Состояние OTG до запуска приложения
    Storage* storage; //Хранилище датчиков
    Stream* file_stream; //Поток файла с датчиками
    uint8_t sensors_count; // Количество загруженных датчиков
    DHT_sensor sensors[8]; //Сохранённые датчики
    DHT_data data; //Инфа из датчика
} PluginData;

/* Функция конвертации GPIO в его номер FZ
Принимает GPIO
Возвращает номер порта на корпусе FZ
*/
static uint8_t gpio_to_int(GpioPin gp) {
    for(uint8_t i = 0; i < 8; i++) {
        if(gpio_item[i].pin->pin == gp.pin && gpio_item[i].pin->port == gp.port) {
            return gpio_item[i].name;
        }
    }
    return 255;
}
/* 
Функция инициализации портов ввода/вывода датчиков
*/
void DHT_sensors_init(PluginData* const pd) {
    //Включение 5V если на порту 1 FZ его нет
    if(furi_hal_power_is_otg_enabled() != true) {
        furi_hal_power_enable_otg();
    }

    //Настройка GPIO загруженных датчиков
    for(uint8_t i = 0; i < pd->sensors_count; i++) {
        //Высокий уровень по умолчанию
        furi_hal_gpio_write(&pd->sensors[i].DHT_Pin, true);
        //Режим работы - OpenDrain, подтяжка включается на всякий случай
        furi_hal_gpio_init(
            &pd->sensors[i].DHT_Pin, //Порт FZ
            GpioModeOutputOpenDrain, //Режим работы - открытый сток
            GpioPullUp, //Принудительная подтяжка линии данных к питанию
            GpioSpeedVeryHigh); //Скорость работы - максимальная
    }
}

/* 
Функция деинициализации портов ввода/вывода датчиков
*/
void DHT_sensors_deinit(PluginData* const pd) {
    //Возврат исходного состояния 5V
    if(pd->last_OTG_State != true) {
        furi_hal_power_disable_otg();
    }

    //Настройка портов на вход
    for(uint8_t i = 0; i < pd->sensors_count; i++) {
        //Режим работы - OpenDrain, подтяжка включается на всякий случай
        furi_hal_gpio_init(
            &pd->sensors[i].DHT_Pin, //Порт FZ
            GpioModeInput, //Режим работы - вход
            GpioPullNo, //Отключение подтяжки
            GpioSpeedLow); //Скорость работы - низкая
        //Установка низкого уровня
        furi_hal_gpio_write(&pd->sensors[i].DHT_Pin, false);
    }
}

/* 
Функция сохранения датчиков в файл
Возвращает количество сохранённых датчиков
*/
uint8_t DHT_sensors_save(PluginData* const pd) {
    //Выделение памяти для потока
    pd->file_stream = file_stream_alloc(pd->storage);
    //Переменная пути к файлу
    char filepath[sizeof(APP_PATH_FOLDER) + sizeof(APP_FILENAME)] = {0};
    //Составление пути к файлу
    strcpy(filepath, APP_PATH_FOLDER);
    strcat(filepath, "/");
    strcat(filepath, APP_FILENAME);

    //Открытие потока. Если поток открылся, то выполнение сохранения датчиков
    if(file_stream_open(pd->file_stream, filepath, FSAM_READ_WRITE, FSOM_CREATE_ALWAYS)) {
        const char template[] =
            "#DHT monitor sensors files\n#Name - name of sensor. Up to 10 sumbols\n#Type - type of sensor. DHT11 - 0,DHT22 - 1\n#GPIO - connection port.May being 2, 3, 4, 5, 6, 7, 15, 16\n";
        stream_write(pd->file_stream, (uint8_t*)template, strlen(template));
        //Сохранение датчиков
        for(uint8_t i = 0; i < pd->sensors_count; i++) {
            stream_write_format(
                pd->file_stream,
                "%s %d %d\n",
                pd->sensors[i].name,
                pd->sensors[i].type,
                gpio_to_int(pd->sensors[i].DHT_Pin));
        }
    } else {
        //TODO: печать ошибки на экран
        FURI_LOG_E(APP_NAME, "cannot open of create sensors file\r\n");
    }
    stream_free(pd->file_stream);

    return 0;
}
/* 
Функция загрузки датчиков из файла
Возвращает истину, если был загружен хотя бы 1 датчик
*/
bool DHT_sensors_load(PluginData* const pd) {
    //Обнуление количества датчиков
    pd->sensors_count = 0;
    //Очистка предыдущих датчиков
    memset(pd->sensors, 0, sizeof(pd->sensors));

    //Тут будет загрузка и парсинг с SD-карты
    // Storage* storage = furi_record_open(RECORD_STORAGE);
    // storage_common_mkdir(storage, "/ext/DHT_monitor/sensors");
    // Stream* stream = file_stream_alloc(storage);

    // if(!file_stream_open(stream, APP_PATH_FOLDER, FSAM_READ, FSOM_OPEN_ALWAYS)) {
    //     FURI_LOG_D(APP_NAME, "Cannot open or create file \"%s\"", APP_PATH_FOLDER);
    // } else {
    // }
    //Типа получил какой-то датчик, сохранение
    DHT_sensor s = {.name = "Room", .DHT_Pin = gpio_ext_pa7, .type = DHT11};
    pd->sensors[0] = s;
    pd->sensors_count++;

    //Инициализация портов датчиков
    if(pd->sensors_count != 0) {
        DHT_sensors_init(pd);
        return true;
    } else {
        return false;
    }
}

static void render_callback(Canvas* const canvas, void* ctx) {
    PluginData* plugin_data = acquire_mutex((ValueMutex*)ctx, 25);
    if(plugin_data == NULL) {
        return;
    }

    if(!furi_hal_power_is_otg_enabled()) {
        furi_hal_power_enable_otg();
    }
    plugin_data->data = DHT_getData(&plugin_data->sensors[0]);

    // border around the edge of the screen
    canvas_draw_frame(canvas, 0, 0, 128, 64);

    canvas_set_font(canvas, FontPrimary);
    snprintf(
        plugin_data->txtbuff,
        sizeof(plugin_data->txtbuff),
        "%s: %dC/%d%%",
        plugin_data->sensors[0].name,
        (int8_t)plugin_data->data.temp,
        (int8_t)plugin_data->data.hum);
    canvas_draw_str(canvas, 2, 10, plugin_data->txtbuff);

    release_mutex((ValueMutex*)ctx, plugin_data);
}

static void input_callback(InputEvent* input_event, FuriMessageQueue* event_queue) {
    furi_assert(event_queue);

    PluginEvent event = {.type = EventTypeKey, .input = *input_event};
    furi_message_queue_put(event_queue, &event, FuriWaitForever);
}

int32_t quenon_dht_mon_app() {
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(PluginEvent));
    PluginData* plugin_data = malloc(sizeof(PluginData));

    //Сохранение состояния наличия 5V на порту 1 FZ
    plugin_data->last_OTG_State = furi_hal_power_is_otg_enabled();

    DHT_sensors_load(plugin_data);

    ValueMutex state_mutex;
    if(!init_mutex(&state_mutex, plugin_data, sizeof(PluginData))) {
        FURI_LOG_E(APP_NAME, "cannot create mutex\r\n");
        free(plugin_data);
        return 255;
    }

    // Set system callbacks
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, render_callback, &state_mutex);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    // Open GUI and register view_port
    Gui* gui = furi_record_open("gui");
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    plugin_data->storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(plugin_data->storage, APP_PATH_FOLDER);
    plugin_data->file_stream = file_stream_alloc(plugin_data->storage);

    PluginEvent event;
    for(bool processing = true; processing;) {
        FuriStatus event_status = furi_message_queue_get(event_queue, &event, 100);

        PluginData* plugin_data = (PluginData*)acquire_mutex_block(&state_mutex);

        if(event_status == FuriStatusOk) {
            // press events
            if(event.type == EventTypeKey) {
                if(event.input.type == InputTypePress) {
                    switch(event.input.key) {
                    case InputKeyUp:
                        //  plugin_data->y--;
                        break;
                    case InputKeyDown:
                        //  plugin_data->y++;
                        break;
                    case InputKeyRight:
                        //   plugin_data->x++;
                        break;
                    case InputKeyLeft:
                        //  plugin_data->x--;
                        break;
                    case InputKeyOk:
                        break;
                    case InputKeyBack:
                        processing = false;
                        break;
                    }
                }
            }
        } else {
            FURI_LOG_D(APP_NAME, "FuriMessageQueue: event timeout");
            // event timeout
        }

        view_port_update(view_port);
        release_mutex(&state_mutex, plugin_data);
    }

    DHT_sensors_save(plugin_data);
    DHT_sensors_deinit(plugin_data);

    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close("gui");
    furi_record_close(RECORD_STORAGE);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    delete_mutex(&state_mutex);

    return 0;
}
//TODO: Освободить порты ввода/вывода перед закрытием приложения
//TODO: Загрузка и сохранение датчиков на SD-карту
//TODO: Добавление датчика из меню