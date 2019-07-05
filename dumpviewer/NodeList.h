#pragma once

#include "SVNDump.h"
#include <windows.h>

namespace NodeList
{
	bool Create( HWND hWnd );
	void Build( const svn::Revision &rev );
	void Resize( RECT rc );
	void Clear();

	LRESULT OnNotify( HWND hWnd, UINT code, LPARAM lParam );
}
