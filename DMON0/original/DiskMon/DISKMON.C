/*****************************************************************************
*
*  DMon - Disk Monitor for Windows
*		
*  Copyright (c) 2009 diyhack 
*
*  See readme.txt for terms and conditions.
*
*  PROGRAM: DMon.c
*
*  PURPOSE: Communicates with the DMon driver to display 
*  Disk read/write information.
*
******************************************************************************/
#define _WIN32_WINNT 0x0501

#include <windows.h>   
#include <windowsx.h>
#include <tchar.h>
#include <commctrl.h>  
#include <stdio.h>
#include <string.h>
#include <winioctl.h>
#include <strsafe.h>
#include "resource.h"
#include "diskmon.h"
#include "driver.h"
#include "../DMon/ioctlcmd.h"

// Version information function
HRESULT (CALLBACK *pDllGetVersionProc)( PDLLVERSIONINFO_ pdvi );

// Driver handle
static HANDLE		SysHandle		= INVALID_HANDLE_VALUE;

// The variable that holds the position settings
POSITION_SETTINGS	PositionInfo;

// button definitions

// for installations that support flat style
TBBUTTON tbButtons[] = {
	{ 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, 0L, 0},
	{ 0, IDM_SAVE, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0},
	{ 8, 0, 0, TBSTYLE_BUTTON, 0L, 0},
    { 2, IDM_CAPTURE, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0},
	{ 4, IDM_AUTOSCROLL, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0},
	{ 6, IDM_CLEAR, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0},
	{ 10, IDM_TIME, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0},
	{ 8, 0, 0, TBSTYLE_BUTTON, 0L, 0},
	{ 12, IDM_HISTORY, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0},
	{ 8, 0, 0, TBSTYLE_BUTTON, 0L, 0},
	{ 7, IDM_FIND, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0},
	{ 8, 0, 0, TBSTYLE_BUTTON, 0L, 0},
};
#define NUMBUTTONS		12

// for older installations
TBBUTTON tbButtonsOld[] = {
	{ 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, 0L, 0},
	{ 0, IDM_SAVE, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0},
	{ 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, 0L, 0},
	{ 2, IDM_CAPTURE, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0},
	{ 4, IDM_AUTOSCROLL, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0},
	{ 6, IDM_CLEAR, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0},	
	{ 10, IDM_TIME, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0},
	{ 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, 0L, 0},
	{ 5, IDM_FILTER, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0},
	{ 12, IDM_HISTORY, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0},
	{ 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, 0L, 0},
	{ 7, IDM_FIND, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0},
};						 
#define NUMBUTTONSOLD	12

// Buffer into which driver can copy statistics
char				Stats[ MAX_STORE ];
// Current fraction of buffer filled
DWORD				StatsLen;

// Search string
CHAR				FindString[MAXITEMLENGTH];
FINDREPLACE			FindTextInfo;
DWORD				FindFlags = FR_DOWN;
BOOLEAN				PrevMatch;
CHAR				PrevMatchString[MAXITEMLENGTH];	

// Application instance handle
HINSTANCE			hInst;

// For info saving
CHAR				szFileName[MAX_PATH];
BOOLEAN				FileChosen = FALSE;

// Misc globals
HWND				hWndMain;
HWND				hWndFind = NULL;
UINT				findMessageID;
HWND				hWndList;
HWND				hWndToolbar;
WNDPROC 			ListViewWinMain;
HWND				hBalloon = NULL;
BOOLEAN				Capture = TRUE;
BOOLEAN				Autoscroll = TRUE;
BOOLEAN				BootLog = FALSE;
BOOLEAN				BootLogMenuUsed = FALSE;
BOOLEAN				Deleting = FALSE;
BOOLEAN				OnTop;
BOOLEAN				ClockTime;
BOOLEAN				ShowToolbar = TRUE;
DWORD				HighlightFg;
DWORD				HighlightBg;

// General buffer for storing temporary strings
static CHAR		msgbuf[ MAXITEMLENGTH ];

// Filter strings
CHAR				FilterString[MAXFILTERLEN];
CHAR				ExcludeString[MAXFILTERLEN];
CHAR				HighlightString[MAXFILTERLEN];

// Filter-related
FILTER				FilterDefinition;

// Recent filters
CHAR				RecentInFilters[NUMRECENTFILTERS][MAXFILTERLEN];
CHAR				RecentExFilters[NUMRECENTFILTERS][MAXFILTERLEN];
CHAR				RecentHiFilters[NUMRECENTFILTERS][MAXFILTERLEN];

// font
HFONT				hFont;
LOGFONT				LogFont;

// listview size limiting
DWORD				MaxLines = 0;
DWORD				LastRow = 0;

// General cursor manipulation
HCURSOR 			hSaveCursor;
HCURSOR 			hHourGlass;

// performance counter frequency
LARGE_INTEGER		PerfFrequency;

BOOL bWow64 = FALSE;
volatile LONG Sequence = 0;

//LARGE_INTEGER StartTime;


/******************************************************************************
*
*	FUNCTION:	Abort:
*
*	PURPOSE:	Handles emergency exit conditions.
*
*****************************************************************************/
LONG Abort( HWND hWnd, CHAR * Msg, DWORD Error )
{
	LPVOID	lpMsgBuf;
	CHAR	errmsg[MAX_PATH];
	DWORD	error = GetLastError();
 
	FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
					NULL, Error, 
					MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
					(LPTSTR) &lpMsgBuf, 0, NULL );
	UnloadDeviceDriver( SYS_NAME );
	StringCbPrintf(errmsg, MAX_PATH,_T("%s: %s"), Msg, lpMsgBuf );
	MessageBox( hWnd, errmsg, APPNAME, MB_OK|MB_ICONERROR );
	PostQuitMessage( 1 );
	LocalFree( lpMsgBuf );
	return -1;
}


/******************************************************************************
*
*	FUNCTION:	BalloonDialog
*
*	PURPOSE:	Dialog function for home-brewed balloon help.
*
******************************************************************************/
LRESULT APIENTRY BalloonDialog( HWND hDlg, UINT message, UINT wParam, LPARAM lParam )
{
	static ITEM_CLICK	ctx;
	static RECT			rect;
	static HFONT		hfont;
	LPCREATESTRUCT		lpcs;
	HDC					hdc;
	POINTS				pts;
	POINT				pt;
	DWORD				newclicktime;
	static POINT		lastclickpt = {0,0};
	static DWORD		lastclicktime = 0;	

	switch (message) {
		case WM_CREATE:

			lpcs = (void *)lParam;
			ctx = *(PITEM_CLICK) lpcs->lpCreateParams;
			hdc = GetDC( hDlg );

			// is the app the focus?
			if( !GetFocus()) return -1;

			// Compute size of required rectangle
			rect.left	= 0;
			rect.top	= 1;
			rect.right	= lpcs->cx;
			rect.bottom	= lpcs->cy;
			SelectObject( hdc, hFont );
			DrawText( hdc, ctx.itemText, -1, &rect, 
						DT_NOCLIP|DT_LEFT|DT_NOPREFIX|DT_CALCRECT );

			// if the bounding rectangle of the subitem is big enough to display
			// the text then don't pop the balloon
			if( ctx.itemPosition.right > rect.right + 3 ) {

				return -1;
			}

			// Move and resize window
			if( ctx.itemPosition.left - 5 + rect.right + 10 >
				 GetSystemMetrics(SM_CXFULLSCREEN) ) {

				 ctx.itemPosition.left = GetSystemMetrics(SM_CXFULLSCREEN) -
							(rect.right+10);
			}
			MoveWindow( hDlg, 
						ctx.itemPosition.left-1, ctx.itemPosition.top, 
						rect.right + 6, 
						rect.bottom + 1,
						TRUE );

			// Adjust rectangle so text is centered
			rect.left	+= 2;
			rect.right	+= 2;
			rect.top	-= 1; 
			rect.bottom	+= 0;

			// make it so this window doesn't get the focus
			ShowWindow( hDlg, SW_SHOWNOACTIVATE );
			break;

		case WM_LBUTTONDBLCLK:
		case WM_RBUTTONDBLCLK:
		case WM_MBUTTONDBLCLK:
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
		case WM_MBUTTONUP:

			pts = MAKEPOINTS( lParam );
			pt.x = (LONG) pts.x;
			pt.y = (LONG) pts.y;
			ClientToScreen( hDlg, &pt );

			// pass this through to the listview
			if( ScreenToClient( hWndList, &pt )) {

				if( message == WM_LBUTTONDOWN ) {

					// see if its a double click
					newclicktime = GetTickCount();
					if( pt.x == lastclickpt.x && pt.y == lastclickpt.y && 
						newclicktime - lastclicktime < 300 ) {

						message = WM_LBUTTONDBLCLK;
					}
					lastclicktime = newclicktime;
					lastclickpt = pt;
				}

				PostMessage( hWndList, message, wParam, (SHORT) pt.y<<16 | (SHORT) pt.x );
			}
			break;

		case WM_PAINT:
			hdc = GetDC( hDlg );

			// Set colors
			SetTextColor( hdc, 0x00000000 );
			SetBkMode( hdc, TRANSPARENT );
			SelectObject( hdc, hFont );
			DrawText( hdc, ctx.itemText, -1, &rect, 
						DT_NOCLIP|DT_LEFT|DT_NOPREFIX|DT_WORDBREAK );
			break;

		case WM_DESTROY:
			hBalloon = NULL;
			break;

		case WM_CLOSE:	
			DestroyWindow( hDlg );
			break;
	}

    return DefWindowProc( hDlg, message, wParam, lParam );
}


/******************************************************************************
*
*	FUNCTION:	CopySelection
*
*	PURPOSE:	Copies the currently selected line in the output to the clip
*				board.
*
*****************************************************************************/
void CopySelection( HWND hWnd )
{
	LPTSTR  lptstrCopy; 
	HGLOBAL hglbCopy; 
	size_t	size = 0, newSize;
	int		currentItem, iColumn;
	TCHAR	curText[MAXITEMLENGTH];
	TCHAR	selectedText[NUMCOLUMNS * MAXITEMLENGTH];	

	// Get the currently selected item and construct
	// the message to go to the clipboard
	currentItem = ListView_GetNextItem( hWndList, -1, LVNI_SELECTED );
	if( currentItem == -1 ) {

		return;
	}
	selectedText[0] = 0;
	for( iColumn = 1; iColumn < NUMCOLUMNS; iColumn++ ) {

		curText[0] = 0;
		ListView_GetItemText( hWndList, currentItem, iColumn, 
			curText, MAXITEMLENGTH );
		strcat( selectedText, curText );
		strcat( selectedText, "\t");
	}
	strcat( selectedText, "\r\n");

	// Empty the clipboard
	if (!OpenClipboard( hWnd )) return; 
	EmptyClipboard(); 

	size = strlen( selectedText )+1;
	hglbCopy = GlobalAlloc( GMEM_DDESHARE|GMEM_MOVEABLE, size ); 
	lptstrCopy = GlobalLock(hglbCopy); 
	StringCbCopy(lptstrCopy, size, selectedText );
	GlobalUnlock(hglbCopy); 

	while( (currentItem = ListView_GetNextItem( hWndList, currentItem, 
		LVNI_SELECTED )) != -1) {
			selectedText[0] = 0;
			for( iColumn = 1; iColumn < NUMCOLUMNS; iColumn++ ) {

				curText[0] = 0;
				ListView_GetItemText( hWndList, currentItem, iColumn, 
					curText, MAXITEMLENGTH );
				strcat( selectedText, curText );
				strcat( selectedText, "\t");
			}
			strcat( selectedText, "\r\n");

			newSize = size + strlen( selectedText );
			hglbCopy = GlobalReAlloc( hglbCopy, newSize, 0 );
			lptstrCopy = GlobalLock(hglbCopy); 
			StringCbCopy( &lptstrCopy[size-1], newSize + 1 - size, selectedText );
			GlobalUnlock(hglbCopy); 

			size = newSize;
	}

	// Place it in the clipboard
	SetClipboardData(CF_TEXT, hglbCopy); 
	CloseClipboard(); 	
}


