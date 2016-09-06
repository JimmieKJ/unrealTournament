//! @file SubstanceImageInput.cpp
//! @brief Implementation of the USubstanceImageInput class
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#include "SubstanceCorePrivatePCH.h"
#include "SubstanceCoreTypedefs.h"
#include "SubstanceInput.h"
#include "SubstanceImageInput.h"
#include "SubstanceCoreHelpers.h"
#include "SubstanceFGraph.h"

#if WITH_EDITOR
#include "Factories.h"
#endif

USubstanceImageInput::USubstanceImageInput(class FObjectInitializer const & PCIP) : Super(PCIP)
{

}


#if WITH_EDITOR
bool USubstanceImageInput::CanEditChange(const UProperty* InProperty) const
{
	if (SourceFilePath.EndsWith(".tga"))
	{
		if (InProperty->GetNameCPP() == TEXT("CompressionAlpha"))
		{
			if (ImageA.GetBulkDataSize() == 0)
			{
				return false;
			}
		}
		
		// check the image input is still available before allowing modification of compression level
		TUniquePtr<FArchive> FileReader(IFileManager::Get().CreateFileReader(*SourceFilePath));
		if (!FileReader)
		{
			return false;
		}

		return true;
	}
	else
	{
		return false;
	}
}


void USubstanceImageInput::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// change in the consumers array can come from the deletion of a graph instance
	// in this case we need to remove invalid entries
	if (PropertyChangedEvent.MemberProperty && PropertyChangedEvent.MemberProperty->GetNameCPP() == TEXT("Consumers"))
	{
		for (auto itConsumer = Consumers.CreateIterator(); itConsumer; ++itConsumer)
		{
			if ((*itConsumer) == 0)
			{
				Consumers.RemoveAt(itConsumer.GetIndex());
				itConsumer.Reset();
			}
		}
	}
	
	if (PropertyChangedEvent.MemberProperty && SourceFilePath.EndsWith(".tga"))
	{
		// start by reloading the image input
		TUniquePtr<FArchive> FileReader(IFileManager::Get().CreateFileReader(*SourceFilePath));

		// which could be missing
		if (!FileReader)
		{
			return;
		}

		uint32	BufferSize = FileReader->TotalSize();
		void* Buffer = FMemory::Malloc(BufferSize);
#if SUBSTANCE_MEMORY_STAT
		INC_MEMORY_STAT_BY(STAT_SubstanceMemory, BufferSize);
#endif

		FileReader->Serialize(Buffer, BufferSize);

		const FTGAFileHeader* TGA = (FTGAFileHeader *)Buffer;
		SizeX = TGA->Width;
		SizeY = TGA->Height;
		int32 TextureDataSizeRGBA = SizeX * SizeY * 4;
		uint32* DecompressedImageRGBA = (uint32*)FMemory::Malloc(TextureDataSizeRGBA);
#if SUBSTANCE_MEMORY_STAT
		INC_MEMORY_STAT_BY(STAT_SubstanceMemory, TextureDataSizeRGBA);
#endif

		DecompressTGA_helper(TGA, DecompressedImageRGBA, TextureDataSizeRGBA, NULL);
#if SUBSTANCE_MEMORY_STAT
		DEC_MEMORY_STAT_BY(STAT_SubstanceMemory, FMemory::GetAllocSize(Buffer));
#endif
		FMemory::Free(Buffer);
		Buffer = NULL; 

		// if the user modified the color channel compression level
		if (PropertyChangedEvent.MemberProperty->GetNameCPP() == TEXT("CompressionRGB") )
		{
			// replace the eventually compressed color channel by the file's content
			ImageRGB.RemoveBulkData();
			ImageRGB.Lock(LOCK_READ_WRITE);

			void* ImagePtr = ImageRGB.Realloc(TextureDataSizeRGBA);

			FMemory::Memcpy(ImagePtr, DecompressedImageRGBA, TextureDataSizeRGBA);

			// perform compression if not 0
			if (CompressionRGB != 0 && ImageRGB.GetBulkDataSize())
			{
				TArray<uint8> CompressedImageRGB;

				// for retro compatibility, 1 = default compression quality, otherwise, its the desired compression level 
				int32 Quality = FMath::Clamp(CompressionRGB == 1 ? 85 : CompressionRGB, 0, 100);

				Substance::Helpers::CompressJpeg(
					ImagePtr,
					ImageRGB.GetBulkDataSize(),
					SizeX,
					SizeY,
					4,
					CompressedImageRGB,
					Quality);

				ImagePtr = ImageRGB.Realloc(CompressedImageRGB.Num());
				FMemory::Memcpy(ImagePtr, CompressedImageRGB.GetData(), CompressedImageRGB.Num());
			}

			ImageRGB.Unlock();
		}
		else if (PropertyChangedEvent.MemberProperty->GetNameCPP() == TEXT("CompressionAlpha") && ImageA.GetBulkDataSize())
		{
			int32 TextureDataSizeA = SizeX * SizeY;
			uint32* DecompressedImageA = (uint32*)FMemory::Malloc(TextureDataSizeA);
#if SUBSTANCE_MEMORY_STAT
			INC_MEMORY_STAT_BY(STAT_SubstanceMemory, TextureDataSizeA);
#endif

			Substance::Helpers::Split_RGBA_8bpp(
				SizeX, SizeY,
				(uint8*)DecompressedImageRGBA, TextureDataSizeRGBA,
				(uint8*)DecompressedImageA, TextureDataSizeA);

			ImageA.RemoveBulkData();
			ImageA.Lock(LOCK_READ_WRITE);

			void* AlphaPtr = ImageA.Realloc(TextureDataSizeA);

			FMemory::Memcpy(AlphaPtr, DecompressedImageA, TextureDataSizeA);
#if SUBSTANCE_MEMORY_STAT
			DEC_MEMORY_STAT_BY(STAT_SubstanceMemory, FMemory::GetAllocSize(DecompressedImageA));
#endif
			FMemory::Free(DecompressedImageA);
			DecompressedImageA = NULL;

			if (CompressionAlpha != 0)
			{
				TArray<uint8> CompressedImageA;
				
				// for retro compatibility, 1 = default compression quality, otherwise, its the desired compression level 
				int32 Quality = FMath::Clamp(CompressionAlpha == 1 ? 85 : CompressionAlpha, 0, 100);

				Substance::Helpers::CompressJpeg(
					AlphaPtr,
					ImageA.GetBulkDataSize(),
					SizeX,
					SizeY,
					1,
					CompressedImageA,
					Quality);

				AlphaPtr = ImageA.Realloc(CompressedImageA.Num());
				FMemory::Memcpy(AlphaPtr, CompressedImageA.GetData(), CompressedImageA.Num());
			}

			ImageA.Unlock();
		}
#if SUBSTANCE_MEMORY_STAT
		DEC_MEMORY_STAT_BY(STAT_SubstanceMemory, FMemory::GetAllocSize(DecompressedImageRGBA));
#endif
		FMemory::Free(DecompressedImageRGBA);
		DecompressedImageRGBA = NULL;
	}
	else
	{
		CompressionRGB = 1;
	}
	
	// update eventual Substance using this image input
	for (auto itConsumer = Consumers.CreateIterator(); itConsumer; ++itConsumer)
	{
		(*itConsumer)->Instance->UpdateInput(
			0,
			this);

		Substance::Helpers::RenderAsync((*itConsumer)->Instance);
	}
}
#endif


