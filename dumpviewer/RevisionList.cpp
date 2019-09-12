#include "RevisionList.h"
#include "dumpviewer.h"
#include "ListView.h"
#include "NodeList.h"
#include "misc.h"
#include <sstream>
#include <strsafe.h>

namespace RevisionList
{
	ListView g_RevisionList;
	std::vector<std::string> g_ActionsList;
	std::vector<int> g_ReviisonNumbers;

	bool Create( HWND hWnd )
	{
		if( !g_RevisionList.Create( hWnd, IDC_REVISION_LIST, true ) )
			return false;

		g_RevisionList.AddColumn( TEXT( "Revision" ), 80 );
		g_RevisionList.AddColumn( TEXT( "Actions" ), 160 );
		g_RevisionList.AddColumn( TEXT( "Author" ), 100 );
		g_RevisionList.AddColumn( TEXT( "Date(UTC)" ), 150 );
		g_RevisionList.AddColumn( TEXT( "Message" ), 1000 );
		return true;
	}

	void Build( svn::Dump &dump )
	{
		auto size = dump.GetRevisionCount();
		g_RevisionList.SetItemCount( int( size ) );
		g_ActionsList.clear();
		g_ActionsList.resize( size );
		g_ReviisonNumbers.clear();
		if( !dump[0] || dump.GetRevisionCount() - 1 != dump.GetMaxRevision() )
			for( auto &r : dump )
				g_ReviisonNumbers.push_back( r.number );
	}

	void Resize( RECT rc )
	{ g_RevisionList.Resize( rc.left, rc.top, rc.right, rc.bottom ); }

	void Clear()
	{ g_RevisionList.Clear(); }

	LRESULT OnNotify( HWND hWnd, UINT code, LPARAM lParam )
	{
		switch( code )
		{
		case LVN_GETDISPINFO:
			{
				const auto di = reinterpret_cast< NMLVDISPINFO * >( lParam );
				if( di->item.mask & LVIF_TEXT )
				{
					const auto &r = g_SVNDump[g_ReviisonNumbers.empty() ? di->item.iItem : g_ReviisonNumbers[di->item.iItem]];
					if( !r )
						break;
					switch( di->item.iSubItem )
					{
					case 0: //Revision
						StringCchPrintf( di->item.pszText, di->item.cchTextMax, L"%d", r.number );
						break;
					case 1: //Actions
						if( !r.nodes.empty() )
						{
							auto &actions = g_ActionsList[di->item.iItem];
							if( actions.empty() )
							{
								auto actmap = std::map< std::string, int >();
								for( const auto &n : r.nodes )
									++actmap[n.NodeAction];

								auto ss = std::stringstream();
								for( const auto &act : actmap )
								{
									if( !ss.str().empty() )
										ss << ", ";
									ss << act.first << ": " << act.second;
								}
								actions = ss.str();
							}
							StringCchPrintf( di->item.pszText, di->item.cchTextMax, L"%S", actions.c_str() );
						}
						break;
					case 2: //Author
						StringCchPrintf( di->item.pszText, di->item.cchTextMax, L"%s", SearchProp( r.prop, "svn:author" ).c_str() );
						break;
					case 3: //Date(UTC)
						{
							auto date = SearchProp( r.prop, "svn:date" );
							if( date.empty() )
								StringCchPrintf( di->item.pszText, di->item.cchTextMax, L"<no date>" );
							else
							{
								date.replace( date.find_first_of( L'T' ), 1, L" " );
								date.erase( date.find_last_of( L'.' ) );
								StringCchPrintf( di->item.pszText, di->item.cchTextMax, L"%s", date.c_str() );
							}
						}
						break;
					case 4: //Message
						StringCchPrintf( di->item.pszText, di->item.cchTextMax, L"%s", SearchProp( r.prop, "svn:log" ).c_str() );
						break;
					}
				}
			}
			break;

		case LVN_ITEMCHANGED:
			{
				const auto lv = LPNMLISTVIEW( lParam );
				if( lv->uNewState & LVIS_SELECTED )
					if( lv->iItem >= 0 )
						NodeList::Build( g_SVNDump[g_ReviisonNumbers.empty() ? lv->iItem : g_ReviisonNumbers[lv->iItem]] );
			}
			break;
		}
		return LRESULT();
	}
}
