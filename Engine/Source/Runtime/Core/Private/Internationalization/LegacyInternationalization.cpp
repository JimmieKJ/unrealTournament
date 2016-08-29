// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "CorePrivatePCH.h"
#include "LegacyInternationalization.h"

#if !UE_ENABLE_ICU

#include "InvariantCulture.h"

FLegacyInternationalization::FLegacyInternationalization(FInternationalization* const InI18N)
	: I18N(InI18N)
{

}

bool FLegacyInternationalization::Initialize()
{
	I18N->InvariantCulture = FInvariantCulture::Create();
	I18N->DefaultCulture = I18N->InvariantCulture;
	I18N->CurrentCulture = I18N->InvariantCulture;

	return true;
}

void FLegacyInternationalization::Terminate()
{
}

void FLegacyInternationalization::LoadAllCultureData()
{
}

bool FLegacyInternationalization::IsCultureRemapped(const FString& Name, FString* OutMappedCulture)
{
	return false;
}

bool FLegacyInternationalization::IsCultureDisabled(const FString& Name)
{
	return false;
}

bool FLegacyInternationalization::SetCurrentCulture(const FString& Name)
{
	return Name.IsEmpty();
}

void FLegacyInternationalization::GetCultureNames(TArray<FString>& CultureNames) const
{
	CultureNames.Add(TEXT(""));
}

TArray<FString> FLegacyInternationalization::GetPrioritizedCultureNames(const FString& Name)
{
	TArray<FString> PrioritizedCultureNames;
	PrioritizedCultureNames.Add(Name);
	return PrioritizedCultureNames;
}

FCulturePtr FLegacyInternationalization::GetCulture(const FString& Name)
{
	return Name.IsEmpty() ? I18N->InvariantCulture : nullptr;
}

#endif