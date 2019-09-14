#include "DetailWindow.h"
#include "dumpviewer.h"
#include "WICManager.h"
#include "misc.h"
#include <iomanip>
#include <sstream>

namespace DetailWindow
{
	enum class DisplayMode
	{ Text, Binary, Image };
	DisplayMode g_DisplayMode;

	HWND g_DetailWindow;
	HFONT g_DetailFont;

	CharsetType g_CharsetType;
	WICManager g_WICManager;
	WICManager::buffer_t g_ImageBuffer;
	std::vector<std::wstring> g_TextBuffer;
	svn::text_data_t g_ContentBuffer;
	int g_Tabstop;
	int g_TextHeight;
	int g_ColumnPerPage;
	int g_LinePerPage;

	LRESULT ScrollHorizon( HWND hWnd, WORD type )
	{
		auto si = SCROLLINFO{sizeof( SCROLLINFO ), SIF_POS | SIF_RANGE};
		GetScrollInfo( hWnd, SB_HORZ, &si );
		auto pos = si.nPos;

		switch( type )
		{
		case SB_LINELEFT:
			if( --pos < 0 )
				pos = 0;
			break;

		case SB_LINERIGHT:
			if( ++pos >= si.nMax )
				pos = si.nMax - 1;
			break;

		case SB_PAGELEFT:
			if( ( pos -= g_ColumnPerPage ) < 0 )
				pos = 0;
			break;

		case SB_PAGERIGHT:
			if( ( pos += g_ColumnPerPage ) >= si.nMax )
				pos = si.nMax - 1;
			break;

		case SB_THUMBTRACK:
			{
				auto si = SCROLLINFO{sizeof( SCROLLINFO ), SIF_TRACKPOS};
				GetScrollInfo( hWnd, SB_HORZ, &si );
				pos = si.nTrackPos;
			}
			break;
		}

		if( pos != si.nPos )
		{
			auto si = SCROLLINFO{sizeof( SCROLLINFO ), SIF_POS, 0, 0, 0, pos};
			SetScrollInfo( hWnd, SB_HORZ, &si, TRUE );
			InvalidateRect( hWnd, nullptr, FALSE );
		}
		return 0;
	}

	LRESULT ScrollVertical( HWND hWnd, WORD type )
	{
		auto si = SCROLLINFO{sizeof( SCROLLINFO ), SIF_POS | SIF_RANGE};
		GetScrollInfo( hWnd, SB_VERT, &si );
		auto pos = si.nPos;

		switch( type )
		{
		case SB_LINEUP:
			if( --pos < 0 )
				pos = 0;
			break;

		case SB_LINEDOWN:
			if( ++pos >= si.nMax )
				pos = si.nMax - 1;
			break;

		case SB_PAGEUP:
			if( ( pos -= g_LinePerPage ) < 0 )
				pos = 0;
			break;

		case SB_PAGEDOWN:
			if( ( pos += g_LinePerPage ) >= si.nMax )
				pos = si.nMax - 1;
			break;

		case SB_THUMBTRACK:
			{
				auto si = SCROLLINFO{sizeof( SCROLLINFO ), SIF_TRACKPOS};
				GetScrollInfo( hWnd, SB_VERT, &si );
				pos = si.nTrackPos;
			}
			break;
		}

		if( pos != si.nPos )
		{
			auto si = SCROLLINFO{sizeof( SCROLLINFO ), SIF_POS, 0, 0, 0, pos};
			SetScrollInfo( hWnd, SB_VERT, &si, TRUE );
			InvalidateRect( hWnd, nullptr, FALSE );
		}
		return 0;
	}

	LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
	{
		switch( message )
		{
		case WM_CREATE:
			{
				if( !g_WICManager.Create() )
					return -1;

				auto logfont = LOGFONT{-MulDiv( 9, GetDpiForWindow( hWnd ), 72 )};
				logfont.lfQuality = CLEARTYPE_NATURAL_QUALITY;
				logfont.lfPitchAndFamily = FIXED_PITCH;
				lstrcpy( logfont.lfFaceName, TEXT( "MS Gothic" ) );
				g_DetailFont = CreateFontIndirect( &logfont );

				g_DisplayMode = DisplayMode::Text;
			}
			return 0;

		case WM_DESTROY:
			DeleteObject( g_DetailFont );
			g_WICManager.Release();
			return 0;

		case WM_SIZE:
			{
				auto rc = RECT{};
				GetWindowRect( hWnd, &rc );
				auto w = rc.right - rc.left;
				auto h = rc.bottom - rc.top;
				auto dc = GetDC( hWnd );
				if( dc )
				{
					auto font = SelectObject( dc, g_DetailFont );
					auto tm = TEXTMETRIC{};
					if( GetTextMetrics( dc, &tm ) )
					{
						g_TextHeight = tm.tmHeight;
						g_ColumnPerPage = w / tm.tmAveCharWidth;
						g_LinePerPage = h / tm.tmHeight;
					}
					SelectObject( dc, font );
					ReleaseDC( hWnd, dc );
				}
			}
			return 0;

		case WM_PAINT:
			{
				auto ps = PAINTSTRUCT{};
				auto dc = BeginPaint( hWnd, &ps );

				auto rc = RECT{};
				GetClientRect( hWnd, &rc );

				if( g_DisplayMode == DisplayMode::Image )
				{
					auto info = LPBITMAPINFOHEADER( g_ImageBuffer.data() );
					auto bits = g_ImageBuffer.data() + sizeof( BITMAPINFOHEADER );
					const auto sw = info->biWidth;
					const auto sh = std::abs( info->biHeight );
					const auto aspect = double( sw ) / sh;
					const auto dw = rc.right - rc.left;
					const auto dh = rc.bottom - rc.top;
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
					FillRect( dc, &rc, HBRUSH( GetStockObject( BLACK_BRUSH ) ) );
					SetStretchBltMode( dc, HALFTONE );
					StretchDIBits( dc, ( dw - w ) / 2, ( dh - h ) / 2, w, h, 0, 0, sw, sh, bits, reinterpret_cast< const LPBITMAPINFO >( info ), DIB_RGB_COLORS, SRCCOPY );
				}
				else if( !g_TextBuffer.empty() )
				{
					auto dc_store = SaveDC( dc );
					SelectObject( dc, g_DetailFont );
					SetBkMode( dc, TRANSPARENT );
					auto sih = SCROLLINFO{sizeof( SCROLLINFO ), SIF_POS};
					auto siv = SCROLLINFO{sizeof( SCROLLINFO ), SIF_POS};
					GetScrollInfo( hWnd, SB_HORZ, &sih );
					GetScrollInfo( hWnd, SB_VERT, &siv );
					rc.bottom = g_TextHeight;
					for( auto l = 0; l < g_LinePerPage; ++l )
					{
						FillRect( dc, &rc, HBRUSH( GetStockObject( WHITE_BRUSH ) ) );
						auto line_no = siv.nPos + l;
						if( line_no < g_TextBuffer.size() )
						{
							auto line = g_TextBuffer[line_no];
							if( sih.nPos < line.length() )
								TextOut( dc, 0, rc.top, line.c_str() + sih.nPos, int( line.length() ) - sih.nPos );
						}
						rc.top += g_TextHeight;
						rc.bottom = rc.top + g_TextHeight;
					}
					RestoreDC( dc, dc_store );
				}

				EndPaint( hWnd, &ps );
			}
			return 0;

		case WM_HSCROLL: return ScrollHorizon( hWnd, LOWORD( wParam ) );
		case WM_VSCROLL: return ScrollVertical( hWnd, LOWORD( wParam ) );
		case WM_LBUTTONDOWN: SetFocus( hWnd ); return 0;
		case WM_MOUSEWHEEL: return ScrollVertical( hWnd, GET_WHEEL_DELTA_WPARAM( wParam ) < 0 ? SB_PAGEDOWN : SB_PAGEUP );
		}
		return DefWindowProc( hWnd, message, wParam, lParam );
	}

	bool Create( HWND hWnd )
	{
		if( g_DetailWindow )
			return false;

		auto CLASS_NAME = TEXT( "DetailWindowClass" );
		auto wcex = WNDCLASSEX{sizeof( WNDCLASSEX )};
		if( !GetClassInfoEx( hInst, CLASS_NAME, &wcex ) )
		{
			wcex.style = CS_HREDRAW | CS_VREDRAW;
			wcex.lpfnWndProc = WndProc;
			wcex.hbrBackground = HBRUSH( GetStockObject( WHITE_BRUSH ) );
			wcex.hInstance = hInst;
			wcex.hCursor = LoadCursor( nullptr, IDC_ARROW );
			wcex.lpszClassName = CLASS_NAME;
			if( !RegisterClassExW( &wcex ) )
				return false;
		}

		SetCharset( hWnd, CharsetType::UTF8 );
		SetTabstop( hWnd, 4 );

		g_DetailWindow = CreateWindowEx( 0, CLASS_NAME, L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL, 0, 0, 0, 0, hWnd, reinterpret_cast< HMENU >( IDC_CONTENTS_DETAIL ), hInst, 0 );
		return g_DetailWindow != NULL;
	}