/******************************************************************************
*
*	FUNCTION:	CCHookProc
*
*	PURPOSE:	We use a hook procedure to force the stupid color
*				selection dialog to do what we want, including preview
*				the highlight text.
*
*****************************************************************************/
UINT CALLBACK CCHookProc( HWND hDlg, 
					  UINT uiMsg, WPARAM wParam,  
					  LPARAM lParam )
{
	static HWND	 sample;
	static DWORD  newFg, newBg;
	static UINT colorOkString, setRgbString;

	switch( uiMsg ) {
	case WM_INITDIALOG:
		sample = GetDlgItem( hDlg, IDC_SAMPLE );
		newFg = HighlightFg;
		newBg = HighlightBg;
		colorOkString = RegisterWindowMessage( COLOROKSTRING );
		setRgbString  = RegisterWindowMessage( SETRGBSTRING ); 
		CheckRadioButton( hDlg, IDC_RADIOFG, IDC_RADIOBG, IDC_RADIOFG );
		SendMessage(hDlg, setRgbString, 0, newFg); 
		SetFocus( GetDlgItem( hDlg, IDC_DONE ));
		break;
	case WM_CTLCOLORSTATIC:
		if( (HWND) lParam == sample ) {
			SetBkColor(GET_WM_CTLCOLOR_HDC(wParam, lParam, msg), 
                    newBg); 
			SetTextColor(GET_WM_CTLCOLOR_HDC(wParam, lParam, msg), 
                    newFg); 
            return (BOOL)GetStockObject(WHITE_BRUSH); 		
		}
		break;
	case WM_COMMAND:
		if( wParam == IDC_DONE ) {
			HighlightFg = newFg;
			HighlightBg = newBg;
			PostMessage( hDlg, WM_COMMAND, IDABORT, 0 );
			return FALSE;
		}
		break;
	default:
		if( uiMsg == colorOkString ) {
			if( !IsDlgButtonChecked( hDlg, IDC_RADIOBG )) {
				newFg = ((LPCHOOSECOLOR) lParam)->rgbResult;
				InvalidateRect( sample, NULL, TRUE );
				SendMessage(hDlg, setRgbString, 0, newBg); 
				return TRUE;
			} else {
				newBg = ((LPCHOOSECOLOR) lParam)->rgbResult;
				InvalidateRect( sample, NULL, TRUE );
				SendMessage(hDlg, setRgbString, 0, newFg); 
				return TRUE;
			}
		}
		break;
	}
	return 0;
}


/******************************************************************************
*
*	FUNCTION:	SelectHighlightColors
*
*	PURPOSE:	Let's the user pick the highlight foreground and background
*				colors.
*
*****************************************************************************/
VOID SelectHighlightColors( HWND hWnd )
{
	DWORD			dwColor;
	DWORD			dwCustClrs [16];
	BOOL			fSetColor = FALSE;
	int				i;
	CHOOSECOLOR		chsclr;

	for (i = 0; i < 15; i++)
		dwCustClrs [i] = RGB (255, 255, 255);
	dwColor = RGB (0, 0, 0);
	chsclr.lStructSize = sizeof (CHOOSECOLOR);
	chsclr.hwndOwner = hWnd;
	chsclr.hInstance = (HANDLE) hInst;
	chsclr.rgbResult = dwColor;
	chsclr.lpCustColors = (LPDWORD)dwCustClrs;
	chsclr.lCustData = 0L;
	chsclr.rgbResult = HighlightFg;
	chsclr.lpTemplateName = "CHOOSECOLORFG";
	chsclr.lpfnHook = (LPCCHOOKPROC)(FARPROC)CCHookProc;
	chsclr.Flags = CC_RGBINIT|CC_PREVENTFULLOPEN|
					CC_ENABLEHOOK|CC_ENABLETEMPLATE;
	ChooseColor (&chsclr);
	// Redraw to apply
	InvalidateRect( hWndList, NULL, TRUE );
} 


/******************************************************************************
*
*	FUNCTION:	FindInListview:
*
*	PURPOSE:	Searches for a string in the listview. Note: its okay if
*				items are being added to the list view or the list view
*				is cleared while this search is in progress - the effect
*				is harmless.
*
*****************************************************************************/
BOOLEAN FindInListview(HWND hWnd, LPFINDREPLACE FindInfo )
{
	int		currentItem, clearItem;
	DWORD	i;
	int		subitem, numItems;
	CHAR	fieldtext[MAXITEMLENGTH];
	BOOLEAN match = FALSE;
	CHAR	errmsg[256];
	BOOLEAN	goUp;

	// get the search direction
	goUp = ((FindInfo->Flags & FR_DOWN) == FR_DOWN);

	// initialize stuff
	if( !(numItems = ListView_GetItemCount( hWndList ))) {

		MessageBox( hWnd, _T("No items to search"), APPNAME, 
			MB_OK|MB_ICONWARNING );
		if( hWndFind ) SetForegroundWindow( hWndFind );
		return FALSE;
	}

	// find the item with the focus
	currentItem = ListView_GetNextItem( hWndList, -1, LVNI_SELECTED );

	// if no current item, start at the top or the bottom
	if( currentItem == -1 ) {
		if( goUp )
			currentItem = 0;
		else {
			if( PrevMatch ) {
				StringCbPrintf(errmsg, 256, _T("Cannot find string \"%s\""), FindInfo->lpstrFindWhat );
				MessageBox( hWnd, errmsg, APPNAME, MB_OK|MB_ICONWARNING );
				if( hWndFind ) SetForegroundWindow( hWndFind );
				else SetFocus( hWndList );
				return FALSE;
			}
			currentItem = numItems;
		}
	}

	// if we're continuing a search, start with the next item
	if( PrevMatch && !strcmp( FindString, PrevMatchString ) ) {
		if( goUp ) currentItem++;
		else currentItem--;

		if( (!goUp && currentItem < 0) ||
			(goUp && currentItem >= numItems )) {

			StringCbPrintf(errmsg, 256, _T("Cannot find string \"%s\""), FindInfo->lpstrFindWhat );
			MessageBox( hWnd, errmsg, APPNAME, MB_OK|MB_ICONWARNING );
			if( hWndFind ) SetForegroundWindow( hWndFind );
			else SetFocus( hWndList );
			return FALSE;
		}
	}

	// loop through each item looking for the string
	while( 1 ) {

		// get the item text
		for( subitem = 0; subitem < NUMCOLUMNS; subitem++ ) {
			fieldtext[0] = 0;
			ListView_GetItemText( hWndList, currentItem, subitem, fieldtext, MAXITEMLENGTH );

			// make sure enought string for a match
			if( strlen( fieldtext ) < strlen( FindInfo->lpstrFindWhat ))
				continue;

			// do a scan all the way through for the substring
			if( FindInfo->Flags & FR_WHOLEWORD ) {

				i = 0;
				while( fieldtext[i] ) {
					while( fieldtext[i] && fieldtext[i] != ' ' ) i++;
					if( FindInfo->Flags & FR_MATCHCASE ) 
						match = !strcmp( fieldtext, FindInfo->lpstrFindWhat );
					else
						match = !_stricmp( fieldtext, FindInfo->lpstrFindWhat );
					if( match) break;
					i++;
				}	
			} else {
				for( i = 0; i < strlen( fieldtext ) - strlen(FindInfo->lpstrFindWhat)+1; i++ ) {
					if( FindInfo->Flags & FR_MATCHCASE ) 
						match = !strncmp( &fieldtext[i], FindInfo->lpstrFindWhat, 
											strlen(FindInfo->lpstrFindWhat) );
					else
						match = !_strnicmp( &fieldtext[i], FindInfo->lpstrFindWhat,
											strlen(FindInfo->lpstrFindWhat) );
					if( match ) break;
				}		
			}

			if( match ) {

				StringCbCopy( PrevMatchString, MAXITEMLENGTH,FindInfo->lpstrFindWhat );
				PrevMatch = TRUE;
				// Clear all previously-selected items
				while( (clearItem = ListView_GetNextItem( hWndList, -1, LVNI_SELECTED )) != -1 ) {
					ListView_SetItemState( hWndList, clearItem, 0, LVIS_SELECTED|LVIS_FOCUSED );
				}
				ListView_SetItemState( hWndList, currentItem, 
							LVIS_SELECTED|LVIS_FOCUSED,
							LVIS_SELECTED|LVIS_FOCUSED );
				ListView_EnsureVisible( hWndList, currentItem, FALSE ); 
				SetFocus( hWndList );
				return TRUE;
			}
		}
		currentItem = currentItem + (goUp ? 1:-1);
		if( !currentItem || currentItem == numItems+1 ) {
			// end of the road
			break;
		}
	}
	StringCbPrintf(errmsg, 256, _T("Cannot find string \"%s\""), FindInfo->lpstrFindWhat );
	MessageBox( hWnd, errmsg, APPNAME, MB_OK|MB_ICONWARNING );
	if( hWndFind ) SetForegroundWindow( hWndFind );
	else SetFocus( hWndList );
	return FALSE;
}


/******************************************************************************
*
*	FUNCTION:	PopFindDialog:
*
*	PURPOSE:	Calls the find message dialog box.
*
*****************************************************************************/
void PopFindDialog(HWND hWnd)
{
	StringCbCopy( FindString, MAXITEMLENGTH,PrevMatchString );
    FindTextInfo.lStructSize = sizeof( FindTextInfo );
    FindTextInfo.hwndOwner = hWnd;
    FindTextInfo.hInstance = hInst;
    FindTextInfo.lpstrFindWhat = FindString;
    FindTextInfo.lpstrReplaceWith = NULL;
    FindTextInfo.wFindWhatLen = sizeof(FindString);
    FindTextInfo.wReplaceWithLen = 0;
    FindTextInfo.lCustData = 0;
    FindTextInfo.Flags =  FindFlags;
    FindTextInfo.lpfnHook = (LPFRHOOKPROC)(FARPROC)NULL;
    FindTextInfo.lpTemplateName = NULL;

    if ((hWndFind = FindText(&FindTextInfo)) == NULL)
		MessageBox( hWnd, _T("Unable to create Find dialog"), APPNAME, MB_OK|MB_ICONERROR );      
}


/****************************************************************************
*
*	FUNCTION: MatchOkay
*
*	PURPOSE: Only thing left after compare is more mask. This routine makes
*	sure that its a valid wild card ending so that its really a match.
*
****************************************************************************/
BOOLEAN MatchOkay( PCHAR Pattern )
{
    // If pattern isn't empty, it must be a wildcard
    if( *Pattern && *Pattern != '*' ) {
 
       return FALSE;
    }

    // Matched
    return TRUE;
}


/****************************************************************************
*
*	FUNCTION: MatchWithPatternCore
*
*	PURPOSE: Performs nifty wildcard comparison.
*
****************************************************************************/
BOOLEAN MatchWithPattern( PCHAR Pattern, PCHAR Name )
{
	char matchchar;

	// End of pattern?
	if( !*Pattern ) {
		return FALSE;
	}

	// If we hit a wild card, do recursion
	if( *Pattern == '*' ) {

		Pattern++;
		while( *Name && *Pattern ) {

			matchchar = *Name;
			if( matchchar >= 'a' && 
				matchchar <= 'z' ) {

					matchchar -= 'a' - 'A';
			}

			// See if this substring matches
			if( *Pattern == matchchar ) {

				if( MatchWithPattern( Pattern+1, Name+1 )) {

					return TRUE;
				}
			}

			// Try the next substring
			Name++;
		}

		// See if match condition was met
		return MatchOkay( Pattern );
	} 

	// Do straight compare until we hit a wild card
	while( *Name && *Pattern != '*' ) {

		matchchar = *Name;
		if( matchchar >= 'a' && 
			matchchar <= 'z' ) {

				matchchar -= 'a' - 'A';
		}

		if( *Pattern == matchchar ) {
			Pattern++;
			Name++;

		} else {

			return FALSE;
		}
	}

	// If not done, recurse
	if( *Name ) {

		return MatchWithPattern( Pattern, Name );
	}

	// Make sure its a match
	return MatchOkay( Pattern );
}


