#include "ListView.h"
#include <stdint.h>

ListView::ListView()
	: m_List( 0 )
	, m_OriginalProc( 0 )
	, m_Columns( 0 )
	, m_Items( 0 )
	, m_Groups( 0 )
{}

bool ListView::Create( HWND parent, int id, bool is_owner_data ) noexcept
{
	auto inst = ( HINSTANCE )GetWindowLongPtr( parent, GWLP_HINSTANCE );
	if( !inst )
		return false;

	auto style = WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS;
	if( is_owner_data )
		style |= LVS_OWNERDATA;

	m_List = CreateWindowEx( WS_EX_CONTROLPARENT, WC_LISTVIEW, L"", style, 0, 0, 0, 0, parent, reinterpret_cast< HMENU >( static_cast< int64_t >( id ) ), inst, NULL );
	if( !m_List )
		return false;

	ListView_SetExtendedListViewStyle( m_List, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_HEADERDRAGDROP );
	return true;
}

bool ListView::SetSubClass( LONG_PTR sub_class_proc ) noexcept
{
	m_OriginalProc = GetWindowLongPtr( m_List, GWLP_WNDPROC );
	return !SetWindowLongPtr( m_List, GWLP_WNDPROC, sub_class_proc );
}

LRESULT ListView::OriginalWindowProc( HWND hWnd, UINT msg, WPARAM wp, LPARAM lp ) const noexcept
{ return !m_OriginalProc ? 0 : CallWindowProc( ( WNDPROC )m_OriginalProc, hWnd, msg, wp, lp ); }

void ListView::ReleaseSubClass() noexcept
{
	if( m_OriginalProc )
	{
		SetWindowLongPtr( m_List, GWLP_WNDPROC, ( LONG_PTR )m_OriginalProc );
		m_OriginalProc = 0;
	}
}

void ListView::Clear() const noexcept
{ ListView_DeleteAllItems( m_List ); }

void ListView::RemoveItem( int item_no ) const noexcept
{ ListView_DeleteItem( m_List, item_no ); }

int ListView::GetItems() const noexcept
{ return ListView_GetItemCount( m_List ); }

void ListView::AddColumn( LPCTSTR name, int width, bool is_right ) noexcept
{
	auto lvcol = LV_COLUMN();
	lvcol.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	lvcol.fmt = !is_right ? LVCFMT_LEFT : LVCFMT_RIGHT;
	lvcol.cx = width;
	lvcol.pszText = const_cast< LPTSTR >( name );
	lvcol.iSubItem = 0;

	if( ListView_InsertColumn( m_List, m_Columns, &lvcol ) != -1 )
		++m_Columns;
}

void ListView::SetItemCount( int count ) const noexcept
{ ListView_SetItemCountEx( m_List, count, LVSICF_NOINVALIDATEALL ); }

void ListView::Resize( int x, int y, int width, int height ) const noexcept
{ MoveWindow( m_List, x, y, width, height, TRUE ); }

int ListView::AddItem( LPCTSTR name, LPARAM data, int group_id ) noexcept
{
	auto item = LV_ITEM();
	item.mask = LVIF_TEXT | LVIF_PARAM | ( group_id >= 0 ? LVIF_GROUPID : 0 );
	item.pszText = const_cast< LPTSTR >( name );
	item.iItem = m_Items;
	item.iSubItem = 0;
	item.lParam = data;

	if( group_id >= 0 )
		item.iGroupId = group_id;

	auto new_item_no = ListView_InsertItem( m_List, &item );
	if( new_item_no != -1 )
		++m_Items;
	return new_item_no;
}

void ListView::SetSubItem( int item_no, int column_no, LPCTSTR name ) const noexcept
{
	auto item = LV_ITEM();
	item.mask = LVIF_TEXT;
	item.pszText = const_cast< LPTSTR >( name );
	item.iItem = item_no;
	item.iSubItem = column_no;
	ListView_SetItem( m_List, &item );
}

