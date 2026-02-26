#include "display_manager.h"

extern "C" void dm_shim_init(void) {
    init();
}

extern "C" void dm_shim_render_demo_ui(void) {
    render_demo_ui();
}
