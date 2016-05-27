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
* Description:  Builder tool mainline.
*
****************************************************************************/


#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#ifdef __UNIX__
    #include <unistd.h>
#else
    #include <direct.h>
#endif
#include "watcom.h"
#include "builder.h"

#include "clibext.h"
#include "bldstruc.h"


#define DEFCTLNAME      "builder.ctl"
#define DEFCTLENV       "BUILDER_CTL"

#define DEF_BACKUP      1
#define MAX_BACKUP      9

#define DOS_EOF_CHAR    0x1a

typedef struct ctl_file {
    struct ctl_file     *next;
    char                name[_MAX_PATH];
} ctl_file;

bool               Quiet;
include            *IncludeStk;
FILE               *LogFile;
static ctl_file    *CtlList;
static char        Line[MAX_LINE];
static char        ProcLine[MAX_LINE];
static unsigned    VerbLevel;
static bool        UndefWarn;
static bool        IgnoreErrors;
static unsigned    ParmCount;
static unsigned    LogBackup;

static void PutNumber( char *src, char *dst, unsigned num )
{
    char        dig;
    int         i;

    memset( dst, 0, 5 );
    strncpy( dst, src, 3 );
    dst[0] = '.';
    for( i = 3; i > 0;--i ) {
        dig = num % 10;
        num /= 10;
        if( dig != 0 || dst[i] == '\0' ) {
            dst[i] = dig + '0';
        }
    }
}

static void BackupLog( const char *log_name, unsigned copies )
{
    char        buff[_MAX_PATH2];
    char        *drive;
    char        *dir;
    char        *fn;
    char        *ext;
    char        old_name[_MAX_PATH];
    char        new_name[_MAX_PATH];
    char        temp_ext[5];

    if( copies > MAX_BACKUP )
        copies = MAX_BACKUP;
    if( copies == 0 ) {
        remove( log_name );
        return;
    }
    _splitpath2( log_name, buff, &drive, &dir, &fn, &ext );
    while( copies != 0 ) {
        PutNumber( ext, temp_ext, copies );
        _makepath( new_name, drive, dir, fn, temp_ext );
        remove( new_name );
        if( copies == 1 ) {
            strcpy( old_name, log_name );
        } else {
            PutNumber( ext, temp_ext, copies - 1 );
            _makepath( old_name, drive, dir, fn, temp_ext );
        }
        rename( old_name, new_name );
        --copies;
    }
}

static void AddCtlFile( const char *name )
{
    ctl_file    **owner;
    ctl_file    *curr;

    owner = &CtlList;
    for( ;; ) {
        curr = *owner;
        if( curr == NULL )
            break;
        owner = &curr->next;
    }
    curr = Alloc( sizeof( *curr ) );
    curr->next = NULL;
    strcpy( curr->name, name );
    *owner = curr;
}

static char **getvalue( char **argv, char *buff )
{
    if( argv[0][2] != '\0' ) {
        strcpy( buff, &argv[0][2] );
        return( argv );
    }
    ++argv;
    strcpy( buff, argv[0] );
    return( argv );
}

static void Usage( void )
{
    printf( "Usage: builder [-c <ctl>] [-l <log>] [-b <bak>] [-i] [-v] [-u] [-q] [--] <parm>\n" );
    printf( "    See builder.doc for more information\n" );
    exit( 0 );
}

