//! @file std_wrappers.h
//! @brief Substance Framework STL compatible containers
//! @date 20120111
//! @copyright Allegorithmic. All rights reserved.

#pragma once

#include <Core.h>

namespace Substance
{

template<typename T>
class List
{
	TArray<T> mList;

public:

	typedef TIndexedContainerIterator<      TArray<T>,       T, int32> TIterator;
	typedef TIndexedContainerIterator<const TArray<T>, const T, int32> TConstIterator;

	List() {}

	const TArray<T>& getArray() const
	{
		return mList;
	}

	TArray<T>& getArray()
	{
		return mList;
	}

	inline TIterator itfront(){return TIterator(mList);}

	inline TConstIterator itfrontconst() const{return TConstIterator(mList);}

	inline T & operator[](uint32 idx)
	{
		return mList.GetData()[idx];
	}

	inline const T & operator[](uint32 idx) const
	{
		return mList.GetData()[idx];
	}

	inline void push(const T & elem)
	{
		mList.Push(elem);
	}

	int32 AddZeroed(int32 count)
	{
		return mList.AddZeroed(count);
	}

	int32 AddUnique(const T& elem)
	{
		return mList.AddUnique(elem);
	}

	bool FindItem(const T& elem, int32& idx ) const
	{
		return mList.Find(elem, idx);
	}

	int32 Remove(const T& elem)
	{
		return mList.Remove(elem);
	}

	void RemoveAt(int32 idx)
	{
		return mList.RemoveAt(idx);
	}

	void Reserve(int32 Size)
	{
		mList.Reserve(Size);
	}

	void BulkSerialize(FArchive& ar)
	{
		mList.BulkSerialize(ar);
	}

	List<T>& operator+=( const List<T>& Other )
	{
		mList.Append( Other.mList );
		return *this;
	}

	inline T front()
	{
		return mList.GetData()[0];
	}

	inline T& Last()
	{
		return mList.Last();
	}

	inline T pop()
	{
		return mList.Pop();
	}

	inline void erase(const T & elem)
	{
		mList.RemoveItem(elem);
	}

	inline uint32 size() const
	{
		return mList.Num();
	}

	inline int32 Num() const
	{
		return mList.Num();
	}

	inline void Empty()
	{
		mList.Empty();
	}
};


template<class T>
class Vector
{
	TArray<T> mVector;

public:

	typedef TIndexedContainerIterator<      TArray<T>,       T, int32> TIterator;
	typedef TIndexedContainerIterator<const TArray<T>, const T, int32> TConstIterator;

	inline TIterator itfront(){return TIterator(mVector);}

	inline TConstIterator itfrontconst() const{return TConstIterator(mVector);}

	Vector() {}

	Vector(const Vector & other) : mVector(other.mVector)
	{
	}

	Vector & operator=(const Vector & other) 
	{ 
		mVector = other.mVector;
		return *this; 
	}

	inline void push_back(const T & elem)
	{
		mVector.Push(elem);
	}

	inline T & operator[](uint32 idx)
	{
		return mVector.GetTypedData()[idx];
	}

	inline const T & operator[](uint32 idx) const
	{
		return mVector.GetTypedData()[idx];
	}

	inline uint32 size() const
	{
		return (uint32)mVector.Num();
	}

	inline void clear()
	{
		mVector.Reset();
	}

	inline bool operator==(const Vector & other) const
	{
		return (mVector == other.mVector != 0 ? true : false);
	}

	inline void reserve(uint32 elemCount)
	{
		mVector.Reserve(elemCount);
	}

	inline void resize(uint32 elemCount)
	{
		mVector.Reserve(elemCount);
		while (elemCount != size())
		{
			mVector.Push(T());
		}
	}

	inline int32 removeItem(const T& elem)
	{
		return mVector.Remove(elem);
	}

	void pop_front()
	{
		mVector.RemoveAt(0);
	}
	
	void pop_back()
	{
		mVector.Shrink();
	}

	T& back()
	{
		return mVector.Last();
	}

};

} // namespace Substance
