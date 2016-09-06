//! @file SubstanceImageInputFactoryClasses.cpp
//! @brief Factory to create Substance Image Inputs
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#include "SubstanceEditorPrivatePCH.h"

#include "SubstanceCoreTypedefs.h"
#include "SubstanceCoreHelpers.h"
#include "SubstanceCoreClasses.h"


bool USubstanceImageInputFactory::bSuppressImportOverwriteDialog = false;


USubstanceImageInputFactory::USubstanceImageInputFactory(const class FObjectInitializer& PCIP):Super(PCIP)
{
	SupportedClass	= USubstanceImageInput::StaticClass();

	// format of the file to import
	Formats.Add(TEXT("jpeg;Substance Image Input"));
	Formats.Add(TEXT("jpg;Substance Image Input"));
	Formats.Add(TEXT("tga;Substance Image Input"));

	// imports binary data via FactoryCreateBinary
	bText			= false;
	bCreateNew		= false;
	bEditorImport   = 1;
	ImportPriority	= -1;
}


USubstanceImageInput* USubstanceImageInputFactory::FactoryCreateBinaryFromJpeg(
	UObject* InParent, 
	FName Name,
	const uint8*& Buffer,
	const uint8*	BufferEnd,
	FFeedbackContext* Warn)
{
	check(NULL != Buffer);
	check(NULL != BufferEnd);

	int32 Width = 0;
	int32 Height = 0;
	int32 Length = BufferEnd - Buffer;

	TArray<uint8> DecompressedImage;
	Substance::Helpers::DecompressJpeg(
		Buffer, Length, DecompressedImage, &Width, &Height);

	if (DecompressedImage.Num() <= 0)
	{
		UE_LOG(LogSubstanceEditor, Error, TEXT("Image Input import failed: Failed to decompress Image."));
		return NULL;
	}

	/*free the decompressed version once the decompression succeeded*/
	DecompressedImage.Empty();

	if (Width < 16 || Height < 16)
	{
		UE_LOG(LogSubstanceEditor, Error, TEXT("Image Input import failed: Image too small, minimum size is 16x16"));
		return NULL;
	}
	else if (Width > 2048 || Height > 2048)
	{
		UE_LOG(LogSubstanceEditor, Error, TEXT("Image Input import failed: Image too large, maximum size is 2048x2048"));
		return NULL;
	}

	USubstanceImageInput* ImageInput = NewObject<USubstanceImageInput>(
			InParent,
			Name,
			RF_Standalone|RF_Public);

	ImageInput->ImageRGB.Lock(LOCK_READ_WRITE);
	uint32* DestImageData = (uint32*) ImageInput->ImageRGB.Realloc(Length);
	FMemory::Memcpy(DestImageData, Buffer, Length);
	ImageInput->ImageRGB.Unlock();

	ImageInput->SizeX = Width;
	ImageInput->SizeY = Height;
	ImageInput->NumComponents = 3;

	ImageInput->SourceFilePath = 
		IFileManager::Get().ConvertToRelativePath(*GetCurrentFilename());
	ImageInput->SourceFileTimestamp = IFileManager::Get().GetTimeStamp( *ImageInput->SourceFilePath ).ToString();

	ImageInput->CompressionRGB = 1;
	ImageInput->CompressionAlpha = 0;

	return ImageInput;
}


void USubstanceImageInputFactory::SuppressImportOverwriteDialog()
{
	bSuppressImportOverwriteDialog = true;
}


