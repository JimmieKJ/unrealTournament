// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SourceControlWindowsPCH.h"
#include "AssetToolsModule.h"
#include "MessageLog.h"
#include "SNotificationList.h"
#include "NotificationManager.h"
#include "FileHelpers.h"


IMPLEMENT_MODULE( FDefaultModuleImpl, SourceControlWindows );

#if SOURCE_CONTROL_WITH_SLATE

#define LOCTEXT_NAMESPACE "SSourceControlSubmit"

//-------------------------------------
//Source Control Window Constants
//-------------------------------------
namespace ESubmitResults
{
	enum Type
	{
		SUBMIT_ACCEPTED,
		SUBMIT_CANCELED
	};
}

//-----------------------------------------------
//Source Control Check in Helper Struct
//-----------------------------------------------
class FChangeListDescription
{
public:
	TArray<FString> FilesForAdd;
	TArray<FString> FilesForSubmit;
	FText Description;
};

/** Holds the display string and check state for items being submitted. */
struct TSubmitItemData
{
	/** The display text for the item. */
	TSharedPtr< FText > DisplayText;

	/** Whether or not the item is checked. */
	bool bIsChecked;

	TSubmitItemData(TSharedPtr< FText > InListItemPtr)
	{
		DisplayText = InListItemPtr;
		bIsChecked = true;
	}
};

/** The item that appears in the list of items for submitting. */
class SSubmitItem : public STableRow< TSharedPtr<FText> >
{
public:
	SLATE_BEGIN_ARGS( SSubmitItem )
		: _SubmitItemData()
	{}
		/** The item data to be displayed. */
		SLATE_ARGUMENT( TSharedPtr< TSubmitItemData >, SubmitItemData )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView )
	{
		SubmitItemData = InArgs._SubmitItemData;

		STableRow< TSharedPtr<FText> >::Construct(
			STableRow< TSharedPtr<FText> >::FArguments()
			[
				SNew(SCheckBox)
				.IsChecked(this, &SSubmitItem::IsChecked)
				.OnCheckStateChanged(this, &SSubmitItem::OnCheckStateChanged)
				[
					SNew(STextBlock)				
					.Text(*SubmitItemData->DisplayText)
				]
			]
			,InOwnerTableView
		);
	}

private:
	/** The check status of the item. */
	ECheckBoxState IsChecked() const
	{
		return SubmitItemData->bIsChecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	/** Changes the check status of the item .*/
	void OnCheckStateChanged(ECheckBoxState InState)
	{
		SubmitItemData->bIsChecked = InState == ECheckBoxState::Checked;
	}

private:
	/** The item data associated with this list item. */
	TSharedPtr< TSubmitItemData > SubmitItemData;
};

class SSourceControlSubmitWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SSourceControlSubmitWidget )
		: _ParentWindow()
		, _InAddFiles() 
		, _InOpenFiles()
	{}

		SLATE_ATTRIBUTE( TSharedPtr<SWindow>, ParentWindow )
		SLATE_ATTRIBUTE( TArray<FString>, InAddFiles)
		SLATE_ATTRIBUTE( TArray<FString>, InOpenFiles)

	SLATE_END_ARGS()

	SSourceControlSubmitWidget()
	{
	}

	void Construct( const FArguments& InArgs )
	{
		ParentFrame = InArgs._ParentWindow.Get();
		AddFiles(InArgs._InAddFiles.Get(), InArgs._InOpenFiles.Get());

		ChildSlot
		[
			
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(5)
				[
					SNew( STextBlock )
					.Text( NSLOCTEXT("SourceControl.SubmitPanel", "ChangeListDesc", "Changelist Description") )
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(5, 0, 5, 5))
				[
					SNew(SBox)
					.WidthOverride(520)
					[
						SAssignNew( ChangeListDescriptionTextCtrl, SMultiLineEditableTextBox )
						.SelectAllTextWhenFocused( true )
						.AutoWrapText( true )
					]
				]
				+SVerticalBox::Slot()
				.Padding(FMargin(5, 0))
				[
					SNew(SBorder)
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SBorder)
							.BorderImage(FEditorStyle::GetBrush(TEXT("PackageDialog.ListHeader")))
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.HAlign(HAlign_Center)
								[
									SNew(SCheckBox)
									.IsChecked(this, &SSourceControlSubmitWidget::GetToggleSelectedState)
									.OnCheckStateChanged(this, &SSourceControlSubmitWidget::OnToggleSelectedCheckBox)
								]
								+SHorizontalBox::Slot()
								.VAlign(VAlign_Center)
								.HAlign(HAlign_Left)
								.FillWidth(1)
								[
									SNew(STextBlock) 
									.Text(LOCTEXT("File", "File"))
								]
							]
						]
						+SVerticalBox::Slot()
						.Padding(2)
						[
							SNew(SBox)
							[
								SAssignNew( ListView, SListType)
								.ItemHeight(24)
								.ListItemsSource( &ListViewItems )
								.OnGenerateRow( this, &SSourceControlSubmitWidget::OnGenerateRowForList )
							]
						]
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(5, 5, 5, 0))
				[
					SNew( SBorder)
					.Visibility(this, &SSourceControlSubmitWidget::IsWarningPanelVisible)
					.Padding(5)
					[
						SNew( SErrorText )
						.ErrorText( NSLOCTEXT("SourceControl.SubmitPanel", "ChangeListDescWarning", "Changelist description is required to submit") )
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(5)
				[
					SNew(SWrapBox)
					.UseAllottedWidth(true)
					+SWrapBox::Slot()
					.Padding(0.0f, 0.0f, 16.0f, 0.0f)
					[
						SNew(SCheckBox)
						.OnCheckStateChanged( this, &SSourceControlSubmitWidget::OnCheckStateChanged_KeepCheckedOut)
						.IsChecked( this, &SSourceControlSubmitWidget::GetKeepCheckedOut )
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("SourceControl.SubmitPanel", "KeepCheckedOut", "Keep Files Checked Out") )
						]
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Bottom)
				.Padding(0.0f,0.0f,0.0f,5.0f)
				[
					SNew(SUniformGridPanel)
					.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
					.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
					.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
					+SUniformGridPanel::Slot(0,0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
						.IsEnabled(this, &SSourceControlSubmitWidget::IsOKEnabled)
						.Text( NSLOCTEXT("SourceControl.SubmitPanel", "OKButton", "OK") )
						.OnClicked(this, &SSourceControlSubmitWidget::OKClicked)
					]
					+SUniformGridPanel::Slot(1,0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
						.Text( NSLOCTEXT("SourceControl.SubmitPanel", "CancelButton", "Cancel") )
						.OnClicked(this, &SSourceControlSubmitWidget::CancelClicked)
					]
				]
			]
		]
		;

		DialogResult = ESubmitResults::SUBMIT_CANCELED;
		KeepCheckedOut = ECheckBoxState::Unchecked;

		ParentFrame.Pin()->SetWidgetToFocusOnActivate(ChangeListDescriptionTextCtrl);
	}


public:
	ESubmitResults::Type GetResult() {return DialogResult;}


	void AddFiles(const TArray<FString>& InAddFiles, const TArray<FString>& InOpenFiles)
	{
		for (int32 i = 0; i < InAddFiles.Num(); ++i)
		{
			AddFileToListView (InAddFiles[i]);
		}
		for (int32 i = 0; i < InOpenFiles.Num(); ++i)
		{
			AddFileToListView (InOpenFiles[i]);
		}
	}

