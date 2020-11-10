/******************************************************************************\
**
**  This file is part of the Hades GBA Emulator, and is made available under
**  the terms of the GNU General Public License version 2.
**
**  Copyright (C) 2020 - The Hades Authors
**
\******************************************************************************/

#include "debugger.h"
#include "hades.h"

void
debugger_cmd_step(
    struct debugger *debugger,
    size_t argc,
    char const * const *argv
) {
    if (argc == 1) {
        core_step(debugger->core);
        debugger_dump_context(debugger);
    } else if (argc == 2) {
        unsigned long nb;

        nb = strtoul(argv[1], NULL, 0);
        while (nb > 0) {
            core_step(debugger->core);
            --nb;
        }
        debugger_dump_context(debugger);
    } else {
        printf("Usage: %s\n", g_commands[CMD_STEP].usage);
    }
}