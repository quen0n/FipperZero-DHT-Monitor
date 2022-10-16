#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <stdio.h>
#include "furi_hal_power.h"

#include "DHT.h"

#define APP_NAME "DHT monitor"

typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} PluginEvent;

//Структура с данными плагина
typedef struct {
    char txtbuff[25]; //Буффер для печати строк на экране
    bool last_OTG_State; //Состояние OTG до запуска приложения
    uint8_t sensors_count; // Количество загруженных датчиков
    DHT_sensor sensors[8]; //Сохранённые датчики
    DHT_data data; //Инфа из датчика
} PluginData;

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
uint8_t DHT_sensors_save(void) {
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

    //Типа получил какой-то датчик, сохранение
    DHT_sensor s = {.DHT_Pin = gpio_ext_pa7, .type = DHT11};
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
        "Temp: %dC Hum: %d%%",
        (int8_t)plugin_data->data.temp,
        (int8_t)plugin_data->data.hum);
    canvas_draw_str(canvas, 20, 35, plugin_data->txtbuff);

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

    DHT_sensors_save();
    DHT_sensors_deinit(plugin_data);

    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close("gui");
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    delete_mutex(&state_mutex);

    return 0;
}
//TODO: Освободить порты ввода/вывода перед закрытием приложения
//TODO: Загрузка и сохранение датчиков на SD-карту
//TODO: Добавление датчика из меню
