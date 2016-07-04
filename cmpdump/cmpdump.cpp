#include <iostream>
#include <fstream>
#include <string>
#include <vector>



///////////////////////////////////////////////////////////////////////////////

class SVNDump
{
public:
	struct Content
	{
		std::string	NodePath;
		std::string	NodeKind;
		std::string	NodeAction;
		int			NodeCopyfromRev;
		std::string	NodeCopyfromPath;
		std::string	TextContentMD5;
		std::string	TextContentSHA1;
		std::string	TextCopySourceMD5;
		std::string	TextCopySourceSHA1;
		std::vector< char >	prop;
		std::vector< char >	text;

		bool SVNDump::Content::operator ==( Content &right )
		{
			return NodePath == right.NodePath
				&& NodeKind == right.NodeKind
				&& NodeAction == right.NodeAction
				&& NodeCopyfromRev == right.NodeCopyfromRev
				&& NodeCopyfromPath == right.NodeCopyfromPath
				&& TextContentMD5 == right.TextContentMD5
				&& TextContentSHA1 == right.TextContentSHA1
				&& TextCopySourceMD5 == right.TextCopySourceMD5
				&& TextCopySourceSHA1 == right.TextCopySourceSHA1;
		}

		void Clear()
		{
			NodePath.clear();
			NodeKind.clear();
			NodeAction.clear();
			NodeCopyfromRev	= 0;
			NodeCopyfromPath.clear();
			TextContentMD5.clear();
			TextContentSHA1.clear();
			TextCopySourceMD5.clear();
			TextCopySourceSHA1.clear();
			prop.clear();
			text.clear();
		}
	};

	struct Revision
	{
		int						number;
		std::vector< Content >	contents;
	};

	typedef	std::vector< Revision >	Revisions;

	SVNDump( const char *path )
		: _IsInit( false )
	{
		Init( path );
	}

	operator bool()
	{
		return _IsInit;
	}

	Revision *	operator []( int number )
	{
		for( Revisions::iterator it = _revisions.begin(); it != _revisions.end(); ++it )
			if( ( *it ).number == number )
				return &*it;
		return NULL;
	}

	bool	Init( const char *path )
	{
		std::ifstream	in( path, std::ios_base::binary );

		_IsInit	= false;

		if( in && CheckFormatVersion( in ) )
		{
			std::string	line;

			if( !GetKeyValue( in, line, _UUID ) || line != "UUID" )
				return false;

			while( ParseRevision( in ) )
				;

			_IsInit	= true;
		}

		return _IsInit;
	}

	const std::string &	UUID()
	{
		return _UUID;
	}

protected:
	bool		_IsInit;
	std::string	_UUID;
	Revisions	_revisions;

	bool GetLine( std::ifstream &in, std::string &line )
	{
		line.clear();

		while( !in.eof() )
		{
			getline( in, line );
			if( !line.empty() )
				break;
		}

		return !line.empty();
	}

	bool GetKeyValue( std::ifstream &in, std::string &key, std::string &value )
	{
		std::string	line;

		if( !GetLine( in, line ) )
			return false;

		std::string::size_type	pos	= line.find_first_of( ':' );
		if( pos == std::string::npos )
		{
			std::cerr << "not key/value line: " << line << std::endl;
			return false;
		}

		key		= line.substr( 0, pos );
		try
		{
			value	= line.substr( line.find_first_not_of( " \t", pos + 1 ) );
		}
		catch( ... )
		{
			value.clear();
		}

		return true;
	}

	bool GetContent( std::ifstream &in, std::vector< char > &data, size_t length )
	{
		data.clear();

		if( in.eof() )
		{
			std::cerr << "end of file." << std::endl;
			return false;
		}

		data.resize( length );
		in.seekg( length, std::ios_base::cur );
		//in.read( &data[0], length );
		in.seekg( 1, std::ios_base::cur );	// prop/text‚ÌI‚í‚è‚É0x0A‚ª“ü‚é‚Ì‚Å“Ç‚Ý”ò‚Î‚·

		return true;
	}

