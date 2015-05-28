// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "PersonaPrivatePCH.h"

#include "Persona.h"
#include "SSkeletonAnimNotifies.h"
#include "AssetRegistryModule.h"
#include "ScopedTransaction.h"
#include "SSearchBox.h"
#include "SInlineEditableTextBlock.h"
#include "SNotificationList.h"
#include "NotificationManager.h"
#include "BlueprintActionDatabase.h"

#define LOCTEXT_NAMESPACE "SkeletonAnimNotifies"

static const FName ColumnId_AnimNotifyNameLabel( "AnimNotifyName" );

//////////////////////////////////////////////////////////////////////////
// SMorphTargetListRow

typedef TSharedPtr< FDisplayedAnimNotifyInfo > FDisplayedAnimNotifyInfoPtr;

class SAnimNotifyListRow
	: public SMultiColumnTableRow< FDisplayedAnimNotifyInfoPtr >
{
public:

	SLATE_BEGIN_ARGS( SAnimNotifyListRow ) {}

	/** The item for this row **/
	SLATE_ARGUMENT( FDisplayedAnimNotifyInfoPtr, Item )

	/* Widget used to display the list of morph targets */
	SLATE_ARGUMENT( TSharedPtr<SSkeletonAnimNotifies>, NotifiesListView )

	/* Persona used to update the viewport when a weight slider is dragged */
	SLATE_ARGUMENT( TWeakPtr<FPersona>, Persona )

	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTableView );

	/** Overridden from SMultiColumnTableRow.  Generates a widget for this column of the tree row. */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override;

private:

	/** Widget used to display the list of notifies */
	TSharedPtr<SSkeletonAnimNotifies> NotifiesListView;

	/** The notify being displayed by this row */
	FDisplayedAnimNotifyInfoPtr	Item;

	/** Pointer back to the Persona that owns us */
	TWeakPtr<FPersona> PersonaPtr;
};

void SAnimNotifyListRow::Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView )
{
	Item = InArgs._Item;
	NotifiesListView = InArgs._NotifiesListView;
	PersonaPtr = InArgs._Persona;

	check( Item.IsValid() );

	SMultiColumnTableRow< FDisplayedAnimNotifyInfoPtr >::Construct( FSuperRowType::FArguments(), InOwnerTableView );
}

TSharedRef< SWidget > SAnimNotifyListRow::GenerateWidgetForColumn( const FName& ColumnName )
{
	check( ColumnName == ColumnId_AnimNotifyNameLabel );

	return SNew( SVerticalBox )
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding( 0.0f, 4.0f )
	.VAlign( VAlign_Center )
	[
		SAssignNew(Item->InlineEditableText, SInlineEditableTextBlock)
		.Text( FText::FromName(Item->Name) )
		.OnVerifyTextChanged(NotifiesListView.Get(), &SSkeletonAnimNotifies::OnVerifyNotifyNameCommit, Item)
		.OnTextCommitted(NotifiesListView.Get(), &SSkeletonAnimNotifies::OnNotifyNameCommitted, Item)
		.IsSelected(NotifiesListView.Get(), &SSkeletonAnimNotifies::IsSelected)
	];
}

/////////////////////////////////////////////////////
// FSkeletonAnimNotifiesSummoner

FSkeletonAnimNotifiesSummoner::FSkeletonAnimNotifiesSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp)
	: FWorkflowTabFactory(FPersonaTabs::SkeletonAnimNotifiesID, InHostingApp)
{
	TabLabel = LOCTEXT("SkeletonAnimNotifiesTabTitle", "Animation Notifies");
	TabIcon = FSlateIcon(FEditorStyle::GetStyleSetName(), "Persona.Tabs.AnimationNotifies");

	EnableTabPadding();
	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("SkeletonAnimNotifiesMenu", "Animation Notifies");
	ViewMenuTooltip = LOCTEXT("SkeletonAnimNotifies_ToolTip", "Shows the skeletons notifies list");
}

TSharedRef<SWidget> FSkeletonAnimNotifiesSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	return SNew(SSkeletonAnimNotifies)
		.Persona(StaticCastSharedPtr<FPersona>(HostingApp.Pin()));
}

