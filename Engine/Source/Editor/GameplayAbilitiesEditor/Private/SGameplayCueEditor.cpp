// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemEditorPrivatePCH.h"
#include "SGameplayCueEditor.h"
#include "GameplayCueSet.h"
#include "GameplayCueManager.h"
#include "Editor/ContentBrowser/Public/ContentBrowserModule.h"
#include "GameplayCueNotify_Static.h"
#include "GameplayCueNotify_Actor.h"
#include "SExpandableArea.h"
#include "SlateIconFinder.h"
#include "EditorClassUtils.h"
#include "GameplayTagsSettings.h"
#include "SNotificationList.h"
#include "NotificationManager.h"
#include "SHyperlink.h"
#include "SSearchBox.h"
#include "GameplayTagsModule.h"
#include "AssetEditorManager.h"
#include "AbilitySystemGlobals.h"
#include "AssetToolsModule.h"

#define LOCTEXT_NAMESPACE "SGameplayCueEditor"


/** Widget for picking a new GameplayCue Notify class (similar to actor class picker)  */
class SGameplayCuePickerDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SGameplayCuePickerDialog )
	{}

		SLATE_ARGUMENT( TSharedPtr<SWindow>, ParentWindow )
		SLATE_ARGUMENT( TArray<UClass*>, DefaultClasses )
		SLATE_ARGUMENT( FString, GameplayCueTag )

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

	static bool PickGameplayCue(const FText& TitleText, const TArray<UClass*>& DefaultClasses, UClass*& OutChosenClass, FString InGameplayCueTag);

private:
	/** Handler for when a class is picked in the class picker */
	void OnClassPicked(UClass* InChosenClass);

	/** Creates the default class widgets */
	TSharedRef<ITableRow> GenerateListRow(UClass* InItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** Handler for when "Ok" we selected in the class viewer */
	FReply OnClassPickerConfirmed();

	FReply OnDefaultClassPicked(UClass* InChosenClass);

	/** Handler for the custom button to hide/unhide the default class viewer */
	void OnDefaultAreaExpansionChanged(bool bExpanded);

	/** Handler for the custom button to hide/unhide the class viewer */
	void OnCustomAreaExpansionChanged(bool bExpanded);	

private:
	/** A pointer to the window that is asking the user to select a parent class */
	TWeakPtr<SWindow> WeakParentWindow;

	/** The class that was last clicked on */
	UClass* ChosenClass;

	/** A flag indicating that Ok was selected */
	bool bPressedOk;

	/**  An array of default classes used in the dialog **/
	TArray<UClass*> DefaultClasses;

	FString GameplayCueTag;
};

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SGameplayCuePickerDialog::Construct(const FArguments& InArgs)
{
	WeakParentWindow = InArgs._ParentWindow;
	DefaultClasses = InArgs._DefaultClasses;
	GameplayCueTag = InArgs._GameplayCueTag;

	FLinearColor AssetColor = FLinearColor::White;

	bPressedOk = false;
	ChosenClass = NULL;

	FString PathStr = SGameplayCueEditor::GetPathNameForGameplayCueTag(GameplayCueTag);
	FGameplayCueEditorStrings Strings;
	auto del = IGameplayAbilitiesEditorModule::Get().GetGameplayCueEditorStringsDelegate();
	if (del.IsBound())
	{
		Strings = del.Execute();
	}

	ChildSlot
	[
		SNew(SBorder)
		.Visibility(EVisibility::Visible)
		.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
		[
			SNew(SBox)
			.Visibility(EVisibility::Visible)
			.Padding(2.f)
			.WidthOverride(520.0f)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.Padding(2.f, 2.f)
				.AutoHeight()
				[
					SNew(SBorder)
					.Visibility(EVisibility::Visible)
					.BorderImage( FEditorStyle::GetBrush("AssetThumbnail.AssetBackground") )
					.BorderBackgroundColor(AssetColor.CopyWithNewOpacity(0.3f))
					[
						SNew(SExpandableArea)
						.AreaTitle(NSLOCTEXT("SGameplayCuePickerDialog", "CommonClassesAreaTitle", "GameplayCue Notifies"))
						.BodyContent()
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.Padding(2.f, 2.f)
							.AutoHeight()
							[
								SNew(STextBlock)
								
								.Text(FText::FromString(Strings.GameplayCueNotifyDescription1))
								.AutoWrapText(true)
							]

							+SVerticalBox::Slot()
							.AutoHeight( )
							[
								SNew(SListView < UClass*  >)
								.ItemHeight(48)
								.SelectionMode(ESelectionMode::None)
								.ListItemsSource(&DefaultClasses)
								.OnGenerateRow(this, &SGameplayCuePickerDialog::GenerateListRow)
							]

							+SVerticalBox::Slot()
							.Padding(2.f, 2.f)
							.AutoHeight()
							[
								SNew(STextBlock)
								.Text(FText::FromString(FString::Printf(TEXT("This will create a new GameplayCue Notify here:"))))
								.AutoWrapText(true)
							]
							+SVerticalBox::Slot()
							.Padding(2.f, 2.f)
							.AutoHeight()
							[
								SNew(STextBlock)
								.Text(FText::FromString(PathStr))
								.HighlightText(FText::FromString(PathStr))
								.AutoWrapText(true)
							]
							+SVerticalBox::Slot()
							.Padding(2.f, 2.f)
							.AutoHeight()
							[
								SNew(STextBlock)
								.Text(FText::FromString(Strings.GameplayCueNotifyDescription2))
								.AutoWrapText(true)
							]
						]
					]
				]

				+SVerticalBox::Slot()
				.Padding(2.f, 2.f)
				.AutoHeight()
				[
					SNew(SBorder)
					.Visibility(EVisibility::Visible)
					.BorderImage( FEditorStyle::GetBrush("AssetThumbnail.AssetBackground") )
					.BorderBackgroundColor(AssetColor.CopyWithNewOpacity(0.3f))
					[
						SNew(SExpandableArea)
						.AreaTitle(NSLOCTEXT("SGameplayCuePickerDialogEvents", "CommonClassesAreaTitleEvents", "Custom BP Events"))
						.BodyContent()
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.Padding(2.f, 2.f)
							.AutoHeight()
							[
								SNew(STextBlock)
								.Text(FText::FromString(Strings.GameplayCueEventDescription1))
								.AutoWrapText(true)

							]

							+SVerticalBox::Slot()
							.Padding(2.f, 2.f)
							.AutoHeight()
							[
								SNew(STextBlock)
								.Text(FText::FromString(Strings.GameplayCueEventDescription2))
								.AutoWrapText(true)
							]							
						]
					]
				]
			]
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

