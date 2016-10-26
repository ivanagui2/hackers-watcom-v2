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
* Description:  Entry points for ORL routines.
*
****************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "orl.h"
#include "orllevel.h"
#include "orlentry.h"
#include "orlflhnd.h"
#include "pcobj.h"

orl_handle ORLENTRY ORLInit( orl_funcs * funcs )
/**********************************************/
{
    orli_handle                 orli_hnd;

    orli_hnd = (orli_handle)funcs->alloc( sizeof( struct orli_handle_struct ) );
    if( orli_hnd == NULL )
        return( NULL );
    orli_hnd->error = ORL_OKAY;
    orli_hnd->funcs = funcs;
    orli_hnd->elf_hnd = ElfInit( funcs );
    if( orli_hnd->elf_hnd == NULL ) {
        funcs->free( orli_hnd );
        return( NULL );
    }
    orli_hnd->coff_hnd = CoffInit( funcs );
    if( orli_hnd->coff_hnd == NULL ) {
        funcs->free( orli_hnd );
        return( NULL );
    }
    orli_hnd->omf_hnd = OmfInit( funcs );
    if( orli_hnd->omf_hnd == NULL ) {
        funcs->free( orli_hnd );
        return( NULL );
    }
    orli_hnd->first_file_hnd = NULL;
    return( (orl_handle)orli_hnd );
}

orl_return ORLENTRY ORLGetError( orl_handle orl_hnd )
/***************************************************/
{
    return( ((orli_handle)orl_hnd)->error );
}

orl_return ORLENTRY ORLFini( orl_handle orl_hnd )
/***********************************************/
{
    orl_return      error;
    orli_handle     orli_hnd = (orli_handle)orl_hnd;

    if( ( error = ElfFini( orli_hnd->elf_hnd ) ) != ORL_OKAY ) return( error );
    if( ( error = CoffFini( orli_hnd->coff_hnd ) ) != ORL_OKAY ) return( error );
    if( ( error = OmfFini( orli_hnd->omf_hnd ) ) != ORL_OKAY ) return( error );
    while( orli_hnd->first_file_hnd ) {
        error = ORLRemoveFileLinks( orli_hnd->first_file_hnd );
        if( error != ORL_OKAY ) return( error );
    }
    ORL_FUNCS_FREE( orli_hnd, orli_hnd );
    return( ORL_OKAY );
}