/////////////////////////////////////////////////////
// SSkeletonAnimNotifies

void SSkeletonAnimNotifies::Construct(const FArguments& InArgs)
{
	PersonaPtr = InArgs._Persona;
	TargetSkeleton = PersonaPtr.Pin()->GetSkeleton();

	PersonaPtr.Pin()->RegisterOnPostUndo(FPersona::FOnPostUndo::CreateSP( this, &SSkeletonAnimNotifies::PostUndo ) );
	PersonaPtr.Pin()->RegisterOnChangeAnimNotifies(FPersona::FOnAnimNotifiesChanged::CreateSP( this, &SSkeletonAnimNotifies::RefreshNotifiesListWithFilter ) );

	this->ChildSlot
	[
		SNew( SVerticalBox )

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding( FMargin( 0.0f, 0.0f, 0.0f, 4.0f ) )
		[
			SAssignNew( NameFilterBox, SSearchBox )
			.SelectAllTextWhenFocused( true )
			.OnTextChanged( this, &SSkeletonAnimNotifies::OnFilterTextChanged )
			.OnTextCommitted( this, &SSkeletonAnimNotifies::OnFilterTextCommitted )
			.HintText( LOCTEXT( "NotifiesSearchBoxHint", "Search Animation Notifies...") )
		]

		+ SVerticalBox::Slot()
		.FillHeight( 1.0f )		// This is required to make the scrollbar work, as content overflows Slate containers by default
		[
			SAssignNew( NotifiesListView, SAnimNotifyListType )
			.ListItemsSource( &NotifyList )
			.OnGenerateRow( this, &SSkeletonAnimNotifies::GenerateNotifyRow )
			.OnContextMenuOpening( this, &SSkeletonAnimNotifies::OnGetContextMenuContent )
			.OnSelectionChanged( this, &SSkeletonAnimNotifies::OnNotifySelectionChanged )
			.ItemHeight( 22.0f )
			.HeaderRow
			(
				SNew( SHeaderRow )
				+ SHeaderRow::Column( ColumnId_AnimNotifyNameLabel )
				.DefaultLabel( LOCTEXT( "AnimNotifyNameLabel", "Notify Name" ) )
			)
		]
	];

	CreateNotifiesList();
}

SSkeletonAnimNotifies::~SSkeletonAnimNotifies()
{
	if ( PersonaPtr.IsValid() )
	{
		PersonaPtr.Pin()->UnregisterOnPostUndo( this );
		PersonaPtr.Pin()->UnregisterOnChangeAnimNotifies( this );
	}
}

void SSkeletonAnimNotifies::OnFilterTextChanged( const FText& SearchText )
{
	FilterText = SearchText;

	RefreshNotifiesListWithFilter();
}

void SSkeletonAnimNotifies::OnFilterTextCommitted( const FText& SearchText, ETextCommit::Type CommitInfo )
{
	// Just do the same as if the user typed in the box
	OnFilterTextChanged( SearchText );
}

TSharedRef<ITableRow> SSkeletonAnimNotifies::GenerateNotifyRow(TSharedPtr<FDisplayedAnimNotifyInfo> InInfo, const TSharedRef<STableViewBase>& OwnerTable)
{
	check( InInfo.IsValid() );

	return
		SNew( SAnimNotifyListRow, OwnerTable )
		.Persona( PersonaPtr )
		.Item( InInfo )
		.NotifiesListView( SharedThis( this ) );
}

