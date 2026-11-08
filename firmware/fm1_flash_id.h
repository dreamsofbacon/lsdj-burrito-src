/* SPDX-License-Identifier: GPL-3.0-only
 * The FM-1 flash chips the firmware will write to (JEDEC ID, command 9Fh):
 *   0x856014  Puya P25Q80: the FM-1s up to 2026
 *   0xB36014  UCUN UC25HQ80: newer FM-1s (from late 2026, mostly AliExpress)
 * Both are 1 MiB (0x14) and take the same commands (fm1_flash.h uses only 9F 05 35 06 20 02 0B).
 * Any other answer (a bus fault, an unknown part) keeps the flash read-only: no saves, no updates.
 * Other FM-1 firmwares made the same change: SLOOP 2.8.5, Clipper 1.3.1, Felucca 1.8.2-beta. */
#pragma once
#include <stdint.h>
#define FL_JEDEC_OK(id) ((uint32_t)(id) == 0x856014u || (uint32_t)(id) == 0xB36014u)
