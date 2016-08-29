// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PersonaPrivatePCH.h"
#include "SSkeletonBlendProfiles.h"
#include "Animation/BlendProfile.h"
#include "Persona.h"
#include "SSearchBox.h"
#include "STextEntryPopup.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "SkeletonBlendProfiles"

static const FName ColumnId_BlendProfileName("BlendProfileName");
static const FName ColumnId_NumEntries("NumEntries");

//////////////////////////////////////////////////////////////////////////

class SBlendProfileListRow : public SMultiColumnTableRow<TSharedPtr<FDisplayedBlendProfileInfo>>
{
public:

	SLATE_BEGIN_ARGS(SBlendProfileListRow)
	{}

	SLATE_ARGUMENT(TSharedPtr<FDisplayedBlendProfileInfo>, Item)
	SLATE_ARGUMENT(TSharedPtr<SSkeletonBlendProfiles>, BlendProfilesView)
	SLATE_ARGUMENT(TWeakPtr<FPersona>, WeakPersona)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);

	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& InColumnName ) override;

private:

	// Get the number of entries in the active blend profile as text
	FText GetBlendProfileNumEntriesText() const;

	// The item we are observing
	TSharedPtr<FDisplayedBlendProfileInfo> Item;

	// The view widget that owns us
	TSharedPtr<SSkeletonBlendProfiles> BlendProfilesView;

	// Hosting Persona instance
	TWeakPtr<FPersona> WeakPersona;
};

void SBlendProfileListRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	Item = InArgs._Item;
	BlendProfilesView = InArgs._BlendProfilesView;
	WeakPersona = InArgs._WeakPersona;

	SMultiColumnTableRow<TSharedPtr<FDisplayedBlendProfileInfo>>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
}

TSharedRef<SWidget> SBlendProfileListRow::GenerateWidgetForColumn(const FName& InColumnName)
{
	if(Item.IsValid() && Item->ObservedBlendProfile.IsValid())
	{
		UBlendProfile* Profile = Item->ObservedBlendProfile.Get();
		if(InColumnName == ColumnId_BlendProfileName)
		{
			return SNew(STextBlock).Text(FText::FromName(Profile->GetFName()));
		}
		else if(InColumnName == ColumnId_NumEntries)
		{
			return SNew(STextBlock).Text(this, &SBlendProfileListRow::GetBlendProfileNumEntriesText);
		}
	}

	return SNullWidget::NullWidget;
}

FText SBlendProfileListRow::GetBlendProfileNumEntriesText() const
{
	if(Item.IsValid() && Item->ObservedBlendProfile.IsValid())
	{
		UBlendProfile* Profile = Item->ObservedBlendProfile.Get();
		return FText::AsNumber(Profile->GetNumBlendEntries());
	}
	return FText::GetEmpty();
}

//////////////////////////////////////////////////////////////////////////

FSkeletonBlendProfilesSummoner::FSkeletonBlendProfilesSummoner(TSharedPtr<FAssetEditorToolkit> InHostingApp)
	: FWorkflowTabFactory(FPersonaTabs::BlendProfileManagerID, InHostingApp)
{
	TabLabel = LOCTEXT("TabLabel", "Blend Profiles");
	
	EnableTabPadding();
	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("MenuDescription", "Blend Profiles");
	ViewMenuTooltip = LOCTEXT("MenuToolTip", "Shows the blend profile manager");
}

TSharedRef<SWidget> FSkeletonBlendProfilesSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	return SNew(SSkeletonBlendProfiles)
		.WeakPersona(StaticCastSharedPtr<FPersona>(HostingApp.Pin()));
}

//////////////////////////////////////////////////////////////////////////

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SSkeletonBlendProfiles::Construct(const FArguments& InArgs)
{
	WeakPersona = InArgs._WeakPersona;

	OnSuccesfulRenameCommit = FSuccessfulCommit::CreateSP(this, &SSkeletonBlendProfiles::RenameSelectedProfile);
	OnSuccesfulNewProfileCommit = FSuccessfulCommit::CreateSP(this, &SSkeletonBlendProfiles::CreateNewBlendProfile);

	TSharedPtr<FPersona> SharedPersona = WeakPersona.Pin();
	if(SharedPersona.IsValid())
	{
		CurrentSkeleton = SharedPersona->GetSkeleton();

		ChildSlot
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(SearchBox, SSearchBox)
				.SelectAllTextWhenFocused(true)
				.HintText(LOCTEXT("SearchBoxHint", "Search Blend Profiles..."))
			]

			+SVerticalBox::Slot()
			[
				SAssignNew(ListView, SBlendProfileListView)
				.ListItemsSource(&BlendProfileList)
				.OnGenerateRow(this, &SSkeletonBlendProfiles::GenerateViewRow)
				.OnContextMenuOpening(this, &SSkeletonBlendProfiles::GetContextMenu)
				.OnSelectionChanged(this, &SSkeletonBlendProfiles::OnListSelectionChanged)
				.ItemHeight(22.0f)
				.HeaderRow
				(
					SNew(SHeaderRow)
					+SHeaderRow::Column(ColumnId_BlendProfileName)
					.DefaultLabel(LOCTEXT("NameColumnLabel", "Blend Profile Name"))
					+SHeaderRow::Column(ColumnId_NumEntries)
					.DefaultLabel(LOCTEXT("NumEntriesColumnLabel", "Blend Entry Count"))
				)
			]
		];

		RebuildBlendProfileList();
	}
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

