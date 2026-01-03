#ifndef __widgets_json_h_
#define __widgets_json_h_

#include "json.h"
#include "widgets.h"

int deserialise_widgets_file(const char* file_path, view_context* ctx);
void deserialise_widgets(json_value* value, view_context* ctx);

#endif
