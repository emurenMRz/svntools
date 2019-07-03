#include "framework.h"
#include "dumpviewer.h"
#include "ListView.h"
#include "SVNDump.h"
#include "WICManager.h"
#include <iomanip>
#include <sstream>
#include <map>
#include <filesystem>
#include <shellapi.h>
#include <commctrl.h>
#include <commdlg.h>

#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(lib, "comctl32")

#define MAX_LOADSTRING 100
#define BORDER_WIDTH 2
#define BORDER_HIT_WIDTH ( BORDER_WIDTH * 4 )
#define BORDER_LIMIT 64

HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];

svn::Dump g_SVNDump;
ListView g_RevisionList;
ListView g_ContentsList;
HWND g_DetailWindow;
HWND g_BitmapWindow;
HFONT g_DetailFont;
std::map<std::string, int> g_GroupNumber;
int g_GroupNumberEtc;

enum ActiveBorderType
{ None, Horizon, Vertical };

enum DetailCharsetType
{ UTF8, Unicode, ShiftJIS };

DetailCharsetType g_DetailCharsetType;
WICManager g_WICManager;
WICManager::buffer_t g_ImageBuffer;
svn::text_data_t g_DetailText;

ATOM MyRegisterClass( HINSTANCE hInstance );
BOOL InitInstance( HINSTANCE, int );
LRESULT CALLBACK WndProc( HWND, UINT, WPARAM, LPARAM );
void OnSize( WORD width, WORD height, double h_border_pos, double v_border_pos );
int OnCreate( HWND hWnd );
LRESULT OnNotify( HWND hWnd, const LPNMHDR hdr, const LPNMLISTVIEW lv );
void OnDropFiles( HWND hWnd, HDROP drop );
std::wstring toWide( const void *str, size_t length );
std::wstring toWide( const std::string &str );
std::wstring SJIStoWide( const void *str, size_t length );
std::wstring SearchProp( const svn::prop_data_t &props, const std::string name );
void SetDetailBinary( const void *data, size_t size );
void SetDetailText( std::wstring text );
void SetDetailCharset( HWND hWnd, DetailCharsetType type );
void SetDetailTabstop( HWND hWnd, int tabstop );
void SaveToFile( HWND hWnd, const svn::Node *node );
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
	wcex.hbrBackground = HBRUSH( GetStockObject( GRAY_BRUSH ) );
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
		SendMessage( g_DetailWindow, WM_SETFONT, 0, MAKELPARAM( TRUE, 0 ) );
		if( g_DetailFont )
			DeleteObject( g_DetailFont );
		g_WICManager.Release();
		PostQuitMessage( 0 );
		break;

	case WM_SIZE:
		window_width = LOWORD( lParam );
		window_height = HIWORD( lParam );
		OnSize( window_width, window_height, h_border_pos, v_border_pos );
		break;

	case WM_DRAWITEM:
		{
			auto dis = LPDRAWITEMSTRUCT( lParam );
			if( dis->CtlID == IDC_CONTENTS_BITMAP && !g_ImageBuffer.empty() )
				if( dis->itemAction & ODA_DRAWENTIRE )
				{
					auto info = LPBITMAPINFOHEADER( g_ImageBuffer.data() );
					auto bits = g_ImageBuffer.data() + sizeof( BITMAPINFOHEADER );
					const auto sw = info->biWidth;
					const auto sh = std::abs( info->biHeight );
					const auto aspect = double( sw ) / sh;
					const auto dw = dis->rcItem.right - dis->rcItem.left;
					const auto dh = dis->rcItem.bottom - dis->rcItem.top;
					auto w = dw;
					auto h = dh;
					if( aspect >= 1.0 )
					{
						auto wh = w / aspect + .5;
						if( wh <= h )
							h = decltype( h )( wh );
						else
							w = decltype( w )( h * aspect + .5 );
					}
					else
					{
						auto ww = h * aspect + .5;
						if( ww <= w )
							w = decltype( w )( ww );
						else
							h = decltype( h )( w / aspect + .5 );
					}
					FillRect( dis->hDC, &dis->rcItem, (HBRUSH )GetStockObject( BLACK_BRUSH ) );
					SetStretchBltMode( dis->hDC, HALFTONE );
					StretchDIBits( dis->hDC, ( dw - w ) / 2, ( dh - h ) / 2, w, h, 0, 0, sw, sh, bits, reinterpret_cast< const LPBITMAPINFO >( info ), DIB_RGB_COLORS, SRCCOPY );
					return TRUE;
				}
		}
		break;

	case WM_NOTIFY:
		return OnNotify( hWnd, LPNMHDR( lParam ), LPNMLISTVIEW( lParam ) );

	case WM_COMMAND:
		switch( LOWORD( wParam ) )
		{
		case ID_CHARSET_UTF8: SetDetailCharset( hWnd, DetailCharsetType::UTF8 ); break;
		case ID_CHARSET_UNICODE: SetDetailCharset( hWnd, DetailCharsetType::Unicode ); break;
		case ID_CHARSET_SHIFTJIS: SetDetailCharset( hWnd, DetailCharsetType::ShiftJIS ); break;
		case ID_TABSTOP_2: SetDetailTabstop( hWnd, 2 ); break;
		case ID_TABSTOP_4: SetDetailTabstop( hWnd, 4 ); break;
		case ID_TABSTOP_8: SetDetailTabstop( hWnd, 8 ); break;

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
		if( active_border_type == ActiveBorderType::None )
		{
			auto x = LOWORD( lParam );
			auto y = HIWORD( lParam );
			auto bar_x = int( window_width * v_border_pos );
			auto bar_y = int( window_height * h_border_pos );
			if( y >= bar_y - BORDER_HIT_WIDTH && y <= bar_y + BORDER_HIT_WIDTH )
				default_cursor = SetCursor( ns_cursor );
			else if( y >= bar_y && x >= bar_x - BORDER_HIT_WIDTH && x <= bar_x + BORDER_HIT_WIDTH )
				default_cursor = SetCursor( we_cursor );
			else if( default_cursor )
			{
				SetCursor( default_cursor );
				default_cursor = 0;
			}
		}
		else
		{
			auto x = LOWORD( lParam );
			auto y = HIWORD( lParam );

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
			auto x = LOWORD( lParam );
			auto y = HIWORD( lParam );
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
			auto x = min( max( LOWORD( lParam ), BORDER_LIMIT ), window_width - BORDER_LIMIT );
			auto y = min( max( HIWORD( lParam ), BORDER_LIMIT ), window_height - BORDER_LIMIT );

			switch( active_border_type )
			{
			case ActiveBorderType::Horizon:  h_border_pos += double( y - capture_base_pos.y ) / window_height; break;
			case ActiveBorderType::Vertical: v_border_pos += double( x - capture_base_pos.x ) / window_width;  break;
			}
			SetWindowLongPtr( hWnd, GWL_STYLE, GetWindowLongPtr( hWnd, GWL_STYLE ) | WS_CLIPCHILDREN );
			OnSize( window_width, window_height, h_border_pos, v_border_pos );
			InvalidateRect( hWnd, nullptr, TRUE );
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
	if( !g_WICManager.Create() )
		return -1;

	if( !g_RevisionList.Create( hWnd, IDC_REVISION_LIST ) ||
		!g_ContentsList.Create( hWnd, IDC_CONTENTS_LIST ) )
		return -1;

	g_DetailWindow = CreateWindowEx( 0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_LEFT | ES_MULTILINE | ES_READONLY, 0, 0, 0, 0, hWnd, (HMENU )IDC_CONTENTS_DETAIL, hInst, 0 );
	if( !g_DetailWindow )
		return -1;

	g_BitmapWindow = CreateWindowEx( 0, L"STATIC", L"", WS_CHILD | SS_OWNERDRAW, 0, 0, 0, 0, hWnd, (HMENU )IDC_CONTENTS_BITMAP, hInst, 0 );
	if( !g_BitmapWindow )
		return -1;

	g_DetailFont = CreateFont( 16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_NATURAL_QUALITY, FIXED_PITCH, L"Terminal" );
	if( g_DetailFont )
		SendMessage( g_DetailWindow, WM_SETFONT, (WPARAM )g_DetailFont, MAKELPARAM( TRUE, 0 ) );

	g_RevisionList.AddColumn( TEXT( "Revision" ), 80 );
	g_RevisionList.AddColumn( TEXT( "Actions" ), 160 );
	g_RevisionList.AddColumn( TEXT( "Author" ), 100 );
	g_RevisionList.AddColumn( TEXT( "Date(GMT)" ), 150 );
	g_RevisionList.AddColumn( TEXT( "Message" ), 1000 );

	g_ContentsList.AddColumn( TEXT( "Action" ), 50 );
	g_ContentsList.AddColumn( TEXT( "Kind" ), 50 );
	g_ContentsList.AddColumn( TEXT( "Path" ), 300 );
	g_ContentsList.AddColumn( TEXT( "Copyfrom" ), 400 );
	g_ContentsList.AddColumn( TEXT( "Delta" ), 200 );
	g_ContentsList.EnableGroupView( true );

	g_GroupNumber["add"] = g_ContentsList.AddGroup( L"add", L"" );
	g_GroupNumber["change"] = g_ContentsList.AddGroup( L"change", L"" );
	g_GroupNumber["delete"] = g_ContentsList.AddGroup( L"delete", L"" );
	g_GroupNumber["replace"] = g_ContentsList.AddGroup( L"replace", L"" );
	g_GroupNumberEtc = g_ContentsList.AddGroup( L"etc", L"" );

	SetDetailCharset( hWnd, DetailCharsetType::UTF8 );
	SetDetailTabstop( hWnd, 4 );

	return 0;
}

void OnSize( WORD width, WORD height, double h_border_pos, double v_border_pos )
{
	auto border = int( height * h_border_pos );
	auto separate = int( width * v_border_pos );
	g_RevisionList.Resize( 0, 0, width, border - BORDER_WIDTH );
	g_ContentsList.Resize( 0, border + BORDER_WIDTH, separate - BORDER_WIDTH, height - border );
	SetWindowPos( g_DetailWindow, 0, separate + BORDER_WIDTH, border + BORDER_WIDTH, width - separate - BORDER_WIDTH, height - border - BORDER_WIDTH, SWP_NOZORDER );
	SetWindowPos( g_BitmapWindow, 0, separate + BORDER_WIDTH, border + BORDER_WIDTH, width - separate - BORDER_WIDTH, height - border - BORDER_WIDTH, SWP_NOCOPYBITS | SWP_NOZORDER );
}

LRESULT OnNotify( HWND hWnd, const LPNMHDR hdr, const LPNMLISTVIEW lv )
{
	switch( hdr->idFrom )
	{
	case IDC_REVISION_LIST:
		switch( hdr->code )
		{
		case LVN_ITEMCHANGED:
			if( lv->uNewState & LVIS_SELECTED )
				if( lv->iItem >= 0 )
				{
					const auto &r = g_SVNDump[int( lv->lParam )];
					SetDetailText( SearchProp( r.prop, "svn:log" ) );

					g_ContentsList.Clear();
					g_ContentsList.SetRedraw( false );
					g_ContentsList.EnableGroupView( false );
					for( const auto &n : r.nodes )
						if( !n.NodeAction.empty() )
						{
							auto group = g_GroupNumberEtc;
							auto find = g_GroupNumber.find( n.NodeAction );
							if( find != g_GroupNumber.end() )
								group = ( *find ).second;

							auto item_no = g_ContentsList.AddItem( toWide( n.NodeAction ).c_str(), reinterpret_cast< LPARAM >( &n ), group );
							if( item_no < 0 )
								continue;
							g_ContentsList.SetSubItem( item_no, 1, toWide( n.NodeKind ).c_str() );
							g_ContentsList.SetSubItem( item_no, 2, toWide( n.NodePath ).c_str() );

							if( n.NodeCopyfromRev > 0 )
							{
								auto ss = std::wstringstream();
								ss << toWide( n.NodeCopyfromPath ) << L", rev: " << n.NodeCopyfromRev;
								g_ContentsList.SetSubItem( item_no, 3, ss.str().c_str() );
							}

							if( n.PropDelta || n.TextDelta )
							{
								auto ss = std::wstringstream();
								if( n.PropDelta )
									ss << L"prop";
								if( n.TextDelta )
								{
									if( n.PropDelta )
										ss << L", ";
									ss << L"text";
								}
								g_ContentsList.SetSubItem( item_no, 4, ss.str().c_str() );
							}
						}
					g_ContentsList.SetRedraw( true );
					g_ContentsList.EnableGroupView( true );
				}
			break;

			//case LVN_GETDISPINFO:
			//	{
			//		auto lpDispInfo = (NMLVDISPINFO * )lv;
			//		if( lpDispInfo->item.mask & LVIF_TEXT )
			//		{
			//			auto item_no = lpDispInfo->item.iItem;
			//			auto subitem_no = lpDispInfo->item.iSubItem;
			//			TCHAR szBuf[256];
			//			wsprintf( szBuf, TEXT( "%c" ), L's' );
			//			lstrcpy( lpDispInfo->item.pszText, szBuf );
			//		}
			//	}
			//	break;
		}
		break;

	case IDC_CONTENTS_LIST:
		switch( hdr->code )
		{
		case LVN_ITEMCHANGED:
			if( lv->uNewState & LVIS_SELECTED )
			{
				const auto node = reinterpret_cast< const svn::Node * >( lv->lParam );
				if( !node->text.empty() && node->NodeKind == "file" )
				{
					auto mime_type = SearchProp( node->prop, "svn:mime-type" );
					try
					{
						g_ImageBuffer = g_WICManager.Load( node->text.data(), node->text.size() );
						ShowWindow( g_DetailWindow, SW_HIDE );
						ShowWindow( g_BitmapWindow, SW_SHOWNORMAL );
						InvalidateRect( g_BitmapWindow, NULL, TRUE );
					}
					catch( const std::exception & )
					{
						g_DetailText.clear();
						if( mime_type.empty() )
						{
							g_DetailText = node->text;
							switch( g_DetailCharsetType )
							{
							case DetailCharsetType::UTF8: SetDetailText( toWide( node->text.data(), node->text.size() ) ); break;
							case DetailCharsetType::Unicode: SetDetailText( std::wstring( node->text.cbegin(), node->text.cend() ) ); break;
							case DetailCharsetType::ShiftJIS: SetDetailText( SJIStoWide( node->text.data(), node->text.size() ) ); break;
							}
						}
						else if( mime_type == L"application/octet-stream" )
							SetDetailBinary( node->text.data(), node->text.size() );
						else
							SetDetailText( L"" );
						ShowWindow( g_DetailWindow, SW_SHOWNORMAL );
						ShowWindow( g_BitmapWindow, SW_HIDE );
					}
				}
				else
				{
					ShowWindow( g_DetailWindow, SW_SHOWNORMAL );
					ShowWindow( g_BitmapWindow, SW_HIDE );
					SetDetailText( L"" );
				}
			}
			break;

		case NM_DBLCLK:
			{
				auto lpia = reinterpret_cast< LPNMITEMACTIVATE >( lv );
				if( lpia->iItem < 0 )
					break;
				const auto node = reinterpret_cast< const svn::Node * >( g_ContentsList.GetItemData( lpia->iItem ) );
				if( node && node->NodeKind == "file" )
					SaveToFile( hWnd, node );
			}
			break;

		case NM_CUSTOMDRAW:
			{
				auto lplvcd = reinterpret_cast< LPNMLVCUSTOMDRAW >( lv );
				switch( lplvcd->nmcd.dwDrawStage )
				{
				case CDDS_PREPAINT:
					return CDRF_NOTIFYITEMDRAW;
				case CDDS_ITEMPREPAINT:
					{
						const auto c = reinterpret_cast< const svn::Node * >( lplvcd->nmcd.lItemlParam );
						lplvcd->clrTextBk = c->NodeAction == "add" ? RGB( 0xf8, 0xff, 0xf8 ) : c->NodeAction == "change" ? RGB( 0xf8, 0xf8, 0xff ) : c->NodeAction == "replace" ? RGB( 0xff, 0xf8, 0xf8 ) : 0xffffff;
						lplvcd->clrText = c->NodeAction == "delete" ? 0x808080 : 0;
					}
					return CDRF_DODEFAULT;
				}
			}
			return CDRF_DODEFAULT;
		}
		break;
	}
	return 0;
}

void OnDropFiles( HWND hWnd, HDROP drop )
{
	try
	{
		auto files = DragQueryFile( drop, -1, nullptr, -1 );
		if( files > 1 )
			files = 1;
		for( auto i = decltype( files )( 0 ); i < files; ++i )
		{
			char path[MAX_PATH];
			if( DragQueryFileA( drop, i, path, MAX_PATH ) )
				if( g_SVNDump.Init( path ) )
				{
					TCHAR title[MAX_PATH * 2];
					wsprintf( title, L"%s - %S [format:%d, max rev:%d]", szTitle, path, g_SVNDump.GetFormatVersion(), g_SVNDump.GetMaxRevision() );
					SetWindowText( hWnd, title );
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

	g_RevisionList.Clear();
	g_ContentsList.Clear();
	SetDetailText( L"" );

	g_RevisionList.SetRedraw( false );
	for( const auto &r : g_SVNDump )
	{
		TCHAR name[16];
		wsprintf( name, L"%d", r.number );
		auto item_no = g_RevisionList.AddItem( name, r.number );
		if( item_no < 0 )
			continue;

		if( !r.nodes.empty() )
		{
			auto actmap = std::map< std::string, int >();
			for( const auto &n : r.nodes )
				++actmap[n.NodeAction];

			auto actions = std::stringstream();
			for( const auto &act : actmap )
			{
				if( !actions.str().empty() )
					actions << ", ";
				actions << act.first << ": " << act.second;
			}
			g_RevisionList.SetSubItem( item_no, 1, toWide( actions.str() ).c_str() );
		}

		auto author = SearchProp( r.prop, "svn:author" );
		auto date = SearchProp( r.prop, "svn:date" );
		auto log = SearchProp( r.prop, "svn:log" );

		date.replace( date.find_first_of( L'T' ), 1, L" " );
		date.erase( date.find_last_of( L'.' ) );

		g_RevisionList.SetSubItem( item_no, 2, author.c_str() );
		g_RevisionList.SetSubItem( item_no, 3, date.c_str() );
		g_RevisionList.SetSubItem( item_no, 4, log.c_str() );
	}
	g_RevisionList.SetRedraw( true );
}

std::wstring toWide( const void *str, size_t length )
{
	auto w = std::vector<TCHAR>( MultiByteToWideChar( CP_UTF8, 0, LPCSTR( str ), int( length ), nullptr, 0 ) );
	MultiByteToWideChar( CP_UTF8, 0, LPCSTR( str ), int( length ), w.data(), int( w.size() ) );
	return std::wstring( w.cbegin(), w.cend() );
}

std::wstring toWide( const std::string &str )
{ return toWide( str.c_str(), str.size() ); }

std::wstring SJIStoWide( const void *str, size_t length )
{
	auto w = std::vector<TCHAR>( MultiByteToWideChar( CP_ACP, 0, LPCSTR( str ), int( length ), nullptr, 0 ) );
	MultiByteToWideChar( CP_ACP, 0, LPCSTR( str ), int( length ), w.data(), int( w.size() ) );
	return std::wstring( w.cbegin(), w.cend() );
}

std::wstring SearchProp( const svn::prop_data_t &props, const std::string name )
{
	auto find = props.find( std::vector< uint8_t >( name.cbegin(), name.cend() ) );
	if( find == props.end() )
		return std::wstring();
	return toWide( ( *find ).second.data(), ( *find ).second.size() );
}

void SetDetailBinary( const void *data, size_t size )
{
	TCHAR ascii[17] = {L'\0'};

	auto binary = std::wstringstream();
	auto byte_count = 0;
	auto xchs = L"0123456789ABCDEF";
	auto src = LPBYTE( data );

	binary << L"                 | " << std::setfill( L'0' ) << std::hex;
	for( auto i = 0; i < 16; ++i )
		binary << std::setw( 2 ) << i << L' ';
	binary << L"| Ascii\r\n";
	binary << L"-----------------+-------------------------------------------------+-----------------\r\n";
	for( auto i = decltype( size )( 0 ); i < size; ++i )
	{
		if( !byte_count )
			binary << std::setw( 16 ) << i << L" | ";

		auto byte = src[i];
		ascii[byte_count] = isprint( byte ) ? static_cast< TCHAR >( byte ) : L'.';
		binary << xchs[byte >> 4] << xchs[byte & 0xf] << L' ';
		if( ++byte_count >= 16 )
		{
			binary << L"| " << ascii << L"\r\n";
			byte_count = 0;
		}
	}

	SendMessage( g_DetailWindow, WM_SETTEXT, 0, reinterpret_cast< LPARAM >( binary.str().c_str() ) );
}

void SetDetailText( std::wstring text )
{
	auto pos = text.find_first_of( L"\r\n" );
	while( pos != std::wstring::npos )
	{
		if( text[pos] == L'\r' && text[pos + 1] != L'\n' )
			text.insert( pos + 1, L"\n" );
		else if( text[pos] == L'\n' )
			text.insert( pos, L"\r" );
		pos = text.find_first_of( L"\r\n", pos + 2 );
	}
	SendMessage( g_DetailWindow, WM_SETTEXT, 0, reinterpret_cast< LPARAM >( text.c_str() ) );
}

void SetDetailCharset( HWND hWnd, DetailCharsetType type )
{
	auto menu = GetMenu( hWnd );
	CheckMenuItem( menu, ID_CHARSET_UTF8, type == DetailCharsetType::UTF8 ? MF_CHECKED : MF_UNCHECKED );
	CheckMenuItem( menu, ID_CHARSET_UNICODE, type == DetailCharsetType::Unicode ? MF_CHECKED : MF_UNCHECKED );
	CheckMenuItem( menu, ID_CHARSET_SHIFTJIS, type == DetailCharsetType::ShiftJIS ? MF_CHECKED : MF_UNCHECKED );
	g_DetailCharsetType = type;

	if( !g_DetailText.empty() )
		switch( type )
		{
		case DetailCharsetType::UTF8: SetDetailText( toWide( g_DetailText.data(), g_DetailText.size() ) ); break;
		case DetailCharsetType::Unicode: SetDetailText( std::wstring( g_DetailText.cbegin(), g_DetailText.cend() ) ); break;
		case DetailCharsetType::ShiftJIS: SetDetailText( SJIStoWide( g_DetailText.data(), g_DetailText.size() ) ); break;
		}
}

void SetDetailTabstop( HWND hWnd, int tabstop )
{
	auto menu = GetMenu( hWnd );
	CheckMenuItem( menu, ID_TABSTOP_2, tabstop == 2 ? MF_CHECKED : MF_UNCHECKED );
	CheckMenuItem( menu, ID_TABSTOP_4, tabstop == 4 ? MF_CHECKED : MF_UNCHECKED );
	CheckMenuItem( menu, ID_TABSTOP_8, tabstop == 8 ? MF_CHECKED : MF_UNCHECKED );

	tabstop *= 4;
	SendMessage( g_DetailWindow, EM_SETTABSTOPS, 1, LPARAM( &tabstop ) );
	InvalidateRect( g_DetailWindow, nullptr, FALSE );
}

void SaveToFile( HWND hWnd, const svn::Node *node )
{
	namespace fs = std::experimental::filesystem;

	TCHAR fullpath[MAX_PATH];
	lstrcpy( fullpath, fs::path( node->NodePath ).filename().generic_wstring().c_str() );

	TCHAR initpath[MAX_PATH];
	lstrcpy( initpath, fs::current_path().native().c_str() );

	auto ofn = OPENFILENAME{0};
	ofn.lStructSize = sizeof( OPENFILENAME );
	ofn.hwndOwner = hWnd;
	ofn.lpstrFilter = L"All files\0*.*\0\0";
	ofn.lpstrFile = fullpath;
	ofn.nMaxFile = sizeof( fullpath );
	ofn.lpstrInitialDir = initpath;
	ofn.Flags = OFN_OVERWRITEPROMPT;
	if( !::GetSaveFileName( &ofn ) )
		return;

	auto out = std::ofstream( fullpath, std::ios_base::binary );
	if( out )
		out.write( reinterpret_cast< const char * >( node->text.data() ), node->text.size() );
}

INT_PTR CALLBACK About( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
	UNREFERENCED_PARAMETER( lParam );
	switch( message )
	{
	case WM_INITDIALOG:
		return (INT_PTR )TRUE;

	case WM_COMMAND:
		if( LOWORD( wParam ) == IDOK || LOWORD( wParam ) == IDCANCEL )
		{
			EndDialog( hDlg, LOWORD( wParam ) );
			return (INT_PTR )TRUE;
		}
		break;
	}
	return (INT_PTR )FALSE;
}
