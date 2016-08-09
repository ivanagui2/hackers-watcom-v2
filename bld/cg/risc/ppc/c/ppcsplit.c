/****************************************************************************
*
*                            Open Watcom Project
*
* Copyright (c) 2002-2016 The Open Watcom Contributors. All Rights Reserved.
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
* Description:  WHEN YOU FIGURE OUT WHAT THIS FILE DOES, PLEASE
*               DESCRIBE IT HERE!
*
****************************************************************************/


#include "cgstd.h"
#include "coderep.h"
#include "tables.h"
#include "zoiks.h"
#include "model.h"
#include "makeins.h"
#include "namelist.h"
#include "split.h"
#include "insutil.h"


extern  void            UpdateLive( instruction *, instruction * );
extern  name            *OffsetMem( name *, type_length, type_class_def );

extern instruction      *rCONSTLOAD( instruction *ins ) {
/*******************************************************/

    unsigned_32         low;
    unsigned_32         high;
    unsigned_32         c;
    name                *high_part;
    name                *temp;
    instruction         *first_ins;
    instruction         *new_ins;
    type_class_def      class;

    assert( ins->operands[0]->n.class == N_CONSTANT );
    assert( ins->operands[0]->c.const_type == CONS_ABSOLUTE );

    class = ins->type_class;
    c = ins->operands[0]->c.lo.int_value;
    high = ( c >> 16 ) & 0xffff;
    low = c & 0xffff;
    high_part = AllocAddrConst( NULL, high, CONS_HIGH_ADDR, class );
    temp = AllocTemp( class );
    first_ins = MakeMove( high_part, temp, class );
    PrefixIns( ins, first_ins );
    new_ins = MakeBinary( OP_OR, temp, AllocS32Const( low ), ins->result, class );
    ReplIns( ins, new_ins );
    UpdateLive( first_ins, new_ins );
    return( first_ins );
}

extern  instruction     *rMOD2DIV( instruction *ins ) {
/*****************************************************/

    instruction         *first_ins;
    instruction         *new_ins;

    first_ins = MakeBinary( OP_DIV, ins->operands[0], ins->operands[1], ins->result, ins->type_class );
    PrefixIns( ins, first_ins );
    new_ins = MakeBinary( OP_MUL, ins->operands[1], ins->result, ins->result, ins->type_class );
    PrefixIns( ins, new_ins );
    new_ins = MakeBinary( OP_SUB, ins->operands[0], ins->result, ins->result, ins->type_class );
    ReplIns( ins, new_ins );
    UpdateLive( first_ins, new_ins );
    return( first_ins );
}
