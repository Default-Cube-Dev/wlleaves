#define NOB_IMPLEMENTATION
#include "../nob.h"

int main(int argc, char* argv[]) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    Nob_Cmd cmd = {0};

    nob_cmd_append(&cmd, "clang");
    nob_cmd_append(&cmd, "-Wall", "-Wextra", "-ggdb");
    nob_cmd_append(&cmd, "-ldrm", "-lgbm", "-lGLESv2", "-lEGL", "-linput", "-ludev");
    nob_cmd_append(&cmd, "-I", "/usr/include/drm/");
    nob_cmd_append(&cmd, "./main.c");
    nob_cmd_append(&cmd, "-o", "main");

    if (!nob_cmd_run(&cmd)) return 1;
    return 0;
}
