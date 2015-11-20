#ifndef _ssfString_h_
#define _ssfString_h_

#include "ssfBinaryInputStream.h"
#include "ssfBinaryOutputStream.h"

namespace ssf
	{
	class ssfString 
		{
		public:
			static const uint64 DataTypeId = 0x0C;
			static const bool DataTypeIsConstantSize = false; // the size of the string varies

			string Value; 
		
			ssfString();
			ssfString( const string &value );
			ssfString( const std::basic_string<wchar_t> &value );
			ssfString( const ssfString &other );
			~ssfString();
			const ssfString & operator = ( const ssfString &other );
			operator const string & () const;
		
			static auto_ptr<ssfString> Create( const string &_value );		
			static auto_ptr<ssfString> Create( const char * _value );		
		
			std::basic_string<char> ToCharString() const; 
			std::basic_string<wchar_t> ToWCharString() const;
			std::basic_string<TCHAR> ToTCharString() const;
			
			void Read( ssfBinaryInputStream *s );
			void Write( ssfBinaryOutputStream *s );
			uint64 TotalSizeInBytes() const;

			// get the total size of all items in a vector (not counting the vector overhead)
			static uint64 TotalVectorSizeInBytes( const vector<ssfString> & value );

			// utility method for other objects that need to write a string
			static uint64 StringSizeInBytes( const string &value );
		};
	};
	
#endif//_ssfString_h_ 