// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "GameplayTagContainerCustomization.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SButton.h"
#include "Framework/Docking/TabManager.h"

#include "Editor.h"
#include "PropertyHandle.h"
#include "DetailWidgetRow.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "GameplayTagContainerCustomization"

void FGameplayTagContainerCustomization::CustomizeHeader(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructPropertyHandle = InStructPropertyHandle;

	FSimpleDelegate OnTagContainerChanged = FSimpleDelegate::CreateSP(this, &FGameplayTagContainerCustomization::RefreshTagList);
	StructPropertyHandle->SetOnPropertyValueChanged(OnTagContainerChanged);

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
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SButton)
					.IsEnabled(!StructPropertyHandle->GetProperty()->HasAnyPropertyFlags(CPF_EditConst))
					.Text(LOCTEXT("GameplayTagContainerCustomization_Edit", "Edit..."))
					.OnClicked(this, &FGameplayTagContainerCustomization::OnEditButtonClicked)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SButton)
					.IsEnabled(!StructPropertyHandle->GetProperty()->HasAnyPropertyFlags(CPF_EditConst))
					.Text(LOCTEXT("GameplayTagContainerCustomization_Clear", "Clear All"))
					.OnClicked(this, &FGameplayTagContainerCustomization::OnClearAllButtonClicked)
					.Visibility(this, &FGameplayTagContainerCustomization::GetClearAllVisibility)
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBorder)
				.Padding(4.0f)
				.Visibility(this, &FGameplayTagContainerCustomization::GetTagsListVisibility)
				[
					ActiveTags()
				]
			]
		];

	GEditor->RegisterForUndo(this);
}

TSharedRef<SWidget> FGameplayTagContainerCustomization::ActiveTags()
{	
	RefreshTagList();
	
	SAssignNew( TagListView, SListView<TSharedPtr<FString>> )
	.ListItemsSource(&TagNames)
	.SelectionMode(ESelectionMode::None)
	.OnGenerateRow(this, &FGameplayTagContainerCustomization::MakeListViewWidget);

	return TagListView->AsShared();
}

void FGameplayTagContainerCustomization::RefreshTagList()
{
	// Rebuild Editable Containers as container references can become unsafe
	BuildEditableContainerList();

	// Clear the list
	TagNames.Empty();

	// Add tags to list
	for (int32 ContainerIdx = 0; ContainerIdx < EditableContainers.Num(); ++ContainerIdx)
	{
		FGameplayTagContainer* Container = EditableContainers[ContainerIdx].TagContainer;
		if (Container)
		{
			for (auto It = Container->CreateConstIterator(); It; ++It)
			{
				TagNames.Add(MakeShareable(new FString(It->ToString())));
			}
		}
	}

	// Refresh the slate list
	if( TagListView.IsValid() )
	{
		TagListView->RequestListRefresh();
	}
}

TSharedRef<ITableRow> FGameplayTagContainerCustomization::MakeListViewWidget(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew( STableRow< TSharedPtr<FString> >, OwnerTable )
			[
				SNew(STextBlock) .Text( FText::FromString(*Item.Get()) )
			];
}

