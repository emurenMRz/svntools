#include "SVNDump.h"
#include <iostream>
#include <sstream>
#include <string>
#include <map>

using namespace std;

void compare_nodes( int rev_no, const svn::Nodes &nodes1, const svn::Nodes &nodes2 );



bool dump_compare( const char *dump1_path, const char *dump2_path )
{
	try
	{
		auto dmp1 = svn::Dump( dump1_path );
		if( !dmp1 )
			return false;
		cout << dump1_path << ": max revision number: " << dmp1.GetMaxRevision() << ", format version: " << dmp1.GetFormatVersion() << endl;

		auto dmp2 = svn::Dump( dump2_path );
		if( !dmp2 )
			return false;
		cout << dump2_path << ": max revision number: " << dmp2.GetMaxRevision() << ", format version: " << dmp2.GetFormatVersion() << endl;

		if( dmp1 != dmp2 )
		{
			cerr << "different UUID: " << dmp1.UUID() << " != " << dmp2.UUID() << endl;
			return false;
		}

		for( int i = 0; ; ++i )
		{
			const auto &rev1 = dmp1[i];
			const auto &rev2 = dmp2[i];

			if( !rev1 && !rev2 )
				break;

			if( !rev1 || !rev2 )
			{
				cout << "check rev: " << i << endl;
				if( !rev1 )
					cout << "	not found revision number in rev1." << endl;
				if( !rev2 )
					cout << "	not found revision number in rev2." << endl;
				continue;
			}

			if( rev1.prop != rev2.prop )
			{
				cout << "check rev: " << i << "; different property." << endl;
				continue;
			}

			if( rev1.nodes.size() != rev2.nodes.size() )
			{
				cout << "check rev: " << i << endl;
				cout << "	different nodes size: " << rev1.nodes.size() << " != " << rev2.nodes.size() << endl;
				continue;
			}

			compare_nodes( i, rev1.nodes, rev2.nodes );
		}
	}
	catch( const std::exception &e )
	{
		cerr << e.what() << endl;
	}

	cout << "complete." << endl;
	return true;
}



std::ostream &operator<<( std::ostream &out, const svn::text_data_t &text )
{
	const auto xch = "0123456789ABCDEF";
	for( const auto &ch : text )
		out << xch[ch >> 4] << xch[ch & 0xf] << " ";
	return out;
}

