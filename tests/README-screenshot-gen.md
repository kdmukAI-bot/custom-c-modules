# screenshot_gen (LVGL SeedSigner screenshot generator)

Generates deterministic PNG screenshots from **real LVGL SeedSigner screen render paths** (not synthetic test patterns).

## Defaults

- Resolution: `480x320` (landscape)
- Output root: `artifacts/screenshots`
- Scenarios: `main_menu,button_list_screen,button_list_1_item,button_list_4_items,button_list_scroll_many`, `button_list_long_title`

## Build

From repo root:

```bash
set -e
LVGL_ROOT=references/micropython/ports/esp32/managed_components/lvgl__lvgl
rm -rf custom-c-modules/tests/.build-screenshot && mkdir -p custom-c-modules/tests/.build-screenshot

find "$LVGL_ROOT/src" -name '*.c' > custom-c-modules/tests/.build-screenshot/lvgl_sources.txt
while IFS= read -r f; do
  obj="custom-c-modules/tests/.build-screenshot/$(echo "$f" | sed 's#[/ ]#_#g').o"
  cc -O2 -std=c11 -DLV_CONF_SKIP -I"$LVGL_ROOT" -c "$f" -o "$obj"
done < custom-c-modules/tests/.build-screenshot/lvgl_sources.txt

for f in \
  custom-c-modules/components/seedsigner/fonts/opensans_semibold_20_4bpp_125x.c \
  custom-c-modules/components/seedsigner/fonts/opensans_semibold_18_4bpp_125x.c \
  custom-c-modules/components/seedsigner/fonts/opensans_regular_17_4bpp_125x.c \
  custom-c-modules/components/seedsigner/fonts/seedsigner_icons_24_4bpp_125x.c \
  custom-c-modules/components/seedsigner/fonts/seedsigner_icons_36_4bpp_125x.c; do
  obj="custom-c-modules/tests/.build-screenshot/$(echo "$f" | sed 's#[/ ]#_#g').o"
  cc -O2 -std=c11 -DLV_CONF_SKIP -I"$LVGL_ROOT" -c "$f" -o "$obj"
done

g++ -O2 -std=c++17 -DLV_CONF_SKIP -I"$LVGL_ROOT" -Icustom-c-modules/components/seedsigner -c custom-c-modules/components/seedsigner/components.cpp -o custom-c-modules/tests/.build-screenshot/components.o
g++ -O2 -std=c++17 -DLV_CONF_SKIP -I"$LVGL_ROOT" -Icustom-c-modules/components/seedsigner -c custom-c-modules/components/seedsigner/seedsigner.cpp -o custom-c-modules/tests/.build-screenshot/seedsigner.o
g++ -O2 -std=c++17 -DLV_CONF_SKIP -I"$LVGL_ROOT" -Icustom-c-modules/components/seedsigner -c custom-c-modules/tests/screenshot_gen.cpp -o custom-c-modules/tests/.build-screenshot/screenshot_gen.o

objs=$(find custom-c-modules/tests/.build-screenshot -name '*.o' | tr '\n' ' ')
g++ $objs -o custom-c-modules/tests/screenshot_gen
```

## Usage

```bash
custom-c-modules/tests/screenshot_gen [options]
```

Options:

- `--out-dir <path>` output root (default `artifacts/screenshots`)
- `--width <px>` (default `480`)
- `--height <px>` (default `320`)
- `--scenarios <list>` comma-separated scenario names
  - supported: `main_menu`, `button_list_screen`, `button_list_buttonless`, `button_list_1_item`, `button_list_4_items`, `button_list_scroll_many`, `button_list_long_title`

Output policy:

- Creates run dir: `<out-dir>/YYYY-MM-DDTHH-MM-SSZ/`
- Writes one PNG per scenario, e.g. `main_menu.png`
- Updates `<out-dir>/latest` symlink to the timestamp dir
- If symlink fails, writes fallback pointer file `<out-dir>/latest_path.txt`
