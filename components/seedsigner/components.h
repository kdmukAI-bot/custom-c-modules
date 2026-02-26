#include "lvgl.h"

lv_obj_t* top_nav(const char* label_text);

lv_obj_t* button(lv_obj_t* lv_parent, char* text, lv_obj_t* align_to);

void button_set_active(lv_obj_t* lv_button, bool active);
