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
#include <stdarg.h>
#if defined( __WATCOMC__ ) || !defined( __UNIX__ )
#include <malloc.h>
#include <process.h>
#endif
#include "global.h"
#include "errprt.h"
#include "idedll.h"
#include "rcmem.h"
#include "rcspawn.h"
#include "rcldstr.h"
#include "errors.h"
#include "banner.h"
#include "rc.h"
#include "rccore.h"

#include "clibext.h"


#define PRINTF_BUF_SIZE         2048

extern void InitGlobs( void );
extern void FiniGlobs( void );
extern void RCmain( void );

#ifdef __OSI__
extern char *_Copyright;
#endif

jmp_buf     jmpbuf_RCFatalError;

static IDECBHdl         cbHandle;
static IDECallBacks     *ideCb;
static IDEInitInfo      *initInfo;

static IDEMsgSeverity SeverityMap[] = {
//   IDE msg severity       WRC msg severity
    IDEMSGSEV_BANNER,       // SEV_BANNER
    IDEMSGSEV_DEBUG,        // SEV_DEBUG
    IDEMSGSEV_WARNING,      // SEV_WARNING
    IDEMSGSEV_ERROR,        // SEV_ERROR
    IDEMSGSEV_ERROR         // SEV_FATAL_ERR
};

static char     formatBuffer[PRINTF_BUF_SIZE];
static char     *curBufPos;

static void flushPrintf( void ) {
/********************************/

    OutPutInfo          info;

    if( curBufPos != formatBuffer ) {
        InitOutPutInfo( &info );
        info.severity = SEV_WARNING;
        RcMsgFprintf( NULL, &info, "\n" );
    }
}

static void setPrintInfo( IDEMsgInfo *buf, OutPutInfo *src, char *msg )
/*********************************************************************/
{
    if( src == NULL ) {
        IdeMsgInit( buf, IDEMSGSEV_DEBUG, msg );
    } else {
        IdeMsgInit( buf, SeverityMap[src->severity], msg );
        if( src->flags & OUTFLAG_FILE ) {
            IdeMsgSetSrcFile( buf, src->file );
        }
        if( src->flags & OUTFLAG_ERRID ) {
            IdeMsgSetMsgNo( buf, src->errid ) ;
            IdeMsgSetHelp( buf, "wrcerrs.hlp", src->errid );
        }
        if( src->flags & OUTFLAG_LINE ) {
            IdeMsgSetSrcLine( buf, src->lineno );
        }
    }
}

void InitOutPutInfo( OutPutInfo *info )
/*************************************/
{
    info->flags = 0;
}

int RcMsgFprintf( FILE *fp, OutPutInfo *info, const char *format, ... )
/*********************************************************************/
{
    int             err;
    va_list         args;
    char            *start;
    char            *end;
    size_t          len;
    IDEMsgInfo      msginfo;

    fp = fp;
    va_start( args, format );
    err = vsnprintf( curBufPos, PRINTF_BUF_SIZE - ( curBufPos - formatBuffer ), format, args );
    va_end( args );
    start = formatBuffer;
    end = curBufPos;
    for( ;; ) {
        if( *end == '\n' ) {
            *end = '\0';
            setPrintInfo( &msginfo, info, start );
            ideCb->PrintWithInfo( cbHandle, &msginfo );
            start = end + 1;
        } else if( *end == '\0' ) {
            len = strlen( start );
            memmove( formatBuffer, start, len + 1 );
            curBufPos = formatBuffer + len;
            break;
        }
        end++;
    }
    return( err );
}

const char *RcGetEnv( const char *name )
/**************************************/
{
    const char  *val;

    if( ideCb != NULL && !initInfo->ignore_env ) {
        if( !ideCb->GetInfo( cbHandle, IDE_GET_ENV_VAR, (IDEGetInfoWParam)name, (IDEGetInfoLParam)&val ) ) {
            return( val );
        }
    }
    return( NULL );
}

static const char * BannerText =
#if defined( _BETAVER )
    banner1w1( "Windows and OS/2 Resource Compiler" )"\n"
    banner1w2( _WRC_VERSION_ )"\n"
#else
    banner1w( "Windows and OS/2 Resource Compiler", _WRC_VERSION_ )"\n"
#endif
    banner2         "\n"
    banner2a("1993") "\n"
    banner3         "\n"
    banner3a        "\n"
;

static void RcIoPrintBanner( void )
/*********************************/
{
    OutPutInfo          errinfo;

    InitOutPutInfo( &errinfo );
    errinfo.severity = SEV_BANNER;
    RcMsgFprintf( stderr, &errinfo, BannerText );
}

static void RcIoPrintHelp( void )
/*******************************/
{
    char        progfname[_MAX_FNAME];
    int         index;
    char        buf[256];
    OutPutInfo  errinfo;
    char        imageName[_MAX_PATH];

    _cmdname( imageName );
    InitOutPutInfo( &errinfo );
    errinfo.severity = SEV_BANNER;
#ifdef __OSI__
    if( _Copyright != NULL ) {
        RcMsgFprintf( stdout, &errinfo, "%s\n", _Copyright );
    }
#endif
    _splitpath( imageName, NULL, NULL, progfname, NULL );
    strlwr( progfname );

    index = USAGE_MSG_FIRST;
    GetRcMsg( index, buf, sizeof( buf ) );
    RcMsgFprintf( stdout, &errinfo, buf, progfname );
    RcMsgFprintf( stdout, &errinfo, "\n" );
    for( ++index; index <= USAGE_MSG_LAST; index++ ) {
        GetRcMsg( index, buf, sizeof( buf ) );
        RcMsgFprintf( stdout, &errinfo, "%s\n", buf );
    }
}

