;//################################################################################
;//
;// fieldsensor_protocol.h - Definitions describing the host to device protocol used by the
;//                          fieldsensor. This file is both valid C and assembly source, so it can be
;//                          shared by host and device code.
;//
;// The USB Electric Field Sensor project
;// Copyright (C) 2004 Micah Dowty <micah@navi.cx>
;//
;//  This library is free software; you can redistribute it and/or
;//  modify it under the terms of the GNU Lesser General Public
;//  License as published by the Free Software Foundation; either
;//  version 2.1 of the License, or (at your option) any later version.
;//
;//  This library is distributed in the hope that it will be useful,
;//  but WITHOUT ANY WARRANTY; without even the implied warranty of
;//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;//  Lesser General Public License for more details.
;//
;//  You should have received a copy of the GNU Lesser General Public
;//  License along with this library; if not, write to the Free Software
;//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
;//
;//###############################################################################

#ifndef __FIELDSENSOR_PROTOCOL_H
#define __FIELDSENSOR_PROTOCOL_H

;//************************************************** Idenfification

;// The protocol version, stored in binary coded decimal.
;// This is available from the device in the bcdDevice field
;// of its DEVICE descriptor.
#define EFS_PROTOCOL_VERSION  0x0100

;// Device vendor and product IDs.
;// The device class and protocol are both set to 'vendor-specific'.
#define EFS_VENDOR_ID   0xE461
#define EFS_PRODUCT_ID  0x0004


;//************************************************** Sampling parameters

;// This section defines the offsets describing the parameters
;// we use for sampling. Several of these parameter blocks can be
;// stored in RAM for automatic sampling.

;// Number of the accumulator to store the result in
#define EFS_PARAM_ACCUMULATOR_NUM	0

;// Value to XOR LC outputs with in every excitation half-period
#define EFS_PARAM_LC_PORT_XOR		1

;// Value to initialize the A/D converter control register to
#define EFS_PARAM_ADCON_INIT		2

;// Jump table value defining the excitation period length
#define EFS_PARAM_PERIOD		3

;// Jump table value defining the sampling phase
#define EFS_PARAM_PHASE			4

;// Number of excitation half-periods
#define	EFS_PARAM_NUM_HALF_PERIODS	5

;// Value to initialize LC tristate register to
#define EFS_PARAM_LC_TRIS_INIT		6

;// Value to initialize LC outputs to
#define EFS_PARAM_LC_PORT_INIT		7

#define EFS_PARAMCOUNT			8


;//************************************************** Control requests

;// So far these are all just for debugging

;// Set a parameter byte. Address in wIndex, value in wValue
#define EFS_CTRL_SET_PARAM_BYTE		0x01

;// Take a sensor reading using the current parameters, returns one byte
#define EFS_CTRL_TAKE_READING		0x02


#endif

;//### The End ###

