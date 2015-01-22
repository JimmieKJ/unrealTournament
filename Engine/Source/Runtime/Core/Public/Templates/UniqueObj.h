// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UniquePtr.h"

// This is essentially a reference version of TUniquePtr
// Useful to force heap allocation of a value, e.g. in a TMap to give similar behaviour to TIndirectArray

template <typename T>
class TUniqueObj
{
public:
	TUniqueObj(const TUniqueObj& other)
		: Obj(MakeUnique<T>(*other))
	{
	}

	// As TUniqueObj's internal pointer can never be null, we can't move that
	// On the other hand we can call the move constructor of "T"
	TUniqueObj(TUniqueObj&& other)
		: Obj(MakeUnique<T>(MoveTemp(*other)))
	{
	}

#if PLATFORM_COMPILER_HAS_VARIADIC_TEMPLATES

	template <typename... Args>
	explicit TUniqueObj(Args&&... args)
		: Obj(MakeUnique<T>(Forward<Args>(args)...))
	{
	}

#else

	                                                                      explicit TUniqueObj(                                                  ) : Obj(MakeUnique<T>(                                                                                  )) {}
	template <typename Arg0                                             > explicit TUniqueObj(Arg0&& arg0                                       ) : Obj(MakeUnique<T>(Forward<Arg0>(arg0)                                                               )) {}
	template <typename Arg0, typename Arg1                              > explicit TUniqueObj(Arg0&& arg0, Arg1&& arg1                          ) : Obj(MakeUnique<T>(Forward<Arg0>(arg0), Forward<Arg1>(arg1)                                          )) {}
	template <typename Arg0, typename Arg1, typename Arg2               > explicit TUniqueObj(Arg0&& arg0, Arg1&& arg1, Arg2&& arg2             ) : Obj(MakeUnique<T>(Forward<Arg0>(arg0), Forward<Arg1>(arg1), Forward<Arg2>(arg2)                     )) {}
	template <typename Arg0, typename Arg1, typename Arg2, typename Arg3> explicit TUniqueObj(Arg0&& arg0, Arg1&& arg1, Arg2&& arg2, Arg3&& arg3) : Obj(MakeUnique<T>(Forward<Arg0>(arg0), Forward<Arg1>(arg1), Forward<Arg2>(arg2), Forward<Arg3>(arg3))) {}

#endif

	// Disallow copy-assignment
#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS
	TUniqueObj& operator=(const TUniqueObj&) = delete;
#else
private:
	TUniqueObj& operator=(const TUniqueObj&);
public:
#endif

	// Move-assignment is implemented as swapping the internal pointer
	TUniqueObj& operator=(TUniqueObj&& other)
	{
		Swap(Obj, other.Obj);
		return *this;
	}

	template <typename Arg>
	TUniqueObj& operator=(Arg&& other)
	{
		*Obj = Forward<Arg>(other);
		return *this;
	}

	      T& Get()       { return *Obj; }
	const T& Get() const { return *Obj; }

	      T* operator->()       { return Obj.Get(); }
	const T* operator->() const { return Obj.Get(); }

	      T& operator*()       { return *Obj; }
	const T& operator*() const { return *Obj; }

	friend FArchive& operator<<(FArchive& Ar, TUniqueObj& P)
	{
		Ar << *P.Obj;
		return Ar;
	}

private:
	TUniquePtr<T> Obj;
};