static int RCMainLine( const char *opts, int argc, char **argv )
/**************************************************************/
{
    char        *cmdbuf = NULL;
    const char  *str;
    char        infile[_MAX_PATH + 2];  // +2 for quotes
    char        outfile[_MAX_PATH + 6]; // +6 for -fo="" or -fe=""
    bool        pass1;
    int         i;
    int         rc;

    curBufPos = formatBuffer;
    RcMemInit();
    InitGlobs();
    rc = setjmp( jmpbuf_RCFatalError );
    if( rc == 0 ) {
        InitRcMsgs();
        if( opts != NULL ) {
            str = opts;
            argc = ParseEnvVar( str, NULL, NULL );
            argv = RcMemMalloc( ( argc + 4 ) * sizeof( char * ) );
            cmdbuf = RcMemMalloc( strlen( str ) + argc + 1 );
            ParseEnvVar( str, argv, cmdbuf );
            pass1 = false;
            for( i = 0; i < argc; i++ ) {
                if( argv[i] != NULL && !stricmp( argv[i], "-r" ) ) {
                    pass1 = true;
                    break;
                }
            }
            if( initInfo != NULL && initInfo->ver > 1 && initInfo->cmd_line_has_files ) {
                if( !ideCb->GetInfo( cbHandle, IDE_GET_SOURCE_FILE, 0, (IDEGetInfoLParam)( infile + 1 ) ) ) {
                    infile[0] = '\"';
                    strcat( infile, "\"" );
                    argv[argc++] = infile;
                }
                if( !ideCb->GetInfo( cbHandle, IDE_GET_TARGET_FILE, 0, (IDEGetInfoLParam)( outfile + 5 ) ) ) {
                    if( pass1 ) {
                        strcpy( outfile, "-fo=\"" );
                    } else {
                        strcpy( outfile, "-fe=\"" );
                    }
                    strcat( outfile, "\"" );
                    argv[argc++] = outfile;
                }
            }
            argv[argc] = NULL;        // last element of the array must be NULL
        }
        if( !ScanParams( argc, argv ) ) {
            rc = 1;
        }
        if( !CmdLineParms.Quiet ) {
            RcIoPrintBanner();
        }
        if( CmdLineParms.PrintHelp ) {
            RcIoPrintHelp();
        }
        if( rc == 0 ) {
            rc = RCSpawn( RCmain );
        }
        if( opts != NULL ) {
            RcMemFree( argv );
            RcMemFree( cmdbuf );
        }
        FiniRcMsgs();
    }
    FiniGlobs();
    flushPrintf();
    RcMemShutdown();
    return( rc );
}

unsigned IDEAPI IDEGetVersion( void ) {
/*************************************/
    return( IDE_CUR_DLL_VER );
}

IDEBool IDEAPI IDEInitDLL( IDECBHdl hdl, IDECallBacks *cb, IDEDllHdl *info )
/**************************************************************************/
{
    cbHandle = hdl;
    ideCb = cb;
    *info = 0;
    initInfo = NULL;
    // init wrc
    IgnoreINCLUDE = false;
    IgnoreCWD = false;
    return( false );
}

IDEBool IDEAPI IDEPassInitInfo( IDEDllHdl hdl, IDEInitInfo *info )
/****************************************************************/
{
    hdl = hdl;
    if( info == NULL || info->ver < 2 ) {
        return( true );
    }
    if( info->ignore_env ) {
        IgnoreINCLUDE = true;
        IgnoreCWD = true;
    }
    if( info->cmd_line_has_files ) {
//        CompFlags.ide_cmd_line = true;
    }
    if( info->ver >= 3 ) {
        if( info->console_output ) {
//            CompFlags.ide_console_output = true;
        }
        if( info->ver >= 4 ) {
            if( info->progress_messages ) {
//                CompFlags.progress_messages = true;
            }
        }
    }
    initInfo = info;
    return( false );
}

void IDEAPI IDEStopRunning( void )
/********************************/
{
    StopInvoked = true;
}

IDEBool IDEAPI IDERunYourSelf( IDEDllHdl hdl, const char *opts, IDEBool *fatalerr )
/*********************************************************************************/
{
    int         rc;

    hdl = hdl;
    StopInvoked = false;
    if( fatalerr != NULL )
        *fatalerr = false;
    rc = RCMainLine( opts, 0, NULL );
    if( rc == -1 && fatalerr != NULL )
        *fatalerr = true;
    return( rc != 0 );
}

IDEBool IDEAPI IDERunYourSelfArgv( IDEDllHdl hdl, int argc, char **argv, IDEBool* fatalerr )
/******************************************************************************************/
{
    int         rc;

#if !defined( __WATCOMC__ )
    _argc = argc;
    _argv = argv;
#endif
    hdl = hdl;
    StopInvoked = false;
    if( fatalerr != NULL )
        *fatalerr = false;
    rc = RCMainLine( NULL, argc, argv );
    if( rc == -1 && fatalerr != NULL )
        *fatalerr = true;
    return( rc != 0 );
}

void IDEAPI IDEFreeHeap( void )
/*****************************/
{
#if defined( __WATCOMC__ )
    _heapshrink();
#endif
}

void IDEAPI IDEFiniDLL( IDEDllHdl hdl )
/*************************************/
{
    // fini wrc
    hdl = hdl;
}
