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
* Description:  Minimal implementation of DIG client routines.
*
****************************************************************************/


#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include "digtypes.h"
#include "digcli.h"

#if 0
# define dprintf(a)     do { printf a; } while( 0 )
#else
# define dprintf(a)     do {} while( 0 )
#endif


void *DIGCLIENT DIGCliAlloc( size_t amount )
{
    void    *ptr = malloc( amount );

    dprintf(( "DIGCliAlloc: amount=%#x -> %p\n", (unsigned)amount, ptr ));
    return( ptr );
}

void *DIGCLIENT DIGCliRealloc( void *p, size_t amount )
{
    void    *ptr = realloc( p, amount);

    dprintf(( "DIGCliRealloc: p=%p amount=%3x -> %p\n", p, (unsigned)amount, ptr ));
    return( ptr );
}

void DIGCLIENT DIGCliFree( void *p )
{
    dprintf(( "DIGCliFree: p=%p\n", p ));
    free( p );
}

dig_fhandle DIGCLIENT DIGCliOpen( char const *name, dig_open mode )
{
    int     fd;
    int     flgs;

    dprintf(( "DIGCliOpen: name=%p:{%s} mode=%#x\n", name, name, mode ));

    /* convert flags. */
    switch( mode & (DIG_READ | DIG_WRITE) ) {
        case DIG_READ:
            flgs = O_RDONLY;
            break;
        case DIG_WRITE:
            flgs = O_WRONLY;
            break;
        case DIG_WRITE | DIG_READ:
            flgs = O_RDWR;
            break;
        default:
            return DIG_NIL_HANDLE;

    }
#ifdef O_BINARY
    flgs |= O_BINARY;
#endif
    if( mode & DIG_CREATE )
        flgs |= O_CREAT;
    if( mode & DIG_TRUNC )
        flgs |= O_TRUNC;
    if( mode & DIG_APPEND )
        flgs |= O_APPEND;
    /* (ignore the remaining flags) */

    fd = open( name, flgs, 0777 );

    dprintf(( "DIGCliOpen: returns %d\n", fd ));
    return( fd );
}

unsigned long DIGCLIENT DIGCliSeek( dig_fhandle h, unsigned long p, dig_seek k )
{
    int     whence;
    long    off;

    switch( k ) {
        case DIG_ORG:   whence = SEEK_SET; break;
        case DIG_CUR:   whence = SEEK_CUR; break;
        case DIG_END:   whence = SEEK_END; break;
        default:
            dprintf(( "DIGCliSeek: h=%d p=%ld k=%d -> -1\n", h, p, k ));
            return( DIG_SEEK_ERROR );
    }

    off = lseek( h, p, whence );
    dprintf(( "DIGCliSeek: h=%d p=%ld k=%d -> %ld\n", h, p, k, off ));
    return( off );
}

size_t DIGCLIENT DIGCliRead( dig_fhandle h, void *b , size_t s )
{
    size_t      rc;
#ifdef _WIN64
    unsigned    read_len;
    unsigned    amount;

    amount = INT_MAX;
    rc = 0;
    while( s > 0 ) {
        if( amount > s )
            amount = (unsigned)s;
        read_len = read( h, b, amount );
        if( read_len == (unsigned)-1 ) {
            rc = (size_t)-1;
            break;
        }
        rc += read_len;
        if( read_len != amount ) {
            break;
        }
        buffer = (char *)b + amount;
        s -= amount;
    }
#else
    rc = read( h, b, s );
#endif

    dprintf(( "DIGCliRead: h=%d b=%p s=%d -> %d\n", h, b, (unsigned)s, (unsigned)rc ));
    return( rc );
}

size_t DIGCLIENT DIGCliWrite( dig_fhandle h, const void *b, size_t s )
{
    size_t      rc;
#ifdef _WIN64
    unsigned    write_len;
    unsigned    amount;

    amount = INT_MAX;
    rc = 0;
    while( s > 0 ) {
        if( amount > s )
            amount = (unsigned)s;
        write_len = write( h, b, amount );
        if( write_len == (unsigned)-1 ) {
            rc = (size_t)-1;
            break;
        }
        rc += write_len;
        if( write_len != amount ) {
            break;
        }
        buffer = (char *)b + amount;
        s -= amount;
    }
#else
    rc = write( h, b, s );
#endif

    dprintf(( "DIGCliWrite: h=%d b=%p s=%d -> %d\n", h, b, (unsigned)s, (unsigned)rc ));
    return( rc );
}

void DIGCLIENT DIGCliClose( dig_fhandle h )
{
    dprintf(( "DIGCliClose: h=%d\n", h ));
    if( close( h ) )
        dprintf(( "DIGCliClose: h=%d failed!!\n", h ));
}

void DIGCLIENT DIGCliRemove( char const *name, dig_open mode )
{
    dprintf(( "DIGCliRemove: name=%p:{%s} mode=%#x\n", name, name, mode ));
    unlink( name );
    mode = mode;
}

unsigned DIGCLIENT DIGCliMachineData( address addr, dig_info_type info_type,
                        dig_elen in_size,  const void *in,
                        dig_elen out_size, void *out )
{
    dprintf(( "DIGCliMachineData: \n" ));
    return( 0 ); /// @todo check this out out.
}
