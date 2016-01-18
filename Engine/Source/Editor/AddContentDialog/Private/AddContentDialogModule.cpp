// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AddContentDialogPCH.h"

#include "AssertionMacros.h"
#include "FeaturePackContentSourceProvider.h"
#include "ModuleManager.h"
#include "SDockTab.h"
#include "WidgetCarouselModule.h"
#include "WidgetCarouselStyle.h"

#define LOCTEXT_NAMESPACE "AddContentDialog"

class FAddContentDialogModule : public IAddContentDialogModule
{

public:
	virtual void StartupModule() override
	{
		FModuleManager::LoadModuleChecked<FWidgetCarouselModule>("WidgetCarousel");
		FWidgetCarouselModuleStyle::Initialize();

		ContentSourceProviderManager = TSharedPtr<FContentSourceProviderManager>(new FContentSourceProviderManager());
		ContentSourceProviderManager->RegisterContentSourceProvider(MakeShareable(new FFeaturePackContentSourceProvider()));
		FAddContentDialogStyle::Initialize();
	}

	virtual void ShutdownModule() override
	{
		FAddContentDialogStyle::Shutdown();
		FWidgetCarouselModuleStyle::Shutdown();
	}

	virtual TSharedRef<FContentSourceProviderManager> GetContentSourceProviderManager() override
	{
		return ContentSourceProviderManager.ToSharedRef();
	}

	virtual void ShowDialog(TSharedRef<SWindow> ParentWindow) override
	{
		if (AddContentDialog.IsValid() == false)
		{
			TSharedRef<SWindow> Dialog = SNew(SAddContentDialog);
			FSlateApplication::Get().AddWindowAsNativeChild(Dialog, ParentWindow);
			AddContentDialog = TWeakPtr<SWindow>(Dialog);
		}		
	}

private:
	TSharedPtr<FContentSourceProviderManager> ContentSourceProviderManager;
	TWeakPtr<SWindow> AddContentDialog;
};

IMPLEMENT_MODULE(FAddContentDialogModule, AddContentDialog);

#undef LOCTEXT_NAMESPACE