#include "logging.h"
#include "widgets_json.h"

int main(int argc, char** argv)
{
    enable_printf(JSON_PRINTF);
    if (argc != 2) {
        error_printf("%s <file_json>\n", argv[0]);
        return EXIT_FAILURE;
    }
    app_context app_ctx = {
        .window_rect = {.x=0, .y=0, .w=800, .h=480},
    };
    setup_orientation(0.0, 800, 480, &app_ctx.window_rect);
    
    view_context view_ctx = { 
        .app = &app_ctx,
        .list = create_widget_list(&view_ctx),
    };
    puts("---------------------------------");    
    if ( 0 == deserialise_widgets_file(argv[1], &view_ctx)) {
        for(widget* widget=&view_ctx.list->head; widget != NULL; widget=widget->next) {
            switch(widget->type) {
                case WIDGET_NONE:
                case WIDGET_END:
                    break;
                default:
                    json_printf("widget_type:%d ", widget->type);
//                    json_printf("rect:{%4d, %4d, %4d, %4d}, ", widget->rect.x, widget->rect.y, widget->rect.w, widget->rect.h);
//                    json_printf("image_rect:{%4d, %4d, %4d, %4d}, ", widget->draw_rect.x, widget->draw_rect.y, widget->draw_rect.w, widget->draw_rect.h);
                    json_printf("input_rect:{%4d, %4d, %4d, %4d}, ", widget->input_rect.x, widget->input_rect.y, widget->input_rect.w, widget->input_rect.h);
                    json_printf(" %s\n", widget_type_name(widget->type));
                    break;
            }
        }
    
        destroy_widget_list(view_ctx.list);
    }
    puts("---------------------------------");    
    return EXIT_SUCCESS;
}
