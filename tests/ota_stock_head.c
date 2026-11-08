/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test for the stock-head rule in vendor/felucca/firmware/src/ota.c: the official loader may
 * replace the FM-1's 16 KiB head only when the FM-1 has V15's head and the package has V16's.
 * Every other differing head is refused (-5) before anything is written.
 * Uses Felucca's fake M-UPGRADE host and flash (tests/ota_test.c).
 *   ota_stock_head FM-1_015.fwsc FM-1_016.fwsc   (M-VAVE's stock packages: never in the repos) */
#define main felucca_ota_test_main
#include "../vendor/felucca/tests/ota_test.c"
#undef main

static uint8_t *load(const char *path, size_t *len)   /* the logical image, as ota_test.c reads it */
{
    FILE *f = fopen(path, "rb");
    uint8_t *raw, *img;
    size_t n, i;
    if (!f) { perror(path); exit(2); }
    raw = malloc(0x200000);
    n = fread(raw, 1, 0x200000, f);
    fclose(f);
    if (n < 20 * 48 + 0x4400) { printf("%s: too short for a package\n", path); exit(2); }
    img = malloc(n);
    for (i = 0; i < 20; i++) memcpy(img + i * 47, raw + i * 48, 47);
    memcpy(img + 20 * 47, raw + 20 * 48, n - 20 * 48);
    free(raw);
    *len = n - 20;
    return img;
}

static uint8_t *v15, *v16;
static size_t v15_len, v16_len;

/* The FM-1's head is dev_head (flash.bin at 0x400 in both packages); M-UPGRADE serves pkg. */
static int session(const uint8_t *dev_head, uint8_t *pkg, size_t pkg_len, uint8_t *flip)
{
    uint8_t before[0x4000];
    int rc;
    logical = pkg; logical_len = pkg_len;
    reset_dev();
    memcpy(nor, dev_head, 0x4000);
    memcpy(before, nor, 0x4000);
    corrupt_at = flip;
    rc = ota_session();
    if (memcmp(before, nor, 0x4000) || bad_erase) { printf("  wrote outside the staging area\n"); return -100; }
    return rc;
}

static int refused(int rc) { return rc == -5 && last_code == -5 && !committed && area_erased(); }

int main(int argc, char **argv)
{
    uint8_t h15[0x4000], h16[0x4000], odd[0x4000];
    int bad = 0;
    if (argc < 3) { printf("usage: ota_stock_head FM-1_015.fwsc FM-1_016.fwsc\n"); return 2; }
    v15 = load(argv[1], &v15_len);
    v16 = load(argv[2], &v16_len);
    memcpy(h15, v15 + 0x400, sizeof h15);
    memcpy(h16, v16 + 0x400, sizeof h16);
    bad += check("V15 head pinned (CRC-32, CRC-16)",
                 (ota_crc32(h15, sizeof h15, 0xFFFFFFFFu) ^ 0xFFFFFFFFu) == OTA_HEAD_V15_CRC32 &&
                 ota_crc16(h15, sizeof h15, 0) == OTA_HEAD_V15_CRC16);
    bad += check("V16 head pinned (CRC-32, CRC-16)",
                 (ota_crc32(h16, sizeof h16, 0xFFFFFFFFu) ^ 0xFFFFFFFFu) == OTA_HEAD_V16_CRC32 &&
                 ota_crc16(h16, sizeof h16, 0) == OTA_HEAD_V16_CRC16);
    bad += check("the heads differ", memcmp(h15, h16, sizeof h15) != 0);

    bad += check("V15 FM-1 -> V16: commits (new)", session(h15, v16, v16_len, NULL) == 0 && committed);
    bad += check("V16 FM-1 -> V16: commits", session(h16, v16, v16_len, NULL) == 0 && committed);
    bad += check("V15 FM-1 -> V15: commits", session(h15, v15, v15_len, NULL) == 0 && committed);
    bad += check("V16 FM-1 -> V15: refused, nothing kept", refused(session(h16, v15, v15_len, NULL)));

    memcpy(odd, h15, sizeof odd); odd[0x2000] ^= 0x01;
    bad += check("V15 head off by one bit -> V16: refused", refused(session(odd, v16, v16_len, NULL)));
    memcpy(odd, h15, sizeof odd); odd[0x3FFF] ^= 0x80;
    bad += check("V15 head, last byte off -> V16: refused", refused(session(odd, v16, v16_len, NULL)));
    bad += check("V15 FM-1, V16 head damaged in transit: refused",
                 refused(session(h15, v16, v16_len, v16 + 0x400 + 0x1234)));
    memset(odd, 0xFF, sizeof odd);
    bad += check("blank head -> V16: refused", refused(session(odd, v16, v16_len, NULL)));

    printf("%s\n", bad ? "OTA STOCK HEAD TEST FAILED" : "ota_stock_head: ok");
    return bad != 0;
}
