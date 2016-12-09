// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ImageComparer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

#include "Interfaces/IImageWrapperModule.h"
#include "Async/ParallelFor.h"

#define LOCTEXT_NAMESPACE "ImageComparer"

const FImageTolerance FImageTolerance::DefaultIgnoreNothing(0, 0, 0, 0, 0, 255, false, false, 0.00f, 0.00f);
const FImageTolerance FImageTolerance::DefaultIgnoreLess(16, 16, 16, 16, 16, 240, false, false, 0.02f, 0.02f);
const FImageTolerance FImageTolerance::DefaultIgnoreAntiAliasing(32, 32, 32, 32, 64, 96, true, false, 0.02f, 0.02f);
const FImageTolerance FImageTolerance::DefaultIgnoreColors(16, 16, 16, 16, 16, 240, false, true, 0.02f, 0.02f);

class FImageDelta
{
public:
	int32 Width;
	int32 Height;
	TArray<uint8> Image;

	FImageDelta(int32 InWidth, int32 InHeight)
		: Width(InWidth)
		, Height(InHeight)
	{
		Image.SetNumUninitialized(Width * Height * 4);
	}

	FString ComparisonFile;

	FORCEINLINE void SetPixel(int32 X, int32 Y, FColor Color)
	{
		int32 Offset = ( Y * Width + X ) * 4;
		check(Offset < ( Width * Height * 4 ));

		Image[Offset] = Color.R;
		Image[Offset + 1] = Color.G;
		Image[Offset + 2] = Color.B;
		Image[Offset + 3] = Color.A;
	}

	FORCEINLINE void SetPixelGrayScale(int32 X, int32 Y, FColor Color)
	{
		int32 Offset = ( Y * Width + X ) * 4;
		check(Offset < ( Width * Height * 4 ));

		const float Brightness = FPixelOperations::GetBrightness(Color);

		Image[Offset] = Brightness;
		Image[Offset + 1] = Brightness;
		Image[Offset + 2] = Brightness;
		Image[Offset + 3] = Color.A;
	}

	FORCEINLINE void SetClearPixel(int32 X, int32 Y)
	{
		int32 Offset = ( Y * Width + X ) * 4;
		check(Offset < ( Width * Height * 4 ));

		Image[Offset] = 0;
		Image[Offset + 1] = 0;
		Image[Offset + 2] = 0;
		Image[Offset + 3] = 255;
	}

	FORCEINLINE void SetErrorPixel(int32 X, int32 Y, FColor ErrorColor = FColor(255, 255, 255, 255))
	{
		int32 Offset = ( Y * Width + X ) * 4;
		check(Offset < ( Width * Height * 4 ));

		Image[Offset] = ErrorColor.R;
		Image[Offset + 1] = ErrorColor.G;
		Image[Offset + 2] = ErrorColor.B;
		Image[Offset + 3] = ErrorColor.A;
	}

	void Save(FString OutputDirectory)
	{
		FString TempDir = OutputDirectory.IsEmpty() ? FString(FPlatformProcess::UserTempDir()) : OutputDirectory;
		FString TempDeltaFile = FPaths::CreateTempFilename(*TempDir, TEXT("ImageCompare-"), TEXT(".png"));

		IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
		IImageWrapperPtr ImageWriter = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

		if ( ImageWriter.IsValid() )
		{
			if ( ImageWriter->SetRaw(Image.GetData(), Image.Num(), Width, Height, ERGBFormat::RGBA, 8) )
			{
				const TArray<uint8>& PngData = ImageWriter->GetCompressed();

				if ( FFileHelper::SaveArrayToFile(PngData, *TempDeltaFile) )
				{
					ComparisonFile = FPaths::GetCleanFilename(TempDeltaFile);
				}
			}
		}
	}
};

float FPixelOperations::GetHue(const FColor& Color)
{
	float R = Color.R / 255.0f;
	float G = Color.G / 255.0f;
	float B = Color.B / 255.0f;

	float Max = FMath::Max3(R, G, B);
	float Min = FMath::Min3(R, G, B);

	if ( Max == Min )
	{
		return 0; // achromatic
	}
	else
	{
		float Hue = 0;
		float Delta = Max - Min;

		if ( Max == R )
		{
			Hue = ( G - B ) / Delta + ( G < B ? 6 : 0 );
		}
		else if ( Max == G )
		{
			Hue = ( B - R ) / Delta + 2;
		}
		else if ( Max == B )
		{
			Hue = ( R - G ) / Delta + 4;
		}

		return Hue /= 6.0f;
	}
}

