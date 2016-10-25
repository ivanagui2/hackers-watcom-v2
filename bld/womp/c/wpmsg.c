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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <io.h>

#include "watcom.h"
#include "wpmsg.h"
#include "wressetr.h"
#include "wresset2.h"


#define STDOUT_FILENO   1

static  HANDLE_INFO     hInstance = { 0 };
static  bool            res_failure = true;

#define NO_RES_MESSAGE "Error: could not open message resource file.\r\n"
#define NO_RES_SIZE (sizeof( NO_RES_MESSAGE ) - 1)


static WResFileOffset resSeek( WResFileID handle, WResFileOffset position, int where )
/* fool the resource compiler into thinking that the resource information
 * starts at offset 0 */
{
    if( where == SEEK_SET ) {
        return( lseek( handle, position + WResFileShift, where ) - WResFileShift );
    } else {
        return( lseek( handle, position, where ) );
    }
}

WResSetRtns( open, close, read, write, resSeek, tell, malloc, free );

bool MsgInit( char *fname )
/*************************/
{
    hInstance.handle = NIL_HANDLE;
    if( !OpenResFile( &hInstance, fname ) ) {
        res_failure = false;
        if( !FindResources( &hInstance ) && !InitResources( &hInstance ) ) {
            return( true );
        }
        CloseResFile( &hInstance );
        hInstance.handle = NIL_HANDLE;
    }
    write( STDOUT_FILENO, NO_RES_MESSAGE, NO_RES_SIZE );
    res_failure = true;
    return( false );
}

void MsgGet( int resourceid, char *buffer )
/*****************************************/
{
    if( res_failure || WResLoadString( &hInstance, resourceid, (LPSTR)buffer, MAX_RESOURCE_SIZE ) <= 0 ) {
        buffer[0] = '\0';
    }
}

bool MsgFini( void )
/******************/
{
    bool    retcode = true;

    if( !res_failure ) {
        if ( CloseResFile( &hInstance ) ) {
            res_failure = true;
            retcode = false;
        }
    }
    return( retcode );
}
