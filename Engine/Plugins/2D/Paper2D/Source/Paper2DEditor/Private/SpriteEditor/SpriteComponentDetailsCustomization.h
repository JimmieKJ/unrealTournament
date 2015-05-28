// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyEditing.h"

//////////////////////////////////////////////////////////////////////////
// FSpriteComponentDetailsCustomization

class FSpriteComponentDetailsCustomization : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	// IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
	// End of IDetailCustomization interface

protected:
	FReply MergeSprites();

protected:
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
};
