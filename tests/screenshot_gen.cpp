#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <string>
#include <vector>

#include "lvgl.h"
#include "seedsigner.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define DEFAULT_WIDTH 480
#define DEFAULT_HEIGHT 320
#define DEFAULT_OUT_DIR "artifacts/screenshots"

static int g_width = DEFAULT_WIDTH;
static int g_height = DEFAULT_HEIGHT;
static std::vector<lv_color_t> g_fb;

static void write_u32_be(uint8_t *out, uint32_t v) {
    out[0] = (uint8_t)((v >> 24) & 0xFF);
    out[1] = (uint8_t)((v >> 16) & 0xFF);
    out[2] = (uint8_t)((v >> 8) & 0xFF);
    out[3] = (uint8_t)(v & 0xFF);
}

static uint32_t crc32_png(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc & 1u) ? (crc >> 1) ^ 0xEDB88320u : (crc >> 1);
        }
    }
    return ~crc;
}

static uint32_t adler32(const uint8_t *data, size_t len) {
    uint32_t a = 1;
    uint32_t b = 0;
    const uint32_t MOD = 65521;
    for (size_t i = 0; i < len; i++) {
        a = (a + data[i]) % MOD;
        b = (b + a) % MOD;
    }
    return (b << 16) | a;
}

static int append_chunk(FILE *f, const char type[4], const uint8_t *data, uint32_t len) {
    uint8_t len_be[4];
    uint8_t crc_be[4];

    write_u32_be(len_be, len);
    if (fwrite(len_be, 1, 4, f) != 4) return -1;
    if (fwrite(type, 1, 4, f) != 4) return -1;
    if (len > 0 && fwrite(data, 1, len, f) != len) return -1;

    uint8_t *crc_buf = (uint8_t *)malloc((size_t)len + 4u);
    if (!crc_buf) return -1;
    memcpy(crc_buf, type, 4);
    if (len > 0) memcpy(crc_buf + 4, data, len);
    uint32_t crc = crc32_png(crc_buf, (size_t)len + 4u);
    free(crc_buf);

    write_u32_be(crc_be, crc);
    if (fwrite(crc_be, 1, 4, f) != 4) return -1;
    return 0;
}

static int write_png_rgb24(const char *path, const uint8_t *rgb, int width, int height) {
    if (width <= 0 || height <= 0) return -1;

    const size_t row_raw = (size_t)width * 3u;
    const size_t raw_size = (row_raw + 1u) * (size_t)height;
    uint8_t *raw = (uint8_t *)malloc(raw_size);
    if (!raw) return -1;

    for (int y = 0; y < height; y++) {
        uint8_t *dst = raw + (size_t)y * (row_raw + 1u);
        const uint8_t *src = rgb + (size_t)y * row_raw;
        dst[0] = 0;
        memcpy(dst + 1, src, row_raw);
    }

    const size_t max_blocks = (raw_size + 65534u) / 65535u;
    const size_t zlib_size = 2u + raw_size + max_blocks * 5u + 4u;
    uint8_t *zlib = (uint8_t *)malloc(zlib_size);
    if (!zlib) {
        free(raw);
        return -1;
    }

    size_t zi = 0;
    zlib[zi++] = 0x78;
    zlib[zi++] = 0x01;

    size_t remaining = raw_size;
    size_t ri = 0;
    while (remaining > 0) {
        uint16_t block_len = (remaining > 65535u) ? 65535u : (uint16_t)remaining;
        uint16_t nlen = (uint16_t)~block_len;
        uint8_t bfinal = (remaining <= 65535u) ? 1u : 0u;
        zlib[zi++] = bfinal;
        zlib[zi++] = (uint8_t)(block_len & 0xFF);
        zlib[zi++] = (uint8_t)((block_len >> 8) & 0xFF);
        zlib[zi++] = (uint8_t)(nlen & 0xFF);
        zlib[zi++] = (uint8_t)((nlen >> 8) & 0xFF);
        memcpy(zlib + zi, raw + ri, block_len);
        zi += block_len;
        ri += block_len;
        remaining -= block_len;
    }

    uint32_t ad = adler32(raw, raw_size);
    zlib[zi++] = (uint8_t)((ad >> 24) & 0xFF);
    zlib[zi++] = (uint8_t)((ad >> 16) & 0xFF);
    zlib[zi++] = (uint8_t)((ad >> 8) & 0xFF);
    zlib[zi++] = (uint8_t)(ad & 0xFF);

    FILE *f = fopen(path, "wb");
    if (!f) {
        free(raw);
        free(zlib);
        return -1;
    }

    static const uint8_t sig[8] = {137,80,78,71,13,10,26,10};
    uint8_t ihdr[13] = {0};
    if (fwrite(sig, 1, 8, f) != 8) goto fail;

    write_u32_be(ihdr, (uint32_t)width);
    write_u32_be(ihdr + 4, (uint32_t)height);
    ihdr[8] = 8;
    ihdr[9] = 2;

    if (append_chunk(f, "IHDR", ihdr, 13) != 0) goto fail;
    if (append_chunk(f, "IDAT", zlib, (uint32_t)zi) != 0) goto fail;
    if (append_chunk(f, "IEND", NULL, 0) != 0) goto fail;

    fclose(f);
    free(raw);
    free(zlib);
    return 0;

fail:
    fclose(f);
    free(raw);
    free(zlib);
    return -1;
}

