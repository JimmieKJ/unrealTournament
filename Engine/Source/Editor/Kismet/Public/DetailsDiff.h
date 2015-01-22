// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "PropertyPath.h"
#include "DiffUtils.h"

class KISMET_API FDetailsDiff
{
public:
	DECLARE_DELEGATE( FOnDisplayedPropertiesChanged );

	FDetailsDiff( const UObject* InObject, const TArray< FPropertyPath >& InDifferingProperties, FOnDisplayedPropertiesChanged InOnDisplayedPropertiesChanged );
	~FDetailsDiff();

	void HighlightProperty( const FPropertySoftPath& PropertyName );
	TSharedRef< SWidget > DetailsWidget();
	TArray<FPropertySoftPath> GetDisplayedProperties() const;

private:
	void HandlePropertiesChanged();

	FOnDisplayedPropertiesChanged OnDisplayedPropertiesChanged;

	TArray< FPropertyPath > DifferingProperties;
	const UObject* DisplayedObject;

	TSharedPtr< class IDetailsView > DetailsView;
};
