/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test for firmware/upd_screen.h, the FM-1's screen while firmware is installed: every screen the
   firmware (fm1_recovery.h) and the update loader (loader/loader.c) draw, on a pretend 240x240 LCD. Checks
   that every text fits the font and the width, the bar grows with the percentage and never leaves its frame,
   only the bar and number move between progress steps, and nothing is drawn once the LCD has timed out.
   With a folder: each screen as a PPM. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static uint16_t lcd[240 * 240];                 /* plain RGB565, as the panel shows it */
static uint32_t fills, blits, syncs, timeouts;
static void lcd_sync(void) { syncs++; }
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    fills++;
    if (!w || !h) return;
    assert(x + w <= 240u && y + h <= 240u);
    for (uint32_t j = y; j < y + h; j++) for (uint32_t i = x; i < x + w; i++) lcd[j * 240u + i] = c;
}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    blits++;
    assert(x + w <= 240u && y + h <= 240u);
    for (uint32_t j = 0; j < h; j++) for (uint32_t i = 0; i < w; i++) {
        uint16_t v = px[j * w + i];
        lcd[(y + j) * 240u + x + i] = (uint16_t)(v >> 8 | v << 8);   /* the strip is byte-swapped */
    }
}
static uint16_t strip[240u * 21u + 64u];
#define UPD_STRIP strip
#define UPD_OK() (timeouts == 0u)
#include "upd_screen.h"

