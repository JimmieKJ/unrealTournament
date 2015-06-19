/************************************************************************************

Filename    :   VrLocale.cpp
Content     :   Localization and internationalization (i18n) functionality.
Created     :   11/24/2914
Authors     :   Jonathan Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "VrLocale.h"

#include "Kernel/OVR_Array.h"
#include "Android/JniUtils.h"
#include "Android/LogUtils.h"

namespace OVR {

char const *	VrLocale::LOCALIZED_KEY_PREFIX = "@string/";
size_t			VrLocale::LOCALIZED_KEY_PREFIX_LEN = OVR_strlen( LOCALIZED_KEY_PREFIX );
jclass			VrLocale::VrActivityClass;

//==============================
// VrLocale::GetString
// Get's a localized UTF-8-encoded string from the string table.
bool VrLocale::GetString( JNIEnv* jni, jobject activityObject, char const * key, char const * defaultOut, String & out )
{
	if ( jni == NULL )
	{
		DROIDWARN( "OVR_ASSERT", "jni = NULL!" );
	}
	if ( activityObject == NULL )
	{
		DROIDWARN( "OVR_ASSERT", "activityObject = NULL!" );
	}

	//LOG( "Localizing key '%s'", key );
	// if the key doesn't start with KEY_PREFIX then it's not a valid key, just return
	// the key itself as the output text.
	if ( strstr( key, LOCALIZED_KEY_PREFIX ) != key )
	{
		out = defaultOut;
		LOG( "no prefix, localized to '%s'", out.ToCStr() );
		return true;
	}

	char const * realKey = key + LOCALIZED_KEY_PREFIX_LEN;
	//LOG( "realKey = %s", realKey );

#if defined( OVR_OS_ANDROID )
	jmethodID const getLocalizedStringId = ovr_GetMethodID( jni, VrActivityClass, "getLocalizedString", "(Ljava/lang/String;)Ljava/lang/String;" );
	if ( getLocalizedStringId != NULL )
	{
		JavaString keyObj( jni, realKey );
		JavaUTFChars resultStr( jni, static_cast< jstring >( jni->CallObjectMethod( activityObject, getLocalizedStringId, keyObj.GetJString() ) ) );
		if ( !jni->ExceptionOccurred() )
		{
			out = resultStr;
			if ( out.IsEmpty() )
			{
				out = defaultOut;
				LOG( "key not found, localized to '%s'", out.ToCStr() );
				return false;
			}

			//LOG( "localized to '%s'", out.ToCStr() );
			return true;
		}
	}
#else
	OVR_COMPILER_ASSERT( false );	
#endif
	out = "JAVAERROR";
	OVR_ASSERT( false );	// the java code is missing getLocalizedString or an exception occured while calling it
	return false;
}

//==============================
// VrLocale::MakeStringIdFromUTF8
// Turns an arbitray ansi string into a string id.
// - Deletes any character that is not a space, letter or number.
// - Turn spaces into underscores.
// - Ignore contiguous spaces.
String VrLocale::MakeStringIdFromUTF8( char const * str )
{
	enum eLastOutputType
	{
		LO_LETTER,
		LO_DIGIT,
		LO_SPACE,
		LO_MAX
	};
	eLastOutputType lastOutputType = LO_MAX;
	String out = LOCALIZED_KEY_PREFIX;
	char const * ptr = str;
	if ( strstr( str, LOCALIZED_KEY_PREFIX ) == str )
	{
		// skip UTF-8 chars... technically could just += LOCALIZED_KEY_PREFIX_LEN if the key prefix is only ANSI chars...
		for ( size_t i = 0; i < LOCALIZED_KEY_PREFIX_LEN; ++i )
		{
			UTF8Util::DecodeNextChar( &ptr );
		}
	}
	int n = static_cast< int >( UTF8Util::GetLength( ptr ) );
	for ( int i = 0; i < n; ++i )
	{
		uint32_t c = UTF8Util::DecodeNextChar( &ptr );
		if ( ( c >= '0' && c <= '9' ) ) 
		{
			if ( i == 0 )
			{
				// string identifiers in Android cannot start with a number because they
				// are also encoded as Java identifiers, so output an underscore first.
				out.AppendChar( '_' );
			}
			out.AppendChar( c );
			lastOutputType = LO_DIGIT;
		}
		else if ( ( c >= 'a' && c <= 'z' ) )
		{
			// just output the character
			out.AppendChar( c );
			lastOutputType = LO_LETTER;
		}
		else if ( ( c >= 'A' && c <= 'Z' ) )
		{
			// just output the character as lowercase
			out.AppendChar( c + 32 );
			lastOutputType = LO_LETTER;
		}
		else if ( c == 0x20 )
		{
			if ( lastOutputType != LO_SPACE )
			{
				out.AppendChar( '_' );
				lastOutputType = LO_SPACE;
			}
			continue;
		}
		// ignore everything else
	}
	return out;
}

//==============================
// VrLocale::MakeStringIdFromANSI
// Turns an arbitray ansi string into a string id.
// - Deletes any character that is not a space, letter or number.
// - Turn spaces into underscores.
// - Ignore contiguous spaces.
String VrLocale::MakeStringIdFromANSI( char const * str )
{
	enum eLastOutputType
	{
		LO_LETTER,
		LO_DIGIT,
		LO_SPACE,
		LO_PUNCTUATION,
		LO_MAX
	};
	eLastOutputType lastOutputType = LO_MAX;
	String out = LOCALIZED_KEY_PREFIX;
	char const * ptr = strstr( str, LOCALIZED_KEY_PREFIX ) == str ? str + LOCALIZED_KEY_PREFIX_LEN : str;
	int n = OVR_strlen( ptr );
	for ( int i = 0; i < n; ++i )
	{
		unsigned char c = ptr[i];
		if ( ( c >= '0' && c <= '9' ) ) 
		{
			if ( i == 0 )
			{
				// string identifiers in Android cannot start with a number because they
				// are also encoded as Java identifiers, so output an underscore first.
				out.AppendChar( '_' );
			}
			out.AppendChar( c );
			lastOutputType = LO_DIGIT;
		}
		else if ( ( c >= 'a' && c <= 'z' ) )
		{
			// just output the character
			out.AppendChar( c );
			lastOutputType = LO_LETTER;
		}
		else if ( ( c >= 'A' && c <= 'Z' ) )
		{
			// just output the character as lowercase
			out.AppendChar( c + 32 );
			lastOutputType = LO_LETTER;
		}
		else if ( c == 0x20 )
		{
			if ( lastOutputType != LO_SPACE )
			{
				out.AppendChar( '_' );
				lastOutputType = LO_SPACE;
			}
			continue;
		}
		// ignore everything else
	}
	return out;
}

//==============================
// Supports up to 9 arguments and %s format only
String private_GetXliffFormattedString( const String & inXliffStr, ... )
{
	// format spec looks like: %1$s - we expect at least 3 chars after %
	const int MIN_NUM_EXPECTED_FORMAT_CHARS = 3;

	// If the passed in string is shorter than minimum expected xliff formatting, just return it
	if ( static_cast< int >( inXliffStr.GetSize() ) <= MIN_NUM_EXPECTED_FORMAT_CHARS )
	{
		return inXliffStr;
	}

	// Buffer that holds formatted return string
	StringBuffer retStrBuffer;

	char const * p = inXliffStr.ToCStr();
	for ( ; ; )
	{
		uint32_t charCode = UTF8Util::DecodeNextChar( &p );
		if ( charCode == '\0' )
		{
			break;
		}
		else if( charCode == '%' )
		{
			// We found the start of the format specifier
			// Now check that there are at least three more characters which contain the format specification
			Array< uint32_t > formatSpec;
			for ( int count = 0; count < MIN_NUM_EXPECTED_FORMAT_CHARS; ++count )
			{
				uint32_t formatCharCode = UTF8Util::DecodeNextChar( &p );
				formatSpec.PushBack( formatCharCode );
			}

			OVR_ASSERT( formatSpec.GetSizeI() >= MIN_NUM_EXPECTED_FORMAT_CHARS );
			
			uint32_t desiredArgIdxChar = formatSpec.At( 0 );
			uint32_t dollarThing = formatSpec.At( 1 );
			uint32_t specifier = formatSpec.At( 2 );

			// Checking if it has supported xliff format specifier
			if( ( desiredArgIdxChar >= '1' && desiredArgIdxChar <= '9' ) &&
				( dollarThing == '$' ) &&
				( specifier == 's' ) )
			{
				// Found format valid specifier, so processing entire format specifier.
				int desiredArgIdxint = desiredArgIdxChar - '0';

				va_list args;
				va_start( args, inXliffStr );

				// Loop till desired argument is found.
				for( int j = 0; ; ++j )
				{
					const char* tempArg = va_arg( args, const char* );
					if( j == ( desiredArgIdxint - 1 ) ) // found desired argument
					{
						retStrBuffer.AppendFormat( "%s", tempArg );
						break;
					}
				}

				va_end(args);
			}
			else
			{
				LOG( "%s has invalid xliff format - has unsupported format specifier.", (const char*)(inXliffStr) );
				return inXliffStr;
			}
		}
		else
		{
			retStrBuffer.AppendChar( charCode );
		}
	}

	return String(retStrBuffer);
}

String VrLocale::GetXliffFormattedString( const String & inXliffStr, const char * arg1 )
{
	return private_GetXliffFormattedString( inXliffStr, arg1 );
}

String VrLocale::GetXliffFormattedString( const String & inXliffStr, const char * arg1, const char * arg2 )
{
	return private_GetXliffFormattedString( inXliffStr, arg1, arg2 );
}

OVR::String VrLocale::GetXliffFormattedString( const String & inXliffStr, const char * arg1, const char * arg2, const char * arg3 )
{
	return private_GetXliffFormattedString( inXliffStr, arg1, arg2, arg3 );
}

String VrLocale::ToString( char const * fmt, float const f )
{
	char buffer[128];
	OVR_sprintf( buffer, 128, fmt, f );
	return String( buffer );
}

String VrLocale::ToString( char const * fmt, int const i )
{
	char buffer[128];
	OVR_sprintf( buffer, 128, fmt, i );
	return String( buffer );
}

} // namespace OVR
