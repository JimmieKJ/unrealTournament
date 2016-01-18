#ifndef _ssfIndexedList_h_
#define _ssfIndexedList_h_

#include "ssfBinaryInputStream.h"
#include "ssfBinaryOutputStream.h"

namespace ssf
	{
	template<class T> class ssfIndexedList 
		{
		public:
			static const uint64 DataTypeId = 0x5000;
			static const bool DataTypeIsConstantSize = false; // the number of items differ

			vector<uint32> Indices; // the indices into the item list
			vector<T> Items; // the items in the list

			ssfIndexedList();
			ssfIndexedList( const ssfIndexedList<T> &other );
			~ssfIndexedList();
			const ssfIndexedList & operator = ( const ssfIndexedList<T> &other );

			void Read( ssfBinaryInputStream *s );
			void Write( ssfBinaryOutputStream *s );
			uint64 TotalSizeInBytes() const;
		};

	template<class T> ssfIndexedList<T>::ssfIndexedList()
		{
		}

	template<class T> ssfIndexedList<T>::ssfIndexedList(const ssfIndexedList<T> &other)
		{
		this->Items = other.Items;
		this->Indices = other.Indices;
		}

	template<class T> ssfIndexedList<T>::~ssfIndexedList()
		{
		}

	template<class T> const ssfIndexedList<T> & ssfIndexedList<T>::operator=(const ssfIndexedList<T> &other)
		{
		this->Items = other.Items;
		this->Indices = other.Indices;
		return *this;
		}

	template<class T> void ssfIndexedList<T>::Read(ssfBinaryInputStream *s)
		{
		// make sure this list is made to store this type of data
		uint64 item_data_type = s->ReadInt64();
		if( item_data_type != T::DataTypeId )
			{
			throw exception("Invalid data type id for indexed list that is being read");
			}

		// get the number of indices
		uint32 index_count = s->ReadUInt32();

		// resize the vector, and read all items from the input stream
		Indices.resize((uint32)index_count);
		for( uint32 i=0; i<index_count; ++i )
			{
			Indices[i] = s->ReadUInt32();
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

	template<class T> void ssfIndexedList<T>::Write(ssfBinaryOutputStream *s)
		{
		// write the type of data that we are storing to the stream
		s->WriteUInt64(T::DataTypeId);

		// write the number of items
		s->WriteUInt32(Indices.size());

		// write all items to the stream
		for( uint32 i=0; i<(uint32)Indices.size(); ++i )
			{
			s->WriteUInt32( Indices[i] );
			}

		// write the number of items
		s->WriteUInt32( (uint32)Items.size());

		// write all items to the stream
		for( uint32 i=0; i<(uint32)Items.size(); ++i )
			{
			Items[i].Write(s);
			}
		}


	template<class T> uint64 ssfIndexedList<T>::TotalSizeInBytes() const
		{
		uint64 total_size = 0;

		total_size += sizeof(uint64); // DataTypeId

		total_size += sizeof(uint32); // number of indices
		total_size += sizeof(uint32) * Indices.size(); // the indices
		total_size += sizeof(uint32); // number of items
		total_size += T::TotalVectorSizeInBytes( this->Items ); // items

		return total_size;
		}

	};

#endif//_ssfIndexedList_h_ 