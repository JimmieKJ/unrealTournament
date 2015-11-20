/**********************************************************************************

Filename    :   OVR_Lexer.h
Content     :   A light-weight lexer 
Created     :   May 1, 2015
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Oculus360Photos/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

************************************************************************************/

#if !defined( OVR_LEXER_H )
#define OVR_LEXER_H

#include <stdint.h>

namespace OVR {

//==============================================================
// ovrLexer
//
// This was originally intended to get rid of sscanf usage which
// has security issues (buffer overflows) and cannot parse strings containing
// spaces without using less-than-ideal workarounds. Supports keeping quoted 
// strings as single tokens, UTF-8 encoded text, but not much else.
//==============================================================
class ovrLexer 
{
public:
	enum ovrResult 
	{
		LEX_RESULT_OK,
		LEX_RESULT_ERROR,	// ran out of buffer space
		LEX_RESULT_EOF		// tried to read past the end of the buffer
	};

	// expects a 0-terminated string as input
	ovrLexer( const char * source );

	~ovrLexer();

	ovrResult	NextToken( char * token, size_t const maxTokenSize );
	ovrResult	ParseInt( int & value, int defaultVal );
	ovrResult	ParsePointer( unsigned char * & ptr, unsigned char * defaultVal );
	
	ovrResult	GetError() const { return Error; }

private:
	static	bool		FindChar( char const * buffer, uint32_t const ch );
	static	bool		IsWhitespace( uint32_t const ch );
	static	bool		IsQuote( uint32_t const ch );
	static	ovrResult	SkipWhitespace( char const * & p );
	static	void		CopyResult( char const * buffer, char * token, size_t const maxTokenSize );

private:
	const char *	p;	// pointer to current position
	ovrResult		Error;	
};

} // namespace OVR

#endif // OVR_LEXER_H