orl_file_format ORLFileIdentify( orl_handle orl_hnd, void * file )
/****************************************************************/
{
    unsigned char *     magic;
    uint_16             machine_type;
    uint_16             offset;
    uint_16             len;
    unsigned char       chksum;
    orli_handle         orli_hnd = (orli_handle)orl_hnd;

    magic = ORL_FUNCS_READ( orli_hnd, file, 4 );
    if( magic == NULL ) {
        return ORL_UNRECOGNIZED_FORMAT;
    }
    if( ORL_FUNCS_SEEK( orli_hnd, file, -4, SEEK_CUR ) == -1 ) {
        return ORL_UNRECOGNIZED_FORMAT;
    }
    if( magic[0] == 0x7f && magic[1] == 'E' && magic[2] == 'L' && magic[3] == 'F' ) {
        return ORL_ELF;
    }

    // See if this is the start of an OMF object file
    // the first record must be a THEADR record and we check that it is
    // valid, if it is then we assume that this is an OMF object file.
    if( magic[0] == CMD_THEADR ) {
        len = magic[1] | ( magic[2] << 8 );
        len -= (unsigned char)(magic[3]);
        len -= 2;
        if( !len ) {
            // This looks good so far, we must now check the record
            len = (unsigned char)(magic[3]) + 1;
            if( ORL_FUNCS_SEEK( orli_hnd, file, 4, SEEK_CUR ) == -1 ) {
                return ORL_UNRECOGNIZED_FORMAT;
            }
            chksum = magic[0] + magic[1] + magic[2] + magic[3];
            magic = ORL_FUNCS_READ( orli_hnd, file, len );
            if( ORL_FUNCS_SEEK( orli_hnd, file, -(long)( 4 + len ), SEEK_CUR ) == -1 ) {
                return ORL_UNRECOGNIZED_FORMAT;
            }
            if( magic ) {
                // Go on to check record checksum
                while( len ) {
                    chksum += (unsigned char)(*magic);
                    len--;
                    magic++;
                }
                magic--;
                if( !( *magic ) || !chksum ) {
                    // seems to be a correct OMF record to start the OBJ
                    return( ORL_OMF );
                }
            } else {
                magic = ORL_FUNCS_READ( orli_hnd, file, 4 );
                if( magic == NULL ) {
                    return ORL_UNRECOGNIZED_FORMAT;
                } else if( ORL_FUNCS_SEEK( orli_hnd, file, -4, SEEK_CUR ) == -1 ) {
                    return ORL_UNRECOGNIZED_FORMAT;
                }
            }
        }
    }

    machine_type = *( (uint_16 *) magic );
    switch( machine_type ) {
    case IMAGE_FILE_MACHINE_I860:
    case IMAGE_FILE_MACHINE_I386A:
    case IMAGE_FILE_MACHINE_I386:
    case IMAGE_FILE_MACHINE_R3000:
    case IMAGE_FILE_MACHINE_R4000:
    case IMAGE_FILE_MACHINE_ALPHA:
    case IMAGE_FILE_MACHINE_POWERPC:
    case IMAGE_FILE_MACHINE_AMD64:
    case IMAGE_FILE_MACHINE_UNKNOWN:
        return ORL_COFF;
    }
    // Is it PE?
    if( magic[0] == 'M' && magic[1] == 'Z' ) {
        if( ORL_FUNCS_SEEK( orli_hnd, file, 0x3c, SEEK_CUR ) == -1 ) {
            return ORL_UNRECOGNIZED_FORMAT;
        }
        magic = ORL_FUNCS_READ( orli_hnd, file, 0x4 );
        if( magic == NULL ) {
            return ORL_UNRECOGNIZED_FORMAT;
        }
        offset = *( (uint_16 *) magic );
        if( ORL_FUNCS_SEEK( orli_hnd, file, offset-0x40, SEEK_CUR ) == -1 ) {
            return ORL_UNRECOGNIZED_FORMAT;
        }
        magic = ORL_FUNCS_READ( orli_hnd, file, 4 );
        if( magic == NULL ) {
            return ORL_UNRECOGNIZED_FORMAT;
        }
        if( magic[0]=='P' && magic[1] == 'E' && magic[2] == '\0' && magic[3] == '\0' ) {
            magic = ORL_FUNCS_READ( orli_hnd, file, 4 );
            if( magic == NULL ) {
                return ORL_UNRECOGNIZED_FORMAT;
            }
            if( ORL_FUNCS_SEEK( orli_hnd, file, -(long)(offset+8), SEEK_CUR ) == -1 ) {
                return ORL_UNRECOGNIZED_FORMAT;
            }
            machine_type = *( (uint_16 *) magic );
            switch( machine_type ) {
            case IMAGE_FILE_MACHINE_I860:
            case IMAGE_FILE_MACHINE_I386:
            case IMAGE_FILE_MACHINE_R3000:
            case IMAGE_FILE_MACHINE_R4000:
            case IMAGE_FILE_MACHINE_ALPHA:
            case IMAGE_FILE_MACHINE_POWERPC:
            case IMAGE_FILE_MACHINE_AMD64:
                return ORL_COFF;
            }
        }
    }
    return ORL_UNRECOGNIZED_FORMAT;
}

orl_file_handle ORLENTRY ORLFileInit( orl_handle orl_hnd, void *file, orl_file_format type )
{
    orli_file_handle    orli_file_hnd;
    orli_handle         orli_hnd = (orli_handle)orl_hnd;

    switch( type ) {
    case( ORL_ELF ):
    case( ORL_COFF ):
    case( ORL_OMF ):
        orli_file_hnd = (orli_file_handle)ORL_FUNCS_ALLOC( orli_hnd, sizeof( struct orli_file_handle_struct ) );
        if( orli_file_hnd == NULL ) {
            orli_hnd->error = ORL_OUT_OF_MEMORY;
            break;
        }
        orli_file_hnd->type = type;
        switch( type ) {
        case ORL_ELF:
            orli_hnd->error = ElfFileInit( orli_hnd->elf_hnd, file, &orli_file_hnd->file_hnd.elf );
            break;
        case ORL_COFF:
            orli_hnd->error = CoffFileInit( orli_hnd->coff_hnd, file, &orli_file_hnd->file_hnd.coff );
            break;
        case ORL_OMF:
            orli_hnd->error = OmfFileInit( orli_hnd->omf_hnd, file, &orli_file_hnd->file_hnd.omf );
            break;
        default:
            break;
        }
        if( orli_hnd->error != ORL_OKAY ) {
            ORL_FUNCS_FREE( orli_hnd, orli_file_hnd );
            break;
        }
        ORLAddFileLinks( orli_hnd, orli_file_hnd );
        return( (orl_file_handle)orli_file_hnd );
    default:    //ORL_UNRECOGNIZED_FORMAT
        break;
    }
    return( NULL );
}

