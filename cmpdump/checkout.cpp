#include "SVNDump.h"
#include <iostream>
#include <sstream>
#include <filesystem>
#include <windows.h>

using namespace std;

namespace fs = std::filesystem;

struct repos_node_t;
using repos_node_tree = std::map<std::string, repos_node_t>;
using node_array_t = std::vector<const svn::Node *>;

struct repos_node_t
{
	bool is_delete = false;

	std::string mime_type;
	repos_node_tree child;

public:
	int get_rev_no() const;
	void set_delete();

	void set_node( const svn::Node *n );
	void reset_node( const svn::Node *n );
	const svn::Node *get_node( int rev_no = INT_MAX ) const;

	void copyfrom( const repos_node_t &r, int rev_no );

private:
	node_array_t node;
};

int repos_node_t::get_rev_no() const
{ return node.empty() || !( *node.crbegin() )->parent ? -1 : ( *node.crbegin() )->parent->number; }

void repos_node_t::set_delete()
{
	is_delete = true;
	for( auto &&n : child )
		n.second.set_delete();
}

void repos_node_t::set_node( const svn::Node *n )
{
	if( n )
		node.push_back( n );
}

void repos_node_t::reset_node( const svn::Node *n )
{
	if( !n )
		return;
	node.pop_back();
	node.push_back( n );
}

const svn::Node *repos_node_t::get_node( int rev_no ) const
{
	for( auto n = node.crbegin(); n != node.crend(); ++n )
		if( ( *n )->parent && ( *n )->parent->number <= rev_no )
			return *n;
	return nullptr;
}

void repos_node_t::copyfrom( const repos_node_t &r, int rev_no )
{
	child = r.child;
	//for( const auto &n : r.child )
	//{
	//	auto src = n.second;
	//	auto new_node = repos_node_t();
	//	new_node.mime_type = src.mime_type;
	//	new_node.set_node( src.get_node( rev_no ) );
	//	new_node.copyfrom( src, rev_no );
	//	child.insert( repos_node_tree::value_type( n.first, new_node ) );
	//}
}

std::string get_mimetype( const svn::Node &node )
{
	auto find = node.prop.find( "svn:mime-type" );
	if( find == node.prop.end() )
		return std::string();
	return std::string( ( *find ).second.cbegin(), ( *find ).second.cend() );
}

repos_node_tree BuildINodeTree( svn::Dump &dump, int target_revision = -1 )
{
	auto root = repos_node_tree();

	using strarray = std::vector<std::string>;
	auto separate_path = []( std::string path )
	{
		auto v = strarray();
		auto pos = size_t( 0 );
		for( ;; )
		{
			auto end = path.find_first_of( '/', pos );
			if( end == path.npos )
			{
				v.push_back( path.substr( pos ) );
				break;
			}
			v.push_back( path.substr( pos, end - pos ) );
			pos = end + 1;
		}
		return v;
	};

	auto search_inode = [&root]( const strarray sarray )->repos_node_t &
	{
		auto in = &root;
		for( auto i = 0; ; ++i )
		{
			auto &now = ( *in )[sarray[i]];
			if( i >= sarray.size() - 1 )
				return now;
			in = &now.child;
		}
	};

	for( const auto &r : dump )
	{
		for( const auto &n : r.nodes )
		{
			auto action = n.NodeAction;
			auto copyfrom_rev = n.NodeCopyfromRev;

			auto v = separate_path( n.NodePath );
			auto &now = search_inode( v );

			if( action == "add" )
			{
				now.is_delete = false;
				now.set_node( &n );
				now.mime_type = get_mimetype( n );
				if( copyfrom_rev > 0 )
				{
					auto cv = separate_path( n.NodeCopyfromPath );
					auto &cnow = search_inode( cv );

					if( n.NodeKind == "file" )
					{
						if( cnow.get_rev_no() < 0 )
						{
							std::stringstream ss;
							ss << "not found Copyfrom-node[rev:" << r.number << "]" << endl
								<< "  " << n.NodePath << endl
								<< "    <= " << n.NodeCopyfromPath << endl;
							throw std::logic_error( ss.str() );
						}
						if( !n.text_offset && !n.text_size )
							now.reset_node( cnow.get_node( copyfrom_rev ) );
						now.mime_type = cnow.mime_type;
					}
					else
						now.copyfrom( cnow, copyfrom_rev );
				}
			}
			else if( action == "delete" )
				now.set_delete();
			else if( action == "change" )
			{
				if( now.is_delete )
					throw std::logic_error( "delete change" );
				now.set_node( &n );
				if( copyfrom_rev > 0 )
					cout << "change: " << r.number << " <= " << copyfrom_rev << endl;
			}
			else if( action == "replace" )
			{
				if( now.is_delete )
					throw std::logic_error( "delete replace" );
				now.set_node( &n );
				if( copyfrom_rev > 0 )
					cout << "replace: " << r.number << " <= " << copyfrom_rev << endl;
			}
			else
				cerr << "unknown action: " << action << endl;
		}

		if( r.number == target_revision )
			break;
	}

	return root;
}

std::wstring UTF8toWide( const std::string &s )
{
	auto w = std::vector<wchar_t>( MultiByteToWideChar( CP_UTF8, 0, s.c_str(), int( s.size() ), nullptr, 0 ) );
	MultiByteToWideChar( CP_UTF8, 0, s.c_str(), int( s.size() ), w.data(), int( w.size() ) );
	return std::wstring( w.cbegin(), w.cend() );
}

void Export( const fs::path parent, svn::Dump &dump, repos_node_tree &tree, int depth = 0 )
{
	for( auto &&t : tree )
	{
		auto path = parent / UTF8toWide( t.first );
		auto &n = t.second;
		auto node = n.get_node();
		auto is_dir = !node || node->NodeKind == "dir";

		if( !t.second.is_delete )
		{
			for( auto i = 0; i < depth; ++i )
				cout << "  ";
			if( t.second.is_delete )
				cout << "** deleted ** ";
			cout << path.filename().generic_string();

			if( is_dir )
				cout << '/';
			cout << " [rev:" << n.get_rev_no() << ']';
			if( !n.mime_type.empty() )
				cout << ' ' << n.mime_type;
			cout << endl;
		}

		if( is_dir )
		{
			if( !t.second.is_delete )
				fs::create_directories( path );
			Export( path, dump, n.child, depth + 1 );
		}
		else if( node && !t.second.is_delete )
		{
			fs::create_directories( parent );
			auto out = std::ofstream( path, std::ios_base::binary );
			auto content = node->get_text_content();
			out.write( reinterpret_cast< char * >( content.data() ), content.size() );
		}
	}
}

bool dump_checkout( const char *dump_path, int target_revision )
{
	try
	{
		auto dmp = svn::Dump( dump_path );
		if( !dmp )
			return false;

		cout << dump_path << ": max revision number: " << dmp.GetMaxRevision() << ", format version: " << dmp.GetFormatVersion() << endl
			<< "UUID: " << dmp.UUID() << endl
			<< endl;

		if( !dmp )
			return false;

		if( target_revision <= 0 )
			target_revision = -1;

		Export( "root", dmp, BuildINodeTree( dmp, target_revision ) );
	}
	catch( const std::exception &e )
	{
		cerr << e.what() << endl;
	}

	cout << "complete." << endl;
	return true;
}
