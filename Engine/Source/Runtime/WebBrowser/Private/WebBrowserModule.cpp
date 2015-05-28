// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "WebBrowserPrivatePCH.h"
#include "WebBrowserSingleton.h"
#include "ModuleManager.h"
#include "CEF3Utils.h"

DEFINE_LOG_CATEGORY(LogWebBrowser);

static FWebBrowserSingleton* WebBrowserSingleton = nullptr;

class FWebBrowserModule : public IWebBrowserModule
{
private:
	// IModuleInterface Interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

public:
	virtual IWebBrowserSingleton* GetSingleton() override;
};

IMPLEMENT_MODULE( FWebBrowserModule, WebBrowser );

void FWebBrowserModule::StartupModule()
{
#if WITH_CEF3
	CEF3Utils::LoadCEF3Modules();
#endif

	WebBrowserSingleton = new FWebBrowserSingleton();
}

void FWebBrowserModule::ShutdownModule()
{
	delete WebBrowserSingleton;
	WebBrowserSingleton = NULL;

#if WITH_CEF3
	CEF3Utils::UnloadCEF3Modules();
#endif
}

IWebBrowserSingleton* FWebBrowserModule::GetSingleton()
{
	return WebBrowserSingleton;
}