orl_return ORLENTRY ORLFileFini( orl_file_handle orl_file_hnd )
{
    orl_return                          error = ORL_ERROR;
    /* jump table replace: */
    switch( ORL_FILE_TYPE( orl_file_hnd ) ) {
    case( ORL_ELF ):
        error = ElfFileFini( ORL_FILE_HND_ELF( orl_file_hnd ) );
        break;
    case( ORL_COFF ):
        error = CoffFileFini( ORL_FILE_HND_COFF( orl_file_hnd ) );
        break;
    case( ORL_OMF ):
        error = OmfFileFini( ORL_FILE_HND_OMF( orl_file_hnd ) );
        break;
    default: break;//ORL_UNRECOGNIZED_FORMAT
    }

    if( error != ORL_OKAY ) return( error );
    return( ORLRemoveFileLinks( (orli_file_handle)orl_file_hnd ) );
}


unsigned long ORLENTRY ORLExportTableRVA( orl_file_handle orl_file_hnd )
{
    unsigned long rva = 0L;

    if( ORL_FILE_TYPE( orl_file_hnd ) == ORL_COFF ) {
        rva = CoffExportTableRVA( ORL_FILE_HND_COFF( orl_file_hnd ) );
    }

    return rva;
}

orl_return ORLENTRY ORLFileScan( orl_file_handle orl_file_hnd, char *desired, orl_sec_return_func return_func )
{
    switch( ORL_FILE_TYPE( orl_file_hnd ) ) {
    case( ORL_ELF ):
        return( ElfFileScan( ORL_FILE_HND_ELF( orl_file_hnd ), desired, return_func ) );
    case( ORL_COFF ):
        return( CoffFileScan( ORL_FILE_HND_COFF( orl_file_hnd ), desired, return_func ) );
    case( ORL_OMF ):
        return( OmfFileScan( ORL_FILE_HND_OMF( orl_file_hnd ), desired, return_func ) );
    default: break;//ORL_UNRECOGNIZED_FORMAT
    }
    return( ORL_ERROR );
}

orl_machine_type ORLENTRY ORLFileGetMachineType( orl_file_handle orl_file_hnd )
{
    switch( ORL_FILE_TYPE( orl_file_hnd ) ) {
    case( ORL_ELF ):
        return( ElfFileGetMachineType( ORL_FILE_HND_ELF( orl_file_hnd ) ) );
    case( ORL_COFF ):
        return( CoffFileGetMachineType( ORL_FILE_HND_COFF( orl_file_hnd ) ) );
    case( ORL_OMF ):
        return( OmfFileGetMachineType( ORL_FILE_HND_OMF( orl_file_hnd ) ) );
    default: break;//ORL_UNRECOGNIZED_FORMAT
    }
    return( 0 );
}

orl_file_flags ORLENTRY ORLFileGetFlags( orl_file_handle orl_file_hnd )
{
    switch( ORL_FILE_TYPE( orl_file_hnd ) ) {
    case( ORL_ELF ):
        return( ElfFileGetFlags( ORL_FILE_HND_ELF( orl_file_hnd ) ) );
    case( ORL_COFF ):
        return( CoffFileGetFlags( ORL_FILE_HND_COFF( orl_file_hnd ) ) );
    case( ORL_OMF ):
        return( OmfFileGetFlags( ORL_FILE_HND_OMF( orl_file_hnd ) ) );
    default: break;//ORL_UNRECOGNIZED_FORMAT
    }
    return( 0 );
}

orl_file_size ORLENTRY ORLFileGetSize( orl_file_handle orl_file_hnd )
{
    switch( ORL_FILE_TYPE( orl_file_hnd ) ) {
    case( ORL_ELF ):
        return( ElfFileGetSize( ORL_FILE_HND_ELF( orl_file_hnd ) ) );
    case( ORL_COFF ):
        return( CoffFileGetSize( ORL_FILE_HND_COFF( orl_file_hnd ) ) );
    case( ORL_OMF ):
        return( OmfFileGetSize( ORL_FILE_HND_OMF( orl_file_hnd ) ) );
    default: break;//ORL_UNRECOGNIZED_FORMAT
    }
    return( 0 );
}

orl_file_type ORLENTRY ORLFileGetType( orl_file_handle orl_file_hnd )
{
    switch( ORL_FILE_TYPE( orl_file_hnd ) ) {
    case( ORL_ELF ):
        return( ElfFileGetType( ORL_FILE_HND_ELF( orl_file_hnd ) ) );
    case( ORL_COFF ):
        return( CoffFileGetType( ORL_FILE_HND_COFF( orl_file_hnd ) ) );
    case( ORL_OMF ):
        return( OmfFileGetType( ORL_FILE_HND_OMF( orl_file_hnd ) ) );
    default: break;//ORL_UNRECOGNIZED_FORMAT
    }
    return( 0 );
}

