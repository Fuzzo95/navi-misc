;################################################################################
;
; usb_main.asm - Main loop and interrupt service routine for the Raster Wand project.
;
; This file is part of the Raster Wand project. This file is based
; on the revision 2.00 USB firmware distributed by Microchip for use with the
; PIC16C745 and PIC16C765 microcontrollers. New code added for this project
; is placed in the public domain.
;
; Raster Wand modifications done by Micah Dowty <micah@navi.cx>
;;
;###############################################################################
;
; The original license agreement and author information for the USB firmware follow:
;
;                            Software License Agreement
;
; The software supplied herewith by Microchip Technology Incorporated (the "Company")
; for its PICmicro(r) Microcontroller is intended and supplied to you, the Company's
; customer, for use solely and exclusively on Microchip PICmicro Microcontroller
; products.
;
; The software is owned by the Company and/or its supplier, and is protected under
; applicable copyright laws. All rights are reserved. Any use in violation of the
; foregoing restrictions may subject the user to criminal sanctions under applicable
; laws, as well as to civil liability for the breach of the terms and conditions of
; this license.
;
; THIS SOFTWARE IS PROVIDED IN AN "AS IS" CONDITION. NO WARRANTIES, WHETHER EXPRESS,
; IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
; MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. THE
; COMPANY SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL OR
; CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
;
;	Author:			Dan Butler and Reston Condit
;	Company:		Microchip Technology Inc
;
;################################################################################

#include <p16c745.inc>
#include "usb_defs.inc"
#include "macros.inc"
#include "hardware.inc"

	errorlevel -302		; supress "register not in bank0, check page bits" message

	__CONFIG  _H4_OSC & _WDT_ON & _PWRTE_OFF & _CP_OFF

unbanked	udata_shr
W_save		res	1	; register for saving W during ISR

bank0	udata
Status_save	res	1	; registers for saving context
PCLATH_save	res	1	;  during ISR
FSR_save	res	1
PIRmasked	res	1
USBMaskedInterrupts  res  1
epBuffer	res 8
epBufferSize res 1
epBufferSizeTemp res 1
epBufferPtrTemp res 1
epByteTemp	res 1

	extern	InitUSB
	extern	USBReset
	extern	USBActivity
	extern	USBStall
	extern	USBError
	extern	USBSleep
	extern	TokenDone
	extern	USB_dev_req
	extern	finish_set_address

	extern	display_poll
	extern	display_init

STARTUP	code
	pagesel	Main
	goto	Main
	nop
InterruptServiceVector
	movwf	W_save		; save W
	movf	STATUS,W
	banksel	Status_save
	movwf	Status_save	; save STATUS
	movf	PCLATH,w
	movwf	PCLATH_save	; save PCLATH
	movf	FSR,w
	movwf	FSR_save	; save FSR

; *************************************************************
; Interrupt Service Routine
; First we step through several stages, attempting to identify the source
; of the interrupt.
; ******************************************************************

PERIPHERALTEST
	pagesel	EndISR
	btfss	INTCON,PEIE	; is there a peripheral interrupt?
	goto	EndISR		; all done....

TEST_PIR1
	banksel PIE1
	movf	PIE1,w
	banksel	PIR1
	andwf	PIR1,w		; mask the enables with the flags
	banksel	PIRmasked
	movwf	PIRmasked

	btfss	PIRmasked,USBIF	; USB interrupt flag
	goto	EndISR
	banksel	PIR1
	bcf		PIR1,USBIF

	banksel UIR
	movf	UIR,w
	andwf	UIE,w
	banksel USBMaskedInterrupts
	movwf	USBMaskedInterrupts

	pagesel	USBActivity
	btfsc	USBMaskedInterrupts,ACTIVITY	; Is there activity on the bus?
	call	USBActivity

	pagesel	USBReset
	btfsc	USBMaskedInterrupts,USB_RST	; is it a reset?
	call	USBReset			; yes, reset the SIE

	pagesel	EndISR
	btfss	USBMaskedInterrupts,TOK_DNE	; is it a Token Done?
	goto	EndISR				; no, skip the queueing process

CheckFinishSetAddr
	banksel UIR
	bcf	UIR, TOK_DNE	; clear Token Done
	banksel	USB_dev_req
	movf	USB_dev_req,w	; yes: Are we waiting for the In transaction ack-ing the end of the set address?
	xorlw	SET_ADDRESS
	btfss	STATUS,Z
	goto	EndISR		; no - skip the rest.. just queue the USTAT register

	pagesel finish_set_address
	call	finish_set_address

; ******************************************************************
; End ISR, restore context and return to the Main program
; ******************************************************************
EndISR
	banksel	FSR_save
	movf	FSR_save,w	; restore the FSR
	movwf	FSR
	movf	PCLATH_save,w	; restore PCLATH
	movwf	PCLATH
	movf	Status_save,w	; restore Status
	movwf	STATUS
	swapf	W_save,f	; restore W without corrupting STATUS
	swapf	W_save,w
	retfie

	code

; ******************************************************************
; ServiceUSB
;    Services any outstanding USB interrupts.
;    Checks for
;    This should be called from the main loop.
; ******************************************************************
ServiceUSB
	banksel	UIR
	movf	UIR,w
	banksel	USBMaskedInterrupts
	movwf	USBMaskedInterrupts

	pagesel	USBError
	btfsc	USBMaskedInterrupts,UERR
	call	USBError

	pagesel	USBSleep
	btfsc	USBMaskedInterrupts,UIDLE
	call	USBSleep

	pagesel	USBStall
	btfsc	USBMaskedInterrupts,STALL
	call	USBStall

	pagesel	TokenDone
	btfsc	USBMaskedInterrupts,TOK_DNE	; is there a TOKEN Done interrupt?
	call	TokenDone

	return


;******************************************************************* Setup

Main
	movlw	.30			; delay 16 uS to wait for USB to reset
	movwf	W_save		; SIE before initializing registers
	decfsz	W_save,f	; W_save is merely a convienient register
	goto	$-1			; to use for the delay counter.

	pagesel	InitUSB
	call	InitUSB

	; Initialize hardware
	movlf	PORTA_VALUE, PORTA
	movlf	PORTB_VALUE, PORTB
	movlf	PORTC_VALUE, PORTC
	movlf	TRISA_VALUE, TRISA
	movlf	TRISB_VALUE, TRISB
	movlf	TRISC_VALUE, TRISC
	movlf	ADCON1_VALUE, ADCON1
	movlf	T1CON_VALUE, T1CON

	pagesel	display_init
	call	display_init

;******************************************************************* Main Loop

MainLoop
	pagesel ServiceUSB
	call	ServiceUSB	; see if there are any USB tokens to process

	banksel	PORTC
	bcf		DEBUG_PIN

	clrwdt				; This should be the only place we clear the WDT!

	pagesel	display_poll
	call	display_poll

	pagesel MainLoop
	goto    MainLoop

	end

;### The End ###