static void ProcessOptions( char *argv[] )
{
    char        parm_buff[_MAX_PATH];
    bool        opt_end;

    LogBackup = DEF_BACKUP;
    opt_end = false;
    while( argv[0] != NULL ) {
        if( !opt_end && argv[0][0] == '-' ) {
            switch( tolower( argv[0][1] ) ) {
            case 'c':
                argv = getvalue( argv, parm_buff );
                AddCtlFile( parm_buff );
                break;
            case 'l':
                argv = getvalue( argv, parm_buff );
                if( LogFile != NULL ) {
                    Fatal( "-l option specified twice\n" );
                }
                BackupLog( parm_buff, LogBackup );
                OpenLog( parm_buff );
                break;
            case 'b':
                argv = getvalue( argv, parm_buff );
                LogBackup = strtoul( parm_buff, NULL, 0 );
                if( LogBackup > MAX_BACKUP ) {
                    Fatal( "-b value is exceeds maximum of %d\n", MAX_BACKUP );
                }
                break;
            case 'i':
                IgnoreErrors = true;
                break;
            case 'v':
                ++VerbLevel;
                break;
            case 'u':
                UndefWarn = true;
                break;
            case 'q':
                Quiet = true;
                break;
            case '-':
                opt_end = true;
                break;
            default:
                fprintf( stderr, "Unknown option '%c'\n\n", argv[0][1] );
                /* fall through */
            case '?':
                Usage();
                break;
            }
        } else if( !opt_end && strchr( argv[0], '=' ) != NULL ) {
            putenv( argv[0] );
        } else {
            sprintf( parm_buff, "%d",++ParmCount );
            if( setenv( parm_buff, argv[0], 1 ) != 0 ) {
                Fatal( "Can not set parameter %u\n", ParmCount );
            }
        }
        ++argv;
    }
}

static void PushInclude( const char *name )
{
    include     *new;
    char        buff[_MAX_PATH2];
    char        *drive;
    char        *dir;
    char        *fn;
    char        *ext;
    char        dir_name[_MAX_PATH];

    new = Alloc( sizeof( *new ) );
    new->prev = IncludeStk;
    new->skipping = 0;
    new->ifdefskipping = 0;
    new->lineno = 0;
    new->reset_abit = NULL;
    IncludeStk = new;
    new->fp = fopen( name, "rb" );      // We will cook (handle \r) internally
    if( new->fp == NULL ) {
        Fatal( "Could not open '%s': %s\n", name, strerror( errno ) );
    }
    strcpy( new->name, name );
    _splitpath2( name, buff, &drive, &dir, &fn, &ext );
    _makepath( dir_name, drive, dir, NULL, NULL );
    if( SysChdir( dir_name ) != 0 ) {
        Fatal( "Could not CD to '%s': %s\n", dir_name, strerror( errno ) );
    }
    getcwd( IncludeStk->cwd, sizeof( IncludeStk->cwd ) );
}

static bool PopInclude( void )
{
    include     *curr;

    curr = IncludeStk;
    fclose( curr->fp );
    IncludeStk = curr->prev;
    ResetArchives( curr->reset_abit );
    free( curr );
    if( IncludeStk == NULL )
        return( false );
    SysChdir( IncludeStk->cwd );
    return( true );
}

static bool GetALine( char *line )
{
    for( ;; ) {
        fgets( line, MAX_LINE, IncludeStk->fp );
        if( ferror( IncludeStk->fp ) ) {
            Fatal( "Error reading '%s' line %d: %s\n", IncludeStk->name, IncludeStk->lineno + 1, strerror( errno ) );
        }
        if( !feof( IncludeStk->fp ) ) {
            IncludeStk->lineno++;
            break;
        }
        if( !PopInclude() ) {
            return( false );
        }
    }
    return( true );
}

static char *SubstOne( const char **inp, char *out )
{
    const char  *in;
    char        *p;
    char        *starpos;
    const char  *rep;
    unsigned    parm;

    in = *inp;
    p = out;
    for( ;; ) {
        switch( *in ) {
        case '>':
            *p = '\0';
            // If the parameter is a number (n) followed by an asterisk,
            // copy from parameter n to the end to out. E.g. <2*>
            parm = 1;
            for( starpos = out; isdigit( *starpos ); starpos++ )
                ;
            if( stricmp( starpos, "*" ) == 0 ) {
                rep = NULL;
                p = out;
                sscanf( out, "%u", &parm );
                for( ; parm <= ParmCount; ++parm ) {
                    sprintf( out, "%d", parm );
                    rep = getenv( out );
                    if( rep != NULL ) {
                        if( out != p )
                            *out++ = ' ';
                        strcpy( out, rep );
                        out += strlen( out );
                    }
                }
                *inp = in + 1;
                *out = '\0';
                return( out );
            } else if( stricmp( out, "CWD" ) == 0 ) {
                rep = IncludeStk->cwd;
            } else {
                rep = getenv( out );
            }
            if( rep == NULL ) {
                if( UndefWarn ) {
                    Log( false, "<%s> is undefined\n", out );
                }
                rep = "";
            }
            strcpy( out, rep );
            *inp = in + 1;
            return( out + strlen( out ) );
        case '<':
            ++in;
            p = SubstOne( &in, p );
            break;
        case '\0':
            Fatal( "Missing '>' in '%s' line %d\n", IncludeStk->name, IncludeStk->lineno );
            break;
        default:
            *p++ = *in++;
            break;
        }
    }
}

