/************************************************************************************

Filename    :   String_Utils.h
Content     :   String utility functions.
Created     :   May, 2014
Authors     :   J.M.P. van Waveren

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef __STRING_UTILS_H__
#define __STRING_UTILS_H__

#include "OVR_Math.h"
#include "OVR_String.h"
#include "OVR_Array.h"

namespace OVR
{

namespace StringUtils
{
	static const int MAX_PATH_LENGTH	= 1024;

	template < size_t size >
	inline const char * Copy( char (&dest)[size], const char * src )
	{
		size_t length = strlen( src ) + 1;
		strncpy( dest, src, length < size ? length : size );
		dest[size - 1] = '\0';
		return dest;
	}

	inline String ReplaceChar( const char *text, const char charToReplace, const char newChar )
	{
		String result;

		for( UPInt index = 0; text[ index ]  != '\0'; index++ )
		{
			char ch = text[ index ];
			if ( ch == charToReplace )
			{
				ch = newChar;
			}
			result.AppendChar( ch );
		}

		return result;
	}

	template < size_t size >
	inline const char * SPrintf( char (&dest)[size], const char * format, ... )
	{
		va_list args;
		va_start( args, format );
		vsnprintf( dest, size, format, args );
		dest[size - 1] = '\0';
		return dest;
	}

	template < size_t size >
	inline const char * VSPrintf( char (&dest)[size], const char * format, va_list args )
	{
		vsnprintf( dest, size, format, args );
		dest[size - 1] = '\0';
		return dest;
	}

	template < size_t size >
	inline const char * GetCleanPath( char (&dest)[size], const char * path, const char separator = '/' )
	{
		for ( int i = 0; i < (int)size; path++ )
		{
			if ( path[0] == '/' || path[0] == '\\' )
			{
				if ( i == 0 || dest[i - 1] != separator )
				{
					dest[i++] = separator;
				}
			}
			else if ( path[0] == '.' && path[1] == '.' &&
						i > 2 && dest[i - 1] == separator && dest[i - 2] != '.' )
			{
				for ( --i; i > 0 && dest[i - 1] != separator; --i ) {}
				path++;
			}
			else
			{
				dest[i++] = path[0];
				if ( path[0] == '\0' )
				{
					break;
				}
			}
		}
		return dest;
	}

	inline int PathCharCmp( const char a, const char b )
	{
		const char c = tolower( a );
		const char d = tolower( b );
		if ( c == d )
		{
			return 0;
		}
		if ( ( c == '\\' || c == '/' ) && ( d == '\\' || d == '/' ) )
		{
			return 0;
		}
		return c - d;
	}

	template < size_t size >
	inline void GetRelativePath( char (&dest)[size], const char * path, const char * relativeTo, const char separator = '/' )
	{
		int match = 0;
		for ( ; path[match] != '\0' && PathCharCmp( path[match], relativeTo[match] ) == 0; match++ ) {}
		if ( match == 0 )
		{
			Copy( dest, path );
			return;
		}
		int folders = 0;
		for ( int i = match; relativeTo[i] != '\0'; i++ )
		{
			if ( relativeTo[i] == '/' || relativeTo[i] == '\\' )
			{
				folders++;
			}
		}
		int length = 0;
		for ( int i = 0; i < folders && length < (int)size - 4; i++ )
		{
			dest[length++] = '.';
			dest[length++] = '.';
			dest[length++] = separator;
		}
		for ( ; path[match] == '\\' || path[match] == '/'; match++ ) {}
		for ( int i = match; path[i] != '\0' && length < (int)size - 1; i++ )
		{
			dest[length++] = path[i];
		}
		dest[length] = '\0';
	}

	template < size_t size >
	inline void GetFolder( char (&dest)[size], const char * path )
	{
		int nameOffset = 0;
		int length = 0;
		for ( int index = 0; path[index] != '\0' && index < (int)size - 1; index++, length++ )
		{
			dest[index] = path[index];
			if ( path[index] == '/' || path[index] == '\\' || path[index] == ':' )
			{
				nameOffset = index + 1;
			}
		}
		dest[nameOffset] = '\0';
	}

	template < size_t size >
	inline void GetFileName( char (&dest)[size], const char * path )
	{
		int nameOffset = 0;
		int length = 0;
		for ( int index = 0; path[index] != '\0'; index++, length++ )
		{
			if ( path[index] == '/' || path[index] == '\\' || path[index] == ':' )
			{
				nameOffset = index + 1;
			}
		}
		int index = 0;
		for ( ; nameOffset + index < length && index < (int)size - 1; index++ )
		{
			dest[index] = path[nameOffset + index];
		}
		dest[index] = '\0';
	}

	template < size_t size >
	inline void GetFileBase( char (&dest)[size], const char * path )
	{
		int nameOffset = 0;
		int extensionOffset = -1;
		int length = 0;
		for ( int index = 0; path[index] != '\0'; index++, length++ )
		{
			if ( path[index] == '/' || path[index] == '\\' || path[index] == ':' )
			{
				nameOffset = index + 1;
			}
			else if ( path[index] == '.' )
			{
				extensionOffset = index;
			}
		}
		if ( extensionOffset == -1 )
		{
			extensionOffset = length;
		}
		int index = 0;
		for ( ; nameOffset + index < extensionOffset && index < (int)size - 1; index++ )
		{
			dest[index] = path[nameOffset + index];
		}
		dest[index] = '\0';
	}

	template < size_t size >
	inline void GetFileExtension( char (&dest)[size], const char * path )
	{
		int extensionOffset = -1;
		for ( int index = 0; path[index] != '\0'; index++ )
		{
			if ( path[index] == '.' )
			{
				extensionOffset = index + 1;
			}
		}
		int index = 0;
		if ( extensionOffset != -1 )
		{
			for ( ; path[extensionOffset + index] != '\0' && index < (int)size - 1; index++ )
			{
				dest[index] = path[extensionOffset + index];
			}
		}
		dest[index] = '\0';
	}

	template < size_t size >
	inline void SetFileExtension( char (&dest)[size], const char * path, const char * extension )
	{
		int extensionOffset = -1;
		int length = 0;
		for ( int index = 0; path[index] != '\0' && index < (int)size - 1; index++, length++ )
		{
			dest[index] = path[index];
			if ( path[index] == '.' )
			{
				extensionOffset = index;
			}
		}
		if ( length >= (int)size - 1 )
		{
			return;
		}
		if ( extensionOffset == -1 )
		{
			extensionOffset = length;
		}
		dest[extensionOffset++] = '.';
		if ( extension[0] == '.' )
		{
			extension++;
		}
		int index = 0;
		for ( ; extension[index] != '\0' && extensionOffset + index < (int)size - 1; index++ )
		{
			dest[extensionOffset + index] = extension[index];
		}
		dest[extensionOffset + index] = '\0';
	}

	inline String GetCleanPathString( const char * path, const char separator = '/' ) { char buffer[MAX_PATH_LENGTH]; GetCleanPath( buffer, path, separator ); return String( buffer ); }
	inline String GetRelativePathString( const char * path, const char * relativeTo, const char separator = '/' ) { char buffer[MAX_PATH_LENGTH]; GetRelativePath( buffer, path, relativeTo, separator ); return String( buffer ); }
	inline String GetFolderString( const char * path ) { char buffer[MAX_PATH_LENGTH]; GetFolder( buffer, path ); return String( buffer ); }
	inline String GetFileNameString( const char * path ) { char buffer[MAX_PATH_LENGTH]; GetFileName( buffer, path ); return String( buffer ); }
	inline String GetFileBaseString( const char * path ) { char buffer[MAX_PATH_LENGTH]; GetFileBase( buffer, path ); return String( buffer ); }
	inline String GetFileExtensionString( const char * path ) { char buffer[MAX_PATH_LENGTH]; GetFileExtension( buffer, path ); return String( buffer ); }
	inline String SetFileExtensionString( const char * path, const char * extension ) { char buffer[MAX_PATH_LENGTH]; SetFileExtension( buffer, path, extension ); return String( buffer ); }

	// String format functor.
	class Va
	{
	public:

		Va( const char * format, ... )
		{
			va_list args;
			va_start( args, format );
			VSPrintf( buffer, format, args );
		}

		operator const char * () { return buffer; }

	private:
		char buffer[MAX_PATH_LENGTH];
	};

#if defined( _MSC_VER ) && _MSC_VER <= 1700
	// MSVC doesn't fully support C99
	inline float strtof( const char * str, char ** endptr )	{ return (float)strtod( str, endptr ); }
#endif

	//
	// Convert a common type to a string.
	//

	template< typename _type_ > inline String ToString( const _type_ & value ) { return String(); }

	template< typename _type_ > inline String ToString( const _type_ * valueArray, const int count )
	{
		String string = "{";
		for ( int i = 0; i < count; i++ )
		{
			string += ToString( valueArray[i] );
		}
		string += "}";
		return string;
	}

	template< typename _type_ > inline String ToString( const Array< _type_ > & valueArray )
	{
		String string = "{";
		for ( int i = 0; i < valueArray.GetSizeI(); i++ )
		{
			string += ToString( valueArray[i] );
		}
		string += "}";
		return string;
	}

	// specializations

	template<> inline String ToString( const short &          value ) { return String( Va( " %hi", value ) ); }
	template<> inline String ToString( const unsigned short & value ) { return String( Va( " %uhi", value ) ); }
	template<> inline String ToString( const int &            value ) { return String( Va( " %li", value ) ); }
	template<> inline String ToString( const unsigned int &   value ) { return String( Va( " %uli", value ) ); }
	template<> inline String ToString( const float &          value ) { return String( Va( " %f", value ) ); }
	template<> inline String ToString( const double &         value ) { return String( Va( " %f", value ) ); }

	template<> inline String ToString( const Vector2f & value ) { return String( Va( "{ %f %f }", value.x, value.y ) ); }
	template<> inline String ToString( const Vector2d & value ) { return String( Va( "{ %f %f }", value.x, value.y ) ); }
	template<> inline String ToString( const Vector2i & value ) { return String( Va( "{ %d %d }", value.x, value.y ) ); }

	template<> inline String ToString( const Vector3f & value ) { return String( Va( "{ %f %f %f }", value.x, value.y, value.z ) ); }
	template<> inline String ToString( const Vector3d & value ) { return String( Va( "{ %f %f %f }", value.x, value.y, value.z ) ); }
	template<> inline String ToString( const Vector3i & value ) { return String( Va( "{ %d %d %d }", value.x, value.y, value.z ) ); }

	template<> inline String ToString( const Vector4f & value ) { return String( Va( "{ %f %f %f %f }", value.x, value.y, value.z, value.w ) ); }
	template<> inline String ToString( const Vector4d & value ) { return String( Va( "{ %f %f %f %f }", value.x, value.y, value.z, value.w ) ); }
	template<> inline String ToString( const Vector4i & value ) { return String( Va( "{ %d %d %d %d }", value.x, value.y, value.z, value.w ) ); }

	template<> inline String ToString( const Matrix4f & value ) { return String( Va( "{ %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f }", value.M[ 0 ][ 0 ], value.M[ 0 ][ 1 ], value.M[ 0 ][ 2 ], value.M[ 0 ][ 3 ], value.M[ 1 ][ 0 ], value.M[ 1 ][ 1 ], value.M[ 1 ][ 2 ], value.M[ 1 ][ 3 ], value.M[ 2 ][ 0 ], value.M[ 2 ][ 1 ], value.M[ 2 ][ 2 ], value.M[ 2 ][ 3 ], value.M[ 3 ][ 0 ], value.M[ 3 ][ 1 ], value.M[ 3 ][ 2 ], value.M[ 3 ][ 3 ] ) ); }
	template<> inline String ToString( const Matrix4d & value ) { return String( Va( "{ %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f }", value.M[ 0 ][ 0 ], value.M[ 0 ][ 1 ], value.M[ 0 ][ 2 ], value.M[ 0 ][ 3 ], value.M[ 1 ][ 0 ], value.M[ 1 ][ 1 ], value.M[ 1 ][ 2 ], value.M[ 1 ][ 3 ], value.M[ 2 ][ 0 ], value.M[ 2 ][ 1 ], value.M[ 2 ][ 2 ], value.M[ 2 ][ 3 ], value.M[ 3 ][ 0 ], value.M[ 3 ][ 1 ], value.M[ 3 ][ 2 ], value.M[ 3 ][ 3 ] ) ); }

	template<> inline String ToString( const Quatf &    value ) { return String( Va( "{ %f %f %f %f }", value.x, value.y, value.z, value.w ) ); }
	template<> inline String ToString( const Quatd &    value ) { return String( Va( "{ %f %f %f %f }", value.x, value.y, value.z, value.w ) ); }

	template<> inline String ToString( const Planef &   value ) { return String( Va( "{ %f %f %f %f }", value.N.x, value.N.y, value.N.z, value.D ) ); }
	template<> inline String ToString( const Planed &   value ) { return String( Va( "{ %f %f %f %f }", value.N.x, value.N.y, value.N.z, value.D ) ); }

	template<> inline String ToString( const Bounds3f & value ) { return String( Va( "{{ %f %f %f }{ %f %f %f }}", value.b[0].x, value.b[0].y, value.b[0].z, value.b[1].x, value.b[1].y, value.b[1].z ) ); }
	template<> inline String ToString( const Bounds3d & value ) { return String( Va( "{{ %f %f %f }{ %f %f %f }}", value.b[0].x, value.b[0].y, value.b[0].z, value.b[1].x, value.b[1].y, value.b[1].z ) ); }

	//
	// Convert a string to a common type.
	//

	template< typename _type_ > inline size_t StringTo( _type_ & value, const char * string ) { return 0; }

	template< typename _type_ > inline size_t StringTo( _type_ * valueArray, const int count, const char * string )
	{
		size_t length = 0;
		length += strspn( string + length, "{ \t\n\r" );
		for ( int i = 0; i < count; i++ )
		{
			length += StringTo< _type_ >( valueArray[i], string + length );
		}
		length += strspn( string + length, "} \t\n\r" );
		return length;
	}

	template< typename _type_ > inline size_t StringTo( Array< _type_ > & valueArray, const char * string )
	{
		size_t length = 0;
		length += strspn( string + length, "{ \t\n\r" );
		for ( ; ; )
		{
			_type_ value;
			size_t s = StringTo< _type_ >( value, string + length );
			if ( s == 0 ) break;
			valueArray.PushBack( value );
			length += s;
		}
		length += strspn( string + length, "} \t\n\r" );
		return length;
	}

	// specializations

	template<> inline size_t StringTo( short &          value, const char * str ) { char * endptr; value = (short) strtol( str, &endptr, 10 ); return endptr - str; }
	template<> inline size_t StringTo( unsigned short & value, const char * str ) { char * endptr; value = (unsigned short) strtoul( str, &endptr, 10 ); return endptr - str; }
	template<> inline size_t StringTo( int &            value, const char * str ) { char * endptr; value = strtol( str, &endptr, 10 ); return endptr - str; }
	template<> inline size_t StringTo( unsigned int &   value, const char * str ) { char * endptr; value = strtoul( str, &endptr, 10 ); return endptr - str; }
	template<> inline size_t StringTo( float &          value, const char * str ) { char * endptr; value = strtof( str, &endptr ); return endptr - str; }
	template<> inline size_t StringTo( double &         value, const char * str ) { char * endptr; value = strtod( str, &endptr ); return endptr - str; }

	template<> inline size_t StringTo( Vector2f & value, const char * string ) { return StringTo( &value.x, 2, string ); }
	template<> inline size_t StringTo( Vector2d & value, const char * string ) { return StringTo( &value.x, 2, string ); }
	template<> inline size_t StringTo( Vector2i & value, const char * string ) { return StringTo( &value.x, 2, string ); }

	template<> inline size_t StringTo( Vector3f & value, const char * string ) { return StringTo( &value.x, 3, string ); }
	template<> inline size_t StringTo( Vector3d & value, const char * string ) { return StringTo( &value.x, 3, string ); }
	template<> inline size_t StringTo( Vector3i & value, const char * string ) { return StringTo( &value.x, 3, string ); }

	template<> inline size_t StringTo( Vector4f & value, const char * string ) { return StringTo( &value.x, 4, string ); }
	template<> inline size_t StringTo( Vector4d & value, const char * string ) { return StringTo( &value.x, 4, string ); }
	template<> inline size_t StringTo( Vector4i & value, const char * string ) { return StringTo( &value.x, 4, string ); }

	template<> inline size_t StringTo( Matrix4f & value, const char * string ) { return StringTo( &value.M[0][0], 16, string ); }
	template<> inline size_t StringTo( Matrix4d & value, const char * string ) { return StringTo( &value.M[0][0], 16, string ); }

	template<> inline size_t StringTo( Quatf &    value, const char * string ) { return StringTo( &value.x, 4, string ); }
	template<> inline size_t StringTo( Quatd &    value, const char * string ) { return StringTo( &value.x, 4, string ); }

	template<> inline size_t StringTo( Planef &   value, const char * string ) { return StringTo( &value.N.x, 4, string ); }
	template<> inline size_t StringTo( Planed &   value, const char * string ) { return StringTo( &value.N.x, 4, string ); }

	template<> inline size_t StringTo( Bounds3f & value, const char * string ) { return StringTo( value.b, 2, string ); }
	template<> inline size_t StringTo( Bounds3d & value, const char * string ) { return StringTo( value.b, 2, string ); }

} // StringUtils

} // OVR

#endif // !__STRING_UTILS_H__
