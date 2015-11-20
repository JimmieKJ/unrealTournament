#ifndef _ssfAttributeHeader_h_
#define _ssfAttributeHeader_h_

#include "ssfBinaryInputStream.h"
#include "ssfBinaryOutputStream.h"

#include "ssfAttributeTypes.h"

namespace ssf
	{
	class ssfAttributeHeader 
		{
		public:
			uint64 DataTypeId;
			string Name;
			uint64 DataSize;

			ssfAttributeHeader();

			void Read( ssfBinaryInputStream *s );
			void Write( ssfBinaryOutputStream *s );
			uint64 HeaderSizeInBytes() const;

			template<class T> void SetFromDataType( const char *name , T *data );
			template<class T> void WriteAttributeToOutputStream( ssfBinaryOutputStream *s , const char *name , T *data );

			void ssfAttributeHeader::SetFromDataType( const char *name , double data )
				{
				this->Name = string(name);
				this->DataTypeId = ssfDouble::DataTypeId;
				this->DataSize = 8;
				}

			void ssfAttributeHeader::SetFromDataType( const char *name , int data )
				{
				this->Name = string(name);
				this->DataTypeId = ssfInt32::DataTypeId;
				this->DataSize = 4;
				}

			void WriteAttributeToOutputStream( ssfBinaryOutputStream *s , const char *name , double data)
				{
					this->Write(s);
					s->WriteDouble(data);
				}

			void WriteAttributeToOutputStream( ssfBinaryOutputStream *s , const char *name , int data)
				{
					this->Write(s);
					s->WriteInt32(data);
				}

		};
	
	template<class T> void ssfAttributeHeader::SetFromDataType( const char *name , T *data )
		{
		this->Name = string(name);
		this->DataTypeId = T::DataTypeId;
		this->DataSize = data->TotalSizeInBytes();
		}

	template<class T> void ssfAttributeHeader::WriteAttributeToOutputStream( ssfBinaryOutputStream *s , const char *name , T *data )
		{
#ifdef _DEBUG
		ssfAttributeHeader dbg_header;
		dbg_header.SetFromDataType(name,data);
		if( this->Name != dbg_header.Name ||
			this->DataTypeId != dbg_header.DataTypeId || 
			this->DataSize != dbg_header.DataSize )
			{
			throw std::exception("Debug exception: In ssfAttributeHeader::WriteAttributeToOutputStream(): The data does not match the attribute header");
			}
#endif

		// write the header to the stream
		this->Write(s);

		// write the data to the stream
		data->Write(s);
		}


	};

#endif//_ssfAttributeHeader_h_ 