USubstanceImageInput* USubstanceImageInputFactory::FactoryCreateBinaryFromTga(
	UObject* InParent, 
	FName Name,
	const uint8*& Buffer,
	const uint8*	BufferEnd,
	FFeedbackContext* Warn)
{
	const FTGAFileHeader* TGA = (FTGAFileHeader *)Buffer;
	int32 Length = BufferEnd - Buffer;
	int32 Width = TGA->Width;
	int32 Height = TGA->Height;

	int32 TextureDataSizeRGBA = TGA->Width * TGA->Height * 4;
	uint32* DecompressedImageRGBA = NULL;

	int32 TextureDataSizeA = TGA->Width * TGA->Height;
	uint32* DecompressedImageA = NULL;

	bool res = false;

	USubstanceImageInput* ImageInput = NULL;

	if (TGA->BitsPerPixel == 24 || TGA->BitsPerPixel == 32)
	{
		DecompressedImageRGBA = (uint32*)FMemory::Malloc(TextureDataSizeRGBA);
		res = DecompressTGA_helper(TGA, DecompressedImageRGBA, TextureDataSizeRGBA, Warn);

		if (false == res)
		{
			FMemory::Free(DecompressedImageRGBA);
			GWarn->Log( TEXT("-- cannot import: failed to decode TGA image."));

			return NULL;
		}

		ImageInput = NewObject<USubstanceImageInput>(
				InParent,
				Name,
				RF_Standalone|RF_Public);

		if (TGA->BitsPerPixel == 24 )
		{
			ImageInput->NumComponents = 3;
			ImageInput->CompressionRGB = 1;
			ImageInput->CompressionAlpha = 0;
		}
		else if (TGA->BitsPerPixel == 32)
		{
			DecompressedImageA = (uint32*)FMemory::Malloc(TextureDataSizeA);

			ImageInput->NumComponents = 4;
			ImageInput->CompressionRGB = 1;
			ImageInput->CompressionAlpha = 1;
		
			Substance::Helpers::Split_RGBA_8bpp(
				Width, Height,
				(uint8*)DecompressedImageRGBA, TextureDataSizeRGBA,
				(uint8*)DecompressedImageA, TextureDataSizeA);
		}
	}
	else
	{
		GWarn->Log( TEXT("-- cannot import: unsupported TGA format (supported formats are RGB (24bpp) and RGBA (32bpp)."));
		return NULL;
	}

	TArray<uint8> CompressedImageRGB;
	int SizeCompressedImageRGB = 0;

	TArray<uint8> CompressedImageA;
	int SizeCompressedImageA = 0;

	if (DecompressedImageRGBA)
	{
		Substance::Helpers::CompressJpeg(
			(uint8*)DecompressedImageRGBA,
			TextureDataSizeRGBA,
			Width,
			Height,
			4,
			CompressedImageRGB);
		FMemory::Free(DecompressedImageRGBA);
	}

	if (DecompressedImageA)
	{
		Substance::Helpers::CompressJpeg(
			(uint8*)DecompressedImageA,
			TextureDataSizeA,
			Width,
			Height,
			1,
			CompressedImageA);
		FMemory::Free(DecompressedImageA);
	}

	/*Copy the compressed version in the image input struct*/
	if (CompressedImageRGB.Num() == 0 && CompressedImageA.Num() == 0)
	{
		ImageInput->ClearFlags(RF_Standalone);
		return NULL;
	}

	if (CompressedImageRGB.Num() != 0)
	{
		ImageInput->ImageRGB.Lock(LOCK_READ_WRITE);
		uint32* DestImageData =
			(uint32*) ImageInput->ImageRGB.Realloc(CompressedImageRGB.Num());

		FMemory::Memcpy(DestImageData, CompressedImageRGB.GetData(), CompressedImageRGB.Num());
		ImageInput->ImageRGB.Unlock();

		CompressedImageRGB.Empty();
	}

	if (CompressedImageA.Num() != 0)
	{
		ImageInput->ImageA.Lock(LOCK_READ_WRITE);
		uint32* DestImageData =
			(uint32*) ImageInput->ImageA.Realloc(CompressedImageA.Num());

		FMemory::Memcpy(DestImageData, CompressedImageA.GetData(), CompressedImageA.Num());
		ImageInput->ImageA.Unlock();

		CompressedImageA.Empty();
	}

	ImageInput->SourceFilePath =
		IFileManager::Get().ConvertToRelativePath(*GetCurrentFilename());
	ImageInput->SourceFileTimestamp = IFileManager::Get().GetTimeStamp(*ImageInput->SourceFilePath).ToString();
	
	ImageInput->SizeX = Width;
	ImageInput->SizeY = Height;

	return ImageInput;
}


