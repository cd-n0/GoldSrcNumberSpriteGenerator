#define NOB_IMPLEMENTATION
#include "nob.h"

int main(int argc, char** argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);
    Nob_Cmd cmd = { 0 };
    nob_cc(&cmd);
    #ifdef __linux__
        nob_cmd_append(&cmd, "-lm");
    #endif
    nob_cmd_append(&cmd, "GoldSrcNumberSpriteGenerator.c");
    nob_cc_flags(&cmd);
    nob_cc_output(&cmd, "GoldSrcNumberSpriteGenerator");
    if (!nob_cmd_run(&cmd)) return 1;
    return 0;
}
