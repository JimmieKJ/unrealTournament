// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
ImageUtils.cpp: Image utility functions.
=============================================================================*/

#include "EnginePrivate.h"
#include "ImageUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogImageUtils, Log, All);

/**
 * Resizes the given image using a simple average filter and stores it in the destination array.
 *
 * @param SrcWidth		Source image width.
 * @param SrcHeight		Source image height.
 * @param SrcData		Source image data.
 * @param DstWidth		Destination image width.
 * @param DstHeight		Destination image height.
 * @param DstData		Destination image data.
 * @param bLinearSpace	If true, convert colors into linear space before interpolating (slower but more accurate)
 */
void FImageUtils::ImageResize(int32 SrcWidth, int32 SrcHeight, const TArray<FColor> &SrcData, int32 DstWidth, int32 DstHeight, TArray<FColor> &DstData, bool bLinearSpace )
{
	DstData.Empty(DstWidth*DstHeight);
	DstData.AddZeroed(DstWidth*DstHeight);

	float SrcX = 0;
	float SrcY = 0;

	const float StepSizeX = SrcWidth / (float)DstWidth;
	const float StepSizeY = SrcHeight / (float)DstHeight;

	for(int32 Y=0; Y<DstHeight;Y++)
	{
		int32 PixelPos = Y * DstWidth;
		SrcX = 0.0f;	
	
		for(int32 X=0; X<DstWidth; X++)
		{
			int32 PixelCount = 0;
			float EndX = SrcX + StepSizeX;
			float EndY = SrcY + StepSizeY;
			
			// Generate a rectangular region of pixels and then find the average color of the region.
			int32 PosY = FMath::TruncToInt(SrcY+0.5f);
			PosY = FMath::Clamp<int32>(PosY, 0, (SrcHeight - 1));

			int32 PosX = FMath::TruncToInt(SrcX+0.5f);
			PosX = FMath::Clamp<int32>(PosX, 0, (SrcWidth - 1));

			int32 EndPosY = FMath::TruncToInt(EndY+0.5f);
			EndPosY = FMath::Clamp<int32>(EndPosY, 0, (SrcHeight - 1));

			int32 EndPosX = FMath::TruncToInt(EndX+0.5f);
			EndPosX = FMath::Clamp<int32>(EndPosX, 0, (SrcWidth - 1));

			FColor FinalColor;
			if(bLinearSpace)
			{
				FLinearColor LinearStepColor(0.0f,0.0f,0.0f,0.0f);
				for(int32 PixelX = PosX; PixelX <= EndPosX; PixelX++)
				{
					for(int32 PixelY = PosY; PixelY <= EndPosY; PixelY++)
					{
						int32 StartPixel =  PixelX + PixelY * SrcWidth;

						// Convert from gamma space to linear space before the addition.
						LinearStepColor += SrcData[StartPixel];
						PixelCount++;
					}
				}
				LinearStepColor /= (float)PixelCount;

				// Convert back from linear space to gamma space.
				FinalColor = LinearStepColor.ToFColor(true);
			}
			else
			{
				FVector StepColor(0,0,0);
				for(int32 PixelX = PosX; PixelX <= EndPosX; PixelX++)
				{
					for(int32 PixelY = PosY; PixelY <= EndPosY; PixelY++)
					{
						int32 StartPixel =  PixelX + PixelY * SrcWidth;
						StepColor.X += (float)SrcData[StartPixel].R;
						StepColor.Y += (float)SrcData[StartPixel].G;
						StepColor.Z += (float)SrcData[StartPixel].B;
						PixelCount++;
					}
				}
				StepColor /= (float)PixelCount;
				uint8 FinalR = FMath::Clamp(FMath::TruncToInt(StepColor.X), 0, 255);
				uint8 FinalG = FMath::Clamp(FMath::TruncToInt(StepColor.Y), 0, 255);
				uint8 FinalB = FMath::Clamp(FMath::TruncToInt(StepColor.Z), 0, 255);
				FinalColor = FColor(FinalR, FinalG, FinalB);
			}

			// Store the final averaged pixel color value.
			FinalColor.A = 255;
			DstData[PixelPos] = FinalColor;

			SrcX = EndX;	
			PixelPos++;
		}

		SrcY += StepSizeY;
	}
}

