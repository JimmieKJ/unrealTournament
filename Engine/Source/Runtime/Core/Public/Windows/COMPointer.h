// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
* Smart COM object pointer.
*/
template<typename T>
class TComPtr
{
public:
	typedef T PointerType;

public:
	TComPtr() :	RawPointer(nullptr)
	{
	}

	TComPtr(PointerType* const Source) : RawPointer(Source)
	{
		if(RawPointer)
		{
			RawPointer->AddRef();
		}
	}

	TComPtr(const TComPtr<PointerType>& Source) : RawPointer(Source.RawPointer)
	{
		if(RawPointer)
		{
			RawPointer->AddRef();
		}
	}

	TComPtr(TComPtr<PointerType>&& Source) : RawPointer(Source.RawPointer)
	{	
		Source.RawPointer = nullptr;
	}	

	TComPtr<PointerType>& operator=(PointerType* const Source) 
	{
		if(RawPointer != Source)
		{
			if (Source)
			{
				Source->AddRef();
			}
			if (RawPointer)
			{
				RawPointer->Release();
			}
			RawPointer = Source;
		}
		return *this;
	}

	TComPtr<PointerType>& operator=(const TComPtr<PointerType>& Source) 
	{
		if(RawPointer != Source.RawPointer)
		{
			if (Source.RawPointer)
			{
				Source->AddRef();
			}
			if (RawPointer)
			{
				RawPointer->Release();
			}
			RawPointer = Source.RawPointer;
		}
		return *this;
	}

	TComPtr<PointerType>& operator=(TComPtr<PointerType>&& Source) 
	{			
		if(RawPointer != Source.RawPointer)
		{
			if(RawPointer)
			{
				RawPointer->Release();
			}
			RawPointer = Source.RawPointer;
			Source.RawPointer = nullptr;
		}
		return *this;
	}

	~TComPtr() 
	{
		if(RawPointer)
		{
			RawPointer->Release();
		}
	}

	PointerType** operator&()
	{
		return &(RawPointer);
	}

	PointerType* operator->() const 
	{
		check(RawPointer != nullptr);
		return RawPointer;
	}
	
	bool operator==(PointerType* const InRawPointer) const
	{
		return RawPointer == InRawPointer;
	}

	bool operator!=(PointerType* const InRawPointer) const
	{
		return RawPointer != InRawPointer;
	}

	operator PointerType*() const
	{
		return RawPointer;
	}

	void Reset()
	{
		if(RawPointer)
		{
			RawPointer->Release();
			RawPointer = nullptr;
		}
	}

	// Set the pointer without adding a reference.
	void Attach(PointerType* InRawPointer)
	{
		if(RawPointer)
		{
			RawPointer->Release();
		}
		RawPointer = InRawPointer;
	}

	// Reset the pointer without releasing a reference.
	void Detach()
	{
		RawPointer = nullptr;
	}

	HRESULT FromQueryInterface(REFIID riid, IUnknown* Unknown)
	{
		if(RawPointer)
		{
			RawPointer->Release();
			RawPointer = nullptr;
		}
		return Unknown->QueryInterface(riid, reinterpret_cast<void**>(&(RawPointer)));
	}

private:
	PointerType* RawPointer;
};