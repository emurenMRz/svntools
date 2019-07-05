#include "misc.h"
#include <filesystem>

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
