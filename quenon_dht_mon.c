#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <stdio.h>

#define APP_NAME "DHT monitor"

typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} PluginEvent;

typedef struct {
    float temp;
    float hum;
    char txtbuff[25];
} PluginData;

static void render_callback(Canvas* const canvas, void* ctx) {
    PluginData* plugin_state = acquire_mutex((ValueMutex*)ctx, 25);
    if(plugin_state == NULL) {
        return;
    }
    // border around the edge of the screen
    canvas_draw_frame(canvas, 0, 0, 128, 64);

    canvas_set_font(canvas, FontPrimary);
    snprintf(
        plugin_state->txtbuff,
        sizeof(plugin_state->txtbuff),
        "Temp: %dC Hum: %d%%",
        (uint8_t)plugin_state->temp,
        (uint8_t)plugin_state->hum);
    canvas_draw_str(canvas, 20, 35, plugin_state->txtbuff);

    release_mutex((ValueMutex*)ctx, plugin_state);
}

static void input_callback(InputEvent* input_event, FuriMessageQueue* event_queue) {
    furi_assert(event_queue);

    PluginEvent event = {.type = EventTypeKey, .input = *input_event};
    furi_message_queue_put(event_queue, &event, FuriWaitForever);
}

static void hello_world_state_init(PluginData* const plugin_state) {
    plugin_state->temp = -128;
    plugin_state->hum = -128;
}

int32_t quenon_dht_app() {
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(PluginEvent));

    PluginData* plugin_state = malloc(sizeof(PluginData));

    hello_world_state_init(plugin_state);

    ValueMutex state_mutex;
    if(!init_mutex(&state_mutex, plugin_state, sizeof(PluginData))) {
        FURI_LOG_E(APP_NAME, "cannot create mutex\r\n");
        free(plugin_state);
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

        PluginData* plugin_state = (PluginData*)acquire_mutex_block(&state_mutex);

        if(event_status == FuriStatusOk) {
            // press events
            if(event.type == EventTypeKey) {
                if(event.input.type == InputTypePress) {
                    switch(event.input.key) {
                    case InputKeyUp:
                        //  plugin_state->y--;
                        break;
                    case InputKeyDown:
                        //  plugin_state->y++;
                        break;
                    case InputKeyRight:
                        //   plugin_state->x++;
                        break;
                    case InputKeyLeft:
                        //  plugin_state->x--;
                        break;
                    case InputKeyOk:
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
        release_mutex(&state_mutex, plugin_state);
    }

    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close("gui");
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    delete_mutex(&state_mutex);

    return 0;
}