static int mkdir_p(const char *path) {
    char tmp[PATH_MAX];
    size_t len = strlen(path);
    if (len == 0 || len >= sizeof(tmp)) return -1;
    memcpy(tmp, path, len + 1);

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0775) != 0 && errno != EEXIST) return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, 0775) != 0 && errno != EEXIST) return -1;
    return 0;
}

static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
    (void)drv;
    for (int y = area->y1; y <= area->y2; ++y) {
        if (y < 0 || y >= g_height) continue;
        for (int x = area->x1; x <= area->x2; ++x) {
            if (x < 0 || x >= g_width) continue;
            size_t di = (size_t)y * (size_t)g_width + (size_t)x;
            g_fb[di] = *color_p;
            color_p++;
        }
    }
    lv_disp_flush_ready(drv);
}

static void render_main_menu() {
    main_menu_screen(NULL);
}

static void render_button_list_screen() {
    static const button_list_item_t items[] = {
        {.label = "Language", .value = NULL},
        {.label = "Persistent Settings", .value = NULL},
        {.label = "Camera", .value = NULL},
    };
    button_list_screen_ctx_t ctx = {
        .top_nav = {
            .title = "Settings",
            .show_back_button = true,
            .show_power_button = false,
        },
        .button_list = items,
        .button_list_len = sizeof(items) / sizeof(items[0]),
    };
    button_list_screen(&ctx);
}


static void render_button_list_1_item() {
    static const button_list_item_t items[] = {
        {.label = "Continue", .value = NULL},
    };
    button_list_screen_ctx_t ctx = {
        .top_nav = {
            .title = "Settings",
            .show_back_button = true,
            .show_power_button = false,
        },
        .button_list = items,
        .button_list_len = sizeof(items) / sizeof(items[0]),
    };
    button_list_screen(&ctx);
}

static void render_button_list_4_items() {
    static const button_list_item_t items[] = {
        {.label = "Language", .value = NULL},
        {.label = "Persistent Settings", .value = NULL},
        {.label = "Camera", .value = NULL},
        {.label = "Network", .value = NULL},
    };
    button_list_screen_ctx_t ctx = {
        .top_nav = {
            .title = "Settings",
            .show_back_button = true,
            .show_power_button = false,
        },
        .button_list = items,
        .button_list_len = sizeof(items) / sizeof(items[0]),
    };
    button_list_screen(&ctx);
}

