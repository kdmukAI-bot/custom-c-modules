# POC C Module (`poc`)

This proof-of-concept MicroPython C module exposes two functions in the REPL:

- `poc.add(a, b)` → returns integer sum
- `poc.ping()` → returns `"pong"`

## Build integration

Use `USER_C_MODULES` pointing to:

`custom-c-modules/usercmodule.cmake`

Example:

```bash
export IDF_PATH="$MP_WORKDIR/toolchains/esp-idf"
. "$IDF_PATH/export.sh"

cd "$MP_WORKDIR/references/micropython/ports/esp32"
make -j$(nproc) \
  BOARD=WAVESHARE_ESP32_S3_TOUCH_LCD_35B \
  BUILD="$MP_WORKDIR/build/WAVESHARE_ESP32_S3_TOUCH_LCD_35B" \
  USER_C_MODULES="$MP_WORKDIR/custom-c-modules/usercmodule.cmake"
```

## REPL smoke test

```python
import poc
print(poc.ping())      # pong
print(poc.add(2, 3))   # 5
```
