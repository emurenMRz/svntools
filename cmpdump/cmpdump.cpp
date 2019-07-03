#include "SVNDump.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

using std::cout;
using std::cerr;
using std::endl;

int main( int argc, char *argv[] )
{
	if( argc != 3 )
	{
		cout << "<USAGE> cmpdump repos1.dump repos2.dump" << endl;
		return 1;
	}

	try
	{
		auto dmp1 = svn::Dump( argv[1] );
		if( !dmp1 )
			return 1;
		cout << argv[1] << ": max revision number: " << dmp1.GetMaxRevision() << ", format version: " << dmp1.GetFormatVersion() << endl;

		auto dmp2 = svn::Dump( argv[2] );
		if( !dmp2 )
			return 1;
		cout << argv[2] << ": max revision number: " << dmp2.GetMaxRevision() << ", format version: " << dmp2.GetFormatVersion() << endl;

		if( dmp1 != dmp2 )
		{
			cerr << "different UUID: " << dmp1.UUID() << " != " << dmp2.UUID() << endl;
			return 1;
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

			auto first = true;
			for( auto no = size_t( 0 ); no < rev1.nodes.size(); ++no )
				if( !( rev1.nodes[no] == rev2.nodes[no] ) )
				{
					if( first )
					{
						cout << "check rev: " << i << endl;
						first = false;
					}
					cout << "	different contents no: " << no << endl;
				}
		}
	}
	catch( const std::exception &e )
	{
		cerr << e.what() << endl;
	}

	cout << "complete." << endl;

	return 0;
}