orl_file_format ORLENTRY ORLFileGetFormat( orl_file_handle orl_file_hnd )
{
    return( ORL_FILE_TYPE( orl_file_hnd ) );
}

orl_sec_handle ORLENTRY ORLFileGetSymbolTable( orl_file_handle orl_file_hnd )
{
    switch( ORL_FILE_TYPE( orl_file_hnd ) ) {
    case( ORL_ELF ):
        return( (orl_sec_handle)ElfFileGetSymbolTable( ORL_FILE_HND_ELF( orl_file_hnd ) ) );
    case( ORL_COFF ):
        return( (orl_sec_handle)CoffFileGetSymbolTable( ORL_FILE_HND_COFF( orl_file_hnd ) ) );
    case( ORL_OMF ):
        return( (orl_sec_handle)OmfFileGetSymbolTable( ORL_FILE_HND_OMF( orl_file_hnd ) ) );
    default: break;//ORL_UNRECOGNIZED_FORMAT
    }
    return( NULL );
}

char * ORLENTRY ORLSecGetName( orl_sec_handle orl_sec_hnd )
{
    switch( ORL_SEC_TYPE( orl_sec_hnd ) ) {
    case( ORL_ELF ):
        return( ElfSecGetName( (elf_sec_handle)orl_sec_hnd ) );
    case( ORL_COFF ):
        return( CoffSecGetName( (coff_sec_handle)orl_sec_hnd ) );
    case( ORL_OMF ):
        return( OmfSecGetName( (omf_sec_handle)orl_sec_hnd ) );
    default: break;//ORL_UNRECOGNIZED_FORMAT
    }
    return( NULL );
}

orl_sec_offset ORLENTRY ORLSecGetBase( orl_sec_handle orl_sec_hnd )
{
    switch( ORL_SEC_TYPE( orl_sec_hnd ) ) {
    case( ORL_ELF ):
        return( ElfSecGetBase( (elf_sec_handle)orl_sec_hnd ) );
    case( ORL_COFF ):
        return( CoffSecGetBase( (coff_sec_handle)orl_sec_hnd ) );
    case( ORL_OMF ):
        return( OmfSecGetBase( (omf_sec_handle)orl_sec_hnd ) );
    default: break;//ORL_UNRECOGNIZED_FORMAT
    }
    return( 0 );
}

orl_sec_size ORLENTRY ORLSecGetSize( orl_sec_handle orl_sec_hnd )
{
    switch( ORL_SEC_TYPE( orl_sec_hnd ) ) {
    case( ORL_ELF ):
        return( ElfSecGetSize( (elf_sec_handle)orl_sec_hnd ) );
    case( ORL_COFF ):
        return( CoffSecGetSize( (coff_sec_handle)orl_sec_hnd ) );
    case( ORL_OMF ):
        return( OmfSecGetSize( (omf_sec_handle)orl_sec_hnd ) );
    default: break;//ORL_UNRECOGNIZED_FORMAT
    }
    return( 0 );
}

orl_sec_type ORLENTRY ORLSecGetType( orl_sec_handle orl_sec_hnd )
{
    switch( ORL_SEC_TYPE( orl_sec_hnd ) ) {
    case( ORL_ELF ):
        return( ElfSecGetType( (elf_sec_handle)orl_sec_hnd ) );
    case( ORL_COFF ):
        return( CoffSecGetType( (coff_sec_handle)orl_sec_hnd ) );
    case( ORL_OMF ):
        return( OmfSecGetType( (omf_sec_handle)orl_sec_hnd ) );
    default: break;//ORL_UNRECOGNIZED_FORMAT
    }
    return( 0 );
}

orl_sec_alignment ORLENTRY ORLSecGetAlignment( orl_sec_handle orl_sec_hnd )
{
    switch( ORL_SEC_TYPE( orl_sec_hnd ) ) {
    case( ORL_ELF ):
        return( ElfSecGetAlignment( (elf_sec_handle)orl_sec_hnd ) );
    case( ORL_COFF ):
        return( CoffSecGetAlignment( (coff_sec_handle)orl_sec_hnd ) );
    case( ORL_OMF ):
        return( OmfSecGetAlignment( (omf_sec_handle)orl_sec_hnd ) );
    default: break;//ORL_UNRECOGNIZED_FORMAT
    }
    return( 0 );
}

orl_sec_flags ORLENTRY ORLSecGetFlags( orl_sec_handle orl_sec_hnd )
{
    switch( ORL_SEC_TYPE( orl_sec_hnd ) ) {
    case( ORL_ELF ):
        return( ElfSecGetFlags( (elf_sec_handle)orl_sec_hnd ) );
    case( ORL_COFF ):
        return( CoffSecGetFlags( (coff_sec_handle)orl_sec_hnd ) );
    case( ORL_OMF ):
        return( OmfSecGetFlags( (omf_sec_handle)orl_sec_hnd ) );
    default: break;//ORL_UNRECOGNIZED_FORMAT
    }
    return( 0 );
}

