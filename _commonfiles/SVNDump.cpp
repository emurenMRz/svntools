#include "SVNDump.h"
#include <sstream>

svn::Node::Node()
	: NodeCopyfromRev( 0 )
	, TextDelta( false )
	, PropDelta( false )
{}

svn::Node::Node( Node &&r ) noexcept
	: NodePath( std::move( r.NodePath ) )
	, NodeKind( std::move( r.NodeKind ) )
	, NodeAction( std::move( r.NodeAction ) )
	, NodeCopyfromRev( r.NodeCopyfromRev )
	, NodeCopyfromPath( std::move( r.NodeCopyfromPath ) )
	, TextContentMD5( std::move( r.TextContentMD5 ) )
	, TextContentSHA1( std::move( r.TextContentSHA1 ) )
	, TextCopySourceMD5( std::move( r.TextCopySourceMD5 ) )
	, TextCopySourceSHA1( std::move( r.TextCopySourceSHA1 ) )
	, TextDelta( r.TextDelta )
	, PropDelta( r.PropDelta )
	, TextDeltaBaseMD5( std::move( r.TextDeltaBaseMD5 ) )
	, TextDeltaBaseSHA1( std::move( r.TextDeltaBaseSHA1 ) )
	, prop( std::move( r.prop ) )
	, text( std::move( r.text ) )
{
	r.NodeCopyfromRev = 0;
	r.TextDelta = false;
	r.PropDelta = false;
}

bool svn::Node::operator ==( const Node &right ) const
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
		&& TextDelta == right.TextDelta
		&& PropDelta == right.PropDelta
		&& TextDeltaBaseMD5 == right.TextDeltaBaseMD5
		&& TextDeltaBaseSHA1 == right.TextDeltaBaseSHA1
		&& prop == right.prop
		&& text == right.text;
}

void svn::Node::Clear()
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
	TextDelta = false;
	PropDelta = false;
	TextDeltaBaseMD5.clear();
	TextDeltaBaseSHA1.clear();
	prop.clear();
	text.clear();
}

///////////////////////////////////////////////////////////////////////

svn::Dump::Dump()
	: _MaxRevision( -1 )
	, _FormatVersion( 0 )
{}

svn::Dump::Dump( const char *path )
	: _MaxRevision( -1 )
	, _FormatVersion( 0 )
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
	auto version = CheckFormatVersion( in );

	if( !version )
		return false;

	auto line = std::string();
	if( !GetKeyValue( in, line, _UUID ) || line != "UUID" )
		return false;

	_MaxRevision = -1;
	_FormatVersion = version;
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

svn::text_data_t svn::Dump::GetTextData( std::ifstream &in, size_t length )
{
	auto content = text_data_t( length );
	in.read( reinterpret_cast< char * >( content.data() ), length );
	if( size_t( in.gcount() ) < length )
		throw std::out_of_range( "less than the requested size." );
	return content;
}

svn::prop_data_t svn::Dump::GetProperty( std::ifstream &in, size_t length )
{
	if( !length )
		return prop_data_t();

	if( in.eof() )
		throw std::runtime_error( "end of file." );

	auto props = prop_data_t();
	auto key = text_data_t();
	auto content = GetTextData( in, length );

	for( auto head = content.data(); head < content.data() + content.size(); )
	{
		//key
		auto tail = head;
		while( *tail != '\n' )
			++tail;
		*tail = '\0';

		if( std::equal( head, tail, "PROPS-END" ) )
		{
			if( tail - content.data() != content.size() - 1 )
				throw std::runtime_error( "abnormal property size." );
			break;
		}

		auto type = *head;
		if( *head != 'K' && *head != 'V' && *head != 'D' )
			throw std::runtime_error( "unknown property type: " + std::string( head, tail ) );

		auto size = strtoul( reinterpret_cast< char * >( head + 2 ), nullptr, 10 );

		//value
		head = tail + 1;
		tail = head + size;
		if( *tail != '\n' )
			throw std::out_of_range( "abnormal property size: " + std::string( head, tail ) );

		if( type == 'K' )
			key = text_data_t( head, tail );
		else if( type == 'V' )
			props.insert( prop_data_t::value_type( key, text_data_t( head, tail ) ) );
		else
			props.insert( prop_data_t::value_type( text_data_t( head, tail ), text_data_t( '*' ) ) );
		head = tail + 1;
	}

	return props;
}

