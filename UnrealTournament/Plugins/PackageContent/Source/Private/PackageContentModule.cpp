// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PackageContentPrivatePCH.h"
#include "IPackageContent.h"
#include "PackageContent.h"
#include "PackageContentStyle.h"

class FPackageContentModule : public IPackageContentModule
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:

	TSharedPtr< FPackageContent > PackageContent;
};

IMPLEMENT_MODULE(FPackageContentModule, PackageContent)

void FPackageContentModule::StartupModule()
{
	if ( !IsRunningCommandlet() )
	{
		// Register slate style overrides
		FPackageContentStyle::Initialize();

		PackageContent = FPackageContent::Create();
	}
}


void FPackageContentModule::ShutdownModule()
{
	PackageContent.Reset();

	FPackageContentStyle::Shutdown();
}