/****************************************************************************
*
*	FUNCTION: MatchWithHighlightPattern
*
*	PURPOSE: Converts strings to upper-case before calling core 
*	comparison routine.
*
****************************************************************************/
BOOLEAN MatchWithHighlightPattern( PCHAR String )
{
	char		*filterPtr;
	char		curFilterBuf[MAXFILTERLEN];
	char		curMatchTest[MAXFILTERLEN];
	char		*curFilter, *endFilter;

	// Is there a highlight filter?
	if( !HighlightString[0] ||
		(HighlightString[0] == ' ' && !HighlightString[1] )) return FALSE;

	// see if its in an highlight
	filterPtr = HighlightString;
	curFilter = curFilterBuf;
	while( 1 ) {

		endFilter = strchr( filterPtr, ';' );
		if( !endFilter )
			curFilter = filterPtr;
		else {
			strncpy( curFilter, filterPtr, (int) (endFilter - filterPtr ) );
			curFilter[ (int) (endFilter - filterPtr ) ] = 0;
		}

		// Now do the comparison
		StringCbPrintf( curMatchTest, MAXFILTERLEN, "%s%s%s",
			*curFilter == '*' ? "" : "*",
			curFilter,
			curFilter[ strlen(curFilter)-1] == '*' ? "" : "*" );
		if( MatchWithPattern( curMatchTest, String ) ) {

			return TRUE;
		}

		if( endFilter ) filterPtr = endFilter+1;
		else break;
	}
	return FALSE;	
}


/****************************************************************************
*
*	FUNCTION:	FilterProc
*
*	PURPOSE:	Processes messages for "Filter" dialog box
*
****************************************************************************/
BOOL APIENTRY FilterProc( HWND hDlg, UINT message, UINT wParam, LONG lParam )
{
	CHAR			newFilter[MAXFILTERLEN];
	CHAR			newExFilter[MAXFILTERLEN];
	CHAR			newHiFilter[MAXFILTERLEN], oldHighlight[MAXFILTERLEN];
	int				i, j, nb;
	static HWND		hInFilter;
	static HWND		hExFilter;
	static HWND		hHiFilter;

	switch ( message )  {
	case WM_INITDIALOG:

		// initialize the controls to reflect the current filter
		// We use a ' ' as a placeholder in the filter strings to represent no filter ("")
		hInFilter = GetDlgItem( hDlg, IDC_FILTERSTRING );
		for( i = 0; i < NUMRECENTFILTERS; i++ ) {
			if( RecentInFilters[i][0] ) {
				SendMessage( hInFilter,	CB_ADDSTRING, 0, 
					(LPARAM ) (strcmp( RecentInFilters[i], " ") ? 
							RecentInFilters[i] : _T("")));
			}
		}
		hExFilter = GetDlgItem( hDlg, IDC_EXFILTERSTRING );
		for( i = 0; i < NUMRECENTFILTERS; i++ ) {
			if( RecentExFilters[i][0] ) {
				SendMessage( hExFilter, CB_ADDSTRING, 0, 
					(LPARAM ) (_tcscmp( RecentExFilters[i], _T(" ")) ? 
							RecentExFilters[i] : _T("")));
			}
		}
		hHiFilter = GetDlgItem( hDlg, IDC_HIFILTERSTRING );
		for( i = 0; i < NUMRECENTFILTERS; i++ ) {
			if( RecentHiFilters[i][0] ) {
				SendMessage( hHiFilter, CB_ADDSTRING, 0, 
					(LPARAM ) (_tcscmp( RecentHiFilters[i], _T(" ")) ? 
							RecentHiFilters[i] : _T("")));			
			}
		}
		SendMessage( hInFilter, CB_SETCURSEL, 0, 0);
		SendMessage( hExFilter, CB_SETCURSEL, 0, 0);
		SendMessage( hHiFilter, CB_SETCURSEL, 0, 0);

		return TRUE;

	case WM_COMMAND:              
		StringCbCopy( oldHighlight, MAXFILTERLEN, HighlightString );
		if ( LOWORD( wParam ) == IDOK )	 {

			// make sure that max lines is legal
			GetDlgItemText( hDlg, IDC_FILTERSTRING, newFilter, MAXFILTERLEN );
			GetDlgItemText( hDlg, IDC_EXFILTERSTRING, newExFilter, MAXFILTERLEN );
			GetDlgItemText( hDlg, IDC_HIFILTERSTRING, newHiFilter, MAXFILTERLEN );
			if( !newFilter[0] ) StringCbCopy( newFilter, MAXFILTERLEN, _T(" ") );
			if( !newExFilter[0] ) StringCbCopy( newExFilter, MAXFILTERLEN, _T(" ") );
			if( !newHiFilter[0] ) StringCbCopy( newHiFilter, MAXFILTERLEN, _T(" ") );

			StringCbCopy( FilterString, MAXFILTERLEN, newFilter );
			_tcsupr_s( FilterString, MAXFILTERLEN );
			for( i = 0; i < NUMRECENTFILTERS; i++ ) {
				if( !_tcsicmp( RecentInFilters[i], newFilter )) {	
					i++;
					break;
				}
			}
			for( j = i-2; j != (DWORD) -1; j-- ) {
				StringCbCopy( RecentInFilters[j+1], MAXFILTERLEN, RecentInFilters[j] );
			}
			StringCbCopy( RecentInFilters[0], MAXFILTERLEN, newFilter );

			StringCbCopy( ExcludeString, MAXFILTERLEN, newExFilter );
			_tcsupr_s( ExcludeString, MAXFILTERLEN );
			for( i = 0; i < NUMRECENTFILTERS; i++ ) {
				if( !_tcsicmp( RecentExFilters[i], newExFilter )) {	
					i++;
					break;
				}
			}
			for( j = i-2; j != (DWORD) -1; j-- ) {
				StringCbCopy( RecentExFilters[j+1], MAXFILTERLEN, RecentExFilters[j] );
			}
			StringCbCopy( RecentExFilters[0], MAXFILTERLEN, newExFilter );

			StringCbCopy( HighlightString, MAXFILTERLEN, newHiFilter );
			_strupr_s( HighlightString, MAXFILTERLEN );
			for( i = 0; i < NUMRECENTFILTERS; i++ ) {
				if( !_tcsicmp( RecentHiFilters[i], newHiFilter )) {	
					i++;
					break;
				}
			}
			for( j = i-2; j != (DWORD) -1; j-- ) {
				StringCbCopy( RecentHiFilters[j+1], MAXFILTERLEN, RecentHiFilters[j] );
			}
			StringCbCopy( RecentHiFilters[0], MAXFILTERLEN, newHiFilter );

			if( _tcsicmp( oldHighlight, HighlightString )) {
				InvalidateRgn( hWndList, NULL, TRUE );
			}

			EndDialog( hDlg, TRUE );

			return TRUE;

		} else if( LOWORD( wParam ) == IDCANCEL ) {

			EndDialog( hDlg, TRUE );

		} else if( LOWORD( wParam ) == IDRESET ) {

			// initialize the controls to reflect the current filter
			SetDlgItemText( hDlg, IDC_FILTERSTRING, _T("*") );
			SetDlgItemText( hDlg, IDC_EXFILTERSTRING, _T("") );
			SetDlgItemText( hDlg, IDC_HIFILTERSTRING, _T("") );
			if( _tcsicmp( oldHighlight, HighlightString )) {
				InvalidateRgn( hWndList, NULL, TRUE );
			}
		}
		break;

	case WM_CLOSE:
		EndDialog( hDlg, TRUE );
		return TRUE;
	}
	return FALSE;   
}

/****************************************************************************
*
*	FUNCTION:	HistoryProc
*
*	PURPOSE:	Processes messages for "Filter" dialog box
*
****************************************************************************/
BOOL APIENTRY HistoryProc( HWND hDlg, UINT message, UINT wParam, LONG lParam )
{
	DWORD			newMaxLines, numRows;
	CHAR			history[64];

	switch ( message )  {
	case WM_INITDIALOG:

		// initialize the controls to reflect the current filter
		StringCbPrintf( history, 64, _T("%d"), MaxLines );
		SetDlgItemText( hDlg, IDC_HISTORY, history );
		SendMessage (GetDlgItem( hDlg, IDC_SPIN), UDM_SETRANGE, 0L, 
							MAKELONG (9999, 0));
		return TRUE;

	case WM_COMMAND:              
		if ( LOWORD( wParam ) == IDOK )	 {

			// make sure that max lines is legal
			GetDlgItemText( hDlg, IDC_HISTORY, history, 64 );
			if( !sscanf_s( history, _T("%d"), &newMaxLines )) {

				MessageBox(	NULL, _T("Invalid History Depth."),
						_T("Filter Error"), MB_OK|MB_ICONWARNING );
				return TRUE;
			} 
			MaxLines = newMaxLines;

			EndDialog( hDlg, TRUE );
			if (MaxLines ) {
				numRows = ListView_GetItemCount( hWndList );
				SendMessage(hWndList, WM_SETREDRAW, FALSE, 0);
				while ( numRows >= MaxLines ) {
					ListView_DeleteItem ( hWndList, 0 );
					numRows--;
				}
				SendMessage(hWndList, WM_SETREDRAW, TRUE, 0);
			}
			return TRUE;

		} else if( LOWORD( wParam ) == IDCANCEL ) {

			EndDialog( hDlg, TRUE );
		} else if( LOWORD( wParam ) == IDRESET ) {

			// reset filter to default of none
			SetDlgItemTextA( hDlg, IDC_HISTORY, "0" );
		}
		break;
	case WM_CLOSE:
		EndDialog( hDlg, TRUE );
		return TRUE;
	}
	return FALSE;   
}


