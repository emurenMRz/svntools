#pragma once

#include "SVNDump.h"
#include <string>
#include <windows.h>

namespace DetailWindow
{
	enum CharsetType
	{ UTF8, Unicode, ShiftJIS };

	bool Create( HWND hWnd );
	void Resize( RECT rc );
	void Clear();

	void SetNode( const svn::Node *node );
	void SetBinaryData( const void *data, size_t size );
	void SetTextData( std::wstring text );

	void SetCharset( HWND hWnd, CharsetType type );
	void SetTabstop( HWND hWnd, int tabstop );
}
