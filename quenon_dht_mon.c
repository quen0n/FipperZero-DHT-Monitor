#include "quenon_dht_mon.h"

//Порты ввода/вывода, которые не были обозначены в общем списке
const GpioPin SWC_10 = {.pin = LL_GPIO_PIN_14, .port = GPIOA};
const GpioPin SIO_12 = {.pin = LL_GPIO_PIN_13, .port = GPIOA};
const GpioPin TX_13 = {.pin = LL_GPIO_PIN_6, .port = GPIOB};
const GpioPin RX_14 = {.pin = LL_GPIO_PIN_7, .port = GPIOB};

//Количество доступных портов ввода/вывода
#define GPIO_ITEMS (sizeof(gpio_item) / sizeof(GpioItem))

//Перечень достуных портов ввода/вывода
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

//Данные плагина
static PluginData* app;

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
void DHT_sensors_init(void) {
    //Включение 5V если на порту 1 FZ его нет
    if(furi_hal_power_is_otg_enabled() != true) {
        furi_hal_power_enable_otg();
    }

    //Настройка GPIO загруженных датчиков
    for(uint8_t i = 0; i < app->sensors_count; i++) {
        //Высокий уровень по умолчанию
        furi_hal_gpio_write(&app->sensors[i].DHT_Pin, true);
        //Режим работы - OpenDrain, подтяжка включается на всякий случай
        furi_hal_gpio_init(
            &app->sensors[i].DHT_Pin, //Порт FZ
            GpioModeOutputOpenDrain, //Режим работы - открытый сток
            GpioPullUp, //Принудительная подтяжка линии данных к питанию
            GpioSpeedVeryHigh); //Скорость работы - максимальная
    }
}

/* 
Функция деинициализации портов ввода/вывода датчиков
*/
void DHT_sensors_deinit(void) {
    //Возврат исходного состояния 5V
    if(app->last_OTG_State != true) {
        furi_hal_power_disable_otg();
    }

    //Настройка портов на вход
    for(uint8_t i = 0; i < app->sensors_count; i++) {
        furi_hal_gpio_init(
            &app->sensors[i].DHT_Pin, //Порт FZ
            GpioModeInput, //Режим работы - вход
            GpioPullNo, //Отключение подтяжки
            GpioSpeedLow); //Скорость работы - низкая
        //Установка низкого уровня
        furi_hal_gpio_write(&app->sensors[i].DHT_Pin, false);
    }
}

/* 
Функция сохранения датчиков в файл
Возвращает количество сохранённых датчиков
*/
uint8_t DHT_sensors_save(void) {
    //Выделение памяти для потока
    app->file_stream = file_stream_alloc(app->storage);
    //Переменная пути к файлу
    char filepath[sizeof(APP_PATH_FOLDER) + sizeof(APP_FILENAME)] = {0};
    //Составление пути к файлу
    strcpy(filepath, APP_PATH_FOLDER);
    strcat(filepath, "/");
    strcat(filepath, APP_FILENAME);

    //Открытие потока. Если поток открылся, то выполнение сохранения датчиков
    if(file_stream_open(app->file_stream, filepath, FSAM_READ_WRITE, FSOM_CREATE_ALWAYS)) {
        const char template[] =
            "#DHT monitor sensors file\n#Name - name of sensor. Up to 10 sumbols\n#Type - type of sensor. DHT11 - 0, DHT22 - 1\n#GPIO - connection port. May being 2-7, 10, 12-17\n#Name Type GPIO\n";
        stream_write(app->file_stream, (uint8_t*)template, strlen(template));
        //Сохранение датчиков
        for(uint8_t i = 0; i < app->sensors_count; i++) {
            stream_write_format(
                app->file_stream,
                "%s %d %d\n",
                app->sensors[i].name,
                app->sensors[i].type,
                gpio_to_int(&app->sensors[i].DHT_Pin));
        }
    } else {
        //TODO: печать ошибки на экран
        FURI_LOG_E(APP_NAME, "cannot open or create sensors file\r\n");
    }
    stream_free(app->file_stream);

    return 0;
}
/* 
Функция загрузки датчиков из файла
Возвращает истину, если был загружен хотя бы 1 датчик
*/
bool DHT_sensors_load(void) {
    //Обнуление количества датчиков
    app->sensors_count = -1;
    //Очистка предыдущих датчиков
    memset(app->sensors, 0, sizeof(app->sensors));

    //Открытие файла на SD-карте
    //Выделение памяти для потока
    app->file_stream = file_stream_alloc(app->storage);
    //Переменная пути к файлу
    char filepath[sizeof(APP_PATH_FOLDER) + sizeof(APP_FILENAME)] = {0};
    //Составление пути к файлу
    strcpy(filepath, APP_PATH_FOLDER);
    strcat(filepath, "/");
    strcat(filepath, APP_FILENAME);
    //Открытие потока к файлу
    if(!file_stream_open(app->file_stream, filepath, FSAM_READ_WRITE, FSOM_OPEN_EXISTING)) {
        //Если файл отсутствует, то создание болванки
        FURI_LOG_W(APP_NAME, "Missing sensors file. Creating new file\r\n");
        app->sensors_count = 0;
        stream_free(app->file_stream);
        DHT_sensors_save();
        return false;
    }
    //Вычисление размера файла
    size_t file_size = stream_size(app->file_stream);
    if(file_size == (size_t)0) {
        //Выход если файл пустой
        FURI_LOG_W(APP_NAME, "Sensors file is empty\r\n");
        app->sensors_count = 0;
        stream_free(app->file_stream);
        return false;
    }

    //Выделение памяти под загрузку файла
    uint8_t* file_buf = malloc(file_size);
    //Опустошение буфера файла
    memset(file_buf, 0, file_size);
    //Загрузка файла
    if(stream_read(app->file_stream, file_buf, file_size) != file_size) {
        //Выход при ошибке чтения
        FURI_LOG_E(APP_NAME, "Error reading sensor file\r\n");
        app->sensors_count = 0;
        stream_free(app->file_stream);
        return false;
    }
    //Построчное чтение файла
    char* line = strtok((char*)file_buf, "\n");
    while(line != NULL) {
        if(line[0] != '#') {
            DHT_sensor s = {0};
            int type, port;
            char name[11];
            sscanf(line, "%s %d %d", name, &type, &port);
            //Проверка правильности
            if((type == DHT11 || type == DHT22) && (int_to_gpio(port) != NULL)) {
                if(app->sensors_count == -1) app->sensors_count = 0;
                s.type = type;
                s.DHT_Pin = *int_to_gpio(port);
                name[10] = '\0';
                strcpy(s.name, name);
                app->sensors[app->sensors_count] = s;
                app->sensors_count++;
            }
        }
        line = strtok((char*)NULL, "\n");
    }
    stream_free(app->file_stream);

    if(app->sensors_count == -1) app->sensors_count = 0;

    //Инициализация портов датчиков
    if(app->sensors_count > 0) {
        DHT_sensors_init();
        return true;
    } else {
        return false;
    }
    return false;
}