static const FName CueTagColumnName("GameplayCueTags");
static const FName CueHandlerColumnName("GameplayCueHandlers");

/** Spawns window for picking new GameplayCue handler/notify */
bool SGameplayCuePickerDialog::PickGameplayCue(const FText& TitleText, const TArray<UClass*>& DefaultClasses, UClass*& OutChosenClass, FString InGameplayCueName)
{
	// Create the window to pick the class
	TSharedRef<SWindow> PickerWindow = SNew(SWindow)
		.Title(TitleText)
		.SizingRule( ESizingRule::Autosized )
		.ClientSize( FVector2D( 0.f, 600.f ))
		.SupportsMaximize(false)
		.SupportsMinimize(false);

	TSharedRef<SGameplayCuePickerDialog> ClassPickerDialog = SNew(SGameplayCuePickerDialog)
		.ParentWindow(PickerWindow)
		.DefaultClasses(DefaultClasses)
		.GameplayCueTag(InGameplayCueName);

	PickerWindow->SetContent(ClassPickerDialog);

	GEditor->EditorAddModalWindow(PickerWindow);

	if (ClassPickerDialog->bPressedOk)
	{
		OutChosenClass = ClassPickerDialog->ChosenClass;
		return true;
	}
	else
	{
		// Ok was not selected, NULL the class
		OutChosenClass = NULL;
		return false;
	}
}

void SGameplayCuePickerDialog::OnClassPicked(UClass* InChosenClass)
{
	ChosenClass = InChosenClass;
}