/******************************************************************************
*
*	FUNCTION:	GetPositionSettings
*
*	PURPOSE:	Reads the Registry to get the last-set window position.
*
******************************************************************************/
VOID GetPositionSettings()
{
	HKEY		hKey;
	DWORD		ParamSize, newPosSize;
	LOGFONT		lf;
	int			i;
	POSITION_SETTINGS newPositionInfo;

	CHAR		*nextString;
	CHAR		recentExList[(MAXFILTERLEN+1) * NUMRECENTFILTERS + 1];
	CHAR		recentInList[(MAXFILTERLEN+1) * NUMRECENTFILTERS + 1];
	CHAR		recentHiList[(MAXFILTERLEN+1) * NUMRECENTFILTERS + 1];

	memset( &newPositionInfo ,   0, sizeof( newPositionInfo ));

	// Default font
	//GetObject( GetStockObject(SYSTEM_FONT), sizeof lf, &lf ); 
	//lf.lfWeight = FW_NORMAL;
	//lf.lfHeight = -12;//8;
	//lf.lfWidth  = 0;
    ZeroMemory( &lf, sizeof( lf ) );  // fixed for uninitialized memory, majun 7.26    

    lf.lfHeight=-14;   
    lf.lfWidth=0;   
    lf.lfEscapement=0;   
    lf.lfOrientation=0;   
    lf.lfWeight=FW_NORMAL;   
    lf.lfItalic=0;   
    lf.lfUnderline=0;   
    lf.lfStrikeOut=0;   
    lf.lfCharSet=DEFAULT_CHARSET; //ANSI_CHARSET;    
    lf.lfOutPrecision=OUT_DEFAULT_PRECIS;   
    lf.lfClipPrecision=CLIP_DEFAULT_PRECIS;   
    lf.lfQuality=PROOF_QUALITY;   
    lf.lfPitchAndFamily=VARIABLE_PITCH | FF_ROMAN; 
	StringCbCopy( lf.lfFaceName, LF_FACESIZE, "Courier New" );
	PositionInfo.font = lf;

	// Fist, set the default settings
	PositionInfo.top		= 100;
	PositionInfo.left		= 100;
	PositionInfo.width		= 680;
	PositionInfo.height		= 460;
	PositionInfo.maximized	= FALSE;
	PositionInfo.highlightfg = 0x00FFFFFF;
	PositionInfo.highlightbg = 0x000000FF;
	PositionInfo.ontop		= FALSE;

	// set the default listview widths
	PositionInfo.column[0] = 40;
	PositionInfo.column[1] = 120;
	PositionInfo.column[2] = 60;
	PositionInfo.column[3] = 200;
	PositionInfo.column[4] = 120;
	PositionInfo.column[5] = 120;

	// Clear the recent arrays
	recentInList[0] = '*';
	recentInList[1] = 0;
	recentInList[2] = 0;
	recentExList[0] = 0;
	recentHiList[0] = 0;
	memset( RecentExFilters,   0, sizeof( RecentExFilters ));
	memset( RecentInFilters,   0, sizeof( RecentInFilters ));
	memset( RecentHiFilters,   0, sizeof( RecentHiFilters ));

	// set the default history depth (infinite)
	PositionInfo.historydepth = 0;

	// get the params and ignore errors
	RegCreateKey(HKEY_CURRENT_USER, TOKENMON_SETTINGS_KEY, &hKey );
	newPosSize = sizeof( PositionInfo );
	newPositionInfo.posversion = 0;
	RegQueryValueEx( hKey,TOKENMON_SETTINGS_VALUE, NULL, NULL, (LPBYTE) &newPositionInfo,
				&newPosSize );
	ParamSize = sizeof( recentInList );
	RegQueryValueEx( hKey,TOKENMON_RECENT_INFILTER_VALUE, NULL, NULL, (LPBYTE) &recentInList,
				&ParamSize );
	ParamSize = sizeof( recentExList );
	RegQueryValueEx( hKey,TOKENMON_RECENT_EXFILTER_VALUE, NULL, NULL, (LPBYTE) &recentExList,
				&ParamSize );
	ParamSize = sizeof( recentHiList );
	RegQueryValueEx( hKey,TOKENMON_RECENT_HIFILTER_VALUE, NULL, NULL, (LPBYTE) &recentHiList,
				&ParamSize );
	CloseHandle( hKey );

	// See if we have new position info
	if( newPositionInfo.posversion == POSITION_VERSION &&
		newPosSize == sizeof(PositionInfo)) {

		PositionInfo = newPositionInfo;
	}

	// extract the history depth
	MaxLines	= PositionInfo.historydepth;
	OnTop		= PositionInfo.ontop;
	ClockTime	= PositionInfo.clocktime;

	// get font
	LogFont = PositionInfo.font;
 	hFont = CreateFontIndirect( &LogFont ); 

	// set highlight colors
	HighlightFg = PositionInfo.highlightfg;
	HighlightBg = PositionInfo.highlightbg;

	// Get the recent filter lists
	nextString = recentInList;
	i = 0;
	while( *nextString ) {
		StringCbCopy( RecentInFilters[i++], MAXFILTERLEN, nextString );
		nextString = &nextString[_tcslen(nextString)+1];
	}
	nextString = recentExList;
	i = 0;
	while( *nextString ) {
		StringCbCopy( RecentExFilters[i++], MAXFILTERLEN, nextString );
		nextString = &nextString[_tcslen(nextString)+1];
	}
	nextString = recentHiList;
	i = 0;
	while( *nextString ) {
		StringCbCopy( RecentHiFilters[i++], MAXFILTERLEN, nextString );
		nextString = &nextString[_tcslen(nextString)+1];
	}

	StringCbCopy( FilterString, MAXFILTERLEN, RecentInFilters[0] );
	_strupr_s( FilterString, MAXFILTERLEN );
	StringCbCopy( ExcludeString, MAXFILTERLEN, RecentExFilters[0] );
	_strupr_s( ExcludeString, MAXFILTERLEN );	
	StringCbCopy( HighlightString, MAXFILTERLEN, RecentHiFilters[0] );
	_strupr_s( HighlightString, MAXFILTERLEN );
}


/******************************************************************************
*
*	FUNCTION:	SavePositionSettings
*
*	PURPOSE:	Saves the current window settings to the Registry.
*
******************************************************************************/
VOID SavePositionSettings( HWND hWnd )
{
	RECT		rc;
	int			i;
	CHAR		*nextInString, *nextExString, *nextHiString;
	CHAR		recentExList[(MAXFILTERLEN+1) * NUMRECENTFILTERS + 1];
	CHAR		recentInList[(MAXFILTERLEN+1) * NUMRECENTFILTERS + 1];
	CHAR		recentHiList[(MAXFILTERLEN+1) * NUMRECENTFILTERS + 1];
	HKEY		hKey;

	// get the position of the main window
	PositionInfo.posversion = POSITION_VERSION;
	GetWindowRect( hWnd, &rc );
	if( !IsIconic( hWnd ) && !IsZoomed( hWnd )) {

		PositionInfo.left = rc.left;
		PositionInfo.top = rc.top;
		PositionInfo.width = rc.right - rc.left;
		PositionInfo.height = rc.bottom - rc.top;
	} 
	PositionInfo.maximized = IsZoomed( hWnd );
	PositionInfo.ontop = OnTop;

	// get the widths of the listview columns
	for( i = 0; i < NUMCOLUMNS; i++ ) {
		PositionInfo.column[i] = ListView_GetColumnWidth( hWndList, i );
	}

	// get the history depth
	PositionInfo.historydepth = MaxLines;

	// save time format
	PositionInfo.clocktime = ClockTime;
	

	// save font
	PositionInfo.font = LogFont;

	// save highlight colors
	PositionInfo.highlightfg = HighlightFg;
	PositionInfo.highlightbg = HighlightBg;

	// Save recent filters
	recentInList[0] = 0;
	nextInString = recentInList;
	for( i = 0; i < NUMRECENTFILTERS; i++ ) {
		if( !RecentInFilters[i][0] ) {
			break;
		}
		StringCbCopy( nextInString, MAXFILTERLEN, RecentInFilters[i] );
		nextInString = &nextInString[ _tcslen( nextInString ) + 1];
	}
	*nextInString = 0;

	recentExList[0] = 0;
	nextExString = recentExList;
	for( i = 0; i < NUMRECENTFILTERS; i++ ) {
		if( !RecentExFilters[i][0] ) {
			break;
		}
		StringCbCopy( nextExString, MAXFILTERLEN, RecentExFilters[i] );
		nextExString = &nextExString[ _tcslen( nextExString ) + 1];
	}	
	*nextExString = 0;

	recentHiList[0] = 0;
	nextHiString = recentHiList;
	for( i = 0; i < NUMRECENTFILTERS; i++ ) {
		if( !RecentHiFilters[i][0] ) {
			break;
		}
		StringCbCopy( nextHiString, MAXFILTERLEN, RecentHiFilters[i] );
		nextHiString = &nextHiString[ _tcslen( nextHiString ) + 1];
	}
	*nextHiString = 0;

	// save connection info to registry
	RegOpenKey(HKEY_CURRENT_USER, TOKENMON_SETTINGS_KEY,	&hKey );
	RegSetValueEx( hKey, TOKENMON_SETTINGS_VALUE, 0, REG_BINARY, (LPBYTE) &PositionInfo,
			sizeof( PositionInfo ) );
	RegSetValueEx( hKey, TOKENMON_RECENT_INFILTER_VALUE, 0, REG_BINARY, (LPBYTE) &recentInList,
			(DWORD) (nextInString - recentInList) + 1 );
	RegSetValueEx( hKey, TOKENMON_RECENT_EXFILTER_VALUE, 0, REG_BINARY, (LPBYTE) &recentExList,
			(DWORD) (nextExString - recentExList) + 1 );
	RegSetValueEx( hKey, TOKENMON_RECENT_HIFILTER_VALUE, 0, REG_BINARY, (LPBYTE) &recentHiList,
			(DWORD) (nextHiString - recentHiList) + 1 );
	CloseHandle( hKey );
}	


/******************************************************************************
*
*	FUNCTION:	Split
*
*	PURPOSE:	Split a delimited line into components
*
******************************************************************************/
int Split( CHAR * line, CHAR delimiter, CHAR * items[] )
{
	int		cnt = 0;

	for (;;)  {
		// Add prefix to list of components		
		items[cnt++] = line;

		// Check for more components
		line = _tcschr( line, delimiter );
		if ( line == (PCHAR) NULL )
			return cnt;

		// Terminate previous component and move to next
		*line++ = _T('\0');
	}		
}

/******************************************************************************
*
*	FUNCTION:	ListAppend
*
*	PURPOSE:	Add a new line to List window
*
******************************************************************************/
BOOL List_Append( HWND hWndList, DWORD seq, 
				 LONGLONG time, LONG Disk,
				 CHAR *line )
{
	LV_ITEM		lvI;	// list view item structure
	int			row;
	CHAR	*	items[NUMCOLUMNS];
	int			itemcnt = 0;
//	float		elapsed;
	FILETIME	localTime;
	SYSTEMTIME	systemTime;

	// Split line into columns, chopping off win32 pids if necessary
	itemcnt = Split( line, _T('\t'), items );
	if ( itemcnt == 0 )
		return FALSE;

	// Determine row number for request
	if ( *items[0] )  {
		// Its a new request.  Put at end.
		row = 0x7FFFFFFF;
	} else {
		// Its a status.  Locate its associated request.
		lvI.mask = LVIF_PARAM;
		lvI.iSubItem = 0;
		for ( row = ListView_GetItemCount(hWndList) - 1; row >= 0; --row )  {
			lvI.iItem = row;
			if ( ListView_GetItem( hWndList, &lvI )  &&  (DWORD)lvI.lParam == seq )
				break;
		}
		if ( row == -1 ) {
			// No request associated with status.
			return FALSE;
		}
	}

	// Sequence number if a new item
	if ( *items[0] )  {
		StringCbPrintf( msgbuf, MAXITEMLENGTH, _T("%d"), Sequence/*seq*/ );
		lvI.mask		= LVIF_TEXT | LVIF_PARAM;
		lvI.iItem		= row;
		lvI.iSubItem	= 0;
		lvI.pszText		= msgbuf;
		lvI.cchTextMax	= lstrlen( lvI.pszText ) + 1;
		lvI.lParam		= Sequence;//seq;
		row = ListView_InsertItem( hWndList, &lvI );
		if ( row == -1 )  {
			StringCbPrintf( msgbuf, MAXITEMLENGTH, _T("Error adding item %d to list view"), seq );
			MessageBox( hWndList, msgbuf, APPNAME _T(" Error"), MB_OK );
			return FALSE;
		}

		InterlockedIncrement( (volatile LONG*)&Sequence );

        LastRow = row;
	}

	// format timestamp according to user preference
	if( 1/*ClockTime*/ ) {

	    FileTimeToLocalFileTime( (PFILETIME) &time, &localTime );
    	FileTimeToSystemTime( &localTime, &systemTime );

        StringCbPrintf(msgbuf, MAXITEMLENGTH, "%02d:%02d:%02d.%03d", systemTime.wHour,
            systemTime.wMinute, systemTime.wSecond, systemTime.wMilliseconds);

	} else {

	}
	ListView_SetItemText( hWndList, row, 1, msgbuf );

	StringCbPrintf(msgbuf, MAXITEMLENGTH, "%d", Disk);
	// Disk
	//if ( itemcnt>0 && *items[0] ) {
		ListView_SetItemText( hWndList, row, 2, msgbuf/*items[0]*/ );
	//}

	// Request
	if ( itemcnt>0 && *items[0] )  {
		ListView_SetItemText( hWndList, row, 3, items[0] );
	}

	// Sector
	if ( itemcnt>1 && *items[1] )  {
		ListView_SetItemText( hWndList, row, 4, items[1] );
	}

	// Length
	if ( itemcnt>2 && *items[2] )  {
		ListView_SetItemText( hWndList, row, 5, items[2] );
	}

	//// Additional
	//if ( itemcnt>3 && *items[3] )  {
	//	ListView_SetItemText( hWndList, row, 6, items[3] );
	//}
	return TRUE;
}

