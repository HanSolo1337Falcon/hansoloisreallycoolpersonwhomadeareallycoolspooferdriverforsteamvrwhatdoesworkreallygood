// MiniJson.h - dependency-free JSON reader for the VRSpoofer driver.
// It only has to parse config files written by the VRSpoofer GUI, but it is a
// complete reader for objects, arrays, strings, numbers, bools and null so a
// hand-edited config still behaves predictably.
#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>

namespace mj
{
	enum class Type { Null, Bool, Number, String, Array, Object };

	class Value
	{
	public:
		Type type = Type::Null;
		bool boolean = false;
		double number = 0.0;
		std::string str;
		std::vector<Value> array;
		std::map<std::string, Value> object;

		bool IsNull() const { return type == Type::Null; }

		bool Has( const std::string &key ) const
		{
			return type == Type::Object && object.find( key ) != object.end();
		}

		// Returns a shared Null value (never a dangling reference) for missing
		// keys, so chained lookups like cfg["hmd"]["modelNumber"] are safe.
		const Value &operator[]( const std::string &key ) const
		{
			static const Value kNull;
			if ( type != Type::Object )
				return kNull;
			auto it = object.find( key );
			return it == object.end() ? kNull : it->second;
		}

		const Value &operator[]( size_t index ) const
		{
			static const Value kNull;
			if ( type != Type::Array || index >= array.size() )
				return kNull;
			return array[ index ];
		}

		size_t Size() const { return type == Type::Array ? array.size() : 0; }

		std::string AsString( const std::string &fallback = "" ) const
		{
			if ( type == Type::String ) return str;
			if ( type == Type::Bool )   return boolean ? "true" : "false";
			if ( type == Type::Number )
			{
				char buf[ 64 ];
				if ( number == std::floor( number ) && std::fabs( number ) < 1e15 )
					snprintf( buf, sizeof( buf ), "%lld", (long long)number );
				else
					snprintf( buf, sizeof( buf ), "%g", number );
				return buf;
			}
			return fallback;
		}

		double AsNumber( double fallback = 0.0 ) const
		{
			if ( type == Type::Number ) return number;
			if ( type == Type::Bool )   return boolean ? 1.0 : 0.0;
			if ( type == Type::String )
			{
				try { return std::stod( str ); }
				catch ( ... ) { return fallback; }
			}
			return fallback;
		}

		float AsFloat( float fallback = 0.0f ) const { return (float)AsNumber( fallback ); }
		int   AsInt( int fallback = 0 ) const        { return (int)AsNumber( fallback ); }

		bool AsBool( bool fallback = false ) const
		{
			if ( type == Type::Bool )   return boolean;
			if ( type == Type::Number ) return number != 0.0;
			if ( type == Type::String ) return str == "true" || str == "1";
			return fallback;
		}
	};

	class Parser
	{
	public:
		// Returns false and leaves out untouched on malformed input.
		static bool Parse( const std::string &text, Value &out )
		{
			Parser p( text );
			p.SkipWhitespace();
			Value v;
			if ( !p.ParseValue( v ) )
				return false;
			p.SkipWhitespace();
			if ( p.m_pos != text.size() )
				return false;   // trailing garbage
			out = std::move( v );
			return true;
		}

	private:
		explicit Parser( const std::string &text ) : m_text( text ) {}

		static const int kMaxDepth = 64;

		const std::string &m_text;
		size_t m_pos = 0;
		int m_depth = 0;

		bool Eof() const  { return m_pos >= m_text.size(); }
		char Peek() const { return m_pos < m_text.size() ? m_text[ m_pos ] : '\0'; }

		void SkipWhitespace()
		{
			while ( !Eof() )
			{
				char c = m_text[ m_pos ];
				if ( c == ' ' || c == '\t' || c == '\n' || c == '\r' )
					m_pos++;
				else
					break;
			}
		}

		bool Literal( const char *lit )
		{
			size_t n = strlen( lit );
			if ( m_text.compare( m_pos, n, lit ) != 0 )
				return false;
			m_pos += n;
			return true;
		}

		bool ParseValue( Value &out )
		{
			if ( m_depth > kMaxDepth )
				return false;
			SkipWhitespace();
			if ( Eof() )
				return false;

			switch ( Peek() )
			{
			case '{':
				return ParseObject( out );
			case '[':
				return ParseArray( out );
			case '"':
			{
				std::string s;
				if ( !ParseString( s ) )
					return false;
				out.type = Type::String;
				out.str = std::move( s );
				return true;
			}
			case 't':
				if ( !Literal( "true" ) ) return false;
				out.type = Type::Bool;
				out.boolean = true;
				return true;
			case 'f':
				if ( !Literal( "false" ) ) return false;
				out.type = Type::Bool;
				out.boolean = false;
				return true;
			case 'n':
				if ( !Literal( "null" ) ) return false;
				out.type = Type::Null;
				return true;
			default:
				return ParseNumber( out );
			}
		}

