/* SPDX-License-Identifier: GPL-3.0-only */
/* The FM-1's screen while firmware is installed: what is going on, a progress bar, and DO NOT UNPLUG.
   Two programs draw it:
     - the running firmware while it prepares the update (fm1_recovery.h, ota_show), about 5 s;
     - the update loader while it checks and writes the new firmware (loader/loader.c), the long part.
   Drawn straight to the LCD (lcd.c's lcd_fill and lcd_blit), one line of text at a time through UPD_STRIP:
   a RAM buffer of at least UPD_STRIP_PX pixels that the includer defines (the LCD's DMA reads it). Text uses
   the overlay font (lsdj_overlay.h), so the firmware carries no second font. Host test: tests/upd_screen.c. */
#pragma once
#include <stdint.h>
#include "lsdj_overlay.h"

#define UPD_STRIP_PX (240u * 21u)          /* the tallest line: scale 3 */
#define UPD_BG     0x0843u                 /* plain RGB565 (lcd_fill); text goes through LSDJ_OV_SWAP */
#define UPD_TEXT   0xEF5Eu
#define UPD_DIM    0x8C51u
#define UPD_ACCENT 0xFD20u
#define UPD_WARN   0xFA08u
#define UPD_BAR_X 20u
#define UPD_BAR_Y 100u
#define UPD_BAR_W 200u
#define UPD_BAR_H 24u
#define UPD_NO_BAR (-1)
#ifndef UPD_OK
#define UPD_OK() 1                         /* the loader: 0 after an LCD timeout, then nothing is drawn */
#endif

static const char *upd_status_shown, *upd_note_shown;   /* the screen as drawn (NULL: not yet) */
static int32_t upd_pct_shown;

/* One line of text, centred, scale s, on the background (the whole width, 7 s rows from y). */
static void upd_line(uint32_t y, const char *t, uint16_t fg, uint32_t s)
{
    uint32_t i, n = 240u * 7u * s;
    if (!UPD_OK()) return;
    lcd_sync();                                     /* the last line's transfer may still read the strip */
    for (i = 0; i < n; i++)
        UPD_STRIP[i] = LSDJ_OV_SWAP(UPD_BG);
    lsdj_ov_text_c(UPD_STRIP, 120u, 0u, t, LSDJ_OV_SWAP(fg), s);
    lcd_blit(0u, y, 240u, 7u * s, UPD_STRIP);
    lcd_sync();                                     /* nothing left running (the loader writes flash next) */
}

/* "42%", or "CODE -13" (code != 0), in the line under the bar. */
static void upd_number(int32_t pct, int32_t code)
{
    char t[16];
    uint32_t n = 0, v = (uint32_t)(code ? (code < 0 ? -code : code) : pct), d = 1;
    if (code) {
        const char *c = code < 0 ? "CODE -" : "CODE ";
        while (*c) t[n++] = *c++;
    }
    while (v / d >= 10u) d *= 10u;
    for (; d; d /= 10u) t[n++] = (char)('0' + v / d % 10u);
    if (!code) t[n++] = '%';
    t[n] = 0;
    upd_line(UPD_BAR_Y + UPD_BAR_H + 10u, t, code ? UPD_WARN : UPD_TEXT, 2u);
}

/* The whole screen: status (one small line), the bar at pct 0..100 (UPD_NO_BAR: none), then DO NOT UNPLUG
   and how long to keep everything connected, or (note != NULL) that line instead of the warning. Drawn
   whole when status or note changes; otherwise only the bar and the number move. */
static void upd_screen(const char *status, int32_t pct, const char *note)
{
    if (!UPD_OK()) return;
    if (pct > 100) pct = 100;
    if (status != upd_status_shown || note != upd_note_shown) {
        lcd_fill(0u, 0u, 240u, 240u, UPD_BG);
        upd_line(34u, "INSTALLING", UPD_TEXT, 3u);
        upd_line(60u, "FIRMWARE", UPD_TEXT, 3u);
        upd_line(86u, status, UPD_ACCENT, 1u);
        if (note)
            upd_line(180u, note, UPD_TEXT, 1u);
        else {
            upd_line(168u, "DO NOT UNPLUG", UPD_WARN, 3u);
            upd_line(198u, "KEEP THE USB CABLE IN AND THE", UPD_TEXT, 1u);
            upd_line(210u, "INSTALLER OPEN UNTIL IT SAYS DONE", UPD_TEXT, 1u);
        }
        upd_status_shown = status;
        upd_note_shown = note;
        upd_pct_shown = UPD_NO_BAR;
        if (pct >= 0) {                             /* the bar's frame and empty inside */
            lcd_fill(UPD_BAR_X, UPD_BAR_Y, UPD_BAR_W, UPD_BAR_H, UPD_DIM);
            lcd_fill(UPD_BAR_X + 2u, UPD_BAR_Y + 2u, UPD_BAR_W - 4u, UPD_BAR_H - 4u, UPD_BG);
        }
    }
    if (pct < 0 || pct == upd_pct_shown)
        return;
    if (upd_pct_shown < 0 || pct < upd_pct_shown)   /* (never shrinks within a status; to be safe) */
        lcd_fill(UPD_BAR_X + 2u, UPD_BAR_Y + 2u, UPD_BAR_W - 4u, UPD_BAR_H - 4u, UPD_BG);
    lcd_fill(UPD_BAR_X + 2u, UPD_BAR_Y + 2u, (UPD_BAR_W - 4u) * (uint32_t)pct / 100u, UPD_BAR_H - 4u, UPD_ACCENT);
    lcd_sync();
    upd_pct_shown = pct;
    upd_number(pct, 0);
}
