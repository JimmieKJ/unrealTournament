// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyEditorDelegates.h"

/**
 * Interface class for all detail views
 */
class IStructureDetailsView
{
public:
	virtual TSharedPtr<class SWidget> GetWidget() = 0;

	virtual void SetStructureData(TSharedPtr<class FStructOnScope> StructData) = 0;
	virtual FOnFinishedChangingProperties& GetOnFinishedChangingPropertiesDelegate() = 0;
};
