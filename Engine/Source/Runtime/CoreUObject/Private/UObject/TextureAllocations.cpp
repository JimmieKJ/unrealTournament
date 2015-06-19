// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"

#if WITH_ENGINE

#include "Runtime/RHI/Public/RHIDefinitions.h"	// For TexCreate* flags

/*-----------------------------------------------------------------------------
	Linker file package summary texture allocation info
-----------------------------------------------------------------------------*/

FTextureAllocations::FTextureType::FTextureType()
:	SizeX(0)
,	SizeY(0)
,	NumMips(0)
,	Format(0)
,	TexCreateFlags(0)
,	NumExportIndicesProcessed(0)
{
}

FTextureAllocations::FTextureType::FTextureType( int32 InSizeX, int32 InSizeY, int32 InNumMips, uint32 InFormat, uint32 InTexCreateFlags )
:	SizeX(InSizeX)
,	SizeY(InSizeY)
,	NumMips(InNumMips)
,	Format(InFormat)
,	TexCreateFlags(InTexCreateFlags)
,	NumExportIndicesProcessed(0)
{
}

/**
 * Serializes an FTextureType
 */
FArchive& operator<<( FArchive& Ar, FTextureAllocations::FTextureType& TextureType )
{
	Ar << TextureType.SizeX;
	Ar << TextureType.SizeY;
	Ar << TextureType.NumMips;
	Ar << TextureType.Format;
	Ar << TextureType.TexCreateFlags;
	Ar << TextureType.ExportIndices;
	return Ar;
}

/**
 * Finds a suitable ResourceMem allocation, removes it from this container and return it to the user.
 *
 * @param SizeX				Width of texture
 * @param SizeY				Height of texture
 * @param NumMips			Number of mips
 * @param Format			Texture format (EPixelFormat)
 * @param TexCreateFlags	ETextureCreateFlags bit flags
 **/
FTexture2DResourceMem* FTextureAllocations::FindAndRemove( int32 SizeX, int32 SizeY, int32 NumMips, uint32 Format, uint32 TexCreateFlags )
{
	FTexture2DResourceMem* ResourceMem = NULL;
	FTextureType* TextureType = FindTextureType( SizeX, SizeY, NumMips, Format, TexCreateFlags );
	if ( TextureType && TextureType->Allocations.Num() > 0 )
	{
		ResourceMem = TextureType->Allocations[0];
		ResourceMem->FinishAsyncAllocation();
		check( ResourceMem->HasAsyncAllocationCompleted() );
		TextureType->Allocations.RemoveAtSwap( 0 );
		PendingAllocationSize -= ResourceMem->GetResourceBulkDataSize();
	}
	return ResourceMem;
}

/**
 * Finds a texture type that matches the given specifications.
 *
 * @param SizeX				Width of the largest mip-level stored in the package
 * @param SizeY				Height of the largest mip-level stored in the package
 * @param NumMips			Number of mips
 * @param Format			Texture format (EPixelFormat)
 * @param TexCreateFlags	ETextureCreateFlags bit flags
 * @return					Matching texture type, or NULL if none was found
 */
FTextureAllocations::FTextureType* FTextureAllocations::FindTextureType( int32 SizeX, int32 SizeY, int32 NumMips, uint32 Format, uint32 TexCreateFlags )
{
	FTexture2DResourceMem* ResourceMem = NULL;
	const uint32 FlagMask = ~(TexCreate_AllowFailure | TexCreate_DisableAutoDefrag); // TexCreate_AllowFailure | TexCreate_DisableAutoDefrag (from ETextureCreateFlags in RHI.h)
	for ( int32 TypeIndex=0; TypeIndex < TextureTypes.Num(); ++TypeIndex )
	{
		FTextureType& TextureType = TextureTypes[ TypeIndex ];
		if ( TextureType.SizeX == SizeX &&
			 TextureType.SizeY == SizeY &&
			 TextureType.NumMips == NumMips &&
			 TextureType.Format == Format &&
			 ((TextureType.TexCreateFlags ^ TexCreateFlags) & FlagMask) == 0 )
		{
			return &TextureType;
		}
	}
	return NULL;
}

/**
 * Adds a dummy export index (-1) for a specified texture type.
 * Creates the texture type entry if needed.
 *
 * @param SizeX				Width of the largest mip-level stored in the package
 * @param SizeY				Height of the largest mip-level stored in the package
 * @param NumMips			Number of mips
 * @param Format			Texture format (EPixelFormat)
 * @param TexCreateFlags	ETextureCreateFlags bit flags
 */
void FTextureAllocations::AddResourceMemInfo( int32 SizeX, int32 SizeY, int32 NumMips, uint32 Format, uint32 TexCreateFlags )
{
	FTextureType* TextureType = FindTextureType( SizeX, SizeY, NumMips, Format, TexCreateFlags );
	if ( TextureType == NULL )
	{
		TextureType = new (TextureTypes) FTextureType( SizeX, SizeY, NumMips, Format, TexCreateFlags );
	}
	TextureType->ExportIndices.Add(-1);
}

FTextureAllocations::FTextureAllocations( const FTextureAllocations& Other )
:	TextureTypes( Other.TextureTypes )
,	PendingAllocationCount( Other.PendingAllocationCount.GetValue() )
,	PendingAllocationSize( Other.PendingAllocationSize )
,	NumTextureTypesConsidered( Other.NumTextureTypesConsidered )
{
}

void FTextureAllocations::operator=(const FTextureAllocations& Other)
{
	TextureTypes = Other.TextureTypes;
	PendingAllocationCount.Set( Other.PendingAllocationCount.GetValue() );
	PendingAllocationSize = Other.PendingAllocationSize;
	NumTextureTypesConsidered = Other.NumTextureTypesConsidered;
}