		bool ParseObject( Value &out )
		{
			m_pos++;   // consume '{'
			m_depth++;
			out.type = Type::Object;
			out.object.clear();

			SkipWhitespace();
			if ( Peek() == '}' )
			{
				m_pos++;
				m_depth--;
				return true;
			}

			for ( ;; )
			{
				SkipWhitespace();
				std::string key;
				if ( !ParseString( key ) )
					return false;
				SkipWhitespace();
				if ( Peek() != ':' )
					return false;
				m_pos++;

				Value child;
				if ( !ParseValue( child ) )
					return false;
				out.object[ key ] = std::move( child );

				SkipWhitespace();
				if ( Peek() == ',' ) { m_pos++; continue; }
				if ( Peek() == '}' ) { m_pos++; m_depth--; return true; }
				return false;
			}
		}

		bool ParseArray( Value &out )
		{
			m_pos++;   // consume '['
			m_depth++;
			out.type = Type::Array;
			out.array.clear();

			SkipWhitespace();
			if ( Peek() == ']' )
			{
				m_pos++;
				m_depth--;
				return true;
			}

			for ( ;; )
			{
				Value child;
				if ( !ParseValue( child ) )
					return false;
				out.array.push_back( std::move( child ) );

				SkipWhitespace();
				if ( Peek() == ',' ) { m_pos++; continue; }
				if ( Peek() == ']' ) { m_pos++; m_depth--; return true; }
				return false;
			}
		}

		bool ParseString( std::string &out )
		{
			if ( Peek() != '"' )
				return false;
			m_pos++;
			out.clear();

			while ( !Eof() )
			{
				char c = m_text[ m_pos++ ];
				if ( c == '"' )
					return true;

				if ( c != '\\' )
				{
					out.push_back( c );
					continue;
				}

				if ( Eof() )
					return false;

				char esc = m_text[ m_pos++ ];
				switch ( esc )
				{
				case '"':  out.push_back( '"' );  break;
				case '\\': out.push_back( '\\' ); break;
				case '/':  out.push_back( '/' );  break;
				case 'b':  out.push_back( '\b' ); break;
				case 'f':  out.push_back( '\f' ); break;
				case 'n':  out.push_back( '\n' ); break;
				case 'r':  out.push_back( '\r' ); break;
				case 't':  out.push_back( '\t' ); break;
				case 'u':
				{
					uint32_t cp = 0;
					if ( !ReadHex4( cp ) )
						return false;
					// Combine a surrogate pair when the low half follows.
					if ( cp >= 0xD800 && cp <= 0xDBFF &&
					     m_pos + 1 < m_text.size() &&
					     m_text[ m_pos ] == '\\' && m_text[ m_pos + 1 ] == 'u' )
					{
						size_t save = m_pos;
						m_pos += 2;
						uint32_t low = 0;
						if ( ReadHex4( low ) && low >= 0xDC00 && low <= 0xDFFF )
							cp = 0x10000 + ( ( cp - 0xD800 ) << 10 ) + ( low - 0xDC00 );
						else
							m_pos = save;
					}
					AppendUtf8( out, cp );
					break;
				}
				default:
					return false;
				}
			}
			return false;   // unterminated string
		}

		bool ReadHex4( uint32_t &out )
		{
			if ( m_pos + 4 > m_text.size() )
				return false;
			out = 0;
			for ( int i = 0; i < 4; i++ )
			{
				char c = m_text[ m_pos++ ];
				out <<= 4;
				if ( c >= '0' && c <= '9' )      out |= (uint32_t)( c - '0' );
				else if ( c >= 'a' && c <= 'f' ) out |= (uint32_t)( c - 'a' + 10 );
				else if ( c >= 'A' && c <= 'F' ) out |= (uint32_t)( c - 'A' + 10 );
				else return false;
			}
			return true;
		}

		static void AppendUtf8( std::string &out, uint32_t cp )
		{
			if ( cp < 0x80 )
			{
				out.push_back( (char)cp );
			}
			else if ( cp < 0x800 )
			{
				out.push_back( (char)( 0xC0 | ( cp >> 6 ) ) );
				out.push_back( (char)( 0x80 | ( cp & 0x3F ) ) );
			}
			else if ( cp < 0x10000 )
			{
				out.push_back( (char)( 0xE0 | ( cp >> 12 ) ) );
				out.push_back( (char)( 0x80 | ( ( cp >> 6 ) & 0x3F ) ) );
				out.push_back( (char)( 0x80 | ( cp & 0x3F ) ) );
			}
			else
			{
				out.push_back( (char)( 0xF0 | ( cp >> 18 ) ) );
				out.push_back( (char)( 0x80 | ( ( cp >> 12 ) & 0x3F ) ) );
				out.push_back( (char)( 0x80 | ( ( cp >> 6 ) & 0x3F ) ) );
				out.push_back( (char)( 0x80 | ( cp & 0x3F ) ) );
			}
		}

		bool ParseNumber( Value &out )
		{
			const char *begin = m_text.c_str() + m_pos;
			char *end = nullptr;
			double d = strtod( begin, &end );
			if ( end == begin )
				return false;
			m_pos += (size_t)( end - begin );
			out.type = Type::Number;
			out.number = d;
			return true;
		}
	};
}