	bool	CheckFormatVersion( std::ifstream &in )
	{
		std::string	key, value;

		if( !GetKeyValue( in, key, value ) )
		{
			std::cerr << "not found format-version." << std::endl;
			return false;
		}

		if( key != "SVN-fs-dump-format-version" )
		{
			std::cerr << "unknown format: " << key << std::endl;
			return false;
		}

		if( strtoul( value.c_str(), NULL, 10 ) != 2 )
		{
			std::cerr << "unknown version: " << value << std::endl;
			return false;
		}

		return true;
	};

	bool	ParseRevision( std::ifstream &in )
	{
		std::string	key, value;

		Revision	rev;
		rev.number	= -1;

		Content		cnt;
		int			Prop_content_length	= 0;
		int			Text_content_length	= 0;
		int			Content_length		= 0;

		for(;;)
		{
			if( !GetKeyValue( in, key, value ) )
				return false;

			if( key == "Revision-number" )
			{
				std::cout << "parse revision: " << value << std::endl;

				if( rev.number >= 0 )
					_revisions.push_back( rev );

				rev.number	= strtoul( value.c_str(), NULL, 10 );
				rev.contents.clear();
			}
			else if( rev.number >= 0 )
			{
				if( key == "Prop-content-length" )
					Prop_content_length	= strtoul( value.c_str(), NULL, 10 );
				else if( key == "Node-path" )
					cnt.NodePath	= value;
				else if( key == "Node-kind" )
					cnt.NodeKind	= value;
				else if( key == "Node-action" )
					cnt.NodeAction	= value;
				else if( key == "Node-copyfrom-rev" )
					cnt.NodeCopyfromRev	= strtoul( value.c_str(), NULL, 10 );
				else if( key == "Node-copyfrom-path" )
					cnt.NodeCopyfromPath	= value;
				else if( key == "Text-content-length" )
					Text_content_length	= strtoul( value.c_str(), NULL, 10 );
				else if( key == "Text-content-md5" )
					cnt.TextContentMD5	= value;
				else if( key == "Text-content-sha1" )
					cnt.TextContentSHA1= value;
				else if( key == "Text-copy-source-md5" )
					cnt.TextCopySourceMD5	= value;
				else if( key == "Text-copy-source-sha1" )
					cnt.TextCopySourceSHA1	= value;
				else if( key == "Content-length" )
				{
					Content_length	= strtoul( value.c_str(), NULL, 10 );

					if( Prop_content_length + Text_content_length != Content_length )
					{
						std::cerr << "illiegal content length: " << Prop_content_length << " + " << Text_content_length << " != " << Content_length << std::endl;
						return false;
					}

					if( !GetContent( in, cnt.prop, Prop_content_length ) )
					{
						std::cerr << "failed read content." << std::endl;
						return false;
					}

					if( !GetContent( in, cnt.text, Text_content_length ) )
					{
						std::cerr << "failed read content." << std::endl;
						return false;
					}

					rev.contents.push_back( cnt );

					cnt.Clear();
					Prop_content_length = Text_content_length = Content_length = 0;
				}
			}
			else
			{
				std::cerr << "illiegal format: " << key << ": " << value << std::endl;
				return false;
			}
		}

		return true;
	}
};



///////////////////////////////////////////////////////////////////////////////

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

	SVNDump	dmp1( argv[1] ), dmp2( argv[2] );

	if( !dmp1 || !dmp2 )
		return 1;

	if( dmp1 != dmp2 )
	{
		cerr << "different UUID: " << dmp1.UUID() << " != " << dmp2.UUID() << endl;
		return 1;
	}

	for( int i = 0; ; ++i )
	{
		SVNDump::Revision	*rev1	= dmp1[i];
		SVNDump::Revision	*rev2	= dmp2[i];

		cout << "check rev: " << i << endl;
		if( !rev1 && !rev2 )
			break;

		if( !rev1 || !rev2 )
		{
			if( !rev1 )
				cout << "	not found revision number in rev1." << endl;
			if( !rev2 )
				cout << "	not found revision number in rev2." << endl;
			continue;
		}

		if( rev1->contents.size() != rev2->contents.size() )
		{
			cout << "	different contents size: " << rev1->contents.size() << " != " << rev2->contents.size() << endl;
			continue;
		}

		for( size_t no = 0; no < rev1->contents.size(); ++no )
			if( !( rev1->contents[no] == rev2->contents[no] ) )
				cout << "	different contents no: " << no << endl;
	}

	cout << "complete." << endl;

	return 0;
}
