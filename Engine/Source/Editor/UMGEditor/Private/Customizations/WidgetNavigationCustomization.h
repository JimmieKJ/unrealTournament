// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WidgetBlueprintEditor.h"

class FWidgetNavigationCustomization : public IPropertyTypeCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<class IPropertyTypeCustomization> MakeInstance(TSharedRef<FWidgetBlueprintEditor> InEditor)
	{
		return MakeShareable(new FWidgetNavigationCustomization(InEditor));
	}

	FWidgetNavigationCustomization(TSharedRef<FWidgetBlueprintEditor> InEditor)
		: Editor(InEditor)
	{
	}
	
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	
private:

	void FillOutChildren(TSharedRef<IPropertyHandle> PropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils);

	FReply OnCustomizeNavigation(TWeakPtr<IPropertyHandle> PropertyHandle);
	
	FText HandleNavigationText(TWeakPtr<IPropertyHandle> PropertyHandle, EUINavigation Nav) const;

	TSharedRef<class SWidget> MakeNavRow(TWeakPtr<IPropertyHandle> PropertyHandle, EUINavigation Nav, FText NavName);

	TSharedRef<class SWidget> MakeNavMenu(TWeakPtr<IPropertyHandle> PropertyHandle, EUINavigation Nav);

	void HandleNavMenuEntryClicked(TWeakPtr<IPropertyHandle> PropertyHandle, EUINavigation Nav, EUINavigationRule Rule);

	void SetNav(UWidget* Widget, EUINavigation Nav, EUINavigationRule Rule);

private:
	TWeakPtr<FWidgetBlueprintEditor> Editor;
};
