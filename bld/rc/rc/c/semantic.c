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
* Description:  Semantic actions called by the YACC generated driver
*               (for both grammars).
*
****************************************************************************/


#include <stdio.h>      /* remove call for MS format stuff */
#include "global.h"
#include "errors.h"
#include "semantic.h"
#include "tmpctl.h"
#include "rcrtns.h"
#include "rccore.h"
#include "wres.h"


/* used in the work around for MS format RES files */
static char         *MSFormatTmpFile = NULL;

static WResFileID   save_handle;        /* holding place for the RES file handle */
static char         *save_name;

/* Modified from WINNT.H */
#ifndef MAKELANGID
#define MAKELANGID(p, s)       ((((uint_16)(s)) << 10) | (uint_16)(p))
#endif

SemOffset SemStartResource( void )
/********************************/
{
    if( StopInvoked ) {
        RcFatalError( ERR_STOP_REQUESTED );
    }
    if( CurrResFile.IsWatcomRes ) {
        return( ResTell( CurrResFile.handle ) );
    } else {
        /* save current values */
        save_handle = CurrResFile.handle;
        save_name = CurrResFile.filename;
        /* open a temporary file and trade handles with the RES file */
        MSFormatTmpFile = RcTmpFileName();
        CurrResFile.handle = MResOpenNewFile( MSFormatTmpFile );
        if( CurrResFile.handle == NIL_HANDLE ) {
            CurrResFile.handle = save_handle;
            ResCloseFile( save_handle );
            remove( save_name );
            RcFatalError( ERR_OPENING_TMP, MSFormatTmpFile, LastWresErrStr() );
        } else {
            RegisterTmpFile( MSFormatTmpFile );
            CurrResFile.filename = MSFormatTmpFile;
        }
        /* The start position should be 0 but to be safe call ResTell */
        return( ResTell( CurrResFile.handle ) );
    }
}

SemLength SemEndResource( SemOffset start )
/*****************************************/
{
    SemLength   len;

    if( CurrResFile.IsWatcomRes ) {
        return( ResTell( CurrResFile.handle ) - start );
    } else {
        /* Close the temperary file, reset the RES file handle and return */
        /* the length of the resource */
        len = ResTell( CurrResFile.handle ) - start;

        if( ResCloseFile( CurrResFile.handle ) ) {
            RcError( ERR_CLOSING_TMP, CurrResFile.filename, LastWresErrStr() );
            ErrorHasOccured = true;
        }
        /* restore previous values */
        CurrResFile.handle = save_handle;
        CurrResFile.filename = save_name;
        return( len );
    }
}

void SemAddResourceFree( WResID * name, WResID * type, ResMemFlags flags,
                ResLocation loc )
/***********************************************************************/
{
    SemAddResource2( name, type, flags, loc, NULL );
    RCFREE( name );
    RCFREE( type );
}

static void copyMSFormatRes( WResID * name, WResID * type, ResMemFlags flags,
                ResLocation loc, const WResLangType *lang )
