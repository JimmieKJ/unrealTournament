#ifndef _ssfTupleList_h_
#define _ssfTupleList_h_

#include "ssfBinaryInputStream.h"
#include "ssfBinaryOutputStream.h"

namespace ssf
	{
	template<class T> class ssfTupleList 
		{
		public:
			static const uint64 DataTypeId = 0x2000;
			static const bool DataTypeIsConstantSize = false; // the number of items differ

			uint32 TupleSize; // the size of each tuple in the list
			vector<T> Items; // the items in the list

			ssfTupleList();
			ssfTupleList( const ssfTupleList<T> &other );
			~ssfTupleList();
			const ssfTupleList & operator = ( const ssfTupleList<T> &other );

			void Read( ssfBinaryInputStream *s );
			void Write( ssfBinaryOutputStream *s );
			uint64 TotalSizeInBytes() const;
		};

	template<class T> ssfTupleList<T>::ssfTupleList()
		{
		this->TupleSize = 1;
		}

	template<class T> ssfTupleList<T>::ssfTupleList(const ssfTupleList<T> &other)
		{
		this->TupleSize = other.TupleSize;
		this->Items = other.Items;
		}

	template<class T> ssfTupleList<T>::~ssfTupleList()
		{
		}

	template<class T> const ssfTupleList<T> & ssfTupleList<T>::operator=(const ssfTupleList<T> &other)
		{
		this->TupleSize = other.TupleSize;
		this->Items = other.Items;
		return *this;
		}

	template<class T> void ssfTupleList<T>::Read(ssfBinaryInputStream *s)
		{
		// make sure this list is made to store this type of data
		uint64 item_data_type = s->ReadInt64();
		if( item_data_type != T::DataTypeId )
			{
			throw exception("Invalid data type id for indexed list that is being read");
			}

		// get the number of items
		uint32 item_count = s->ReadUInt32();

		// get the tuple size
		this->TupleSize = s->ReadUInt32();

		// resize the vector, and read all items from the input stream
		Items.resize((uint32)item_count);
		for( uint32 i=0; i<item_count; ++i )
			{
			Items[i].Read(s);
			}
		}

	template<class T> void ssfTupleList<T>::Write(ssfBinaryOutputStream *s)
		{
		// write the type of data that we are storing to the stream
		s->WriteUInt64(T::DataTypeId);

		// write the number of items
		s->WriteUInt32((uint32)Items.size());

		// write the number of items
		s->WriteUInt32(TupleSize);

		// write all items to the stream
		for( uint32 i=0; i<(uint32)Items.size(); ++i )
			{
			Items[i].Write(s);
			}
		}


	template<class T> uint64 ssfTupleList<T>::TotalSizeInBytes() const
		{
		uint64 total_size = 0;

		total_size += sizeof(uint64); // DataTypeId
		total_size += sizeof(uint32); // number of items
		total_size += sizeof(uint32); // TupleSize
		total_size += T::TotalVectorSizeInBytes( this->Items ); // items

		return total_size;
		}
	};

#endif//_ssfTupleList_h_ 