static void render_button_list_scroll_many() {
    static const button_list_item_t items[] = {
        {.label = "Language", .value = NULL},
        {.label = "Persistent Settings", .value = NULL},
        {.label = "Camera", .value = NULL},
        {.label = "Network", .value = NULL},
        {.label = "Display", .value = NULL},
        {.label = "Security", .value = NULL},
        {.label = "Diagnostics", .value = NULL},
        {.label = "Advanced", .value = NULL},
        {.label = "About", .value = NULL},
        {.label = "Factory Reset", .value = NULL},
    };
    button_list_screen_ctx_t ctx = {
        .top_nav = {
            .title = "Settings",
            .show_back_button = true,
            .show_power_button = false,
        },
        .button_list = items,
        .button_list_len = sizeof(items) / sizeof(items[0]),
    };
    button_list_screen(&ctx);
}


static void render_button_list_long_title() {
    static const button_list_item_t items[] = {
        {.label = "Language", .value = NULL},
        {.label = "Persistent Settings", .value = NULL},
        {.label = "Camera", .value = NULL},
        {.label = "Network", .value = NULL},
    };
    button_list_screen_ctx_t ctx = {
        .top_nav = {
            .title = "Donaudampfschifffahrtsgesellschaftskapitaensanwaerterpruefungsordnung",
            .show_back_button = true,
            .show_power_button = false,
        },
        .button_list = items,
        .button_list_len = sizeof(items) / sizeof(items[0]),
    };
    button_list_screen(&ctx);
}


static void render_button_list_buttonless_screen() {
    static const button_list_item_t items[] = {
        {.label = "Language", .value = NULL},
        {.label = "Persistent Settings", .value = NULL},
        {.label = "Camera", .value = NULL},
    };
    button_list_screen_ctx_t ctx = {
        .top_nav = {
            .title = "Settings",
            .show_back_button = false,
            .show_power_button = false,
        },
        .button_list = items,
        .button_list_len = sizeof(items) / sizeof(items[0]),
    };
    button_list_screen(&ctx);
}

static bool render_named_scenario(const char *name) {
    if (strcmp(name, "main_menu") == 0) {
        render_main_menu();
        return true;
    }
    if (strcmp(name, "button_list_screen") == 0) {
        render_button_list_screen();
        return true;
    }
    if (strcmp(name, "button_list_screen_1_item") == 0) {
        render_button_list_1_item();
        return true;
    }
    if (strcmp(name, "button_list_screen_4_items") == 0) {
        render_button_list_4_items();
        return true;
    }
    if (strcmp(name, "button_list_screen_scroll_many") == 0) {
        render_button_list_scroll_many();
        return true;
    }
    if (strcmp(name, "button_list_screen_long_title") == 0) {
        render_button_list_long_title();
        return true;
    }
    return false;
}

static int framebuffer_to_rgb24(std::vector<uint8_t> &rgb) {
    rgb.resize((size_t)g_width * (size_t)g_height * 3u);
    for (int y = 0; y < g_height; ++y) {
        for (int x = 0; x < g_width; ++x) {
            size_t si = (size_t)y * (size_t)g_width + (size_t)x;
            size_t di = si * 3u;
            uint32_t c32 = lv_color_to32(g_fb[si]);
            rgb[di + 0] = (uint8_t)((c32 >> 16) & 0xFF);
            rgb[di + 1] = (uint8_t)((c32 >> 8) & 0xFF);
            rgb[di + 2] = (uint8_t)(c32 & 0xFF);
        }
    }
    return 0;
}

static std::string html_escape(const std::string &in) {
    std::string out;
    out.reserve(in.size());
    for (char ch : in) {
        switch (ch) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out += ch; break;
        }
    }
    return out;
}


static std::string scenario_display_name(const std::string &scenario) {
    const std::string marker = "_screen_";
    size_t pos = scenario.find(marker);
    if (pos == std::string::npos) return scenario;

    std::string base = scenario.substr(0, pos + marker.size() - 1); // include "_screen"
    std::string variation = scenario.substr(pos + marker.size());
    if (variation.empty()) return scenario;
    return base + " (" + variation + ")";
}

