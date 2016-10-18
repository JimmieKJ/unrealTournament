// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "GameplayTagsEditorModulePrivatePCH.h"
#include "GameplayTagCustomization.h"
#include "Editor/PropertyEditor/Public/PropertyEditing.h"
#include "MainFrame.h"
#include "SGameplayTagWidget.h"
#include "GameplayTagContainer.h"
#include "ScopedTransaction.h"
#include "SScaleBox.h"

#define LOCTEXT_NAMESPACE "GameplayTagCustomization"

void FGameplayTagCustomization::CustomizeHeader(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TagContainer = MakeShareable(new FGameplayTagContainer);
	StructPropertyHandle = InStructPropertyHandle;

	FSimpleDelegate OnTagChanged = FSimpleDelegate::CreateSP(this, &FGameplayTagCustomization::OnPropertyValueChanged);
	StructPropertyHandle->SetOnPropertyValueChanged(OnTagChanged);

	BuildEditableContainerList();

	HeaderRow
	.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MaxDesiredWidth(512)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SAssignNew(ComboButton, SComboButton)
			.OnGetMenuContent(this, &FGameplayTagCustomization::GetListContent)
			.ContentPadding(FMargin(2.0f, 2.0f))
			.MenuPlacement(MenuPlacement_BelowAnchor)
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("GameplayTagCustomization_Edit", "Edit"))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBorder)
			.Padding(4.0f)
			[
				SNew(STextBlock)
				.Text(this, &FGameplayTagCustomization::SelectedTag)
			]
		]
	];

	GEditor->RegisterForUndo(this);
}

TSharedRef<SWidget> FGameplayTagCustomization::GetListContent()
{
	BuildEditableContainerList();
	
	FString Categories;
	if (StructPropertyHandle->GetProperty()->HasMetaData(TEXT("Categories")))
	{
		Categories = StructPropertyHandle->GetProperty()->GetMetaData(TEXT("Categories"));
	}

	bool bReadOnly = StructPropertyHandle->GetProperty()->HasAnyPropertyFlags(CPF_EditConst);

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight(400)
		[
            SNew(SGameplayTagWidget, EditableContainers)
            .Filter(Categories)
            .ReadOnly(bReadOnly)
            .TagContainerName(StructPropertyHandle->GetPropertyDisplayName().ToString())
            .MultiSelect(false)
            .OnTagChanged(this, &FGameplayTagCustomization::OnTagChanged)
            .PropertyHandle(StructPropertyHandle)
		];
}

void FGameplayTagCustomization::OnPropertyValueChanged()
{
	TagName = TEXT("");
	if (StructPropertyHandle.IsValid() && StructPropertyHandle->GetProperty() && EditableContainers.Num() > 0)
	{
		TArray<void*> RawStructData;
		StructPropertyHandle->AccessRawData(RawStructData);
		if (RawStructData.Num() > 0)
		{
			FGameplayTag* Tag = (FGameplayTag*)(RawStructData[0]);
			FGameplayTagContainer* Container = EditableContainers[0].TagContainer;			
			if (Tag && Container)
			{
				Container->RemoveAllTags();
				Container->AddTag(*Tag);
				TagName = Tag->ToString();
			}			
		}
	}
}

void FGameplayTagCustomization::OnTagChanged()
{
	TagName = TEXT("");
	if (StructPropertyHandle.IsValid() && StructPropertyHandle->GetProperty() && EditableContainers.Num() > 0)
	{
		TArray<void*> RawStructData;
		StructPropertyHandle->AccessRawData(RawStructData);
		if (RawStructData.Num() > 0)
		{
			FGameplayTag* Tag = (FGameplayTag*)(RawStructData[0]);			

			// Update Tag from the one selected from list
			FGameplayTagContainer* Container = EditableContainers[0].TagContainer;
			if (Tag && Container)
			{
				for (auto It = Container->CreateConstIterator(); It; ++It)
				{
					*Tag = *It;
					TagName = It->ToString();
				}
			}
		}
	}
}

void FGameplayTagCustomization::PostUndo(bool bSuccess)
{
	if (bSuccess && !StructPropertyHandle.IsValid())
	{
		OnTagChanged();
	}
}

void FGameplayTagCustomization::PostRedo(bool bSuccess)
{
	if (bSuccess && !StructPropertyHandle.IsValid())
	{
		OnTagChanged();
	}
}

FGameplayTagCustomization::~FGameplayTagCustomization()
{
	GEditor->UnregisterForUndo(this);
}

void FGameplayTagCustomization::BuildEditableContainerList()
{
	EditableContainers.Empty();

	if(StructPropertyHandle.IsValid() && StructPropertyHandle->GetProperty())
	{
		TArray<void*> RawStructData;
		StructPropertyHandle->AccessRawData(RawStructData);

		if (RawStructData.Num() > 0)
		{
			FGameplayTag* Tag = (FGameplayTag*)(RawStructData[0]);
			if (Tag->IsValid())
			{
				TagName = Tag->ToString();
				TagContainer->AddTag(*Tag);
			}
		}

		EditableContainers.Add(SGameplayTagWidget::FEditableGameplayTagContainerDatum(nullptr, TagContainer.Get()));
	}
}

FText FGameplayTagCustomization::SelectedTag() const
{
	return FText::FromString(*TagName);
}

#undef LOCTEXT_NAMESPACE