USubstanceImageInput* USubstanceImageInputFactory::FactoryCreateBinaryFromTexture(
	UObject* InParent, 
	FName Name,
	UTexture2D* ContextTexture,
	const uint8*& BGRA_Buffer,
	const uint8*	BGRA_BufferEnd,
	FFeedbackContext* Warn)
{
	const uint8* DecompressedImage_RGBA = BGRA_Buffer;
	int32 Width = ContextTexture->GetSizeX();
	int32 Height = ContextTexture->GetSizeY();
	int32 DecompressedImageRGBA_Size = BGRA_BufferEnd - BGRA_Buffer;

	bool bTextureHasAlpha = false;

	for (int32 Y = Height - 1 ; Y >= 0 ; --Y)
	{
		const uint8* Color = &DecompressedImage_RGBA[Y * Width * 4];
		for (int32 X = Width ; X > 0; --X)
		{
			// Skip color info
			Color+=3;

			// Get Alpha value then increment the pointer past it for the next pixel
			uint8 Alpha = *Color++;
			if (Alpha != 255)
			{
				// When a texture is imported with no alpha, the alpha bits are set to 255
				// So if the texture has non 255 alpha values, the texture is a valid alpha channel
				bTextureHasAlpha = true;
				break;
			}
		}
		if (bTextureHasAlpha)
		{
			break;
		}
	}

	uint8* DecompressedImageRGB = NULL;
	int32 SizeDecompressedImageRGB = Width * Height * 3;

	uint8* DecompressedImageA = NULL;
	int32 SizeDecompressedImageA = Width * Height;

	USubstanceImageInput* ImageInput = NewObject<USubstanceImageInput>(
		InParent,
		Name,
		RF_Standalone|RF_Public);

	if (bTextureHasAlpha)
	{
		DecompressedImageA = (uint8*)FMemory::Malloc(SizeDecompressedImageA);

		ImageInput->NumComponents = 4;
		ImageInput->CompressionRGB = 1;
		ImageInput->CompressionAlpha = 1;

		Substance::Helpers::Split_RGBA_8bpp(
			Width, Height,
			(uint8*)DecompressedImage_RGBA, DecompressedImageRGBA_Size,
			DecompressedImageA, SizeDecompressedImageA);
	}

	TArray<uint8> CompressedImageRGB;
	int SizeCompressedImageRGB = 0;

	TArray<uint8> CompressedImageA;
	int SizeCompressedImageA = 0;

	if (DecompressedImage_RGBA)
	{
		Substance::Helpers::CompressJpeg(
			(uint8*)DecompressedImage_RGBA,
			DecompressedImageRGBA_Size,
			Width,
			Height,
			4,
			CompressedImageRGB);
		FMemory::Free(DecompressedImageRGB);
	}

	if (DecompressedImageA)
	{
		Substance::Helpers::CompressJpeg(
			(uint8*)DecompressedImageA,
			SizeDecompressedImageA,
			Width,
			Height,
			1,
			CompressedImageA);
		FMemory::Free(DecompressedImageA);
	}

	/*Copy the compressed version in the image input struct*/
	if (CompressedImageRGB.Num() <= 0 && CompressedImageA.Num() <= 0)
	{
		ImageInput->ClearFlags(RF_Standalone);
		return NULL;
	}

	if (CompressedImageRGB.Num())
	{
		ImageInput->ImageRGB.Lock(LOCK_READ_WRITE);
		uint32* DestImageData =
			(uint32*) ImageInput->ImageRGB.Realloc(SizeCompressedImageRGB);

		FMemory::Memcpy(DestImageData, &CompressedImageRGB[0], CompressedImageRGB.Num());
		ImageInput->ImageRGB.Unlock();
	}

	if (CompressedImageA.Num())
	{
		ImageInput->ImageA.Lock(LOCK_READ_WRITE);
		uint32* DestImageData =
			(uint32*) ImageInput->ImageA.Realloc(SizeCompressedImageA);

		FMemory::Memcpy(DestImageData, &CompressedImageA[0], CompressedImageA.Num());
		ImageInput->ImageA.Unlock();
	}

	ImageInput->SizeX = Width;
	ImageInput->SizeY = Height;

	return ImageInput;
}


