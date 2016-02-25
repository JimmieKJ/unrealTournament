// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ImageDownloadPCH.h"

DEFINE_LOG_CATEGORY(LogImageDownload);

/**
 * Image Download module
 */
class FImageDownloadModule
	: public IModuleInterface
{
public:

	// IModuleInterface interface

	virtual void StartupModule( ) override { }
	virtual void ShutdownModule( ) override { }
};

IMPLEMENT_MODULE(FImageDownloadModule, ImageDownload);

