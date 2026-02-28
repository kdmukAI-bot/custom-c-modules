#define _POSIX_C_SOURCE 200809L
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

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define DEFAULT_WIDTH_LANDSCAPE 480
#define DEFAULT_HEIGHT_LANDSCAPE 320
#define DEFAULT_WIDTH_PORTRAIT 320
#define DEFAULT_HEIGHT_PORTRAIT 480
#define DEFAULT_OUT_DIR "artifacts/screenshots"
#define DEFAULT_NAME_PREFIX "frame"

typedef enum {
    ORIENT_LANDSCAPE = 0,
    ORIENT_PORTRAIT = 1,
} orientation_t;

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
    uint8_t *crc_buf = NULL;

    write_u32_be(len_be, len);
    if (fwrite(len_be, 1, 4, f) != 4) return -1;
    if (fwrite(type, 1, 4, f) != 4) return -1;
    if (len > 0 && fwrite(data, 1, len, f) != len) return -1;

    crc_buf = (uint8_t *)malloc((size_t)len + 4u);
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
    if (fwrite(sig, 1, 8, f) != 8) goto fail;

    uint8_t ihdr[13] = {0};
    write_u32_be(ihdr, (uint32_t)width);
    write_u32_be(ihdr + 4, (uint32_t)height);
    ihdr[8] = 8;
    ihdr[9] = 2;
    ihdr[10] = 0;
    ihdr[11] = 0;
    ihdr[12] = 0;

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

static void render_frame(uint8_t *rgb, int w, int h, int frame_idx) {
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            size_t i = ((size_t)y * (size_t)w + (size_t)x) * 3u;
            rgb[i + 0] = (uint8_t)((x + frame_idx * 17) % 256);
            rgb[i + 1] = (uint8_t)((y + frame_idx * 29) % 256);
            rgb[i + 2] = (uint8_t)(((x ^ y) + frame_idx * 11) % 256);
        }
    }
}

static void usage(const char *argv0) {
    printf("Usage: %s [options]\n", argv0);
    printf("  --out-dir <path>           Output root (default: %s)\n", DEFAULT_OUT_DIR);
    printf("  --width <px>               Frame width\n");
    printf("  --height <px>              Frame height\n");
    printf("  --orientation <o>          portrait|landscape (default: landscape)\n");
    printf("  --name-prefix <prefix>     PNG filename prefix (default: %s)\n", DEFAULT_NAME_PREFIX);
    printf("  --frames <n>               Number of PNG files to emit (default: 3)\n");
    printf("  --help                     Show this help\n");
}

int main(int argc, char **argv) {
    const char *out_dir = DEFAULT_OUT_DIR;
    const char *name_prefix = DEFAULT_NAME_PREFIX;
    orientation_t orient = ORIENT_LANDSCAPE;
    int frames = 3;
    int width = -1;
    int height = -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--out-dir") == 0 && i + 1 < argc) {
            out_dir = argv[++i];
        } else if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) {
            width = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) {
            height = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--orientation") == 0 && i + 1 < argc) {
            const char *o = argv[++i];
            if (strcmp(o, "portrait") == 0) orient = ORIENT_PORTRAIT;
            else if (strcmp(o, "landscape") == 0) orient = ORIENT_LANDSCAPE;
            else {
                fprintf(stderr, "Invalid orientation: %s\n", o);
                return 2;
            }
        } else if (strcmp(argv[i], "--name-prefix") == 0 && i + 1 < argc) {
            name_prefix = argv[++i];
        } else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            frames = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            usage(argv[0]);
            return 2;
        }
    }

    if (width <= 0) {
        width = (orient == ORIENT_PORTRAIT) ? DEFAULT_WIDTH_PORTRAIT : DEFAULT_WIDTH_LANDSCAPE;
    }
    if (height <= 0) {
        height = (orient == ORIENT_PORTRAIT) ? DEFAULT_HEIGHT_PORTRAIT : DEFAULT_HEIGHT_LANDSCAPE;
    }
    if (frames <= 0 || frames > 1000 || width <= 0 || height <= 0) {
        fprintf(stderr, "Invalid dimensions/frames\n");
        return 2;
    }

    if (mkdir_p(out_dir) != 0) {
        fprintf(stderr, "Failed to create out dir '%s': %s\n", out_dir, strerror(errno));
        return 1;
    }

    time_t now = time(NULL);
    struct tm tm_utc;
    if (gmtime_r(&now, &tm_utc) == NULL) {
        fprintf(stderr, "gmtime_r failed\n");
        return 1;
    }

    char ts[32];
    if (strftime(ts, sizeof(ts), "%Y-%m-%dT%H-%M-%SZ", &tm_utc) == 0) {
        fprintf(stderr, "strftime failed\n");
        return 1;
    }

    char run_dir[PATH_MAX];
    snprintf(run_dir, sizeof(run_dir), "%s/%s", out_dir, ts);
    if (mkdir_p(run_dir) != 0) {
        fprintf(stderr, "Failed to create run dir '%s': %s\n", run_dir, strerror(errno));
        return 1;
    }

    const size_t pixels = (size_t)width * (size_t)height;
    uint8_t *rgb = (uint8_t *)malloc(pixels * 3u);
    if (!rgb) {
        fprintf(stderr, "Allocation failed\n");
        return 1;
    }

    for (int i = 0; i < frames; i++) {
        render_frame(rgb, width, height, i);
        char out_png[PATH_MAX];
        snprintf(out_png, sizeof(out_png), "%s/%s_%03d.png", run_dir, name_prefix, i + 1);
        if (write_png_rgb24(out_png, rgb, width, height) != 0) {
            fprintf(stderr, "Failed writing PNG: %s\n", out_png);
            free(rgb);
            return 1;
        }
        printf("wrote %s\n", out_png);
    }
    free(rgb);

    char latest_link[PATH_MAX];
    snprintf(latest_link, sizeof(latest_link), "%s/latest", out_dir);
    unlink(latest_link);
    if (symlink(ts, latest_link) != 0) {
        char latest_txt[PATH_MAX];
        snprintf(latest_txt, sizeof(latest_txt), "%s/latest_path.txt", out_dir);
        FILE *lf = fopen(latest_txt, "w");
        if (!lf) {
            fprintf(stderr, "Failed to update latest pointer\n");
            return 1;
        }
        fprintf(lf, "%s\n", run_dir);
        fclose(lf);
        printf("symlink unavailable, wrote fallback pointer: %s\n", latest_txt);
    } else {
        printf("updated latest -> %s\n", ts);
    }

    printf("done: %d frame(s), %dx%d, orientation=%s\n", frames, width, height,
           orient == ORIENT_PORTRAIT ? "portrait" : "landscape");
    return 0;
}
