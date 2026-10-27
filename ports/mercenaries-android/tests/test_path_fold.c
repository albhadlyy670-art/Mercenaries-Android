/* The game asks for files in arbitrary case; the importer wrote lower-case names (Android is case-sensitive). */
#include "kernel/kernel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
static int bad;
#define CHECK(c, ...) do { if (!(c)) { bad++; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)
int main(void)
{
    char game[] = "/tmp/pathfoldXXXXXX", save[256], host[512], want[600];
    if (!mkdtemp(game)) return 2; snprintf(save, sizeof(save), "%s_saves", game);
    char p[600]; snprintf(p, sizeof(p), "%s/media", game); mkdir(p, 0775); snprintf(p, sizeof(p), "%s/media/intro.xmv", game); FILE *f = fopen(p, "w"); fputs("x", f); fclose(f);
    xbox_path_init(game, save);
    const char *reqs[] = { "D:\\Media\\Intro.XMV", "d:\\MEDIA\\intro.xmv", "\\Device\\CdRom0\\Media\\INTRO.xmv" };
    for (int i = 0; i < 3; ++i) {
        CHECK(xbox_translate_path(reqs[i], host, sizeof(host)), "translate %s", reqs[i]);
        snprintf(want, sizeof(want), "%s/media/intro.xmv", game);
        CHECK(!strcmp(host, want), "%s -> %s (want %s)", reqs[i], host, want);
        CHECK(access(host, R_OK) == 0, "the translated path of %s exists on a case-sensitive file system", reqs[i]);
    }
    snprintf(want, sizeof(want), "%s/TitleData/saves/slot1/savegame.dat", save);
    CHECK(xbox_translate_path("T:\\Saves\\Slot1\\SaveGame.DAT", host, sizeof(host)) && !strcmp(host, want), "save path folds consistently (got %s, want %s)", host, want);
    printf(bad ? "%d FAILURES\n" : "path case folding: all checks passed\n", bad); return bad != 0;
}
