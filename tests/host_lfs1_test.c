/* SPDX-License-Identifier: MIT
 * Host check of the vendored littlefs v1 reader against a volume written by the same
 * library with the Adafruit InternalFS geometry MeshCore uses (128 B blocks, 224 blocks).
 *   cc -I src/lfs1 -o /tmp/t tests/host_lfs1_test.c src/lfs1/lfs1.c src/lfs1/lfs1_util.c && /tmp/t
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lfs1.h"

#define BS 128
#define BC ((7 * 4096) / BS)
static uint8_t disk[BC * BS];
static int rd(const struct lfs1_config *c, lfs1_block_t b, lfs1_off_t o, void *p, lfs1_size_t n) { memcpy(p, disk + b * BS + o, n); return 0; }
static int pg(const struct lfs1_config *c, lfs1_block_t b, lfs1_off_t o, const void *p, lfs1_size_t n) { memcpy(disk + b * BS + o, p, n); return 0; }
static int er(const struct lfs1_config *c, lfs1_block_t b) { memset(disk + b * BS, 0xFF, BS); return 0; }
static int sy(const struct lfs1_config *c) { return 0; }
static uint8_t r[BS], w[BS], la[16], fb[BS];
static const struct lfs1_config cfg = { .read = rd, .prog = pg, .erase = er, .sync = sy,
	.read_size = BS, .prog_size = BS, .block_size = BS, .block_count = BC, .lookahead = 128,
	.read_buffer = r, .prog_buffer = w, .lookahead_buffer = la, .file_buffer = fb };

static void put(lfs1_t *l, const char *path, const void *d, size_t n) {
	lfs1_file_t f;
	if (lfs1_file_open(l, &f, path, LFS1_O_WRONLY | LFS1_O_CREAT | LFS1_O_TRUNC)) { printf("open %s failed\n", path); exit(1); }
	if (lfs1_file_write(l, &f, d, n) != (lfs1_ssize_t)n) { printf("write %s failed\n", path); exit(1); }
	lfs1_file_close(l, &f);
}

int main(void) {
	static lfs1_t l;
	static uint8_t big[4352], id[96], got[8192];
	const char *json = "{\"name\":\"RPT\",\"radio\":{\"freq\":869.525,\"bw\":250,\"sf\":11,\"cr\":5}}";
	memset(disk, 0xFF, sizeof disk);
	for (int i = 0; i < 96; i++) id[i] = i * 7 + 1;
	for (size_t i = 0; i < sizeof big; i++) big[i] = (uint8_t)(i * 31);
	if (lfs1_format(&l, &cfg) || lfs1_mount(&l, &cfg)) { puts("format/mount failed"); return 1; }
	put(&l, "/_main.id", id, 96);
	put(&l, "/prefs.json", json, strlen(json));
	put(&l, "/s_contacts", big, sizeof big);
	/* rewrite prefs a few times: littlefs v1 moves/deorphans metadata like a real device */
	for (int i = 0; i < 40; i++) { lfs1_remove(&l, "/prefs.json"); put(&l, "/prefs.json", json, strlen(json)); }
	lfs1_unmount(&l);

	/* re-open read-only: prog/erase must never be needed */
	static const struct lfs1_config ro = { .read = rd, .prog = NULL, .erase = NULL, .sync = sy,
		.read_size = BS, .prog_size = BS, .block_size = BS, .block_count = BC, .lookahead = 128,
		.read_buffer = r, .prog_buffer = w, .lookahead_buffer = la, .file_buffer = fb };
	if (lfs1_mount(&l, &ro)) { puts("ro mount failed"); return 1; }
	struct lfs1_info in; lfs1_file_t f;
	if (lfs1_stat(&l, "/_main.id", &in) || in.size != 96) { puts("stat id failed"); return 1; }
	lfs1_file_open(&l, &f, "/_main.id", LFS1_O_RDONLY); lfs1_file_read(&l, &f, got, 96); lfs1_file_close(&l, &f);
	if (memcmp(got, id, 96)) { puts("id mismatch"); return 1; }
	if (lfs1_stat(&l, "/s_contacts", &in) || in.size != sizeof big) { puts("stat acl failed"); return 1; }
	lfs1_file_open(&l, &f, "/s_contacts", LFS1_O_RDONLY); lfs1_file_read(&l, &f, got, sizeof big); lfs1_file_close(&l, &f);
	if (memcmp(got, big, sizeof big)) { puts("acl mismatch"); return 1; }
	if (lfs1_stat(&l, "/regions2", &in) == 0) { puts("phantom file"); return 1; }
	puts("PASS: littlefs v1 read-only reader OK");
	return 0;
}