static bool is_scroll_title_scenario(const std::string &scenario) {
    return scenario == "button_list_screen_long_title";
}

static const char* imagemagick_binary() {
    if (system("command -v magick >/dev/null 2>&1") == 0) return "magick";
    if (system("command -v convert >/dev/null 2>&1") == 0) return "convert";
    return NULL;
}


static void cleanup_frames_dir(const char *frames_dir) {
    DIR *d = opendir(frames_dir);
    if (!d) return;

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        const char *name = ent->d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", frames_dir, name);
        unlink(path);
    }
    closedir(d);
    rmdir(frames_dir);
}

static int maybe_write_scroll_gif(const char *run_dir, const std::string &scenario, lv_disp_t *disp, const char *im_bin) {
    if (!im_bin || !is_scroll_title_scenario(scenario)) {
        return 0;
    }

    char frames_dir[PATH_MAX];
    snprintf(frames_dir, sizeof(frames_dir), "%s/%s.frames", run_dir, scenario.c_str());
    if (mkdir_p(frames_dir) != 0) {
        return -1;
    }

    const int frame_count = 180;
    const int frame_step_ms = 56;  // ~18 FPS over ~10s

    for (int i = 0; i < frame_count; ++i) {
        lv_tick_inc(frame_step_ms);
        lv_timer_handler();
        lv_refr_now(disp);

        std::vector<uint8_t> rgb;
        framebuffer_to_rgb24(rgb);

        char frame_png[PATH_MAX];
        snprintf(frame_png, sizeof(frame_png), "%s/frame_%03d.png", frames_dir, i);
        if (write_png_rgb24(frame_png, rgb.data(), g_width, g_height) != 0) {
            return -1;
        }
    }

    char out_gif[PATH_MAX];
    snprintf(out_gif, sizeof(out_gif), "%s/%s.gif", run_dir, scenario.c_str());

    char cmd[PATH_MAX * 3];
    snprintf(cmd, sizeof(cmd), "%s -delay 8 -loop 0 '%s'/frame_*.png '%s'", im_bin, frames_dir, out_gif);
    int rc = system(cmd);
    if (rc != 0) {
        cleanup_frames_dir(frames_dir);
        return -1;
    }

    cleanup_frames_dir(frames_dir);
    return 0;
}

static int write_run_index_html(const char *run_dir, const char *ts, const std::vector<std::string> &scenarios, int width, int height) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/index.html", run_dir);
    FILE *f = fopen(path, "w");
    if (!f) return -1;

    fprintf(f, "<!doctype html>\n");
    fprintf(f, "<html><head><meta charset=\"utf-8\">\n");
    fprintf(f, "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n");
    fprintf(f, "<title>Screenshots %s</title>\n", ts);
    fprintf(f, "<style>body{font-family:system-ui,sans-serif;margin:16px;background:#111;color:#eee}"
               "a{color:#9cf} .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(520px,1fr));gap:14px;align-items:start}"
               "figure{margin:0;background:#1b1b1b;border:1px solid #333;border-radius:8px;padding:10px;overflow:auto}"
               "img{display:block;width:auto;max-width:100%%;height:auto;background:#000;border-radius:6px;margin:0 auto;image-rendering:auto}"
               "figcaption{margin:0 0 10px 0;font-size:22px;font-weight:700;letter-spacing:.2px;color:#f2f2f2;text-align:center}</style></head><body>\n");
    fprintf(f, "<h1>Unit tests for SeedSigner lvgl screens</h1><p>Run: <code>%s</code> • %dx%d</p>\n", ts, width, height);
    fprintf(f, "<p><a href=\"../index.html\">All runs</a> • <a href=\"../latest/\">Latest</a></p>\n");
    fprintf(f, "<div class=\"grid\">\n");

    for (const std::string &scenario : scenarios) {
        std::string base_esc = html_escape(scenario);
        std::string label_esc = html_escape(scenario_display_name(scenario));

        char gif_path[PATH_MAX];
        snprintf(gif_path, sizeof(gif_path), "%s/%s.gif", run_dir, scenario.c_str());
        struct stat gif_st;
        bool has_gif = (stat(gif_path, &gif_st) == 0) && S_ISREG(gif_st.st_mode);
        const char *ext = has_gif ? "gif" : "png";

        fprintf(f, "<figure><figcaption>%s%s</figcaption><a href=\"%s.%s\"><img loading=\"lazy\" src=\"%s.%s\" alt=\"%s\" width=\"%d\" height=\"%d\"></a></figure>\n",
                label_esc.c_str(), has_gif ? " [animated]" : "", base_esc.c_str(), ext, base_esc.c_str(), ext, label_esc.c_str(), width, height);
    }

    fprintf(f, "</div></body></html>\n");
    fclose(f);
    return 0;
}

