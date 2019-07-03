#pragma once

class ListBox
{
public:
	ListBox();
	ListBox( const ListBox &r ) = delete;
	ListBox( const ListBox &&r ) = delete;
	~ListBox() = default;

	ListBox &operator =( const ListBox &r ) = delete;
	ListBox &operator =( const ListBox &&r ) = delete;

	operator HWND() const noexcept { return m_Self; }

	bool Create( HWND parent, int id ) noexcept;
	void Resize( int x, int y, int width, int height ) const noexcept;
	int Size() const noexcept;

	void Clear() const noexcept;
	void Add( LPCTSTR item ) const noexcept;
	void Add( int value ) const noexcept;
	void Insert( int no, LPCTSTR item ) const noexcept;

	int GetItemData( int no = -1 ) const noexcept;

private:
	HWND m_Self;
};