std::string compare_contents( const svn::Node &node1, const svn::Node &node2 )
{
	auto result = std::stringstream();

	if( node1.NodePath != node2.NodePath )
		result << "		NodePath: " << node1.NodePath << " <> " << node2.NodePath << endl;
	if( node1.NodeKind != node2.NodeKind )
		result << "		NodeKind: " << node1.NodeKind << " <> " << node2.NodeKind << endl;
	if( node1.NodeAction != node2.NodeAction )
		result << "		NodeAction: " << node1.NodeAction << " <> " << node2.NodeAction << endl;
	if( node1.NodeCopyfromRev != node2.NodeCopyfromRev )
		result << "		NodeCopyfromRev: " << node1.NodeCopyfromRev << " <> " << node2.NodeCopyfromRev << endl;
	if( node1.NodeCopyfromPath != node2.NodeCopyfromPath )
		result << "		NodeCopyfromPath: " << node1.NodeCopyfromPath << " <> " << node2.NodeCopyfromPath << endl;
	if( node1.TextContentMD5 != node2.TextContentMD5 )
		result << "		TextContentMD5: " << node1.TextContentMD5 << " <> " << node2.TextContentMD5 << endl;
	if( node1.TextContentSHA1 != node2.TextContentSHA1 )
		result << "		TextContentSHA1: " << node1.TextContentSHA1 << " <> " << node2.TextContentSHA1 << endl;
	if( node1.TextCopySourceMD5 != node2.TextCopySourceMD5 )
		result << "		TextCopySourceMD5: " << node1.TextCopySourceMD5 << " <> " << node2.TextCopySourceMD5 << endl;
	if( node1.TextCopySourceSHA1 != node2.TextCopySourceSHA1 )
		result << "		TextCopySourceSHA1: " << node1.TextCopySourceSHA1 << " <> " << node2.TextCopySourceSHA1 << endl;
	if( node1.TextDelta != node2.TextDelta )
		result << "		TextDelta: " << node1.TextDelta << " <> " << node2.TextDelta << endl;
	if( node1.PropDelta != node2.PropDelta )
		result << "		PropDelta: " << node1.PropDelta << " <> " << node2.PropDelta << endl;
	if( node1.TextDeltaBaseMD5 != node2.TextDeltaBaseMD5 )
		result << "		TextDeltaBaseMD5: " << node1.TextDeltaBaseMD5 << " <> " << node2.TextDeltaBaseMD5 << endl;
	if( node1.TextDeltaBaseSHA1 != node2.TextDeltaBaseSHA1 )
		result << "		TextDeltaBaseSHA1: " << node1.TextDeltaBaseSHA1 << " <> " << node2.TextDeltaBaseSHA1 << endl;

	if( node1.prop != node2.prop )
	{
		result << "		prop: " << endl;

		struct cmpprop
		{
			bool text1 = false;
			bool text2 = false;
		};
		auto table = std::map<std::string, cmpprop>();
		for( const auto &pair : node1.prop )
			table[pair.first].text1 = true;
		for( const auto &pair : node2.prop )
			table[pair.first].text2 = true;

		auto cmptext = [&result]( const auto &text1, const auto &text2 )
		{
			auto len1 = text1.size();
			auto len2 = text2.size();

			if( len1 != len2 )
				result << "		different text size: " << text1 << "<> " << text2 << endl;
			else
				for( auto i = 0; i < len1; ++i )
					if( text1[i] != text2[i] )
					{
						result << "		different text pos: " << i << endl;
						break;
					}
		};
		for( const auto &pair : table )
		{
			const auto &key = pair.first;
			if( pair.second.text1 && pair.second.text2 )
				cmptext( node1.prop.at( key ), node2.prop.at( key ) );
			else if( pair.second.text1 )
				result << "		node1: " << node1.prop.at( key ) << endl;
			else if( pair.second.text2 )
				result << "		node2: " << node2.prop.at( key ) << endl;
		}
	}

	if( node1.text_size != node2.text_size )
		result << "		text_size: " << node1.text_size << " <> " << node2.text_size << endl;

	auto node1_rev_no = node1.parent ? node1.parent->number : -1;
	auto node2_rev_no = node2.parent ? node2.parent->number : -1;
	if( node1_rev_no != node2_rev_no )
		result << "		parent revision no: " << node1_rev_no << " <> " << node2_rev_no << endl;

	return result.str();
}



void compare_nodes( int rev_no, const svn::Nodes &nodes1, const svn::Nodes &nodes2 )
{
	struct node_set
	{
		const svn::Node *node1 = nullptr;
		const svn::Node *node2 = nullptr;
	};
	auto table = std::map<std::string, node_set>();

	for( const auto &node : nodes1 )
	{
		const auto path = node.NodePath + '#' + node.NodeAction;
		if( table[path].node1 )
			throw std::runtime_error( "Node existing: " + path );
		table[path].node1 = &node;
	}
	for( const auto &node : nodes2 )
	{
		const auto path = node.NodePath + '#' + node.NodeAction;
		if( table[path].node2 )
			throw std::runtime_error( "Node existing: " + path );
		table[path].node2 = &node;
	}

	auto result = std::stringstream();

	if( nodes1.size() != table.size() )
		result << "	different node size: " << nodes1.size() << " <> " << table.size() << endl;

	for( const auto &pair : table )
	{
		const auto &path = pair.first;
		if( pair.second.node1 && pair.second.node2 )
		{
			auto s = compare_contents( *pair.second.node1, *pair.second.node2 );
			if( s.length() )
				result << "	" << path << endl << s;
		}
		else if( pair.second.node1 )
			result << "	node1 only: " << path << endl;
		else if( pair.second.node2 )
			result << "	node2 only: " << path << endl;
	}

	if( result.str().length() )
		cout << "check rev: " << rev_no << endl << result.str();
}