TSharedRef<ITableRow> SSkeletonBlendProfiles::GenerateViewRow(TSharedPtr<FDisplayedBlendProfileInfo> InInfo, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SBlendProfileListRow, OwnerTable)
		.WeakPersona(WeakPersona)
		.Item(InInfo)
		.BlendProfilesView(SharedThis(this));
}

TSharedPtr<SWidget> SSkeletonBlendProfiles::GetContextMenu() const
{
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("BlendProfileActions", "Blend Profile Actions"));
	{
		{
			FUIAction Action;
			Action.ExecuteAction = FExecuteAction::CreateSP(this, &SSkeletonBlendProfiles::OnRequestNewBlendProfile);

			const FText Label = LOCTEXT("Menu_NewProfileLabel", "Create New...");
			const FText ToolTipText = LOCTEXT("Menu_NewProfileToolTip", "Create a new blend profile in the current skeleton.");

			MenuBuilder.AddMenuEntry(Label, ToolTipText, FSlateIcon(), Action);
		}
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("SelectedBlendProfileActions", "Selected Blend Profile Actions"));
	{
		{
			FUIAction Action;
			Action.ExecuteAction = FExecuteAction::CreateSP(this, &SSkeletonBlendProfiles::DeleteSelectedProfiles);
			Action.CanExecuteAction = FCanExecuteAction::CreateSP(this, &SSkeletonBlendProfiles::MenuCanDelete);

			const FText Label = LOCTEXT("Menu_DeleteProfileLabel", "Delete");
			const FText ToolTipText = LOCTEXT("Menu_DeleteProfileToolTip", "Delete selected blend profiles.");

			MenuBuilder.AddMenuEntry(Label, ToolTipText, FSlateIcon(), Action);
		}

		{
			FUIAction Action;
			Action.ExecuteAction = FExecuteAction::CreateSP(this, &SSkeletonBlendProfiles::OnRequestRenameSelectedProfile);
			Action.CanExecuteAction = FCanExecuteAction::CreateSP(this, &SSkeletonBlendProfiles::MenuCanRename);

			const FText Label = LOCTEXT("Menu_RenameProfileLabel", "Rename");
			const FText ToolTipText = LOCTEXT("Menu_RenameProfileToolTip", "Rename selected profile.");

			MenuBuilder.AddMenuEntry(Label, ToolTipText, FSlateIcon(), Action);
		}
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SSkeletonBlendProfiles::RebuildBlendProfileList()
{
	BlendProfileList.Empty();

	if(CurrentSkeleton)
	{
		for(UBlendProfile* BlendProfile : CurrentSkeleton->BlendProfiles)
		{
			TWeakObjectPtr<UBlendProfile> WeakBlendProfile(BlendProfile);
			BlendProfileList.Add(FDisplayedBlendProfileInfo::Make(WeakBlendProfile));
		}
	}

	ListView->RequestListRefresh();
}

bool SSkeletonBlendProfiles::MenuCanDelete() const
{
	TArray<TSharedPtr<FDisplayedBlendProfileInfo>> SelectedRows = ListView->GetSelectedItems();
	return SelectedRows.Num() > 0;
}

bool SSkeletonBlendProfiles::MenuCanRename() const
{
	TArray<TSharedPtr<FDisplayedBlendProfileInfo>> SelectedRows = ListView->GetSelectedItems();
	return SelectedRows.Num() == 1;
}

void SSkeletonBlendProfiles::OnRequestNewBlendProfile()
{
	SAssignNew(CurrentTextEntryPopup, STextEntryPopup)
		.Label(LOCTEXT("RenamePopupLabel", "New Profile Name..."))
		.OnTextChanged(this, &SSkeletonBlendProfiles::ValidateUserTextEntry)
		.OnTextCommitted(FOnTextCommitted::CreateSP(this, &SSkeletonBlendProfiles::HandleTextEntryCommit, OnSuccesfulNewProfileCommit));

	FSlateApplication::Get().PushMenu(
		AsShared(),
		FWidgetPath(),
		CurrentTextEntryPopup.ToSharedRef(),
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup));
}

void SSkeletonBlendProfiles::CreateNewBlendProfile(const FName& InNewName)
{
	if(CurrentSkeleton)
	{
		FScopedTransaction(LOCTEXT("NewBlendProfileTransaction", "Created new Blend Profile"));
		CurrentSkeleton->CreateNewBlendProfile(InNewName);

		RefreshObservedData();
	}
}