/** Generates rows in the list of GameplayCueNotify classes to pick from */
TSharedRef<ITableRow> SGameplayCuePickerDialog::GenerateListRow(UClass* ItemClass, const TSharedRef<STableViewBase>& OwnerTable)
{	
	const FSlateBrush* ItemBrush = FSlateIconFinder::FindIconBrushForClass(ItemClass);

	return 
	SNew(STableRow< UClass* >, OwnerTable)
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.MaxHeight(60.0f)
		.Padding(10.0f, 6.0f, 0.0f, 4.0f)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(0.65f)
			[
				SNew(SButton)
				.OnClicked(this, &SGameplayCuePickerDialog::OnDefaultClassPicked, ItemClass)
				.Content()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.FillWidth(0.12f)
					[
						SNew(SImage)
						.Image(ItemBrush)
					]
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.Padding(4.0f, 0.0f)
					.FillWidth(0.8f)
					[
						SNew(STextBlock)
						.Text(ItemClass->GetDisplayNameText())
					]

				]
			]
			+SHorizontalBox::Slot()
			.Padding(10.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(ItemClass->GetToolTipText(true))
				.AutoWrapText(true)
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				FEditorClassUtils::GetDocumentationLinkWidget(ItemClass)
			]
		]
	];
}

FReply SGameplayCuePickerDialog::OnDefaultClassPicked(UClass* InChosenClass)
{
	ChosenClass = InChosenClass;
	bPressedOk = true;
	if (WeakParentWindow.IsValid())
	{
		WeakParentWindow.Pin()->RequestDestroyWindow();
	}
	return FReply::Handled();
}

FReply SGameplayCuePickerDialog::OnClassPickerConfirmed()
{
	if (ChosenClass == NULL)
	{
		FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("EditorFactories", "MustChooseClassWarning", "You must choose a class."));
	}
	else
	{
		bPressedOk = true;

		if (WeakParentWindow.IsValid())
		{
			WeakParentWindow.Pin()->RequestDestroyWindow();
		}
	}
	return FReply::Handled();
}

FString SGameplayCueEditor::GetPathNameForGameplayCueTag(FString GameplayCueTagName)
{
	FString NewDefaultPathName;
	auto PathDel = IGameplayAbilitiesEditorModule::Get().GetGameplayCueNotifyPathDelegate();
	if (PathDel.IsBound())
	{
		NewDefaultPathName = PathDel.Execute(GameplayCueTagName);
	}
	else
	{
		GameplayCueTagName = GameplayCueTagName.Replace(TEXT("GameplayCue."), TEXT(""), ESearchCase::IgnoreCase);
		NewDefaultPathName = FString::Printf(TEXT("/Game/GC_%s"), *GameplayCueTagName);
	}
	NewDefaultPathName.ReplaceInline(TEXT("."), TEXT("_"));
	return NewDefaultPathName;
}


class SGameplayCueEditorImpl : public SGameplayCueEditor
{
public:

	virtual void Construct(const FArguments& InArgs) override;

	virtual void OnNewGameplayCueTagCommited(const FText& InText, ETextCommit::Type InCommitType) override
	{
		// Only support adding tags via ini file
		if (UGameplayTagsManager::ShouldImportTagsFromINI() == false)
		{
			return;
		}

		if (InCommitType == ETextCommit::OnEnter)
		{
			CreateNewGameplayCueTag();
		}
	}

	virtual void OnSearchTagCommited(const FText& InText, ETextCommit::Type InCommitType) override
	{
		if (InCommitType == ETextCommit::OnEnter || InCommitType == ETextCommit::OnCleared || InCommitType == ETextCommit::OnUserMovedFocus)
		{
			SearchText = InText;
			UpdateGameplayCueListItems();
		}
	}

	FReply DoSearch()
	{
		UpdateGameplayCueListItems();
		return FReply::Handled();
	}


	virtual FReply OnNewGameplayCueButtonPressed() override
	{
		CreateNewGameplayCueTag();
		return FReply::Handled();
	}

private:

	TSharedPtr<SEditableTextBox>	NewGameplayCueTextBox;

	/**
	*	Checks out config file, adds new tag, repopulates widget cue list
	*/
	void CreateNewGameplayCueTag()
	{
		FScopedSlowTask SlowTask(0.f, LOCTEXT("AddingNewGameplaycue", "Adding new GameplayCue Tag"));
		SlowTask.MakeDialog();

		FString str = NewGameplayCueTextBox->GetText().ToString();
		if (str.IsEmpty())
		{
			return;
		}

		AutoSelectTag = str;

		UGameplayTagsManager::AddNewGameplayTagToINI(str);

		UpdateGameplayCueListItems();

		NewGameplayCueTextBox->SetText(FText::GetEmpty());
	}