orl_sec_handle ORLENTRY ORLSecGetStringTable( orl_sec_handle orl_sec_hnd )
{
    switch( ORL_SEC_TYPE( orl_sec_hnd ) ) {
    case( ORL_ELF ):
        return( (orl_sec_handle) ElfSecGetStringTable( (elf_sec_handle)orl_sec_hnd ) );
    case( ORL_COFF ):
        return( (orl_sec_handle) CoffSecGetStringTable( (coff_sec_handle)orl_sec_hnd ) );
    case( ORL_OMF ):
        return( (orl_sec_handle) OmfSecGetStringTable( (omf_sec_handle)orl_sec_hnd ) );
    default: break;//ORL_UNRECOGNIZED_FORMAT
    }
    return( NULL );
}

orl_sec_handle ORLENTRY ORLSecGetSymbolTable( orl_sec_handle orl_sec_hnd )
{
    switch( ORL_SEC_TYPE( orl_sec_hnd ) ) {
    case( ORL_ELF ):
        return( (orl_sec_handle) ElfSecGetSymbolTable( (elf_sec_handle)orl_sec_hnd ) );
    case( ORL_COFF ):
        return( (orl_sec_handle) CoffSecGetSymbolTable( (coff_sec_handle)orl_sec_hnd ) );
    case( ORL_OMF ):
        return( (orl_sec_handle) OmfSecGetSymbolTable( (omf_sec_handle)orl_sec_hnd ) );
    default: break;//ORL_UNRECOGNIZED_FORMAT
    }
    return( NULL );
}

orl_sec_handle ORLENTRY ORLSecGetRelocTable( orl_sec_handle orl_sec_hnd )
{
    switch( ORL_SEC_TYPE( orl_sec_hnd ) ) {
    case( ORL_ELF ):
        return( (orl_sec_handle) ElfSecGetRelocTable( (elf_sec_handle)orl_sec_hnd ) );
    case( ORL_COFF ):
        return( (orl_sec_handle) CoffSecGetRelocTable( (coff_sec_handle)orl_sec_hnd ) );
    case( ORL_OMF ):
        return( (orl_sec_handle) OmfSecGetRelocTable( (omf_sec_handle)orl_sec_hnd ) );
    default: break;//ORL_UNRECOGNIZED_FORMAT
    }
    return( NULL );
}

orl_linnum * ORLENTRY ORLSecGetLines( orl_sec_handle orl_sec_hnd )
{
    switch( ORL_SEC_TYPE( orl_sec_hnd ) ) {
    case( ORL_ELF ):
        return( NULL );
    case( ORL_COFF ):
        return( CoffSecGetLines( (coff_sec_handle)orl_sec_hnd ) );
    case( ORL_OMF ):
        return( OmfSecGetLines( (omf_sec_handle)orl_sec_hnd ) );
    default: break;//ORL_UNRECOGNIZED_FORMAT
    }
    return NULL;
}

orl_table_index ORLENTRY ORLSecGetNumLines( orl_sec_handle orl_sec_hnd )
{
    switch( ORL_SEC_TYPE( orl_sec_hnd ) ) {
    case( ORL_ELF ):
        return( 0 );
    case( ORL_COFF ):
        return( CoffSecGetNumLines( (coff_sec_handle)orl_sec_hnd ) );
    case( ORL_OMF ):
        return( OmfSecGetNumLines( (omf_sec_handle)orl_sec_hnd ) );
    default: break;//ORL_UNRECOGNIZED_FORMAT
    }
    return 0;
}

orl_sec_offset ORLENTRY ORLSecGetOffset( orl_sec_handle orl_sec_hnd )
{
    if( ORL_SEC_TYPE( orl_sec_hnd ) == ORL_COFF ) {
        return( CoffSecGetOffset( (coff_sec_handle)orl_sec_hnd ) );
    }
    return 0;
}

char * ORLENTRY ORLSecGetClassName( orl_sec_handle orl_sec_hnd )
{
    if( ORL_SEC_TYPE( orl_sec_hnd ) == ORL_OMF ) {
        return( OmfSecGetClassName( (omf_sec_handle)orl_sec_hnd ) );
    }
    return 0;
}

orl_sec_combine ORLENTRY ORLSecGetCombine( orl_sec_handle orl_sec_hnd )
{
    if( ORL_SEC_TYPE( orl_sec_hnd ) == ORL_OMF ) {
        return( OmfSecGetCombine( (omf_sec_handle)orl_sec_hnd ) );
    }
    return ORL_SEC_COMBINE_NONE;
}