static int write_root_index_html(const char *out_dir) {
    DIR *d = opendir(out_dir);
    if (!d) return -1;

    std::vector<std::string> runs;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        const char *name = ent->d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
        if (strcmp(name, "latest") == 0) continue;
        if (strstr(name, ".") != NULL) continue;

        char probe[PATH_MAX];
        snprintf(probe, sizeof(probe), "%s/%s/index.html", out_dir, name);
        struct stat st;
        if (stat(probe, &st) == 0 && S_ISREG(st.st_mode)) {
            runs.emplace_back(name);
        }
    }
    closedir(d);

    std::sort(runs.begin(), runs.end(), std::greater<std::string>());

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/index.html", out_dir);
    FILE *f = fopen(path, "w");
    if (!f) return -1;

    fprintf(f, "<!doctype html><html><head><meta charset=\"utf-8\">"
               "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
               "<title>Screenshot Runs</title>"
               "<style>body{font-family:system-ui,sans-serif;margin:16px;background:#111;color:#eee}"
               "a{color:#9cf} li{margin:8px 0}</style></head><body>");
    fprintf(f, "<h1>Screenshot Runs</h1><p><a href=\"latest/\">Open latest</a></p><ul>");
    for (const std::string &run : runs) {
        std::string esc = html_escape(run);
        fprintf(f, "<li><a href=\"%s/\">%s</a></li>", esc.c_str(), esc.c_str());
    }
    fprintf(f, "</ul></body></html>\n");
    fclose(f);
    return 0;
}

static void usage(const char *argv0) {
    printf("Usage: %s [options]\n", argv0);
    printf("  --out-dir <path>     Output root (default: %s)\n", DEFAULT_OUT_DIR);
    printf("  --width <px>         Width (default: %d)\n", DEFAULT_WIDTH);
    printf("  --height <px>        Height (default: %d)\n", DEFAULT_HEIGHT);
    printf("  --scenarios <list>   Ignored (always runs: main_menu, button_list_screen, button_list_screen_1_item, button_list_screen_4_items, button_list_screen_scroll_many, button_list_screen_long_title)\n");
    printf("  --help               Show this help\n");
}

