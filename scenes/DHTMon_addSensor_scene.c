#include "../quenon_dht_mon.h"

static DHT_sensor tempSensor = {0};
static VariableItem* nameItem;
static VariableItemList* variable_item_list;

const char* const sensorsTypes[2] = {
    "DHT11",
    "DHT22",
};
const char* const GPIOs[13] = {
    "2",
    "3",
    "4",
    "5",
    "6",
    "7",
    "10",
    "12",
    "13",
    "14",
    "15",
    "16",
    "17",
};

// /* ============== Добавление датчика ============== */
static uint32_t addSensor_exitCallback(void* context) {
    UNUSED(context);
    variable_item_list_free(variable_item_list);

    return VIEW_NONE;
}

static void addSensor_sensorTypeChanged(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, sensorsTypes[index]);
    tempSensor.type = index;
}

static void addSensor_GPIOChanged(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, GPIOs[index]);
    tempSensor.DHT_Pin = *index_to_gpio(index);
}

static void addSensor_sensorNameChanged(void* context) {
    PluginData* app = context;
    variable_item_set_current_value_text(nameItem, tempSensor.name);
    view_dispatcher_switch_to_view(app->view_dispatcher, ADDSENSOR_MENU_VIEW);
}
static void addSensor_sensorNameChange(PluginData* app) {
    text_input_set_header_text(app->text_input, "Sensor name");
    text_input_set_result_callback(
        app->text_input, addSensor_sensorNameChanged, app, tempSensor.name, 10, true);
    view_dispatcher_switch_to_view(app->view_dispatcher, TEXTINPUT_VIEW);
}

static void addSensor_enterCallback(void* context, uint32_t index) {
    PluginData* app = context;
    if(index == 0) {
        addSensor_sensorNameChange(app);
    }
    if(index == 3) {
        //Сохранение датчика
        //TODO: проверить правильность данных датчика
        app->sensors[app->sensors_count] = tempSensor;
        app->sensors_count++;
        DHT_sensors_save();
        view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_NONE);
    }
}

void addSensor_scene(PluginData* app) {
    uint8_t value_index = 0;
    variable_item_list = variable_item_list_alloc();

    strcpy(tempSensor.name, "NewSensor");

    nameItem = variable_item_list_add(variable_item_list, "Name: ", 1, NULL, NULL);
    variable_item_set_current_value_index(nameItem, value_index);
    variable_item_set_current_value_text(nameItem, tempSensor.name);

    app->item =
        variable_item_list_add(variable_item_list, "Type:", 2, addSensor_sensorTypeChanged, app);
    value_index = 0;
    tempSensor.DHT_Pin = *index_to_gpio(value_index);
    variable_item_set_current_value_index(app->item, value_index);
    variable_item_set_current_value_text(app->item, sensorsTypes[value_index]);

    app->item =
        variable_item_list_add(variable_item_list, "GPIO:", 13, addSensor_GPIOChanged, app);
    value_index = 0;
    variable_item_set_current_value_index(app->item, value_index);
    variable_item_set_current_value_text(app->item, GPIOs[value_index]);

    variable_item_list_add(variable_item_list, "Save", 1, NULL, app);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TEXTINPUT_VIEW, text_input_get_view(app->text_input));

    app->view = variable_item_list_get_view(variable_item_list);
    view_dispatcher_add_view(app->view_dispatcher, ADDSENSOR_MENU_VIEW, app->view);
    view_dispatcher_switch_to_view(app->view_dispatcher, ADDSENSOR_MENU_VIEW);

    variable_item_list_set_enter_callback(variable_item_list, addSensor_enterCallback, app);
    view_set_previous_callback(app->view, addSensor_exitCallback);
}