	void Resize( RECT rc )
	{ SetWindowPos( g_DetailWindow, 0, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER ); }

	void Clear()
	{ SetTextData( L"" ); }

	void Update( DisplayMode display_mode, bool fixed_display_pos = false )
	{
		g_DisplayMode = display_mode;
		auto flags = UINT( SIF_ALL );
		if( fixed_display_pos )
			flags &= ~SIF_POS;
		auto sih = SCROLLINFO{sizeof( SCROLLINFO ), flags};
		auto siv = SCROLLINFO{sizeof( SCROLLINFO ), flags};
		if( display_mode != DisplayMode::Image )
		{
			auto max_length = 0;
			for( auto l : g_TextBuffer )
			{
				auto length = lstrlen( l.c_str() );
				if( length > max_length )
					max_length = length;
			}
			sih.nMax = max_length;
			sih.nPage = g_ColumnPerPage;
			siv.nMax = int( g_TextBuffer.size() );
			siv.nPage = g_LinePerPage;
		}
		SetScrollInfo( g_DetailWindow, SB_HORZ, &sih, TRUE );
		SetScrollInfo( g_DetailWindow, SB_VERT, &siv, TRUE );
		InvalidateRect( g_DetailWindow, nullptr, TRUE );
	}

	void SetNode( const svn::Node *node )
	{
		if( !node->text_size || node->NodeKind != "file" )
		{
			Clear();
			return;
		}

		try
		{
			g_ContentBuffer = node->get_text_content();

			if( node->TextDelta )
			{
				auto data = g_ContentBuffer.data();
				if( !memcmp( data, "SVN\0", 4 ) )
				{
					auto get_offset = []( const uint8_t *data, int &offset )
					{
						auto value = 0;
						for( ; ; ++offset )
						{
							value <<= 8;
							value |= data[offset];
							if( !( data[offset] & 0x80 ) )
								break;
						}
						++offset;
						return value;
					};

				#if 0
					//data[4]: unknown
					auto offset = 5;
					auto begin_addr = get_offset( data, offset );
					auto end_addr = get_offset( data, offset );
					auto foo_length = data[offset];
					++offset;
					auto replace_length = data[offset];
					offset += foo_length;
					g_ContentBuffer.erase( g_ContentBuffer.begin(), g_ContentBuffer.begin() + offset );
				#endif
				}
			}

			try
			{
				g_ImageBuffer = g_WICManager.Load( g_ContentBuffer.data(), g_ContentBuffer.size() );
				Update( DisplayMode::Image );
				}
			catch( const WICManager::failed_load & )
			{
				auto mime_type = SearchProp( node->prop, "svn:mime-type" );
				if( mime_type.empty() )
				{
					switch( g_CharsetType )
					{
					case CharsetType::UTF8: SetTextData( toWide( g_ContentBuffer.data(), g_ContentBuffer.size() ) ); break;
					case CharsetType::Unicode: SetTextData( std::wstring( g_ContentBuffer.cbegin(), g_ContentBuffer.cend() ) ); break;
					case CharsetType::ShiftJIS: SetTextData( SJIStoWide( g_ContentBuffer.data(), g_ContentBuffer.size() ) ); break;
					}
				}
				else if( mime_type == L"application/octet-stream" )
					SetBinaryData( g_ContentBuffer.data(), g_ContentBuffer.size() );
				else
					SetTextData( L"" );
			}
			}
		catch( const std::exception &e )
		{ MessageBoxA( nullptr, e.what(), nullptr, MB_OK ); }
		}

