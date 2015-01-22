// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "EngineFontServices.h"
#include "SlateBasics.h"
#if !UE_SERVER
	#include "ISlateRHIRendererModule.h"
#endif

FEngineFontServices* FEngineFontServices::Instance = nullptr;

FEngineFontServices::FEngineFontServices()
{
	check(IsInGameThread());

#if !UE_SERVER
	if (!IsRunningDedicatedServer())
	{
		RenderThreadFontAtlasFactory = FModuleManager::Get().GetModuleChecked<ISlateRHIRendererModule>("SlateRHIRenderer").CreateSlateFontAtlasFactory();
	}
#endif
}

FEngineFontServices::~FEngineFontServices()
{
	check(IsInGameThread());

	if(RenderThreadFontCache.IsValid())
	{
		RenderThreadFontCache->ReleaseResources();
		FlushRenderingCommands();

		RenderThreadFontCache.Reset();
	}
}

void FEngineFontServices::Create()
{
	check(!Instance);
	Instance = new FEngineFontServices();
}

void FEngineFontServices::Destroy()
{
	check(Instance);
	delete Instance;
	Instance = nullptr;
}

bool FEngineFontServices::IsInitialized()
{
	return Instance != nullptr;
}

FEngineFontServices& FEngineFontServices::Get()
{
	check(Instance);
	return *Instance;
}

TSharedPtr<FSlateFontCache> FEngineFontServices::GetFontCache()
{
	check(IsInGameThread() || IsInRenderingThread());

#if !UE_SERVER
	if(IsInGameThread())
	{
		if(FSlateApplication::IsInitialized())
		{
			return FSlateApplication::Get().GetRenderer()->GetFontCache();
		}
	}
	else
	{
		ConditionalCreateRenderThreadFontCache();
		return RenderThreadFontCache;
	}
#endif

	return nullptr;
}

TSharedPtr<FSlateFontMeasure> FEngineFontServices::GetFontMeasure()
{
	check(IsInGameThread() || IsInRenderingThread());

#if !UE_SERVER
	if(IsInGameThread())
	{
		if(FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		}
	}
	else
	{
		ConditionalCreatRenderThreadFontMeasure();
		return RenderThreadFontMeasure;
	}
#endif

	return nullptr;
}

void FEngineFontServices::UpdateCache()
{
	check(IsInGameThread() || IsInRenderingThread());

	if(IsInGameThread())
	{
		if(FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().GetRenderer()->GetFontCache()->UpdateCache();
		}
	}
	else
	{
		if(RenderThreadFontCache.IsValid())
		{
			RenderThreadFontCache->UpdateCache();
		}
	}
}

void FEngineFontServices::ConditionalCreateRenderThreadFontCache()
{
	if(!RenderThreadFontCache.IsValid())
	{
		RenderThreadFontCache = MakeShareable(new FSlateFontCache(RenderThreadFontAtlasFactory.ToSharedRef()));
	}
}

void FEngineFontServices::ConditionalCreatRenderThreadFontMeasure()
{
	ConditionalCreateRenderThreadFontCache();

	if(!RenderThreadFontMeasure.IsValid())
	{
		RenderThreadFontMeasure = FSlateFontMeasure::Create(RenderThreadFontCache.ToSharedRef());
	}
}
