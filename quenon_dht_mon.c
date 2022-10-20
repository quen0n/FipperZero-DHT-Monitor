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
GpioPin SWC_10 = {.pin = LL_GPIO_PIN_14, .port = GPIOA};
GpioPin SIO_12 = {.pin = LL_GPIO_PIN_13, .port = GPIOA};
GpioPin TX_13 = {.pin = LL_GPIO_PIN_6, .port = GPIOB};
GpioPin RX_14 = {.pin = LL_GPIO_PIN_7, .port = GPIOB};
#define GPIO_ITEMS (sizeof(gpio_item) / sizeof(GpioItem))
static const GpioItem gpio_item[] = {
    {2, &gpio_ext_pa7},
    {3, &gpio_ext_pa6},
    {4, &gpio_ext_pa4},
    {5, &gpio_ext_pb3},
    {6, &gpio_ext_pb2},
    {7, &gpio_ext_pc3},
    {10, &SWC_10},
    {12, &SIO_12},
    {13, &TX_13},
    {14, &RX_14},
    {15, &gpio_ext_pc1},
    {16, &gpio_ext_pc0},
    {17, &ibutton_gpio}};

//Структура с данными плагина
typedef struct {
    char txtbuff[30]; //Буффер для печати строк на экране
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
static uint8_t gpio_to_int(GpioPin* gp) {
    for(uint8_t i = 0; i < GPIO_ITEMS; i++) {
        if(gpio_item[i].pin->pin == gp->pin && gpio_item[i].pin->port == gp->port) {
            return gpio_item[i].name;
        }
    }
    return 255;
}

/* Функция конвертации порта FZ в порт GPIO 
Принимает номер порта на корпусе FZ
Возвращает порт GPIO 
*/
const GpioPin* int_to_gpio(uint8_t name) {
    for(uint8_t i = 0; i < GPIO_ITEMS; i++) {
        if(gpio_item[i].name == name) {
            return gpio_item[i].pin;
        }
    }
    return NULL;
}

/* 
Функция инициализации портов ввода/вывода датчиков
*/
void DHT_sensors_init(PluginData* pd) {
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
void DHT_sensors_deinit(PluginData* pd) {
    //Возврат исходного состояния 5V
    if(pd->last_OTG_State != true) {
        furi_hal_power_disable_otg();
    }

    //Настройка портов на вход
    for(uint8_t i = 0; i < pd->sensors_count; i++) {
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
uint8_t DHT_sensors_save(PluginData* pd) {
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
            "#DHT monitor sensors file\n#Name - name of sensor. Up to 10 sumbols\n#Type - type of sensor. DHT11 - 0, DHT22 - 1\n#GPIO - connection port. May being 2, 3, 4, 5, 6, 7, 10, 12, 13, 14, 15, 16, 17\n#Name Type GPIO\n";
        stream_write(pd->file_stream, (uint8_t*)template, strlen(template));
        //Сохранение датчиков
        for(uint8_t i = 0; i < pd->sensors_count; i++) {
            stream_write_format(
                pd->file_stream,
                "%s %d %d\n",
                pd->sensors[i].name,
                pd->sensors[i].type,
                gpio_to_int(&pd->sensors[i].DHT_Pin));
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
bool DHT_sensors_load(PluginData* pd) {
    //Обнуление количества датчиков
    pd->sensors_count = 0;
    //Очистка предыдущих датчиков
    memset(pd->sensors, 0, sizeof(pd->sensors));

    //Открытие файла на SD-карте
    //Выделение памяти для потока
    pd->file_stream = file_stream_alloc(pd->storage);
    //Переменная пути к файлу
    char filepath[sizeof(APP_PATH_FOLDER) + sizeof(APP_FILENAME)] = {0};
    //Составление пути к файлу
    strcpy(filepath, APP_PATH_FOLDER);
    strcat(filepath, "/");
    strcat(filepath, APP_FILENAME);
    //Открытие потока к файлу
    if(!file_stream_open(pd->file_stream, filepath, FSAM_READ_WRITE, FSOM_OPEN_ALWAYS)) {
        //Если файл отсутствует, то выход
        FURI_LOG_E(APP_NAME, "Missing sensors file\r\n");
        return false;
    }
    //Вычисление размера файла
    size_t file_size = stream_size(pd->file_stream);
    if(file_size == (size_t)0) {
        //Выход если файл пустой
        FURI_LOG_E(APP_NAME, "Sensors file is empty\r\n");
        return false;
    }

    //Выделение памяти под загрузки файла
    uint8_t* file_buf = malloc(file_size);
    //Опустошение буфера файла
    memset(file_buf, 0, file_size);
    //Загрузка файла
    if(stream_read(pd->file_stream, file_buf, file_size) != file_size) {
        //Выход при ошибке чтения
        FURI_LOG_E(APP_NAME, "Error reading sensor file\r\n");
        return false;
    }
    //Построчное чтение файла
    char* line = strtok((char*)file_buf, "\n");
    while(line != NULL) {
        if(line[0] != '#') {
            DHT_sensor s = {0};
            int type, port;
            sscanf(line, "%s %d %d", s.name, &type, &port);
            //Проверка правильности
            if((type == DHT11 || type == DHT22) && (int_to_gpio(port) != NULL)) {
                s.type = type;
                s.DHT_Pin = *int_to_gpio(port);
                pd->sensors[pd->sensors_count] = s;
                pd->sensors_count++;
            }
        }
        line = strtok((char*)NULL, "\n");
    }
    stream_free(pd->file_stream);

    //Инициализация портов датчиков
    if(pd->sensors_count != 0) {
        DHT_sensors_init(pd);
        return true;
    } else {
        return false;
    }
    return false;
}

static void render_callback(Canvas* const canvas, void* ctx) {
    PluginData* plugin_data = acquire_mutex((ValueMutex*)ctx, 25);
    if(plugin_data == NULL) {
        return;
    }
    // border around the edge of the screen
    canvas_draw_frame(canvas, 0, 0, 128, 64);
    canvas_set_font(canvas, FontPrimary);

    if(plugin_data->sensors_count > 0) {
        if(!furi_hal_power_is_otg_enabled()) {
            furi_hal_power_enable_otg();
        }
        for(uint8_t i = 0; i < plugin_data->sensors_count; i++) {
            plugin_data->data = DHT_getData(&plugin_data->sensors[i]);
            char name[11] = {0};
            memcpy(name, plugin_data->sensors[i].name, 10);

            if(plugin_data->data.hum == -128.0f && plugin_data->data.temp == -128.0f) {
                snprintf(plugin_data->txtbuff, sizeof(plugin_data->txtbuff), "%s: timeout", name);
            } else {
                snprintf(
                    plugin_data->txtbuff,
                    sizeof(plugin_data->txtbuff),
                    "%s: %2.1f*C / %d%%",
                    name,
                    (double)plugin_data->data.temp,
                    (int8_t)plugin_data->data.hum);
            }

            canvas_draw_str(canvas, 2, 10 + 10 * i, plugin_data->txtbuff);
        }

    } else {
        canvas_draw_str(canvas, 2, 10, "Sensors not found");
    }

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

    //Подготовка хранилища
    plugin_data->storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(plugin_data->storage, APP_PATH_FOLDER);
    plugin_data->file_stream = file_stream_alloc(plugin_data->storage);

    //Сохранение состояния наличия 5V на порту 1 FZ
    plugin_data->last_OTG_State = furi_hal_power_is_otg_enabled();

    //Загрузка датчиков с SD-карты
    DHT_sensors_load(plugin_data);

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

    //Сохранение датчиков на SD-карту перед выходом
    //сохранение по необходимости DHT_sensors_save(plugin_data);

    //Деинициализация портов датчиков
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
//TODO: Добавление датчика из меню
