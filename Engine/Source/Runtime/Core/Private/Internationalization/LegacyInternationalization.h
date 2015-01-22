// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#if !UE_ENABLE_ICU

class FLegacyInternationalization
{
public:
	FLegacyInternationalization(FInternationalization* const I18N);

	bool Initialize();
	void Terminate();

	bool SetCurrentCulture(const FString& Name);
	void GetCultureNames(TArray<FString>& CultureNames) const;
	FCulturePtr GetCulture(const FString& Name);

private:
	FInternationalization* const I18N;
};

#endif