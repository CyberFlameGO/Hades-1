/******************************************************************************\
**
**  This file is part of the Hades GBA Emulator, and is made available under
**  the terms of the GNU General Public License version 2.
**
**  Copyright (C) 2020 - The Hades Authors
**
\******************************************************************************/

#include <endian.h>
#include "hades.h"
#include "core.h"

/*
** Read the byte at the given address.
*/
uint8_t
core_bus_read8(
    struct core const *core,
    uint32_t addr
) {
    if (addr >= core->memory_size) {
        panic(CORE, "Segmentation fault: invalid read of size 8 at address %#08x", addr);
    } else {
        return (core->memory[addr]);
    }
}

/*
** Read the byte at the given address.
*/
void
core_bus_write8(
    struct core *core,
    uint32_t addr,
    uint8_t val
) {
    if (addr >= core->memory_size) {
        panic(CORE, "Segmentation fault: invalid read of size 8 at address %#08x", addr);
    } else {
        core->memory[addr] = val;
    }
}

/*
** Read the word at the given address, hiding all endianness conversions.
**
** This function returns an `uint32_t` instead of an `uint16_t` to account for
** some of the shenanigans the ARM7TDMI does when supplied an unligned address.
*/
uint32_t
core_bus_read16(
    struct core const *core,
    uint32_t addr
) {
    uint32_t value;
    uint32_t rotate;

    rotate = (addr % 2) << 3;
    addr &= 0xFFFFFFFE;

    if (addr >= (core->memory_size - 1)) {
        panic(CORE, "Segmentation fault: invalid read of size 16 at address %#08x", addr);
    } else {
        if (core->big_endian) {
            value = be16toh(*(uint16_t *)(core->memory + addr));
        } else {
            value = le16toh(*(uint16_t *)(core->memory + addr));
        }
    }

    /* Unaligned 16-bits loads are supposed to be unpredictable, but in practise the GBA rotates them */
    return ((value >> rotate) | (value << (32 - rotate)));
}

/*
** Read the word at the given address, hiding all endianness conversions.
*/
void
core_bus_write16(
    struct core *core,
    uint32_t addr,
    uint16_t val
) {
    if (addr >= (core->memory_size - 1)) {
        panic(CORE, "Segmentation fault: invalid write of size 16 at address %#08x", addr);
    } else {
        if (core->big_endian) {
            *(uint16_t *)(core->memory + addr) = htobe16(val);
        } else {
            *(uint16_t *)(core->memory + addr) = htole16(val);
        }
    }
}

/*
** Read the double-word at the given address, hiding all endianness conversions.
*/
uint32_t
core_bus_read32(
    struct core const *core,
    uint32_t addr
) {
    uint32_t value;
    uint32_t rotate;

    rotate = (addr % 4) << 3;
    addr &= 0xFFFFFFFE;

    if (addr >= (core->memory_size - 3)) {
        panic(CORE, "Segmentation fault: invalid read of size 32 at address %#08x", addr);
    } else {
        if (core->big_endian) {
            value = be32toh(*(uint32_t *)(core->memory + addr));
        } else {
            value = le32toh(*(uint32_t *)(core->memory + addr));
        }
    }

    /* Unaligned 32-bits loads are rotated */
    return ((value >> rotate) | (value << (32 - rotate)));
}

/*
** Read the double-word at the given address, hiding all endianness conversions.
*/
void
core_bus_write32(
    struct core *core,
    uint32_t addr,
    uint32_t val
) {
    if (addr >= (core->memory_size - 3)) {
        panic(CORE, "Segmentation fault: invalid write of size 32 at address %#08x", addr);
    } else {
        if (core->big_endian) {
            *(uint32_t *)(core->memory + addr) = htobe32(val);
        } else {
            *(uint32_t *)(core->memory + addr) = htole32(val);
        }
    }
}