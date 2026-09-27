/*
 * The in-game randomizer's code, for RANDOMIZER=1 builds only: the Makefile includes
 * this in the linker script in place of the padding at the end of the ROM, and pads
 * the ROM back to 32MB afterwards.
 *
 * Everything the randomizer adds lives here, in two fragments of its own that the pick
 * screen (fragment61) and the battle-select screen (fragment64) load when they start.
 * Nothing before this point moves, so the first megabyte of the ROM, and with it the
 * checksum in the header, stays exactly as in the original game.
 *
 * Their headers and relocation tables are generated at link time
 * (tools/fragment_relocs.py, yamls/us/fragment_regen_randomizer.txt).
 */

    randomizer_pick_ROM_START = __romPos;
    randomizer_pick_VRAM = ADDR(.randomizer_pick);
    .randomizer_pick 0x8C000000 : AT(randomizer_pick_ROM_START) SUBALIGN(16)
    {
        FILL(0x00000000);
        randomizer_pick_TEXT_START = .;
        build/randomizer/randomizer_pick_header.o(.text);
        build/src/fragments/61/randomizer_pick.o(.text);
        build/src/fragments/61/randomizer.o(.text);
        build/src/fragments/61/randomizer_logic.o(.text);
        build/src/fragments/61/randomizer_panel.o(.text);
        build/src/fragments/61/randomizer_data.o(.text);
        randomizer_pick_TEXT_END = .;
        randomizer_pick_DATA_START = .;
        build/src/fragments/61/randomizer_pick.o(.data);
        build/src/fragments/61/randomizer.o(.data);
        build/src/fragments/61/randomizer_logic.o(.data);
        build/src/fragments/61/randomizer_panel.o(.data);
        build/src/fragments/61/randomizer_data.o(.data);
        randomizer_pick_DATA_END = .;
        randomizer_pick_RODATA_START = .;
        build/src/fragments/61/randomizer_pick.o(.rodata);
        build/src/fragments/61/randomizer.o(.rodata);
        build/src/fragments/61/randomizer_logic.o(.rodata);
        build/src/fragments/61/randomizer_panel.o(.rodata);
        build/src/fragments/61/randomizer_data.o(.rodata);
        . = ALIGN(16);
        randomizer_pick_RODATA_END = .;
    }
    .randomizer_pick_bss (NOLOAD) : SUBALIGN(16)
    {
        build/src/fragments/61/randomizer_pick.o(.bss);
        build/src/fragments/61/randomizer.o(.bss);
        build/src/fragments/61/randomizer_logic.o(.bss);
        build/src/fragments/61/randomizer_panel.o(.bss);
        build/src/fragments/61/randomizer_data.o(.bss);
    }
    __romPos += SIZEOF(.randomizer_pick);
    randomizer_pick_ROM_END = __romPos;
    __romPos = ALIGN(__romPos, 16);

    randomizer_pick_relocs_ROM_START = __romPos;
    .randomizer_pick_relocs 0x0 : AT(randomizer_pick_relocs_ROM_START) SUBALIGN(16)
    {
        FILL(0x00000000);
        build/randomizer/randomizer_pick_reloc.o(.rodata);
    }
    __romPos += SIZEOF(.randomizer_pick_relocs);
    randomizer_pick_relocs_ROM_END = __romPos;
    __romPos = ALIGN(__romPos, 16);

    randomizer_battle_ROM_START = __romPos;
    randomizer_battle_VRAM = ADDR(.randomizer_battle);
    .randomizer_battle 0x8C100000 : AT(randomizer_battle_ROM_START) SUBALIGN(16)
    {
        FILL(0x00000000);
        randomizer_battle_TEXT_START = .;
        build/randomizer/randomizer_battle_header.o(.text);
        build/src/fragments/64/randomizer_battle.o(.text);
        randomizer_battle_TEXT_END = .;
        randomizer_battle_DATA_START = .;
        build/src/fragments/64/randomizer_battle.o(.data);
        randomizer_battle_DATA_END = .;
        randomizer_battle_RODATA_START = .;
        build/src/fragments/64/randomizer_battle.o(.rodata);
        . = ALIGN(16);
        randomizer_battle_RODATA_END = .;
    }
    .randomizer_battle_bss (NOLOAD) : SUBALIGN(16)
    {
        build/src/fragments/64/randomizer_battle.o(.bss);
    }
    __romPos += SIZEOF(.randomizer_battle);
    randomizer_battle_ROM_END = __romPos;
    __romPos = ALIGN(__romPos, 16);

    randomizer_battle_relocs_ROM_START = __romPos;
    .randomizer_battle_relocs 0x0 : AT(randomizer_battle_relocs_ROM_START) SUBALIGN(16)
    {
        FILL(0x00000000);
        build/randomizer/randomizer_battle_reloc.o(.rodata);
    }
    __romPos += SIZEOF(.randomizer_battle_relocs);
    randomizer_battle_relocs_ROM_END = __romPos;
    __romPos = ALIGN(__romPos, 16);

    ASSERT(__romPos <= 0x2000000, "The randomizer doesn't fit in the free space at the end of the ROM")
