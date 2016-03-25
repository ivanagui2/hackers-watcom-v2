/****************************************************************************
*
*                            Open Watcom Project
*
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
* Description:  Editor configuration header.
*
****************************************************************************/


#ifndef __CONTROL_INCLUDED__
#define __CONTROL_INCLUDED__

#if defined( __OS2__ ) && defined( __386__ )
  #define __OS2V2__
#endif

#if defined( __WINDOWS__ )
    #define _FAR
    #define __WIN__
#elif defined( __OS2__ ) || defined( __UNIX__ ) || defined( __NT__ )
    #define _FAR
#elif defined( _M_I86 )
  #if _M_IX86 == 200
    #define USE_XMS
    #define USE_EMS
    #define USE_XTD
    #define _FAR
  #elif _M_IX86 < 200
    #define USE_EMS
    #define _FAR
  #endif
#elif defined( _M_IX86 )
  #if !defined( __4G__ )
    #define _FAR    __far
  #else
    #define _FAR
  #endif
#else
    #error _FAR not configured
#endif

#ifndef __WIN__
  #if defined( __CURSES__ ) || defined( __OS2__ ) || defined( __UNIX__ ) || defined( __NT__ )
    #define __VIO__
  #endif
#endif

#if defined( _M_I86 )
  #define _NEAR __near
#else
  #define _NEAR
#endif

#if defined( __WATCOMC__ ) && !defined( __AXP__ ) && !defined( __PPC__ ) && !defined( __MIPS__ )
  #define VI_RCS  1
#endif

#ifdef __UNIX__
  #define FSYS_CASE_SENSITIVE         1
#else
  #define FSYS_CASE_SENSITIVE         0
#endif

#endif
