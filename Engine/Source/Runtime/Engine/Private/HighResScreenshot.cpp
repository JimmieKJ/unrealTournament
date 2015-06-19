// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "EnginePrivate.h"
#include "HighResScreenshot.h"
#include "Slate/SceneViewport.h"
#include "ImageWrapper.h"

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

void FHighResScreenshotConfig::Init()
{
	IImageWrapperModule* ImageWrapperModule = FModuleManager::LoadModulePtr<IImageWrapperModule>(FName("ImageWrapper"));
	if (ImageWrapperModule != nullptr)
	{
		ImageCompressorLDR = ImageWrapperModule->CreateImageWrapper(EImageFormat::PNG);
		ImageCompressorHDR = ImageWrapperModule->CreateImageWrapper(EImageFormat::EXR);
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
		GScreenMessagesRestoreState = GAreScreenMessagesEnabled;
		GAreScreenMessagesEnabled = false;

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
		return bCaptureHDR || CVarDumpFramesAsHDR->GetValueOnRenderThread();
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

	TSharedPtr<class IImageWrapper> ImageCompressor = bIsWritingHDRImage ? ImageCompressorHDR : ImageCompressorLDR;

	// here we require the input file name to have an extension
	FString NewExtension = bIsWritingHDRImage ? TEXT(".exr") : TEXT(".png");
	FString Filename = FPaths::GetBaseFilename(File, false) + NewExtension;

	if (OutFilename != nullptr)
	{
		*OutFilename = Filename;
	}

	if (ImageCompressor.IsValid() && ImageCompressor->SetRaw((void*)&Bitmap[0], sizeof(TPixelType)* x * y, x, y, Traits::SourceChannelLayout, BitsPerPixel))
	{
		FArchive* Ar = FileManager->CreateFileWriter(Filename.GetCharArray().GetData());
		if (Ar != nullptr)
		{
			const TArray<uint8>& CompressedData = ImageCompressor->GetCompressed();
			int32 CompressedSize = CompressedData.Num();
			Ar->Serialize((void*)CompressedData.GetData(), CompressedSize);
			delete Ar;
		}
		else
		{
			return false;
		}
	}

	return true;
}

template ENGINE_API bool FHighResScreenshotConfig::SaveImage<FColor>(const FString& File, const TArray<FColor>& Bitmap, const FIntPoint& BitmapSize, FString* OutFilename) const;
template ENGINE_API bool FHighResScreenshotConfig::SaveImage<FFloat16Color>(const FString& File, const TArray<FFloat16Color>& Bitmap, const FIntPoint& BitmapSize, FString* OutFilename) const;
