#pragma once

#include <fstream>
#include <string>
#include <vector>
#include <map>

namespace svn
{
	struct Content
	{
		using text_data_t = std::vector< uint8_t >;
		using prop_data_t = std::map< text_data_t, text_data_t >;

		std::string	NodePath;
		std::string	NodeKind;
		std::string	NodeAction;
		int			NodeCopyfromRev;
		std::string	NodeCopyfromPath;
		std::string	TextContentMD5;
		std::string	TextContentSHA1;
		std::string	TextCopySourceMD5;
		std::string	TextCopySourceSHA1;
		prop_data_t prop;
		text_data_t text;

		Content();
		~Content() = default;

		bool operator ==( const Content &right ) const;
		void Clear();
	};

	struct Revision
	{
		int number;
		std::vector< Content > contents;

		Revision():number( -1 ) {}
		~Revision() = default;
		operator bool() const noexcept { return number >= 0; }
	};

	using Revisions = std::vector< Revision >;

	class Dump
	{
	public:
		Dump();
		Dump( const char *path );
		~Dump() = default;

		operator bool() const noexcept { return _MaxRevision >= 0; }
		bool operator ==( const Dump &r ) const noexcept { return UUID() == r.UUID(); }
		bool operator !=( const Dump &r ) const noexcept { return !this->operator==( r ); }
		Revision &operator []( int number );

		Revisions::iterator begin() { return _revisions.begin(); }
		Revisions::iterator end() { return _revisions.end(); }
		const Revisions::const_iterator cbegin() const { return _revisions.cbegin(); }
		const Revisions::const_iterator cend() const { return _revisions.cend(); }

		bool Init( const char *path );
		const std::string &UUID() const noexcept { return _UUID; }
		int	GetMaxRevision() const noexcept { return _MaxRevision; }

	protected:
		int _MaxRevision;
		std::string _UUID;
		Revisions _revisions;

		std::string GetLine( std::ifstream &in );
		bool GetKeyValue( std::ifstream &in, std::string &key, std::string &value );
		Content::prop_data_t GetPropContent( std::ifstream &in, size_t length );
		Content::text_data_t GetTextContent( std::ifstream &in, size_t length );
		bool CheckFormatVersion( std::ifstream &in );
		void ParseRevision( std::ifstream &in );
	};
}
