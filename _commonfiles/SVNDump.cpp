#include "SVNDump.h"
#include <sstream>

svn::Content::Content()
	: NodeCopyfromRev( 0 )
{}

bool svn::Content::operator ==( const Content &right ) const
{
	return NodePath == right.NodePath
		&& NodeKind == right.NodeKind
		&& NodeAction == right.NodeAction
		&& NodeCopyfromRev == right.NodeCopyfromRev
		&& NodeCopyfromPath == right.NodeCopyfromPath
		&& TextContentMD5 == right.TextContentMD5
		&& TextContentSHA1 == right.TextContentSHA1
		&& TextCopySourceMD5 == right.TextCopySourceMD5
		&& TextCopySourceSHA1 == right.TextCopySourceSHA1
		&& prop == right.prop
		&& text == right.text;
}

void svn::Content::Clear()
{
	NodePath.clear();
	NodeKind.clear();
	NodeAction.clear();
	NodeCopyfromRev = 0;
	NodeCopyfromPath.clear();
	TextContentMD5.clear();
	TextContentSHA1.clear();
	TextCopySourceMD5.clear();
	TextCopySourceSHA1.clear();
	prop.clear();
	text.clear();
}

///////////////////////////////////////////////////////////////////////

svn::Dump::Dump()
	: _MaxRevision( -1 )
{}

svn::Dump::Dump( const char *path )
	: _MaxRevision( -1 )
{
	Init( path );
}

svn::Revision &svn::Dump::operator []( int number )
{
	static auto empty = svn::Revision();
	for( auto &it : _revisions )
		if( it.number == number )
			return it;
	return empty;
}

bool svn::Dump::Init( const char *path )
{
	auto in = std::ifstream( path, std::ios_base::binary );
	if( !CheckFormatVersion( in ) )
		return false;

	auto line = std::string();
	if( !GetKeyValue( in, line, _UUID ) || line != "UUID" )
		return false;

	_MaxRevision = -1;
	_revisions.clear();

	ParseRevision( in );

	for( const auto &it : _revisions )
		if( it.number > _MaxRevision )
			_MaxRevision = it.number;

	return _MaxRevision >= 0;
}

std::string svn::Dump::GetLine( std::ifstream &in )
{
	auto line = std::string();
	while( !in.eof() )
	{
		getline( in, line );
		if( !line.empty() )
			break;
	}
	return line;
}

bool svn::Dump::GetKeyValue( std::ifstream &in, std::string &key, std::string &value )
{
	auto line = GetLine( in );
	if( line.empty() )
		return false;

	auto pos = line.find_first_of( ':' );
	if( pos == std::string::npos )
		throw std::runtime_error( "not key/value line: " + line );

	try
	{
		key = line.substr( 0, pos );
		value = line.substr( line.find_first_not_of( " \t", pos + 1 ) );
	}
	catch( ... )
	{
		value.clear();
	}

	return true;
}

