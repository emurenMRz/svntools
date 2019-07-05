#include "NodeList.h"
#include "resource.h"
#include "ListView.h"
#include "misc.h"
#include "DetailWindow.h"
#include <sstream>

namespace NodeList
{
	ListView g_ContentsList;
	std::map<std::string, int> g_GroupNumber;
	int g_GroupNumberEtc;

	bool Create( HWND hWnd )
	{
		if( !g_ContentsList.Create( hWnd, IDC_CONTENTS_LIST ) )
			return false;

		g_ContentsList.AddColumn( TEXT( "Action" ), 50 );
		g_ContentsList.AddColumn( TEXT( "Kind" ), 50 );
		g_ContentsList.AddColumn( TEXT( "Path" ), 300 );
		g_ContentsList.AddColumn( TEXT( "Copyfrom" ), 400 );
		g_ContentsList.AddColumn( TEXT( "Delta" ), 200 );
		g_ContentsList.EnableGroupView( true );

		g_GroupNumber["add"] = g_ContentsList.AddGroup( L"add", L"" );
		g_GroupNumber["change"] = g_ContentsList.AddGroup( L"change", L"" );
		g_GroupNumber["delete"] = g_ContentsList.AddGroup( L"delete", L"" );
		g_GroupNumber["replace"] = g_ContentsList.AddGroup( L"replace", L"" );
		g_GroupNumberEtc = g_ContentsList.AddGroup( L"etc", L"" );
		return true;
	}

	void Build( const svn::Revision &rev )
	{
		DetailWindow::SetTextData( SearchProp( rev.prop, "svn:log" ) );

		g_ContentsList.Clear();
		g_ContentsList.SetRedraw( false );
		g_ContentsList.EnableGroupView( false );
		for( const auto &n : rev.nodes )
			if( !n.NodeAction.empty() )
			{
				auto group = g_GroupNumberEtc;
				auto find = g_GroupNumber.find( n.NodeAction );
				if( find != g_GroupNumber.end() )
					group = ( *find ).second;

				auto item_no = g_ContentsList.AddItem( toWide( n.NodeAction ).c_str(), reinterpret_cast< LPARAM >( &n ), group );
				if( item_no < 0 )
					continue;
				g_ContentsList.SetSubItem( item_no, 1, toWide( n.NodeKind ).c_str() );
				g_ContentsList.SetSubItem( item_no, 2, toWide( n.NodePath ).c_str() );

				if( n.NodeCopyfromRev > 0 )
				{
					auto ss = std::wstringstream();
					ss << toWide( n.NodeCopyfromPath ) << L", rev: " << n.NodeCopyfromRev;
					g_ContentsList.SetSubItem( item_no, 3, ss.str().c_str() );
				}

				if( n.PropDelta || n.TextDelta )
				{
					auto ss = std::wstringstream();
					if( n.PropDelta )
						ss << L"prop";
					if( n.TextDelta )
					{
						if( n.PropDelta )
							ss << L", ";
						ss << L"text";
					}
					g_ContentsList.SetSubItem( item_no, 4, ss.str().c_str() );
				}
			}
		g_ContentsList.SetRedraw( true );
		g_ContentsList.EnableGroupView( true );
	}

	void Resize( RECT rc )
	{ g_ContentsList.Resize( rc.left, rc.top, rc.right, rc.bottom ); }

	void Clear()
	{ g_ContentsList.Clear(); }

	const svn::Node *GetNode( int item_no )
	{ return reinterpret_cast< const svn::Node * >( g_ContentsList.GetItemData( item_no ) ); }
}
