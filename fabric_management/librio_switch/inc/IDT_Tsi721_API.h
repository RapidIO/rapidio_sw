/*
*******************************************************************************
* Copyright (c) 2010 Integrated Device Technology, Inc.
*      All Rights Reserved
*
* Distribution of source code or binaries derived from this file is not
* permitted except as specifically allowed for in the Integrated Device
* Technology Software License agreement.  All copies of this source code
* modified or unmodified must retain this entire copyright notice and
* comment as is.
*
* THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR
* IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
*
*******************************************************************************
*/

#ifndef __IDT_TSI721_API_H__
#define __IDT_TSI721_API_H__

#include <DAR_Basic_Defs.h>

/* Routine to bind in all Tsi721 specific DAR support routines.
*/
STATUS bind_tsi721_DAR_support( void );

/* Routine to bind in all Tsi721 specific Device Specific Function routines.
*/
UINT32 bind_tsi721_DSF_support( void );
#endif /* __IDT_TSI721_API_H__ */