bool FPixelOperations::IsAntialiased(const FColor& SourcePixel, FComparableImage* Image, int32 X, int32 Y, const FImageTolerance& Tolerance)
{
	int32 hasHighContrastSibling = 0;
	int32 hasSiblingWithDifferentHue = 0;
	int32 hasEquivalentSibling = 0;

	float SourceHue = GetHue(SourcePixel);

	int32 Distance = 1;
	for ( int32 i = Distance * -1; i <= Distance; i++ )
	{
		for ( int32 j = Distance * -1; j <= Distance; j++ )
		{
			if ( i == 0 && j == 0 )
			{
				// ignore source pixel
			}
			else
			{
				if ( !Image->CanGetPixel(X + j, Y + i) )
				{
					continue;
				}

				FColor TargetPixel = Image->GetPixel(X + j, Y + i);

				float TargetPixelBrightness = GetBrightness(TargetPixel);
				float TargetPixelHue = GetHue(TargetPixel);

				if ( FPixelOperations::IsContrasting(SourcePixel, TargetPixel, Tolerance) )
				{
					hasHighContrastSibling++;
				}

				if ( FPixelOperations::IsRGBSame(SourcePixel, TargetPixel) )
				{
					hasEquivalentSibling++;
				}

				if ( FMath::Abs(SourceHue - TargetPixelHue) > 0.3 )
				{
					hasSiblingWithDifferentHue++;
				}

				if ( hasSiblingWithDifferentHue > 1 || hasHighContrastSibling > 1 )
				{
					return true;
				}
			}
		}
	}

	if ( hasEquivalentSibling < 2 )
	{
		return true;
	}

	return false;
}

void FComparableImage::Process()
{
	ParallelFor(Width,
		[&] (int32 ColumnIndex)
	{
		for ( int Y = 0; Y < Height; Y++ )
		{
			FColor Pixel = GetPixel(ColumnIndex, Y);
			float Brightness = FPixelOperations::GetBrightness(Pixel);

			RedTotal += ( Pixel.R / 255.0 );
			GreenTotal += ( Pixel.G / 255.0 );
			BlueTotal += ( Pixel.B / 255.0 );
			AlphaTotal += ( Pixel.A / 255.0 );
			BrightnessTotal += ( Brightness / 255.0 );
		}
	});

	double PixelCount = Width * Height;

	RedAverage = RedTotal / PixelCount;
	GreenAverage = GreenTotal / PixelCount;
	BlueAverage = BlueTotal / PixelCount;
	AlphaAverage = AlphaTotal / PixelCount;
	BbrightnessAverage = BrightnessTotal / PixelCount;
}

TSharedPtr<FComparableImage> FImageComparer::Open(const FString& ImagePath, FText& OutError)
{
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	IImageWrapperPtr ImageReader = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

	if ( !ImageReader.IsValid() )
	{
		OutError = LOCTEXT("PNGWrapperMissing", "Unable locate the PNG Image Processor");
		return nullptr;
	}

	TArray<uint8> PngData;
	const bool OpenSuccess = FFileHelper::LoadFileToArray(PngData, *ImagePath);

	if ( !OpenSuccess )
	{
		OutError = LOCTEXT("ErrorOpeningImageA", "Unable to read image");
		return nullptr;
	}

	if ( !ImageReader->SetCompressed(PngData.GetData(), PngData.Num()) )
	{
		OutError = LOCTEXT("ErrorParsingImageA", "Unable to parse image");
		return nullptr;
	}

	TSharedPtr<FComparableImage> Image = MakeShareable(new FComparableImage());
	
	const TArray<uint8>* RawData = nullptr;

	if ( !ImageReader->GetRaw(ERGBFormat::RGBA, 8, RawData) )
	{
		OutError = LOCTEXT("ErrorReadingRawDataA", "Unable decompress ImageA");
		return nullptr;
	}
	else
	{
		Image->Width = ImageReader->GetWidth();
		Image->Height = ImageReader->GetHeight();
		Image->Bytes = *RawData;
	}

	return Image;
}

FImageComparer::FImageComparer()
	: DeltaDirectory(FPlatformProcess::UserTempDir())
{
}