private:
	ESubmitResults::Type DialogResult;

	typedef SListView< TSharedPtr< TSubmitItemData > > SListType;
	/** ListBox for selecting which object to consolidate */
	TSharedPtr< SListType > ListView;
	/** Collection of objects (Widgets) to display in the List View. */
	TArray< TSharedPtr< TSubmitItemData > > ListViewItems;

	/** 
	 * Get all the selected items in the dialog
	 * 
	 * @param bAllIfNone - if true, returns all the items when none are selected
	 * @return the array of items to be altered by OnToggleSelectedCheckBox
	 */
	TArray< TSharedPtr<TSubmitItemData> > GetSelectedItems( const bool bAllIfNone ) const
	{
		// Get the list of highlighted items
		TArray< TSharedPtr<TSubmitItemData> > SelectedItems = ListView->GetSelectedItems();
		if(SelectedItems.Num() == 0)
		{
			// If no items are explicitly highlighted, return all items in the list.
			SelectedItems = ListViewItems;
		}

		return SelectedItems;
	}

	/** 
	 * @return the desired toggle state for the ToggleSelectedCheckBox.
	 * Returns Unchecked, unless all of the selected items are Checked.
	 */
	ECheckBoxState GetToggleSelectedState() const
	{
		// Default to a Checked state
		ECheckBoxState PendingState = ECheckBoxState::Checked;

		TArray< TSharedPtr<TSubmitItemData> > SelectedItems = GetSelectedItems(true);

		// Iterate through the list of selected items
		for(auto SelectedItem = SelectedItems.CreateConstIterator(); SelectedItem; ++SelectedItem)
		{
			TSubmitItemData* const Item = SelectedItem->Get();
			if(!Item->bIsChecked)
			{
				// If any item in the selection is Unchecked, then represent the entire set of highlighted items as Unchecked,
				// so that the first (user) toggle of ToggleSelectedCheckBox consistently Checks all highlighted items
				PendingState = ECheckBoxState::Unchecked;
			}
		}

		return PendingState;
	}

	/** 
	 * Toggles the highlighted items.
	 * If no items are explicitly highlighted, toggles all items in the list.
	 */
	void OnToggleSelectedCheckBox(ECheckBoxState InNewState)
	{
		TArray< TSharedPtr<TSubmitItemData> > SelectedItems = GetSelectedItems(true);

		const bool bIsChecked = (InNewState == ECheckBoxState::Checked);
		for(auto SelectedItem = SelectedItems.CreateConstIterator(); SelectedItem; ++SelectedItem)
		{
			TSubmitItemData* const Item = SelectedItem->Get();
			Item->bIsChecked = bIsChecked;
		}

		ListView->RequestListRefresh();
	}

public:

	/**Gets the requested files and the change list description*/
	void FillChangeListDescription (FChangeListDescription& OutDesc, TArray<FString>& InAddFiles, TArray<FString>& InOpenFiles)
	{
		OutDesc.Description = ChangeListDescriptionTextCtrl->GetText();

		check(ListViewItems.Num() == InAddFiles.Num() + InOpenFiles.Num());

		for (int32 i = 0; i < InAddFiles.Num(); ++i)
		{
			if (ListViewItems[i]->bIsChecked)
			{
				OutDesc.FilesForAdd.Add(InAddFiles[i]);
			}
		}
		for (int32 i = 0; i < InOpenFiles.Num(); ++i)
		{
			if (ListViewItems[i + InAddFiles.Num()]->bIsChecked)
			{
				OutDesc.FilesForSubmit.Add(InOpenFiles[i]);
			}
		}
	}

	/** Does the user want to keep the files checked out */
	bool WantToKeepCheckedOut()
	{
		return KeepCheckedOut == ECheckBoxState::Checked ? true : false;
	}

private:

	/** Called when the settings of the dialog are to be accepted*/
	FReply OKClicked()
	{
		DialogResult = ESubmitResults::SUBMIT_ACCEPTED;
		ParentFrame.Pin()->RequestDestroyWindow();

		return FReply::Handled();
	}

	/** Called when the settings of the dialog are to be ignored*/
	FReply CancelClicked()
	{
		DialogResult = ESubmitResults::SUBMIT_CANCELED;
		ParentFrame.Pin()->RequestDestroyWindow();

		return FReply::Handled();
	}

	/** Called to check if the OK button is enabled or not. */
	bool IsOKEnabled() const
	{
		return !ChangeListDescriptionTextCtrl->GetText().IsEmpty();
	}

	/** Check if the warning panel should be visible. */
	EVisibility IsWarningPanelVisible() const
	{
		return IsOKEnabled()? EVisibility::Hidden : EVisibility::Visible;
	}

	/** Called when the Keep checked out Checkbox is changed */
	void OnCheckStateChanged_KeepCheckedOut(ECheckBoxState InState)
	{
		KeepCheckedOut = InState;
	}

	/** Get the current state of the Keep Checked Out checkbox  */
	ECheckBoxState GetKeepCheckedOut() const
	{
		return KeepCheckedOut;
	}

	/**Helper function to append a check box and label to the list view*/
	void AddFileToListView (const FString& InFileName)
	{
		ListViewItems.Add( MakeShareable( new TSubmitItemData( MakeShareable(new FText(FText::FromString(InFileName)) ) ) ) );
	}

	TSharedRef<ITableRow> OnGenerateRowForList( TSharedPtr< TSubmitItemData > SubmitItemData, const TSharedRef<STableViewBase>& OwnerTable )
	{
		TSharedRef<ITableRow> Row =
			SNew( SSubmitItem, OwnerTable )
				.SubmitItemData(SubmitItemData);

		return Row;
	}