#define ROUND_TO_SIZE(Length) ((((ULONG)(Length)) + ((4) - 1)) & ~(ULONG) ((4) - 1))
/******************************************************************************
*
*	FUNCTION:	UpdateStatistics
*
*	PURPOSE:	Clear the statistics window and refill it with the current 
*				contents of the statistics buffer.  
*
******************************************************************************/
void UpdateStatistics( HWND hWnd, HWND hWndList, PCHAR Buffer, DWORD BufLen, BOOL Clear )
{
	PENTRY	ptr;
	BOOL	itemsAdded = FALSE;
    size_t   len;

	BOOLEAN isfirst = TRUE;
	BOOLEAN Frag = FALSE;
	static CHAR lasttext[4096];

	// Just return if nothing to do
	if ( !Clear  &&  BufLen < sizeof(int)+2 )
		return;

	// Do different stuff on NT than Win9x
    for ( ptr = (void *)Buffer; (char *)ptr < min(Buffer+BufLen,Buffer + MAX_STORE); )  {

        len = strlen(ptr->text) + 1;
		len = ROUND_TO_SIZE(len);

        // truncate if necessary
        if( len > MAXITEMLENGTH - 1 ) ptr->text[MAXITEMLENGTH-1] = 0;
        isfirst = FALSE;

        itemsAdded |= List_Append( hWndList, Sequence, 
                                   ptr->time.QuadPart, 
                                   ptr->DiskNum,
                                   ptr->text );
        ptr = (void *)(ptr->text + len);
    }

	// Limit number of lines saved
	if (MaxLines && itemsAdded ) {
		SendMessage(hWndList, WM_SETREDRAW, FALSE, 0);
		while ( LastRow >= MaxLines ) {
			ListView_DeleteItem ( hWndList, 0 );
		    LastRow--;
		}
		SendMessage(hWndList, WM_SETREDRAW, TRUE, 0);
    }

	// Scroll so newly added items are visible
	if ( Autoscroll && itemsAdded ) {
		if( hBalloon ) DestroyWindow( hBalloon );
		ListView_EnsureVisible( hWndList, ListView_GetItemCount(hWndList)-1, FALSE ); 
	}

	// Start with empty list
	if ( Clear ) {

		ListView_DeleteAllItems( hWndList );
		LastRow = 0;
	}
}



/****************************************************************************
*
*    FUNCTION: CalcStringEllipsis
*
*    PURPOSE:  Determines if an item will fit in a listview row, and if
*			   not, attaches the appropriate number of '.' to a truncated 
*			   version.
*
****************************************************************************/
BOOL WINAPI CalcStringEllipsis (HDC     hdc, 
                                LPTSTR  szString, 
                                int     cchMax, 
                                UINT    uColWidth) 
{ 
    static CHAR szEllipsis3[] = _T("..."); 
    static CHAR szEllipsis2[] = _T(".."); 
    static CHAR szEllipsis1[] = _T("."); 
    SIZE		sizeString; 
    SIZE		sizeEllipsis3, sizeEllipsis2, sizeEllipsis1; 
    int			cbString; 
    LPTSTR		lpszTemp; 
     
    // Adjust the column width to take into account the edges 
    uColWidth -= 4; 

	// Allocate a string for us to work with.  This way we can mangle the 
    // string and still preserve the return value 
    lpszTemp = (LPTSTR)HeapAlloc (GetProcessHeap(), HEAP_ZERO_MEMORY, cchMax); 
    if (!lpszTemp) return FALSE;
    lstrcpy (lpszTemp, szString); 
 
    // Get the width of the string in pixels 
    cbString = lstrlen(lpszTemp); 
    if (!GetTextExtentPoint32 (hdc, lpszTemp, cbString, &sizeString)) {
        HeapFree (GetProcessHeap(), 0, (LPVOID)lpszTemp); 
		return FALSE;
    } 
 
    // If the width of the string is greater than the column width shave 
    // the string and add the ellipsis 
    if ((ULONG)sizeString.cx > uColWidth) {
		
        if (!GetTextExtentPoint32 (hdc, 
                                   szEllipsis3, 
                                   lstrlen(szEllipsis3), 
                                   &sizeEllipsis3)) {
			HeapFree (GetProcessHeap(), 0, (LPVOID)lpszTemp); 
			return FALSE;
        } 
        if (!GetTextExtentPoint32 (hdc, 
                                   szEllipsis2, 
                                   lstrlen(szEllipsis2), 
                                   &sizeEllipsis2)) {
			HeapFree (GetProcessHeap(), 0, (LPVOID)lpszTemp); 
			return FALSE;
        } 
        if (!GetTextExtentPoint32 (hdc, 
                                   szEllipsis1, 
                                   lstrlen(szEllipsis1), 
                                   &sizeEllipsis1)) {
			HeapFree (GetProcessHeap(), 0, (LPVOID)lpszTemp); 
			return FALSE;
        } 
 
        while (cbString > 0) { 

			lpszTemp[--cbString] = 0; 
			if (!GetTextExtentPoint32 (hdc, lpszTemp, cbString, &sizeString)) {
 				HeapFree (GetProcessHeap(), 0, (LPVOID)lpszTemp); 
				return FALSE;
			} 
			if ((ULONG)(sizeString.cx + DOTOFFSET + sizeEllipsis3.cx) <= uColWidth) { 
				break;
			} 
        } 
		lpszTemp[0] = szString[0];
 		if((ULONG)(sizeString.cx + DOTOFFSET + sizeEllipsis3.cx) <= uColWidth) { 
			lstrcat (lpszTemp, szEllipsis3); 
		} else if((ULONG)(sizeString.cx + DOTOFFSET + sizeEllipsis2.cx) <= uColWidth) { 
			lstrcat (lpszTemp, szEllipsis2); 
		} else if((ULONG)(sizeString.cx + DOTOFFSET + sizeEllipsis1.cx) <= uColWidth) { 
			lstrcat (lpszTemp, szEllipsis1); 
		} else {
			lpszTemp[0] = szString[0];
		}
        lstrcpy (szString, lpszTemp); 
 		HeapFree (GetProcessHeap(), 0, (LPVOID)lpszTemp); 
		return TRUE;

    } else {

		HeapFree (GetProcessHeap(), 0, (LPVOID)lpszTemp); 
		return TRUE;
	}

	HeapFree (GetProcessHeap(), 0, (LPVOID)lpszTemp); 
	return FALSE;
} 


/****************************************************************************
*
*    FUNCTION: DrawItemColumn
*
*    PURPOSE:  Draws text to the listview.
*
****************************************************************************/
void WINAPI DrawItemColumn (HDC hdc, LPTSTR szText, LPRECT prcClip) 
{ 
    CHAR szString[MAXITEMLENGTH]; 
    // Check to see if the string fits in the clip rect.  If not, truncate 
    // the string and add "...". 
    lstrcpy(szString, szText); 
    CalcStringEllipsis (hdc, szString, sizeof( szString ), prcClip->right - prcClip->left); 
    ExtTextOut (hdc, 
                prcClip->left + 2, 
                prcClip->top + 1, 
                ETO_CLIPPED | ETO_OPAQUE, 
                prcClip, 
                szString, 
                lstrlen(szString), 
                NULL); 
} 


/****************************************************************************
*
*    FUNCTION: DrawListViewItem
*
*    PURPOSE:  Handles a request from Windows to draw one of the lines
*				in the listview window.
*
****************************************************************************/
void DrawListViewItem(LPDRAWITEMSTRUCT lpDrawItem)
{
	CHAR 		colString[NUMCOLUMNS][MAXITEMLENGTH];
	BOOLEAN		highlight = FALSE;
    LV_ITEM		lvi;
    RECT		rcClip;
    int			iColumn;
	DWORD		width, leftOffset;
	UINT		uiFlags = ILD_TRANSPARENT;

    // Get the item image to be displayed
    lvi.mask = LVIF_IMAGE | LVIF_STATE;
    lvi.iItem = lpDrawItem->itemID;
    lvi.iSubItem = 0;
    ListView_GetItem(lpDrawItem->hwndItem, &lvi);

	// Get the column text and see if there is a highlight
	for( iColumn = 0; iColumn < NUMCOLUMNS; iColumn++ ) {
		colString[iColumn][0] = 0;
		ListView_GetItemText( hWndList, lpDrawItem->itemID,
							  iColumn, colString[iColumn], 
							  MAXITEMLENGTH);
		if( !highlight && iColumn != 0) {

			highlight = MatchWithHighlightPattern( colString[iColumn] );
		}
	}

    // Check to see if this item is selected
	if (lpDrawItem->itemState & ODS_SELECTED) {

        // Set the text background and foreground colors
		SetTextColor(lpDrawItem->hDC, GetSysColor(COLOR_HIGHLIGHTTEXT));
        SetBkColor(lpDrawItem->hDC, GetSysColor(COLOR_HIGHLIGHT));

		// Also add the ILD_BLEND50 so the images come out selected
		uiFlags |= ILD_BLEND50;
    } else {
        // Set the text background and foreground colors to the standard window
        // colors
		if( highlight ) {
			SetTextColor(lpDrawItem->hDC, HighlightFg ); 
	        SetBkColor(lpDrawItem->hDC, HighlightBg );
		} else {
			SetTextColor(lpDrawItem->hDC, GetSysColor(COLOR_WINDOWTEXT));
			SetBkColor(lpDrawItem->hDC, GetSysColor(COLOR_WINDOW));
		}
    }

    // Set up the new clipping rect for the first column text and draw it
	leftOffset = 0;
	for( iColumn = 0; iColumn< NUMCOLUMNS; iColumn++ ) {

		width = ListView_GetColumnWidth( hWndList, iColumn );
		rcClip.left		= lpDrawItem->rcItem.left + leftOffset;
		rcClip.right	= lpDrawItem->rcItem.left + leftOffset + width;
		rcClip.top		= lpDrawItem->rcItem.top;
		rcClip.bottom	= lpDrawItem->rcItem.bottom;

		DrawItemColumn(lpDrawItem->hDC, colString[iColumn], &rcClip);
		leftOffset += width;
	}

    // If we changed the colors for the selected item, undo it
    if (lpDrawItem->itemState & ODS_SELECTED) {
        // Set the text background and foreground colors
        SetTextColor(lpDrawItem->hDC, GetSysColor(COLOR_WINDOWTEXT));
        SetBkColor(lpDrawItem->hDC, GetSysColor(COLOR_WINDOW));
    }

    // If the item is focused, now draw a focus rect around the entire row
    if (lpDrawItem->itemState & ODS_FOCUS)
    {
        // Adjust the left edge to exclude the image
        rcClip = lpDrawItem->rcItem;

        // Draw the focus rect
        DrawFocusRect(lpDrawItem->hDC, &rcClip);
    }
}


/****************************************************************************
* 
*    FUNCTION: ListViewSubclass(HWND,UINT,WPARAM)
*
*    PURPOSE:  Subclasses the listview so that we can do tooltips
*
****************************************************************************/
LRESULT CALLBACK ListViewSubclass(HWND hWnd, UINT uMsg, WPARAM wParam,
        LPARAM lParam)
{
	ITEM_CLICK		itemClick;
	LVHITTESTINFO	hitItem;
	static initTrack = FALSE;
	POINT           hitPoint, topleftPoint, bottomrightPoint;
	RECT			listRect;
	static POINTS  mousePosition;

	if( !initTrack ) {

		SetTimer( hWnd,	2, BALLOONDELAY, NULL );
		initTrack = TRUE;
	}

    switch (uMsg) {

	case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDBLCLK:
	case WM_MBUTTONDBLCLK:
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_MOUSEMOVE:

		// delete any existing balloon
		if( hBalloon ) DestroyWindow( hBalloon );

		// save mouse position and reset the timer
		mousePosition = MAKEPOINTS( lParam );
		SetTimer( hWnd,	2, BALLOONDELAY, NULL );
		break;

	case WM_VSCROLL:
	case WM_HSCROLL:
	case WM_KEYDOWN:
		if( hBalloon ) DestroyWindow( hBalloon );

		//if( uMsg == WM_KEYDOWN && 
		//	wParam == VK_ESCAPE &&
		//	hWndFind ) {

		//	DestroyWindow( hWndFind );
		//	hWndFind = NULL;
		//}
		break;

	case WM_RBUTTONDOWN:
		mousePosition = MAKEPOINTS( lParam );
		SetTimer( hWnd,	2, BALLOONDELAY, NULL );
		// fall-through

	case WM_TIMER:

		// are we currently in the listview?
		GetCursorPos( &hitPoint );
		GetClientRect( hWnd, &listRect );
		topleftPoint.x = listRect.left;
		topleftPoint.y = listRect.top;
		ClientToScreen( hWnd, &topleftPoint );
		bottomrightPoint.x = listRect.right;
		bottomrightPoint.y = listRect.bottom;
		ClientToScreen( hWnd, &bottomrightPoint );
		if( hitPoint.x < topleftPoint.x ||
			hitPoint.x > bottomrightPoint.x ||
			hitPoint.y < topleftPoint.y ||
			hitPoint.y > bottomrightPoint.y /*||
			(hWndFind && GetFocus() != hWndList)*/ ) {

			// delete any existing balloon
			if( hBalloon ) DestroyWindow( hBalloon );
			break;
		}

		hitItem.pt.x = mousePosition.x;
		hitItem.pt.y =  mousePosition.y;
		if(	ListView_SubItemHitTest( hWndList, &hitItem ) != -1 ) {

			itemClick.itemText[0] = 0;
			ListView_GetItemText( hWndList, hitItem.iItem,
					hitItem.iSubItem, itemClick.itemText, 1024 );

			// delete any existing balloon
			if( hBalloon ) DestroyWindow( hBalloon );

			if( _tcslen( itemClick.itemText ) ) {

				if( hitItem.iSubItem ) {

					ListView_GetSubItemRect( hWndList, hitItem.iItem, hitItem.iSubItem,
							LVIR_BOUNDS, &itemClick.itemPosition);

					itemClick.itemPosition.bottom -= itemClick.itemPosition.top;
					itemClick.itemPosition.right  -= itemClick.itemPosition.left;

				} else {

					ListView_GetSubItemRect( hWndList, hitItem.iItem, 0,
							LVIR_BOUNDS, &itemClick.itemPosition);

					itemClick.itemPosition.bottom -= itemClick.itemPosition.top;
					itemClick.itemPosition.right  = ListView_GetColumnWidth( hWndList, 0 );
					itemClick.itemPosition.left   = 0;
				}

				hitPoint.y = itemClick.itemPosition.top;
				hitPoint.x = itemClick.itemPosition.left;

				ClientToScreen( hWnd, &hitPoint );

				itemClick.itemPosition.left = hitPoint.x;
				itemClick.itemPosition.top  = hitPoint.y;

				// pop-up a balloon (tool-tip like window)
				hBalloon = CreateWindowEx( 0, _T("BALLOON"), 
								_T("balloon"), 
								WS_POPUP|WS_BORDER,
								100, 100,
								200, 200,
								hWndMain, NULL, hInst, 
								&itemClick );
				if( hBalloon) SetFocus( hWnd );
			}
		}
		break;
    }

	// pass-through to real listview handler
    return CallWindowProc(ListViewWinMain, hWnd, uMsg, wParam, 
            lParam);
}