	void HandleShowAllCheckedStateChanged(ECheckBoxState NewValue)
	{
		bShowAll = (NewValue == ECheckBoxState::Unchecked);
		UpdateGameplayCueListItems();
	}

	/** Callback for getting the checked state of the focus check box. */
	ECheckBoxState HandleShowAllCheckBoxIsChecked() const
	{
		return bShowAll ? ECheckBoxState::Unchecked : ECheckBoxState::Checked;
	}

	struct FCueItem;
	struct FCueHandlerItem;
	
	/** Base class for any item in the the Cue/Handler Tree. */
	struct FTreeItem : public TSharedFromThis<FTreeItem>
	{
		virtual ~FTreeItem(){}
		virtual TSharedPtr<FCueItem> AsCueItem(){ return TSharedPtr<FCueItem>(); }
		virtual TSharedPtr<FCueHandlerItem> AsCueHandlerItem(){ return TSharedPtr<FCueHandlerItem>(); }
	};

	struct FCueHandlerItem : public FTreeItem
	{
		FStringAssetReference GameplayCueNotifyObj;
		TWeakObjectPtr<UFunction> FunctionPtr;

		virtual TSharedPtr<FCueHandlerItem> AsCueHandlerItem() override
		{
			return SharedThis(this);
		}
	};

	typedef SListView< TSharedPtr< FCueHandlerItem > > SGameplayCueHandlerListView;

	/** An item in the GameplayCue list */
	struct FCueItem : public FTreeItem
	{
		virtual TSharedPtr<FCueItem> AsCueItem(){ return SharedThis(this); }

		FGameplayTag GameplayCueTagName;

		TArray< TSharedPtr< FCueHandlerItem > > HandlerItems;

		TSharedPtr< SGameplayCueHandlerListView > GameplayCueHandlerListView;

		
		void OnNewClassPicked(FString SelectedPath)
		{

		}
	};

	typedef STreeView< TSharedPtr< FTreeItem > > SGameplayCueTreeView;

	class SCueItemWidget : public SMultiColumnTableRow< TSharedPtr< FCueItem > >
	{
	public:
		SLATE_BEGIN_ARGS(SCueItemWidget) {}
		SLATE_END_ARGS()

