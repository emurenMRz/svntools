#include "framework.h"
#include "dumpviewer.h"
#include "ListView.h"
#include "DetailWindow.h"
#include "RevisionList.h"
#include "NodeList.h"
#include "SVNDump.h"
#include "misc.h"
#include <sstream>
#include <map>
#include <windowsx.h>
#include <objbase.h>
#include <shellapi.h>
#include <commctrl.h>
#include <commdlg.h>

#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(lib, "comctl32")

#define SUPPORT_SITE TEXT( "http://www.mrz-net.org/" )
#define MAX_LOADSTRING 100
#define BORDER_WIDTH 2
#define BORDER_HIT_WIDTH ( BORDER_WIDTH * 4 )
#define BORDER_LIMIT 64

HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];

svn::Dump g_SVNDump;

enum ActiveBorderType
{ None, Horizon, Vertical };

ATOM MyRegisterClass( HINSTANCE hInstance );
BOOL InitInstance( HINSTANCE, int );
LRESULT CALLBACK WndProc( HWND, UINT, WPARAM, LPARAM );
int OnCreate( HWND hWnd );
void OnSize( WORD width, WORD height, double h_border_pos, double v_border_pos );
LRESULT OnNotify( HWND hWnd, const LPNMHDR hdr, LPARAM lParam );
void OnDropFiles( HWND hWnd, HDROP drop );
INT_PTR CALLBACK About( HWND, UINT, WPARAM, LPARAM );

int APIENTRY wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
	UNREFERENCED_PARAMETER( hPrevInstance );
	UNREFERENCED_PARAMETER( lpCmdLine );

	HeapSetInformation( NULL, HeapEnableTerminationOnCorruption, NULL, 0 );
	if( FAILED( CoInitializeEx( NULL, COINIT_APARTMENTTHREADED ) ) )
		return 0;

	LoadStringW( hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING );
	LoadStringW( hInstance, IDC_DUMPVIEWER, szWindowClass, MAX_LOADSTRING );
	MyRegisterClass( hInstance );

	auto icex = INITCOMMONCONTROLSEX{sizeof( INITCOMMONCONTROLSEX ), ICC_LISTVIEW_CLASSES};
	InitCommonControlsEx( &icex );

	if( !InitInstance( hInstance, nCmdShow ) )
		return 0;

	auto hAccelTable = LoadAccelerators( hInstance, MAKEINTRESOURCE( IDC_DUMPVIEWER ) );
	auto msg = MSG();

	while( GetMessage( &msg, nullptr, 0, 0 ) )
		if( !TranslateAccelerator( msg.hwnd, hAccelTable, &msg ) )
		{
			TranslateMessage( &msg );
			DispatchMessage( &msg );
		}

	CoUninitialize();
	return (int )msg.wParam;
}

ATOM MyRegisterClass( HINSTANCE hInstance )
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof( WNDCLASSEX );

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = wcex.hIconSm = LoadIcon( hInstance, MAKEINTRESOURCE( IDI_DUMPVIEWER ) );
	wcex.hCursor = LoadCursor( nullptr, IDC_ARROW );
	wcex.hbrBackground = HBRUSH( GetStockObject( LTGRAY_BRUSH ) );
	wcex.lpszMenuName = MAKEINTRESOURCEW( IDC_DUMPVIEWER );
	wcex.lpszClassName = szWindowClass;

	return RegisterClassExW( &wcex );
}

BOOL InitInstance( HINSTANCE hInstance, int nCmdShow )
{
	hInst = hInstance;

	SetThreadDpiAwarenessContext( DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED );

	auto hWnd = CreateWindowEx( WS_EX_ACCEPTFILES, szWindowClass, szTitle, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr );
	if( !hWnd )
		return FALSE;

	ShowWindow( hWnd, nCmdShow );
	UpdateWindow( hWnd );

	return TRUE;
}

LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	static auto default_cursor = HCURSOR( 0 );
	static auto ns_cursor = LoadCursor( 0, (LPCTSTR )( IDC_SIZENS ) );
	static auto we_cursor = LoadCursor( 0, (LPCTSTR )( IDC_SIZEWE ) );
	static auto window_width = 0;
	static auto window_height = 0;
	static auto h_border_pos = .5;
	static auto v_border_pos = .5;
	static auto active_border_type = ActiveBorderType::None;
	static auto capture_bar = RECT{0,0,0,0};
	static auto capture_base_pos = POINT{0,0};

	switch( message )
	{
	case WM_CREATE:
		return OnCreate( hWnd );

	case WM_DESTROY:
		PostQuitMessage( 0 );
		break;

	case WM_SIZE:
		window_width = LOWORD( lParam );
		window_height = HIWORD( lParam );
		OnSize( window_width, window_height, h_border_pos, v_border_pos );
		break;

	case WM_KILLFOCUS:
		default_cursor = 0;
		active_border_type = ActiveBorderType::None;
		break;

	case WM_SETCURSOR:
		if( !DefWindowProc( hWnd, message, wParam, lParam ) )
		{
			auto pt = POINT{};
			GetCursorPos( &pt );
			ScreenToClient( hWnd, &pt );

			auto bar_x = int( window_width * v_border_pos );
			auto bar_y = int( window_height * h_border_pos );
			if( pt.y >= bar_y - BORDER_HIT_WIDTH && pt.y <= bar_y + BORDER_HIT_WIDTH )
				default_cursor = SetCursor( ns_cursor );
			else if( pt.y >= bar_y && pt.x >= bar_x - BORDER_HIT_WIDTH && pt.x <= bar_x + BORDER_HIT_WIDTH )
				default_cursor = SetCursor( we_cursor );
			else
			{
				default_cursor = 0;
				return FALSE;
			}
		}
		return TRUE;

	case WM_NOTIFY:
		return OnNotify( hWnd, LPNMHDR( lParam ), lParam );

	case WM_COMMAND:
		switch( LOWORD( wParam ) )
		{
		case ID_CHARSET_UTF8: DetailWindow::SetCharset( hWnd, DetailWindow::CharsetType::UTF8 ); break;
		case ID_CHARSET_UNICODE: DetailWindow::SetCharset( hWnd, DetailWindow::CharsetType::Unicode ); break;
		case ID_CHARSET_SHIFTJIS: DetailWindow::SetCharset( hWnd, DetailWindow::CharsetType::ShiftJIS ); break;
		case ID_TABSTOP_2: DetailWindow::SetTabstop( hWnd, 2 ); break;
		case ID_TABSTOP_4: DetailWindow::SetTabstop( hWnd, 4 ); break;
		case ID_TABSTOP_8: DetailWindow::SetTabstop( hWnd, 8 ); break;

		case IDM_ABOUT:
			DialogBox( hInst, MAKEINTRESOURCE( IDD_ABOUTBOX ), hWnd, About );
			break;
		case IDM_EXIT:
			DestroyWindow( hWnd );
			break;
		default:
			return DefWindowProc( hWnd, message, wParam, lParam );
		}
		break;

	case WM_MOUSEMOVE:
		if( active_border_type != ActiveBorderType::None )
		{
			auto x = GET_X_LPARAM( lParam );
			auto y = GET_Y_LPARAM( lParam );

			if( x < BORDER_LIMIT || x >= window_width - BORDER_LIMIT || y < BORDER_LIMIT || y >= window_height - BORDER_LIMIT )
				break;

			InvertRect( GetDC( hWnd ), &capture_bar );
			switch( active_border_type )
			{
			case ActiveBorderType::Horizon:
				{
					auto bar_y = int( window_height * h_border_pos );
					auto height = bar_y + ( y - capture_base_pos.y );
					capture_bar.top = height - BORDER_WIDTH;
					capture_bar.bottom = height + BORDER_WIDTH;
				}
				break;
			case ActiveBorderType::Vertical:
				{
					auto bar_x = int( window_width * v_border_pos );
					auto width = bar_x + ( x - capture_base_pos.x );
					capture_bar.left = width - BORDER_WIDTH;
					capture_bar.right = width + BORDER_WIDTH;
				}
				break;
			}
			InvertRect( GetDC( hWnd ), &capture_bar );
		}
		break;

	case WM_LBUTTONDOWN:
		if( default_cursor )
		{
			auto x = GET_X_LPARAM( lParam );
			auto y = GET_Y_LPARAM( lParam );
			auto bar_x = int( window_width * v_border_pos );
			auto bar_y = int( window_height * h_border_pos );
			if( y >= bar_y - BORDER_HIT_WIDTH && y <= bar_y + BORDER_HIT_WIDTH )
			{
				active_border_type = ActiveBorderType::Horizon;
				capture_bar = RECT{0, bar_y - BORDER_WIDTH, window_width, bar_y + BORDER_WIDTH};
			}
			else if( y >= bar_y && x >= bar_x - BORDER_HIT_WIDTH && x <= bar_x + BORDER_HIT_WIDTH )
			{
				active_border_type = ActiveBorderType::Vertical;
				capture_bar = RECT{bar_x - BORDER_WIDTH, bar_y, bar_x + BORDER_WIDTH, window_height};
			}
			SetWindowLongPtr( hWnd, GWL_STYLE, GetWindowLongPtr( hWnd, GWL_STYLE ) & ~WS_CLIPCHILDREN );
			InvertRect( GetDC( hWnd ), &capture_bar );
			capture_base_pos.x = x;
			capture_base_pos.y = y;
			SetCapture( hWnd );
		}
		break;

	case WM_LBUTTONUP:
		if( active_border_type != ActiveBorderType::None )
		{
			auto x = min( max( GET_X_LPARAM( lParam ), BORDER_LIMIT ), window_width - BORDER_LIMIT );
			auto y = min( max( GET_Y_LPARAM( lParam ), BORDER_LIMIT ), window_height - BORDER_LIMIT );

			switch( active_border_type )
			{
			case ActiveBorderType::Horizon:  h_border_pos += double( y - capture_base_pos.y ) / window_height; break;
			case ActiveBorderType::Vertical: v_border_pos += double( x - capture_base_pos.x ) / window_width;  break;
			}
			SetWindowLongPtr( hWnd, GWL_STYLE, GetWindowLongPtr( hWnd, GWL_STYLE ) | WS_CLIPCHILDREN );
			OnSize( window_width, window_height, h_border_pos, v_border_pos );
			InvalidateRect( hWnd, nullptr, TRUE );
			default_cursor = 0;
			active_border_type = ActiveBorderType::None;
			ReleaseCapture();
		}
		break;

	case WM_DROPFILES:
		OnDropFiles( hWnd, HDROP( wParam ) );
		break;

	default:
		return DefWindowProc( hWnd, message, wParam, lParam );
	}
	return 0;
}

