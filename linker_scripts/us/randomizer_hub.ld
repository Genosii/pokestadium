    /*
     * HUB=1 builds only: the 3D hub (src/fragments/37/randomizer_hub.c), after the first
     * 32 MB of the ROM, which has no room left for it (randomizer.ld). The ROM is padded to
     * 64 MB. Included right after randomizer.ld.
     */
    __romPos = 0x2000000;

    randomizer_hub_ROM_START = __romPos;
    randomizer_hub_VRAM = ADDR(.randomizer_hub);
    .randomizer_hub 0x8CB00000 : AT(randomizer_hub_ROM_START) SUBALIGN(16)
    {
        FILL(0x00000000);
        randomizer_hub_TEXT_START = .;
        build/randomizer/randomizer_hub_header.o(.text);
        build/src/fragments/37/randomizer_hub.o(.text);
        build/src/fragments/37/randomizer_hub_stack.o(.text);
        randomizer_hub_TEXT_END = .;
        randomizer_hub_DATA_START = .;
        build/src/fragments/37/randomizer_hub.o(.data);
        randomizer_hub_DATA_END = .;
        randomizer_hub_RODATA_START = .;
        build/src/fragments/37/randomizer_hub.o(.rodata);
        . = ALIGN(16);
        randomizer_hub_RODATA_END = .;
    }
    .randomizer_hub_bss (NOLOAD) : SUBALIGN(16)
    {
        build/src/fragments/37/randomizer_hub.o(.bss);
    }
    __romPos += SIZEOF(.randomizer_hub);
    randomizer_hub_ROM_END = __romPos;
    __romPos = ALIGN(__romPos, 16);

    randomizer_hub_relocs_ROM_START = __romPos;
    .randomizer_hub_relocs 0x0 : AT(randomizer_hub_relocs_ROM_START) SUBALIGN(16)
    {
        FILL(0x00000000);
        build/randomizer/randomizer_hub_reloc.o(.rodata);
    }
    __romPos += SIZEOF(.randomizer_hub_relocs);
    randomizer_hub_relocs_ROM_END = __romPos;
    __romPos = ALIGN(__romPos, 16);

    ASSERT(__romPos <= 0x4000000, "The hub doesn't fit in the 64 MB ROM")
