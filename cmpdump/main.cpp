#include <iostream>
#include <string>

using std::cout;
using std::cerr;
using std::endl;

bool dump_compare( const char *dump1_path, const char *dump2_path );
bool dump_checkout( const char *dump_path, int target_revision );

bool subcmd_compare( int argc, char *argv[] )
{
	auto arg_no = 2;
	for( ; arg_no < argc && argv[arg_no][0] == '-'; ++arg_no )
	{
		const auto op_cmd = &argv[arg_no][1];
		switch( *op_cmd )
		{
		default:
			throw std::invalid_argument( std::string( "unknown option: " ) + *op_cmd );
		}
	}

	if( arg_no + 1 >= argc )
	{
		cout << "<compare: usage> svndumptool compare DUMP1_PATH DUMP2_PATH" << endl;
		return false;
	}

	return dump_compare( argv[arg_no], argv[arg_no + 1] );
}

bool subcmd_checkout( int argc, char *argv[] )
{
	auto arg_no = 2;
	auto revision_no = -1;
	for( ; arg_no < argc && argv[arg_no][0] == '-'; ++arg_no )
	{
		const auto op_cmd = &argv[arg_no][1];
		switch( *op_cmd )
		{
		case 'r':
			if( ++arg_no >= argc )
				throw std::invalid_argument( std::string( "Insufficient arguments." ) );
			revision_no = strtoul( argv[arg_no], nullptr, 10 );
			break;

		case '-':
			{
				auto fullname_subcmd = std::string( op_cmd + 1 );
				if( fullname_subcmd == "revision" )
				{
					if( ++arg_no >= argc )
						throw std::invalid_argument( std::string( "Insufficient arguments." ) );
					revision_no = strtoul( argv[arg_no], nullptr, 10 );
				}
				else
					throw std::invalid_argument( std::string( "unknown option: " ) + fullname_subcmd );
			}
			break;

		default:
			throw std::invalid_argument( std::string( "unknown option: " ) + *op_cmd );
		}
	}

	if( arg_no >= argc )
	{
		cout << "<checkout: usage> svndumptool checkout DUMP_PATH" << endl;
		return false;
	}

	return dump_checkout( argv[arg_no], revision_no );
}

int main( int argc, char *argv[] )
{
	try
	{
		if( argc >= 3 )
		{
			auto cmd = std::string( argv[1] );
			if( cmd == "compare" )
				return subcmd_compare( argc, argv ) ? 0 : 1;
			//else if( cmd == "checkout" )
			//	return subcmd_checkout( argc, argv ) ? 0 : 1;
		}
	}
	catch( const std::exception &e )
	{
		cerr << e.what() << endl;
		return 1;
	}

	cout << "<usage> svndumptool SUBCOMMAND [ARGS & OPTIONS ...]" << endl
		<< "Subversion dumpfile tool."
		<< endl
		<< "Available subcommands:" << endl
		<< "    compare" << endl
		<< "    checkout" << endl
		<< endl;
	return 1;
}
