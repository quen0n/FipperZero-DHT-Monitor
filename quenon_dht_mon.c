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

/**
 * @brief Функция конвертации GPIO в его номер FZ
 * 
 * @param gp Указатель на преобразовываемый GPIO
 * @return Номер порта на корпусе FZ
 */
uint8_t DHT_GPIO_to_int(const GpioPin* gp) {
    if(gp == NULL) return 255;
    for(uint8_t i = 0; i < GPIO_ITEMS; i++) {
        if(gpio_item[i].pin->pin == gp->pin && gpio_item[i].pin->port == gp->port) {
            return gpio_item[i].name;
        }
    }
    return 255;
}

/**
 * @brief Функция конвертации порта FZ в GPIO 
 * 
 * @param name Номер порта на корпусе FZ
 * @return GPIO при успехе, NULL при ошибке
 */
const GpioPin* DHT_GPIO_form_int(uint8_t name) {
    for(uint8_t i = 0; i < GPIO_ITEMS; i++) {
        if(gpio_item[i].name == name) {
            return gpio_item[i].pin;
        }
    }
    return NULL;
}

/**
 * @brief Преобразование порядкового номера свободного порта в GPIO
 * 
 * @param index Индекс порта от 0 до GPIO_ITEMS-1
 * @return GPIO при успехе, NULL при ошибке
 */
const GpioPin* index_to_gpio(uint8_t index) {
    if(index > GPIO_ITEMS) return NULL;
    return gpio_item[index].pin;
}

/**
 * @brief Инициализация портов ввода/вывода датчиков
 * 
 */
void DHT_sensors_init(void) {
    //Включение 5V если на порту 1 FZ его нет
    if(furi_hal_power_is_otg_enabled() != true) {
        furi_hal_power_enable_otg();
    }

    //Настройка GPIO загруженных датчиков
    for(uint8_t i = 0; i < app->sensors_count; i++) {
        //Высокий уровень по умолчанию
        furi_hal_gpio_write(app->sensors[i].GPIO, true);
        //Режим работы - OpenDrain, подтяжка включается на всякий случай
        furi_hal_gpio_init(
            app->sensors[i].GPIO, //Порт FZ
            GpioModeOutputOpenDrain, //Режим работы - открытый сток
            GpioPullUp, //Принудительная подтяжка линии данных к питанию
            GpioSpeedVeryHigh); //Скорость работы - максимальная
    }
}

/**
 * @brief Функция деинициализации портов ввода/вывода датчиков
 * 
 */
void DHT_sensors_deinit(void) {
    //Возврат исходного состояния 5V
    if(app->last_OTG_State != true) {
        furi_hal_power_disable_otg();
    }

    //Перевод портов GPIO в состояние по умолчанию
    for(uint8_t i = 0; i < app->sensors_count; i++) {
        furi_hal_gpio_init(
            app->sensors[i].GPIO, //Порт FZ
            GpioModeAnalog, //Режим работы - аналог
            GpioPullNo, //Отключение подтяжки
            GpioSpeedLow); //Скорость работы - низкая
        //Установка низкого уровня
        furi_hal_gpio_write(app->sensors[i].GPIO, false);
    }
}

/**
 * @brief Проверка корректности параметров датчика
 * 
 * @param sensor Указатель на проверяемый датчик
 * @return true Параметры датчика корректные
 * @return false Параметры датчика некорректные
 */
bool DHT_sensor_check(DHT_sensor* sensor) {
    //Проверка имени
    if(strlen(sensor->name) == 0 && strlen(sensor->name) > 10 && sensor->name[0] == ' ') {
        FURI_LOG_D(APP_NAME, "Sensor [%s] name check failed\r\n", sensor->name);
        return false;
    }

    //Проверка GPIO
    if(DHT_GPIO_to_int(sensor->GPIO) == 255) {
        FURI_LOG_D(
            APP_NAME,
            "Sensor [%s] GPIO check failed: %d\r\n",
            sensor->name,
            DHT_GPIO_to_int(sensor->GPIO));
        return false;
    }
    //Проверка типа датчика
    if(sensor->type != DHT11 && sensor->type != DHT22) {
        FURI_LOG_D(APP_NAME, "Sensor [%s] type check failed: %d\r\n", sensor->name, sensor->type);
        return false;
    }

    //Возврат истины если всё ок
    FURI_LOG_D(APP_NAME, "Sensor [%s] all checks passed\r\n", sensor->name);
    return true;
}

