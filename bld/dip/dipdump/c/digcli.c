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

void *DIGCLIENTRY( Alloc )( size_t amount )
{
    void    *ptr = malloc( amount );

    dprintf(( "DIGCliAlloc: amount=%#x -> %p\n", (unsigned)amount, ptr ));
    return( ptr );
}

void *DIGCLIENTRY( Realloc )( void *p, size_t amount )
{
    void    *ptr = realloc( p, amount);

    dprintf(( "DIGCliRealloc: p=%p amount=%3x -> %p\n", p, (unsigned)amount, ptr ));
    return( ptr );
}

void DIGCLIENTRY( Free )( void *p )
{
    dprintf(( "DIGCliFree: p=%p\n", p ));
    free( p );
}

dig_fhandle DIGCLIENTRY( Open )( char const *name, dig_open mode )
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
        return( DIG_NIL_HANDLE );
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

    fd = open( name, flgs, PMODE_RWX );

    dprintf(( "DIGCliOpen: returns %d\n", fd ));
    if( fd == -1 )
        return( DIG_NIL_HANDLE );
    return( DIG_PH2FID( fd ) );
}

unsigned long DIGCLIENTRY( Seek )( dig_fhandle fid, unsigned long p, dig_seek k )
{
    int     whence;
    long    off;

    switch( k ) {
    case DIG_ORG:   whence = SEEK_SET; break;
    case DIG_CUR:   whence = SEEK_CUR; break;
    case DIG_END:   whence = SEEK_END; break;
    default:
        dprintf(( "DIGCliSeek: h=%d p=%ld k=%d -> -1\n", DIG_FID2PH( fid ), p, k ));
        return( DIG_SEEK_ERROR );
    }

    off = lseek( DIG_FID2PH( fid ), p, whence );
    dprintf(( "DIGCliSeek: h=%d p=%ld k=%d -> %ld\n", DIG_FID2PH( fid ), p, k, off ));
    return( off );
}

size_t DIGCLIENTRY( Read )( dig_fhandle fid, void *b , size_t s )
{
    size_t      rc;

    rc = read( DIG_FID2PH( fid ), b, s );
    dprintf(( "DIGCliRead: h=%d b=%p s=%d -> %d\n", DIG_FID2PH( fid ), b, (unsigned)s, (unsigned)rc ));
    return( rc );
}

size_t DIGCLIENTRY( Write )( dig_fhandle fid, const void *b, size_t s )
{
    size_t      rc;

    rc = write( DIG_FID2PH( fid ), b, s );
    dprintf(( "DIGCliWrite: h=%d b=%p s=%d -> %d\n", DIG_FID2PH( fid ), b, (unsigned)s, (unsigned)rc ));
    return( rc );
}

void DIGCLIENTRY( Close )( dig_fhandle fid )
{
    dprintf(( "DIGCliClose: h=%d\n", DIG_FID2PH( fid ) ));
    if( close( DIG_FID2PH( fid ) ) ) {
        dprintf(( "DIGCliClose: h=%d failed!!\n", DIG_FID2PH( fid ) ));
    }
}

void DIGCLIENTRY( Remove )( char const *name, dig_open mode )
{
    dprintf(( "DIGCliRemove: name=%p:{%s} mode=%#x\n", name, name, mode ));
    unlink( name );
    mode = mode;
}

unsigned DIGCLIENTRY( MachineData )( address addr, dig_info_type info_type,
                        dig_elen in_size,  const void *in,
                        dig_elen out_size, void *out )
{
    dprintf(( "DIGCliMachineData: \n" ));
    return( 0 ); /// @todo check this out out.
}