private:
	TWeakPtr<SWindow> ParentFrame;

	/** Internal widgets to save having to get in multiple places*/
	TSharedPtr<SMultiLineEditableTextBox> ChangeListDescriptionTextCtrl;

	ECheckBoxState	KeepCheckedOut;
};


TWeakPtr<SNotificationItem> FSourceControlWindows::ChoosePackagesToCheckInNotification;

void FSourceControlWindows::ChoosePackagesToCheckInCompleted(const TArray<UPackage*>& LoadedPackages, const TArray<FString>& PackageNames, const TArray<FString>& ConfigFiles)
{
	if (ChoosePackagesToCheckInNotification.IsValid())
	{
		ChoosePackagesToCheckInNotification.Pin()->ExpireAndFadeout();
	}
	ChoosePackagesToCheckInNotification.Reset();

	check(PackageNames.Num() > 0 || ConfigFiles.Num() > 0);

	// Prompt the user to ask if they would like to first save any dirty packages they are trying to check-in
	const FEditorFileUtils::EPromptReturnCode UserResponse = FEditorFileUtils::PromptForCheckoutAndSave(LoadedPackages, true, true);

	// If the user elected to save dirty packages, but one or more of the packages failed to save properly OR if the user
	// canceled out of the prompt, don't follow through on the check-in process
	const bool bShouldProceed = (UserResponse == FEditorFileUtils::EPromptReturnCode::PR_Success || UserResponse == FEditorFileUtils::EPromptReturnCode::PR_Declined);
	if (bShouldProceed)
	{
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		FSourceControlWindows::PromptForCheckin(PackageNames, ConfigFiles);
	}
	else
	{
		// If a failure occurred, alert the user that the check-in was aborted. This warning shouldn't be necessary if the user cancelled
		// from the dialog, because they obviously intended to cancel the whole operation.
		if (UserResponse == FEditorFileUtils::EPromptReturnCode::PR_Failure)
		{
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "SCC_Checkin_Aborted", "Check-in aborted as a result of save failure."));
		}
	}
}

void FSourceControlWindows::ChoosePackagesToCheckInCancelled(FSourceControlOperationRef InOperation)
{
	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	SourceControlProvider.CancelOperation(InOperation);

	if (ChoosePackagesToCheckInNotification.IsValid())
	{
		ChoosePackagesToCheckInNotification.Pin()->ExpireAndFadeout();
	}
	ChoosePackagesToCheckInNotification.Reset();
}

void FSourceControlWindows::ChoosePackagesToCheckInCallback(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult)
{
	if (ChoosePackagesToCheckInNotification.IsValid())
	{
		ChoosePackagesToCheckInNotification.Pin()->ExpireAndFadeout();
	}
	ChoosePackagesToCheckInNotification.Reset();

	if (InResult == ECommandResult::Succeeded)
	{
		// Get a list of all the checked out packages
		int32 NumSelectedNames = 0;
		int32 NumSelectedPackages = 0;
		TArray<FString> PackageNames;
		TArray<UPackage*> LoadedPackages;
		TMap<FString, FSourceControlStatePtr> PackageStates;
		FEditorFileUtils::FindAllSubmittablePackageFiles(PackageStates, true);
		for (TMap<FString, FSourceControlStatePtr>::TConstIterator PackageIter(PackageStates); PackageIter; ++PackageIter)
		{
			const FString Filename = *PackageIter.Key();
			const FString PackageName = FPackageName::FilenameToLongPackageName(Filename);
			const FSourceControlStatePtr CurPackageSCCState = PackageIter.Value();

			UPackage* Package = FindPackage(NULL, *PackageName);

			// Put pre-selected items at the start of the list
			if (CurPackageSCCState.IsValid() && CurPackageSCCState->IsSourceControlled())
			{
				if (Package != NULL)
				{
					LoadedPackages.Insert(Package, NumSelectedPackages++);
				}
				PackageNames.Insert(PackageName, NumSelectedNames++);
			}
			else
			{
				if (Package != NULL)
				{
					LoadedPackages.Add(Package);
				}
				PackageNames.Add(PackageName);
			}
		}

		// Get a list of all the checked out config files
		TMap<FString, FSourceControlStatePtr> ConfigFileStates;
		TArray<FString> ConfigFilesToSubmit;
		FEditorFileUtils::FindAllSubmittableConfigFiles(ConfigFileStates);
		for (TMap<FString, FSourceControlStatePtr>::TConstIterator It(ConfigFileStates); It; ++It)
		{
			ConfigFilesToSubmit.Add(It.Key());
		}

		if (PackageNames.Num() > 0 || ConfigFilesToSubmit.Num() > 0)
		{
			ChoosePackagesToCheckInCompleted(LoadedPackages, PackageNames, ConfigFilesToSubmit);
		}
		else
		{
			FMessageLog EditorErrors("EditorErrors");
			EditorErrors.Warning(LOCTEXT("NoAssetsToCheckIn", "No assets to check in!"));
			EditorErrors.Notify();
		}
	}
	else if (InResult == ECommandResult::Failed)
	{
		FMessageLog EditorErrors("EditorErrors");
		EditorErrors.Warning(LOCTEXT("CheckInOperationFailed", "Failed checking source control status!"));
		EditorErrors.Notify();
	}
}