static void SubstLine( const char *in, char *out )
{
    bool        first;

    first = true;
    in = SkipBlanks( in );
    for( ;; ) {
        switch( *in ) {
        case '^':                       // Escape next byte special meaning
            ++in;
            switch( *in ) {
            case '\n':
            case '\0':
            case '\r':                  // Allow DOS line in UNIX port
            case DOS_EOF_CHAR:          // Allow DOS EOF in UNIX port
                break;
            default:
                *out++ = *in++;
                break;
            }
            break;
        case '[':                       // Surround special chars with a space
        case ']':
        case '(':
        case ')':
            if( !first )
                *out++ = ' ';
            *out++ = *in++;
            *out++ = ' ';
            break;
        case '<':
            ++in;
            out = SubstOne( &in, out );
            break;
        case '\n':
        case '\0':
        case '\r':                      // Allow DOS line in UNIX port
        case DOS_EOF_CHAR:              // Allow DOS EOF in UNIX port
            *out = '\0';
            return;
        default:
            *out++ = *in++;
        }
        first = false;
    }
}

static char *FirstWord( char *p )
{
    char        *start;

    p = SkipBlanks( p );
    if( *p == '\0' )
        return( NULL );
    start = p;
    for( ;; ) {
        switch( *p ) {
        case '\0':
            p[1] = '\0';
            /* fall through */
        case ' ':
        case '\t':
            *p = '\0';
            return( start );
        }
        ++p;
    }
}

static char *NextWord( char *p )
{
    return( FirstWord( p + strlen( p ) + 1 ) );
}


/****************************************************************************
*
* MatchFound. Examines a string of space separated words. If the first word or
* words (between parentheses) matches any of the words following it, returns 1.
* If not, returns 0. String is terminated by 0 or ']'.
* If there isn't at least one word in the string, terminates program.
*
***************************************************************************/
static int MatchFound( char *p )
{
    char    *Match[20];                     // 20 is enough for builder
    int     MatchWords = 0;
    int     i;
    int     EmptyOk = false;
    int     WordsExamined = 0;

    p = NextWord( p );
    if( p == NULL )
        Fatal( "Missing match word\n" );

    if( *p == '(' ) { // Multiple match words, store them
        p = NextWord( p );
        for( ; MatchWords < 20; ) {
            if( p == NULL )
                Fatal( "Missing match word\n" );
            if( stricmp( p, "\"\"" ) == 0 ) // 'No parameter' indicator
                EmptyOk = true;
            else
                Match[MatchWords++] = p;
            p = NextWord( p );
            if( strcmp( p, ")" ) == 0 ) {
                p = NextWord( p );
                break;
            }
        }
    } else {
        Match[MatchWords++] = p;
        p = NextWord( p );
    }

    // At this point, p must point to the first word after the (last) match word

    for( ;; ) {
        if( p == NULL || strcmp( p, "]" ) == 0 ) { // End of string
            if( WordsExamined == 0 && EmptyOk )
                return 1;
            else
                return 0;
        }
        WordsExamined++;
        for( i = 0; i < MatchWords; i++ )
            if( stricmp( Match[i], p ) == 0 )
                return 1;
        p = NextWord( p );
    }
}