			void Construct(const FArguments& InArgs, const TSharedRef<SGameplayCueTreeView>& InOwnerTable, FSimpleDelegate InRefreshDelegate, TSharedPtr<FCueItem> InListItem, FText InSearchTerm)
		{
			Item = InListItem;
			SearchTerm = InSearchTerm;
			RefreshDelegate = InRefreshDelegate;
			SMultiColumnTableRow< TSharedPtr< FCueItem > >::Construct(FSuperRowType::FArguments(), InOwnerTable);
		}
	private:

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
		{
			if (ColumnName == CueTagColumnName)
			{
				return
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SExpanderArrow, SharedThis(this))
					]
				+ SHorizontalBox::Slot()
					.FillWidth(1)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.HighlightText(SearchTerm)
					.Text(FText::FromString(Item->GameplayCueTagName.ToString()))
					];
			}
			else if (ColumnName == CueHandlerColumnName)
			{
				bool HasNotifies = false;
				for (auto& HandlerItem : Item->HandlerItems)
				{
					if (HandlerItem->GameplayCueNotifyObj.IsValid())
					{
						HasNotifies = true;
						break;
					}
				}

				return
					SNew(SBox)
					.Padding(FMargin(2.0f, 0.0f))
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.Visibility(HasNotifies ? EVisibility::Collapsed : EVisibility::Visible)
					[
						SNew(SButton)
						.Text(LOCTEXT("AddNew", "Add New"))
					.OnClicked(this, &SCueItemWidget::OnAddNewClicked)
					];
			}
			else
			{
				return SNew(STextBlock).Text(LOCTEXT("UnknownColumn", "Unknown Column"));
			}

		}

		/** Create new GameplayCueNotify: brings up dialogue to pick class, then creates it via the content browser. */
		FReply OnAddNewClicked()
		{
			SGameplayCueEditor::CreateNewGameplayCueNotifyDialogue(Item->GameplayCueTagName.ToString());
			RefreshDelegate.ExecuteIfBound();
			return FReply::Handled();
		}

		TSharedPtr< FCueItem > Item;
		FText SearchTerm;
		FSimpleDelegate RefreshDelegate;
	};

	class SCueHandlerItemWidget : public SMultiColumnTableRow < TSharedPtr< FCueHandlerItem > >
	{
	public:
		SLATE_BEGIN_ARGS(SCueHandlerItemWidget) {}
		SLATE_END_ARGS()

			void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable, TSharedPtr<FCueHandlerItem> InListItem)
		{
			CueHandlerItem = InListItem;
			SMultiColumnTableRow< TSharedPtr< FCueHandlerItem > >::Construct(FSuperRowType::FArguments(), InOwnerTable);
		}
	private:

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
		{
			if (ColumnName == CueTagColumnName)
			{
				return SNew(SSpacer);
			}
			else if (ColumnName == CueHandlerColumnName)
			{
				if (CueHandlerItem->GameplayCueNotifyObj.ToString().IsEmpty() == false)
				{
					FString ObjName = CueHandlerItem->GameplayCueNotifyObj.ToString();

					int32 idx;
					if (ObjName.FindLastChar(TEXT('.'), idx))
					{
						ObjName = ObjName.RightChop(idx + 1);
						if (ObjName.FindLastChar(TEXT('_'), idx))
						{
							ObjName = ObjName.Left(idx);
						}
					}

					return
						SNew(SBox)
						.HAlign(HAlign_Left)
						[
							SNew(SHyperlink)
							.Style(FEditorStyle::Get(), "Common.GotoBlueprintHyperlink")
						.Text(FText::FromString(ObjName))
						.OnNavigate(this, &SCueHandlerItemWidget::NavigateToHandler)
						];
				}
				else if (CueHandlerItem->FunctionPtr.IsValid())
				{
					FString ObjName;
					UFunction* Func = CueHandlerItem->FunctionPtr.Get();
					UClass* OuterClass = Cast<UClass>(Func->GetOuter());
					if (OuterClass)
					{
						ObjName = OuterClass->GetName();
						ObjName.RemoveFromEnd(TEXT("_c"));
					}

					return
						SNew(SBox)
						.HAlign(HAlign_Left)
						[
							SNew(SHyperlink)
							.Text(FText::FromString(ObjName))
						.OnNavigate(this, &SCueHandlerItemWidget::NavigateToHandler)
						];
				}
				else
				{
					return SNew(STextBlock).Text(LOCTEXT("UnknownHandler", "Unknown HandlerType"));
				}
			}
			else
			{
				return SNew(STextBlock).Text(LOCTEXT("UnknownColumn", "Unknown Column"));
			}
		}

		void NavigateToHandler()
		{
			if (CueHandlerItem->GameplayCueNotifyObj.IsValid())
			{
				SGameplayCueEditor::OpenEditorForNotify(CueHandlerItem->GameplayCueNotifyObj.ToString());

			}
			else if (CueHandlerItem->FunctionPtr.IsValid())
			{
				SGameplayCueEditor::OpenEditorForNotify(CueHandlerItem->FunctionPtr->GetOuter()->GetPathName());
			}
		}

		TSharedPtr<FCueHandlerItem> CueHandlerItem;
	};

	/** Builds widget for rows in the GameplayCue Editor tab */
	TSharedRef<ITableRow> OnGenerateWidgetForGameplayCueListView(TSharedPtr< FTreeItem > InItem, const TSharedRef<STableViewBase>& OwnerTable)
	{
		TSharedPtr<FCueItem> CueItem = InItem->AsCueItem();
		TSharedPtr<FCueHandlerItem> CueHandlerItem = InItem->AsCueHandlerItem();

		if ( CueItem.IsValid() )
		{
			return SNew(SCueItemWidget, GameplayCueTreeView.ToSharedRef(), FSimpleDelegate::CreateRaw(this, &SGameplayCueEditorImpl::UpdateGameplayCueListItems), CueItem, SearchText);
		}
		else if (CueHandlerItem.IsValid())
		{
			return SNew(SCueHandlerItemWidget, OwnerTable, CueHandlerItem);
		}
		else
		{
			return
			SNew(STableRow< TSharedPtr<FTreeItem> >, OwnerTable)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("UnknownItemType", "Unknown Item Type"))
			];
		}
	}

	void OnPropertyValueChanged()
	{
		UpdateGameplayCueListItems();
	}


	void OnGetChildren(TSharedPtr<FTreeItem> Item, TArray< TSharedPtr<FTreeItem> >& Children)
	{
		TSharedPtr<FCueItem> CueItem = Item->AsCueItem();
		if (CueItem.IsValid())
		{
			Children.Append(CueItem->HandlerItems);
		}
	}

	/** Builds content of the list in the GameplayCue Editor */
	void UpdateGameplayCueListItems()
	{
#if STATS
		FString PerfMessage = FString::Printf(TEXT("SGameplayCueEditor::UpdateGameplayCueListItems"));
		SCOPE_LOG_TIME_IN_SECONDS(*PerfMessage, nullptr)
#endif
		UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager();
		if (!CueManager)
		{
			return;
		}

		GameplayCueListItems.Reset();
		IGameplayTagsModule& GameplayTagModule = IGameplayTagsModule::Get();
		
		TSharedPtr<FCueItem> SelectItem;

		FString SearchString = SearchText.ToString();

		CueManager->LoadAllGameplayCueNotifiesForEditor();

		FGameplayTagContainer AllGameplayCueTags = IGameplayTagsModule::Get().GetGameplayTagsManager().RequestGameplayTagChildren(UGameplayCueSet::BaseGameplayCueTag());
		for (FGameplayTag ThisGameplayCueTag : AllGameplayCueTags)
		{
			if (SearchString.IsEmpty() == false && ThisGameplayCueTag.ToString().Contains(SearchString) == false)
			{
				continue;
			}

			bool Handled = false;

			TSharedRef< FCueItem > NewItem(new FCueItem());
			NewItem->GameplayCueTagName = ThisGameplayCueTag;

			if (AutoSelectTag.IsEmpty() == false && AutoSelectTag == ThisGameplayCueTag.ToString())
			{
				SelectItem = NewItem;
			}

			// Add notifies from the global set
			if (CueManager && CueManager->GlobalCueSet)
			{
				int32* idxPtr = CueManager->GlobalCueSet->GameplayCueDataMap.Find(ThisGameplayCueTag);
				if (idxPtr)
				{
					int32 idx = *idxPtr;
					if (CueManager->GlobalCueSet->GameplayCueData.IsValidIndex(idx))
					{
						FGameplayCueNotifyData& Data = CueManager->GlobalCueSet->GameplayCueData[*idxPtr];

						TSharedRef< FCueHandlerItem > NewHandlerItem(new FCueHandlerItem());
						NewHandlerItem->GameplayCueNotifyObj = Data.GameplayCueNotifyObj;
						NewItem->HandlerItems.Add(NewHandlerItem);
						Handled = true;
					}
				}
			}

			// Add events implemented by IGameplayCueInterface blueprints
			TArray<UFunction*> Funcs;
			EventMap.MultiFind(ThisGameplayCueTag, Funcs);

			for (UFunction* Func : Funcs)
			{
				TSharedRef< FCueHandlerItem > NewHandlerItem(new FCueHandlerItem());
				NewHandlerItem->FunctionPtr = Func;
				NewItem->HandlerItems.Add(NewHandlerItem);
				Handled = true;
			}

			if (bShowAll || Handled)
			{
				GameplayCueListItems.Add(NewItem);
			}

			// If there are any handlers, automatically expand the item
			if (NewItem->HandlerItems.Num() > 0)
			{
				GameplayCueTreeView->SetItemExpansion(NewItem, true);
			}

			// Todo: Add notifies from all GameplayCueSets?
		}


		if (GameplayCueTreeView.IsValid())
		{
			GameplayCueTreeView->RequestTreeRefresh();
		}

		if (SelectItem.IsValid())
		{
			GameplayCueTreeView->SetItemSelection(SelectItem, true);
			GameplayCueTreeView->RequestScrollIntoView(SelectItem);

		}
		AutoSelectTag.Empty();
	}

	/** Slow task: use asset registry to find blueprints, load an inspect them to see what GC events they implement. */
	FReply BuildEventMap()
	{
		FScopedSlowTask SlowTask(100.f, LOCTEXT("BuildEventMap", "Searching Blueprints for GameplayCue events"));
		SlowTask.MakeDialog();
		SlowTask.EnterProgressFrame(10.f);

		EventMap.Empty();

		IGameplayTagsModule& GameplayTagModule = IGameplayTagsModule::Get();

		auto del = IGameplayAbilitiesEditorModule::Get().GetGameplayCueInterfaceClassesDelegate();
		if (del.IsBound())
		{
			TArray<UClass*> InterfaceClasses;
			del.ExecuteIfBound(InterfaceClasses);
			float WorkPerClass = InterfaceClasses.Num() > 0 ? 90.f / static_cast<float>(InterfaceClasses.Num()) : 0.f;

			for (UClass* InterfaceClass : InterfaceClasses)
			{
				SlowTask.EnterProgressFrame(WorkPerClass);

				TArray<FAssetData> GameplayCueInterfaceActors;
				{
#if STATS
					FString PerfMessage = FString::Printf(TEXT("Searched asset registry %s "), *InterfaceClass->GetName());
					SCOPE_LOG_TIME_IN_SECONDS(*PerfMessage, nullptr)
#endif

						UObjectLibrary* ObjLibrary = UObjectLibrary::CreateLibrary(InterfaceClass, true, true);
					ObjLibrary->LoadBlueprintAssetDataFromPath(TEXT("/Game/"));
					ObjLibrary->GetAssetDataList(GameplayCueInterfaceActors);
				}

			{
#if STATS
				FString PerfMessage = FString::Printf(TEXT("Fully Loaded GameplayCueNotify actors %s "), *InterfaceClass->GetName());
				SCOPE_LOG_TIME_IN_SECONDS(*PerfMessage, nullptr)
#endif				

					for (const FAssetData& AssetData : GameplayCueInterfaceActors)
					{
						UBlueprint* BP = Cast<UBlueprint>(AssetData.GetAsset());
						if (BP)
						{
							for (TFieldIterator<UFunction> FuncIt(BP->GeneratedClass, EFieldIteratorFlags::ExcludeSuper); FuncIt; ++FuncIt)
							{
								FString FuncName = FuncIt->GetName();
								if (FuncName.Contains("GameplayCue"))
								{
									FuncName.ReplaceInline(TEXT("_"), TEXT("."));
									FGameplayTag FoundTag = GameplayTagModule.RequestGameplayTag(FName(*FuncName), false);
									if (FoundTag.IsValid())
									{
										EventMap.AddUnique(FoundTag, *FuncIt);
									}
								}

							}
						}
					}
				}
			}

			UpdateGameplayCueListItems();
		}

		return FReply::Handled();
	}

	bool bShowAll;

	TSharedPtr< SGameplayCueTreeView > GameplayCueTreeView;
	TArray< TSharedPtr< FTreeItem > > GameplayCueListItems;

	TMultiMap<FGameplayTag, UFunction*> EventMap;

	FString AutoSelectTag;
	FText SearchText;
};

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SGameplayCueEditorImpl::Construct(const FArguments& InArgs)
{
	UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager();
	if (CueManager)
	{
		CueManager->OnGameplayCueNotifyAddOrRemove.AddSP(this, &SGameplayCueEditorImpl::OnPropertyValueChanged);
	}

	bShowAll = true;
	bool CanAddFromINI = UGameplayTagsManager::ShouldImportTagsFromINI(); // We only support adding new tags to the ini files.
	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(2.0f, 2.0f)
			.AutoWidth()
			[

				SNew(SButton)
				.Text(LOCTEXT("SearchBPEvents", "Search BP Events"))
				.OnClicked(this, &SGameplayCueEditorImpl::BuildEventMap)
			]

			+ SHorizontalBox::Slot()
			.Padding(2.0f, 2.0f)
			.AutoWidth()
			[
				// Check box that controls LIVE MODE
				SNew(SCheckBox)
				.IsChecked(this, &SGameplayCueEditorImpl::HandleShowAllCheckBoxIsChecked)
				.OnCheckStateChanged(this, &SGameplayCueEditorImpl::HandleShowAllCheckedStateChanged)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("HideUnhandled", "Hide Unhandled GameplayCues"))
				]
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(2.0f, 2.0f)
			.AutoWidth()
			[
				SAssignNew(NewGameplayCueTextBox, SEditableTextBox)
				.MinDesiredWidth(210.0f)
				.HintText(LOCTEXT("GameplayCueXY", "GameplayCue.X.Y"))
				.OnTextCommitted(this, &SGameplayCueEditorImpl::OnNewGameplayCueTagCommited)
			]

			+ SHorizontalBox::Slot()
			.Padding(2.0f, 2.0f)
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("AddNew", "Add New"))
				.OnClicked(this, &SGameplayCueEditorImpl::OnNewGameplayCueButtonPressed)
				.Visibility(CanAddFromINI ? EVisibility::Visible : EVisibility::Collapsed)
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(2.0f)
			.AutoWidth()
			[
				SNew(SSearchBox)
				.MinDesiredWidth(210.0f)
				.OnTextCommitted(this, &SGameplayCueEditorImpl::OnSearchTagCommited)
			]
			+ SHorizontalBox::Slot()
			.Padding(2.0f)
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("Search", "Search"))
				.OnClicked(this, &SGameplayCueEditorImpl::DoSearch)
			]
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolBar.Background"))
			[
				SAssignNew(GameplayCueTreeView, SGameplayCueTreeView)
				.ItemHeight(24)
				.TreeItemsSource(&GameplayCueListItems)
				.OnGenerateRow(this, &SGameplayCueEditorImpl::OnGenerateWidgetForGameplayCueListView)
				.OnGetChildren( this, &SGameplayCueEditorImpl::OnGetChildren )
				.HeaderRow
				(
					SNew(SHeaderRow)
					+ SHeaderRow::Column(CueTagColumnName)
					.DefaultLabel(NSLOCTEXT("GameplayCueEditor", "GameplayCueTag", "Tag"))
					.FillWidth(0.25)

					+ SHeaderRow::Column(CueHandlerColumnName)
					.DefaultLabel(NSLOCTEXT("GameplayCueEditor", "GameplayCueHandlers", "Handlers"))
					//.FillWidth(1000)
				)
			]
		]
	];

	UpdateGameplayCueListItems();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

