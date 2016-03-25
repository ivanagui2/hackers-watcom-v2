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
* Description:  WHEN YOU FIGURE OUT WHAT THIS FILE DOES, PLEASE
*               DESCRIBE IT HERE!
*
****************************************************************************/


#include <string.h>
#include "uidef.h"

#include "clibext.h"


void UIAPI uitextfield( VSCREEN *vptr, ORD row, ORD col, ORD len,
                   ATTR attr, LPC_STRING string, int slen )
/****************************************************************/
{
    int             count;
    int             scount;
    SAREA           dirty_area;

    if( len == 0 ) return;
    if( slen < 0 ) {
        slen = 0;
    }
    count = vptr->area.width - col;
    if( count > len )
        count = len;
    scount = count;
    if( scount > slen )
        scount = slen;
    if( count > 0 ) {
        okopen( vptr );
        okline( row, col, count, vptr->area );
        dirty_area.row = row;
        dirty_area.col = col;
        dirty_area.height = 1;
        dirty_area.width = (ORD) count;
        uivdirty( vptr, dirty_area );
        bstring( &(vptr->window.type.buffer), row, col, attr, string, scount );
        if( count > scount ) {
            bfill( &(vptr->window.type.buffer), row, col+scount, attr, ' ', count-scount );
        }
    }
}


void UIAPI uivtextput( VSCREEN *vptr, ORD row, ORD col, ATTR attr, const char *string, int len )
/**********************************************************************************************/
{
    if( len == 0 && string != NULL ) {
        len = strlen( string );
    }
    uitextfield( vptr, row, col, len, attr, string, len );
}


void UIAPI uivrawput( VSCREEN *vptr, ORD row, ORD col, PIXEL *pixels, int len )
/*****************************************************************************/
{
    SAREA           dirty_area;

    okopen( vptr );
    okline( row, col, len, vptr->area );
    dirty_area.row = row;
    dirty_area.col = col;
    dirty_area.height = 1;
    dirty_area.width = (ORD) len;
    uivdirty( vptr, dirty_area );
    braw( &(vptr->window.type.buffer), row, col, pixels, len );
}