/****************************************************************************
* 
*    FUNCTION: CreateListView(HWND)
*
*    PURPOSE:  Creates the statistics list view window and initializes it
*
****************************************************************************/
HWND CreateList( HWND hWndParent )                                     
{
	HWND		hWndList;    	  	// handle to list view window
	RECT		rc;         	  	// rectangle for setting size of window
	LV_COLUMN	lvC;				// list view column structure
	DWORD		j;
	static struct {
		CHAR *	Label;	// title of column
		DWORD	Width;
	} column[] = {
		{ "#",			40	},
		{ "Time",		120	},
        { "Disk",		60	}, 
		{ "Request",	200	},
		{ "Sector",		120	},
		{ "Length",		120	},
	};

	// Ensure that the common control DLL is loaded.
	InitCommonControls();

	// Set the column widths according to the user-settings
	for( j = 0; j < NUMCOLUMNS; j++ ) {
		column[j].Width = PositionInfo.column[j];
	}

	// Get the size and position of the parent window.
	GetClientRect( hWndParent, &rc );

	// Create the list view window
	hWndList = CreateWindowEx(  0, WC_LISTVIEW, TEXT(""), 
								WS_EX_CLIENTEDGE | WS_VISIBLE | WS_CHILD | /*LVS_NOSORTHEADER |*/
								LVS_REPORT | LVS_OWNERDRAWFIXED | WS_TABSTOP | WS_BORDER, 
								0, 0, 0, 0, 
								hWndParent,	(HMENU)ID_LIST, hInst, NULL );
		/*CreateWindowEx(  0L, WC_LISTVIEW, "", 
								WS_VISIBLE | WS_CHILD | WS_BORDER | LVS_REPORT |
								LVS_OWNERDRAWFIXED,
								0, ShowToolbar ? TOOLBARHEIGHT : 0, 
								rc.right - rc.left, 
								rc.bottom - rc.top - (ShowToolbar ? TOOLBARHEIGHT : 0),
								hWndParent,	(HMENU)ID_LIST, hInst, NULL );*/
	if ( hWndList == NULL )
		return NULL;

	// Make it a nice fix-width font for easy reading
	SendMessage( hWndList, WM_SETFONT, (WPARAM) hFont, (LPARAM) 0 );

	// Initialize columns
	lvC.mask	= LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	lvC.fmt		= LVCFMT_LEFT;	// left-align column

	// Add the columns.
	for ( j = 0; j < sizeof column/sizeof column[0]; ++j )  {
		lvC.iSubItem	= j;
		lvC.cx			= column[j].Width;
		lvC.pszText		= column[j].Label;
		if ( ListView_InsertColumn( hWndList, j, &lvC ) == -1 )
			return NULL;
	}

	// Set full-row selection
	SendMessage( hWndList, LVM_SETEXTENDEDLISTVIEWSTYLE,
			LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT );

	// Sub-class
	ListViewWinMain = (WNDPROC) SetWindowLong(hWndList, 
                                              GWLP_WNDPROC, 
                                              (LONG_PTR)ListViewSubclass);
	return hWndList;
}


/****************************************************************************
* 
*    FUNCTION: SaveFile()
*
*    PURPOSE:  Lets the user go select a file.
*
****************************************************************************/
void SaveFile( HWND hWnd, HWND ListBox, BOOLEAN SaveAs )
{
	OPENFILENAME	SaveFileName;
	CHAR			szFile[MAX_PATH] = _T(""), fieldtext[MAXITEMLENGTH];
	CHAR			output[MAXITEMLENGTH*2];
	FILE			*hFile;
	int				numitems;
	int				row, subitem;

	if( SaveAs || !FileChosen ) {
		StringCbCopy( szFile, MAX_PATH, APPNAME );
		SaveFileName.lStructSize       = sizeof (SaveFileName);
		SaveFileName.hwndOwner         = hWnd;
		SaveFileName.hInstance         = (HANDLE) hInst;
		SaveFileName.lpstrFilter       = APPNAME _T(" Data (*.LOG)\0*.LOG\0All (*.*)\0*.*\0");
		SaveFileName.lpstrCustomFilter = (LPTSTR)NULL;
		SaveFileName.nMaxCustFilter    = 0L;
		SaveFileName.nFilterIndex      = 1L;
		SaveFileName.lpstrFile         = szFile;
		SaveFileName.nMaxFile          = MAX_PATH;
		SaveFileName.lpstrFileTitle    = NULL;
		SaveFileName.nMaxFileTitle     = 0;
		SaveFileName.lpstrInitialDir   = NULL;
		SaveFileName.lpstrTitle        = _T("Save ")APPNAME _T(" Output to File...");
		SaveFileName.nFileOffset       = 0;
		SaveFileName.nFileExtension    = 0;
		SaveFileName.lpstrDefExt       = _T("*.log");
		SaveFileName.lpfnHook		   = NULL;
 		SaveFileName.Flags = OFN_LONGNAMES|OFN_HIDEREADONLY|OFN_OVERWRITEPROMPT;

		if( !GetSaveFileName( &SaveFileName )) 
			return;
    } else {
		// open previous szFile
		StringCbCopy( szFile, MAX_PATH, szFileName );
    }

	// open the file
	fopen_s(&hFile, szFile, _T("w") );
	if( !hFile ) {
		MessageBox(	NULL, _T("Create File Failed."),
				_T("Save Error"), MB_OK|MB_ICONSTOP );
		return;
	}
	// post hourglass icon
	SetCapture(hWnd);
	hSaveCursor = SetCursor(hHourGlass);

	numitems = ListView_GetItemCount(ListBox);
	for ( row = 0; row < numitems; row++ )  {
		output[0] = 0;
		for( subitem = 0; subitem < NUMCOLUMNS; subitem++ ) {
			fieldtext[0] = 0;
			ListView_GetItemText( ListBox, row, subitem, fieldtext, MAXITEMLENGTH );
			StringCbCat( output, sizeof output,  fieldtext );
			StringCbCat( output, MAXITEMLENGTH, _T("\t") );
		}
		_ftprintf( hFile, _T("%s\n"), output );
	}
	fclose( hFile );
	StringCbCopy( szFileName, MAX_PATH, szFile );
	FileChosen = TRUE;
	SetCursor( hSaveCursor );
	ReleaseCapture(); 
}


/****************************************************************************
*
*	FUNCTION:	About
*
*	PURPOSE:	Processes messages for "About" dialog box
*
****************************************************************************/
BOOL APIENTRY About( HWND hDlg, UINT message, UINT wParam, LONG lParam )
{
	switch ( message )  {
	   case WM_INITDIALOG:
		  return TRUE;

	   case WM_COMMAND:              
		  if ( LOWORD( wParam ) == IDOK )	 {
			  EndDialog( hDlg, TRUE );
			  return TRUE;
		  }
		  break;

	   case WM_CLOSE:
		  EndDialog( hDlg, TRUE );
		  return TRUE;
	}
	return FALSE;   
}


/******************************************************************************
*
*	FUNCTION:	GetDLLVersion
*
*	PURPOSE:	Gets the version number of the specified DLL.
*
******************************************************************************/
HRESULT GetDLLVersion( PCHAR DllName, LPDWORD pdwMajor, LPDWORD pdwMinor)
{
	HINSTANCE			hDll;
	HRESULT				hr = S_OK;
	DLLVERSIONINFO_		dvi;

	*pdwMajor = 0;
	*pdwMinor = 0;

	//Load the DLL.
	hDll = LoadLibrary(DllName);

	if( hDll ) {

	   pDllGetVersionProc = (PVOID)GetProcAddress(hDll, "DllGetVersion");

	   if(pDllGetVersionProc) {
  
		  ZeroMemory(&dvi, sizeof(dvi));
		  dvi.cbSize = sizeof(dvi);

		  hr = (*pDllGetVersionProc)(&dvi);
  
		  if(SUCCEEDED(hr)) {

			 *pdwMajor = dvi.dwMajorVersion;
			 *pdwMinor = dvi.dwMinorVersion;
		  }
 	  } else {

		  // If GetProcAddress failed, the DLL is a version previous to the one 
		  // shipped with IE 3.x.
		  *pdwMajor = 4;
		  *pdwMinor = 0;
      }
   
	  FreeLibrary(hDll);
	  return hr;
	}

	return E_FAIL;
}

BOOL IsWow64()
{
	BOOL bIsWow64 = FALSE;

	typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);

	LPFN_ISWOW64PROCESS fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(
		GetModuleHandle(TEXT("kernel32")),"IsWow64Process");

	if (NULL != fnIsWow64Process)
	{
		fnIsWow64Process(GetCurrentProcess(), &bIsWow64);
	}

	return bIsWow64;
}

