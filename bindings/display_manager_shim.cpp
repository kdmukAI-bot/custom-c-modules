#include "esp_heap_caps.h"
#include "display_manager.h"

extern "C" void dm_shim_init(void) {
    init();
}

extern "C" void dm_shim_render_demo_ui(void) {
    render_demo_ui();
}

extern "C" size_t dm_shim_dma_internal_free(void) {
    return heap_caps_get_free_size(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
}

extern "C" size_t dm_shim_dma_internal_largest(void) {
    return heap_caps_get_largest_free_block(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
}