int OnCreate( HWND hWnd )
{
	if( !RevisionList::Create( hWnd ) ||
		!NodeList::Create( hWnd ) ||
		!DetailWindow::Create( hWnd ) )
		return -1;

	return 0;
}

void OnSize( WORD width, WORD height, double h_border_pos, double v_border_pos )
{
	auto border = int( height * h_border_pos );
	auto separate = int( width * v_border_pos );
	RevisionList::Resize( RECT{0, 0, width, border - BORDER_WIDTH} );
	NodeList::Resize( RECT{0, border + BORDER_WIDTH, separate - BORDER_WIDTH, height - border} );
	DetailWindow::Resize( RECT{separate + BORDER_WIDTH, border + BORDER_WIDTH, width - separate - BORDER_WIDTH, height - border - BORDER_WIDTH} );
}

LRESULT OnNotify( HWND hWnd, const LPNMHDR hdr, LPARAM lParam )
{
	switch( hdr->idFrom )
	{
	case IDC_REVISION_LIST:
		return RevisionList::OnNotify( hWnd, hdr->code, lParam );
	case IDC_CONTENTS_LIST:
		return NodeList::OnNotify( hWnd, hdr->code, lParam );
	}
	return 0;
}

void OnDropFiles( HWND hWnd, HDROP drop )
{
	try
	{
		TCHAR title[MAX_PATH * 2];

		auto files = DragQueryFile( drop, -1, nullptr, -1 );
		if( files > 1 )
			files = 1;
		for( auto i = decltype( files )( 0 ); i < files; ++i )
		{
			char path[MAX_PATH];
			if( DragQueryFileA( drop, i, path, MAX_PATH ) )
			{
				wsprintf( title, L"%s - %S[読み込み中...]", szTitle, path );
				SetWindowText( hWnd, title );
				if( g_SVNDump.Init( path ) )
				{
					wsprintf( title, L"%s - %S [format:%d, max rev:%d]", szTitle, path, g_SVNDump.GetFormatVersion(), g_SVNDump.GetMaxRevision() );
					SetWindowText( hWnd, title );
				}
			}
		}
		DragFinish( drop );

		if( !g_SVNDump )
			return;
	}
	catch( const std::exception &e )
	{
		MessageBoxA( hWnd, e.what(), nullptr, MB_OK );
		return;
	}

	RevisionList::Clear();
	NodeList::Clear();
	DetailWindow::Clear();
	RevisionList::Build( g_SVNDump );
}

INT_PTR CALLBACK About( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
	UNREFERENCED_PARAMETER( lParam );
	switch( message )
	{
	case WM_INITDIALOG:
		SetDlgItemText( hDlg, IDC_WEBSITE, SUPPORT_SITE );
		return (INT_PTR )TRUE;

	case WM_COMMAND:
		if( LOWORD( wParam ) == IDC_WEBSITE )
			ShellExecute( hDlg, TEXT( "open" ), SUPPORT_SITE, NULL, NULL, SW_SHOWNORMAL );
		else if( LOWORD( wParam ) == IDOK || LOWORD( wParam ) == IDCANCEL )
		{
			EndDialog( hDlg, LOWORD( wParam ) );
			return (INT_PTR )TRUE;
		}
		break;
	}
	return (INT_PTR )FALSE;
}
