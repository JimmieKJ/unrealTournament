// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
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

bool FLegacyInternationalization::SetCurrentCulture(const FString& Name)
{
	return Name.IsEmpty();
}

void FLegacyInternationalization::GetCultureNames(TArray<FString>& CultureNames) const
{
	CultureNames.Add(TEXT(""));
}

FCulturePtr FLegacyInternationalization::GetCulture(const FString& Name)
{
	return Name.IsEmpty() ? I18N->InvariantCulture : nullptr;
}

#endif