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
* Description:  file i/o routines for the microsoft linker file translator
*
****************************************************************************/


#include <errno.h>
#include <stdio.h>      /* for SEEK_SET/SEEK_END */
#include <string.h>
#include "wio.h"
#include "ms2wlink.h"

#include "clibext.h"

static bool     DeleteMsg = false;

// file io routines

static void IOError( char *msgstart, const char *name )
/*****************************************************/
{
    char *  tempmsg;
    char *  realmsg;

    DeleteMsg = true;
    tempmsg = Msg3Splice( msgstart, name, ": " );
    realmsg = Msg2Splice( tempmsg, strerror( errno ) );
    MemFree( tempmsg );
    Error( realmsg );
}

f_handle QOpenR( const char *name )
/*********************************/
{
    f_handle h;

    h = open( name, O_RDONLY | O_BINARY );
    if( h >= 0 ) {
        return( h );
    }
    IOError( "can't open ", name );
    return( NIL_HANDLE );
}

unsigned QRead( f_handle file, void *buffer, unsigned len, const char *name )
/***************************************************************************/
{
    int ret;

    ret = read( file, buffer, len );
    if( ret == -1 ) {
        IOError( "io error processing ", name );
    }
    return( ret );
}

unsigned QWrite( f_handle file, const void *buffer, unsigned len, const char *name )
/**********************************************************************************/
/* write from far memory */
{
    int ret;

    if( len == 0 ) return( 0 );

    ret = write( file, buffer, len );
    if( ret == -1 ) {
        IOError( "io error processing ", name );
    }
    return( ret );
}

void QWriteNL( f_handle file, const char *name )
/**********************************************/
{
    QWrite( file, "\n", 1, name );
}

void QClose( f_handle file, const char *name )
/********************************************/
/* file close */
{
    int ret;

    ret = close( file );
    if( ret == -1 ) {
        IOError( "io error processing ", name );
    }
}

static unsigned long QPos( f_handle file )
/****************************************/
{
    return( tell( file ) );
}

unsigned long QFileSize( f_handle file )
/**************************************/
{
    unsigned long   curpos;
    unsigned long   size;

    curpos = QPos( file );
    size = lseek( file, 0L, SEEK_END );
    lseek( file, curpos, SEEK_SET );
    return( size );
}

bool QReadStr( f_handle file, char *dest, unsigned size, const char *name )
/*************************************************************************/
/* quick read string (for reading directive file) */
{
    bool            eof;
    char            ch;

    eof = false;
    while( --size > 0 ) {
        if( QRead( file, &ch, 1, name ) == 0 ) {
            eof = true;
            break;
        } else if( ch != '\r' ) {
            *dest++ = ch;
        }
        if( ch == '\n' ) break;
    }
    *dest = '\0';
    return( eof );
}

bool QIsConIn( f_handle file )
/****************************/
{
    return( isatty( file ) );
}

// routines based on the "quick" file i/o routines.

void Error( char * msg )
/**********************/
{
    QWrite( STDERR_HANDLE, msg, strlen( msg ), "console" );
    QWriteNL( STDERR_HANDLE, "console" );
    if( DeleteMsg ) {
        MemFree( msg );
    }
    Suicide();
}

void CommandOut( char *command )
/******************************/
{
    QWrite( STDOUT_HANDLE, command, strlen( command ), "console" );
    QWriteNL( STDOUT_HANDLE, "console" );
}

void QSetBinary( f_handle file )
/******************************/
{
#if defined( __WATCOMC__ ) || !defined( __UNIX__ )
    setmode( file, O_BINARY );
#endif
}