/**
 * Creates a 2D texture from a array of raw color data.
 *
 * @param SrcWidth		Source image width.
 * @param SrcHeight		Source image height.
 * @param SrcData		Source image data.
 * @param Outer			Outer for the texture object.
 * @param Name			Name for the texture object.
 * @param Flags			Object flags for the texture object.
 * @param InParams		Params about how to set up the texture.
 * @return				Returns a pointer to the constructed 2D texture object.
 *
 */
UTexture2D* FImageUtils::CreateTexture2D(int32 SrcWidth, int32 SrcHeight, const TArray<FColor> &SrcData, UObject* Outer, const FString& Name, const EObjectFlags &Flags, const FCreateTexture2DParameters& InParams)
{
#if WITH_EDITOR
	UTexture2D* Tex2D;

	Tex2D = NewObject<UTexture2D>(Outer, FName(*Name), Flags);
	Tex2D->Source.Init(SrcWidth, SrcHeight, /*NumSlices=*/ 1, /*NumMips=*/ 1, TSF_BGRA8);
	
	// Create base mip for the texture we created.
	uint8* MipData = Tex2D->Source.LockMip(0);
	for( int32 y=0; y<SrcHeight; y++ )
	{
		uint8* DestPtr = &MipData[(SrcHeight - 1 - y) * SrcWidth * sizeof(FColor)];
		FColor* SrcPtr = const_cast<FColor*>(&SrcData[(SrcHeight - 1 - y) * SrcWidth]);
		for( int32 x=0; x<SrcWidth; x++ )
		{
			*DestPtr++ = SrcPtr->B;
			*DestPtr++ = SrcPtr->G;
			*DestPtr++ = SrcPtr->R;
			if( InParams.bUseAlpha )
			{
				*DestPtr++ = SrcPtr->A;
			}
			else
			{
				*DestPtr++ = 0xFF;
			}
			SrcPtr++;
		}
	}
	Tex2D->Source.UnlockMip(0);

	// Set the Source Guid/Hash if specified
	if (InParams.SourceGuidHash.IsValid())
	{
		Tex2D->Source.SetId(InParams.SourceGuidHash, true);
	}

	// Set compression options.
	Tex2D->SRGB = InParams.bSRGB;
	Tex2D->CompressionSettings	= InParams.CompressionSettings;
	if( !InParams.bUseAlpha )
	{
		Tex2D->CompressionNoAlpha = true;
	}
	Tex2D->DeferCompression	= InParams.bDeferCompression;

	Tex2D->PostEditChange();
	return Tex2D;
#else
	UE_LOG(LogImageUtils, Fatal,TEXT("ConstructTexture2D not supported on console."));
	return NULL;
#endif
}

