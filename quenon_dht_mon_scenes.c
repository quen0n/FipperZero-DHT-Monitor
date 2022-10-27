#include "quenon_dht_mon.h"

DHT_sensor tempSensor = {0};

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

enum { MAIN_MENU, ADDSENSOR_MENU, TEXTINPUT } MENU_VIEWS;

void scene_main(Canvas* const canvas, PluginData* app) {
    UNUSED(app);
    //Рисование бара
    canvas_draw_box(canvas, 0, 0, 128, 14);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 32, 11, "DHT Monitor");

    // border around the edge of the screen
    canvas_set_color(canvas, ColorBlack);
    if(app->sensors_count > 0) {
        if(!furi_hal_power_is_otg_enabled()) {
            furi_hal_power_enable_otg();
        }
        for(uint8_t i = 0; i < app->sensors_count; i++) {
            app->data = DHT_getData(&app->sensors[i]);

            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 0, 24 + 10 * i, app->sensors[i].name);

            canvas_set_font(canvas, FontSecondary);
            if(app->data.hum == -128.0f && app->data.temp == -128.0f) {
                canvas_draw_str(canvas, 96, 24 + 10 * i, "timeout");
            } else {
                snprintf(
                    app->txtbuff,
                    sizeof(app->txtbuff),
                    "%2.1f*C/%d%%",
                    (double)app->data.temp,
                    (int8_t)app->data.hum);
                canvas_draw_str(canvas, 64, 24 + 10 * i, app->txtbuff);
            }
        }
    } else {
        canvas_set_font(canvas, FontSecondary);
        if(app->sensors_count == 0) canvas_draw_str(canvas, 0, 24, "Sensors not found");
        if(app->sensors_count == -1) canvas_draw_str(canvas, 0, 24, "Loading...");
    }
}

static uint32_t mainMenu_exit(void* context) {
    DHT_sensors_reload();
    UNUSED(context);
    return VIEW_NONE;
}

// void scene_mainMenu(PluginData* app) {
//     variable_item_list_reset(app->variable_item_list);
//     for(uint8_t i = 0; i < app->sensors_count; i++) {
//         variable_item_list_add(app->variable_item_list, app->sensors[i].name, 1, NULL, NULL);
//     }
//     variable_item_list_add(app->variable_item_list, "+ Add new sensor +", 1, NULL, NULL);

//     app->view_dispatcher = view_dispatcher_alloc();
//     view_dispatcher_enable_queue(app->view_dispatcher);
//     view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
//     view_dispatcher_add_view(app->view_dispatcher, MAIN_MENU, app->view);
//     view_dispatcher_switch_to_view(app->view_dispatcher, MAIN_MENU);

//     view_set_previous_callback(app->view, mainMenu_exit);
// }

static void sensorTypeChanged(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, sensorsTypes[index]);
    tempSensor.type = index;
}
static void GPIOChanged(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, GPIOs[index]);
    tempSensor.DHT_Pin = *index_to_gpio(index);
}

VariableItem* nameItem;

void sensorNameChanged(void* context) {
    PluginData* app = context;
    variable_item_set_current_value_text(nameItem, tempSensor.name);
    view_dispatcher_switch_to_view(app->view_dispatcher, ADDSENSOR_MENU);
}
static void sensorNameChange(PluginData* app) {
    text_input_set_header_text(app->text_input, "Sensor name");
    text_input_set_result_callback(
        app->text_input, sensorNameChanged, app, tempSensor.name, 10, true);
    view_dispatcher_switch_to_view(app->view_dispatcher, TEXTINPUT);
}

static void sensorSave(void* context, uint32_t index) {
    PluginData* app = context;
    if(index == 0) {
        sensorNameChange(app);
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

void scene_addSensorMenu(PluginData* app) {
    variable_item_list_reset(app->variable_item_list);

    uint8_t value_index = 0;

    strcpy(tempSensor.name, "NewSensor");

    nameItem = variable_item_list_add(app->variable_item_list, "Name: ", 1, NULL, NULL);
    variable_item_set_current_value_index(nameItem, value_index);
    variable_item_set_current_value_text(nameItem, tempSensor.name);

    app->item =
        variable_item_list_add(app->variable_item_list, "Type:", 2, sensorTypeChanged, app);
    value_index = 0;
    tempSensor.DHT_Pin = *index_to_gpio(value_index);
    variable_item_set_current_value_index(app->item, value_index);
    variable_item_set_current_value_text(app->item, sensorsTypes[value_index]);

    app->item = variable_item_list_add(app->variable_item_list, "GPIO:", 13, GPIOChanged, app);
    value_index = 0;
    variable_item_set_current_value_index(app->item, value_index);
    variable_item_set_current_value_text(app->item, GPIOs[value_index]);

    variable_item_list_add(app->variable_item_list, "Save", 1, NULL, app);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    view_dispatcher_add_view(app->view_dispatcher, ADDSENSOR_MENU, app->view);

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TEXTINPUT, text_input_get_view(app->text_input));

    view_dispatcher_switch_to_view(app->view_dispatcher, ADDSENSOR_MENU);
    variable_item_list_set_enter_callback(app->variable_item_list, sensorSave, app);
    view_set_previous_callback(app->view, mainMenu_exit);
}