#include "seedsigner.h"
#include "components.h"
#include "gui_constants.h"

#include "lvgl.h"

static void load_screen_and_cleanup_previous(lv_obj_t *new_screen) {
    lv_obj_t *old_screen = lv_scr_act();
    lv_scr_load(new_screen);
    if (old_screen && old_screen != new_screen) {
        lv_obj_del(old_screen);
    }
}

void demo_screen(void *ctx)
{
    top_nav_ctx_t top_nav_ctx = TOP_NAV_CTX_DEFAULTS;
    top_nav_ctx.title = "Settings";

    // Create a new screen
    lv_obj_t * scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(BACKGROUND_COLOR), LV_PART_MAIN); // Set background color to black

    // Set global style defaults
    lv_obj_set_style_radius(scr, 0, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(scr, BODY_LINE_SPACING, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(scr, 0, LV_PART_MAIN);

    lv_obj_t* lv_seedsigner_top_nav = top_nav(scr, &top_nav_ctx); // Add top navigation bar

    lv_obj_t* body_content = lv_obj_create(scr);
    lv_obj_set_size(body_content, lv_obj_get_width(scr), lv_obj_get_height(scr) - TOP_NAV_HEIGHT - COMPONENT_PADDING);
    lv_obj_align_to(body_content, lv_seedsigner_top_nav, LV_ALIGN_OUT_BOTTOM_MID, 0, COMPONENT_PADDING);
    lv_obj_set_style_bg_color(body_content, lv_color_hex(BACKGROUND_COLOR), LV_PART_MAIN);
    lv_obj_set_style_pad_left(body_content, EDGE_PADDING, LV_PART_MAIN);
    lv_obj_set_style_pad_right(body_content, EDGE_PADDING, LV_PART_MAIN);
    lv_obj_set_style_pad_top(body_content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(body_content, COMPONENT_PADDING, LV_PART_MAIN);

    // debugging
    // lv_obj_set_style_border_color(body_content, lv_color_hex(BUTTON_BACKGROUND_COLOR), LV_PART_MAIN);
    lv_obj_set_style_border_width(body_content, 0, LV_PART_MAIN);

    // Limit scrolling to the vertical direction only
    lv_obj_set_scroll_dir(body_content, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(body_content, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_pad_right(body_content, 0, LV_PART_SCROLLBAR);

    static const button_list_item_t demo_buttons[] = {
        { .label = "Language", .value = NULL },
        { .label = "Persistent Settings", .value = NULL },
        { .label = "Another option", .value = NULL },
        { .label = "Wow so many options", .value = NULL },
        { .label = "Continue", .value = NULL },
    };
    lv_obj_t* lv_seedsigner_button = button_list(body_content, demo_buttons, sizeof(demo_buttons) / sizeof(demo_buttons[0]));

    lv_obj_t* lv_body_text = lv_label_create(body_content);
    lv_obj_set_width(lv_body_text, lv_obj_get_width(body_content) - 2 * COMPONENT_PADDING);
    lv_obj_align_to(lv_body_text, lv_seedsigner_button, LV_ALIGN_OUT_BOTTOM_LEFT, 0, COMPONENT_PADDING);
    lv_obj_set_style_text_color(lv_body_text, lv_color_hex(BODY_FONT_COLOR), 0);
    lv_obj_set_style_text_font(lv_body_text, &BODY_FONT, LV_PART_MAIN);
    lv_label_set_text(lv_body_text, "Long the Paris streets, the death-carts rumble, hollow and harsh. Six tumbrils carry the day's wine to La Guillotine. All the devouring and insatiate Monsters imagined since imagination could record itself, are fused in the one realisation, Guillotine. And yet there is not in France, with its rich variety of soil and climate, a blade, a leaf, a root, a sprig, a peppercorn, which will grow to maturity under conditions more certain than those that have produced this horror. Crush humanity out of shape once more, under similar hammers, and it will twist itself into the same tortured forms. Sow the same seed of rapacious license and oppression over again, and it will surely yield the same fruit according to its kind.\n\nSix tumbrils roll along the streets. Change these back again to what they were, thou powerful enchanter, Time, and they shall be seen to be the carriages of absolute monarchs, the equipages of feudal nobles, the toilettes of flaring Jezebels, the churches that are not my Father's house but dens of thieves, the huts of millions of starving peasants! No; the great magician who majestically works out the appointed order of the Creator, never reverses his transformations. \"If thou be changed into this shape by the will of God,\" say the seers to the enchanted, in the wise Arabian stories, \"then remain so! But, if thou wear this form through mere passing conjuration, then resume thy former aspect!\" Changeless and hopeless, the tumbrils roll along.");

    load_screen_and_cleanup_previous(scr);
}



void button_list_screen(void *ctx)
{
    static const button_list_screen_ctx_t defaults = {
        .top_nav = {
            .title = "Settings",
            .show_back_button = false,
            .show_power_button = false,
        },
        .button_list = NULL,
        .button_list_len = 0,
    };

    const button_list_screen_ctx_t *screen_ctx = (const button_list_screen_ctx_t *)ctx;
    if (!screen_ctx) {
        screen_ctx = &defaults;
    }

    // Create a new screen
    lv_obj_t * scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(BACKGROUND_COLOR), LV_PART_MAIN); // Set background color to black

    // Set global style defaults
    lv_obj_set_style_radius(scr, 0, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(scr, BODY_LINE_SPACING, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(scr, 0, LV_PART_MAIN);

    lv_obj_t* lv_seedsigner_top_nav = top_nav(scr, &screen_ctx->top_nav); // Add top navigation bar

    lv_obj_t* body_content = lv_obj_create(scr);
    lv_obj_set_size(body_content, lv_obj_get_width(scr), lv_obj_get_height(scr) - TOP_NAV_HEIGHT - COMPONENT_PADDING);
    lv_obj_align_to(body_content, lv_seedsigner_top_nav, LV_ALIGN_OUT_BOTTOM_MID, 0, COMPONENT_PADDING);
    lv_obj_set_style_bg_color(body_content, lv_color_hex(BACKGROUND_COLOR), LV_PART_MAIN);
    lv_obj_set_style_pad_left(body_content, EDGE_PADDING, LV_PART_MAIN);
    lv_obj_set_style_pad_right(body_content, EDGE_PADDING, LV_PART_MAIN);
    lv_obj_set_style_pad_top(body_content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(body_content, COMPONENT_PADDING, LV_PART_MAIN);

    // debugging
    // lv_obj_set_style_border_color(body_content, lv_color_hex(BUTTON_BACKGROUND_COLOR), LV_PART_MAIN);
    lv_obj_set_style_border_width(body_content, 0, LV_PART_MAIN);

    // Limit scrolling to the vertical direction only
    lv_obj_set_scroll_dir(body_content, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(body_content, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_pad_right(body_content, 0, LV_PART_SCROLLBAR);

    button_list(body_content, screen_ctx->button_list, screen_ctx->button_list_len);

    load_screen_and_cleanup_previous(scr);
}


void main_menu_screen(void *ctx)
{
    (void)ctx;

    top_nav_ctx_t top_nav_ctx = TOP_NAV_CTX_DEFAULTS;
    top_nav_ctx.title = "Home";
    top_nav_ctx.show_back_button = false;
    top_nav_ctx.show_power_button = true;

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(BACKGROUND_COLOR), LV_PART_MAIN);
    lv_obj_set_style_radius(scr, 0, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(scr, BODY_LINE_SPACING, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(scr, 0, LV_PART_MAIN);

    lv_obj_t* lv_seedsigner_top_nav = top_nav(scr, &top_nav_ctx);

    lv_obj_t* body_content = lv_obj_create(scr);
    lv_obj_set_size(body_content, lv_obj_get_width(scr), lv_obj_get_height(scr) - TOP_NAV_HEIGHT - COMPONENT_PADDING);
    lv_obj_align_to(body_content, lv_seedsigner_top_nav, LV_ALIGN_OUT_BOTTOM_MID, 0, COMPONENT_PADDING);
    lv_obj_set_style_bg_color(body_content, lv_color_hex(BACKGROUND_COLOR), LV_PART_MAIN);
    lv_obj_set_style_pad_left(body_content, EDGE_PADDING, LV_PART_MAIN);
    lv_obj_set_style_pad_right(body_content, EDGE_PADDING, LV_PART_MAIN);
    lv_obj_set_style_pad_top(body_content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(body_content, COMPONENT_PADDING, LV_PART_MAIN);
    lv_obj_set_style_border_width(body_content, 0, LV_PART_MAIN);
    lv_obj_set_scroll_dir(body_content, LV_DIR_NONE);
    lv_obj_set_scrollbar_mode(body_content, LV_SCROLLBAR_MODE_OFF);

    static const char *icons[] = {
        SeedSignerIconConstants::SCAN,
        SeedSignerIconConstants::SEEDS,
        SeedSignerIconConstants::TOOLS,
        SeedSignerIconConstants::SETTINGS,
    };
    static const char *labels[] = {"Scan", "Seeds", "Tools", "Settings"};

    const lv_coord_t available_w = lv_obj_get_content_width(body_content);
    const lv_coord_t available_h = lv_obj_get_content_height(body_content);

    const lv_coord_t outer_x = EDGE_PADDING * 3;
    const lv_coord_t outer_y = COMPONENT_PADDING * 3;
    const lv_coord_t gap_x = COMPONENT_PADDING * 3;
    const lv_coord_t gap_y = COMPONENT_PADDING * 3;

    lv_coord_t usable_w = available_w - (outer_x * 2);
    lv_coord_t usable_h = available_h - (outer_y * 2);
    if (usable_w < 100) usable_w = available_w;
    if (usable_h < 100) usable_h = available_h;

    lv_coord_t button_w = (usable_w - gap_x) / 2;
    lv_coord_t button_h = (usable_h - gap_y) / 2;
    if (button_w < 40) button_w = 40;
    if (button_h < 40) button_h = 40;

    const lv_coord_t grid_w = (button_w * 2) + gap_x;
    const lv_coord_t grid_h = (button_h * 2) + gap_y;
    const lv_coord_t x0 = (available_w > grid_w) ? ((available_w - grid_w) / 2) : 0;
    const lv_coord_t y0 = (available_h > grid_h) ? ((available_h - grid_h) / 2) : 0;

    lv_obj_t *buttons[4] = {NULL, NULL, NULL, NULL};
    for (uint32_t i = 0; i < 4; ++i) {
        lv_obj_t *btn = large_icon_button(body_content, icons[i], labels[i], NULL);
        lv_obj_set_size(btn, button_w, button_h);

        lv_coord_t col = i % 2;
        lv_coord_t row = i / 2;
        lv_obj_set_pos(btn, x0 + col * (button_w + gap_x), y0 + row * (button_h + gap_y));
        buttons[i] = btn;
    }

    if (buttons[0]) {
        button_set_active(buttons[0], true);
    }

    load_screen_and_cleanup_previous(scr);
}


void lv_seedsigner_screen_close(void)
{
    /*Delete all animation*/
    lv_anim_del(NULL, NULL);

    // lv_timer_del(meter2_timer);
    // meter2_timer = NULL;

    lv_obj_clean(lv_scr_act());

    // lv_style_reset(&style_text_muted);
    // lv_style_reset(&style_title);
    // lv_style_reset(&style_icon);
    // lv_style_reset(&style_bullet);
}


