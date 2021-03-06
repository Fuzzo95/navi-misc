{{

VectorMachine
-------------

This object is a simple vector graphics virtual machine.
We read instructions from a buffer in hub memory, and those
instructions tell us how to generate X/Y samples as well
as laser blanking information.

The virtual machine is capable of interpolating polynomials
using fixed-point math. It has a few important registers for
each axis:

 • S: Current sample value, a signed value
      represented in 18.14 fixed point.

 • DS: Derivative of S. At every sample, we add DS to S.

 • DDS: Second derivative of S. At every sample, we add DDS to DS.

General instruction word format:

       For all instructions    For REG != %00
       ┌────────┬────┬────────┐┌────────┬───────┬──────┐
  Bits │ 31..30 │ 29 │ 28..22 ││ 21..18 │ 17..9 │ 8..0 │
       ├────────┼────┼────────┤├────────┼───────┼──────┤
 Width │ 2      │ 1  │ 7      ││ 4      │ 9     │ 9    │
       ├────────┼────┼────────┤├────────┼───────┼──────┤
  Name │ REG    │ LE │ SCNT   ││ EXP    │ Y     │ X    │
       └────────┴────┴────────┘└────────┴───────┴──────┘

  REG: Register to write
        
    REG=%00, Reserved for special instructions.
    REG=%01, Write S
    REG=%10, Write DS
    REG=%11, Write DDS

  LE: Laser enable.
       
  SCNT: Number of samples to emit after this instuction.

  EXP: Number of bits to shift X and Y left by.

Special instructions:

REG, LE, and SCNT keep their usual meaning, but the
bits used for EXP, X, and Y are repurposed. Table of
meanings for bits 21..0:

  %0000_xxxxxxxxxxxxxxxxxx: No-op (x = don't care)
  %0001_00aaaaaaaaaaaaaaaa: Absolute jump to 16-bit address
  %0010_00aaaaaaaaaaaaaaaa: Write 0 to 16-bit address
  %0011_00aaaaaaaaaaaaaaaa: Increment value at 16-bit address  
  %0100_000000000000000000: Reset instruction memory and registers. (SCNT ignored)
  
  (All other bit patterns are reserved.)

These instructions can be useful for implementing double-buffering.
Multiple programs can coexist in the same memory area. The programs
can clear and write flags to report their progress back to another cog.


┌──────────────────────────────────────────┐
│ Copyright (c) 2008 Micah Dowty           │               
│     See end of file for terms of use.    │               
└──────────────────────────────────────────┘

}}

CON
  PARAM_CMD = 0
  PARAM_LASER_FRQA = 1
  NUM_PARAMS = 2

  COG_OUTPUT_X = 2
  COG_OUTPUT_Y = 3
  COG_MEM_BASE = 4
  COG_MEM_LIMIT = 5
  COG_LASER_MASK = 6
  COG_LASER_CTRA = 7
  COG_START_TIME = 8
  COG_LOOP_PERIOD = 9
  COG_DATA_SIZE = 10

  INST_NOP    = %00_0_0000001_0000_111111111_111111111
  INST_NOP1   = %00_0_0000001_0000_000000000_000000000
  INST_JUMP   = %00_0_0000000_0001_00_0000000000000000
  INST_JUMP1  = INST_JUMP + INST_NOP1
  INST_CLEAR  = %00_0_0000000_0010_00_0000000000000000  
  INST_INC    = %00_0_0000000_0011_00_0000000000000000
  INST_RESET  = %00_0_0000000_0100_00_0000000000000000

VAR
  long cog
  long cogdata[COG_DATA_SIZE]
  
PUB start(startTime, loopHz, laserPin, memBase, memSize) | okay
  '' Launch a new cog for the VectorMachine object. It will start its
  '' first cycle at 'startTime'. Pass the same startTime to all VoiceCoilServo
  '' objects to keep them in lockstep with us.
  ''
  '' loopHz is the period update rate to use. Normally it should match
  '' the LOOP_HZ of each VoiceCoilServo you're using.
  ''
  '' laserPin is an optional pin number for an output to control with the
  '' LE (Laser Enable) bit. If it's negative, laser control is disabled.
  ''
  '' memBase and memSize are the base address and size (in longs), respectively,
  '' of the hub memory area available for use as instruction and counter memory
  '' by the VM.
  ''
  '' Asynchronously after a start() or reset(), the cog will initialize the
  '' first word of instruction memory with an infinite loop instruction, and
  '' it will jump to that instruction.

  stop

  longfill(@cogdata, 0, COG_DATA_SIZE)

  cogdata[COG_MEM_BASE] := memBase
  cogdata[COG_MEM_LIMIT] := memBase + (memSize << 2)
  cogdata[COG_START_TIME] := startTime
  cogdata[COG_LOOP_PERIOD] := clkfreq / loopHz

  if laserPin => 0
    cogdata[COG_LASER_MASK] := |< laserPin
    cogdata[COG_LASER_CTRA] := (%00110 << 26) | laserPin 

  okay := cog := cognew(@entry, @cogdata) + 1
     
PUB stop
  '' Immediately stop the driver cog.

  if cog
    cogstop(cog~ - 1)

PUB getOutputAddress(axis) : addr
  '' Get the address of one axis's output word. This memory location
  '' is updated just after the beginning of every loop period.

  addr := @cogdata[COG_OUTPUT_X + axis]

PUB cmd(c)
  '' Asynchronously execute the supplied instruction on the virtual machine.
  ''
  '' As soon as the cog notices this instruction, it will temporarily suspend
  '' execution of instructions from memory and it will execute this one instead.
  ''
  '' This function blocks only if the VM is already busy executing another
  '' instruction provided with cmd().


  ' The cog acknowledges by writing 0, so a zero command can't be
  ' transmitted by this function. It so happens, though, that 0 is
  ' the encoding for a zero-cycle no-op, so this doesn't much matter.

  repeat while cogdata[PARAM_CMD]
  cogdata[PARAM_CMD] := c

PUB getParams : addr
  '' Get the address of our parameter block (public PARAM_* values)

  addr := @cogdata

PUB sync
  '' Wait for the virtual machine to process an entire sample

  cmd(INST_NOP1)
  cmd(INST_NOP)

PUB jump(addr)
  '' Synchronously force the cog to jump to the provided code address.
  '' This can be used to switch between multiple programs loaded into
  '' the memory region, or to reset a program. The recommended technique
  '' for loading a program after the cog is running is to write an
  '' infinite loop into memory, jump to that infinite loop, rewrite
  '' the rest of memory, then jump to the beginning of the new program.
  ''
  '' This uses a zero-cycle jump, so the cog will not emit an extra
  '' sample because of this instruction.

  cmd(INST_JUMP + (addr & $FFFF))
  cmd(INST_NOP)

PUB reset(addr)
  '' Synchronously reset the cog's registers, and stick it in an
  '' infinite loop at address zero.

  cmd(INST_RESET)
  cmd(INST_NOP) 


DAT

'==============================================================================
' Driver Cog
'==============================================================================

                        org

                        '======================================================
                        ' Initialization
                        '======================================================

entry                   mov     t1, par
                        add     t1, #4*COG_MEM_BASE
                        rdlong  mem_base, t1

                        mov     t1, par
                        add     t1, #4*COG_MEM_LIMIT
                        rdlong  mem_limit, t1

                        mov     t1, par
                        add     t1, #4*COG_LASER_MASK
                        rdlong  laser_mask, t1

                        mov     t1, par
                        add     t1, #4*COG_LASER_CTRA
                        rdlong  ctra, t1

                        mov     t1, par
                        add     t1, #4*COG_START_TIME
                        rdlong  loop_cnt, t1

                        mov     t1, par
                        add     t1, #4*COG_LOOP_PERIOD
                        rdlong  loop_period, t1
                        
                        mov     addr_output_x, par
                        add     addr_output_x, #4*COG_OUTPUT_X

                        mov     addr_output_y, par
                        add     addr_output_y, #4*COG_OUTPUT_Y

                        mov     addr_cmd, par
                        add     addr_cmd, #4*PARAM_CMD

                        mov     addr_laser_frqa, par
                        add     addr_laser_frqa, #4*PARAM_LASER_FRQA

                        ' Set up I/O. We drive the laser via
                        ' CTRA in "DUTY" mode. This gives us a
                        ' very high frequency PWM generator for
                        ' the laser, letting us dim it (at the cost
                        ' of some gnarly electrical noise...).                        

                        mov     outa, #0
                        mov     dira, laser_mask

                        ' Synchronize to the provided start time.
                        waitcnt loop_cnt, loop_period


reset_state             ' Reset instruction memory.
                        mov     vm_addr, #0
                        wrlong  jmp1, mem_base

                        ' Reset registers
                        mov     vm_s_x, #0
                        mov     vm_s_y, #0
                        mov     vm_ds_x, #0
                        mov     vm_ds_y, #0
                        mov     vm_dds_x, #0
                        mov     vm_dds_y, #0


                        '======================================================
                        ' Virtual Machine loop
                        '======================================================

main_loop

                        ' Instruction fetch.
                        '
                        ' Try the cmd() buffer first, then try fetching from
                        ' vm_addr within our memory range. If we fetch from vm_addr,
                        ' increment it. If the address is out of range, we read a no-op.

                        rdlong  vm_instr, addr_cmd wz       ' Check for an instruction from cmd()
                        mov     t1, vm_addr                 ' Calculate next mem read address.
                        shl     t1, #2                      '    longs -> bytes
        if_nz           wrlong  h00000000, addr_cmd         ' Acknowledge the cmd() instruction.
                        add     t1, mem_base
                        cmp     t1, mem_limit wc            ' Addr is valid if C=1
        if_z_and_c      rdlong  vm_instr, t1                ' Mem read if valid. If not, vm_instr=0.
        if_z            add     vm_addr, #1                 ' Next instruction.

                        ' Instruction decode.

                        mov     vm_x, vm_instr              ' Extract X
                        shl     vm_x, #(32-9)               '   Justify, trim, and sign-extend
                        sar     vm_x, #(32-9)
                        mov     vm_y, vm_instr              ' Extract Y
                        shl     vm_y, #(32-18)              '   Justify, trim, and sign-extend
                        sar     vm_y, #(32-9)

                        mov     vm_exp, vm_instr            ' Extract EXP
                        shl     vm_exp, #(32-22)            '   Justify/trim
                        shr     vm_exp, #(32-4)

                        shl     vm_x, vm_exp                ' Apply EXP to X and Y
                        shl     vm_y, vm_exp                        
                        
                        mov     vm_scnt, vm_instr           ' Extract SCNT
                        shl     vm_scnt, #(32-29)           '   Justify/trim
                        shr     vm_scnt, #(32-7)

                        mov     vm_reg, vm_instr            ' Extract REG
                        shr     vm_reg, #30                 '   Justify/trim
                        add     vm_reg, #:reg_jmp_table     '   Jump to handler for this REG value
                        jmp     vm_reg
:reg_jmp_table          jmp     #:reg_special
                        jmp     #:reg_s
                        jmp     #:reg_ds
                        jmp     #:reg_dds

                        ' REG writes

:reg_s                  mov     vm_s_x, vm_x
                        mov     vm_s_y, vm_y
                        jmp     #sample_loop

:reg_ds                 mov     vm_ds_x, vm_x
                        mov     vm_ds_y, vm_y
                        jmp     #sample_loop

:reg_dds                mov     vm_dds_x, vm_x
                        mov     vm_dds_y, vm_y
                        jmp     #sample_loop

:reg_special            add     vm_exp, #:special_jmp_table
                        jmp     vm_exp
:special_jmp_table      jmp     #sample_loop            ' %0000 - No-op
                        jmp     #:special_jmp16         ' %0001 - 16-bit unsigned jump
                        jmp     #:special_clear16       ' %0010 - 16-bit clear
                        jmp     #:special_inc16         ' %0011 - 16-bit increment
                        jmp     #reset_state            ' %0100 - Reset
                        jmp     #sample_loop            ' %0101 - Reserved
                        jmp     #sample_loop            ' %0110 - Reserved
                        jmp     #sample_loop            ' %0111 - Reserved
                        jmp     #sample_loop            ' %1000 - Reserved
                        jmp     #sample_loop            ' %1001 - Reserved
                        jmp     #sample_loop            ' %1010 - Reserved
                        jmp     #sample_loop            ' %1011 - Reserved
                        jmp     #sample_loop            ' %1100 - Reserved
                        jmp     #sample_loop            ' %1101 - Reserved
                        jmp     #sample_loop            ' %1110 - Reserved
                        jmp     #sample_loop            ' %1111 - Reserved

                        ' Special instructions
                        
:special_jmp16          mov     vm_addr, vm_instr       ' Set address, and mask to 16 bits.
                        and     vm_addr, h0000FFFF
                        jmp     #sample_loop              

:special_clear16        mov     t1, vm_instr            ' Get 16-bit address
                        and     t1, h0000FFFF
                        shl     t1, #2                  ' longs -> bytes
                        add     t1, mem_base            ' Calculate hub address
                        cmp     t1, mem_limit wc        ' Addr is valid if C=1
        if_c            wrlong  h00000000, t1
                        jmp     #sample_loop

:special_inc16          mov     t1, vm_instr            ' Get 16-bit address
                        and     t1, h0000FFFF
                        shl     t1, #2                  ' longs -> bytes
                        add     t1, mem_base            ' Calculate hub address
                        cmp     t1, mem_limit wc        ' Addr is valid if C=1
        if_c            rdlong  t2, t1
        if_c            add     t2, #1
        if_c            wrlong  t2, t1
                        jmp     #sample_loop


                        '======================================================
                        ' Sample Generator
                        '======================================================

sample_loop             cmp     vm_scnt, #0 wz          ' No samples left?
              if_z      jmp     #main_loop

                        adds    vm_ds_x, vm_dds_x       ' Integrate this sample
                        adds    vm_ds_y, vm_dds_y
                        adds    vm_s_x, vm_ds_x
                        adds    vm_s_y, vm_ds_y

                        mov     t1, vm_s_x              ' Fixed point to integer
                        sar     t1, #14
                        mov     t2, vm_s_y
                        sar     t2, #14
                        
                        waitcnt loop_cnt, loop_period   ' Sync to the next loop period

                        wrlong  t1, addr_output_x       ' Write results

                        test    vm_instr, LE_bit wz     ' Enable/disable laser
              if_z      mov     frqa, #0
              if_nz     rdlong  frqa, addr_laser_frqa  

                        sub     vm_scnt, #1
                        wrlong  t2, addr_output_y

                        jmp     #sample_loop


'------------------------------------------------------------------------------
' Initialized Data
'------------------------------------------------------------------------------

h00000000               long    $00000000
h0000FFFF               long    $0000FFFF

LE_bit                  long    $20000000
jmp1                    long    INST_JUMP1

                        
'------------------------------------------------------------------------------
' Uninitialized Data
'------------------------------------------------------------------------------

t1                      res     1
t2                      res     1

mem_base                res     1
mem_limit               res     1
laser_mask              res     1
loop_cnt                res     1
loop_period             res     1

addr_output_x           res     1
addr_output_y           res     1
addr_cmd                res     1
addr_laser_frqa         res     1

vm_addr                 res     1
vm_instr                res     1
vm_x                    res     1
vm_y                    res     1
vm_exp                  res     1
vm_scnt                 res     1
vm_reg                  res     1
vm_s_x                  res     1
vm_s_y                  res     1
vm_ds_x                 res     1
vm_ds_y                 res     1
vm_dds_x                res     1
vm_dds_y                res     1

                        fit

{{
┌──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐
│                                                   TERMS OF USE: MIT License                                                  │                                                            
├──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
│Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation    │ 
│files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy,    │
│modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software│
│is furnished to do so, subject to the following conditions:                                                                   │
│                                                                                                                              │
│The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.│
│                                                                                                                              │
│THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE          │
│WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR         │
│COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,   │
│ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                         │
└──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘
}}