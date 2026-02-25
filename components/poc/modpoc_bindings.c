#include "py/obj.h"
#include "py/runtime.h"

#include "poc_core.h"

// poc.add(a, b) -> int
static mp_obj_t mp_poc_add(mp_obj_t a_in, mp_obj_t b_in) {
    mp_int_t a = mp_obj_get_int(a_in);
    mp_int_t b = mp_obj_get_int(b_in);
    return mp_obj_new_int((mp_int_t)poc_add((int32_t)a, (int32_t)b));
}
static MP_DEFINE_CONST_FUN_OBJ_2(poc_add_obj, mp_poc_add);

// poc.ping() -> "pong"
static mp_obj_t mp_poc_ping(void) {
    const char *s = poc_ping();
    return mp_obj_new_str(s, 4);
}
static MP_DEFINE_CONST_FUN_OBJ_0(poc_ping_obj, mp_poc_ping);

static const mp_rom_map_elem_t poc_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_poc) },
    { MP_ROM_QSTR(MP_QSTR_add), MP_ROM_PTR(&poc_add_obj) },
    { MP_ROM_QSTR(MP_QSTR_ping), MP_ROM_PTR(&poc_ping_obj) },
};
static MP_DEFINE_CONST_DICT(poc_module_globals, poc_module_globals_table);

const mp_obj_module_t poc_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&poc_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_poc, poc_user_cmodule);
