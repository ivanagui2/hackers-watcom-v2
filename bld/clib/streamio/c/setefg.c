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
* Description:  Routine to bring in real floating-point formatting code.
*
****************************************************************************/


#include "variety.h"
#include <stdio.h>
#include "farsupp.h"
#include "ftos.h"
#include "setefg.h"


_WMRTLINK extern FAR_STRING (*__get_EFG_Format())();
_WMRTLINK extern void       (*__get__cnvs2d())();

extern FAR_STRING           _EFG_Format();
extern void                 __cnvs2d();

/* This routine will be called by cstart if "_fltused" is referenced. */

void __setEFGfmt( void )
{
#ifdef __SW_BR
    __EFG_printf = (_type_EFG_printf)__get_EFG_Format();
    __EFG_scanf  = (_type_EFG_scanf)__get__cnvs2d();
#else
    __EFG_printf = (_type_EFG_printf)_EFG_Format;
    __EFG_scanf  = (_type_EFG_scanf)__cnvs2d;
#endif
}
