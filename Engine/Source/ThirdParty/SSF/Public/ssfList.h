#ifndef _ssfList_h_
#define _ssfList_h_

#include "ssfBinaryInputStream.h"
#include "ssfBinaryOutputStream.h"

namespace ssf
	{
	template<class T> class ssfList 
		{
		public:
			static const uint64 DataTypeId = 0x1000;
			static const bool DataTypeIsConstantSize = false; // the number of items differ

			vector<T> Items; // the items in the list

			ssfList();
			ssfList( const ssfList<T> &other );
			~ssfList();
			const ssfList & operator = ( const ssfList<T> &other );

			void Read( ssfBinaryInputStream *s );
			void Write( ssfBinaryOutputStream *s );
			uint64 TotalSizeInBytes() const;
		};

	template<class T> ssfList<T>::ssfList()
		{
		}

	template<class T> ssfList<T>::ssfList(const ssfList<T> &other)
		{
		this->Items = other.Items;
		}

	template<class T> ssfList<T>::~ssfList()
		{
		}

	template<class T> const ssfList<T> & ssfList<T>::operator=(const ssfList<T> &other)
		{
		this->Items = other.Items;
		return *this;
		}

	template<class T> void ssfList<T>::Read(ssfBinaryInputStream *s)
		{
		// make sure this list is made to store this type of data
		uint64 item_data_type = s->ReadInt64();
		if( item_data_type != T::DataTypeId )
			{
			throw exception("Invalid data type id for list that is being read");
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

	template<class T> void ssfList<T>::Write(ssfBinaryOutputStream *s)
		{
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


	template<class T> uint64 ssfList<T>::TotalSizeInBytes() const
		{
		uint64 total_size = 0;

		total_size += sizeof(uint64); // DataTypeId
		total_size += sizeof(uint32); // number of items

		total_size += T::TotalVectorSizeInBytes( this->Items ); // items

		return total_size;
		}

	}


#endif//_ssfList_h_ 