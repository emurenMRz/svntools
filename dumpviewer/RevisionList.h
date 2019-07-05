#pragma once

#include "SVNDump.h"
#include <windows.h>

namespace RevisionList
{
	bool Create( HWND hWnd );
	void Build( svn::Dump &dump );
	void Resize( RECT rc );
	void Clear();

	LRESULT OnNotify( HWND hWnd, UINT code, LPARAM lParam );
}
