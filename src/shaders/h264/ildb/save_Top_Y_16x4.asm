/*
 * Copyright © <2010>, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Eclipse Public License (EPL), version 1.0.  The full text of the EPL is at
 * http://www.opensource.org/licenses/eclipse-1.0.php.
 *
 */
// Module name: save_Top_Y_16x4.asm
//
// Save a Y 16x4 block 
//
//----------------------------------------------------------------
//  Symbols need to be defined before including this module
//
//	Source region in :ud
//	SRC_YD:			SRC_YD Base=rxx ElementSize=4 SrcRegion=REGION(8,1) Type=ud			// 2 GRFs
//
//	Binding table index: 
//	BI_DEST_Y:		Binding table index of Y surface
//
//----------------------------------------------------------------

#if defined(_DEBUG) 
	mov		(1)		EntrySignatureC:w			0xDDD5:w
#endif

#if defined(_FIELD)
    and.nz.f0.1 (1) NULLREGW 	BitFields:w  	BotFieldFlag:w			// Get bottom field flag
#endif

    mov (2)	MSGSRC.0<1>:ud	ORIX_TOP<2;2,1>:w			{ NoDDClr }		// Block origin
    mov (1)	MSGSRC.2<1>:ud	0x0003000F:ud				{ NoDDChk }		// Block width and height (16x4)

	// Pack Y    
	mov	(16)	MSGPAYLOADD(0)<1>		TOP_MB_YD(0)					// Compressed inst
    

#if defined(_PROGRESSIVE) 
	mov (1)	MSGDSC	MSG_LEN(2)+DWBWMSGDSC_WC+BI_DEST_Y:ud
//    send (8)	NULLREG		MSGHDR		MSGSRC<8;8,1>:ud	DWBWMSGDSC+0x00200000+BI_DEST_Y
#endif

#if defined(_FIELD)
	// Field picture
    (f0.1) mov (1)	MSGDSC	MSG_LEN(2)+DWBWMSGDSC_WC+ENMSGDSCBF+BI_DEST_Y:ud  // Write 2 GRFs to DEST_Y bottom field
    (-f0.1) mov (1)	MSGDSC	MSG_LEN(2)+DWBWMSGDSC_WC+ENMSGDSCTF+BI_DEST_Y:ud  // Write 2 GRFs to DEST_Y top field

#endif

    send (8)	WritebackResponse(0)<1>		MSGHDR	MSGSRC<8;8,1>:ud	DAPWRITE	MSGDSC
// End of save_Top_Y_16x4.asm
