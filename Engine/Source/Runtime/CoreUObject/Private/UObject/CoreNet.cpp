// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnCoreNet.cpp: Core networking support.
=============================================================================*/

#include "CoreUObjectPrivate.h"

DEFINE_LOG_CATEGORY_STATIC(LogCoreNet, Log, All);

DEFINE_STAT(STAT_NetSerializeFast_Array);

/*-----------------------------------------------------------------------------
	FClassNetCache implementation.
-----------------------------------------------------------------------------*/

FClassNetCache::FClassNetCache()
{}
FClassNetCache::FClassNetCache( UClass* InClass )
: Class( InClass )
{}

FClassNetCache * FClassNetCacheMgr::GetClassNetCache( UClass* Class )
{
	FClassNetCache * Result = ClassFieldIndices.FindRef( Class );

	if ( !Result )
	{
		Result					= ClassFieldIndices.Add( Class, new FClassNetCache( Class ) );
		Result->Super			= NULL;
		Result->FieldsBase		= 0;

		if ( Class->GetSuperClass() )
		{
			Result->Super		= GetClassNetCache(Class->GetSuperClass());
			Result->FieldsBase	= Result->Super->GetMaxIndex();
		}

		Result->Fields.Empty( Class->NetFields.Num() );

		for( int32 i = 0; i < Class->NetFields.Num(); i++ )
		{
			// Add sandboxed items to net cache.  
			UField * Field = Class->NetFields[i];
			int32 ThisIndex	= Result->GetMaxIndex();
			new ( Result->Fields )FFieldNetCache( Field, ThisIndex );
		}

		Result->Fields.Shrink();

		for ( TArray< FFieldNetCache >::TIterator It( Result->Fields ); It; ++It )
		{
			Result->FieldMap.Add( It->Field, &*It );
		}
	}
	return Result;
}

void FClassNetCacheMgr::ClearClassNetCache()
{
	for ( auto It = ClassFieldIndices.CreateIterator(); It; ++It)
	{
		delete It.Value();
	}

	ClassFieldIndices.Empty();
}

/*-----------------------------------------------------------------------------
	UPackageMap implementation.
-----------------------------------------------------------------------------*/

bool UPackageMap::SerializeName(FArchive& Ar, FName& Name)
{
	if (Ar.IsLoading())
	{
		uint8 bHardcoded = 0;
		Ar.SerializeBits(&bHardcoded, 1);
		if (bHardcoded)
		{
			// replicated by hardcoded index
			uint32 NameIndex;
			Ar.SerializeInt(NameIndex, MAX_NETWORKED_HARDCODED_NAME + 1);
			Name = EName(NameIndex);
			// hardcoded names never have a Number
		}
		else
		{
			// replicated by string
			FString InString;
			int32 InNumber;
			Ar << InString << InNumber;
			Name = FName(*InString, InNumber);
		}
	}
	else if (Ar.IsSaving())
	{
		uint8 bHardcoded = Name.GetComparisonIndex() <= MAX_NETWORKED_HARDCODED_NAME;
		Ar.SerializeBits(&bHardcoded, 1);
		if (bHardcoded)
		{
			// send by hardcoded index
			checkSlow(Name.GetNumber() <= 0); // hardcoded names should never have a Number
			uint32 NameIndex = uint32(Name.GetComparisonIndex());
			Ar.SerializeInt(NameIndex, MAX_NETWORKED_HARDCODED_NAME + 1);
		}
		else
		{
			// send by string
			FString OutString = Name.GetPlainNameString();
			int32 OutNumber = Name.GetNumber();
			Ar << OutString << OutNumber;
		}
	}
	return true;
}

IMPLEMENT_CORE_INTRINSIC_CLASS(UPackageMap, UObject,
	{
	}
);

// ----------------------------------------------------------------

void SerializeChecksum(FArchive &Ar, uint32 x, bool ErrorOK)
{
	if (Ar.IsLoading() )
	{
		uint32 Magic = 0;
		Ar << Magic;
		if((!ErrorOK || !Ar.IsError()) && !ensure(Magic==x))
		{
			UE_LOG(LogCoreNet, Warning, TEXT("%d == %d"), Magic, x );
		}
		
	}
	else
	{
		uint32 Magic = x;
		Ar << Magic;
	}
}

// ----------------------------------------------------------------
//	FNetBitWriter
// ----------------------------------------------------------------
FNetBitWriter::FNetBitWriter()
:	FBitWriter(0)
{}

FNetBitWriter::FNetBitWriter( int64 InMaxBits )
:	FBitWriter (InMaxBits, true)
,	PackageMap( NULL )
{

}

FNetBitWriter::FNetBitWriter( UPackageMap * InPackageMap, int64 InMaxBits )
:	FBitWriter (InMaxBits, true)
,	PackageMap( InPackageMap )
{

}

FArchive& FNetBitWriter::operator<<( class FName& N )
{
	PackageMap->SerializeName( *this, N );
	return *this;
}

FArchive& FNetBitWriter::operator<<( UObject*& Object )
{
	PackageMap->SerializeObject( *this, UObject::StaticClass(), Object );
	return *this;
}

FArchive& FNetBitWriter::operator<<(FStringAssetReference& Value)
{
	return *this << Value.AssetLongPathname;
}


// ----------------------------------------------------------------
//	FNetBitReader
// ----------------------------------------------------------------
FNetBitReader::FNetBitReader( UPackageMap *InPackageMap, uint8* Src, int64 CountBits )
:	FBitReader	(Src, CountBits)
,   PackageMap  ( InPackageMap )
{
}

FArchive& FNetBitReader::operator<<( UObject*& Object )
{
	PackageMap->SerializeObject( *this, UObject::StaticClass(), Object );
	return *this;
}

FArchive& FNetBitReader::operator<<( class FName& N )
{
	PackageMap->SerializeName( *this, N );
	return *this;
}

FArchive& FNetBitReader::operator<<(FStringAssetReference& Value)
{
	return *this << Value.AssetLongPathname;
}

static const TCHAR* GLastRPCFailedReason = NULL;

void RPC_ResetLastFailedReason()
{
	GLastRPCFailedReason = NULL;
}
void RPC_ValidateFailed( const TCHAR* Reason )
{
	GLastRPCFailedReason = Reason;
}

const TCHAR* RPC_GetLastFailedReason()
{
	return GLastRPCFailedReason;
}
