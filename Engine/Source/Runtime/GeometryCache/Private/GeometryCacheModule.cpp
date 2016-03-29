// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "GeometryCacheModulePrivatePCH.h"
#include "GeometryCacheModule.h"
#include "GeometryCacheComponent.h"
#include "ModuleManager.h"
#include "GeometryCacheEdModule.h"

IMPLEMENT_MODULE(FGeometryCacheModule, GeometryCache)

void FGeometryCacheModule::StartupModule()
{
#if WITH_EDITOR
	FGeometryCacheEdModule& Module = FModuleManager::LoadModuleChecked<FGeometryCacheEdModule>(TEXT("GeometryCacheEd"));
#endif
}

void FGeometryCacheModule::ShutdownModule()
{
}