orl_sec_frame ORLENTRY ORLSecGetAbsFrame( orl_sec_handle orl_sec_hnd )
{
    if( ORL_SEC_TYPE( orl_sec_hnd ) == ORL_OMF ) {
        return( OmfSecGetAbsFrame( (omf_sec_handle)orl_sec_hnd ) );
    }
    return ORL_SEC_NO_ABS_FRAME;
}

orl_sec_handle ORLENTRY ORLSecGetAssociated( orl_sec_handle orl_sec_hnd )
{
    if( ORL_SEC_TYPE( orl_sec_hnd ) == ORL_OMF ) {
        return( OmfSecGetAssociated( (omf_sec_handle)orl_sec_hnd ) );
    }
    return NULL;
}

orl_group_handle ORLENTRY ORLSecGetGroup( orl_sec_handle orl_sec_hnd )
{
    if( ORL_SEC_TYPE( orl_sec_hnd ) == ORL_OMF ) {
        return( OmfSecGetGroup( (omf_sec_handle)orl_sec_hnd ) );
    }
    return NULL;
}

orl_return ORLENTRY ORLSecGetContents( orl_sec_handle orl_sec_hnd, unsigned char **buffer )
{
    switch( ORL_SEC_TYPE( orl_sec_hnd ) ) {
    case( ORL_ELF ):
        return( ElfSecGetContents( (elf_sec_handle)orl_sec_hnd, buffer ) );
    case( ORL_COFF ):
        return( CoffSecGetContents( (coff_sec_handle)orl_sec_hnd, buffer ) );
    case( ORL_OMF ):
        return( OmfSecGetContents( (omf_sec_handle)orl_sec_hnd, buffer ) );
    default: break;//ORL_UNRECOGNIZED_FORMAT
    }
    return( ORL_ERROR );
}

orl_return ORLENTRY ORLSecQueryReloc( orl_sec_handle orl_sec_hnd, orl_sec_offset sec_offset, orl_reloc_return_func return_func )
{
    switch( ORL_SEC_TYPE( orl_sec_hnd ) ) {
    case( ORL_ELF ):
        return( ElfSecQueryReloc( (elf_sec_handle)orl_sec_hnd, sec_offset, return_func ) );
    case( ORL_COFF ):
        return( CoffSecQueryReloc( (coff_sec_handle)orl_sec_hnd, sec_offset, return_func ) );
    case( ORL_OMF ):
        return( OmfSecQueryReloc( (omf_sec_handle)orl_sec_hnd, sec_offset, return_func ) );
    default: break;//ORL_UNRECOGNIZED_FORMAT
    }
    return( ORL_ERROR );
}

orl_return ORLENTRY ORLSecScanReloc( orl_sec_handle orl_sec_hnd, orl_reloc_return_func return_func )
{
    switch( ORL_SEC_TYPE( orl_sec_hnd ) ) {
    case( ORL_ELF ):
        return( ElfSecScanReloc( (elf_sec_handle)orl_sec_hnd, return_func ) );
    case( ORL_COFF ):
        return( CoffSecScanReloc( (coff_sec_handle)orl_sec_hnd, return_func ) );
    case( ORL_OMF ):
        return( OmfSecScanReloc( (omf_sec_handle)orl_sec_hnd, return_func ) );
    default: break;//ORL_UNRECOGNIZED_FORMAT
    }
    return( ORL_ERROR );
}


orl_table_index ORLENTRY ORLCvtSecHdlToIdx( orl_sec_handle orl_sec_hnd )
/***************************************************************/
{
    switch( ORL_SEC_TYPE( orl_sec_hnd ) ) {
    case( ORL_ELF ):
        return ElfCvtSecHdlToIdx( (elf_sec_handle)orl_sec_hnd );
    case( ORL_COFF ):
        return CoffCvtSecHdlToIdx( (coff_sec_handle)orl_sec_hnd );
    case( ORL_OMF ):
        return OmfCvtSecHdlToIdx( (omf_sec_handle)orl_sec_hnd );
    default: break;//ORL_UNRECOGNIZED_FORMAT
    }
    return( 0 );
}

orl_sec_handle ORLENTRY ORLCvtIdxToSecHdl( orl_file_handle orl_file_hnd,
                                           orl_table_index idx )
