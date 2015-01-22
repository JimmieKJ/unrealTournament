// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"

class IPropertyHandle;
class FDetailWidgetRow;
class IDetailLayoutBuilder;

DECLARE_LOG_CATEGORY_EXTERN(LogGameplayEffectDetails, Log, All);

class UGameplayEffectTemplate;

class FGameplayEffectDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

private:
	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;

	

	TArray<TSharedPtr<FString>> PropertyOptions;

	TSharedPtr<IPropertyHandle> MyProperty;

	IDetailLayoutBuilder* MyDetailLayout;

	TSharedPtr<IPropertyHandle> TemplateProperty;
	TSharedPtr<IPropertyHandle> ShowAllProperty;

	void OnShowAllChange();
	void OnTemplateChange();
	void OnDurationPolicyChange();

	bool HideProperties(IDetailLayoutBuilder& DetailLayout, TSharedPtr<IPropertyHandle> PropHandle, UGameplayEffectTemplate* Template);
};