svn::text_data_t svn::Dump::GetTextContent( std::ifstream &in, size_t length )
{
	if( !length )
		return text_data_t();

	if( in.eof() )
		throw std::runtime_error( "end of file." );

#if defined(SKIP_READ_CONTENT)
	auto content = text_data_t( length );
	in.seekg( length, std::ios_base::cur );
	return content;
#else
	return GetTextData( in, length );
#endif
}

int svn::Dump::CheckFormatVersion( std::ifstream &in )
{
	if( !in )
		return 0;

	auto key = std::string();
	auto value = std::string();

	if( !GetKeyValue( in, key, value ) )
		throw std::runtime_error( "not found format-version." );

	if( key != "SVN-fs-dump-format-version" )
		throw std::runtime_error( "unknown format: " + key );

	auto version = strtoul( value.c_str(), NULL, 10 );
	if( version < 2 || version > 3 )
		throw std::runtime_error( "unsupport version: " + value );

	return version;
}

void svn::Dump::ParseRevision( std::ifstream &in )
{
	auto key = std::string();
	auto value = std::string();
	auto rev = Revision();
	auto node = Node();
	auto Prop_content_length = 0;
	auto Text_content_length = 0;

	enum ContextType
	{ Root, Revision, Node };
	auto parse_context = ContextType::Root;

	while( GetKeyValue( in, key, value ) )
	{
		if( key == "Revision-number" )
		{
			if( rev.number >= 0 )
			{
				if( !node.NodePath.empty() )
					rev.nodes.emplace_back( std::move( node ) );
				_revisions.push_back( rev );
			}

			rev.number = strtoul( value.c_str(), NULL, 10 );
			rev.prop.clear();
			rev.nodes.clear();
			parse_context = ContextType::Revision;
		}
		else if( rev.number >= 0 )
		{
			if( key == "Node-path" )
			{
				if( !node.NodePath.empty() )
					rev.nodes.emplace_back( std::move( node ) );
				node.NodePath = value;
				parse_context = ContextType::Node;
			}
			else if( key == "Node-kind" )
				node.NodeKind = value;
			else if( key == "Node-action" )
				node.NodeAction = value;

			else if( key == "Node-copyfrom-rev" )
				node.NodeCopyfromRev = strtoul( value.c_str(), NULL, 10 );
			else if( key == "Node-copyfrom-path" )
				node.NodeCopyfromPath = value;
			else if( key == "Text-copy-source-md5" )
				node.TextCopySourceMD5 = value;
			else if( key == "Text-copy-source-sha1" )
				node.TextCopySourceSHA1 = value;

			else if( key == "Text-content-md5" )
				node.TextContentMD5 = value;
			else if( key == "Text-content-sha1" )
				node.TextContentSHA1 = value;

			else if( key == "Text-delta" )
				node.TextDelta = value == "true";
			else if( key == "Prop-delta" )
				node.PropDelta = value == "true";
			else if( key == "Text-delta-base-md5" )
				node.TextDeltaBaseMD5 = value;
			else if( key == "Text-delta-base-sha1" )
				node.TextDeltaBaseSHA1 = value;

			else if( key == "Text-content-length" )
				Text_content_length = strtoul( value.c_str(), NULL, 10 );
			else if( key == "Prop-content-length" )
				Prop_content_length = strtoul( value.c_str(), NULL, 10 );
			else if( key == "Content-length" )
			{
				auto Content_length = strtoul( value.c_str(), NULL, 10 );

				if( Prop_content_length + Text_content_length != Content_length )
				{
					auto ss = std::stringstream();
					ss << "revision[" << rev.number << "] illiegal content length: " << Prop_content_length << " + " << Text_content_length << " != " << Content_length;
					throw std::runtime_error( ss.str() );
				}

				auto empty_line = in.get();
				if( empty_line != '\n' )
				{
					auto ss = std::stringstream();
					ss << "revision[" << rev.number << "] empty line is required: " << empty_line;
					throw std::runtime_error( ss.str() );
				}

				//read body
				try
				{
					if( parse_context == ContextType::Revision )
						rev.prop = GetProperty( in, Prop_content_length );
					else
					{
						node.prop = GetProperty( in, Prop_content_length );
						node.text = GetTextContent( in, Text_content_length );
					}
				}
				catch( const std::exception &e )
				{
					auto ss = std::stringstream();
					ss << "revision[" << rev.number << "] failed read content: " << e.what();
					throw std::runtime_error( ss.str() );
				}

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
	{
		if( !node.NodePath.empty() )
			rev.nodes.emplace_back( std::move( node ) );
		_revisions.push_back( rev );
	}
}