/**************************************************************/
{
    switch( ORL_FILE_TYPE( orl_file_hnd ) ) {
    case( ORL_ELF ):
        return (orl_sec_handle) ElfCvtIdxToSecHdl( ORL_FILE_HND_ELF( orl_file_hnd ), idx );
    case( ORL_COFF ):
        return (orl_sec_handle) CoffCvtIdxToSecHdl( ORL_FILE_HND_COFF( orl_file_hnd ), idx );
    case( ORL_OMF ):
        return (orl_sec_handle) OmfCvtIdxToSecHdl( ORL_FILE_HND_OMF( orl_file_hnd ), idx );
    default: break;//ORL_UNRECOGNIZED_FORMAT
    }
    return( NULL );
}

orl_return ORLENTRY ORLRelocSecScan( orl_sec_handle orl_sec_hnd, orl_reloc_return_func return_func )
{
    switch( ORL_SEC_TYPE( orl_sec_hnd ) ) {
    case( ORL_ELF ):
        return( ElfRelocSecScan( (elf_sec_handle)orl_sec_hnd, return_func ) );
    case( ORL_COFF ):
        return( CoffRelocSecScan( (coff_sec_handle)orl_sec_hnd, return_func ) );
    case( ORL_OMF ):
        return( OmfRelocSecScan( (omf_sec_handle)orl_sec_hnd, return_func ) );
    default: break;//ORL_UNRECOGNIZED_FORMAT
    }
    return( ORL_ERROR );
}

orl_return ORLENTRY ORLSymbolSecScan( orl_sec_handle orl_sec_hnd, orl_symbol_return_func return_func )
{
    switch( ORL_SEC_TYPE( orl_sec_hnd ) ) {
    case( ORL_ELF ):
        return( ElfSymbolSecScan( (elf_sec_handle)orl_sec_hnd, return_func ) );
    case( ORL_COFF ):
        return( CoffSymbolSecScan( (coff_sec_handle)orl_sec_hnd, return_func ) );
    case( ORL_OMF ):
        return( OmfSymbolSecScan( (omf_sec_handle)orl_sec_hnd, return_func ) );
    default: break;//ORL_UNRECOGNIZED_FORMAT
    }
    return( ORL_ERROR );
}

orl_return ORLENTRY ORLNoteSecScan( orl_sec_handle orl_sec_hnd,
                                    orl_note_callbacks * fn, void *cookie )
/*************************************************************************/
{
    switch( ORL_SEC_TYPE( orl_sec_hnd ) ) {
    case( ORL_ELF ):
        return( ElfNoteSecScan( (elf_sec_handle)orl_sec_hnd, fn, cookie ) );
    case( ORL_COFF ):
        return( CoffNoteSecScan( (coff_sec_handle)orl_sec_hnd, fn, cookie ) );
    case( ORL_OMF ):
        return( OmfNoteSecScan( (omf_sec_handle)orl_sec_hnd, fn, cookie ) );
    default: break;//ORL_UNRECOGNIZED_FORMAT
    }
    return( ORL_ERROR );
}


char * ORLENTRY ORLSymbolGetName( orl_symbol_handle orl_symbol_hnd )
{
    switch( ORL_SYMBOL_TYPE( orl_symbol_hnd ) ) {
    case( ORL_ELF ):
        return( ElfSymbolGetName( (elf_symbol_handle)orl_symbol_hnd ) );
    case( ORL_COFF ):
        return( CoffSymbolGetName( (coff_symbol_handle)orl_symbol_hnd ) );
    case( ORL_OMF ):
        return( OmfSymbolGetName( (omf_symbol_handle)orl_symbol_hnd ) );
    default: break;//ORL_UNRECOGNIZED_FORMAT
    }
    return( NULL );
}

orl_symbol_value ORLENTRY ORLSymbolGetValue( orl_symbol_handle orl_symbol_hnd )
{
    switch( ORL_SYMBOL_TYPE( orl_symbol_hnd ) ) {
    case( ORL_ELF ):
        return( ElfSymbolGetValue( (elf_symbol_handle)orl_symbol_hnd ) );
    case( ORL_COFF ):
        return( CoffSymbolGetValue( (coff_symbol_handle)orl_symbol_hnd ) );
    case( ORL_OMF ):
        return( OmfSymbolGetValue( (omf_symbol_handle)orl_symbol_hnd ) );
    default: {   //ORL_UNRECOGNIZED_FORMAT
        unsigned_64 val64 = { 0, 0 };
        return( val64 );
        }
    }
}

orl_symbol_binding ORLENTRY ORLSymbolGetBinding( orl_symbol_handle orl_symbol_hnd )
{
    switch( ORL_SYMBOL_TYPE( orl_symbol_hnd ) ) {
    case( ORL_ELF ):
        return( ElfSymbolGetBinding( (elf_symbol_handle)orl_symbol_hnd ) );
    case( ORL_COFF ):
        return( CoffSymbolGetBinding( (coff_symbol_handle)orl_symbol_hnd ) );
    case( ORL_OMF ):
        return( OmfSymbolGetBinding( (omf_symbol_handle)orl_symbol_hnd ) );
    default: break;//ORL_UNRECOGNIZED_FORMAT
    }
    return( 0 );
}