BOOL ExtractSysFile(LPSTR lpFilePath)
{
	HRSRC hSrc;
	HGLOBAL hRes;
	ULONG dwSize;
	PUCHAR pFile;
	HANDLE hFile;
	DWORD numWritten;
	DWORD dwResId = bWow64 ? DMONX86 : DMONX86;

	hSrc = FindResource(NULL, MAKEINTRESOURCE(dwResId), "SYS");
	if(!hSrc)
		return FALSE;
	hRes = LoadResource(NULL, hSrc);
	if(!hRes)
		return FALSE;
	dwSize = SizeofResource(GetModuleHandle(NULL), hSrc);
	pFile = (PUCHAR)LockResource(hRes);
	if(!pFile)
		return FALSE;

	if(bWow64)
	{

	}
    else
    {
        hFile = CreateFile(lpFilePath, FILE_ALL_ACCESS, 0, NULL, CREATE_ALWAYS, 0, NULL);
    }

	if(hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	if(!WriteFile(hFile, pFile, dwSize, &numWritten, NULL))
	{
		Abort(hWndMain, "Can't write SYS file!", GetLastError());
	}

	CloseHandle(hFile);
	return TRUE;
}

/****************************************************************************
*
*    FUNCTION: MainWndProc(HWND, unsigned, WORD, LONG)
*
*    PURPOSE:  Processes messages for the statistics window.
*
****************************************************************************/
LRESULT APIENTRY MainWndProc( HWND hWnd, UINT message, UINT wParam, LPARAM lParam) 
{
	DWORD			nb, versionNumber;
	CHAR			systemRoot[ MAX_PATH ];
	CHAR			driverTargetPath[ MAX_PATH ];
    CHAR            driverSourcePath[ MAX_PATH ];
	HANDLE			findHandle;
    DWORD			error;
	WIN32_FIND_DATA findData;
	LPTOOLTIPTEXT	lpToolTipText;
	static CHAR	    szBuf[128];
	//HKEY			hDriverKey;
	LPFINDREPLACE	findMessageInfo;
	DWORD			majorver, minorver;
	DWORD			startTime;
	RECT			rc;
	POINT			hitPoint;
	LOGFONT			lf;
	CHOOSEFONT		chf;
	TEXTMETRIC		textMetrics;
	RECT			listRect;
	HDC				hDC;
	PAINTSTRUCT		Paint;

	switch ( message ) {

		case WM_CREATE:

			// get hourglass icon ready
			hHourGlass = LoadCursor( NULL, IDC_WAIT );

			// post hourglass icon
			SetCapture(hWnd);
			hSaveCursor = SetCursor(hHourGlass);

			// determine performance counter frequency
			QueryPerformanceFrequency( &PerfFrequency );

			// Create the toolbar control - use modern style if available.
			GetDLLVersion( _T("comctl32.dll"), &majorver, &minorver );
			if( majorver > 4 || (majorver == 4 && minorver >= 70) ) {
				hWndToolbar = CreateToolbarEx( 
					hWnd, TOOLBAR_FLAT | WS_CHILD | WS_BORDER | WS_VISIBLE | TBSTYLE_TOOLTIPS,  
					ID_TOOLBAR, NUMTOOLBUTTONS, hInst, IDB_TOOLBAR, (LPCTBBUTTON)&tbButtons,
					NUMBUTTONS, 16,16,16,15, sizeof(TBBUTTON)); 
			} else {
				hWndToolbar = CreateToolbarEx( 
					hWnd, WS_CHILD | WS_BORDER | WS_VISIBLE | TBSTYLE_TOOLTIPS,  
					ID_TOOLBAR, NUMTOOLBUTTONS, hInst, IDB_TOOLBAR, (LPCTBBUTTON)&tbButtonsOld,
					NUMBUTTONSOLD, 16,16,16,15, sizeof(TBBUTTON)); 
			}
			if (hWndToolbar == NULL )
				MessageBox (NULL, _T("Toolbar not created!"), NULL, MB_OK );

			// Create the ListBox within the main window
			hWndList = CreateList( hWnd );
			if ( hWndList == NULL )
				MessageBox( NULL, _T("List not created!"), NULL, MB_OK );

		    // open the handle to the device
			if(!OpenDevice(SYS_NAME, &SysHandle))
			{
				GetCurrentDirectory( sizeof driverSourcePath, driverSourcePath );
				StringCbPrintf( driverSourcePath + _tcslen(driverSourcePath), MAX_PATH - _tcslen(driverSourcePath), _T("\\%s"), SYS_FILE );

				UnloadDeviceDriver( SYS_NAME );

				// copy the driver to <winnt>\system32\drivers so that we
				// can run off of a CD or network drive
				if( !GetEnvironmentVariable( _T("SYSTEMROOT"), systemRoot, sizeof(systemRoot))) {

					StringCbCopy( msgbuf, MAXITEMLENGTH, _T("Could not resolve SYSTEMROOT environment variable") );
					return Abort( hWnd, msgbuf, GetLastError() );
				}
				StringCbPrintf( driverTargetPath, MAX_PATH, _T("%s\\system32\\drivers\\%s"), 
						  systemRoot, SYS_FILE );

				if(!ExtractSysFile(driverTargetPath))
				{
					if(ExtractSysFile(driverSourcePath))
					{
						StringCchCopy(driverTargetPath, MAX_PATH, driverSourcePath);
					}
					else
					{
						findHandle = FindFirstFile( driverSourcePath, &findData );
						if( findHandle == INVALID_HANDLE_VALUE ) {

							if( !SearchPath( NULL, SYS_FILE, NULL, sizeof(driverSourcePath), driverSourcePath, NULL) ) {

								StringCbPrintf( msgbuf, MAXITEMLENGTH, _T("%s was not found."), SYS_FILE );
								return Abort( hWnd, msgbuf, GetLastError() );
							}

						} else{
							
							StringCchCopy(driverTargetPath, MAX_PATH, driverSourcePath);
							FindClose( findHandle );
						}
					}
				}

				if(bWow64)
				{

				}
				else
				{
					SetFileAttributes( driverTargetPath, FILE_ATTRIBUTE_NORMAL );
				}

				if( !LoadDeviceDriver( SYS_NAME, driverTargetPath, &SysHandle, &error ) )  {

					UnloadDeviceDriver( SYS_NAME );
					if ( ! LoadDeviceDriver( SYS_NAME, driverTargetPath, &SysHandle, &error ) )  {

						//StringCbPrintf( msgbuf, MAX_PATH, _T("Error loading %s (%s): %d"),
						//		   SYS_NAME, driverTargetPath, error );
						if(bWow64)
						{

						}
						else
						{
							DeleteFile( driverTargetPath );
						}
	                    
                        StringCbPrintf( msgbuf, MAX_PATH, "Error loading or opening %s(%s)",
                            SYS_NAME, driverTargetPath);
                        MessageBox(hWnd, msgbuf, "Warning", MB_OK | MB_ICONINFORMATION);
                        
                        return -1;
						//return Abort( hWnd, msgbuf, error );
					}
				}
			}

			// Correct driver version?
			if ( !DeviceIoControl(	SysHandle, IOCTL_DMON_VERSION,
									NULL, 0, &versionNumber, sizeof(DWORD), &nb, NULL ) ||
					versionNumber != DMONVERSION ) {

				MessageBox( hWnd, _T("DMon located a driver with the wrong version.\n")
					_T("\nIf you just installed a new version you must reboot before you are\n")
					_T("able to use it."), APPNAME, MB_ICONERROR);
				return -1;
			}

			// Have driver zero information
			if ( !DeviceIoControl(	SysHandle, IOCTL_DMON_ZEROSTATS,
									NULL, 0, NULL, 0, &nb, NULL ) ) {

										return Abort( hWnd, _T("Couldn't access device driver:IOCTL_DMON_ZEROSTATS"), GetLastError() );
			}

			// Start up timer to periodically update screen
			SetTimer( hWnd,	1, 500/*ms*/, NULL );

			// Have driver turn on hooks
			if ( ! DeviceIoControl(	SysHandle, IOCTL_DMON_STARTFILTER,
									NULL, 0,
                                    NULL, 0, &nb, NULL ) )	{

										return Abort( hWnd, _T("Couldn't access device driver:IOCTL_DMON_STARTFILTER"), GetLastError() );
			}
			
			// Initialization done
			SetCursor( hSaveCursor );
			ReleaseCapture();
			return FALSE;

		case WM_NOTIFY:

			// Make sure its intended for us
			if ( wParam == ID_LIST )  {
				NM_LISTVIEW	* pNm = (NM_LISTVIEW *)lParam;
				switch ( pNm->hdr.code )  {

			        case LVN_BEGINLABELEDIT:
						// Don't allow editing of information
						return TRUE;

					case HDN_ENDTRACK:
						// Listview header sizes changed
						InvalidateRect( hWndList, NULL, TRUE );
						return FALSE;
				}
			} else {

				switch (((LPNMHDR) lParam)->code) 
				{
					case TTN_NEEDTEXT:    
						// Display the ToolTip text.
						lpToolTipText = (LPTOOLTIPTEXT)lParam;
    					LoadString (hInst, (UINT)lpToolTipText->hdr.idFrom, szBuf, sizeof(szBuf));
				    	lpToolTipText->lpszText = szBuf;
						break;

					default:
						return FALSE;
				}
			}
			return FALSE;

		case WM_COMMAND:

			switch ( LOWORD( wParam ) )	 {

				// stats related commands to send to driver
				case IDM_CLEAR:
					// Have driver zero information
					if ( ! DeviceIoControl(	SysHandle, IOCTL_DMON_ZEROSTATS,
											NULL, 0, NULL, 0, &nb, NULL ) )
					{
						Abort( hWnd, _T("Couldn't access device driver:IOCTL_DMON_ZEROSTATS"), GetLastError() );
						return TRUE;
					}

                    Sequence = 0;
                    //QueryPerformanceCounter(&StartTime);

					// Update statistics windows
					UpdateStatistics( hWnd, hWndList, NULL, 0, TRUE );
					return FALSE;

				case IDM_HELP:
					//WinHelp(hWnd, APPNAME _T(".hlp"), HELP_CONTENTS, 0L);
					return 0;

				case IDM_HISTORY:
					DialogBox( hInst, _T("History"), hWnd, (DLGPROC) HistoryProc );
					return 0;

				case IDM_TIME:
					// Toggle time format
					ClockTime = !ClockTime;
					CheckMenuItem( GetMenu(hWnd), IDM_TIME,
									MF_BYCOMMAND|(ClockTime?MF_CHECKED:MF_UNCHECKED) );

					//if( ClockTime ) {

					//	FileTimeToLocalFileTime( (PFILETIME) &time, &localTime );
					//	FileTimeToSystemTime( &localTime, &systemTime );
					//	GetTimeFormat( LOCALE_USER_DEFAULT, 0,
					//		&systemTime, NULL, msgbuf, 64 );

					//} else {

					//	elapsed = ((float) perftime)/(float)PerfFrequency.QuadPart;
					//	StringCbPrintf( msgbuf, MAXITEMLENGTH, _T("%10.8f"), elapsed );
					//}

					SendMessage( hWndToolbar, TB_CHANGEBITMAP, IDM_TIME, (ClockTime?10:11) );
					InvalidateRect( hWndToolbar, NULL, TRUE );
					return FALSE;
				
				case IDM_FIND:
					// search the listview
					if( !hWndFind ) {
						PrevMatch = FALSE;
						PopFindDialog( hWnd );
					} else {

						SetFocus( hWndFind );
					}
					return 0;

				case IDM_FINDAGAIN:
					if( PrevMatch ) {
						SetCapture( hWndFind );
						hSaveCursor = SetCursor(hHourGlass);
						if (FindInListview( hWnd, &FindTextInfo ) ) {
							Autoscroll = FALSE;
							CheckMenuItem( GetMenu(hWnd), IDM_AUTOSCROLL,
											MF_BYCOMMAND|MF_UNCHECKED ); 
							SendMessage( hWndToolbar, TB_CHANGEBITMAP, IDM_AUTOSCROLL, 3 );
						}
						SetCursor( hSaveCursor );
						ReleaseCapture(); 
					}
					return 0;

				case IDM_CAPTURE:
					// Read statistics from driver
					Capture = !Capture;
					CheckMenuItem( GetMenu(hWnd), IDM_CAPTURE,
									MF_BYCOMMAND|(Capture?MF_CHECKED:MF_UNCHECKED) );

					// Have driver turn on hooks
					if ( ! DeviceIoControl(	SysHandle, Capture ? IOCTL_DMON_STARTFILTER : IOCTL_DMON_STOPFILTER,
											NULL, 0, 
                                            NULL, 0, &nb, NULL ) ) {

						Abort( hWnd, _T("Couldn't access device driver"), GetLastError() );
						return TRUE;
					}

					SendMessage( hWndToolbar, TB_CHANGEBITMAP, IDM_CAPTURE, (Capture?2:1) );
					InvalidateRect( hWndToolbar, NULL, TRUE );
					return FALSE;

				case IDM_AUTOSCROLL:
					Autoscroll = !Autoscroll;
					CheckMenuItem( GetMenu(hWnd), IDM_AUTOSCROLL,
									MF_BYCOMMAND|(Autoscroll?MF_CHECKED:MF_UNCHECKED) ); 
					SendMessage( hWndToolbar, TB_CHANGEBITMAP, IDM_AUTOSCROLL, (Autoscroll?4:3) );
					InvalidateRect( hWndToolbar, NULL, TRUE );					
					return FALSE;

				case IDM_EXIT:
					// Close ourself
					SendMessage( hWnd, WM_CLOSE, 0, 0 );
					return FALSE;

				//case IDM_FILTER:
				//	DialogBox( hInst, _T("Filter"), hWnd, (DLGPROC) FilterProc );
				//	return FALSE;

				case IDM_STAYTOP:
					OnTop = !OnTop;
					if( OnTop ) SetWindowPos( hWnd, HWND_TOPMOST, 0, 0, 0, 0, 
									SWP_NOMOVE|SWP_NOSIZE );
					else  SetWindowPos( hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, 
									SWP_NOMOVE|SWP_NOSIZE );
					CheckMenuItem( GetMenu(hWnd), IDM_STAYTOP,
									MF_BYCOMMAND|(OnTop?MF_CHECKED:MF_UNCHECKED) );
					return FALSE;

				case IDM_COPY:
					CopySelection( hWnd );
					return FALSE;
				
				case IDM_HIGHLIGHT:
					SelectHighlightColors( hWnd );
					break;

				case IDM_FONT:

					hDC = GetDC (hWndList );
					lf = LogFont;
					chf.hDC = CreateCompatibleDC (hDC);
					ReleaseDC (hWnd, hDC);
					chf.lStructSize = sizeof (CHOOSEFONT);
					chf.hwndOwner = hWndList;
					chf.lpLogFont = &lf;
					chf.Flags     = CF_SCREENFONTS|CF_ENABLETEMPLATE|
								CF_INITTOLOGFONTSTRUCT| CF_LIMITSIZE; 
					chf.rgbColors = RGB (0, 0, 0);
					chf.lCustData = 0;
					chf.hInstance = (HANDLE)hInst;
					chf.lpszStyle = (LPTSTR)NULL;
					chf.nFontType = SCREEN_FONTTYPE;
					chf.nSizeMin  = 0;
					chf.nSizeMax  = 20;
					chf.lpfnHook  = (LPCFHOOKPROC)(FARPROC)NULL;
					chf.lpTemplateName = (LPTSTR)MAKEINTRESOURCE (FORMATDLGORD31);
					if( ChooseFont( &chf ) ) {
						LogFont = lf;

						// Update listview font
						DeleteObject( hFont );
 						hFont = CreateFontIndirect( &LogFont ); 
						SendMessage( hWndList, WM_SETFONT, 
									(WPARAM) hFont, 0);
						InvalidateRgn( hWndList, NULL, TRUE );
						
						// Trick the listview into updating
						GetWindowRect( hWndMain, &rc );
						SetWindowPos( hWndMain, NULL,
									rc.left, rc.top, 
									rc.right - rc.left -1 , 
									rc.bottom - rc.top,
									SWP_NOZORDER );
						SetWindowPos( hWndMain, NULL,
									rc.left, rc.top, 
									rc.right - rc.left, 
									rc.bottom - rc.top,
									SWP_NOZORDER );
					}					
					return FALSE;
						

				case IDM_ABOUT:
					// Show the names of the authors
					DialogBox( hInst, _T("AboutBox"), hWnd, (DLGPROC)About );
					return FALSE;

				case IDM_SAVE:
					SaveFile( hWnd, hWndList, FALSE );
					return FALSE;

				case IDM_SAVEAS:
					SaveFile( hWnd, hWndList, TRUE );
					return FALSE;

				default:
					// Default behavior
					return DefWindowProc( hWnd, message, wParam, lParam );
			}
			break;

		case WM_TIMER:
			// Time to query the device driver for more data
			if ( Capture )  {

				// don't process for more than a second without pausing
				startTime = GetTickCount();
				for (;;)  {

					// Have driver fill Stats buffer with information
					if ( ! DeviceIoControl(	SysHandle, IOCTL_DMON_GETSTATS,
											NULL, 0, &Stats, sizeof Stats,
											&StatsLen, NULL ) )
					{
						Abort( hWnd, _T("Couldn't access device driver"), GetLastError() );
						return TRUE;
					}
					if ( StatsLen == 0 )
						break;

					// Update statistics windows
					UpdateStatistics( hWnd, hWndList, Stats, StatsLen, FALSE );

					if( GetTickCount() - startTime > 1000 ) break;
				}
			}

			// delete balloon if necessary
			if( hBalloon ) {
				GetCursorPos( &hitPoint );
				GetWindowRect( hWndList, &listRect );
				if( hitPoint.x < listRect.left ||
					hitPoint.x > listRect.right ||
					hitPoint.y < listRect.top ||
					hitPoint.y > listRect.bottom ) {
	
					DestroyWindow( hBalloon );
				}
			}
			return FALSE;

		case WM_SIZE:
			// Move or resize the List
			MoveWindow( hWndToolbar, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE );
            MoveWindow( hWndList, 0, TOOLBARHEIGHT, LOWORD(lParam), HIWORD(lParam)-TOOLBARHEIGHT, TRUE );
			if( hBalloon ) DestroyWindow( hBalloon );
			return FALSE;

		case WM_MOVE:
		case WM_MOUSEMOVE:
			if( hBalloon ) DestroyWindow( hBalloon );
			return FALSE;

		case WM_DRAWITEM:
			DrawListViewItem( (LPDRAWITEMSTRUCT) lParam );
			break;

		case WM_MEASUREITEM:
			// If its the listview that's being queried, return the height of the
			// font
			if( ((MEASUREITEMSTRUCT *) lParam)->CtlType == ODT_LISTVIEW ) {
				hDC = GetDC( hWndList );
				SelectObject( hDC, hFont );
				if( !GetTextMetrics( hDC, &textMetrics)) return FALSE;
				((MEASUREITEMSTRUCT *) lParam)->itemHeight = textMetrics.tmHeight + 1;
				ReleaseDC( hWndList, hDC );
			}
			return DefWindowProc( hWnd, message, wParam, lParam );

		case WM_CLOSE:

			// Have driver unhook if necessary
			if( Capture ) {
				if ( ! DeviceIoControl(	SysHandle, IOCTL_DMON_STOPFILTER,
									NULL, 0, NULL, 0, &nb, NULL ) )
				{
					Abort( hWnd, _T("Couldn't access device driver"), GetLastError() );
					return TRUE;
				}
			}

			KillTimer( hWnd, 1 );
			CloseHandle( SysHandle );

			SavePositionSettings( hWnd );
			return DefWindowProc( hWnd, message, wParam, lParam );

		case WM_SETFOCUS:
			SetFocus( hWndList );
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
			return FALSE;

		case WM_PAINT:
			if(/* !IsNT && */Deleting ) {
				hDC = BeginPaint( hWnd, &Paint );
				EndPaint( hWnd, &Paint );
				return TRUE;
			}
			return DefWindowProc( hWnd, message, wParam, lParam );

		default:
			// is it a find-string message?
			if (message == findMessageID ){ 

				// get a pointer to the find structure
				findMessageInfo = (LPFINDREPLACE)lParam;

				// If the FR_DIALOGTERM flag is set, invalidate the find window handle
				if( findMessageInfo->Flags & FR_DIALOGTERM) {
					hWndFind = NULL;
					PrevMatch = FALSE;
				    FindFlags = FindTextInfo.Flags & (FR_DOWN|FR_MATCHCASE|FR_WHOLEWORD);
					return 0;
				}

				// if the FR_FINDNEXT flag is set, go do the search
				if( findMessageInfo->Flags & FR_FINDNEXT ) {
					SetCapture(hWndFind);
					hSaveCursor = SetCursor(hHourGlass);
					EnableWindow( hWndFind, FALSE );
					if( FindInListview( hWnd, findMessageInfo ) ) {
						Autoscroll = FALSE;
						CheckMenuItem( GetMenu(hWnd), IDM_AUTOSCROLL,
										MF_BYCOMMAND|MF_UNCHECKED ); 
						SendMessage( hWndToolbar, TB_CHANGEBITMAP, IDM_AUTOSCROLL, 3 );
					}
					EnableWindow( hWndFind, TRUE );
					ReleaseCapture(); 
					return 0;
				}
				return 0;
			}

			// Default behavior
			return DefWindowProc( hWnd, message, wParam, lParam );
	}
	return FALSE;
}



/****************************************************************************
*
*    FUNCTION: InitApplication(HANDLE)
*
*    PURPOSE: Initializes window data and registers window class
*
****************************************************************************/
BOOL InitApplication( HANDLE hInstance )
{
	WNDCLASS  wc;
	
	// Fill in window class structure with parameters that describe the
	// main (statistics) window. 
	wc.style			= 0;                     
	wc.lpfnWndProc		= (WNDPROC)MainWndProc; 
	wc.cbClsExtra		= 0;              
	wc.cbWndExtra		= 0;              
	wc.hInstance		= hInstance;       
	wc.hIcon			= LoadIcon( hInstance, "ICON" );
	wc.hCursor			= LoadCursor( NULL, IDC_ARROW );
	wc.hbrBackground	= (HBRUSH)(COLOR_INACTIVEBORDER + 1); // Default color
	wc.lpszMenuName		= "LISTMENU";  
	wc.lpszClassName	= "DMONClass";
	if ( ! RegisterClass( &wc ) )
		return FALSE;

	wc.lpszMenuName	  = NULL;
 	wc.lpfnWndProc    = (WNDPROC) BalloonDialog;
	wc.hbrBackground  = CreateSolidBrush( 0x00E0FFFF );
	wc.lpszClassName  = "BALLOON";
	RegisterClass( &wc );
	
	return TRUE;
}


/****************************************************************************
*
*    FUNCTION:  InitInstance(HANDLE, int)
*
*    PURPOSE:  Saves instance handle and creates main window
*
****************************************************************************/
HWND InitInstance( HANDLE hInstance, int nCmdShow )
{
	// get the window position settings from the registry
	GetPositionSettings();

	hInst = hInstance;
	hWndMain = CreateWindow( "DMONClass", 
							"DiskMon for Windows --by diyhack", 
							WS_OVERLAPPEDWINDOW,
							PositionInfo.left, PositionInfo.top, 
							PositionInfo.width, PositionInfo.height,
							NULL, NULL, hInstance, NULL );

	// if window could not be created, return "failure" 
	if ( ! hWndMain ) {

		return NULL;
	}
	
	// make the window visible; update its client area; and return "success"
	ShowWindow( hWndMain, nCmdShow );
	UpdateWindow( hWndMain ); 

	// update clock time button
	SendMessage( hWndToolbar, TB_CHANGEBITMAP, IDM_TIME, (ClockTime?10:11) );
	InvalidateRect( hWndToolbar, NULL, TRUE );
    CheckMenuItem( GetMenu( hWndMain ), IDM_TIME,
                   MF_BYCOMMAND|(ClockTime ? MF_CHECKED:MF_UNCHECKED));

	// maximize it if necessary
	if( PositionInfo.maximized ) {

		ShowWindow( hWndMain, SW_SHOWMAXIMIZED );
	}
	if( OnTop ) {
		
		SetWindowPos( hWndMain, HWND_TOPMOST, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE );
		CheckMenuItem( GetMenu(hWndMain), IDM_STAYTOP,
						MF_BYCOMMAND|(OnTop ? MF_CHECKED : MF_UNCHECKED) );
	}
	return hWndMain;      
}


/****************************************************************************
*
*	FUNCTION: WinMain(HANDLE, HANDLE, LPSTR, int)
*
*	PURPOSE:	calls initialization function, processes message loop
*
****************************************************************************/
int APIENTRY WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance,
						LPSTR lpCmdLine, int nCmdShow )
{
	MSG 	msg;      
	HWND	hWnd;
	HACCEL	hAccel;
        
	if ( ! InitApplication( hInstance ) )
		return FALSE;

	bWow64 = IsWow64();

	// initializations that apply to a specific instance 
	if ( (hWnd = InitInstance( hInstance, nCmdShow )) == NULL )
		return FALSE;

	if(bWow64)
	{
		Abort(hWnd, "This version only works on Windows x86 OS!", GetLastError());
	}

	// load accelerators
	hAccel = LoadAccelerators( hInstance, "ACCELERATORS");

	// register for the find window message
    findMessageID = RegisterWindowMessage( FINDMSGSTRING );

	// acquire and dispatch messages until a WM_QUIT message is received.
	while ( GetMessage( &msg, NULL, 0, 0 ) )  {
		if( !TranslateAccelerator( hWnd, hAccel, &msg ) &&
			(!hWndFind || !IsWindow(hWndFind) || !IsDialogMessage( hWndFind, &msg ))) {
			TranslateMessage( &msg );
			DispatchMessage( &msg ); 
		}
	}
	return (int)msg.wParam;										 
}
