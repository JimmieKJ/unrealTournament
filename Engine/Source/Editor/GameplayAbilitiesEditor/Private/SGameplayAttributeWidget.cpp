// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemEditorPrivatePCH.h"
#include "KismetEditorUtilities.h"
#include "SGameplayAttributeWidget.h"
#include "STextComboBox.h"

#define LOCTEXT_NAMESPACE "K2Node"

void SGameplayAttributeWidget::Construct(const FArguments& InArgs)
{
	FilterMetaData = InArgs._FilterMetaData;
	OnAttributeChanged = InArgs._OnAttributeChanged;
	SelectedProperty = InArgs._DefaultProperty;

	TSharedPtr<FString> InitiallySelected = MakeShareable(new FString("None"));

	PropertyOptions.Empty();
	PropertyOptions.Add(InitiallySelected);

	// Gather all UAttraibute classes
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass *Class = *ClassIt;
		if (Class->IsChildOf(UAttributeSet::StaticClass()) && !FKismetEditorUtilities::IsClassABlueprintSkeleton(Class))
		{
			for (TFieldIterator<UProperty> PropertyIt(Class, EFieldIteratorFlags::ExcludeSuper); PropertyIt; ++PropertyIt)
			{
				UProperty *Property = *PropertyIt;

				if (!FilterMetaData.IsEmpty() && Property->HasMetaData(*FilterMetaData))
				{
					continue;
				}

				TSharedPtr<FString> SelectableProperty = MakeShareable(new FString(FString::Printf(TEXT("%s.%s"), *Class->GetName(), *Property->GetName())));
				if (Property == SelectedProperty)
				{
					InitiallySelected = SelectableProperty;
				}
				PropertyOptions.Add(SelectableProperty);
			}
		}
	}

	ChildSlot
	[
		SNew(STextComboBox)
		.ContentPadding(FMargin(2.0f, 2.0f))
		.OptionsSource(&PropertyOptions)
		.InitiallySelectedItem(InitiallySelected)
		.OnSelectionChanged(this, &SGameplayAttributeWidget::OnChangeProperty)
	];
}

void SGameplayAttributeWidget::OnChangeProperty(TSharedPtr<FString> ItemSelected, ESelectInfo::Type SelectInfo)
{
	SelectedProperty = nullptr;
	if (ItemSelected.IsValid())
	{
		FString FullString = *ItemSelected.Get();
		FString ClassName;
		FString PropertyName;

		FullString.Split(TEXT("."), &ClassName, &PropertyName);

		UClass *FoundClass = FindObject<UClass>(ANY_PACKAGE, *ClassName);
		if (FoundClass)
		{
			SelectedProperty = FindFieldChecked<UProperty>(FoundClass, *PropertyName);
		}
	}	

	OnAttributeChanged.ExecuteIfBound(SelectedProperty);
}

#undef LOCTEXT_NAMESPACE