static int ProcessCtlFile( const char *name )
{
    char        *p;
    char        *log_name;
    int         res;
    bool        logit;
    bool        pmake;
    int         rc;

    rc = 0;
    PushInclude( name );
    while( GetALine( Line ) ) {
        SubstLine( Line, ProcLine );
        p = ProcLine;
        switch( *p ) {
        case '#':
        case '\0':
            /* just a comment */
            break;
        case '[':
            /* a directive */
            p = FirstWord( p + 1 );
            if( stricmp( p, "INCLUDE" ) == 0 ) {
                if( !IncludeStk->skipping && !IncludeStk->ifdefskipping ) {
                    PushInclude( NextWord( p ) );
                }
            } else if( stricmp( p, "LOG" ) == 0 ) {
                if( IncludeStk->skipping == 0 ) {
                    log_name = NextWord( p );
                    p = NextWord( log_name );
                    if( p == NULL || strcmp( p, "]" ) == 0 ) {
                        BackupLog( log_name, LogBackup );
                    } else {
                        BackupLog( log_name, strtoul( p, NULL, 0 ) );
                    }
                    if( LogFile == NULL ) {
                        OpenLog( log_name );
                    }
                }
            } else if( stricmp( p, "BLOCK" ) == 0 ) {
                IncludeStk->skipping = 0;   // New block: reset skip flags
                IncludeStk->ifdefskipping = 0;
                if( !MatchFound( p ) )
                    IncludeStk->skipping++;
                break;
            } else if( stricmp( p, "IFDEF" ) == 0 ) {
                if( IncludeStk->ifdefskipping != 0 )
                    IncludeStk->ifdefskipping--;
                if( !MatchFound( p ) )
                    IncludeStk->ifdefskipping++;
                break;
            } else if( stricmp( p, "ENDIF" ) == 0 ) {
                if( IncludeStk->ifdefskipping != 0 )
                    IncludeStk->ifdefskipping--;
                break;
            } else {
                Fatal( "Unknown directive '%s' in '%s' line %d\n", p, IncludeStk->name, IncludeStk->lineno );
            }
            break;
        default:
            /* a command */
            logit = ( VerbLevel > 0 );
            if( *p == '@' ) {
                logit = false;
                p = SkipBlanks( p + 1 );
            }
            if( IncludeStk->skipping == 0 && IncludeStk->ifdefskipping == 0 ) {
                if( logit ) {
                    Log( false, "+++<%s>+++\n", p );
                }
                strcpy( Line, p );
                pmake = ( strnicmp( p, "PMAKE", 5 ) == 0 && p[5] == ' ' );
                res = RunIt( p, IgnoreErrors );
                if( res != 0 ) {
                    if( !IgnoreErrors || !pmake ) {
                        if( !logit ) {
                            Log( false, "<%s> => ", Line );
                        }
                        Log( false, "non-zero return: %d\n", res );
                    }
                    if( !IgnoreErrors ) {
                        Fatal( "Build failed\n" );
                    }
                    rc = res;
                }
                LogFlush();
            } else if( logit && ( VerbLevel > 1 ) ) {
                Log( false, "---<%s>---\n", p );
            }
        }
    }
    return( rc );
}

static bool SearchUpDirs( const char *name, char *result )
{
    char        buff[_MAX_PATH2];
    char        *drive;
    char        *dir;
    char        *fn;
    char        *ext;
    char        *end;
    FILE        *fp;

    _fullpath( result, name, _MAX_PATH );
    for( ;; ) {
        fp = fopen( result, "r" );
        if( fp != NULL ) {
            fclose( fp );
            return( true );
        }
        _splitpath2( result, buff, &drive, &dir, &fn, &ext );
        end = &dir[strlen( dir ) - 1];
        if( end == dir )
            return( false );
        switch( *end ) {
        case '\\':
        case '/':
            --end;
        }
        for( ;; ) {
            if( end == dir ) {
                *end++ = '/';
                break;
            }
            if( *end == '\\' || *end == '/' )
                break;
            --end;
        }
        *end = '\0';
        _makepath( result, drive, dir, fn, ext );
    }
}


int main( int argc, char *argv[] )
{
    ctl_file    *next;
    const char  *p;
    int         rc = 0;

    SysInit( argc, argv );
    ProcessOptions( argv + 1 );
    if( CtlList == NULL ) {
        p = getenv( DEFCTLENV );
        if( p == NULL )
            p = DEFCTLNAME;
        if( !SearchUpDirs( p, Line ) ) {
            Fatal( "Can not find '%s'\n", p );
        }
        AddCtlFile( Line );
    }
    while( CtlList != NULL ) {
        if( ProcessCtlFile( CtlList->name ) ) {
            rc = 1;
        }
        next = CtlList->next;
        free( CtlList );
        CtlList = next;
    }
    CloseLog();
    return( rc );
}