svn::Content::prop_data_t svn::Dump::GetPropContent( std::ifstream &in, size_t length )
{
	if( in.eof() )
		throw std::runtime_error( "end of file." );

	auto ch = in.get();
	while( ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' )
		ch = in.get();
	in.unget();

	auto props = Content::prop_data_t();
	auto key = Content::text_data_t();

	if( length )
		for( ;; )
		{
			auto line = GetLine( in );
			if( line.empty() )
				throw std::runtime_error( "not found PROPS-END" );

			if( line == "PROPS-END" )
				break;

			auto type = line[0];
			if( type != 'K' && type != 'V' )
				throw std::runtime_error( "unknown Prop Header: " + line );

			line.erase( 0, 2 );

			auto size = strtoul( line.c_str(), nullptr, 10 );
			if( type == 'K' )
			{
				key = GetTextContent( in, size );
				continue;
			}

			auto value = GetTextContent( in, size );
			props.insert( Content::prop_data_t::value_type( key, value ) );
		}

	return props;
}

svn::Content::text_data_t svn::Dump::GetTextContent( std::ifstream &in, size_t length )
{
	if( in.eof() )
		throw std::runtime_error( "end of file." );

	auto ch = in.get();
	while( ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' )
		ch = in.get();
	in.unget();

	auto content = Content::text_data_t( length );
	#if defined(SKIP_READ_CONTENT)
	in.seekg( length, std::ios_base::cur );
	#else
	in.read( (char * )content.data(), length );
	#endif

	return content;
}

bool svn::Dump::CheckFormatVersion( std::ifstream &in )
{
	if( !in )
		return false;

	auto key = std::string();
	auto value = std::string();

	if( !GetKeyValue( in, key, value ) )
		throw std::runtime_error( "not found format-version." );

	if( key != "SVN-fs-dump-format-version" )
		throw std::runtime_error( "unknown format: " + key );

	if( strtoul( value.c_str(), NULL, 10 ) != 2 )
		throw std::runtime_error( "unknown version: " + value );

	return true;
}

void svn::Dump::ParseRevision( std::ifstream &in )
{
	auto key = std::string();
	auto value = std::string();
	auto rev = Revision();
	auto cnt = Content();
	auto Prop_content_length = 0;
	auto Text_content_length = 0;

	while( GetKeyValue( in, key, value ) )
	{
		if( key == "Revision-number" )
		{
			if( rev.number >= 0 )
				_revisions.push_back( rev );

			rev.number = strtoul( value.c_str(), NULL, 10 );
			rev.contents.clear();
		}
		else if( rev.number >= 0 )
		{
			if( key == "Prop-content-length" )
				Prop_content_length = strtoul( value.c_str(), NULL, 10 );
			else if( key == "Node-path" )
				cnt.NodePath = value;
			else if( key == "Node-kind" )
				cnt.NodeKind = value;
			else if( key == "Node-action" )
				cnt.NodeAction = value;
			else if( key == "Node-copyfrom-rev" )
				cnt.NodeCopyfromRev = strtoul( value.c_str(), NULL, 10 );
			else if( key == "Node-copyfrom-path" )
				cnt.NodeCopyfromPath = value;
			else if( key == "Text-content-length" )
				Text_content_length = strtoul( value.c_str(), NULL, 10 );
			else if( key == "Text-content-md5" )
				cnt.TextContentMD5 = value;
			else if( key == "Text-content-sha1" )
				cnt.TextContentSHA1 = value;
			else if( key == "Text-copy-source-md5" )
				cnt.TextCopySourceMD5 = value;
			else if( key == "Text-copy-source-sha1" )
				cnt.TextCopySourceSHA1 = value;
			else if( key == "Content-length" )
			{
				auto Content_length = strtoul( value.c_str(), NULL, 10 );

				if( Prop_content_length + Text_content_length != Content_length )
				{
					auto ss = std::stringstream();
					ss << "revision[" << rev.number << "] illiegal content length: " << Prop_content_length << " + " << Text_content_length << " != " << Content_length;
					throw std::runtime_error( ss.str() );
				}

				try
				{
					cnt.prop = GetPropContent( in, Prop_content_length );
					cnt.text = GetTextContent( in, Text_content_length );
				}
				catch( const std::exception &e )
				{
					auto ss = std::stringstream();
					ss << "revision[" << rev.number << "] failed read content: " << e.what();
					throw std::runtime_error( ss.str() );
				}

				rev.contents.push_back( cnt );

				cnt.Clear();
				Prop_content_length = Text_content_length = 0;
			}
			else
			{
				auto ss = std::stringstream();
				ss << "revision[" << rev.number << "] unknown key: " << key << ": " << value;
				throw std::runtime_error( ss.str() );
			}
		}
		else
		{
			auto ss = std::stringstream();
			ss << "revision[" << rev.number << "] illiegal format: " << key << ": " << value;
			throw std::runtime_error( ss.str() );
		}
	}

	if( rev.number >= 0 )
		_revisions.push_back( rev );
}
