// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DataTableEditorPrivatePCH.h"

#include "SRowEditor.h"
#include "PropertyEditorModule.h"
#include "IStructureDetailsView.h"
#include "IDetailsView.h"
#include "DataTableEditorUtils.h"

#define LOCTEXT_NAMESPACE "SRowEditor"

class FStructFromDataTable : public FStructOnScope
{
	TWeakObjectPtr<UDataTable> DataTable;
	FName RowName;

public:
	FStructFromDataTable(UDataTable* InDataTable, FName InRowName)
		: FStructOnScope()
		, DataTable(InDataTable)
		, RowName(InRowName)
	{}

	virtual uint8* GetStructMemory() override
	{
		return (DataTable.IsValid() && !RowName.IsNone()) ? DataTable->FindRowUnchecked(RowName) : NULL;
	}

	virtual const uint8* GetStructMemory() const override
	{
		return (DataTable.IsValid() && !RowName.IsNone()) ? DataTable->FindRowUnchecked(RowName) : NULL;
	}

	virtual const UScriptStruct* GetStruct() const override
	{
		return DataTable.IsValid() ? DataTable->RowStruct : NULL;
	}

	virtual bool IsValid() const override
	{
		return !RowName.IsNone()
			&& DataTable.IsValid()
			&& DataTable->RowStruct
			&& DataTable->FindRowUnchecked(RowName);
	}

	virtual void Destroy() override
	{
		DataTable = NULL;
		RowName = NAME_None;
	}

	FName GetRowName() const
	{
		return RowName;
	}
};

SRowEditor::SRowEditor() : SCompoundWidget()
{
}

SRowEditor::~SRowEditor()
{
}

void SRowEditor::OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent)
{
	check(PropertyChangedEvent.MemberProperty
		&& PropertyChangedEvent.MemberProperty->GetOwnerStruct()
		&& (PropertyChangedEvent.MemberProperty->GetOwnerStruct() == GetScriptStruct()));

	// TODO:
	// FNotifyHook won't be called, because the struct has no "property-path" from DT object. THe struct is seen as detached.
	// But the notification must be called somewhere.

	FDataTableEditorUtils::BroadcastPostChange(DataTable.Get(), FDataTableEditorUtils::EDataTableChangeInfo::RowDataPostChangeOnly);

	if (DataTable.IsValid())
	{
		DataTable->MarkPackageDirty();
	}
}

void SRowEditor::PreChange(const class UUserDefinedStruct* Struct, FStructureEditorUtils::EStructureEditorChangeInfo Info)
{
	if (Struct && (GetScriptStruct() == Struct))
	{
		CleanBeforeChange();
	}
}

void SRowEditor::PostChange(const class UUserDefinedStruct* Struct, FStructureEditorUtils::EStructureEditorChangeInfo Info)
{
	if (Struct && (GetScriptStruct() == Struct))
	{
		Restore();
	}
}

void SRowEditor::PreChange(const UDataTable* Changed, FDataTableEditorUtils::EDataTableChangeInfo Info)
{
	if ((Changed == DataTable.Get()) && (FDataTableEditorUtils::EDataTableChangeInfo::RowList == Info))
	{
		CleanBeforeChange();
	}
}

void SRowEditor::PostChange(const UDataTable* Changed, FDataTableEditorUtils::EDataTableChangeInfo Info)
{
	FStringAssetReference::InvalidateTag(); // Should be removed after UE-5615 is fixed
	if ((Changed == DataTable.Get()) && (FDataTableEditorUtils::EDataTableChangeInfo::RowList == Info))
	{
		RefreshNameList();
		Restore();
	}
}

void SRowEditor::CleanBeforeChange()
{
	if (StructureDetailsView.IsValid())
	{
		StructureDetailsView->SetStructureData(NULL);
	}
	if (CurrentRow.IsValid())
	{
		CurrentRow->Destroy();
		CurrentRow.Reset();
	}
}

void SRowEditor::RefreshNameList()
{
	CachedRowNames.Empty();
	if (DataTable.IsValid())
	{
		auto RowNames = DataTable->GetRowNames();
		for (auto RowName : RowNames)
		{
			CachedRowNames.Add(MakeShareable(new FName(RowName)));
		}
	}
}

void SRowEditor::Restore()
{
	if (SelectedName.IsValid())
	{
		auto CurrentName = *SelectedName;
		SelectedName = NULL;
		for (auto Element : CachedRowNames)
		{
			if (*Element == CurrentName)
			{
				SelectedName = Element;
				break;
			}
		}
	}

	if (!SelectedName.IsValid() && CachedRowNames.Num() && CachedRowNames[0].IsValid())
	{
		SelectedName = CachedRowNames[0];
	}

	if (RowComboBox.IsValid())
	{
		RowComboBox->SetSelectedItem(SelectedName);
	}

	auto FinalName = SelectedName.IsValid() ? *SelectedName : NAME_None;
	CurrentRow = MakeShareable(new FStructFromDataTable(DataTable.Get(), FinalName));
	if (StructureDetailsView.IsValid())
	{
		StructureDetailsView->SetStructureData(CurrentRow);
	}

	RowSelectedCallback.ExecuteIfBound(FinalName);
}

UScriptStruct* SRowEditor::GetScriptStruct() const
{
	return DataTable.IsValid() ? DataTable->RowStruct : NULL;
}

FName SRowEditor::GetCurrentName() const
{
	return SelectedName.IsValid() ? *SelectedName : NAME_None;
}

