# Latin-1 glyph fallbacks

Original LVGL built-in Montserrat fonts cover ASCII, degree and symbols but not
Spanish accents. These supplemental fonts cover U+00A0–U+00FF at 14, 18, 26 and 30 px,
matching body/artist, notification title and media title. Built-in ASCII/icons and
kerning are retained through font wrappers, without editing managed LVGL sources.
Line metrics include accents to avoid clipping. UI labels remain English.

Generated from LVGL's `Montserrat-Medium.ttf` with `lv_font_conv@1.5.3`, 4 bpp,
uncompressed, no kerning within the small supplemental set. The accompanying
`OFL.txt` contains the source font's SIL Open Font License.

Reproduce from repository root (install the converter outside the repository):

```powershell
# Set $converter to the installed lv_font_conv/lv_font_conv.js.
foreach ($size in @(14, 18, 26, 30)) {
    node $converter --size $size --bpp 4 --format lvgl `
        --font apps/watch-simulator/lvgl/scripts/generators/built_in_font/Montserrat-Medium.ttf `
        --range '0xA0-0xFF' --no-compress --no-kerning --lv-include lvgl.h `
        --lv-font-name "watch_latin_$size" `
        -o "components/watch-ui/fonts/watch_latin_$size.c"
}
```

Android applies bounded NFC normalization before its existing Latin-1 filter and
UTF-8 byte limits: both precomposed `ñ` and `n` + combining tilde become supported
characters. BLE UUIDs/security/framing are unchanged. Emoji and scripts beyond
Latin-1 remain out of scope.

## Validation

- Firmware build, simulator build and headless glyph/layout/animation tests: PASS.
- Android assemble/unit tests/lint and UTF-8/NFC byte-boundary tests: PASS.
- COM9 flash, bounded boot validation, Pixel APK install and diff check: PASS.
- User confirmed Spanish characters display correctly on the physical watch: PASS.
- COM9 monitor stopped cleanly. Existing flash-size, BLE light-sleep and LVGL
  advisory warnings remain. No interface translation or non-Latin-1 script support.
- No commit or push performed.
