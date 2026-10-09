#define NOB_IMPLEMENTATION
#include "./nob.h"

#define OBJS_DIR "./objs"

bool make_object(Nob_Cmd* cmd, char* path) {
    Nob_String_Builder out_path = {0};

    Nob_String_View out_filename_sv = nob_sv_from_cstr(nob_path_name(path));
    nob_sv_chop_right(&out_filename_sv, 2);
    const char* out_filename_cstr = nob_temp_sv_to_cstr(out_filename_sv);

    nob_sb_append_cstr(&out_path, OBJS_DIR);
    nob_sb_append_cstr(&out_path, "/");
    nob_sb_append_cstr(&out_path, out_filename_cstr);
    nob_sb_append_cstr(&out_path, ".o");
    const char* out_path_cstr = nob_temp_sv_to_cstr(nob_sb_to_sv(out_path));

    if (nob_needs_rebuild1(out_path_cstr, path) || !nob_file_exists(out_path_cstr)) {
        nob_log(INFO, "building %s\n", path);
        Nob_Cmd cmd = {0};
        nob_cmd_append(&cmd, "clang", "-c");
        nob_cmd_append(&cmd, "-Wall", "-Wextra", "-ggdb");
        nob_cmd_append(&cmd, "-fPIC");
        nob_cmd_append(&cmd, "-o", out_path_cstr);
        nob_cmd_append(&cmd, path);
        if (!nob_cmd_run(&cmd)) return false;
    }

    return true;
}

int main(int argc, char *argv[]) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    Nob_Cmd cmd = {0};

    nob_mkdir_if_not_exists(OBJS_DIR);
    make_object(&cmd, "./src/wlleaves.c");

    nob_cmd_append(&cmd, "clang", "-shared");
    nob_cmd_append(&cmd, "-o", "libwlleaves.so");
    nob_cmd_append(&cmd, "./objs/wlleaves.o");
    if (!nob_cmd_run(&cmd)) return 1;

    if (argc > 1) {
        if (strcmp(argv[1], "-e") == 0) {
            nob_cmd_append(&cmd, "clang");
            nob_cmd_append(&cmd, "-Wall", "-Wextra", "-ggdb");
            nob_cmd_append(&cmd, "-L", ".", "-rpath", ".");
            nob_cmd_append(&cmd, "-lfalsetype");
            nob_cmd_append(&cmd, "./test/main.c");
            nob_cmd_append(&cmd, "-o", "./test/main");

            if (!nob_cmd_run(&cmd)) return 1;
        }
        return 0;
    }
}
