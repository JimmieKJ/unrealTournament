// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "ContentBrowserPCH.h"
#include "ModuleManager.h"
#include "ContentBrowserModule.h"

IMPLEMENT_MODULE( FContentBrowserModule, ContentBrowser );
DEFINE_LOG_CATEGORY(LogContentBrowser);

void FContentBrowserModule::StartupModule()
{
	ContentBrowserSingleton = new FContentBrowserSingleton();
}

void FContentBrowserModule::ShutdownModule()
{	
	if ( ContentBrowserSingleton )
	{
		delete ContentBrowserSingleton;
		ContentBrowserSingleton = NULL;
	}

}

IContentBrowserSingleton& FContentBrowserModule::Get() const
{
	check(ContentBrowserSingleton);
	return *ContentBrowserSingleton;

}