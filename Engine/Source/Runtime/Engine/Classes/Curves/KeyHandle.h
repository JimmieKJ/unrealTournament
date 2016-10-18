#pragma once

#include "KeyHandle.generated.h"


/**
 * Key handles are used to keep a handle to a key. They are completely transient.
 */
struct FKeyHandle
{
	ENGINE_API FKeyHandle();

	bool operator ==(const FKeyHandle& Other) const
	{
		return Index == Other.Index;
	}
	
	bool operator !=(const FKeyHandle& Other) const
	{
		return Index != Other.Index;
	}
	
	friend uint32 GetTypeHash(const FKeyHandle& Handle)
	{
		return GetTypeHash(Handle.Index);
	}

	friend FArchive& operator<<(FArchive& Ar,FKeyHandle& Handle)
	{
		Ar << Handle.Index;
		return Ar;
	}

private:

	uint32 Index;
};


/**
 * Represents a mapping of key handles to key index that may be serialized 
 */
USTRUCT()
struct FKeyHandleMap
{
	GENERATED_USTRUCT_BODY()
public:
	FKeyHandleMap() {}

	// This struct is not copyable.  This must be public or because derived classes are allowed to be copied
	FKeyHandleMap( const FKeyHandleMap& Other ) {}
	void operator=(const FKeyHandleMap& Other) {}

	/** TMap functionality */
	void Add( const FKeyHandle& InHandle, int32 InIndex );
	void Empty();
	void Remove( const FKeyHandle& InHandle );
	const int32* Find( const FKeyHandle& InHandle ) const;
	const FKeyHandle* FindKey( int32 KeyIndex ) const;
	int32 Num() const;
	TMap<FKeyHandle, int32>::TConstIterator CreateConstIterator() const;
	TMap<FKeyHandle, int32>::TIterator CreateIterator();

	/** ICPPStructOps implementation */
	bool Serialize(FArchive& Ar);
	bool operator==(const FKeyHandleMap& Other) const;
	bool operator!=(const FKeyHandleMap& Other) const;

	friend FArchive& operator<<(FArchive& Ar,FKeyHandleMap& P)
	{
		P.Serialize(Ar);
		return Ar;
	}

private:

	TMap<FKeyHandle, int32> KeyHandlesToIndices;
};


template<>
struct TStructOpsTypeTraits<FKeyHandleMap>
	: public TStructOpsTypeTraitsBase
{
	enum 
	{
		WithSerializer = true,
		WithCopy = false,
		WithIdenticalViaEquality = true,
	};
};
