// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyEditing.h"
#include "PropertyCustomizationHelpers.h"

class FBehaviorDecoratorDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailLayout ) override;

	bool IsEditingEnabled() const;

private:

	void UpdateAllowedAbortModes();
	void InitPropertyValues();

	EVisibility GetModeVisibility() const;
	bool GetAbortModeEnabled() const;

	void OnAbortModeChange(int32 Index);
	TSharedRef<SWidget> OnGetAbortModeContent() const;
	FText GetCurrentAbortModeDesc() const;

	TSharedPtr<IPropertyHandle> ModeProperty;

	/** cached names of abort behaviors */
	struct FStringIntPair
	{
		FString Str;
		int32 Int;
	};

	TArray<FStringIntPair> ModeValues;

	uint32 bShowMode : 1;
	uint32 bIsModeEnabled : 1;

	class UObject* MyNode;
	/** property utils */
	class IPropertyUtilities* PropUtils;
};
