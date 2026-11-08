/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test for firmware/fm1_flash_id.h: both FM-1 flash chips are accepted, nothing else. */
#include <assert.h>
#include <stdio.h>
#include "fm1_flash_id.h"

int main(void) {
    assert(FL_JEDEC_OK(0x856014u));   /* Puya P25Q80 */
    assert(FL_JEDEC_OK(0xB36014u));   /* UCUN UC25HQ80 */
    const uint32_t rejected[] = {0, 0xFFFFFFu, 0xFFFFFFFFu, 0xEB6014u, 0xEF4014u, 0x856015u, 0xB36015u,
                                 0x856013u, 0xB36013u, 0x01856014u, 0x01B36014u, 0x016014u};
    for (unsigned i = 0; i < sizeof rejected / sizeof *rejected; i++) assert(!FL_JEDEC_OK(rejected[i]));
    puts("fm1_flash_id: ok");
    return 0;
}