int main(int argc, char **argv) {
    const char *out_dir = DEFAULT_OUT_DIR;
    std::string scenarios_csv = "main_menu,button_list_screen,button_list_screen_1_item,button_list_screen_4_items,button_list_screen_scroll_many,button_list_screen_long_title";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--out-dir") == 0 && i + 1 < argc) {
            out_dir = argv[++i];
        } else if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) {
            g_width = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) {
            g_height = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--scenarios") == 0 && i + 1 < argc) {
            (void)argv[++i];
            fprintf(stderr, "--scenarios is ignored; running full suite\n");
        } else if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            return 2;
        }
    }

    if (g_width <= 0 || g_height <= 0) {
        fprintf(stderr, "Invalid dimensions\n");
        return 2;
    }

    if (mkdir_p(out_dir) != 0) {
        fprintf(stderr, "Failed to create out dir '%s': %s\n", out_dir, strerror(errno));
        return 1;
    }

    time_t now = time(NULL);
    struct tm tm_utc;
    if (!gmtime_r(&now, &tm_utc)) return 1;

    char ts[32];
    if (strftime(ts, sizeof(ts), "%Y-%m-%dT%H-%M-%SZ", &tm_utc) == 0) return 1;

    char run_dir[PATH_MAX];
    snprintf(run_dir, sizeof(run_dir), "%s/%s", out_dir, ts);
    if (mkdir_p(run_dir) != 0) {
        fprintf(stderr, "Failed to create run dir '%s': %s\n", run_dir, strerror(errno));
        return 1;
    }

    lv_init();
    g_fb.assign((size_t)g_width * (size_t)g_height, lv_color_black());
    std::vector<lv_color_t> draw_buf((size_t)g_width * (size_t)g_height);

    lv_disp_draw_buf_t disp_buf;
    lv_disp_draw_buf_init(&disp_buf, draw_buf.data(), NULL, (uint32_t)draw_buf.size());

    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = g_width;
    disp_drv.ver_res = g_height;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &disp_buf;
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);
    (void)disp;

    const char *im_bin = imagemagick_binary();
    if (im_bin) {
        printf("imagemagick detected (%s): animated GIF generation enabled for scrolling-title scenarios\n", im_bin);
    } else {
        printf("imagemagick not detected (magick/convert): animated GIF generation disabled\n");
    }

    std::vector<std::string> scenarios;
    size_t start = 0;
    while (start < scenarios_csv.size()) {
        size_t comma = scenarios_csv.find(',', start);
        if (comma == std::string::npos) comma = scenarios_csv.size();
        std::string item = scenarios_csv.substr(start, comma - start);
        if (!item.empty()) scenarios.push_back(item);
        start = comma + 1;
    }

    for (const std::string &scenario : scenarios) {
        if (!render_named_scenario(scenario.c_str())) {
            fprintf(stderr, "Unknown scenario: %s\n", scenario.c_str());
            return 2;
        }

        // Capture initial frame before marquee/animation timers can shift text.
        lv_timer_handler();
        lv_refr_now(disp);

        std::vector<uint8_t> rgb;
        framebuffer_to_rgb24(rgb);

        char out_png[PATH_MAX];
        snprintf(out_png, sizeof(out_png), "%s/%s.png", run_dir, scenario.c_str());
        if (write_png_rgb24(out_png, rgb.data(), g_width, g_height) != 0) {
            fprintf(stderr, "Failed writing PNG: %s\n", out_png);
            return 1;
        }
        printf("wrote %s\n", out_png);

        if (maybe_write_scroll_gif(run_dir, scenario, disp, im_bin) != 0) {
            fprintf(stderr, "Failed writing animated GIF for scenario: %s\n", scenario.c_str());
            return 1;
        }
        if (im_bin && is_scroll_title_scenario(scenario)) {
            char out_gif[PATH_MAX];
            snprintf(out_gif, sizeof(out_gif), "%s/%s.gif", run_dir, scenario.c_str());
            printf("wrote %s\n", out_gif);
        }
    }

    char latest_link[PATH_MAX];
    snprintf(latest_link, sizeof(latest_link), "%s/latest", out_dir);
    unlink(latest_link);
    if (symlink(ts, latest_link) != 0) {
        char latest_txt[PATH_MAX];
        snprintf(latest_txt, sizeof(latest_txt), "%s/latest_path.txt", out_dir);
        FILE *lf = fopen(latest_txt, "w");
        if (!lf) return 1;
        fprintf(lf, "%s\n", run_dir);
        fclose(lf);
    }

    if (write_run_index_html(run_dir, ts, scenarios, g_width, g_height) != 0) {
        fprintf(stderr, "Failed writing run index.html in %s\n", run_dir);
        return 1;
    }
    if (write_root_index_html(out_dir) != 0) {
        fprintf(stderr, "Failed writing root index.html in %s\n", out_dir);
        return 1;
    }

    printf("done: %zu scenario(s), %dx%d\n", scenarios.size(), g_width, g_height);
    return 0;
}