void USubstanceImageInput::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar.UsingCustomVersion(FSubstanceCoreCustomVersion::GUID);

	//! todo: remove image date if all consumers are freezed
	if (Ar.IsSaving())
	{
		ImageRGB.StoreCompressedOnDisk(0 == CompressionRGB ? COMPRESS_ZLIB : COMPRESS_None);
		ImageA.StoreCompressedOnDisk(0 == CompressionAlpha ? COMPRESS_ZLIB : COMPRESS_None);
	}

	ImageRGB.Serialize(Ar, this);
	ImageA.Serialize(Ar, this);

	// image inputs can be used multiple times
	ImageRGB.ClearBulkDataFlags(BULKDATA_SingleUse); 
	ImageA.ClearBulkDataFlags(BULKDATA_SingleUse);

	Ar << CompressionRGB;
	Ar << CompressionAlpha;

	if (Ar.IsCooking())
	{
		SourceFilePath = FString();
		SourceFileTimestamp = FString();
	}
}


FString USubstanceImageInput::GetDesc()
{
	return FString::Printf( TEXT("%dx%d (%d kB)"), SizeX, SizeY, GetResourceSize(EResourceSizeMode::Exclusive)/1024);
}


SIZE_T USubstanceImageInput::GetResourceSize(EResourceSizeMode::Type)
{
	return ImageRGB.GetBulkDataSize() + ImageA.GetBulkDataSize();
}