void DHT_sensor_delete(DHT_sensor* sensor) {
    //Делаем параметры датчика неверными
    sensor->name[0] = '\0';
    sensor->type = 255;
    //Теперь сохраняем текущие датчики. Сохранятор не сохранит неисправный датчик
    DHT_sensors_save();
    //Перезагружаемся с SD-карты
    DHT_sensors_reload();
}

/**
 * @brief Сохранение датчиков на SD-карту
 * 
 * @return Количество сохранённых датчиков
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
            //Если параметры датчика верны, то сохраняемся
            if(DHT_sensor_check(&app->sensors[i])) {
                stream_write_format(
                    app->file_stream,
                    "%s %d %d\n",
                    app->sensors[i].name,
                    app->sensors[i].type,
                    DHT_GPIO_to_int(app->sensors[i].GPIO));
            }
        }
    } else {
        //TODO: печать ошибки на экран
        FURI_LOG_E(APP_NAME, "cannot open or create sensors file\r\n");
    }
    stream_free(app->file_stream);

    //Всегда возвращает 0, над исправить
    return 0;
}

/**
 * @brief Загрузка датчиков с SD-карты
 * 
 * @return true Был загружен хотя бы 1 датчик
 * @return false Датчики отсутствуют
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
            s.type = type;
            s.GPIO = DHT_GPIO_form_int(port);

            name[10] = '\0';
            strcpy(s.name, name);
            //Если данные корректны, то
            if(DHT_sensor_check(&s) == true) {
                //Установка нуля при первом датчике
                if(app->sensors_count == -1) app->sensors_count = 0;
                //Добавление датчика в общий список
                app->sensors[app->sensors_count] = s;
                //Увеличение количества загруженных датчиков
                app->sensors_count++;
            }
        }
        line = strtok((char*)NULL, "\n");
    }
    stream_free(app->file_stream);
    free(file_buf);

    //Обнуление количества датчиков если ни один из них не был загружен
    if(app->sensors_count == -1) app->sensors_count = 0;

    //Инициализация портов датчиков если таковые есть
    if(app->sensors_count > 0) {
        DHT_sensors_init();
        return true;
    } else {
        return false;
    }
    return false;
}

/**
 * @brief Перезагрузка датчиков с SD-карты
 * 
 * @return true Когда был загружен хотя бы 1 датчик
 * @return false Ни один из датчиков не был загружен
 */
bool DHT_sensors_reload(void) {
    DHT_sensors_deinit();
    return DHT_sensors_load();
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

/**
 * @brief Выделение места под переменные плагина
 * 
 * @return true Если всё прошло успешно
 * @return false Если в процессе загрузки произошла ошибка
 */
static bool quenon_dht_mon_init(void) {
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

    app->variable_item_list = variable_item_list_alloc();
    app->view_dispatcher = view_dispatcher_alloc();

    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, WIDGET_VIEW, widget_get_view(app->widget));

    //Уведомления
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    //Подготовка хранилища
    app->storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(app->storage, APP_PATH_FOLDER);
    app->file_stream = file_stream_alloc(app->storage);

    return true;
}

/**
 * @brief Освыбождение памяти после работы приложения
 * 
 */

static void quenon_dht_mon_free(void) {
    //Автоматическое управление подсветкой
    notification_message(app->notifications, &sequence_display_backlight_enforce_auto);

    view_port_enabled_set(app->view_port, false);
    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close("gui");
    furi_record_close(RECORD_STORAGE);
    view_port_free(app->view_port);
    furi_message_queue_free(app->event_queue);
    delete_mutex(&app->state_mutex);
    widget_free(app->widget);
}

/**
 * @brief Точка входа в приложение
 * 
 * @return Код ошибки
 */
int32_t quenon_dht_mon_app() {
    if(!quenon_dht_mon_init()) {
        quenon_dht_mon_free();
        return 255;
    }
    //Постоянное свечение подсветки
    notification_message(app->notifications, &sequence_display_backlight_enforce_on);
    //Сохранение состояния наличия 5V на порту 1 FZ
    app->last_OTG_State = furi_hal_power_is_otg_enabled();

    //Загрузка датчиков с SD-карты
    DHT_sensors_load();

    app->currentSensorEdit = &app->sensors[0];

    PluginEvent event;
    for(bool processing = true; processing;) {
        FuriStatus event_status = furi_message_queue_get(app->event_queue, &event, 100);

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
                        view_port_update(app->view_port);
                        release_mutex(&app->state_mutex, app);
                        sensorActions_scene(app);
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
    DHT_sensors_deinit();
    quenon_dht_mon_free();

    return 0;
}
//TODO: Удаление датчиков из меню
//TODO: Обработка ошибок
//TODO: Пропуск использованных портов в меню добавления датчиков
//TODO: Прокрутка датчиков в основном окне
