{{

OpticalProximity
----------------

This is a simple proximity sensor based on a TSL230R light sensor
and two LEDs. One bright "reference" LED is fixed, and a dimmer
sensor LED is attached to a movable object. The reference LED
sets the minimum sampling frequency, and the moving sensor LED
affects our output reading.

This approach is very simplistic and includes no intrinsic immunity
to ambient light. I tried more complicated approaches, including
a pulsed reference LED with ratiometric sampling. In the end, these
approaches proved to be too slow and have too many numerical stability
problems. In the end, the simple approach wins.

Recommended schematic for low-noise operation:

      +5v
       ┬
       │
        220 Ω                                               
       │                                                      
       ┣─┳──────────┳──────────────┐                                                    
       │ │          │ TSL230R      │               
       │ │          │ ┌──────────┐ │               
      │ 0.1 µF   ┣─┤ S0    S3 ├─┼─┐                      
       │ │          └─┤ S1    S2 ├─┫ │ 1.8 kΩ                    
       │22 µF    ┌─┤ /OE  OUT ├─┼─┼────── freqPin                  
       │ │          ┣─┤ GND  Vdd ├─┘ │                     
       │ │          │ └──────────┘   │                         
       ┣─┻──────────┻────────────────┘           
                                    
                         
               100 Ω      
      ledPin ──────┐   Sensor LED 
                         

               100 Ω      
         +5V ──────┐   Reference LED
                         


The TSL230R can be hard-wired for maximum (100x) sensitivity, and
the 2x frequency divisor. Low power supply noise is very important,
hence the RC filter. You should be sure to use separate ground return
wires for the TSL230R and the LEDs.
     
┌───────────────────────────────────┐
│ Copyright (c) 2008 Micah Dowty    │               
│ See end of file for terms of use. │
└───────────────────────────────────┘

}}

CON
  MIN_DEFAULT = $FFFFFFFF
  MAX_DEFAULT = 0

  SCALE_BITS      = 11
  DEFAULT_FILTER  = 5

  ' Public cog data
  PARAM_OUTPUT    = 0    'OUT,   integer sample (filtered and scaled)
  PARAM_FILTER    = 1    'IN,    log2 number of filter samples
  PARAM_ACCUM     = 2    'OUT,   output accumulator (unfiltered)
  PARAM_COUNT     = 3    'OUT,   sample counter
  NUM_PARAMS      = 4

  ' Private cog data
  COG_OUT_MIN     = 4    'OUT,   minimum sample value so far
  COG_OUT_MAX     = 5    'OUT,   maximum sample value so far
  COG_FREQMASK    = 6    'CONST, pin mask for TSL230R frequency output
  COG_LEDMASK     = 7    'CONST, pin mask for LED
  COG_DATA_SIZE   = 8    '(Must be last)

VAR
  long cog
  long cogdata[COG_DATA_SIZE]
  
PUB start(freqPin, ledPin) : okay
  '' Initialize the sensor, start its driver cogs, and wait for it
  '' to take a single sample.
  ''
  '' freqPin must be the pin connected to the TSL230R's output, and
  '' ledPin must be connected to the sensor LED.

  ' Initialize cog parameters
  longfill(@cogdata, 0, COG_DATA_SIZE)
  cogdata[COG_FREQMASK] := |< freqPin
  cogdata[COG_LEDMASK] := |< ledPin
  cogdata[PARAM_FILTER] := DEFAULT_FILTER
  resetMinMax

  cog := cognew(@entry, @cogdata) 

  if cog < 0
    stop
    okay := 0
  else
    ' Wait for the first sample
    repeat until cogdata[PARAM_OUTPUT]
    okay := 1
     
PUB stop
  '' Immediately turn off the LED and stop the driver cogs.

  if cog > 0
    cogstop(cog)
  cog := -1

PUB read : result | stamp
  '' Take a proximity reading. Never blocks.

  result := cogdata[PARAM_OUTPUT]

