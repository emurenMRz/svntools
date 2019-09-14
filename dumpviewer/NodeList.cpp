#include "NodeList.h"
#include "dumpviewer.h"
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

	LRESULT OnNotify( HWND hWnd, UINT code, LPARAM lParam )
	{
		switch( code )
		{
		case LVN_ITEMCHANGED:
			{
				const auto lv = LPNMLISTVIEW( lParam );
				if( lv->uNewState & LVIS_SELECTED )
					DetailWindow::SetNode( reinterpret_cast< const svn::Node * >( lv->lParam ) );
			}
			break;

		case NM_DBLCLK:
			{
				const auto lpia = reinterpret_cast< LPNMITEMACTIVATE >( lParam );
				if( lpia->iItem < 0 )
					break;
				const auto node = GetNode( lpia->iItem );
				if( node && node->NodeKind == "file" )
					if( node->TextDelta )
						MessageBox( hWnd, TEXT( "--deltasオプションを使用したdumpファイルのコンテンツ出力には対応していません" ), NULL, MB_OK );
					else
						SaveToFile( hWnd, node );
			}
			break;

		case NM_CUSTOMDRAW:
			{
				const auto lplvcd = reinterpret_cast< LPNMLVCUSTOMDRAW >( lParam );
				switch( lplvcd->nmcd.dwDrawStage )
				{
				case CDDS_PREPAINT:
					return CDRF_NOTIFYITEMDRAW;
				case CDDS_ITEMPREPAINT:
					{
						const auto c = reinterpret_cast< const svn::Node * >( lplvcd->nmcd.lItemlParam );
						lplvcd->clrTextBk = c->NodeAction == "add" ? RGB( 0xf8, 0xff, 0xf8 ) : c->NodeAction == "change" ? RGB( 0xf8, 0xf8, 0xff ) : c->NodeAction == "replace" ? RGB( 0xff, 0xf8, 0xf8 ) : 0xffffff;
						lplvcd->clrText = c->NodeAction == "delete" ? 0x808080 : 0;
					}
					return CDRF_DODEFAULT;
				}
			}
			return CDRF_DODEFAULT;
		}
		return 0;
	}
}
