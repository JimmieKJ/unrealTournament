// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ContentBrowserFrontEndFilterExtension.generated.h"

// Override this class in order to make an additional front-end filter available in the Content Browser
UCLASS(Abstract)
class CONTENTBROWSER_API UContentBrowserFrontEndFilterExtension : public UObject
{
	GENERATED_BODY()

public:
	// Override this method to add new frontend filters
	virtual void AddFrontEndFilterExtensions(TSharedPtr<class FFrontendFilterCategory> DefaultCategory, TArray< TSharedRef<class FFrontendFilter> >& InOutFilterList) const { }
};
