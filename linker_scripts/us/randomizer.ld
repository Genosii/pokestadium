/*
 * The in-game randomizer's code, for RANDOMIZER=1 builds only: the Makefile includes
 * this in the linker script in place of the padding at the end of the ROM, and pads
 * the ROM back to 32MB afterwards.
 *
 * Everything the randomizer adds lives here, in fragments of its own that the pick
 * screen (fragment61), the battle-select screen (fragment64), the battle (fragment62),
 * the screen between a cup's battles (fragment63), Rules (fragment55), Options
 * (fragment56), the title screen (fragment36) and the intro (fragment17) load when they
 * start.
 * randomizer_core, the team generator, is shared by the first two, and randomizer_menu,
 * the options panels, by the pick screen, Rules and Options; they're loaded before the
 * screen's own.
 * Nothing before this point moves, so the first megabyte of the ROM, and with it the
 * checksum in the header, stays exactly as in the original game.
 *
 * Their headers and relocation tables are generated at link time
 * (tools/fragment_relocs.py, yamls/us/fragment_regen_randomizer.txt).
 */

    randomizer_core_ROM_START = __romPos;
    randomizer_core_VRAM = ADDR(.randomizer_core);
    .randomizer_core 0x8C300000 : AT(randomizer_core_ROM_START) SUBALIGN(16)
    {
        FILL(0x00000000);
        randomizer_core_TEXT_START = .;
        build/randomizer/randomizer_core_header.o(.text);
        build/src/fragments/61/randomizer_build.o(.text);
        build/src/fragments/61/randomizer_logic.o(.text);
        build/src/fragments/61/randomizer_data.o(.text);
        randomizer_core_TEXT_END = .;
        randomizer_core_DATA_START = .;
        build/src/fragments/61/randomizer_build.o(.data);
        build/src/fragments/61/randomizer_logic.o(.data);
        build/src/fragments/61/randomizer_data.o(.data);
        randomizer_core_DATA_END = .;
        randomizer_core_RODATA_START = .;
        build/src/fragments/61/randomizer_build.o(.rodata);
        build/src/fragments/61/randomizer_logic.o(.rodata);
        build/src/fragments/61/randomizer_data.o(.rodata);
        . = ALIGN(16);
        randomizer_core_RODATA_END = .;
    }
    .randomizer_core_bss (NOLOAD) : SUBALIGN(16)
    {
        build/src/fragments/61/randomizer_build.o(.bss);
        build/src/fragments/61/randomizer_logic.o(.bss);
        build/src/fragments/61/randomizer_data.o(.bss);
    }
    __romPos += SIZEOF(.randomizer_core);
    randomizer_core_ROM_END = __romPos;
    __romPos = ALIGN(__romPos, 16);

    randomizer_core_relocs_ROM_START = __romPos;
    .randomizer_core_relocs 0x0 : AT(randomizer_core_relocs_ROM_START) SUBALIGN(16)
    {
        FILL(0x00000000);
        build/randomizer/randomizer_core_reloc.o(.rodata);
    }
    __romPos += SIZEOF(.randomizer_core_relocs);
    randomizer_core_relocs_ROM_END = __romPos;
    __romPos = ALIGN(__romPos, 16);

    randomizer_pick_ROM_START = __romPos;
    randomizer_pick_VRAM = ADDR(.randomizer_pick);
    .randomizer_pick 0x8C000000 : AT(randomizer_pick_ROM_START) SUBALIGN(16)
    {
        FILL(0x00000000);
        randomizer_pick_TEXT_START = .;
        build/randomizer/randomizer_pick_header.o(.text);
        build/src/fragments/61/randomizer_pick.o(.text);
        build/src/fragments/61/randomizer.o(.text);
        build/src/fragments/61/randomizer_team.o(.text);
        build/src/fragments/61/randomizer_hints.o(.text);
        build/src/fragments/61/randomizer_editor.o(.text);
        randomizer_pick_TEXT_END = .;
        randomizer_pick_DATA_START = .;
        build/src/fragments/61/randomizer_pick.o(.data);
        build/src/fragments/61/randomizer.o(.data);
        build/src/fragments/61/randomizer_team.o(.data);
        build/src/fragments/61/randomizer_hints.o(.data);
        build/src/fragments/61/randomizer_editor.o(.data);
        randomizer_pick_DATA_END = .;
        randomizer_pick_RODATA_START = .;
        build/src/fragments/61/randomizer_pick.o(.rodata);
        build/src/fragments/61/randomizer.o(.rodata);
        build/src/fragments/61/randomizer_team.o(.rodata);
        build/src/fragments/61/randomizer_hints.o(.rodata);
        build/src/fragments/61/randomizer_editor.o(.rodata);
        . = ALIGN(16);
        randomizer_pick_RODATA_END = .;
    }
    .randomizer_pick_bss (NOLOAD) : SUBALIGN(16)
    {
        build/src/fragments/61/randomizer_pick.o(.bss);
        build/src/fragments/61/randomizer.o(.bss);
        build/src/fragments/61/randomizer_team.o(.bss);
        build/src/fragments/61/randomizer_hints.o(.bss);
        build/src/fragments/61/randomizer_editor.o(.bss);
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
        build/src/fragments/64/randomizer_battle_hints.o(.text);
        build/src/fragments/64/randomizer_opponents.o(.text);
        randomizer_battle_TEXT_END = .;
        randomizer_battle_DATA_START = .;
        build/src/fragments/64/randomizer_battle.o(.data);
        build/src/fragments/64/randomizer_battle_hints.o(.data);
        build/src/fragments/64/randomizer_opponents.o(.data);
        randomizer_battle_DATA_END = .;
        randomizer_battle_RODATA_START = .;
        build/src/fragments/64/randomizer_battle.o(.rodata);
        build/src/fragments/64/randomizer_battle_hints.o(.rodata);
        build/src/fragments/64/randomizer_opponents.o(.rodata);
        . = ALIGN(16);
        randomizer_battle_RODATA_END = .;
    }
    .randomizer_battle_bss (NOLOAD) : SUBALIGN(16)
    {
        build/src/fragments/64/randomizer_battle.o(.bss);
        build/src/fragments/64/randomizer_battle_hints.o(.bss);
        build/src/fragments/64/randomizer_opponents.o(.bss);
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

    randomizer_battleui_ROM_START = __romPos;
    randomizer_battleui_VRAM = ADDR(.randomizer_battleui);
    .randomizer_battleui 0x8C200000 : AT(randomizer_battleui_ROM_START) SUBALIGN(16)
    {
        FILL(0x00000000);
        randomizer_battleui_TEXT_START = .;
        build/randomizer/randomizer_battleui_header.o(.text);
        build/src/fragments/62/randomizer_battle_ui.o(.text);
        randomizer_battleui_TEXT_END = .;
        randomizer_battleui_DATA_START = .;
        build/src/fragments/62/randomizer_battle_ui.o(.data);
        randomizer_battleui_DATA_END = .;
        randomizer_battleui_RODATA_START = .;
        build/src/fragments/62/randomizer_battle_ui.o(.rodata);
        . = ALIGN(16);
        randomizer_battleui_RODATA_END = .;
    }
    .randomizer_battleui_bss (NOLOAD) : SUBALIGN(16)
    {
        build/src/fragments/62/randomizer_battle_ui.o(.bss);
    }
    __romPos += SIZEOF(.randomizer_battleui);
    randomizer_battleui_ROM_END = __romPos;
    __romPos = ALIGN(__romPos, 16);

    randomizer_battleui_relocs_ROM_START = __romPos;
    .randomizer_battleui_relocs 0x0 : AT(randomizer_battleui_relocs_ROM_START) SUBALIGN(16)
    {
        FILL(0x00000000);
        build/randomizer/randomizer_battleui_reloc.o(.rodata);
    }
    __romPos += SIZEOF(.randomizer_battleui_relocs);
    randomizer_battleui_relocs_ROM_END = __romPos;
    __romPos = ALIGN(__romPos, 16);

    randomizer_cup_ROM_START = __romPos;
    randomizer_cup_VRAM = ADDR(.randomizer_cup);
    .randomizer_cup 0x8C400000 : AT(randomizer_cup_ROM_START) SUBALIGN(16)
    {
        FILL(0x00000000);
        randomizer_cup_TEXT_START = .;
        build/randomizer/randomizer_cup_header.o(.text);
        build/src/fragments/63/randomizer_cup.o(.text);
        randomizer_cup_TEXT_END = .;
        randomizer_cup_DATA_START = .;
        build/src/fragments/63/randomizer_cup.o(.data);
        randomizer_cup_DATA_END = .;
        randomizer_cup_RODATA_START = .;
        build/src/fragments/63/randomizer_cup.o(.rodata);
        . = ALIGN(16);
        randomizer_cup_RODATA_END = .;
    }
    .randomizer_cup_bss (NOLOAD) : SUBALIGN(16)
    {
        build/src/fragments/63/randomizer_cup.o(.bss);
    }
    __romPos += SIZEOF(.randomizer_cup);
    randomizer_cup_ROM_END = __romPos;
    __romPos = ALIGN(__romPos, 16);

    randomizer_cup_relocs_ROM_START = __romPos;
    .randomizer_cup_relocs 0x0 : AT(randomizer_cup_relocs_ROM_START) SUBALIGN(16)
    {
        FILL(0x00000000);
        build/randomizer/randomizer_cup_reloc.o(.rodata);
    }
    __romPos += SIZEOF(.randomizer_cup_relocs);
    randomizer_cup_relocs_ROM_END = __romPos;
    __romPos = ALIGN(__romPos, 16);

    randomizer_menu_ROM_START = __romPos;
    randomizer_menu_VRAM = ADDR(.randomizer_menu);
    .randomizer_menu 0x8C500000 : AT(randomizer_menu_ROM_START) SUBALIGN(16)
    {
        FILL(0x00000000);
        randomizer_menu_TEXT_START = .;
        build/randomizer/randomizer_menu_header.o(.text);
        build/src/fragments/61/randomizer_menu.o(.text);
        build/src/fragments/61/randomizer_panel.o(.text);
        randomizer_menu_TEXT_END = .;
        randomizer_menu_DATA_START = .;
        build/src/fragments/61/randomizer_menu.o(.data);
        build/src/fragments/61/randomizer_panel.o(.data);
        randomizer_menu_DATA_END = .;
        randomizer_menu_RODATA_START = .;
        build/src/fragments/61/randomizer_menu.o(.rodata);
        build/src/fragments/61/randomizer_panel.o(.rodata);
        . = ALIGN(16);
        randomizer_menu_RODATA_END = .;
    }
    .randomizer_menu_bss (NOLOAD) : SUBALIGN(16)
    {
        build/src/fragments/61/randomizer_menu.o(.bss);
        build/src/fragments/61/randomizer_panel.o(.bss);
    }
    __romPos += SIZEOF(.randomizer_menu);
    randomizer_menu_ROM_END = __romPos;
    __romPos = ALIGN(__romPos, 16);

    randomizer_menu_relocs_ROM_START = __romPos;
    .randomizer_menu_relocs 0x0 : AT(randomizer_menu_relocs_ROM_START) SUBALIGN(16)
    {
        FILL(0x00000000);
        build/randomizer/randomizer_menu_reloc.o(.rodata);
    }
    __romPos += SIZEOF(.randomizer_menu_relocs);
    randomizer_menu_relocs_ROM_END = __romPos;
    __romPos = ALIGN(__romPos, 16);

    randomizer_options_ROM_START = __romPos;
    randomizer_options_VRAM = ADDR(.randomizer_options);
    .randomizer_options 0x8C600000 : AT(randomizer_options_ROM_START) SUBALIGN(16)
    {
        FILL(0x00000000);
        randomizer_options_TEXT_START = .;
        build/randomizer/randomizer_options_header.o(.text);
        build/src/fragments/56/randomizer_options.o(.text);
        randomizer_options_TEXT_END = .;
        randomizer_options_DATA_START = .;
        build/src/fragments/56/randomizer_options.o(.data);
        randomizer_options_DATA_END = .;
        randomizer_options_RODATA_START = .;
        build/src/fragments/56/randomizer_options.o(.rodata);
        . = ALIGN(16);
        randomizer_options_RODATA_END = .;
    }
    .randomizer_options_bss (NOLOAD) : SUBALIGN(16)
    {
        build/src/fragments/56/randomizer_options.o(.bss);
    }
    __romPos += SIZEOF(.randomizer_options);
    randomizer_options_ROM_END = __romPos;
    __romPos = ALIGN(__romPos, 16);

    randomizer_options_relocs_ROM_START = __romPos;
    .randomizer_options_relocs 0x0 : AT(randomizer_options_relocs_ROM_START) SUBALIGN(16)
    {
        FILL(0x00000000);
        build/randomizer/randomizer_options_reloc.o(.rodata);
    }
    __romPos += SIZEOF(.randomizer_options_relocs);
    randomizer_options_relocs_ROM_END = __romPos;
    __romPos = ALIGN(__romPos, 16);

    randomizer_rules_ROM_START = __romPos;
    randomizer_rules_VRAM = ADDR(.randomizer_rules);
    .randomizer_rules 0x8C700000 : AT(randomizer_rules_ROM_START) SUBALIGN(16)
    {
        FILL(0x00000000);
        randomizer_rules_TEXT_START = .;
        build/randomizer/randomizer_rules_header.o(.text);
        build/src/fragments/55/randomizer_rules.o(.text);
        randomizer_rules_TEXT_END = .;
        randomizer_rules_DATA_START = .;
        build/src/fragments/55/randomizer_rules.o(.data);
        randomizer_rules_DATA_END = .;
        randomizer_rules_RODATA_START = .;
        build/src/fragments/55/randomizer_rules.o(.rodata);
        . = ALIGN(16);
        randomizer_rules_RODATA_END = .;
    }
    .randomizer_rules_bss (NOLOAD) : SUBALIGN(16)
    {
        build/src/fragments/55/randomizer_rules.o(.bss);
    }
    __romPos += SIZEOF(.randomizer_rules);
    randomizer_rules_ROM_END = __romPos;
    __romPos = ALIGN(__romPos, 16);

    randomizer_rules_relocs_ROM_START = __romPos;
    .randomizer_rules_relocs 0x0 : AT(randomizer_rules_relocs_ROM_START) SUBALIGN(16)
    {
        FILL(0x00000000);
        build/randomizer/randomizer_rules_reloc.o(.rodata);
    }
    __romPos += SIZEOF(.randomizer_rules_relocs);
    randomizer_rules_relocs_ROM_END = __romPos;
    __romPos = ALIGN(__romPos, 16);

    randomizer_title_ROM_START = __romPos;
    randomizer_title_VRAM = ADDR(.randomizer_title);
    .randomizer_title 0x8C800000 : AT(randomizer_title_ROM_START) SUBALIGN(16)
    {
        FILL(0x00000000);
        randomizer_title_TEXT_START = .;
        build/randomizer/randomizer_title_header.o(.text);
        build/src/fragments/36/randomizer_title.o(.text);
        build/src/fragments/36/randomizer_title_faceoff.o(.text);
        randomizer_title_TEXT_END = .;
        randomizer_title_DATA_START = .;
        build/src/fragments/36/randomizer_title.o(.data);
        build/src/fragments/36/randomizer_title_faceoff.o(.data);
        randomizer_title_DATA_END = .;
        randomizer_title_RODATA_START = .;
        build/src/fragments/36/randomizer_title.o(.rodata);
        build/src/fragments/36/randomizer_title_faceoff.o(.rodata);
        . = ALIGN(16);
        randomizer_title_RODATA_END = .;
    }
    .randomizer_title_bss (NOLOAD) : SUBALIGN(16)
    {
        build/src/fragments/36/randomizer_title.o(.bss);
        build/src/fragments/36/randomizer_title_faceoff.o(.bss);
    }
    __romPos += SIZEOF(.randomizer_title);
    randomizer_title_ROM_END = __romPos;
    __romPos = ALIGN(__romPos, 16);

    randomizer_title_relocs_ROM_START = __romPos;
    .randomizer_title_relocs 0x0 : AT(randomizer_title_relocs_ROM_START) SUBALIGN(16)
    {
        FILL(0x00000000);
        build/randomizer/randomizer_title_reloc.o(.rodata);
    }
    __romPos += SIZEOF(.randomizer_title_relocs);
    randomizer_title_relocs_ROM_END = __romPos;
    __romPos = ALIGN(__romPos, 16);

    randomizer_intro_ROM_START = __romPos;
    randomizer_intro_VRAM = ADDR(.randomizer_intro);
    .randomizer_intro 0x8C900000 : AT(randomizer_intro_ROM_START) SUBALIGN(16)
    {
        FILL(0x00000000);
        randomizer_intro_TEXT_START = .;
        build/randomizer/randomizer_intro_header.o(.text);
        build/src/fragments/17/randomizer_intro.o(.text);
        randomizer_intro_TEXT_END = .;
        randomizer_intro_DATA_START = .;
        build/src/fragments/17/randomizer_intro.o(.data);
        randomizer_intro_DATA_END = .;
        randomizer_intro_RODATA_START = .;
        build/src/fragments/17/randomizer_intro.o(.rodata);
        . = ALIGN(16);
        randomizer_intro_RODATA_END = .;
    }
    .randomizer_intro_bss (NOLOAD) : SUBALIGN(16)
    {
        build/src/fragments/17/randomizer_intro.o(.bss);
    }
    __romPos += SIZEOF(.randomizer_intro);
    randomizer_intro_ROM_END = __romPos;
    __romPos = ALIGN(__romPos, 16);

    randomizer_intro_relocs_ROM_START = __romPos;
    .randomizer_intro_relocs 0x0 : AT(randomizer_intro_relocs_ROM_START) SUBALIGN(16)
    {
        FILL(0x00000000);
        build/randomizer/randomizer_intro_reloc.o(.rodata);
    }
    __romPos += SIZEOF(.randomizer_intro_relocs);
    randomizer_intro_relocs_ROM_END = __romPos;
    __romPos = ALIGN(__romPos, 16);

    /* randomizer_battle_ui_stub.s loads it by this id (RANDOMIZER_BATTLE_UI_ID) */
    ASSERT(randomizer_battleui_VRAM == 0x8C200000, "randomizer_battleui moved; update RANDOMIZER_BATTLE_UI_ID")
    /* randomizer_intro_stub.s loads it by this id (RANDOMIZER_INTRO_ID) */
    ASSERT(randomizer_intro_VRAM == 0x8C900000, "randomizer_intro moved; update RANDOMIZER_INTRO_ID")

    ASSERT(__romPos <= 0x2000000, "The randomizer doesn't fit in the free space at the end of the ROM")
