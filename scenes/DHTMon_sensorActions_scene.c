#include "../quenon_dht_mon.h"

//Текущий вид
static View* view;
//Список
static VariableItemList* variable_item_list;

/**
 * @brief Функция обработки нажатия средней кнопки
 * 
 * @param context Указатель на данные приложения
 * @param index На каком элементе списка была нажата кнопка
 */
static void enterCallback(void* context, uint32_t index) {
    PluginData* app = context;
    UNUSED(index);
    UNUSED(app);
}

/**
 * @brief Функция обработки нажатия кнопки "Назад"
 * 
 * @param context Указатель на данные приложения
 * @return ID вида в который нужно переключиться
 */
static uint32_t exitCallback(void* context) {
    PluginData* app = context;
    UNUSED(app);
    //Возвращаем ID вида, в который нужно вернуться
    return VIEW_NONE;
}

/**
 * @brief Создание списка действий с указанным датчиком
 * 
 * @param app Указатель на данные плагина
 */
void sensorActions_scene(PluginData* app) {
    variable_item_list = variable_item_list_alloc();
    //Сброс всех элементов меню
    variable_item_list_reset(variable_item_list);
    //Добавление элементов в список
    variable_item_list_add(variable_item_list, "Edit", 0, NULL, NULL);
    variable_item_list_add(variable_item_list, "Delete", 0, NULL, NULL);

    //Добавление колбека на нажатие средней кнопки
    variable_item_list_set_enter_callback(variable_item_list, enterCallback, app);

    //Создание вида из списка
    view = variable_item_list_get_view(variable_item_list);
    //Добавление колбека на нажатие кнопки "Назад"
    view_set_previous_callback(view, exitCallback);
    //Добавление вида в диспетчер
    view_dispatcher_add_view(app->view_dispatcher, SENSOR_ACTIONS_VIEW, view);

    //Переключение на наш вид
    view_dispatcher_switch_to_view(app->view_dispatcher, SENSOR_ACTIONS_VIEW);
    //Запуск диспетчера
    view_dispatcher_run(app->view_dispatcher);

    //Очистка списка элементов
    variable_item_list_free(variable_item_list);
    //Удаление вида после обработки
    view_dispatcher_remove_view(app->view_dispatcher, SENSOR_ACTIONS_VIEW);
}