FReply FGameplayTagContainerCustomization::OnEditButtonClicked()
{
	if (!StructPropertyHandle.IsValid() || StructPropertyHandle->GetProperty() == nullptr)
	{
		return FReply::Handled();
	}

	TArray<UObject*> OuterObjects;
	StructPropertyHandle->GetOuterObjects(OuterObjects);

	FString Categories;
	if( StructPropertyHandle->GetProperty()->HasMetaData( TEXT("Categories") ) )
	{
		Categories = StructPropertyHandle->GetProperty()->GetMetaData( TEXT("Categories") );
	}

	bool bReadOnly = StructPropertyHandle->GetProperty()->HasAnyPropertyFlags( CPF_EditConst );

	FText Title;
	FText AssetName;
	FText PropertyName = StructPropertyHandle->GetPropertyDisplayName();

	if (OuterObjects.Num() > 1)
	{
		AssetName = FText::Format( LOCTEXT("GameplayTagDetailsBase_MultipleAssets", "{0} Assets"), FText::AsNumber( OuterObjects.Num() ) );
		Title = FText::Format( LOCTEXT("GameplayTagContainerCustomization_BaseWidgetTitle", "Tag Editor: {0} {1}"), PropertyName, AssetName );
	}
	else if (OuterObjects.Num() > 0 && OuterObjects[0])
	{
		AssetName = FText::FromString( OuterObjects[0]->GetName() );
		Title = FText::Format( LOCTEXT("GameplayTagContainerCustomization_BaseWidgetTitle", "Tag Editor: {0} {1}"), PropertyName, AssetName );
	}

	GameplayTagWidgetWindow = SNew(SWindow)
	.Title(Title)
	.ClientSize(FVector2D(600, 400))
	[
		SAssignNew(GameplayTagWidget, SGameplayTagWidget, EditableContainers)
		.Filter( Categories )
		.OnTagChanged(this, &FGameplayTagContainerCustomization::RefreshTagList)
		.ReadOnly(bReadOnly)
		.TagContainerName( StructPropertyHandle->GetPropertyDisplayName().ToString() )
		.PropertyHandle( StructPropertyHandle )
	];

	GameplayTagWidgetWindow->GetOnWindowDeactivatedEvent().AddRaw(this, &FGameplayTagContainerCustomization::OnGameplayTagWidgetWindowDeactivate);

	if (FGlobalTabmanager::Get()->GetRootWindow().IsValid())
	{
		FSlateApplication::Get().AddWindowAsNativeChild(GameplayTagWidgetWindow.ToSharedRef(), FGlobalTabmanager::Get()->GetRootWindow().ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(GameplayTagWidgetWindow.ToSharedRef());
	}

	return FReply::Handled();
}

FReply FGameplayTagContainerCustomization::OnClearAllButtonClicked()
{
	FScopedTransaction Transaction(LOCTEXT("GameplayTagContainerCustomization_RemoveAllTags", "Remove All Gameplay Tags"));

	for (int32 ContainerIdx = 0; ContainerIdx < EditableContainers.Num(); ++ContainerIdx)
	{
		FGameplayTagContainer* Container = EditableContainers[ContainerIdx].TagContainer;

		if (Container)
		{
			FGameplayTagContainer EmptyContainer;
			StructPropertyHandle->SetValueFromFormattedString(EmptyContainer.ToString());
			RefreshTagList();
		}
	}
	return FReply::Handled();
}

EVisibility FGameplayTagContainerCustomization::GetClearAllVisibility() const
{
	return TagNames.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FGameplayTagContainerCustomization::GetTagsListVisibility() const
{
	return TagNames.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed;
}

void FGameplayTagContainerCustomization::PostUndo( bool bSuccess )
{
	if( bSuccess )
	{
		RefreshTagList();
	}	
}

void FGameplayTagContainerCustomization::PostRedo( bool bSuccess )
{
	if( bSuccess )
	{
		RefreshTagList();
	}	
}

FGameplayTagContainerCustomization::~FGameplayTagContainerCustomization()
{
	if( GameplayTagWidgetWindow.IsValid() )
	{
		GameplayTagWidgetWindow->RequestDestroyWindow();
	}
	GEditor->UnregisterForUndo(this);
}

void FGameplayTagContainerCustomization::BuildEditableContainerList()
{	
	EditableContainers.Empty();

	if( StructPropertyHandle.IsValid() )
	{
		TArray<void*> RawStructData;
		StructPropertyHandle->AccessRawData(RawStructData);

		for (int32 ContainerIdx = 0; ContainerIdx < RawStructData.Num(); ++ContainerIdx)
		{
			EditableContainers.Add(SGameplayTagWidget::FEditableGameplayTagContainerDatum(nullptr, (FGameplayTagContainer*)RawStructData[ContainerIdx]));
		}
	}	
}

void FGameplayTagContainerCustomization::OnGameplayTagWidgetWindowDeactivate()
{
	if( GameplayTagWidgetWindow.IsValid() )
	{
		if (GameplayTagWidget.IsValid() && GameplayTagWidget->IsAddingNewTag() == false)
		{
			GameplayTagWidgetWindow->RequestDestroyWindow();
		}
	}
}

#undef LOCTEXT_NAMESPACE