void FImageUtils::CropAndScaleImage( int32 SrcWidth, int32 SrcHeight, int32 DesiredWidth, int32 DesiredHeight, const TArray<FColor> &SrcData, TArray<FColor> &DstData  )
{
	// Get the aspect ratio, and calculate the dimension of the image to crop
	float DesiredAspectRatio = (float)DesiredWidth/(float)DesiredHeight;

	float MaxHeight = (float)SrcWidth / DesiredAspectRatio;
	float MaxWidth = (float)SrcWidth;

	if ( MaxHeight > (float)SrcHeight)
	{
		MaxHeight = (float)SrcHeight;
		MaxWidth = MaxHeight * DesiredAspectRatio;
	}

	// Store crop width and height as ints for convenience
	int32 CropWidth = FMath::FloorToInt(MaxWidth);
	int32 CropHeight = FMath::FloorToInt(MaxHeight);

	// Array holding the cropped image
	TArray<FColor> CroppedData;
	CroppedData.AddUninitialized(CropWidth*CropHeight);

	int32 CroppedSrcTop  = 0;
	int32 CroppedSrcLeft = 0;

	// Set top pixel if we are cropping height
	if ( CropHeight < SrcHeight )
	{
		CroppedSrcTop = ( SrcHeight - CropHeight ) / 2;
	}

	// Set width pixel if cropping width
	if ( CropWidth < SrcWidth )
	{
		CroppedSrcLeft = ( SrcWidth - CropWidth ) / 2;
	}

	const int32 DataSize = sizeof(FColor);

	//Crop the image
	for (int32 Row = 0; Row < CropHeight; Row++)
	{
	 	//Row*Side of a row*byte per color
	 	int32 SrcPixelIndex = (CroppedSrcTop+Row)*SrcWidth + CroppedSrcLeft;
	 	const void* SrcPtr = &(SrcData[SrcPixelIndex]);
	 	void* DstPtr = &(CroppedData[Row*CropWidth]);
	 	FMemory::Memcpy(DstPtr, SrcPtr, CropWidth*DataSize);
	}

	// Scale the image
	DstData.AddUninitialized(DesiredWidth*DesiredHeight);

	// Resize the image
	FImageUtils::ImageResize( MaxWidth, MaxHeight, CroppedData, DesiredWidth, DesiredHeight, DstData, true );
}

void FImageUtils::CompressImageArray( int32 ImageWidth, int32 ImageHeight, TArray<FColor> &SrcData, TArray<uint8> &DstData )
{
	// PNGs are saved as RGBA but FColors are stored as BGRA. An option to swap the order upon compression may be added at some point. At the moment, manually swapping Red and Blue 
	for ( int32 Index = 0; Index < ImageWidth*ImageHeight; Index++ )
	{
		uint8 TempRed = SrcData[Index].R;
		SrcData[Index].R = SrcData[Index].B;
		SrcData[Index].B = TempRed;
	}

	FObjectThumbnail TempThumbnail;
	TempThumbnail.SetImageSize( ImageWidth, ImageHeight );
	TArray<uint8>& ThumbnailByteArray = TempThumbnail.AccessImageData();

	// Copy scaled image into destination thumb
	int32 MemorySize = ImageWidth*ImageHeight*sizeof(FColor);
	ThumbnailByteArray.AddUninitialized(MemorySize);
	FMemory::Memcpy(&(ThumbnailByteArray[0]), &(SrcData[0]), MemorySize);

	// Compress data - convert into a .png
	TempThumbnail.CompressImageData();
	TArray<uint8>& CompressedByteArray = TempThumbnail.AccessCompressedImageData();
	DstData = TempThumbnail.AccessCompressedImageData();
}

UTexture2D* FImageUtils::CreateCheckerboardTexture(FColor ColorOne, FColor ColorTwo, int32 CheckerSize)
{
	CheckerSize = FMath::Min<uint32>( FMath::RoundUpToPowerOfTwo(CheckerSize), 4096 );
	const int32 HalfPixelNum = CheckerSize >> 1;

	// Create the texture
	UTexture2D* CheckerboardTexture = UTexture2D::CreateTransient(CheckerSize, CheckerSize, PF_B8G8R8A8);

	// Lock the checkerboard texture so it can be modified
	FColor* MipData = static_cast<FColor*>( CheckerboardTexture->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE) );

	// Fill in the colors in a checkerboard pattern
	for ( int32 RowNum = 0; RowNum < CheckerSize; ++RowNum )
	{
		for ( int32 ColNum = 0; ColNum < CheckerSize; ++ColNum )
		{
			FColor& CurColor = MipData[( ColNum + ( RowNum * CheckerSize ) )];

			if ( ColNum < HalfPixelNum )
			{
				CurColor = ( RowNum < HalfPixelNum ) ? ColorOne : ColorTwo;
			}
			else
			{
				CurColor = ( RowNum < HalfPixelNum ) ? ColorTwo : ColorOne;
			}
		}
	}

	// Unlock the texture
	CheckerboardTexture->PlatformData->Mips[0].BulkData.Unlock();
	CheckerboardTexture->UpdateResource();

	return CheckerboardTexture;
}
