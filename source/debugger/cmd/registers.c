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
debugger_cmd_registers(
    struct debugger *debugger,
    size_t argc __unused,
    char const * const *argv __unused
) {
    size_t i;
    struct core *core;

    /* Print the general purpose registers. */

    core = debugger->core;
    for (i = 0; i < 4; ++i) {
        printf(
            LIGHT_GREEN "%3s" RESET ": " LIGHT_MAGENTA "0x%08x" RESET ", "
            LIGHT_GREEN "%3s" RESET ": " LIGHT_MAGENTA "0x%08x" RESET ", "
            LIGHT_GREEN "%3s" RESET ": " LIGHT_MAGENTA "0x%08x" RESET ", "
            LIGHT_GREEN "%3s" RESET ": " LIGHT_MAGENTA "0x%08x" RESET "\n",
            registers_name[i * 4],
            core->registers[i * 4],
            registers_name[i * 4 + 1],
            core->registers[i * 4 + 1],
            registers_name[i * 4 + 2],
            core->registers[i * 4 + 2],
            registers_name[i * 4 + 3],
            core->registers[i * 4 + 3]
        );
    }

    printf("\n");

    /* Print the CPSR and all saved PSRs. */

    printf(
        LIGHT_GREEN "CPSR" RESET ": " LIGHT_MAGENTA "%c%c%c%c%c%c%c" RESET ", %s, (" LIGHT_MAGENTA "0x%08x" RESET ") - %s\n",
        core->cpsr.negative ? 'n' : '-',
        core->cpsr.zero ? 'z' : '-',
        core->cpsr.carry ? 'c' : '-',
        core->cpsr.overflow ? 'v' : '-',
        core->cpsr.irq_disable ? 'i' : '-',
        core->cpsr.fiq_disable ? 'f' : '-',
        core->cpsr.thumb ? 't' : '-',
        core_modes_name[core->cpsr.mode],
        core->cpsr.raw,
        core->big_endian ? "Big endian" : "Little endian"
    );
}