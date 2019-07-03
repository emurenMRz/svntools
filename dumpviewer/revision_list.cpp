#include "framework.h"
#include "dumpviewer.h"
#include "listbox.h"
#include <sstream>

ListBox::ListBox()
	: m_Self( 0 )
{}

bool ListBox::Create( HWND parent, int id ) noexcept
{
	m_Self = CreateWindowEx( WS_EX_WINDOWEDGE, L"LISTBOX", nullptr, WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY, 0, 0, 0, 0, parent, (HMENU )id, hInst, nullptr );
	return m_Self != 0;
}

void ListBox::Resize( int x, int y, int width, int height ) const noexcept
{ SetWindowPos( m_Self, 0, x, y, width, height, SWP_NOZORDER ); }

int ListBox::Size() const noexcept
{ return SendMessage( m_Self, LB_GETCOUNT, 0, 0 ); }

void ListBox::Clear() const noexcept
{ SendMessage( m_Self, LB_RESETCONTENT, 0, 0 ); }

void ListBox::Add( LPCTSTR item ) const noexcept
{ SendMessage( m_Self, LB_ADDSTRING, 0, (LPARAM )item ); }

void ListBox::Add( int value ) const noexcept
{
	auto ss = std::wstringstream();
	ss << value;
	Add( ss.str().c_str() );
	SendMessage( m_Self, LB_SETITEMDATA, Size() - 1, (LPARAM )value );
}

void ListBox::Insert( int no, LPCTSTR item ) const noexcept
{ SendMessage( m_Self, LB_INSERTSTRING, no, (LPARAM )item ); }

int ListBox::GetItemData( int no ) const noexcept
{
	if( no == -1 )
		no = SendMessage( m_Self, LB_GETCURSEL, 0, 0 );
	return SendMessage( m_Self, LB_GETITEMDATA, no, 0 );
}
