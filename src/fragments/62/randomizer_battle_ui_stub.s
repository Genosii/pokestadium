# The randomizer's hooks in fragment62, for RANDOMIZER=1 builds, in place of
# func_84340ACC: nothing calls that function, and fragment62 has to keep its exact
# layout (see randomizer_battle_ui.h), so this is exactly as long as it was. Its
# relocation table entries are regenerated from these instructions.
#
# The hooks' addresses live in gRandomizerState (src/randomizer_state.h), in
# osAppNMIBuffer at 0x8000031C: battlePartyHook3 at 0x80000328, battlePartyHook6 at
# 0x8000032C. Fixed addresses need no relocation.

glabel func_84340ACC

# Called by the battle's setup (func_84301430) where it loads fragment31: loads
# fragment31 all the same, then the randomizer's battle UI fragment, whose entry point
# sets the hooks. Returns what func_80004454 returned for fragment31.
glabel Randomizer_BattleUiLoad
    addiu   $sp, $sp, -0x20
    sw      $ra, 0x1C($sp)
    jal     func_80004454
    sw      $s0, 0x18($sp)
    or      $s0, $v0, $zero
    # No hooks unless the fragment loads
    lui     $t0, 0x8000
    sw      $zero, 0x328($t0)
    sw      $zero, 0x32C($t0)
    # RANDOMIZER_BATTLE_UI_ID: randomizer.ld checks the fragment is at 0x8C200000. A
    # constant, because the relocation for a symbol there could be applied by mistake.
    addiu   $a0, $zero, 0xB2
    lui     $a1, %hi(randomizer_battleui_ROM_START)
    addiu   $a1, $a1, %lo(randomizer_battleui_ROM_START)
    lui     $a2, %hi(randomizer_battleui_relocs_ROM_END)
    jal     func_80004454
    addiu   $a2, $a2, %lo(randomizer_battleui_relocs_ROM_END)
    beqz    $v0, .Lload_done
    nop
    jalr    $v0
    nop
.Lload_done:
    or      $v0, $s0, $zero
    lw      $s0, 0x18($sp)
    lw      $ra, 0x1C($sp)
    jr      $ra
    addiu   $sp, $sp, 0x20

# Called instead of func_8431524C for the party box shown while R is held in the
# Pokemon menu (func_843172A0, func_84317558): the hook if there is one, which draws the
# box too, else the box alone. Arguments pass through untouched.
glabel Randomizer_Party3Stub
    lui     $t9, 0x8000
    lw      $t9, 0x328($t9)
    beqz    $t9, .Lparty3_box
    nop
    jr      $t9
    nop
.Lparty3_box:
    j       func_8431524C
    nop

# The same for func_84315550, the box for six Pokemon
glabel Randomizer_Party6Stub
    lui     $t9, 0x8000
    lw      $t9, 0x32C($t9)
    beqz    $t9, .Lparty6_box
    nop
    jr      $t9
    nop
.Lparty6_box:
    j       func_84315550
    nop

# Up to func_84340ACC's size, 0x1E4 bytes
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
