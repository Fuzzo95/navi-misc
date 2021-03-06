/* -*- Mode: C; c-basic-offset: 4 -*-
 *
 * serial.c --
 *
 *   A simple low-speed transmit only serial port, implemented in
 *   software using Timer 1.
 *
 * Copyright (c) 2008 Micah Dowty <micah@navi.cx>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>

#include "tedrx.h"

#define BUFFER_SIZE      16

volatile static struct {
    uint8_t data[BUFFER_SIZE];
    uint8_t head;
    uint8_t tail;
} buffer;


/*
 * Timer 1 compare match vector --
 *
 *    This is our software async serial transmitter, which implements
 *    stdout.
 *
 *    We intentionally use COMPB instead of COMPA, because this
 *    interrupt must be lower priority than the Timer 0 overflow. This
 *    interrupt must not block the Timer 0 interrupt, or our ASK
 *    demodulation timebase will be inaccurate.
 */

ISR(TIM1_COMPB_vect, ISR_NOBLOCK)
{
    static uint8_t bits_remaining;
    static uint16_t current;
    static uint8_t next_bit;

#if !defined(DEBUG_PIN) || (DEBUG_PIN != SERIAL_OUT_PIN)
    /* Latch the output at the top of the ISR, to reduce jitter. */
    if (next_bit ^ SERIAL_INVERT) {
        PORTB |= _BV(SERIAL_OUT_PIN);
    } else {
        PORTB &= ~_BV(SERIAL_OUT_PIN);
    }
#endif

    /*
     * Reload our shift register, if it's empty.
     */
    if (!bits_remaining) {
        if (buffer.head == buffer.tail) {
            /* Buffer is empty */
            next_bit = 1;
            return;
        }
    
        /* 8-N-1 framing */
        current = _BV(9) | (buffer.data[buffer.head] << 1);
        bits_remaining = 10;

        buffer.head = (buffer.head + 1) & (BUFFER_SIZE - 1);
    }

    next_bit = current & 1;
    current >>= 1;
    bits_remaining--;
}


/*
 * serial_putchar --
 *
 *    Write a character to the software UART's buffer.
 *    This is used as the implementation behind the stderr stream.
 */

static int
serial_putchar(char c, FILE *stream)
{
    uint8_t next_tail;

    do {
        next_tail = (buffer.tail + 1) & (BUFFER_SIZE - 1);
    } while (next_tail == buffer.head);

    buffer.data[buffer.tail] = c;
    buffer.tail = next_tail;

    return 0;
}


/*
 * serial_init --
 *
 *    Set up the software UART's I/O and timer.
 */

void
serial_init(void)
{
    static FILE stream = FDEV_SETUP_STREAM(serial_putchar, NULL, _FDEV_SETUP_WRITE);

    DDRB |= _BV(SERIAL_OUT_PIN);
        
    /*
     * Set up Timer 1 as a baud rate generator for SERIAL_BAUD.
     * We have 1/2 of a software UART, to implement stdout.
     * This baud rate calculation uses a CK/8 prescaler.
     *
     * When the timer reaches OCR1C, it will be reset. When it
     * reaches OCR1B, we fire an interrupt.
     */
    TCCR1 = _BV(CTC1) | _BV(CS12);
    OCR1B = 1;
    OCR1C = F_CPU / 8 / SERIAL_BAUD;
    TIMSK |= _BV(OCIE1B);

    stdout = stderr = &stream;
}
