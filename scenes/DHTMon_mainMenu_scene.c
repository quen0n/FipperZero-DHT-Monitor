#include "../quenon_dht_mon.h"

static VariableItemList* variable_item_list;
/* ============== Главное меню ============== */
static uint32_t mainMenu_exitCallback(void* context) {
    UNUSED(context);
    variable_item_list_free(variable_item_list);
    DHT_sensors_reload();
    return VIEW_NONE;
}
static void mainMenu_enterCallback(void* context, uint32_t index) {
    PluginData* app = context;
    if((uint8_t)index == (uint8_t)app->sensors_count) {
        addSensor_scene(app);
        view_dispatcher_run(app->view_dispatcher);
    }
}
void mainMenu_scene(PluginData* app) {
    variable_item_list = variable_item_list_alloc();
    variable_item_list_reset(variable_item_list);
    for(uint8_t i = 0; i < app->sensors_count; i++) {
        variable_item_list_add(variable_item_list, app->sensors[i].name, 1, NULL, NULL);
    }
    variable_item_list_add(variable_item_list, "+ Add new sensor +", 1, NULL, NULL);

    app->view = variable_item_list_get_view(variable_item_list);
    app->view_dispatcher = view_dispatcher_alloc();

    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_add_view(app->view_dispatcher, MAIN_MENU_VIEW, app->view);
    view_dispatcher_switch_to_view(app->view_dispatcher, MAIN_MENU_VIEW);

    variable_item_list_set_enter_callback(variable_item_list, mainMenu_enterCallback, app);
    view_set_previous_callback(app->view, mainMenu_exitCallback);
}