// This function can be called with the following scenarios :
// * the user imported a jpeg file
// * the user imported a tga file
// * the user is transforming a Texture2D in an image input
//
UObject* USubstanceImageInputFactory::FactoryCreateBinary(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	EObjectFlags		Flags,
	UObject*			Context,
	const TCHAR*		Type,
	const uint8*&		Buffer,
	const uint8*		BufferEnd,
	FFeedbackContext*	Warn)
{
	UTexture2D* ContextTexture = Cast<UTexture2D>(Context);

	//TODO: handle bSuppressImportOverwriteDialog case

	if (FCString::Stricmp( Type, TEXT("tga") ) == 0)
	{
		return FactoryCreateBinaryFromTga(InParent, Name, Buffer, BufferEnd, Warn);
	}
	else if (FCString::Stricmp( Type, TEXT("jpeg") ) == 0 || 
		     FCString::Stricmp( Type, TEXT("jpg") ) == 0)
	{
		return FactoryCreateBinaryFromJpeg(InParent, Name, Buffer, BufferEnd, Warn);
	}
	else if (ContextTexture)
	{
		return FactoryCreateBinaryFromTexture(
			InParent, Name, ContextTexture, Buffer, BufferEnd, Warn);
	}

	return NULL;
}


UReimportSubstanceImageInputFactory::UReimportSubstanceImageInputFactory(class FObjectInitializer const & PCIP):Super(PCIP)
{

}


bool UReimportSubstanceImageInputFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	if (!Obj || !Obj->IsA(USubstanceImageInput::StaticClass()))
	{ 
		return false;
	}

	if (!Cast<USubstanceImageInput>(Obj)->SourceFilePath.Len())
	{
		UE_LOG(LogSubstanceEditor, Error, TEXT("Image Input import failed: no filename available in resource."));
		return false;
	}

	OutFilenames.Add(Cast<USubstanceImageInput>(Obj)->SourceFilePath);
	return true;
}


void UReimportSubstanceImageInputFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	USubstanceImageInput* ImgInput = Cast<USubstanceImageInput>(Obj);
	if(ImgInput && ensure(NewReimportPaths.Num() == 1))
	{
		ImgInput->SourceFilePath = NewReimportPaths[0];
	}
}


EReimportResult::Type UReimportSubstanceImageInputFactory::Reimport(UObject* Obj)
{
	// Suppress the import overwrite dialog, we want to keep existing settings when re-importing
	USubstanceImageInputFactory::SuppressImportOverwriteDialog();

	USubstanceImageInput* PrevImgInput = Cast<USubstanceImageInput>(Obj);

	// backup the list of previous consumers of the image input before reimporting
	TArray< USubstanceGraphInstance* > PreviousConsumers = PrevImgInput->Consumers;

	UObject* ImgInput = UFactory::StaticImportObject(
		Obj->GetClass(),
		Obj->GetOuter(),
		*Obj->GetName(),
		RF_Public | RF_Standalone,
		*PrevImgInput->SourceFilePath, NULL, this);

	// re-apply the the updated image input to the previous consumers to restore the link
	if (ImgInput)
	{
		for (auto itConsumer = PreviousConsumers.CreateIterator(); itConsumer; ++itConsumer)
		{
			(*itConsumer)->Instance->UpdateInput(
				0,
				ImgInput);

			Substance::Helpers::RenderAsync((*itConsumer)->Instance);
		}

		return EReimportResult::Succeeded;
	}
	else
	{
		return EReimportResult::Failed;
	}
}
