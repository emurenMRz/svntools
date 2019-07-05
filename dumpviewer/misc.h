#pragma once

#include "SVNDump.h"
#include <string>
#include <windows.h>

std::wstring toWide( const void *str, size_t length );
std::wstring toWide( const std::string &str );
std::wstring SJIStoWide( const void *str, size_t length );
std::wstring SearchProp( const svn::prop_data_t &props, const std::string name );
void SaveToFile( HWND hWnd, const svn::Node *node );