static void render_callback(Canvas* const canvas, void* ctx) {
    PluginData* app = acquire_mutex((ValueMutex*)ctx, 25);
    if(app == NULL) {
        return;
    }

    scene_main(canvas, app);

    release_mutex((ValueMutex*)ctx, app);
}

static void input_callback(InputEvent* input_event, FuriMessageQueue* event_queue) {
    furi_assert(event_queue);

    PluginEvent event = {.type = EventTypeKey, .input = *input_event};
    furi_message_queue_put(event_queue, &event, FuriWaitForever);
}

bool quenon_dht_mon_init(void) {
    //Выделение места под данные плагина
    app = malloc(sizeof(PluginData));
    //Выделение места под очередь событий
    app->event_queue = furi_message_queue_alloc(8, sizeof(PluginEvent));

    //Обнуление количества датчиков
    app->sensors_count = -1;

    //Инициализация мутекса
    if(!init_mutex(&app->state_mutex, app, sizeof(PluginData))) {
        FURI_LOG_E(APP_NAME, "cannot create mutex\r\n");
        return false;
    }

    // Set system callbacks
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, render_callback, &app->state_mutex);
    view_port_input_callback_set(app->view_port, input_callback, app->event_queue);

    // Open GUI and register view_port
    app->gui = furi_record_open("gui");
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    //Уведомления
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    //Подготовка хранилища
    app->storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(app->storage, APP_PATH_FOLDER);
    app->file_stream = file_stream_alloc(app->storage);

    return true;
}

void quenon_dht_mon_free(void) {
    //Деинициализация портов датчиков
    DHT_sensors_deinit();

    //Автоматическое управление подсветкой
    notification_message(app->notifications, &sequence_display_backlight_enforce_auto);

    view_port_enabled_set(app->view_port, false);
    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close("gui");
    furi_record_close(RECORD_STORAGE);
    view_port_free(app->view_port);
    furi_message_queue_free(app->event_queue);
    delete_mutex(&app->state_mutex);
}

int32_t quenon_dht_mon_app() {
    if(!quenon_dht_mon_init()) {
        quenon_dht_mon_free();
        return 255;
    }
    //Постоянное свечение подсветки
    notification_message(app->notifications, &sequence_display_backlight_enforce_on);
    furi_hal_gpio_init(
        &ibutton_gpio, //Порт FZ
        GpioModeOutputPushPull, //Режим работы - вход
        GpioPullNo, //Отключение подтяжки
        GpioSpeedLow); //Скорость работы - низкая
    furi_hal_gpio_write(&ibutton_gpio, true);
    //Сохранение состояния наличия 5V на порту 1 FZ
    app->last_OTG_State = furi_hal_power_is_otg_enabled();

    //Загрузка датчиков с SD-карты
    DHT_sensors_load();

    PluginEvent event;
    for(bool processing = true; processing;) {
        FuriStatus event_status = furi_message_queue_get(app->event_queue, &event, 100);

        //PluginData* app = (PluginData*)
        acquire_mutex_block(&app->state_mutex);

        if(event_status == FuriStatusOk) {
            // press events
            if(event.type == EventTypeKey) {
                if(event.input.type == InputTypePress) {
                    switch(event.input.key) {
                    case InputKeyUp:
                        break;
                    case InputKeyDown:
                        break;
                    case InputKeyRight:
                        break;
                    case InputKeyLeft:
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

        view_port_update(app->view_port);
        release_mutex(&app->state_mutex, app);
    }
    //Освобождение памяти и деинициализация
    quenon_dht_mon_free();

    return 0;
}
//TODO: Добавление датчика из меню
//TODO: Обработка ошибок