static void fits(const char *t, uint32_t scale)
{
    assert(strlen(t) * 6u * scale - scale <= 240u);
    for (const char *c = t; *c; c++) assert(strchr(lsdj_ov_chars, *c));
}
static uint32_t count(uint16_t c, uint32_t y0, uint32_t y1)
{
    uint32_t n = 0;
    for (uint32_t i = y0 * 240u; i < y1 * 240u; i++) n += lcd[i] == c;
    return n;
}
/* the bar's filled width, along its middle row */
static uint32_t bar_px(void)
{
    uint32_t n = 0, y = UPD_BAR_Y + UPD_BAR_H / 2u;
    for (uint32_t x = 0; x < 240u; x++) n += lcd[y * 240u + x] == UPD_ACCENT;
    return n;
}
static const char *dir;
static void shot(const char *name)
{
    if (!dir) return;
    char path[512];
    snprintf(path, sizeof path, "%s/%s.ppm", dir, name);
    FILE *f = fopen(path, "wb");
    assert(f);
    fprintf(f, "P6 240 240 255\n");
    for (uint32_t i = 0; i < 240u * 240u; i++) {
        uint16_t c = lcd[i];
        uint8_t rgb[3] = {(uint8_t)((c >> 11) << 3), (uint8_t)(((c >> 5) & 63u) << 2), (uint8_t)((c & 31u) << 3)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    dir = argc > 1 ? argv[1] : NULL;
    /* every text the two programs draw (fm1_recovery.h, loader.c) */
    const char *small[] = {"PREPARING. NOTHING IS CHANGED YET", "READY. RESTARTING INTO UPDATE MODE",
        "STOPPED. NOTHING WAS CHANGED", "THE FM-1 CARRIES ON AS BEFORE", "UPDATE MODE: WAITING FOR THE INSTALLER",
        "CHECKING THE NEW FIRMWARE", "WRITING THE NEW FIRMWARE", "DONE. RESTARTING", "STOPPED. TRY THE INSTALL AGAIN",
        "KEEP THE USB CABLE IN AND THE", "INSTALLER OPEN UNTIL IT SAYS DONE"};
    for (unsigned i = 0; i < sizeof small / sizeof small[0]; i++) fits(small[i], 1u);
    fits("INSTALLING", 3u); fits("FIRMWARE", 3u); fits("DO NOT UNPLUG", 3u); fits("CODE -13", 2u); fits("100%", 2u);

    /* the loader waiting: no bar, the warning in red */
    upd_screen("UPDATE MODE: WAITING FOR THE INSTALLER", UPD_NO_BAR, 0);
    assert(lcd[0] == UPD_BG && bar_px() == 0 && count(UPD_DIM, UPD_BAR_Y, UPD_BAR_Y + UPD_BAR_H) == 0);
    assert(count(UPD_WARN, 168, 189) > 300 && count(UPD_WARN, 0, 168) == 0);
    assert(count(UPD_ACCENT, 86, 93) > 100);
    shot("1-waiting");

    /* progress: the bar grows, never past its frame; between steps only the bar and the number are drawn */
    uint32_t last = 0;
    for (int32_t p = 0; p <= 40; p++) {
        uint32_t f = fills, b = blits;
        upd_screen("CHECKING THE NEW FIRMWARE", p, 0);
        if (p > 0) assert(fills - f <= 1u && blits - b == 1u);   /* one fill (the bar) and one line (the number) */
        uint32_t w = bar_px();
        assert(w == (UPD_BAR_W - 4u) * (uint32_t)p / 100u && w >= last);
        last = w;
        assert(lcd[UPD_BAR_Y * 240u + UPD_BAR_X] == UPD_DIM && lcd[(UPD_BAR_Y + 2u) * 240u + UPD_BAR_X + 1u] == UPD_DIM);
    }
    { uint32_t f = fills, b = blits; upd_screen("CHECKING THE NEW FIRMWARE", 40, 0); assert(fills == f && blits == b); }
    shot("2-checking");
    upd_screen("WRITING THE NEW FIRMWARE", 73, 0);
    assert(bar_px() == (UPD_BAR_W - 4u) * 73u / 100u);
    assert(count(UPD_TEXT, UPD_BAR_Y + UPD_BAR_H + 10u, UPD_BAR_Y + UPD_BAR_H + 24u) > 50);   /* "73%" */
    shot("3-writing");
    upd_screen("WRITING THE NEW FIRMWARE", 250, 0);
    assert(bar_px() == UPD_BAR_W - 4u);                         /* 100 at most */
    upd_screen("DONE. RESTARTING", 100, 0);
    shot("4-done");

    /* stopped in the loader: no bar, the code in red, the warning kept */
    upd_screen("STOPPED. TRY THE INSTALL AGAIN", UPD_NO_BAR, 0);
    upd_number(0, -13);
    assert(bar_px() == 0 && count(UPD_WARN, UPD_BAR_Y + UPD_BAR_H + 10u, UPD_BAR_Y + UPD_BAR_H + 24u) > 50);
    assert(count(UPD_WARN, 168, 189) > 300);
    shot("5-loader-stopped");

    /* the firmware preparing (steps 1..4 = 0..75 %), then stopped: a note instead of the warning */
    upd_screen("PREPARING. NOTHING IS CHANGED YET", 50, 0);
    assert(bar_px() == (UPD_BAR_W - 4u) / 2u);
    shot("6-preparing");
    upd_screen("STOPPED. NOTHING WAS CHANGED", UPD_NO_BAR, "THE FM-1 CARRIES ON AS BEFORE");
    upd_number(0, -5);
    assert(count(UPD_WARN, 160, 240) == 0 && count(UPD_TEXT, 180, 187) > 100);
    shot("7-prepare-stopped");

    /* the strip: never written past one scale-3 line */
    for (unsigned i = 240u * 21u; i < sizeof strip / sizeof strip[0]; i++) assert(strip[i] == 0);

    /* after an LCD timeout the loader draws nothing more */
    timeouts = 1;
    { uint32_t f = fills, b = blits, s = syncs;
      upd_screen("WRITING THE NEW FIRMWARE", 90, 0); upd_number(0, -9); upd_line(0, "X", UPD_TEXT, 1u);
      assert(fills == f && blits == b && syncs == s); }

    printf("PASS upd_screen: install screens (waiting, checking, writing, done, stopped x2), bar, number, LCD timeout\n");
    return 0;
}