/**
 * Cancels any pending ResourceMem allocation that hasn't been claimed by a texture yet,
 * just in case there are any mismatches at run-time.
 *
 * @param bCancelEverything		If true, cancels all allocations. If false, only cancels allocations that haven't been completed yet.
 */
void FTextureAllocations::CancelRemainingAllocations( bool bCancelEverything )
{
	int32 NumRemainingAllocations = 0;
	int32 RemainingBulkSize = 0;
	if ( !HasBeenFullyClaimed() )
	{
		for ( int32 TypeIndex=0; TypeIndex < TextureTypes.Num(); ++TypeIndex )
		{
			FTextureAllocations::FTextureType& TextureType = TextureTypes[ TypeIndex ];
			for ( int32 ResourceIndex=0; ResourceIndex < TextureType.Allocations.Num(); ++ResourceIndex )
			{
				FTexture2DResourceMem* ResourceMem = TextureType.Allocations[ ResourceIndex ];
				int32 BulkDataSize = ResourceMem->GetResourceBulkDataSize();
				RemainingBulkSize += BulkDataSize;
				NumRemainingAllocations++;
				if ( bCancelEverything || ResourceMem->HasAsyncAllocationCompleted() == false )
				{
					ResourceMem->CancelAsyncAllocation();
					delete ResourceMem;
					TextureType.Allocations.RemoveAtSwap( ResourceIndex-- );
					PendingAllocationSize -= BulkDataSize;
				}
			}
		}
	}

	check( HasCompleted() );
	check( !bCancelEverything || HasBeenFullyClaimed() );
}

/**
 * Serializes an FTextureAllocations struct
 */
FArchive& operator<<( FArchive& Ar, FTextureAllocations& TextureAllocations )
{
	int32 NumSummaryTextures = 0;
	int32 NumExportTexturesTotal = 0;
	int32 NumExportTexturesAdded = 0;

	#if UE4_TODO
		// Are we cooking a package?
		if ( Ar.IsSaving() && !Ar.IsTransacting() )
		{
			FLinker* Linker = Ar.GetLinker();

			// Do we need to build the texture allocation data?
			if ( TextureAllocations.TextureTypes.Num() == 0 )
			{
				TArray<UObject*> TagExpObjects;
				GetObjectsWithAnyMarks(TagExpObjects, OBJECTMARK_TagExp);
				for(int32 Index = 0; Index < TagExpObjects.Num(); Index++)
				{
					UObject* Object = TagExpObjects(Index);
					check(Object->HasAnyMarks(OBJECTMARK_TagExp));
					if ( !Object->HasAnyFlags(RF_ClassDefaultObject) )
					{
						if (UTexture2D* Texture2D = dynamic_cast<UTexture2D*>(Object))
						{
							int32 PreAllocateSizeX = 0;
							int32 PreAllocateSizeY = 0;
							int32 PreAllocateNumMips = 0;
							uint32 TexCreateFlags = 0;
							if ( Texture2D->GetResourceMemSettings(Texture2D->FirstResourceMemMip, PreAllocateSizeX, PreAllocateSizeY, PreAllocateNumMips, TexCreateFlags ) )
							{
								TextureAllocations.AddResourceMemInfo(PreAllocateSizeX, PreAllocateSizeY, PreAllocateNumMips, Texture2D->Format, TexCreateFlags );
							}
						}
					}
				}
			}
			// Do we need to fixup the export indices?
			else if ( Ar.GetLinker() )
			{
				NumSummaryTextures = 0;
				FLinker* Linker = Ar.GetLinker();
				for ( int32 TypeIndex=0; TypeIndex < TextureAllocations.TextureTypes.Num(); ++TypeIndex )
				{
					FTextureAllocations::FTextureType& TextureType = TextureAllocations.TextureTypes( TypeIndex );
					NumSummaryTextures += TextureType.ExportIndices.Num();
					TextureType.ExportIndices.Empty();
				}

				NumExportTexturesTotal = 0;
				NumExportTexturesAdded = 0;
				for ( int32 ExportIndex=0; ExportIndex < Linker->ExportMap.Num(); ++ExportIndex )
				{
					UTexture2D* Texture2D = dynamic_cast<UTexture2D*>(Linker->ExportMap(ExportIndex).Object);
					if ( Texture2D && !Texture2D->HasAnyFlags(RF_ClassDefaultObject) )
					{
						NumExportTexturesTotal++;
						int32 PreAllocateSizeX = 0;
						int32 PreAllocateSizeY = 0;
						int32 PreAllocateNumMips = 0;
						uint32 TexCreateFlags = 0;
						if ( Texture2D->GetResourceMemSettings(Texture2D->FirstResourceMemMip, PreAllocateSizeX, PreAllocateSizeY, PreAllocateNumMips, TexCreateFlags ) )
						{
							FTextureAllocations::FTextureType* TextureType = TextureAllocations.FindTextureType(PreAllocateSizeX, PreAllocateSizeY, PreAllocateNumMips, Texture2D->Format, TexCreateFlags);
							check( TextureType );
							TextureType->ExportIndices.Add( ExportIndex );
							NumExportTexturesAdded++;
						}
					}
				}
				check( NumSummaryTextures == NumExportTexturesAdded );
			}
		}
	#endif

	Ar << TextureAllocations.TextureTypes;

	TextureAllocations.PendingAllocationSize = 0;
	TextureAllocations.PendingAllocationCount.Reset();

	return Ar;
}

#endif // WITH_ENGINE