void FSourceControlWindows::ChoosePackagesToCheckIn()
{
	if (ISourceControlModule::Get().IsEnabled())
	{
		if (ISourceControlModule::Get().GetProvider().IsAvailable())
		{
			// make sure we update the SCC status of all packages (this could take a long time, so we will run it as a background task)
			TArray<FString> Packages;
			FEditorFileUtils::FindAllPackageFiles(Packages);

			// Get list of filenames corresponding to packages
			TArray<FString> Filenames = SourceControlHelpers::PackageFilenames(Packages);

			// Add game config files to the list
			FEditorFileUtils::FindAllConfigFiles(Filenames);

			ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
			FSourceControlOperationRef Operation = ISourceControlOperation::Create<FUpdateStatus>();
			SourceControlProvider.Execute(Operation, Filenames, EConcurrency::Asynchronous, FSourceControlOperationComplete::CreateStatic(&FSourceControlWindows::ChoosePackagesToCheckInCallback));

			if (ChoosePackagesToCheckInNotification.IsValid())
			{
				ChoosePackagesToCheckInNotification.Pin()->ExpireAndFadeout();
			}

			FNotificationInfo Info(LOCTEXT("ChooseAssetsToCheckInIndicator", "Checking for assets to check in..."));
			Info.bFireAndForget = false;
			Info.ExpireDuration = 0.0f;
			Info.FadeOutDuration = 1.0f;

			if (SourceControlProvider.CanCancelOperation(Operation))
			{
				Info.ButtonDetails.Add(FNotificationButtonInfo(
					LOCTEXT("ChoosePackagesToCheckIn_CancelButton", "Cancel"),
					LOCTEXT("ChoosePackagesToCheckIn_CancelButtonTooltip", "Cancel the check in operation."),
					FSimpleDelegate::CreateStatic(&FSourceControlWindows::ChoosePackagesToCheckInCancelled, Operation)
					));
			}

			ChoosePackagesToCheckInNotification = FSlateNotificationManager::Get().AddNotification(Info);

			if (ChoosePackagesToCheckInNotification.IsValid())
			{
				ChoosePackagesToCheckInNotification.Pin()->SetCompletionState(SNotificationItem::CS_Pending);
			}
		}
		else
		{
			FMessageLog EditorErrors("EditorErrors");
			EditorErrors.Warning(LOCTEXT("NoSCCConnection", "No connection to source control available!"))
				->AddToken(FDocumentationToken::Create(TEXT("Engine/UI/SourceControl")));
			EditorErrors.Notify();
		}
	}
}

bool FSourceControlWindows::CanChoosePackagesToCheckIn()
{
	return !ChoosePackagesToCheckInNotification.IsValid();
}

static void FindFilesForCheckIn(const TArray<FString>& InFilenames, TArray<FString>& OutAddFiles, TArray<FString>& OutOpenFiles)
{
	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();

	TArray<FSourceControlStateRef> States;
	SourceControlProvider.GetState(InFilenames, States, EStateCacheUsage::ForceUpdate);

	for (const FString& Filename : InFilenames)
	{
		FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(Filename, EStateCacheUsage::Use);
		if (SourceControlState.IsValid())
		{
			if (SourceControlState->CanCheckIn())
			{
				OutOpenFiles.Add(Filename);
			}
			else
			{
				if (!SourceControlState->IsSourceControlled())
				{
					OutAddFiles.Add(Filename);
				}
			}
		}
	}
}

