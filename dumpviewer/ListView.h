#pragma	once

#include <windows.h>
#include <commctrl.h>

class ListView
{
public:
	ListView();
	~ListView() = default;

	operator HWND()	const noexcept { return m_List; }

	bool	Create( HWND parent, int id, bool is_owner_data = false ) noexcept;
	bool	SetSubClass( LONG_PTR sub_class_proc ) noexcept;
	LRESULT	OriginalWindowProc( HWND hWnd, UINT msg, WPARAM wp, LPARAM lp ) const noexcept;
	void	ReleaseSubClass() noexcept;

	void	Clear() const noexcept;
	void	RemoveItem( int item_no ) const noexcept;
	int		GetItems() const noexcept;

	void	AddColumn( LPCTSTR name, int width, bool is_right = false ) noexcept;
	int		AddItem( LPCTSTR name, LPARAM data = 0, int group_id = -1 ) noexcept;

	//for owner data
	void	SetItemCount( int count ) const noexcept;

	void	Resize( int x, int y, int width, int height ) const noexcept;

	void	SetSubItem( int item_no, int column_no, LPCTSTR name ) const noexcept;
	bool	GetSubItem( int item_no, int column_no, LPTSTR name, int size ) const noexcept;

	LPARAM	GetItemData( int item_no ) const noexcept;

	void	SetCheckState( int item_no, bool state ) const noexcept;
	bool	GetCheckState( int item_no ) const noexcept;
	void	SetItemState( int item_no, UINT state, UINT mask ) const noexcept;
	UINT	GetItemState( int item_no, UINT mask ) const noexcept;
	void	EnsureVisible( int item_no ) const noexcept;

	void	SetRedraw( bool state ) const noexcept;

	//===== Group関係 =====//
	int		EnableGroupView( bool state ) const noexcept;
	int		AddGroup( LPCTSTR name, LPCTSTR task ) noexcept;

	void	SetGroupState( UINT id, UINT mask, UINT state ) const noexcept;
	UINT	GetGroupState( UINT id, UINT mask ) const noexcept;
	int		SetGroupInfo( int id, const PLVGROUP group ) const noexcept;
	int		GetGroupInfo( int id, PLVGROUP group ) const noexcept;

	int		GetFirstGroupID() const noexcept;

	void	ChangeShowState( PNMLVLINK pli ) const noexcept;

private:

	HWND m_List;
	LONG_PTR m_OriginalProc;
	int m_Columns;
	int m_Items;
	int m_Groups;
};