PUB resetMinMax
  '' Reset minimum/maximum counters

  cogdata[COG_OUT_MIN] := MIN_DEFAULT
  cogdata[COG_OUT_MAX] := MAX_DEFAULT
  
  
PUB readMin : result
  '' Return the minimum output value

  result := cogdata[COG_OUT_MIN]

PUB readMax : result
  '' Return the maximum output value

  result := cogdata[COG_OUT_MAX]

PUB getParams : addr
  '' Get the address of our parameter block (public PARAM_* values)

  addr := @cogdata
  
DAT

'==============================================================================
' Driver Cog
'==============================================================================

                        org

                        '======================================================
                        ' Initialization
                        '======================================================

entry                   mov     t1, par
                        add     t1, #4*COG_FREQMASK
                        rdlong  freqMask, t1

                        mov     t1, par
                        add     t1, #4*COG_LEDMASK
                        rdlong  ledMask, t1                        

                        mov     addr_output, par
                        add     addr_output, #4*PARAM_OUTPUT

                        mov     addr_filter, par
                        add     addr_filter, #4*PARAM_FILTER

                        mov     addr_accum, par
                        add     addr_accum, #4*PARAM_ACCUM

                        mov     addr_count, par
                        add     addr_count, #4*PARAM_COUNT

                        mov     dira, ledMask
                        mov     outa, ledMask           ' Bright LED (On)
                        
                        '======================================================
                        ' Period Sampler
                        '======================================================

samplerLoop
                        ' Initialize filter.
                        '
                        ' At FILTER=0, we take a single sample and shift
                        ' left by SCALE_BITS. At FILTER=1, we take two samples
                        ' and shift by SCALE_BITS-1. At FILTER=2, take four samples
                        ' and shift by SCALE_BITS-2, etc.
                        
                        rdlong  filter_log, addr_filter
                        mov     output, #0
                        mov     filter_samples, #1
                        shl     filter_samples, filter_log
                        mov     filter_scale_bits, #SCALE_BITS
                        sub     filter_scale_bits, filter_log

filterLoop
                        ' Wait for the next edge, alternating
                        ' positive and negative edges.

                        waitpeq freqMask_toggle, freqMask                        
                        xor     freqMask_toggle, freqMask
                        mov     last_cnt, this_cnt
                        mov     this_cnt, cnt

                        ' Add to filter accumulator

                        mov     t1, this_cnt
                        sub     t1, last_cnt
                        add     output, t1

                        ' Update unfiltered public accumulator
                        ' XXX: This is not atomic!
                        add     sample_accum, t1
                        wrlong  sample_accum, addr_accum
                        add     sample_count, #1
                        wrlong  sample_count, addr_count

                        ' Next filter sample...
                        djnz    filter_samples, #filterLoop

                        ' Scale output
                        shl     output, filter_scale_bits

                        '======================================================
                        ' Output and min/max stage
                        '======================================================

                        ' Write output
                        wrlong  output, addr_output

                        ' Update min
                        mov     t1, par
                        add     t1, #4*COG_OUT_MIN
                        rdlong  t2, t1
                        max     t2, output
                        wrlong  t2, t1  

                        ' Update max
                        mov     t1, par
                        add     t1, #4*COG_OUT_MAX
                        rdlong  t2, t1
                        min     t2, output
                        wrlong  t2, t1

                        jmp     #samplerLoop
     

'------------------------------------------------------------------------------
' Initialized Data
'------------------------------------------------------------------------------

freqMask_toggle         long    0
sample_accum            long    0
sample_count            long    0
                        
'------------------------------------------------------------------------------
' Uninitialized Data
'------------------------------------------------------------------------------

t1                      res     1
t2                      res     1

this_cnt                res     1
last_cnt                res     1

output                  res     1

freqMask                res     1
ledMask                 res     1

addr_output             res     1
addr_filter             res     1
addr_accum              res     1
addr_count              res     1

filter_log              res     1
filter_samples          res     1
filter_scale_bits       res     1

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