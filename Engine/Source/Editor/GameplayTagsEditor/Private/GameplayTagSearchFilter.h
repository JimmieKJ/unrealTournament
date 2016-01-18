// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ContentBrowserFrontEndFilterExtension.h"
#include "GameplayTagSearchFilter.generated.h"

UCLASS()
class UGameplayTagSearchFilter : public UContentBrowserFrontEndFilterExtension
{
public:
	GENERATED_BODY()

	// UContentBrowserFrontEndFilterExtension interface
	virtual void AddFrontEndFilterExtensions(TSharedPtr<class FFrontendFilterCategory> DefaultCategory, TArray< TSharedRef<class FFrontendFilter> >& InOutFilterList) const override;
	// End of UContentBrowserFrontEndFilterExtension interface
};
