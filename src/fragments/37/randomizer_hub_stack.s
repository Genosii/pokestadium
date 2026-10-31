# The 3D hub's scene runs on a stack of its own (randomizer_hub.c), as the title's does
# (src/fragments/36/randomizer_title_stack.s, of which this is a copy for the hub's fragment).
# The game thread's stack is 8 KB, directly above the thread's own OSThread (pThreads in
# main.c), and the title calls into the randomizer from deep in its setup and drawing:
# loading and decompressing an arena, then drawing it, go past the end of it and over
# the thread, and the game stops at its next thread switch. Exceptions save registers
# in the OSThread, not on the stack, so a function can run on another stack safely.
# Only in HUB=1 builds (linker_scripts/us/randomizer_hub.ld links it).

.include "macro.inc"

.set noat
.set noreorder

.section .text, "ax"

# void Randomizer_HubCallOnStack(void (*func)(void), void* stackTop)
# Calls func with the stack pointer at stackTop (16-byte aligned, minus the argument
# area a called function may use), then goes back to the caller's stack.
glabel Randomizer_HubCallOnStack
    addiu   $sp, $sp, -0x18
    sw      $ra, 0x14($sp)
    sw      $s0, 0x10($sp)
    or      $s0, $sp, $zero
    addiu   $at, $zero, -0x10
    and     $a1, $a1, $at
    jalr    $a0
    addiu   $sp, $a1, -0x20
    or      $sp, $s0, $zero
    lw      $s0, 0x10($sp)
    lw      $ra, 0x14($sp)
    jr      $ra
    addiu   $sp, $sp, 0x18