void SSkeletonBlendProfiles::DeleteSelectedProfiles()
{
	TArray<TSharedPtr<FDisplayedBlendProfileInfo>> SelectedItems = ListView->GetSelectedItems();

	if(SelectedItems.Num() > 0)
	{
		FScopedTransaction Transaction(LOCTEXT("DeleteTransaction", "Delete Blend Profiles"));
		for(TSharedPtr<FDisplayedBlendProfileInfo>& Item : SelectedItems)
		{
			if(Item->ObservedBlendProfile.IsValid())
			{
				UBlendProfile* Profile = Item->ObservedBlendProfile.Get();
				CurrentSkeleton->Modify();
				
				CurrentSkeleton->BlendProfiles.RemoveAll([Profile](UBlendProfile* Current)
				{
					if(Current == Profile)
					{
						Current->MarkPendingKill();
						return true;
					}

					return false;
				});
			}
		}

		TSharedPtr<FPersona> SharedPersona = WeakPersona.Pin();
		if(SharedPersona.IsValid())
		{
			SharedPersona->SetSelectedBlendProfile(nullptr);
		}

		RefreshObservedData();
	}
}

void SSkeletonBlendProfiles::OnRequestRenameSelectedProfile()
{
	SAssignNew(CurrentTextEntryPopup, STextEntryPopup)
		.Label(LOCTEXT("RenamePopupLabel", "New Profile Name..."))
		.OnTextChanged(this, &SSkeletonBlendProfiles::ValidateUserTextEntry)
		.OnTextCommitted(FOnTextCommitted::CreateSP(this, &SSkeletonBlendProfiles::HandleTextEntryCommit, OnSuccesfulRenameCommit));

	FSlateApplication::Get().PushMenu(
		AsShared(),
		FWidgetPath(),
		CurrentTextEntryPopup.ToSharedRef(),
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup));
}

void SSkeletonBlendProfiles::ValidateUserTextEntry(const FText& InText)
{
	if(CurrentSkeleton && CurrentTextEntryPopup.IsValid())
	{
		bCurrentTextEntryHasError = false;
		FString InputAsString = InText.ToString();

		for(UBlendProfile* Profile : CurrentSkeleton->BlendProfiles)
		{
			// Exists in skeleton
			if(InText.ToString() == Profile->GetName())
			{
				CurrentTextEntryPopup->SetError(FText::Format(LOCTEXT("RenameErrorString", "Error \"{0}\" already exists!"), InText));
				bCurrentTextEntryHasError = true;
				break;
			}
		}

		if(!bCurrentTextEntryHasError)
		{
			// Check for further errors
			FName PossibleNewName(*InputAsString);
			FText NameErrorString;

			if(!PossibleNewName.IsValidObjectName(NameErrorString))
			{
				CurrentTextEntryPopup->SetError(NameErrorString);
				bCurrentTextEntryHasError = true;
			}
		}

		if(!bCurrentTextEntryHasError)
		{
			// Errors, clear any message
			CurrentTextEntryPopup->SetError(FText::GetEmpty());
		}
	}
}

void SSkeletonBlendProfiles::HandleTextEntryCommit(const FText& InText, ETextCommit::Type InCommitType, FSuccessfulCommit OnCommit)
{
	check(CurrentTextEntryPopup.IsValid());

	bool bDismissMenus = true;

	if(InCommitType == ETextCommit::OnEnter)
	{
		if(!bCurrentTextEntryHasError && OnCommit.IsBound())
		{
			OnCommit.Execute(FName(*InText.ToString()));
		}
		else
		{
			bDismissMenus = false;
			CurrentTextEntryPopup->FocusDefaultWidget();
		}
	}

	if(bDismissMenus)
	{
		FSlateApplication::Get().DismissAllMenus();
	}
}

void SSkeletonBlendProfiles::RenameSelectedProfile(const FName& NewName)
{
	TArray<TSharedPtr<FDisplayedBlendProfileInfo>> SelectedItems = ListView->GetSelectedItems();
	check(SelectedItems.Num() == 1);

	TSharedPtr<FDisplayedBlendProfileInfo> Item = SelectedItems[0];

	if(Item->ObservedBlendProfile.IsValid())
	{
		FScopedTransaction Transaction(LOCTEXT("RenameTransaction", "Rename Blend Profile"));
		
		UBlendProfile* Profile = Item->ObservedBlendProfile.Get();
		Profile->Rename(*NewName.ToString());
	}

	RefreshObservedData();
}

void SSkeletonBlendProfiles::RefreshObservedData()
{
	RebuildBlendProfileList();
	ListView->RequestListRefresh();
}

void SSkeletonBlendProfiles::OnListSelectionChanged(TSharedPtr<FDisplayedBlendProfileInfo> Item, ESelectInfo::Type InSelectionType)
{
	TArray<TSharedPtr<FDisplayedBlendProfileInfo>> SelectedItems = ListView->GetSelectedItems();

	TSharedPtr<FPersona> SharedPersona = WeakPersona.Pin();
	if(SharedPersona.IsValid())
	{
		UBlendProfile* ProfileToDisplayInTree = nullptr;

		if(SelectedItems.Num() == 1)
		{
			TSharedPtr<FDisplayedBlendProfileInfo> SelectedItem = SelectedItems[0];
			if (SelectedItem->ObservedBlendProfile.IsValid())
			{
				SharedPersona->SetSelectedBlendProfile(SelectedItem->ObservedBlendProfile.Get());
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
