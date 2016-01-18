#ifndef _ssfNamedTupleList_h_
#define _ssfNamedTupleList_h_

#include "ssfBinaryInputStream.h"
#include "ssfBinaryOutputStream.h"

namespace ssf
	{
	template<class T> class ssfNamedTupleList 
		{
		public:
			static const uint64 DataTypeId = 0x4000;
			static const bool DataTypeIsConstantSize = false; // the number of items differ

			string Name; // the name of the list
			uint32 TupleSize; // the size of each tuple in the list
			vector<T> Items; // the items in the list

			ssfNamedTupleList();
			ssfNamedTupleList( const ssfNamedTupleList<T> &other );
			~ssfNamedTupleList();
			const ssfNamedTupleList & operator = ( const ssfNamedTupleList<T> &other );

			void Read( ssfBinaryInputStream *s );
			void Write( ssfBinaryOutputStream *s );
			uint64 TotalSizeInBytes() const;
		};

	template<class T> ssfNamedTupleList<T>::ssfNamedTupleList()
		{
		this->TupleSize = 1;
		}

	template<class T> ssfNamedTupleList<T>::ssfNamedTupleList(const ssfNamedTupleList<T> &other)
		{
		this->Name = other.Name;
		this->TupleSize = other.TupleSize;
		this->Items = other.Items;
		}

	template<class T> ssfNamedTupleList<T>::~ssfNamedTupleList()
		{
		}

	template<class T> const ssfNamedTupleList<T> & ssfNamedTupleList<T>::operator=(const ssfNamedTupleList<T> &other)
		{
		this->Name = other.Name;
		this->TupleSize = other.TupleSize;
		this->Items = other.Items;
		return *this;
		}

	template<class T> void ssfNamedTupleList<T>::Read(ssfBinaryInputStream *s)
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

		// get the tuple size
		this->TupleSize = s->ReadUInt32();

		// resize the vector, and read all items from the input stream
		Items.resize((uint32)item_count);
		for( uint32 i=0; i<item_count; ++i )
			{
			Items[i].Read(s);
			}
		}

	template<class T> void ssfNamedTupleList<T>::Write(ssfBinaryOutputStream *s)
		{
		// write the name of the list
		s->WriteString(this->Name);

		// write the type of data that we are storing to the stream
		s->WriteUInt64(T::DataTypeId);

		// write the number of items
		s->WriteUInt32((uint32)Items.size());

		// write the TupleSize
		s->WriteUInt32(TupleSize);

		// write all items to the stream
		for( uint32 i=0; i<(uint32)Items.size(); ++i )
			{
			Items[i].Write(s);
			}
		}


	template<class T> uint64 ssfNamedTupleList<T>::TotalSizeInBytes() const
		{
		uint64 total_size = 0;

		total_size += ssfString::StringSizeInBytes(this->Name); // name
		total_size += sizeof(uint64); // DataTypeId
		total_size += sizeof(uint32); // number of items
		total_size += sizeof(uint32); // TupleSize
		total_size += T::TotalVectorSizeInBytes( this->Items ); // items
	
		return total_size;
		}
	}

#endif//_ssfNamedTupleList_h_ 