	void SetBinaryData( const void *data, size_t size )
	{
		auto binary = std::wstringstream();
		auto xchs = L"0123456789ABCDEF";
		auto src = LPBYTE( data );

		g_TextBuffer.clear();

		binary << L"                 | " << std::setfill( L'0' ) << std::hex;
		for( auto i = 0; i < 16; ++i )
			binary << std::setw( 2 ) << i << L' ';
		binary << L"| Ascii";
		g_TextBuffer.emplace_back( binary.str() );
		g_TextBuffer.push_back( L"-----------------+-------------------------------------------------+-----------------" );

		for( auto i = decltype( size )( 0 ); i < size; i += 16 )
		{
			TCHAR ascii[17] = {L'\0'};

			binary.str( L"" );
			binary.clear( std::ios_base::goodbit );
			binary << std::setw( 16 ) << i << L" | ";

			for( auto j = 0; j < 16; ++j )
			{
				if( i + j >= size )
				{
					ascii[j] = L'\0';
					for( ; j < 16; ++j )
						binary << L"   ";
					break;
				}
				auto byte = src[i + j];
				ascii[j] = isprint( byte ) ? static_cast< TCHAR >( byte ) : L'.';
				binary << xchs[byte >> 4] << xchs[byte & 0xf] << L' ';
			}

			binary << L"| " << ascii;
			g_TextBuffer.emplace_back( binary.str() );
		}

		Update( DisplayMode::Binary );
	}

	void SetTextData( std::wstring text, bool fixed_display_pos )
	{
		g_TextBuffer.clear();

		auto replace_tab = []( std::wstring line )
		{
			auto pos = size_t( 0 );
			for( ;; )
			{
				auto p = line.find_first_of( L"\t", pos );
				if( p == std::wstring::npos )
					break;
				line.replace( p, 1, L"        ", g_Tabstop - p % g_Tabstop );
			}
			return line;
		};

		auto pos = size_t( 0 );
		for( ;; )
		{
			auto line_end = text.find_first_of( L"\r\n", pos );
			if( line_end == std::wstring::npos )
			{
				if( pos != std::wstring::npos )
					g_TextBuffer.emplace_back( replace_tab( text.substr( pos ) ) );
				break;
			}
			g_TextBuffer.emplace_back( replace_tab( text.substr( pos, line_end - pos ) ) );
			if( text[line_end] == L'\r' )
				++line_end;
			if( text[line_end] == L'\n' )
				++line_end;
			pos = line_end;
		}

		Update( DisplayMode::Text, fixed_display_pos );
	}

	void SetCharset( HWND hWnd, CharsetType type )
	{
		auto menu = GetMenu( hWnd );
		CheckMenuRadioItem( menu, ID_CHARSET_UTF8, ID_CHARSET_SHIFTJIS, ID_CHARSET_UTF8 + type, MF_BYCOMMAND );
		g_CharsetType = type;

		if( g_DisplayMode == DisplayMode::Text && !g_ContentBuffer.empty() )
			switch( type )
			{
			case CharsetType::UTF8: SetTextData( toWide( g_ContentBuffer.data(), g_ContentBuffer.size() ), true ); break;
			case CharsetType::Unicode: SetTextData( std::wstring( g_ContentBuffer.cbegin(), g_ContentBuffer.cend() ), true ); break;
			case CharsetType::ShiftJIS: SetTextData( SJIStoWide( g_ContentBuffer.data(), g_ContentBuffer.size() ), true ); break;
			}
	}

	void SetTabstop( HWND hWnd, int tabstop )
	{
		auto bit = -1;
		for( auto i = 1; bit < 0; ++i )
			if( tabstop == 0x1 << i )
				bit = i - 1;

		auto menu = GetMenu( hWnd );
		CheckMenuRadioItem( menu, ID_TABSTOP_2, ID_TABSTOP_8, ID_TABSTOP_2 + bit, MF_BYCOMMAND );

		if( tabstop > 8 )
			tabstop = 8;
		g_Tabstop = tabstop;

		if( g_DisplayMode == DisplayMode::Text && !g_ContentBuffer.empty() )
			switch( g_CharsetType )
			{
			case CharsetType::UTF8: SetTextData( toWide( g_ContentBuffer.data(), g_ContentBuffer.size() ), true ); break;
			case CharsetType::Unicode: SetTextData( std::wstring( g_ContentBuffer.cbegin(), g_ContentBuffer.cend() ), true ); break;
			case CharsetType::ShiftJIS: SetTextData( SJIStoWide( g_ContentBuffer.data(), g_ContentBuffer.size() ), true ); break;
			}
	}
	}