TSharedPtr<SWidget> SSkeletonAnimNotifies::OnGetContextMenuContent() const
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder( bShouldCloseWindowAfterMenuSelection, NULL);

	MenuBuilder.BeginSection("AnimNotifyAction", LOCTEXT( "AnimNotifyActions", "Selected Notify Actions" ) );
	{
		{
			FUIAction Action = FUIAction( FExecuteAction::CreateSP( this, &SSkeletonAnimNotifies::OnDeleteAnimNotify ), 
				FCanExecuteAction::CreateSP( this, &SSkeletonAnimNotifies::CanPerformDelete ) );
			const FText Label = LOCTEXT("DeleteAnimNotifyButtonLabel", "Delete");
			const FText ToolTip = LOCTEXT("DeleteAnimNotifyButtonTooltip", "Deletes the selected anim notifies.");
			MenuBuilder.AddMenuEntry( Label, ToolTip, FSlateIcon(), Action);
		}

		{
			FUIAction Action = FUIAction( FExecuteAction::CreateSP( this, &SSkeletonAnimNotifies::OnRenameAnimNotify ), 
				FCanExecuteAction::CreateSP( this, &SSkeletonAnimNotifies::CanPerformRename ) );
			const FText Label = LOCTEXT("RenameAnimNotifyButtonLabel", "Rename");
			const FText ToolTip = LOCTEXT("RenameAnimNotifyButtonTooltip", "Renames the selected anim notifies.");
			MenuBuilder.AddMenuEntry( Label, ToolTip, FSlateIcon(), Action);
		}
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SSkeletonAnimNotifies::OnNotifySelectionChanged(TSharedPtr<FDisplayedAnimNotifyInfo> Selection, ESelectInfo::Type SelectInfo)
{
	if(Selection.IsValid())
	{
		ShowNotifyInDetailsView(Selection->Name);
	}
}

bool SSkeletonAnimNotifies::CanPerformDelete() const
{
	TArray< TSharedPtr< FDisplayedAnimNotifyInfo > > SelectedRows = NotifiesListView->GetSelectedItems();
	return SelectedRows.Num() > 0;
}

bool SSkeletonAnimNotifies::CanPerformRename() const
{
	TArray< TSharedPtr< FDisplayedAnimNotifyInfo > > SelectedRows = NotifiesListView->GetSelectedItems();
	return SelectedRows.Num() == 1;
}

void SSkeletonAnimNotifies::OnDeleteAnimNotify()
{
	TArray< TSharedPtr< FDisplayedAnimNotifyInfo > > SelectedRows = NotifiesListView->GetSelectedItems();

	const FScopedTransaction Transaction( LOCTEXT("DeleteAnimNotify", "Delete Anim Notify") );

	// this one deletes all notifies with same name. 
	TArray<FName> SelectedNotifyNames;
	TargetSkeleton->Modify();

	for(int Selection = 0; Selection < SelectedRows.Num(); ++Selection)
	{
		FName& SelectedName = SelectedRows[Selection]->Name;
		TargetSkeleton->AnimationNotifies.Remove(SelectedName);
		SelectedNotifyNames.Add(SelectedName);
	}

	TArray<FAssetData> CompatibleAnimSequences;
	GetCompatibleAnimSequences(CompatibleAnimSequences);

	int32 NumAnimationsModified = 0;

	for( int32 AssetIndex = 0; AssetIndex < CompatibleAnimSequences.Num(); ++AssetIndex )
	{
		const FAssetData& PossibleAnimSequence = CompatibleAnimSequences[AssetIndex];
		UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(PossibleAnimSequence.GetAsset());

		bool SequenceModified = false;
		for(int32 NotifyIndex = Sequence->Notifies.Num()-1; NotifyIndex >= 0; --NotifyIndex)
		{
			FAnimNotifyEvent& AnimNotify = Sequence->Notifies[NotifyIndex];
			if(SelectedNotifyNames.Contains(AnimNotify.NotifyName))
			{
				if(!SequenceModified)
				{
					Sequence->Modify();
					++NumAnimationsModified;
					SequenceModified = true;
				}
				Sequence->Notifies.RemoveAtSwap(NotifyIndex);
			}
		}

		if(SequenceModified)
		{
			Sequence->MarkPackageDirty();
		}
	}

	if(NumAnimationsModified > 0)
	{
		// Tell the user that the socket is a duplicate
		FFormatNamedArguments Args;
		Args.Add( TEXT("NumAnimationsModified"), NumAnimationsModified );
		FNotificationInfo Info( FText::Format( LOCTEXT( "AnimNotifiesDeleted", "{NumAnimationsModified} animation(s) modified to delete notifications" ), Args ) );

		Info.bUseLargeFont = false;
		Info.ExpireDuration = 5.0f;

		NotifyUser( Info );
	}

	FBlueprintActionDatabase::Get().RefreshAssetActions(TargetSkeleton);

	CreateNotifiesList( NameFilterBox->GetText().ToString() );
}

void SSkeletonAnimNotifies::OnRenameAnimNotify()
{
	TArray< TSharedPtr< FDisplayedAnimNotifyInfo > > SelectedRows = NotifiesListView->GetSelectedItems();

	check(SelectedRows.Num() == 1); // Should be guaranteed by CanPerformRename

	SelectedRows[0]->InlineEditableText->EnterEditingMode();
}

bool SSkeletonAnimNotifies::OnVerifyNotifyNameCommit( const FText& NewName, FText& OutErrorMessage, TSharedPtr<FDisplayedAnimNotifyInfo> Item )
{
	bool bValid(true);

	if(NewName.IsEmpty())
	{
		OutErrorMessage = LOCTEXT( "NameMissing_Error", "You must provide a name." );
		bValid = false;
	}

	FName NotifyName( *NewName.ToString() );
	if(NotifyName != Item->Name)
	{
		if(TargetSkeleton->AnimationNotifies.Contains(NotifyName))
		{
			OutErrorMessage = FText::Format( LOCTEXT("AlreadyInUseMessage", "'{0}' is already in use."), NewName );
			bValid = false;
		}
	}

	return bValid;
}

void SSkeletonAnimNotifies::OnNotifyNameCommitted( const FText& NewName, ETextCommit::Type, TSharedPtr<FDisplayedAnimNotifyInfo> Item )
{
	const FScopedTransaction Transaction( LOCTEXT("RenameAnimNotify", "Rename Anim Notify") );

	FName NewNotifyName = FName( *NewName.ToString() );
	FName NotifyToRename = Item->Name;

	// rename notify in skeleton
	TargetSkeleton->Modify();
	int32 Index = TargetSkeleton->AnimationNotifies.IndexOfByKey(NotifyToRename);
	TargetSkeleton->AnimationNotifies[Index] = NewNotifyName;

	TArray<FAssetData> CompatibleAnimSequences;
	GetCompatibleAnimSequences(CompatibleAnimSequences);

	int32 NumAnimationsModified = 0;

	for( int32 AssetIndex = 0; AssetIndex < CompatibleAnimSequences.Num(); ++AssetIndex )
	{
		const FAssetData& PossibleAnimSequence = CompatibleAnimSequences[AssetIndex];
		UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(PossibleAnimSequence.GetAsset());

		bool SequenceModified = false;
		for(int32 NotifyIndex = Sequence->Notifies.Num()-1; NotifyIndex >= 0; --NotifyIndex)
		{
			FAnimNotifyEvent& AnimNotify = Sequence->Notifies[NotifyIndex];
			if(NotifyToRename == AnimNotify.NotifyName)
			{
				if(!SequenceModified)
				{
					Sequence->Modify();
					++NumAnimationsModified;
					SequenceModified = true;
				}	
				AnimNotify.NotifyName = NewNotifyName;
			}
		}

		if(SequenceModified)
		{
			Sequence->MarkPackageDirty();
		}
	}

	if(NumAnimationsModified > 0)
	{
		// Tell the user that the socket is a duplicate
		FFormatNamedArguments Args;
		Args.Add( TEXT("NumAnimationsModified"), NumAnimationsModified );
		FNotificationInfo Info( FText::Format( LOCTEXT( "AnimNotifiesRenamed", "{NumAnimationsModified} animation(s) modified to rename notification" ), Args ) );

		Info.bUseLargeFont = false;
		Info.ExpireDuration = 5.0f;

		NotifyUser( Info );
	}

	RefreshNotifiesListWithFilter();
}

void SSkeletonAnimNotifies::RefreshNotifiesListWithFilter()
{
	CreateNotifiesList( NameFilterBox->GetText().ToString() );
}

void SSkeletonAnimNotifies::CreateNotifiesList( const FString& SearchText )
{
	NotifyList.Empty();

	if ( TargetSkeleton )
	{
		for(int i = 0; i < TargetSkeleton->AnimationNotifies.Num(); ++i)
		{
			FName& NotifyName = TargetSkeleton->AnimationNotifies[i];
			if ( !SearchText.IsEmpty() )
			{
				if ( NotifyName.ToString().Contains( SearchText ) )
				{
					NotifyList.Add( FDisplayedAnimNotifyInfo::Make( NotifyName ) );
				}
			}
			else
			{
				NotifyList.Add( FDisplayedAnimNotifyInfo::Make( NotifyName ) );
			}
		}
	}

	NotifiesListView->RequestListRefresh();
}

void SSkeletonAnimNotifies::ShowNotifyInDetailsView(FName NotifyName)
{
	UEditorSkeletonNotifyObj *Obj = Cast<UEditorSkeletonNotifyObj>(ShowInDetailsView(UEditorSkeletonNotifyObj::StaticClass()));
	if(Obj != NULL)
	{
		Obj->AnimationNames.Empty();

		TArray<FAssetData> CompatibleAnimSequences;
		GetCompatibleAnimSequences(CompatibleAnimSequences);

		for( int32 AssetIndex = 0; AssetIndex < CompatibleAnimSequences.Num(); ++AssetIndex )
		{
			const FAssetData& PossibleAnimSequence = CompatibleAnimSequences[AssetIndex];
			UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(PossibleAnimSequence.GetAsset());
			for(int32 NotifyIndex = 0; NotifyIndex < Sequence->Notifies.Num(); ++NotifyIndex)
			{
				FAnimNotifyEvent& NotifyEvent = Sequence->Notifies[NotifyIndex];
				if(NotifyEvent.NotifyName == NotifyName)
				{
					Obj->AnimationNames.Add(MakeShareable(new FString(PossibleAnimSequence.AssetName.ToString())));
					break;
				}
			}
		}

		Obj->Name = NotifyName;
	}
}

void SSkeletonAnimNotifies::GetCompatibleAnimSequences(TArray<class FAssetData>& OutAssets)
{
	//Get the skeleton tag to search for
	FString SkeletonExportName = FAssetData(TargetSkeleton).GetExportTextName();

	// Load the asset registry module
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	TArray<FAssetData> AssetDataList;
	AssetRegistryModule.Get().GetAssetsByClass(UAnimSequenceBase::StaticClass()->GetFName(), AssetDataList, true);

	OutAssets.Empty(AssetDataList.Num());

	for( int32 AssetIndex = 0; AssetIndex < AssetDataList.Num(); ++AssetIndex )
	{
		const FAssetData& PossibleAnimSequence = AssetDataList[AssetIndex];
		if( SkeletonExportName == PossibleAnimSequence.TagsAndValues.FindRef("Skeleton") )
		{
			OutAssets.Add( PossibleAnimSequence );
		}
	}
}

UObject* SSkeletonAnimNotifies::ShowInDetailsView( UClass* EdClass )
{
	UObject *Obj = EditorObjectTracker.GetEditorObjectForClass(EdClass);

	if(Obj != NULL)
	{
		PersonaPtr.Pin()->SetDetailObject(Obj);
	}
	return Obj;
}

void SSkeletonAnimNotifies::ClearDetailsView()
{
	PersonaPtr.Pin()->SetDetailObject(NULL);
}

void SSkeletonAnimNotifies::PostUndo()
{
	RefreshNotifiesListWithFilter();
}

void SSkeletonAnimNotifies::AddReferencedObjects( FReferenceCollector& Collector )
{
	EditorObjectTracker.AddReferencedObjects(Collector);
}

void SSkeletonAnimNotifies::NotifyUser( FNotificationInfo& NotificationInfo )
{
	TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification( NotificationInfo );
	if ( Notification.IsValid() )
	{
		Notification->SetCompletionState( SNotificationItem::CS_Fail );
	}
}
#undef LOCTEXT_NAMESPACE
