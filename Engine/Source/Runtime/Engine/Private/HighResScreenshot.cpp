// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "EnginePrivate.h"
#include "HighResScreenshot.h"
#include "Slate/SceneViewport.h"
#include "ImageWrapper.h"

static TAutoConsoleVariable<int32> CVarSaveEXRCompressionQuality(
	TEXT("r.SaveEXR.CompressionQuality"),
	1,
	TEXT("Defines how we save HDR screenshots in the EXR format.\n")
	TEXT(" 0: no compression\n")
	TEXT(" 1: default compression which can be slow (default)"),
	ECVF_RenderThreadSafe);

FHighResScreenshotConfig& GetHighResScreenshotConfig()
{
	static FHighResScreenshotConfig Instance;
	return Instance;
}

const float FHighResScreenshotConfig::MinResolutionMultipler = 1.0f;
const float FHighResScreenshotConfig::MaxResolutionMultipler = 10.0f;

FHighResScreenshotConfig::FHighResScreenshotConfig()
	: ResolutionMultiplier(FHighResScreenshotConfig::MinResolutionMultipler)
	, ResolutionMultiplierScale(0.0f)
	, bMaskEnabled(false)
	, bDumpBufferVisualizationTargets(false)
{
	ChangeViewport(TWeakPtr<FSceneViewport>());
	SetHDRCapture(false);
}

void FHighResScreenshotConfig::Init(uint32 NumAsyncWriters)
{
	IImageWrapperModule* ImageWrapperModule = FModuleManager::LoadModulePtr<IImageWrapperModule>(FName("ImageWrapper"));
	if (ImageWrapperModule != nullptr)
	{
		ImageCompressorsLDR.Reserve(NumAsyncWriters);
		ImageCompressorsHDR.Reserve(NumAsyncWriters);

		for (uint32 Index = 0; Index != NumAsyncWriters; ++Index)
		{
			ImageCompressorsLDR.Emplace(ImageWrapperModule->CreateImageWrapper(EImageFormat::PNG));
			ImageCompressorsHDR.Emplace(ImageWrapperModule->CreateImageWrapper(EImageFormat::EXR));
		}
	}

#if WITH_EDITOR
	HighResScreenshotMaterial = LoadObject<UMaterial>(NULL, TEXT("/Engine/EngineMaterials/HighResScreenshot.HighResScreenshot"));
	HighResScreenshotMaskMaterial = LoadObject<UMaterial>(NULL, TEXT("/Engine/EngineMaterials/HighResScreenshotMask.HighResScreenshotMask"));
	HighResScreenshotCaptureRegionMaterial = LoadObject<UMaterial>(NULL, TEXT("/Engine/EngineMaterials/HighResScreenshotCaptureRegion.HighResScreenshotCaptureRegion"));

	if (HighResScreenshotMaterial)
	{
		HighResScreenshotMaterial->AddToRoot();
	}
	if (HighResScreenshotMaskMaterial)
	{
		HighResScreenshotMaskMaterial->AddToRoot();
	}
	if (HighResScreenshotCaptureRegionMaterial)
	{
		HighResScreenshotCaptureRegionMaterial->AddToRoot();
	}
#endif
}

void FHighResScreenshotConfig::ChangeViewport(TWeakPtr<FSceneViewport> InViewport)
{
	if (FSceneViewport* Viewport = TargetViewport.Pin().Get())
	{
		// Force an invalidate on the old viewport to make sure we clear away the capture region effect
		Viewport->Invalidate();
	}

	UnscaledCaptureRegion = FIntRect(0, 0, 0, 0);
	CaptureRegion = UnscaledCaptureRegion;
	bMaskEnabled = false;
	bDumpBufferVisualizationTargets = false;
	ResolutionMultiplier = 1.0f;
	TargetViewport = InViewport;
}

bool FHighResScreenshotConfig::ParseConsoleCommand(const FString& InCmd, FOutputDevice& Ar)
{
	GScreenshotResolutionX = 0;
	GScreenshotResolutionY = 0;
	ResolutionMultiplier = 1.0f;

	if( GetHighResScreenShotInput(*InCmd, Ar, GScreenshotResolutionX, GScreenshotResolutionY, ResolutionMultiplier, CaptureRegion, bMaskEnabled) )
	{
		GScreenshotResolutionX *= ResolutionMultiplier;
		GScreenshotResolutionY *= ResolutionMultiplier;
		GIsHighResScreenshot = true;

		return true;
	}

	return false;
}

bool FHighResScreenshotConfig::MergeMaskIntoAlpha(TArray<FColor>& InBitmap)
{
	bool bWritten = false;

	if (bMaskEnabled)
	{
		// If this is a high resolution screenshot and we are using the masking feature,
		// Get the results of the mask rendering pass and insert into the alpha channel of the screenshot.
		TArray<FColor>* MaskArray = FScreenshotRequest::GetHighresScreenshotMaskColorArray();
		check(MaskArray->Num() == InBitmap.Num());
		for (int32 i = 0; i < MaskArray->Num(); ++i)
		{
			InBitmap[i].A = (*MaskArray)[i].R;
		}

		bWritten = true;
	}
	else
	{
		// Ensure that all pixels' alpha is set to 255
		for (auto& Color : InBitmap)
		{
			Color.A = 255;
		}
	}

	return bWritten;
}