/***************************************************************************/
{
    MResResourceHeader  ms_head;
    long                cur_byte_num;
    uint_8              cur_byte;
    bool                error;
    WResFileID          tmp_handle;

    /* fill in and output a MS format resource header */
    ms_head.Type = WResIDToNameOrOrd( type );
    ms_head.Name = WResIDToNameOrOrd( name );
    ms_head.MemoryFlags = flags;
    ms_head.Size = loc.len;
    ms_head.LanguageId = MAKELANGID( lang->lang, lang->sublang );
    ms_head.Version = 0L; /* Currently Unsupported */
    ms_head.DataVersion = 0L;
    ms_head.Characteristics = 0L; /* Currently Unsupported */

    /* OS/2 resource header happens to be identical to Win16 */
    if( CmdLineParms.TargetOS == RC_TARGET_OS_WIN16 ||
        CmdLineParms.TargetOS == RC_TARGET_OS_OS2 ) {
        error = MResWriteResourceHeader( &ms_head, CurrResFile.handle, false );
    } else {
        error = MResWriteResourceHeader( &ms_head, CurrResFile.handle, true );
    }
    if( error ) {
        RcError( ERR_WRITTING_RES_FILE, CurrResFile.filename, LastWresErrStr() );
        RCFREE( ms_head.Type );
        RCFREE( ms_head.Name );
        ErrorHasOccured = true;
    } else {
        RCFREE( ms_head.Type );
        RCFREE( ms_head.Name );
        tmp_handle = ResOpenFileRO( MSFormatTmpFile );
        if( tmp_handle == NIL_HANDLE ) {
            RcError( ERR_OPENING_TMP, MSFormatTmpFile, LastWresErrStr() );
            ErrorHasOccured = true;
            return;
        }

        /* copy the data from the temperary file to the RES file */
        if( ResSeek( tmp_handle, loc.start, SEEK_SET ) == -1 ) {
            RcError( ERR_READING_TMP, MSFormatTmpFile, LastWresErrStr() );
            ResCloseFile( tmp_handle );
            ErrorHasOccured = true;
            return;
        }

        /* this is very inefficient but hopefully the buffering in layer0.c */
        /* will make it tolerable */
        for( cur_byte_num = 0; cur_byte_num < loc.len; cur_byte_num++ ) {
            error = ResReadUint8( &cur_byte, tmp_handle );
            if( error ) {
                RcError( ERR_READING_TMP, MSFormatTmpFile, LastWresErrStr() );
                ResCloseFile( tmp_handle );
                ErrorHasOccured = true;
                return;
            } else {
                error = ResWriteUint8( cur_byte, CurrResFile.handle );
                if( error ) {
                    RcError( ERR_WRITTING_RES_FILE, CurrResFile.filename, LastWresErrStr() );
                    ResCloseFile( tmp_handle );
                    ErrorHasOccured = true;
                    return;
                }
            }
        }
        if( ResCloseFile( tmp_handle ) ) {
            RcError( ERR_WRITTING_RES_FILE, MSFormatTmpFile, LastWresErrStr() );
            ErrorHasOccured = true;
        }
    }
}

void SemAddResource( WResID * name, WResID * type, ResMemFlags flags, ResLocation loc )
/*************************************************************************************/
{
    SemAddResource2( name, type, flags, loc, NULL );
}

void SemAddResource2( WResID *name, WResID *type, ResMemFlags flags,
                ResLocation loc, const char *filename )
/******************************************************************/
{
    bool                error;
    bool                duplicate;
    char                *namestr;
    const WResLangType  *lang;

    lang = SemGetResourceLanguage();

    // Windows 95 is currently unable to load an exe that contains a resource
    // with numeric type or numeric identifier greater than 0x7FFF
    // so we warn the user
    if( CmdLineParms.TargetOS == RC_TARGET_OS_WIN32 ) {
        if( !type->IsName && type->ID.Num > 0x7FFF ) {
            namestr = WResIDToStr( type );
            RcWarning( ERR_TYPE_GT_7FFF, namestr );
            RCFREE( namestr );
        }
        if( !name->IsName && name->ID.Num > 0x7FFF ) {
            namestr = WResIDToStr( name );
            RcWarning( ERR_NAME_GT_7FFF, namestr );
            RCFREE( namestr );
        }
    }
    error = WResAddResource( type, name, flags, loc.start, loc.len, CurrResFile.dir, lang, &duplicate );

    if( duplicate ) {
        if( filename == NULL ) {
            ReportDupResource( name, type, NULL, NULL, true );
        } else {
            ReportDupResource( name, type, filename, CmdLineParms.InFileName, true );
        }
        /* The resource has already been written but we can't add it to */
        /* directory. This will make the .RES file larger but will otherwise */
        /* not affect it since there will be no references to the resource in */
        /* the directory. */
    } else if( error ) {
        RcError( ERR_OUT_OF_MEMORY );
        ErrorHasOccured = true;
    }

    if( !CurrResFile.IsWatcomRes ) {
        if( !duplicate ) {
            copyMSFormatRes( name, type, flags, loc, lang );
        }
        /* erase the temporary file */
        remove( MSFormatTmpFile );
        UnregisterTmpFile( MSFormatTmpFile );
        RCFREE( MSFormatTmpFile );
        MSFormatTmpFile = NULL;
    }
}

void SemanticInitStatics( void )
/******************************/
{
    save_handle = NIL_HANDLE;
    MSFormatTmpFile = NULL;
}
