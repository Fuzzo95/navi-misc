{

Minimalist proof-of-concept RFID Reader.

Schematic:

                  INPUT ─┐   CARRIER+ ──┐    
                          │               │    
                       R2                 L1 
               R4         │  C3       D1  │    
  THRESHOLD ────┳────┻────┳──┳────┫    
                   │  R3        │  │      │    
                C4        R1  C2  C1 
                                       │    
                                          │    
                              CARRIER- ──┘    

 C1,C3  1000 nF
 C2     2200 nF
 C4     1500 pF
 R1     1 MΩ
 R2,R4  100 kΩ
 R3     1.8 MΩ
 D1     Some garden variety sigal diode from my junk drawer.
 L1     About 30 turns of magnet wire on a 5.5x7 inch wooden block.
        (Tune for 125 kHz resonance with C1.)
 
Micah Dowty <micah@navi.cx>

---

XXX: This is really messy. It doesn't do automatic threshold control,
     nor does it report back useful information like the length of the
     card's packet, or the presense/absence of a card.

Tested with HID proximity cards. Seems to be a 16-bit all-ones header,
followed by 16*32=512 bits of data.

}

CON
  _clkmode = xtal1 + pll16x
  _xinfreq = 5_000_000

  CARRIER_HZ = 125_000

  CARRIER_NEG_PIN  = 7  ' Carrier wave pin, to capacitor
  THRESHOLD_PIN    = 6  ' PWM output for detection threshold
  INPUT_PIN        = 5  ' Input signal
  CARRIER_POS_PIN  = 1  ' Carrier wave pin, to coil
  DEBUG_PIN        = 0  ' For the 'scope
  DEBUG2_PIN       = 2
  
OBJ
  text : "TV_Text"

VAR
  long debug
  long buffer[128]

  long  capture1[16]
  long  capture2[16]
  
PUB main | i
  text.start(12)
  text.str(string("Minimalist RFID reader", 13, "Micah Dowty <micah@navi.cx>", 13))
  start

  repeat
    longmove(@capture1, @buffer, 16)
    waitcnt(clkfreq/4 + cnt)
    longmove(@capture2, @buffer, 16)

    ' Only display if we get two identical samples, to cut out noise.
    if isMatch and capture2[0]
      display
      buffer[0]~
      waitcnt(clkfreq/2 + cnt)

PRI isMatch : r | i
  repeat i from 0 to 15
    if capture1[i] <> capture2[i]
       r := FALSE
       return
  r := TRUE

PRI display | i
  text.out(13)
  repeat i from 0 to 15
    text.hex(capture1[i], 8)
    text.out(" ")
  text.out(13)
    
PUB start
  init_frqa     := fraction(CARRIER_HZ, clkfreq)
  debounce      := clkfreq / (CARRIER_HZ / 4)
  bit_threshold := clkfreq / (CARRIER_HZ / 9)
  cognew(@entry, @debug)

PRI fraction(a, b) : f
  a <<= 1
  repeat 32
    f <<= 1
    if a => b
      a -= b
      f++
    a <<= 1
  
DAT

                        org

entry                   mov     dira, init_dira
                        mov     ctra, init_ctra         ' CTRA generates the carrier wave
                        mov     frqa, init_frqa
                        mov     ctrb, init_ctrb         ' CTRB generates a pulse threshold bias
                        mov     frqb, init_frqb

packet_loop             mov     buf_offset, #0
                        mov     num_ones, #0
word_loop               mov     bit_count, #32

                        ' Measure the time between falling edges, ignoring short gaps.         
bit_loop
                        or      outa, #1                ' XXX: Debug high

:edge_wait              mov     prev_edge, this_edge
                        waitpeq input_mask, input_mask  ' Wait for another pulse edge...
                        waitpne input_mask, input_mask
                        mov     this_edge, cnt

                        mov     r0, this_edge           ' How long was that?
                        sub     r0, prev_edge                        
                        cmp     r0, debounce wc
              if_c      jmp     #:edge_wait             ' Too short. Look for another edge.
                        
                        andn    outa, #1                ' XXX: Debug low

                        ' Now the difference between first_edge and this_edge
                        ' is the period of the FSK modulated signal that the RFID
                        ' tag is generating. Compare the signal's period against our
                        ' bit threshold, to recover bits.

                        mov     r0, this_edge
                        sub     r0, first_edge
                        cmp     r0, bit_threshold wc
              if_c      add     num_ones, #1
              if_nc     mov     num_ones, #0
                        rcl     shift_reg, #1

                        ' If we've received a long run of ones, sync to the beginning of the packet.
                        cmp     num_ones, #16 wz
              if_z      jmp     #packet_loop

                        mov     first_edge, this_edge   ' Start a new bit

                        djnz    bit_count, #bit_loop

                        ' If we finished a word, write it to hub memory.

                        add     buf_offset, #4
                        and     buf_offset, #$7F
                        mov     r0, par
                        add     r0, buf_offset
                        wrlong  shift_reg, r0

                        jmp     #word_loop


init_dira     long      (|< CARRIER_POS_PIN) | (|< CARRIER_NEG_PIN) | (|< THRESHOLD_PIN) | (|< DEBUG_PIN) | (|< DEBUG2_PIN)
init_frqa     long      0
init_ctra     long      (%00101 << 26) | (CARRIER_POS_PIN << 9) | CARRIER_NEG_PIN 
init_ctrb     long      (%00110 << 26) | THRESHOLD_PIN
input_mask    long      |< INPUT_PIN

init_frqb     long      $7d000000               ' Default threshold
adj_gain      long      $0                    ' Automatic threshold adjustment gain
debounce      long      0
bit_threshold long      0

first_edge    long      0
prev_edge     long      0
this_edge     long      0

buf_offset    long      0
bit_count     long      0
num_ones      long      0
shift_reg     long      0

r0            res       1

                        fit

