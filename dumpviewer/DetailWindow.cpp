#include "DetailWindow.h"
#include "dumpviewer.h"
#include "WICManager.h"
#include "misc.h"
#include <iomanip>
#include <sstream>

namespace DetailWindow
{
	HWND g_DetailWindow;
	HWND g_BitmapWindow;
	HFONT g_DetailFont;

	CharsetType g_CharsetType;
	WICManager g_WICManager;
	WICManager::buffer_t g_ImageBuffer;
	svn::text_data_t g_TextBuffer;

	bool Create( HWND hWnd )
	{
		if( !g_WICManager.Create() )
			return false;

		g_DetailWindow = CreateWindowEx( 0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_LEFT | ES_MULTILINE | ES_READONLY, 0, 0, 0, 0, hWnd, (HMENU )IDC_CONTENTS_DETAIL, hInst, 0 );
		if( !g_DetailWindow )
			return false;

		g_BitmapWindow = CreateWindowEx( 0, L"STATIC", L"", WS_CHILD | SS_OWNERDRAW, 0, 0, 0, 0, hWnd, (HMENU )IDC_CONTENTS_BITMAP, hInst, 0 );
		if( !g_BitmapWindow )
			return false;

		g_DetailFont = CreateFont( 16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_NATURAL_QUALITY, FIXED_PITCH, L"Terminal" );
		if( g_DetailFont )
			SendMessage( g_DetailWindow, WM_SETFONT, (WPARAM )g_DetailFont, MAKELPARAM( TRUE, 0 ) );

		SetCharset( hWnd, CharsetType::UTF8 );
		SetTabstop( hWnd, 4 );

		return true;
	}

	void Destroy()
	{
		SendMessage( g_DetailWindow, WM_SETFONT, 0, MAKELPARAM( TRUE, 0 ) );
		if( g_DetailFont )
			DeleteObject( g_DetailFont );
		g_WICManager.Release();
	}

	bool Draw( const LPDRAWITEMSTRUCT dis )
	{
		if( g_ImageBuffer.empty() || dis->CtlID != IDC_CONTENTS_BITMAP || !( dis->itemAction & ODA_DRAWENTIRE ) )
			return false;

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
		return true;
	}

	void Resize( RECT rc )
	{
		SetWindowPos( g_DetailWindow, 0, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER );
		SetWindowPos( g_BitmapWindow, 0, rc.left, rc.top, rc.right, rc.bottom, SWP_NOCOPYBITS | SWP_NOZORDER );
	}

	void Clear()
	{
		ShowWindow( g_DetailWindow, SW_SHOWNORMAL );
		ShowWindow( g_BitmapWindow, SW_HIDE );
		SetTextData( L"" );
	}

	void SetNode( const svn::Node *node )
	{
		if( !node->text_size || node->NodeKind != "file" )
		{
			Clear();
			return;
		}

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

		try
		{
			g_TextBuffer = node->get_text_content();

			if( node->TextDelta )
			{
				auto data = g_TextBuffer.data();
				if( !memcmp( data, "SVN\0", 4 ) )
				{
					auto str = std::string( "no implementation: --deltas option\r\n" );
					g_TextBuffer.insert( g_TextBuffer.begin(), str.begin(), str.end() );
				#if 0
					//data[4]: unknown
					auto offset = 5;
					auto begin_addr = get_offset( data, offset );
					auto end_addr = get_offset( data, offset );
					auto foo_length = data[offset];
					++offset;
					auto replace_length = data[offset];
					offset += foo_length;
					g_TextBuffer.erase( g_TextBuffer.begin(), g_TextBuffer.begin() + offset );
				#endif
				}
			}

			try
			{
				g_ImageBuffer = g_WICManager.Load( g_TextBuffer.data(), g_TextBuffer.size() );
				ShowWindow( g_DetailWindow, SW_HIDE );
				ShowWindow( g_BitmapWindow, SW_SHOWNORMAL );
				InvalidateRect( g_BitmapWindow, NULL, TRUE );
			}
			catch( const WICManager::failed_load & )
			{
				auto mime_type = SearchProp( node->prop, "svn:mime-type" );
				if( mime_type.empty() )
				{
					switch( g_CharsetType )
					{
					case CharsetType::UTF8: SetTextData( toWide( g_TextBuffer.data(), g_TextBuffer.size() ) ); break;
					case CharsetType::Unicode: SetTextData( std::wstring( g_TextBuffer.cbegin(), g_TextBuffer.cend() ) ); break;
					case CharsetType::ShiftJIS: SetTextData( SJIStoWide( g_TextBuffer.data(), g_TextBuffer.size() ) ); break;
					}
				}
				else if( mime_type == L"application/octet-stream" )
					SetBinaryData( g_TextBuffer.data(), g_TextBuffer.size() );
				else
					SetTextData( L"" );
				ShowWindow( g_DetailWindow, SW_SHOWNORMAL );
				ShowWindow( g_BitmapWindow, SW_HIDE );
			}
		}
		catch( const std::exception &e )
		{ MessageBoxA( NULL, e.what(), NULL, MB_OK ); }
	}

	void SetBinaryData( const void *data, size_t size )
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

	void SetTextData( std::wstring text )
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

	void SetCharset( HWND hWnd, CharsetType type )
	{
		auto menu = GetMenu( hWnd );
		CheckMenuItem( menu, ID_CHARSET_UTF8, type == CharsetType::UTF8 ? MF_CHECKED : MF_UNCHECKED );
		CheckMenuItem( menu, ID_CHARSET_UNICODE, type == CharsetType::Unicode ? MF_CHECKED : MF_UNCHECKED );
		CheckMenuItem( menu, ID_CHARSET_SHIFTJIS, type == CharsetType::ShiftJIS ? MF_CHECKED : MF_UNCHECKED );
		g_CharsetType = type;

		if( !g_TextBuffer.empty() )
			switch( type )
			{
			case CharsetType::UTF8: SetTextData( toWide( g_TextBuffer.data(), g_TextBuffer.size() ) ); break;
			case CharsetType::Unicode: SetTextData( std::wstring( g_TextBuffer.cbegin(), g_TextBuffer.cend() ) ); break;
			case CharsetType::ShiftJIS: SetTextData( SJIStoWide( g_TextBuffer.data(), g_TextBuffer.size() ) ); break;
			}
	}

	void SetTabstop( HWND hWnd, int tabstop )
	{
		auto menu = GetMenu( hWnd );
		CheckMenuItem( menu, ID_TABSTOP_2, tabstop == 2 ? MF_CHECKED : MF_UNCHECKED );
		CheckMenuItem( menu, ID_TABSTOP_4, tabstop == 4 ? MF_CHECKED : MF_UNCHECKED );
		CheckMenuItem( menu, ID_TABSTOP_8, tabstop == 8 ? MF_CHECKED : MF_UNCHECKED );

		tabstop *= 4;
		SendMessage( g_DetailWindow, EM_SETTABSTOPS, 1, LPARAM( &tabstop ) );
		InvalidateRect( g_DetailWindow, nullptr, FALSE );
	}
}
