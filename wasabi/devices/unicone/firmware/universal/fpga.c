/*
 * fpga.h - Abstractions for communicating with our XC4010XL FPGA
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

#include <stdio.h>
#include <string.h>

#include <uart_driver.h>
#include <tusb.h>
#include <util.h>

#include <unicone.h>
#include "hardware.h"

unsigned char fpga_version_stamp[20];

static bit config_in_progress;
static bit reading_version_stamp;
static unsigned char version_stamp_index;

static void      fpga_config_write_byte(unsigned char c);


/**************************************************************************/
/********************************************** Low-level configuration ***/
/**************************************************************************/

void fpga_init()
{
  memset(fpga_version_stamp, 0, sizeof(fpga_version_stamp));

  fpga_din_off();
  fpga_cclk_off();

  fpga_program_off();
  delay(10);
  fpga_program_on();

  /* We need to give the FPGA time for one internal memory-clear cycle */
  delay(100);
}

static void fpga_config_write_byte(unsigned char c)
{
  /* Clock out bits left-to-right. This loop was unrolled for speed. */
#define CLOCK_OUT_BIT(mask) \
  if (c & mask) \
    fpga_din_on(); \
  else \
    fpga_din_off(); \
  fpga_cclk_on(); \
  fpga_cclk_off();

  CLOCK_OUT_BIT(0x80)
  CLOCK_OUT_BIT(0x40)
  CLOCK_OUT_BIT(0x20)
  CLOCK_OUT_BIT(0x10)
  CLOCK_OUT_BIT(0x08)
  CLOCK_OUT_BIT(0x04)
  CLOCK_OUT_BIT(0x02)
  CLOCK_OUT_BIT(0x01)

#undef CLOCK_OUT_BIT
}


/**************************************************************************/
/******************************************************* Public Methods ***/
/**************************************************************************/


void fpga_config_begin()
{
  config_in_progress = 1;
  reading_version_stamp = 1;
  version_stamp_index = 0;
  fpga_init();
}

void fpga_config_write(unsigned char *cfgdata, int length)
{
  int i;

  if (!config_in_progress) {
    printf("fgpa_config_write without config_in_progress\n");
    return;
  }

  if (reading_version_stamp) {
    /* Part or all of this write might still be the version stamp */

    for (i=length; i; i--) {
      if (reading_version_stamp) {
	fpga_version_stamp[version_stamp_index++] = *(cfgdata++);
	if (version_stamp_index == 20)
	  reading_version_stamp = 0;
      }
      else {
	fpga_config_write_byte(*(cfgdata++));
      }
    }
  }
  else {
    /* This entire write goes to the FPGA, take the fast track */

    for (i=length; i; i--)
      fpga_config_write_byte(*(cfgdata++));
  }
}

unsigned char fpga_config_end()
{
  int i;

  config_in_progress = 0;

  if (fpga_done()) {
    /* Give it a few more CCLKs after the bitstream to release user I/Os */
    fpga_config_write_byte(0x00);

    /* After configuration, our hardware uses DIN as a reset. Hold the chip
     * in reset momentarily before continuing.
     */
    fpga_din_on();
    delay(100);
    fpga_din_off();

    printf("FPGA configured [");
    for (i=0; i<sizeof(fpga_version_stamp); i++)
      printf("%02x", fpga_version_stamp[i]);
    printf("]\n");

    return UNICONE_STATUS_OK;
  }
  else {
    printf("Error configuring FPGA\n");
    return UNICONE_STATUS_ERROR;
  }
}

unsigned char fpga_config_status()
{
  if (config_in_progress)
    return UNICONE_STATUS_IN_PROGRESS;
  if (fpga_done())
    return UNICONE_STATUS_OK;
  else
    return UNICONE_STATUS_ERROR;
}


/* The End */
