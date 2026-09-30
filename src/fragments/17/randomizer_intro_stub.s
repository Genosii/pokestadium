# The randomizer's hook in fragment17, for RANDOMIZER=1 builds, in place of
# func_86B044B0: nothing calls that function, and fragment17 has to keep its exact
# layout (see randomizer_intro.h), so this is exactly as long as it was. Its relocation
# table entries are regenerated from these instructions; there are fewer than the
# function had, so the table doesn't grow.

glabel func_86B044B0

# Called by the intro's setup (func_86B01190) in place of func_8002D510: loads the
# randomizer's intro fragment, runs its entry point and frees it (func_80029008, which
# FRAGMENT_LOAD_AND_CALL is), then goes on to func_8002D510.
glabel Randomizer_IntroLoad
    addiu   $sp, $sp, -0x20
    sw      $ra, 0x1C($sp)
    # RANDOMIZER_INTRO_ID: randomizer.ld checks the fragment is at 0x8C900000
    addiu   $a0, $zero, 0xB9
    lui     $a1, %hi(randomizer_intro_ROM_START)
    addiu   $a1, $a1, %lo(randomizer_intro_ROM_START)
    lui     $a2, %hi(randomizer_intro_relocs_ROM_END)
    addiu   $a2, $a2, %lo(randomizer_intro_relocs_ROM_END)
    or      $a3, $zero, $zero
    jal     func_80029008
    sw      $zero, 0x10($sp)
    lw      $ra, 0x1C($sp)
    j       func_8002D510
    addiu   $sp, $sp, 0x20
    # To func_86B044B0's size, 0x50 bytes
    nop
    nop
    nop
    nop
    nop
    nop
    nop
