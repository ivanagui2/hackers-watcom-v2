/****************************************************************************
*
*                            Open Watcom Project
*
* Copyright (c) 2016-2016 The Open Watcom Contributors. All Rights Reserved.
*    Portions Copyright (c) 1983-2002 Sybase, Inc. All Rights Reserved.
*
*  ========================================================================
*
*    This file contains Original Code and/or Modifications of Original
*    Code as defined in and that are subject to the Sybase Open Watcom
*    Public License version 1.0 (the 'License'). You may not use this file
*    except in compliance with the License. BY USING THIS FILE YOU AGREE TO
*    ALL TERMS AND CONDITIONS OF THE LICENSE. A copy of the License is
*    provided with the Original Code and Modifications, and is also
*    available at www.sybase.com/developer/opensource.
*
*    The Original Code and all software distributed under the License are
*    distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
*    EXPRESS OR IMPLIED, AND SYBASE AND ALL CONTRIBUTORS HEREBY DISCLAIM
*    ALL SUCH WARRANTIES, INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF
*    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR
*    NON-INFRINGEMENT. Please see the License for the specific language
*    governing rights and limitations under the License.
*
*  ========================================================================
*
* Description:  Macros to mask differences between 16-bit and 32-bit OS/2.
*
****************************************************************************/


#ifndef OS21632_H_INCLUDED
#define OS21632_H_INCLUDED

#ifdef _M_I86

#define APIRET          USHORT
#define OS_UINT         USHORT
#define OS_PUINT        PUSHORT
#define __FAR           __far

/* values returned by DosQHandType() */

#define HANDTYPE_FILE     0x0000
#define HANDTYPE_DEVICE   0x0001
#define HANDTYPE_PIPE     0x0002
#define HANDTYPE_NETWORK  0x8000

// The following are not defined in the os2 2.0 header files so
// lets do it here
#define NULLHANDLE      ((LHANDLE)0)

#else

#define OS_UINT         ULONG
#define OS_PUINT        PULONG
#define __FAR

#endif

#endif /* OS21632_H_INCLUDED */
