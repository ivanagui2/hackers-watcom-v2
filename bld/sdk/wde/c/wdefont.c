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

#include "wdeglbl.h"
#include "wderesin.h"
#include "wdedebug.h"
#include "wdemain.h"
#include "wdelist.h"
#include "wdefutil.h"
#include "wdefont.h"
#include "windlg.h"

/****************************************************************************/
/* macro definitions                                                        */
/****************************************************************************/
#ifdef __NT__
    #define WDEDLGTEMPLATE (LPCDLGTEMPLATE)
#else
    #define WDEDLGTEMPLATE
#endif


/****************************************************************************/
/* external function prototypes                                             */
/****************************************************************************/
WINEXPORT BOOL CALLBACK WdeDummyProc( HWND, UINT, WPARAM, LPARAM );
WINEXPORT int  CALLBACK WdeEnumFontsProc( ENUMLOGFONT *, TEXTMETRIC *, int, LPARAM );

/****************************************************************************/
/* static function prototypes                                               */
/****************************************************************************/
static BOOL     WdeAddFontFamilyMember( WdeFontNames *, ENUMLOGFONT *, TEXTMETRIC *, int );

/****************************************************************************/
/* static variables                                                         */
/****************************************************************************/
static LIST             *WdeFontList;
static LIST             *WdeFontFamiliesList;
static uint_32          logpixelsy;

LIST *WdeGetFontList( void )
{
    return( WdeFontList );
}

void WdeDumpFontList( void )
{
    char            debug[256];
    LIST            *flist;
    LIST            *nlist;
    WdeFontNames    *font_names;
    WdeFontData     *font_data;

    WdeWriteTrail( "Dumping font names: Start." );

    for( flist = WdeFontList; flist != NULL; flist = ListNext( flist ) ) {
        font_names = (WdeFontNames *)ListElement( flist );
        sprintf( debug, "Font Family: :%s: Children: %d",
                 font_names->name, font_names->num_children );
        WdeWriteTrail( debug );
        for( nlist = font_names->family_list; nlist != NULL; nlist = ListNext( nlist ) ) {
            font_data = (WdeFontData *)ListElement( nlist );
            if( font_data->fonttype & TRUETYPE_FONTTYPE ) {
                sprintf( debug, "\tFont: :%s: DeviceType: %d Pnt Size: %2d "
                         "Wgt: %3d It: %3d Ul: %3d SO: %3d CharSet: %3d ",
                         font_data->elf.elfFullName, font_data->fonttype,
                         (int)font_data->pointsize,
                         (int)font_data->elf.elfLogFont.lfWeight,
                         (int)font_data->elf.elfLogFont.lfItalic,
                         (int)font_data->elf.elfLogFont.lfUnderline,
                         (int)font_data->elf.elfLogFont.lfStrikeOut,
                         (int)font_data->elf.elfLogFont.lfCharSet );
            } else {
                sprintf( debug, "\tFont: :%s: DeviceType: %d Pnt Size: %2d "
                         "Wgt: %3d It: %3d Ul: %3d SO: %3d CharSet: %3d ",
                         font_data->elf.elfLogFont.lfFaceName, font_data->fonttype,
                         (int)font_data->pointsize,
                         (int)font_data->elf.elfLogFont.lfWeight,
                         (int)font_data->elf.elfLogFont.lfItalic,
                         (int)font_data->elf.elfLogFont.lfUnderline,
                         (int)font_data->elf.elfLogFont.lfStrikeOut,
                         (int)font_data->elf.elfLogFont.lfCharSet );
            }
            WdeWriteTrail( debug );
        }
    }

    WdeWriteTrail( "Dumping font names: End." );
}

void WdeFreeFontList( void )
{
    LIST         *flist;
    LIST         *nlist;
    WdeFontNames *font_names;
    WdeFontData  *font_data;

    for( flist = WdeFontList; flist != NULL; flist = ListNext( flist ) ) {
        font_names = (WdeFontNames *)ListElement( flist );
        for( nlist = font_names->family_list; nlist != NULL; nlist = ListNext( nlist ) ) {
            font_data = (WdeFontData *)ListElement( nlist );
            WRMemFree( font_data );
        }
        if( font_names->family_list != NULL ) {
            ListFree( font_names->family_list );
        }
        WRMemFree( font_names );
    }
    ListFree( WdeFontList );

    for( flist = WdeFontFamiliesList; flist != NULL; flist = ListNext( flist ) ) {
        font_names = (WdeFontNames *)ListElement( flist );
        for( nlist = font_names->family_list; nlist != NULL; nlist = ListNext( nlist ) ) {
            font_data = (WdeFontData *)ListElement( nlist );
            WRMemFree( font_data );
        }
        if( font_names->family_list != NULL ) {
            ListFree( font_names->family_list );
        }
        WRMemFree( font_names );
    }
    ListFree( WdeFontFamiliesList );
}