bool FSourceControlWindows::PromptForCheckin(const TArray<FString>& InPackageNames, const TArray<FString>& InConfigFiles)
{
	bool bCheckInSuccess = true;

	// Get filenames for package names
	TArray<FString> AllFiles = SourceControlHelpers::PackageFilenames(InPackageNames);
	AllFiles.Append(InConfigFiles);

	TArray<FString> AddFiles;
	TArray<FString> OpenFiles;
	FindFilesForCheckIn(AllFiles, AddFiles, OpenFiles);

	if (AddFiles.Num() || OpenFiles.Num())
	{
		TSharedRef<SWindow> NewWindow = SNew(SWindow)
			.Title(NSLOCTEXT("SourceControl.SubmitWindow", "Title", "Submit Files"))
			.SizingRule(ESizingRule::UserSized)
			.ClientSize(FVector2D(512, 430))
			.SupportsMaximize(true)
			.SupportsMinimize(false);

		TSharedRef<SSourceControlSubmitWidget> SourceControlWidget = 
			SNew(SSourceControlSubmitWidget)
			.ParentWindow(NewWindow)
			.InAddFiles(AddFiles)
			.InOpenFiles(OpenFiles);

		NewWindow->SetContent(
			SourceControlWidget
			);

		FSlateApplication::Get().AddModalWindow(NewWindow, NULL);

		if (SourceControlWidget->GetResult() == ESubmitResults::SUBMIT_ACCEPTED)
		{
			ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();

			//try the actual submission
			FChangeListDescription Description;

			//Get description from the dialog
			SourceControlWidget->FillChangeListDescription(Description, AddFiles, OpenFiles);

			//revert all unchanged files that were submitted
			if ( Description.FilesForSubmit.Num() > 0 )
			{
				SourceControlHelpers::RevertUnchangedFiles(SourceControlProvider, Description.FilesForSubmit);

				//make sure all files are still checked out
				for (int32 VerifyIndex = Description.FilesForSubmit.Num()-1; VerifyIndex >= 0; --VerifyIndex)
				{
					FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(Description.FilesForSubmit[VerifyIndex], EStateCacheUsage::Use);
					if( SourceControlState.IsValid() && !SourceControlState->IsCheckedOut() && !SourceControlState->IsAdded() )
					{
						Description.FilesForSubmit.RemoveAt(VerifyIndex);
					}
				}
			}

			TArray<FString> CombinedFileList = Description.FilesForAdd;
			CombinedFileList.Append(Description.FilesForSubmit);

			if(Description.FilesForAdd.Num() > 0)
			{
				bCheckInSuccess &= (SourceControlProvider.Execute(ISourceControlOperation::Create<FMarkForAdd>(), Description.FilesForAdd) == ECommandResult::Succeeded);
			}

			if(CombinedFileList.Num() > 0)
			{
				TSharedRef<FCheckIn, ESPMode::ThreadSafe> CheckInOperation = ISourceControlOperation::Create<FCheckIn>();
				CheckInOperation->SetDescription(Description.Description);
				bCheckInSuccess &= (SourceControlProvider.Execute(CheckInOperation, CombinedFileList) == ECommandResult::Succeeded);

				if(bCheckInSuccess)
				{
					// report success with a notification
					FNotificationInfo Info(CheckInOperation->GetSuccessMessage());
					Info.ExpireDuration = 8.0f;
					Info.HyperlinkText = LOCTEXT("SCC_Checkin_ShowLog", "Show Message Log");
					Info.Hyperlink = FSimpleDelegate::CreateStatic([](){ FMessageLog("SourceControl").Open(EMessageSeverity::Info, true); });
					FSlateNotificationManager::Get().AddNotification(Info);

					// also add to the log
					FMessageLog("SourceControl").Info(CheckInOperation->GetSuccessMessage());
				}
			}

			if(!bCheckInSuccess)
			{
				FMessageLog("SourceControl").Notify(LOCTEXT("SCC_Checkin_Failed", "Failed to check in files!"));
			}
			// If we checked in ok, do we want to to re-check out the files we just checked in?
			else
			{
				if( SourceControlWidget->WantToKeepCheckedOut() == true )
				{
					// Re-check out files
					if(SourceControlProvider.Execute(ISourceControlOperation::Create<FCheckOut>(), CombinedFileList) != ECommandResult::Succeeded)
					{
						FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT("SCC_Checkin_ReCheckOutFailed", "Failed to re-check out files.") );
					}
				}
			}
		}
	}

	return bCheckInSuccess;
}


#undef LOCTEXT_NAMESPACE

#endif // SOURCE_CONTROL_WITH_SLATE
