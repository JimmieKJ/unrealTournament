// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ImageWrapperPrivatePCH.h"


DEFINE_LOG_CATEGORY(LogImageWrapper);


/**
 * Image Wrapper module
 */
class FImageWrapperModule
	: public IImageWrapperModule
{
public:

	// IImageWrapperModule interface

	virtual IImageWrapperPtr CreateImageWrapper( const EImageFormat::Type InFormat ) override
	{
		FImageWrapperBase* ImageWrapper = NULL;

		// Allocate a helper for the format type
		switch(InFormat)
		{
#if WITH_UNREALPNG
		case EImageFormat::PNG:
			ImageWrapper = new FPngImageWrapper();
			break;
#endif	// WITH_UNREALPNG
#if WITH_UNREALJPEG
		case EImageFormat::JPEG:
			ImageWrapper = new FJpegImageWrapper();
			break;
		case EImageFormat::GrayscaleJPEG:
			ImageWrapper = new FJpegImageWrapper(1);
			break;
#endif	//WITH_UNREALJPEG

		case EImageFormat::BMP:
			ImageWrapper = new FBmpImageWrapper();
			break;

		case EImageFormat::ICO:
			ImageWrapper = new FIcoImageWrapper();
			break;
#if WITH_UNREALEXR
		case EImageFormat::EXR:
			ImageWrapper = new FExrImageWrapper();
			break;
#endif
		case EImageFormat::ICNS:
			ImageWrapper = new FIcnsImageWrapper();
			break;
		default:
			break;
		}

		return MakeShareable(ImageWrapper);
	}

public:

	// IModuleInterface interface

	virtual void StartupModule( ) override { }
	virtual void ShutdownModule( ) override { }
};


IMPLEMENT_MODULE(FImageWrapperModule, ImageWrapper);