FText SRowEditor::GetCurrentNameAsText() const
{
	return FText::FromName(GetCurrentName());
}

FString SRowEditor::GetStructureDisplayName() const
{
	const auto Struct = DataTable.IsValid() ? DataTable->RowStruct : NULL;
	return Struct 
		? Struct->GetDisplayNameText().ToString()
		: LOCTEXT("Error_UnknownStruct", "Error: Unknown Struct").ToString();
}

TSharedRef<SWidget> SRowEditor::OnGenerateWidget(TSharedPtr<FName> InItem)
{
	return SNew(STextBlock).Text(FText::FromName(InItem.IsValid() ? *InItem : NAME_None));
}

void SRowEditor::OnSelectionChanged(TSharedPtr<FName> InItem, ESelectInfo::Type InSeletionInfo)
{
	if (InItem != SelectedName)
	{
		CleanBeforeChange();

		SelectedName = InItem;

		Restore();
	}
}

void SRowEditor::SelectRow(FName InName)
{
	TSharedPtr<FName> NewSelectedName;
	for (auto Name : CachedRowNames)
	{
		if (Name.IsValid() && (*Name == InName))
		{
			NewSelectedName = Name;
		}
	}
	if (!NewSelectedName->IsValid())
	{
		NewSelectedName = MakeShareable(new FName(InName));
	}
	OnSelectionChanged(NewSelectedName, ESelectInfo::Direct);
}

FReply SRowEditor::OnAddClicked()
{
	if (DataTable.IsValid())
	{
		FName NewName = DataTable->MakeValidName(TEXT("NewRow"));
		const TArray<FName> ExisitngNames = DataTable->GetRowNames();
		while (ExisitngNames.Contains(NewName))
		{
			NewName.SetNumber(NewName.GetNumber() + 1);
		}
		SelectedName = MakeShareable(new FName(NewName));
		FDataTableEditorUtils::AddRow(DataTable.Get(), NewName);
	}
	return FReply::Handled();
}

FReply SRowEditor::OnRemoveClicked()
{
	if (DataTable.IsValid())
	{
		const FName RowToRemove = GetCurrentName();
		FDataTableEditorUtils::RemoveRow(DataTable.Get(), RowToRemove);
	}
	return FReply::Handled();
}

void SRowEditor::OnRowRenamed(const FText& Text, ETextCommit::Type CommitType)
{
	if (!GetCurrentNameAsText().EqualTo(Text) && DataTable.IsValid())
	{
		const FName OldName = GetCurrentName();
		const FName NewName = DataTable->MakeValidName(Text.ToString());
		SelectedName = MakeShareable(new FName(NewName));
		FDataTableEditorUtils::RenameRow(DataTable.Get(), OldName, NewName);
	}
}

void SRowEditor::Construct(const FArguments& InArgs, UDataTable* Changed)
{
	DataTable = Changed;
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		FDetailsViewArgs ViewArgs;
		ViewArgs.bAllowSearch = false;
		ViewArgs.bHideSelectionTip = false;
		ViewArgs.bShowActorLabel = false;

		StructureDetailsView = PropertyModule.CreateStructureDetailView(ViewArgs, CurrentRow, false, LOCTEXT("RowValue", "Row Value"));
		StructureDetailsView->GetOnFinishedChangingPropertiesDelegate().AddSP(this, &SRowEditor::OnFinishedChangingProperties);
	}

	RefreshNameList();
	Restore();
	const float ButtonWidth = 85.0f;
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2)
			[
				SNew(SBox)
				.MinDesiredWidth(ButtonWidth)
				[
					SNew(SButton)
					.OnClicked(this, &SRowEditor::OnAddClicked)
					.HAlign(EHorizontalAlignment::HAlign_Center)
					.Text(LOCTEXT("AddRow", "Add"))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2)
			[
				SNew(SBox)
				.MinDesiredWidth(ButtonWidth)
				[
					SNew(SButton)
					.OnClicked(this, &SRowEditor::OnRemoveClicked)
					.HAlign(EHorizontalAlignment::HAlign_Center)
					.Text(LOCTEXT("RemoveRow", "Remove"))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2)
			.VAlign(EVerticalAlignment::VAlign_Center)
			[
				SNew(SBox)
				.HAlign(EHorizontalAlignment::HAlign_Right)
				[
					SNew(STextBlock).Text(LOCTEXT("RenameRow", "Rename:"))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2)
			[
				SNew(SBox)
				.WidthOverride(2 * ButtonWidth)
				[
					SNew(SEditableTextBox)
					.Text(this, &SRowEditor::GetCurrentNameAsText)
					.OnTextCommitted(this, &SRowEditor::OnRowRenamed)
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2)
			.VAlign(EVerticalAlignment::VAlign_Center)
			[
				SNew(SBox)
				.HAlign(EHorizontalAlignment::HAlign_Right)
				[
					SNew(STextBlock).Text(LOCTEXT("SelectRow", "Select:"))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2)
			[
				SNew(SBox)
				.WidthOverride(2 * ButtonWidth)
				[
					SAssignNew(RowComboBox, SComboBox<TSharedPtr<FName>>)
					.OptionsSource(&CachedRowNames)
					.OnSelectionChanged(this, &SRowEditor::OnSelectionChanged)
					.OnGenerateWidget(this, &SRowEditor::OnGenerateWidget)
					.Content()
					[
						SNew(STextBlock).Text(this, &SRowEditor::GetCurrentNameAsText)
					]
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			StructureDetailsView->GetWidget().ToSharedRef()
		]
	];
}

