// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "EngineFontServices.h"
#include "SlateBasics.h"
#if !UE_SERVER
	#include "ISlateRHIRendererModule.h"
	#include "ISlateNullRendererModule.h"
#endif

FEngineFontServices* FEngineFontServices::Instance = nullptr;

FEngineFontServices::FEngineFontServices()
{
	check(IsInGameThread());

#if !UE_SERVER
	if (!IsRunningDedicatedServer() && !IsRunningCommandlet())
	{
		SlateFontServices = FSlateApplication::Get().GetRenderer()->GetFontServices();
	}
#endif
}

FEngineFontServices::~FEngineFontServices()
{
	check(IsInGameThread());

	SlateFontServices.Reset();
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
	if (SlateFontServices.IsValid())
	{
		return SlateFontServices->GetFontCache();
	}

	return nullptr;
}

TSharedPtr<FSlateFontMeasure> FEngineFontServices::GetFontMeasure()
{
	if (SlateFontServices.IsValid())
	{
		return SlateFontServices->GetFontMeasureService();
	}

	return nullptr;
}

void FEngineFontServices::UpdateCache()
{
	if (SlateFontServices.IsValid())
	{
		TSharedRef<FSlateFontCache> FontCache = SlateFontServices->GetFontCache();
		FontCache->UpdateCache();
	}
}
