/*
 * The randomizer's code that goes in the block of the ROM at 0x6CA730 (_6CA730 in rom.yaml,
 * "unused": nothing in the ROM points at it), after the title screen's logo, for
 * RANDOMIZER=1 builds only: the Makefile includes this after the logo's section, which
 * takes the block's place in the linker script. The block ends where it did, at the ROM
 * position and the address it had, so nothing after it moves.
 *
 * The title screen's fragment and the intro's, the rest of the randomizer being at the end
 * of the ROM (randomizer.ld). Their headers and relocation tables are generated at link
 * time like the others' (tools/fragment_relocs.py, yamls/us/fragment_regen_randomizer.txt).
 */

    randomizer_block_SIZE = 0x18860; /* the block's, assets/us/6CA730.bin */

    randomizer_title_ROM_START = __romPos;
    randomizer_title_VRAM = ADDR(.randomizer_title);
    .randomizer_title 0x8C800000 : AT(randomizer_title_ROM_START) SUBALIGN(16)
    {
        FILL(0x00000000);
        randomizer_title_TEXT_START = .;
        build/randomizer/randomizer_title_header.o(.text);
        build/src/fragments/36/randomizer_title.o(.text);
        build/src/fragments/36/randomizer_title_arena.o(.text);
        build/src/fragments/36/randomizer_title_logo.o(.text);
        build/src/fragments/36/randomizer_title_stack.o(.text);
        randomizer_title_TEXT_END = .;
        randomizer_title_DATA_START = .;
        build/src/fragments/36/randomizer_title.o(.data);
        build/src/fragments/36/randomizer_title_arena.o(.data);
        build/src/fragments/36/randomizer_title_logo.o(.data);
        randomizer_title_DATA_END = .;
        randomizer_title_RODATA_START = .;
        build/src/fragments/36/randomizer_title.o(.rodata);
        build/src/fragments/36/randomizer_title_arena.o(.rodata);
        build/src/fragments/36/randomizer_title_logo.o(.rodata);
        . = ALIGN(16);
        randomizer_title_RODATA_END = .;
    }
    .randomizer_title_bss (NOLOAD) : SUBALIGN(16)
    {
        build/src/fragments/36/randomizer_title.o(.bss);
        build/src/fragments/36/randomizer_title_arena.o(.bss);
        build/src/fragments/36/randomizer_title_logo.o(.bss);
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

    ASSERT(__romPos <= _6CA730_ROM_START + randomizer_block_SIZE, "The randomizer doesn't fit in the block at 0x6CA730")
    __romPos = _6CA730_ROM_START + randomizer_block_SIZE;
    . = _6CA730_VRAM + randomizer_block_SIZE;