orl_symbol_type ORLENTRY ORLSymbolGetType( orl_symbol_handle orl_symbol_hnd )
{
    switch( ORL_SYMBOL_TYPE( orl_symbol_hnd ) ) {
    case( ORL_ELF ):
        return( ElfSymbolGetType( (elf_symbol_handle)orl_symbol_hnd ) );
    case( ORL_COFF ):
        return( CoffSymbolGetType( (coff_symbol_handle)orl_symbol_hnd ) );
    case( ORL_OMF ):
        return( OmfSymbolGetType( (omf_symbol_handle)orl_symbol_hnd ) );
    default: break;//ORL_UNRECOGNIZED_FORMAT
    }
    return( 0 );
}

unsigned char ORLENTRY ORLSymbolGetRawInfo( orl_symbol_handle orl_symbol_hnd )
{
    switch( ORL_SYMBOL_TYPE( orl_symbol_hnd ) ) {
    case( ORL_ELF ):
        return( ElfSymbolGetRawInfo( (elf_symbol_handle)orl_symbol_hnd ) );
    case( ORL_COFF ):
        return 0; // no COFF equivilent
    case( ORL_OMF ):
        return( OmfSymbolGetRawInfo( (omf_symbol_handle)orl_symbol_hnd ) );
    default: break;//ORL_UNRECOGNIZED_FORMAT
    }
    return( 0 );
}

orl_sec_handle ORLENTRY ORLSymbolGetSecHandle( orl_symbol_handle orl_symbol_hnd )
{
    switch( ORL_SYMBOL_TYPE( orl_symbol_hnd ) ) {
    case( ORL_ELF ):
        return( (orl_sec_handle) ElfSymbolGetSecHandle( (elf_symbol_handle)orl_symbol_hnd ) );
    case( ORL_COFF ):
        return( (orl_sec_handle) CoffSymbolGetSecHandle( (coff_symbol_handle)orl_symbol_hnd ) );
    case( ORL_OMF ):
        return( (orl_sec_handle) OmfSymbolGetSecHandle( (omf_symbol_handle)orl_symbol_hnd ) );
    default: break;//ORL_UNRECOGNIZED_FORMAT
    }
    return( NULL );
}

orl_symbol_handle ORLENTRY ORLSymbolGetAssociated( orl_symbol_handle orl_symbol_hnd )
{
    switch( ORL_SYMBOL_TYPE( orl_symbol_hnd ) ) {
    case( ORL_ELF ):
        return (orl_symbol_handle) ElfSymbolGetAssociated( (elf_symbol_handle)orl_symbol_hnd );
    case( ORL_COFF ):
        return (orl_symbol_handle) CoffSymbolGetAssociated( (coff_symbol_handle)orl_symbol_hnd );
    case( ORL_OMF ):
        return NULL;    // NYI: call to an OMF func. here.
    default: break;//ORL_UNRECOGNIZED_FORMAT
    }
    return( NULL );
}

orl_return ORLENTRY ORLGroupsScan( orl_file_handle orl_file_hnd,
                                   orl_group_return_func func )
{
    switch( ORL_FILE_TYPE( orl_file_hnd ) ) {
    case( ORL_ELF ):
    case( ORL_COFF ):
        return ORL_OKAY;
    case( ORL_OMF ):
        return OmfGroupsScan( ORL_FILE_HND_OMF( orl_file_hnd ), func );
    default: break;//ORL_UNRECOGNIZED_FORMAT
    }
    return( ORL_ERROR );
}

char * ORLENTRY ORLGroupName( orl_group_handle orl_group_hnd )
{
    if( ORL_GROUP_TYPE( orl_group_hnd ) == ORL_OMF ) {
        return OmfGroupName( (omf_grp_handle)orl_group_hnd );
    }
    return( NULL );
}

orl_table_index ORLENTRY ORLGroupSize( orl_group_handle orl_group_hnd )
{
    if( ORL_GROUP_TYPE( orl_group_hnd ) == ORL_OMF ) {
        return OmfGroupSize( (omf_grp_handle)orl_group_hnd );
    }
    return( 0 );
}

char * ORLENTRY ORLGroupMember( orl_group_handle orl_group_hnd, orl_table_index idx )
{
    if( ORL_GROUP_TYPE( orl_group_hnd ) == ORL_OMF ) {
        return OmfGroupMember( (omf_grp_handle)orl_group_hnd, idx );
    }
    return( NULL );
}
