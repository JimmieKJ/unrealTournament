// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


// TAutoPointer wraps a smart-pointer and adds an implicit conversion to raw pointer
// Its main use is for converting a variable from raw pointer to a smart pointer without breaking existing code
// Currently only used in TAutoWeakObjectPtr
// Not the same thing as TAutoPtr :(
template<class T, class TBASE>
class TAutoPointer : public TBASE
{
public:
	/** NULL constructor **/
	FORCEINLINE TAutoPointer()
	{
	}
	/** Construct from a single argument (I'd use inheriting constructors if all our compilers supported it) **/
	template<typename X>
	FORCEINLINE TAutoPointer(X&& Target)
		: TBASE(Forward<X>(Target))
	{
	}

	FORCEINLINE operator T* () const
	{
		return TBASE::Get();
	}
	FORCEINLINE operator const T* () const
	{
		return (const T*)TBASE::Get();
	}

	FORCEINLINE explicit operator bool() const
	{
		return TBASE::Get() != nullptr;
	}
};


template<class T, class TBASE> struct TIsPODType<TAutoPointer<T, TBASE> > { enum { Value = TIsPODType<TBASE>::Value }; }; // pod-ness is the same as the podness of the base pointer type