void FHighResScreenshotConfig::SetHDRCapture(bool bCaptureHDRIN)
{
	bCaptureHDR = bCaptureHDRIN;
}

template<typename> struct FPixelTypeTraits {};

template<> struct FPixelTypeTraits<FColor>
{
	static const ERGBFormat::Type SourceChannelLayout = ERGBFormat::BGRA;

	static FORCEINLINE bool IsWritingHDRImage(const bool)
	{
		return false;
	}
};

template<> struct FPixelTypeTraits<FFloat16Color>
{
	static const ERGBFormat::Type SourceChannelLayout = ERGBFormat::RGBA;

	static FORCEINLINE bool IsWritingHDRImage(const bool bCaptureHDR)
	{
		static const auto CVarDumpFramesAsHDR = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.BufferVisualizationDumpFramesAsHDR"));
		return bCaptureHDR || CVarDumpFramesAsHDR->GetValueOnAnyThread();
	}
};

template<typename TPixelType>
bool FHighResScreenshotConfig::SaveImage(const FString& File, const TArray<TPixelType>& Bitmap, const FIntPoint& BitmapSize, FString* OutFilename) const
{
	typedef FPixelTypeTraits<TPixelType> Traits;

	static_assert(ARE_TYPES_EQUAL(TPixelType, FFloat16Color) || ARE_TYPES_EQUAL(TPixelType, FColor), "Source format must be either FColor or FFloat16Color");
	const int32 x = BitmapSize.X;
	const int32 y = BitmapSize.Y;
	check(Bitmap.Num() == x * y);

	const bool bIsWritingHDRImage = Traits::IsWritingHDRImage(bCaptureHDR);

	IFileManager* FileManager = &IFileManager::Get();
	const size_t BitsPerPixel = (sizeof(TPixelType) / 4) * 8;

	const FImageWriter* ImageWriter = nullptr;
	{
		const TArray<FImageWriter>& ArrayToUse = bIsWritingHDRImage ? ImageCompressorsHDR : ImageCompressorsLDR;

		// Find a free image writer to use. This can potentially be called on many threads at the same time
		for (;;)
		{
			for (const FImageWriter& Writer : ArrayToUse)
			{
				if (!Writer.bInUse.AtomicSet(true))
				{
					ImageWriter = &Writer;
					break;
				}
			}

			if (ImageWriter)
			{
				break;
			}

			FPlatformProcess::Sleep(0.001f);
		}
	}

	// here we require the input file name to have an extension
	FString NewExtension = bIsWritingHDRImage ? TEXT(".exr") : TEXT(".png");
	FString Filename = FPaths::GetBaseFilename(File, false) + NewExtension;

	if (OutFilename != nullptr)
	{
		*OutFilename = Filename;
	}
	
	bool bSuccess = false;

	if (ImageWriter && ImageWriter->ImageWrapper->SetRaw((void*)&Bitmap[0], sizeof(TPixelType)* x * y, x, y, Traits::SourceChannelLayout, BitsPerPixel))
	{
		ImageCompression::CompressionQuality LocalCompressionQuality = ImageCompression::Default;
		
		if(bIsWritingHDRImage && CVarSaveEXRCompressionQuality.GetValueOnAnyThread() == 0)
		{
			LocalCompressionQuality = ImageCompression::Uncompressed;
		}

		// Compress and write image
		FArchive* Ar = FileManager->CreateFileWriter(Filename.GetCharArray().GetData());
		if (Ar != nullptr)
		{
		const TArray<uint8>& CompressedData = ImageWriter->ImageWrapper->GetCompressed(LocalCompressionQuality);
			int32 CompressedSize = CompressedData.Num();
			Ar->Serialize((void*)CompressedData.GetData(), CompressedSize);
			delete Ar;

			bSuccess = true;
		}
	}

	ImageWriter->bInUse = false;

	return bSuccess;
}

template ENGINE_API bool FHighResScreenshotConfig::SaveImage<FColor>(const FString& File, const TArray<FColor>& Bitmap, const FIntPoint& BitmapSize, FString* OutFilename) const;
template ENGINE_API bool FHighResScreenshotConfig::SaveImage<FFloat16Color>(const FString& File, const TArray<FFloat16Color>& Bitmap, const FIntPoint& BitmapSize, FString* OutFilename) const;

FHighResScreenshotConfig::FImageWriter::FImageWriter(const TSharedPtr<class IImageWrapper>& InWrapper)
	: ImageWrapper(InWrapper)
{
}
