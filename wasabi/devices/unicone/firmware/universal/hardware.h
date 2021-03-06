/*
 * hardware.h - Macros and definitions related to the Unicone hardware
 *
 * Universal Controller Emulator project
 * Copyright (C) 2004 Micah Dowty
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#ifndef _H_HARDWARE
#define _H_HARDWARE


/* FPGA configuration pins */
#define fpga_program_on()   do { MCR |= MCR_RTS;  } while (0)
#define fpga_program_off()  do { MCR &= ~MCR_RTS; } while (0)
#define fpga_din_on()       do { MCR |= MCR_DTR;  } while (0)
#define fpga_din_off()      do { MCR &= ~MCR_DTR; } while (0)
#define fpga_cclk_on()      do { P3_0 = 1;        } while (0)
#define fpga_cclk_off()     do { P3_0 = 0;        } while (0)
#define fpga_done()         ((MSR & MSR_LCTS) != 0)


#endif /* _H_HARDWARE */

/* The End */