bool ListView::GetSubItem( int item_no, int column_no, LPTSTR name, int size ) const noexcept
{
	auto item = LV_ITEM();
	item.mask = LVIF_TEXT;
	item.pszText = name;
	item.cchTextMax = size;
	item.iItem = item_no;
	item.iSubItem = column_no;
	return ListView_GetItem( m_List, &item ) == TRUE;
}

LPARAM ListView::GetItemData( int item_no ) const noexcept
{
	auto item = LV_ITEM();
	item.mask = LVIF_PARAM;
	item.iItem = item_no;
	return !ListView_GetItem( m_List, &item ) ? 0 : item.lParam;
}

void ListView::SetCheckState( int item_no, bool state ) const noexcept
{ ListView_SetCheckState( m_List, item_no, state ); }

bool ListView::GetCheckState( int item_no ) const noexcept
{ return ListView_GetCheckState( m_List, item_no ) != 0; }

void ListView::SetItemState( int item_no, UINT state, UINT mask ) const noexcept
{ ListView_SetItemState( m_List, item_no, state, mask ); }

UINT ListView::GetItemState( int item_no, UINT mask ) const noexcept
{ return ListView_GetItemState( m_List, item_no, mask ); }

void ListView::EnsureVisible( int item_no ) const noexcept
{ ListView_EnsureVisible( m_List, item_no, FALSE ); }

void ListView::SetRedraw( bool state ) const noexcept
{ SendMessage( m_List, WM_SETREDRAW, (WPARAM )( state ), NULL ); }

//===== Group関係 =====//
int ListView::EnableGroupView( bool state ) const noexcept
{ return int( ListView_EnableGroupView( m_List, state ) ); }

int ListView::AddGroup( LPCTSTR name, LPCTSTR task ) noexcept
{
	auto group = LVGROUP();
	group.cbSize = sizeof( LVGROUP );
	group.mask = LVGF_HEADER | LVGF_GROUPID | LVGF_TASK;
	group.pszHeader = const_cast< LPTSTR >( name );
	group.iGroupId = m_Groups;
	group.pszTask = const_cast< LPTSTR >( task );

	auto new_group_no = ( int )ListView_InsertGroup( m_List, -1, &group );
	if( new_group_no != -1 )
		++m_Groups;
	return new_group_no;
}

void ListView::SetGroupState( UINT id, UINT mask, UINT state ) const noexcept
{ ListView_SetGroupState( m_List, id, mask, state ); }

UINT ListView::GetGroupState( UINT id, UINT mask ) const noexcept
{ return ListView_GetGroupState( m_List, id, mask ); }

int ListView::SetGroupInfo( int id, const PLVGROUP group ) const noexcept
{ return int( ListView_SetGroupInfo( m_List, id, group ) ); }

int ListView::GetGroupInfo( int id, PLVGROUP group ) const noexcept
{ return int( ListView_GetGroupInfo( m_List, id, group ) ); }

int ListView::GetFirstGroupID() const noexcept
{
	auto item = LV_ITEM();
	item.mask = LVIF_GROUPID;
	item.iItem = ListView_GetNextItem( m_List, -1, LVNI_ALL );
	item.iSubItem = 0;
	return !ListView_GetItem( m_List, &item ) ? -1 : item.iGroupId;
}

void ListView::ChangeShowState( PNMLVLINK pli ) const noexcept
{
	auto group = LVGROUP();
	auto id = pli->iSubItem;

	if( GetGroupState( id, LVGS_COLLAPSED ) )
	{
		SetGroupState( id, LVGS_COLLAPSED, 0 );
		group.pszTask = const_cast< LPTSTR >( TEXT( "Hide" ) );
	}
	else
	{
		SetGroupState( id, LVGS_COLLAPSED, LVGS_COLLAPSED );
		group.pszTask = const_cast< LPTSTR >( TEXT( "Show" ) );
	}

	group.cbSize = sizeof( LVGROUP );
	group.mask = LVGF_TASK;
	SetGroupInfo( id, &group );
}
