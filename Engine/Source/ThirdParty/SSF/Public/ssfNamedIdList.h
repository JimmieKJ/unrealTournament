#ifndef _ssfNamedIdList_h_
#define _ssfNamedIdList_h_

#include "ssfBinaryInputStream.h"
#include "ssfBinaryOutputStream.h"

namespace ssf
	{
	template<class T> class ssfNamedIdList 
		{
		public:
			static const uint64 DataTypeId = 0x7000;
			static const bool DataTypeIsConstantSize = false; // the number of items differ

			string Name; // the name of the list
			string ID; // the ID of the list
			vector<T> Items; // the items in the list

			ssfNamedIdList();
			ssfNamedIdList( const ssfNamedIdList<T> &other );
			~ssfNamedIdList();
			const ssfNamedIdList & operator = ( const ssfNamedIdList<T> &other );

			void Read( ssfBinaryInputStream *s );
			void Write( ssfBinaryOutputStream *s );
			uint64 TotalSizeInBytes() const;
		};

	template<class T> ssfNamedIdList<T>::ssfNamedIdList()
		{
		}

	template<class T> ssfNamedIdList<T>::ssfNamedIdList(const ssfNamedIdList<T> &other)
		{
		this->Name = other.Name;
		this->ID = other.ID;
		this->Items = other.Items;
		}

	template<class T> ssfNamedIdList<T>::~ssfNamedIdList()
		{
		}

	template<class T> const ssfNamedIdList<T> & ssfNamedIdList<T>::operator=(const ssfNamedIdList<T> &other)
		{
		this->Name = other.Name;
		this->ID = other.ID;
		this->Items = other.Items;
		return *this;
		}

	template<class T> void ssfNamedIdList<T>::Read(ssfBinaryInputStream *s)
		{
		this->Name = s->ReadString();
		this->ID = s->ReadString();

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

	template<class T> void ssfNamedIdList<T>::Write(ssfBinaryOutputStream *s)
		{
		// write the name of the list
		s->WriteString(this->Name);

		// write the ID of the list
		s->WriteString(this->ID);

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


	template<class T> uint64 ssfNamedIdList<T>::TotalSizeInBytes() const
		{
		uint64 total_size = 0;

		total_size += ssfString::StringSizeInBytes(this->Name); // name
		total_size += ssfString::StringSizeInBytes(this->ID); // ID
		total_size += sizeof(uint64); // DataTypeId
		total_size += sizeof(uint32); // number of items
		total_size += T::TotalVectorSizeInBytes( this->Items ); // items
	
		return total_size;
		}
	};

#endif//_ssfNamedIdList_h_ 