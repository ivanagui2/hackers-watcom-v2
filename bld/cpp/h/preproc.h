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


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "bool.h"

#define PPT_NULL            0
#define PPT_SHARP_SHARP     1
#define PPT_LAST_TOKEN      2
#define PPT_EOF             3
#define PPT_SHARP           '#'
#define PPT_LEFT_PAREN      '('
#define PPT_RIGHT_PAREN     ')'
#define PPT_COMMA           ','
#define PPT_ID              'A'
#define PPT_TEMP_ID         'a'
#define PPT_SAVED_ID        'B'
#define PPT_COMMENT         'C'
#define PPT_MACRO_PARM      'P'
#define PPT_NUMBER          '0'
#define PPT_LITERAL         '\"'
#define PPT_WHITE_SPACE     ' '
#define PPT_OTHER           '$'

#define PPBUFSIZE           8192
#define HASH_SIZE           211

#define PPTYPE_SIGNED       0
#define PPTYPE_UNSIGNED     1

#define PPFLAG_PREPROCESSING    0x0001
#define PPFLAG_EMIT_LINE        0x0002
#define PPFLAG_SKIP_COMMENT     0x0004
#define PPFLAG_KEEP_COMMENTS    0x0008
#define PPFLAG_IGNORE_INCLUDE   0x0010
#define PPFLAG_DEPENDENCIES     0x0020
#define PPFLAG_ASM_COMMENT      0x0040
#define PPFLAG_IGNORE_CWD       0x0080
#define PPFLAG_IGNORE_DEFDIRS   0x0100
#define PPFLAG_DB_KANJI         0x0200
#define PPFLAG_DB_CHINESE       0x0400
#define PPFLAG_DB_KOREAN        0x0800
#define PPFLAG_UTF8             0x1000
#define PPFLAG_DONT_READ        0x4000
#define PPFLAG_UNDEFINED_VAR    0x8000

#define CC_ALPHA            1
#define CC_DIGIT            2

#define PPINCLUDE_USR       0
#define PPINCLUDE_SYS       1
#define PPINCLUDE_SRC       2

typedef struct macro_entry {
    struct macro_entry *next;
    char            *replacement_list;
    unsigned char   parmcount;      /* 255 - indicates special macro */
    char            name[1];
} MACRO_ENTRY;
#define PP_SPECIAL_MACRO        255

typedef struct macro_token {
    struct macro_token *next;
    char    token;
    char    data[1];
} MACRO_TOKEN;

typedef struct  file_list {
    struct file_list *prev_file;
    char             *prev_bufptr;
    char             *filename;
    FILE             *handle;
    unsigned         linenum;
    char             buffer[PPBUFSIZE+2];
} FILELIST;

typedef struct preproc_value {
    int             type;   // PPTYPE_SIGNED or PPTYPE_UNSIGNED
    union {
        long int    ivalue;
        unsigned long uvalue;
    } val;
} PREPROC_VALUE;

typedef void        pp_callback(const char *, const char *, int);

extern  int         PP_Init( const char *__filename, unsigned __flags, const char *__incpath);
extern  int         PP_Init2( const char *filename, unsigned flags, const char *include_path, const char *leadbytes );
extern  void        PP_Dependency_List(pp_callback *);
extern  void        PP_SetLeadBytes( const char *bytes );
extern  int         PP_Char(void);
extern  int         PP_Class(char __c);
extern  void        PP_Fini(void);
extern  void        PP_Define( char *__p );
extern  MACRO_ENTRY *PP_AddMacro( const char *__name );
extern  MACRO_ENTRY *PP_MacroLookup( const char *__name );
extern  MACRO_ENTRY *PP_ScanMacroLookup( char *__name );
extern  char        *PP_ScanToken( char *__p, char *__token );
extern  int         PP_ScanNextToken( char *__token );
extern  char        *PP_SkipWhiteSpace( char *__p, char *__white_space );
extern  char        *PP_ScanName( char *__p );
extern  int         PPEvalExpr( char *__p, char **__endptr, PREPROC_VALUE *__val );
extern  void        PP_ConstExpr( PREPROC_VALUE * );
extern  MACRO_TOKEN *PPNextToken(void);
extern  MACRO_TOKEN *NextMToken(void);
extern  void        DeleteNestedMacro(void);
extern  void        DoMacroExpansion( MACRO_ENTRY *__me );
extern  void        PP_AddIncludePath( const char *path_list );
extern  void        PP_IncludePathInit( void );
extern  void        PP_IncludePathFini( void );
extern  int         PP_FindInclude( const char *filename, char *fullfilename, int sys_include );

extern  void        *PP_Malloc( size_t __size );
extern  void        PP_Free( void *__ptr );
extern  void        PP_OutOfMemory(void);

extern  char        *PP_GetEnv( const char *__name );

extern  void        PreprocVarInit( void );
extern  void        PPMacroVarInit( void );

extern  FILELIST    *PP_File;
extern  unsigned    PPLineNumber;
extern  char        *PPTokenPtr;
extern  char        *PPCharPtr;
extern  MACRO_TOKEN *PPTokenList;
extern  MACRO_TOKEN *PPCurToken;
extern  unsigned    PPFlags;
extern  char        PPSavedChar;    // saved char at end of token
extern  char        PreProcChar;
extern  MACRO_ENTRY *PPHashTable[HASH_SIZE];
