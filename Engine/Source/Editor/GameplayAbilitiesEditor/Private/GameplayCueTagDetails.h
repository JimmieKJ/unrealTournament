// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"

class IPropertyHandle;
class FDetailWidgetRow;
class IDetailLayoutBuilder;

struct FGameplayTag;

DECLARE_LOG_CATEGORY_EXTERN(LogGameplayCueDetails, Log, All);

class FGameplayCueTagDetails : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();


private:

	FGameplayCueTagDetails()
	{
	}

	virtual void CustomizeHeader( TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;
	virtual void CustomizeChildren( TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;	

	TSharedPtr<IPropertyHandle> GameplayTagProperty;

	TSharedRef<ITableRow> GenerateListRow(TSharedRef<FStringAssetReference> NotifyName, const TSharedRef<STableViewBase>& OwnerTable);
	TArray<TSharedRef<FStringAssetReference> > NotifyList;
	TSharedPtr< SListView < TSharedRef<FStringAssetReference> > > ListView;

	void NavigateToHandler(TSharedRef<FStringAssetReference> AssetRef);

	FReply OnAddNewNotifyClicked();

	/** Updates the selected tag*/
	void OnPropertyValueChanged();
	bool UpdateNotifyList();
	FGameplayTag* GetTag() const;
	FText GetTagText() const;

	EVisibility GetListViewVisibility() const;
	EVisibility GetAddNewNotifyVisibliy() const;
};