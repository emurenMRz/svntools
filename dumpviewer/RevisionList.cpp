#include "RevisionList.h"
#include "resource.h"
#include "ListView.h"
#include "misc.h"
#include <sstream>

namespace RevisionList
{
	ListView g_RevisionList;

	bool Create( HWND hWnd )
	{
		if( !g_RevisionList.Create( hWnd, IDC_REVISION_LIST ) )
			return false;

		g_RevisionList.AddColumn( TEXT( "Revision" ), 80 );
		g_RevisionList.AddColumn( TEXT( "Actions" ), 160 );
		g_RevisionList.AddColumn( TEXT( "Author" ), 100 );
		g_RevisionList.AddColumn( TEXT( "Date(GMT)" ), 150 );
		g_RevisionList.AddColumn( TEXT( "Message" ), 1000 );
		return true;
	}

	void Build( svn::Dump &dump )
	{
		g_RevisionList.SetRedraw( false );
		for( const auto &r : dump )
		{
			TCHAR name[16];
			wsprintf( name, L"%d", r.number );
			auto item_no = g_RevisionList.AddItem( name, r.number );
			if( item_no < 0 )
				continue;

			if( !r.nodes.empty() )
			{
				auto actmap = std::map< std::string, int >();
				for( const auto &n : r.nodes )
					++actmap[n.NodeAction];

				auto actions = std::stringstream();
				for( const auto &act : actmap )
				{
					if( !actions.str().empty() )
						actions << ", ";
					actions << act.first << ": " << act.second;
				}
				g_RevisionList.SetSubItem( item_no, 1, toWide( actions.str() ).c_str() );
			}

			auto author = SearchProp( r.prop, "svn:author" );
			auto date = SearchProp( r.prop, "svn:date" );
			auto log = SearchProp( r.prop, "svn:log" );

			date.replace( date.find_first_of( L'T' ), 1, L" " );
			date.erase( date.find_last_of( L'.' ) );

			g_RevisionList.SetSubItem( item_no, 2, author.c_str() );
			g_RevisionList.SetSubItem( item_no, 3, date.c_str() );
			g_RevisionList.SetSubItem( item_no, 4, log.c_str() );
		}
		g_RevisionList.SetRedraw( true );
	}

	void Resize( RECT rc )
	{ g_RevisionList.Resize( rc.left, rc.top, rc.right, rc.bottom ); }

	void Clear()
	{ g_RevisionList.Clear(); }
}