TSharedRef<SGameplayCueEditor> SGameplayCueEditor::New()
{
	return MakeShareable(new SGameplayCueEditorImpl());
}

void SGameplayCueEditor::CreateNewGameplayCueNotifyDialogue(FString GameplayCue)
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	TArray<UClass*>	NotifyClasses;
	auto del = IGameplayAbilitiesEditorModule::Get().GetGameplayCueNotifyClassesDelegate();
	del.ExecuteIfBound(NotifyClasses);
	if (NotifyClasses.Num() == 0)
	{
		NotifyClasses.Add(UGameplayCueNotify_Static::StaticClass());
		NotifyClasses.Add(AGameplayCueNotify_Actor::StaticClass());
	}

	// --------------------------------------------------

	// Null the parent class to ensure one is selected	

	const FText TitleText = LOCTEXT("CreateBlueprintOptions", "New GameplayCue Handler");
	UClass* ChosenClass = nullptr;
	const bool bPressedOk = SGameplayCuePickerDialog::PickGameplayCue(TitleText, NotifyClasses, ChosenClass, GameplayCue);
	if (bPressedOk)
	{
		FString NewDefaultPathName = SGameplayCueEditor::GetPathNameForGameplayCueTag(GameplayCue);

		// Make sure the name is unique
		FString AssetName;
		FString PackageName;
		AssetToolsModule.Get().CreateUniqueAssetName(NewDefaultPathName, TEXT(""), /*out*/ PackageName, /*out*/ AssetName);
		const FString PackagePath = FPackageName::GetLongPackagePath(PackageName);

		// Create the GameplayCue Notify
		UBlueprintFactory* BlueprintFactory = NewObject<UBlueprintFactory>();
		BlueprintFactory->ParentClass = ChosenClass;
		ContentBrowserModule.Get().CreateNewAsset(AssetName, PackagePath, UBlueprint::StaticClass(), BlueprintFactory);
	}
}

void SGameplayCueEditor::OpenEditorForNotify(FString NotifyFullPath)
{
	// This nonsense is to handle the case where the asset only exists in memory
	// and there for does not have a linker/exist on disk. (The FString version
	// of OpenEditorForAsset does not handle this).
	FStringAssetReference AssetRef(NotifyFullPath);

	UObject* Obj = AssetRef.ResolveObject();
	if (!Obj)
	{
		Obj = AssetRef.TryLoad();
	}

	if (Obj)
	{
		UPackage* pkg = Cast<UPackage>(Obj->GetOuter());
		if (pkg)
		{
			FString AssetName = FPaths::GetBaseFilename(AssetRef.ToString());
			UObject* AssetObject = FindObject<UObject>(pkg, *AssetName);
			FAssetEditorManager::Get().OpenEditorForAsset(AssetObject);
		}
	}
}

#undef LOCTEXT_NAMESPACE
