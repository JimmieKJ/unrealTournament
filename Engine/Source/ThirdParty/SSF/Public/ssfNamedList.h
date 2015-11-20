#ifndef _ssfNamedList_h_
#define _ssfNamedList_h_

#include "ssfBinaryInputStream.h"
#include "ssfBinaryOutputStream.h"

namespace ssf
	{
	template<class T> class ssfNamedList 
		{
		public:
			static const uint64 DataTypeId = 0x3000;
			static const bool DataTypeIsConstantSize = false; // the number of items differ

			string Name; // the name of the list
			vector<T> Items; // the items in the list

			ssfNamedList();
			ssfNamedList( const ssfNamedList<T> &other );
			~ssfNamedList();
			const ssfNamedList & operator = ( const ssfNamedList<T> &other );

			void Read( ssfBinaryInputStream *s );
			void Write( ssfBinaryOutputStream *s );
			uint64 TotalSizeInBytes() const;
		};

	template<class T> ssfNamedList<T>::ssfNamedList()
		{
		}

	template<class T> ssfNamedList<T>::ssfNamedList(const ssfNamedList<T> &other)
		{
		this->Name = other.Name;
		this->Items = other.Items;
		}

	template<class T> ssfNamedList<T>::~ssfNamedList()
		{
		}

	template<class T> const ssfNamedList<T> & ssfNamedList<T>::operator=(const ssfNamedList<T> &other)
		{
		this->Name = other.Name;
		this->Items = other.Items;
		return *this;
		}

	template<class T> void ssfNamedList<T>::Read(ssfBinaryInputStream *s)
		{
		this->Name = s->ReadString();

		// make sure this list is made to store this type of data
		uint64 item_data_type = s->ReadInt64();
		if( item_data_type != T::DataTypeId )
			{
			throw exception("Invalid data type id for indexed list that is being read");
			}

		// get the number of items
		uint32 item_count = s->ReadUInt32();

		// resize the vector, and read all items from the input stream
		Items.resize((uint32)item_count);
		for( uint32 i=0; i<item_count; ++i )
			{
			Items[i].Read(s);
			}
		}

	template<class T> void ssfNamedList<T>::Write(ssfBinaryOutputStream *s)
		{
		// write the name of the list
		s->WriteString(this->Name);

		// write the type of data that we are storing to the stream
		s->WriteUInt64(T::DataTypeId);

		// write the number of items
		s->WriteUInt32((uint32)Items.size());

		// write all items to the stream
		for( uint32 i=0; i<(uint32)Items.size(); ++i )
			{
			Items[i].Write(s);
			}
		}


	template<class T> uint64 ssfNamedList<T>::TotalSizeInBytes() const
		{
		uint64 total_size = 0;

		total_size += ssfString::StringSizeInBytes(this->Name); // name
		total_size += sizeof(uint64); // DataTypeId
		total_size += sizeof(uint32); // number of items
		total_size += T::TotalVectorSizeInBytes( this->Items ); // items
	
		return total_size;
		}
	};

#endif//_ssfNamedList_h_ 