void WdeSetFontList( HWND main )
{
    FONTENUMPROC    enum_callback;
    LIST            *olist;
    WdeFontNames    *font_names;
    HDC             hDc;

    WdeFontFamiliesList = NULL;
    WdeFontList = NULL;

    hDc = GetDC( main );

    logpixelsy = (uint_32)GetDeviceCaps( hDc, LOGPIXELSY );

    enum_callback = (FONTENUMPROC)MakeProcInstance ( (FARPROC)WdeEnumFontsProc,
                                                     WdeGetAppInstance() );

    WdeFontList = NULL;
    WdeFontFamiliesList = NULL;

    EnumFontFamilies( hDc, NULL, enum_callback, (LPARAM)&WdeFontFamiliesList );

    for( olist = WdeFontFamiliesList; olist != NULL; olist = ListNext( olist ) ) {
        font_names = (WdeFontNames *)ListElement( olist );
        if( !EnumFontFamilies( hDc, font_names->name, enum_callback,
                               (LPARAM)&WdeFontList ) ) {
            WdeWriteTrail( "Getting font names: Enum Failed." );
        }
    }

    ReleaseDC( main, hDc );

    FreeProcInstance( (FARPROC)enum_callback );
}

BOOL WdeAddFontFamilyMember( WdeFontNames *font_element, ENUMLOGFONT *lpelf,
                             TEXTMETRIC *lpntm, int fonttype )
{
    uint_32     mod10;
    WdeFontData *font_data;
    WdeFontData *font_sibling;
    LIST        *olist;

    font_data = (WdeFontData *)WRMemAlloc( sizeof( WdeFontData ) );
    if( font_data == NULL ) {
        WdeWriteTrail( "Could not allocate font data" );
        return( FALSE );
    }

    memcpy( &font_data->elf, lpelf, sizeof( ENUMLOGFONT ) );
    memcpy( &font_data->ntm, lpntm, sizeof( NEWTEXTMETRIC ) );
    font_data->fonttype = fonttype;
    font_element->num_children++;

    /* get the point size (times 10 to check out how to round) */
    font_data->pointsize = ((uint_32)(lpntm->tmHeight - lpntm->tmInternalLeading) *
                            (uint_32)720) / logpixelsy;
    mod10 = font_data->pointsize % 10;
    font_data->pointsize /= 10;
    /* round the point size up if necessary */
    if( mod10 > 4 ) {
        font_data->pointsize++;
    }

    /* lets make sure the font is not already in the list */
    for( olist = font_element->family_list; olist != NULL; olist = ListNext( olist ) ) {
        font_sibling = ListElement( olist );
        if( font_sibling->pointsize == font_data->pointsize ) {
            WRMemFree( font_data );
            return( TRUE );
        }
    }

    WdeInsertObject( &font_element->family_list, (void *)font_data );

    return( TRUE );
}

WINEXPORT int CALLBACK WdeEnumFontsProc( ENUMLOGFONT *lpelf, TEXTMETRIC *lpntm, int fonttype, LPARAM lParam )
{
    LIST            *olist;
    LIST            **list;
    WdeFontNames    *font_names;
    WdeFontNames    *font_element;

    list = (LIST **)lParam;

    /* let's make sure the font is not already in the list */
    for( olist = *list; olist != NULL; olist = ListNext( olist ) ) {
        font_element = ListElement( olist );
        if( strcmp( font_element->name, lpelf->elfLogFont.lfFaceName ) == 0 ) {
            /* do not recursively add TRUE TYPE FONTS */
            if( !(fonttype & TRUETYPE_FONTTYPE) ) {
                WdeAddFontFamilyMember( font_element, lpelf, lpntm, fonttype );
            }
            return( TRUE );
        }
    }

    font_names = (WdeFontNames *)WRMemAlloc( sizeof( WdeFontNames ) );
    if( font_names == NULL ) {
        WdeWriteTrail( "Could not allocate font names structure." );
        return( FALSE );
    }

    if( fonttype & TRUETYPE_FONTTYPE ) {
        strncpy( font_names->name, (char *)lpelf->elfFullName, LF_FULLFACESIZE );
    } else {
        strncpy( font_names->name, lpelf->elfLogFont.lfFaceName, LF_FULLFACESIZE );
    }

    font_names->name[LF_FULLFACESIZE - 1] = '\0';

    font_names->fonttype = fonttype;
    font_names->family_list = NULL;
    font_names->num_children = 0;
    WdeAddFontFamilyMember( font_names, lpelf, lpntm, fonttype );

    WdeInsertObject( list, (void *)font_names );

    return( TRUE );
}