FImageComparisonResult FImageComparer::Compare(const FString& ImagePathA, const FString& ImagePathB, FImageTolerance Tolerance)
{
	FImageComparisonResult Results;
	Results.ApprovedFile = ImagePathA;
	FPaths::MakePathRelativeTo(Results.ApprovedFile, *ImageRootA);
	Results.IncomingFile = ImagePathB;
	FPaths::MakePathRelativeTo(Results.IncomingFile, *ImageRootB);

	TSharedPtr<FComparableImage> ImageA;
	TSharedPtr<FComparableImage> ImageB;

	FText ErrorA;
	FText ErrorB;

	ParallelFor(2,
		[&] (int32 Index)
	{
		if ( Index == 0 )
		{
			ImageA = Open(ImagePathA, ErrorA);
		}
		else
		{
			ImageB = Open(ImagePathB, ErrorB);
		}
	});

	if ( !ImageA.IsValid() )
	{
		Results.ErrorMessage = ErrorA;
		return Results;
	}

	if ( !ImageB.IsValid() )
	{
		Results.ErrorMessage = ErrorB;
		return Results;
	}

	if ( ImageA->Width != ImageB->Width || ImageA->Height != ImageB->Height )
	{
		Results.ErrorMessage = LOCTEXT("DifferentSizesUnsupported", "We can not compare images of different sizes at this time.");
		return Results;
	}

	ImageA->Process();
	ImageB->Process();

	const int32 CompareWidth = ImageA->Width;
	const int32 CompareHeight = ImageA->Height;

	FImageDelta ImageDelta(CompareWidth, CompareHeight);

	volatile int32 MismatchCount = 0;

	// We create 100 blocks of local mismatch area, then bucket the pixel based on a spacial hash.
	int32 BlockSizeX = FMath::RoundFromZero(CompareWidth / 10.0);
	int32 BlockSizeY = FMath::RoundFromZero(CompareHeight / 10.0);
	volatile int32 LocalMismatches[100];
	FPlatformMemory::Memzero((void*)&LocalMismatches, 100 * sizeof(int32));

	ParallelFor(CompareWidth,
		[&] (int32 ColumnIndex)
	{
		for ( int Y = 0; Y < CompareHeight; Y++ )
		{
			FColor PixelA = ImageA->GetPixel(ColumnIndex, Y);
			FColor PixelB = ImageB->GetPixel(ColumnIndex, Y);

			if ( Tolerance.IgnoreColors )
			{
				if ( FPixelOperations::IsBrightnessSimilar(PixelA, PixelB, Tolerance) )
				{
					ImageDelta.SetClearPixel(ColumnIndex, Y);
				}
				else
				{
					ImageDelta.SetErrorPixel(ColumnIndex, Y);
					FPlatformAtomics::InterlockedIncrement(&MismatchCount);
					int32 SpacialHash = ( (Y / BlockSizeY) * 10 + ( ColumnIndex / BlockSizeX ) );
					FPlatformAtomics::InterlockedIncrement(&LocalMismatches[SpacialHash]);
				}

				// Next Pixel
				continue;
			}

			if ( FPixelOperations::IsRGBSimilar(PixelA, PixelB, Tolerance) )
			{
				ImageDelta.SetClearPixel(ColumnIndex, Y);
			}
			else if ( Tolerance.IgnoreAntiAliasing && (
				FPixelOperations::IsAntialiased(PixelA, ImageA.Get(), ColumnIndex, Y, Tolerance) ||
				FPixelOperations::IsAntialiased(PixelB, ImageB.Get(), ColumnIndex, Y, Tolerance)
				) )
			{
				if ( FPixelOperations::IsBrightnessSimilar(PixelA, PixelB, Tolerance) )
				{
					ImageDelta.SetClearPixel(ColumnIndex, Y);
				}
				else
				{
					ImageDelta.SetErrorPixel(ColumnIndex, Y);
					FPlatformAtomics::InterlockedIncrement(&MismatchCount);
					int32 SpacialHash = ( ( Y / BlockSizeY ) * 10 + ( ColumnIndex / BlockSizeX ) );
					FPlatformAtomics::InterlockedIncrement(&LocalMismatches[SpacialHash]);
				}
			}
			else
			{
				ImageDelta.SetErrorPixel(ColumnIndex, Y);
				FPlatformAtomics::InterlockedIncrement(&MismatchCount);
				int32 SpacialHash = ( ( Y / BlockSizeY ) * 10 + ( ColumnIndex / BlockSizeX ) );
				FPlatformAtomics::InterlockedIncrement(&LocalMismatches[SpacialHash]);
			}
		}
	});

	ImageDelta.Save(DeltaDirectory);

	int32 MaximumLocalMismatches = 0;
	for ( int32 SpacialIndex = 0; SpacialIndex < 100; SpacialIndex++ )
	{
		MaximumLocalMismatches = FMath::Max(MaximumLocalMismatches, LocalMismatches[SpacialIndex]);
	}

	Results.Tolerance = Tolerance;
	Results.MaxLocalDifference = MaximumLocalMismatches / (double)( BlockSizeX * BlockSizeY );
	Results.GlobalDifference = MismatchCount / (double)( CompareHeight * CompareWidth );
	Results.ComparisonFile = ImageDelta.ComparisonFile;

	return Results;
}

#undef LOCTEXT_NAMESPACE
