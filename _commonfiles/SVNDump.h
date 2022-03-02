#pragma once

//reference: svn.apache.org/repos/asf/subversion/trunk/notes/dump-load-format.txt

#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <filesystem>

namespace svn
{
	namespace fs = std::filesystem;

	using text_data_t = std::vector< uint8_t >;
	using prop_data_t = std::map< std::string, text_data_t >;
	using path_list_t = std::vector< fs::path >;

	struct Revision;
	struct Node
	{
		const Revision *parent;

		std::string	NodePath;
		std::string	NodeKind;
		std::string	NodeAction;
		int         NodeCopyfromRev;
		std::string	NodeCopyfromPath;
		std::string	TextContentMD5;
		std::string	TextContentSHA1;
		std::string	TextCopySourceMD5;
		std::string	TextCopySourceSHA1;
		//Ver3
		bool        TextDelta;
		bool        PropDelta;
		std::string	TextDeltaBaseMD5;
		std::string	TextDeltaBaseSHA1;
		prop_data_t prop;

		size_t      text_offset;
		size_t      text_size;

		Node();
		Node( const Node &r ) = default;
		Node( Node &&r ) noexcept;
		~Node() = default;

		bool operator ==( const Node &right ) const;
		void clear();

		text_data_t get_text_content() const;

	private:
		Node &operator =( const Node &r ) = delete;
		Node &operator =( Node &&r ) = delete;
	};

	using Nodes = std::vector< Node >;

	class Dump;
	struct Revision
	{
		const Dump *parent;
		size_t path_no;

		int number;
		prop_data_t prop;
		Nodes nodes;

		Revision();
		~Revision() = default;
		operator bool() const noexcept { return number >= 0; }

		const fs::path &get_dump_file_path() const;
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
		int	GetMaxRevision() const noexcept { return _MaxRevision; }
		int GetFormatVersion() const noexcept { return _FormatVersion; }
		const std::string &UUID() const noexcept { return _UUID; }
		size_t GetRevisionCount() const noexcept { return _revisions.size(); }
		const fs::path &GetDumpFilePath( size_t path_no ) const noexcept { return _dump_files[path_no]; }

	protected:
		int _MaxRevision;
		int _FormatVersion;
		std::string _UUID;
		Revisions _revisions;
		path_list_t _dump_files;

		std::string GetLine( std::ifstream &in );
		bool GetKeyValue( std::ifstream &in, std::string &key, std::string &value );
		text_data_t GetTextData( std::ifstream &in, size_t length );
		prop_data_t GetProperty( std::ifstream &in, size_t length );
		size_t GetTextContentOffset( std::ifstream &in, size_t length );
		int CheckFormatVersion( std::ifstream &in );
		void ParseRevision( std::ifstream &in, size_t path_no );
	};
}