bool WdeDialogToScreen( void *obj, WdeResizeRatio *r, DialogSizeInfo *dsize, RECT *s )
{
    WdeResizeRatio  resizer;

    if( dsize == NULL || s == NULL ) {
        return( FALSE );
    }

    if( r == NULL && obj == NULL ) {
        return( FALSE );
    }

    if( r != NULL ) {
        resizer = *r;
    } else {
        if( !Forward( obj, GET_RESIZER, &resizer, NULL ) ) {
            WdeWriteTrail( "WdeDialogToScreen: GET_RESIZER failed!" );
            return( FALSE );
        }
    }

    s->left = MulDiv( (int_16)dsize->x, resizer.xmap, 4 );
    s->top = MulDiv( (int_16)dsize->y, resizer.ymap, 8 );
    s->right = MulDiv( (int_16)dsize->width, resizer.xmap, 4 );
    s->bottom = MulDiv( (int_16)dsize->height, resizer.ymap, 8 );
    s->right += s->left;
    s->bottom += s->top;

    return( TRUE );
}

bool WdeScreenToDialog( void *obj, WdeResizeRatio *r, RECT *s, DialogSizeInfo *dsize )
{
    WdeResizeRatio  resizer;
    RECT            screen;

    if( dsize == NULL ) {
        return( FALSE );
    }

    if( (r == NULL || s == NULL) && obj == NULL ) {
        return( FALSE );
    }

    if( r != NULL ) {
        resizer = *r;
    } else {
        if( !Forward( obj, GET_RESIZER, &resizer, NULL ) ) {
            WdeWriteTrail( "WdeScreenToDialog: GET_RESIZER failed!" );
            return( FALSE );
        }
    }

    if( s != NULL ) {
        screen = *s;
    } else {
        Location( obj, &screen );
    }

    dsize->x = (uint_16)MulDiv( screen.left, 4, resizer.xmap );
    dsize->y = (uint_16)MulDiv( screen.top, 8, resizer.ymap );
    dsize->width = (uint_16)MulDiv( screen.right - screen.left, 4, resizer.xmap );
    dsize->height = (uint_16)MulDiv( screen.bottom - screen.top, 8, resizer.ymap );

    return( TRUE );
}

HFONT WdeGetFont( char *face, int pointsize, int weight )
{
    LOGFONT   lf;
    HDC       dc;

    dc = GetDC( (HWND)NULL );
    if( dc != (HDC)NULL ) {
        memset( &lf, 0, sizeof( LOGFONT ) );
        strcpy( lf.lfFaceName, face );
        lf.lfWeight = weight;
        lf.lfHeight = -MulDiv( pointsize, GetDeviceCaps( dc, LOGPIXELSY ), 72);
        ReleaseDC( (HWND)NULL, dc );
        return( CreateFontIndirect( &lf ) );
    }

    return( (HFONT)NULL );
}

bool WdeGetResizerFromFont( WdeResizeRatio *r, char *face, int ptsz )
{
    GLOBALHANDLE    dialog_template;
    uint_8          *ldlg;
    HWND            hDlg;
    HINSTANCE       inst;
    DLGPROC         proc;
    RECT            rect;
    bool            ok;

    if( r == NULL ) {
        return( false );
    }

    inst = WdeGetAppInstance();
    dialog_template = DialogTemplate( WS_POPUP | DS_SETFONT, 4, 8, 4, 8,
                                      NULL, NULL, NULL, ptsz, face );
    DoneAddingControls( dialog_template );
    ldlg = (uint_8 *)GlobalLock ( dialog_template );
    proc = (DLGPROC)MakeProcInstance ( (FARPROC)WdeDummyProc, inst );
    hDlg = CreateDialogIndirect( inst, WDEDLGTEMPLATE ldlg, (HWND)NULL, proc );
    GlobalUnlock( dialog_template );
    GlobalFree( dialog_template );

    if( hDlg != (HWND)NULL ) {
        SetRect( &rect, 4, 8, 0, 0 );
        MapDialogRect( hDlg, &rect );
        r->xmap = rect.left;
        r->ymap = rect.top;
        DestroyWindow( hDlg );
        ok = true;
    } else {
        ok = false;
    }

    FreeProcInstance( (FARPROC)proc );

    return( ok );
}

WINEXPORT BOOL CALLBACK WdeDummyProc( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    /* touch unused vars to get rid of warning */
    _wde_touch( hDlg );
    _wde_touch( message );
    _wde_touch( wParam );
    _wde_touch( lParam );

    return( FALSE );
}
