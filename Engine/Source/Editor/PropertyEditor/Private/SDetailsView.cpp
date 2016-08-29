// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "PropertyEditorPrivatePCH.h"
#include "AssetSelection.h"
#include "PropertyNode.h"
#include "ItemPropertyNode.h"
#include "CategoryPropertyNode.h"
#include "ObjectPropertyNode.h"
#include "ScopedTransaction.h"
#include "AssetThumbnail.h"
#include "SDetailNameArea.h"
#include "IPropertyUtilities.h"
#include "PropertyEditorHelpers.h"
#include "PropertyEditor.h"
#include "PropertyDetailsUtilities.h"
#include "SPropertyEditorEditInline.h"
#include "ObjectEditorUtils.h"
#include "SColorPicker.h"
#include "SSearchBox.h"


#define LOCTEXT_NAMESPACE "SDetailsView"


SDetailsView::~SDetailsView()
{
	const FRootPropertyNodeList& RootNodes = GetRootNodes();
	for(const TSharedPtr<FComplexPropertyNode>& RootNode : RootNodes)
	{
		SaveExpandedItems(RootNode.ToSharedRef());
	}
};

/**
 * Constructs the widget
 *
 * @param InArgs   Declaration from which to construct this widget.              
 */
void SDetailsView::Construct(const FArguments& InArgs)
{
	DetailsViewArgs = InArgs._DetailsViewArgs;
	bViewingClassDefaultObject = false;

	PropertyUtilities = MakeShareable( new FPropertyDetailsUtilities( *this ) );
	
	ColumnSizeData.LeftColumnWidth = TAttribute<float>( this, &SDetailsView::OnGetLeftColumnWidth );
	ColumnSizeData.RightColumnWidth = TAttribute<float>( this, &SDetailsView::OnGetRightColumnWidth );
	ColumnSizeData.OnWidthChanged = SSplitter::FOnSlotResized::CreateSP( this, &SDetailsView::OnSetColumnWidth );

	// We want the scrollbar to always be visible when objects are selected, but not when there is no selection - however:
	//  - We can't use AlwaysShowScrollbar for this, as this will also show the scrollbar when nothing is selected
	//  - We can't use the Visibility construction parameter, as it gets translated into user visibility and can hide the scrollbar even when objects are selected
	// We instead have to explicitly set the visibility after the scrollbar has been constructed to get the exact behavior we want
	TSharedRef<SScrollBar> ExternalScrollbar = SNew(SScrollBar);
	ExternalScrollbar->SetVisibility( TAttribute<EVisibility>( this, &SDetailsView::GetScrollBarVisibility ) );

		FMenuBuilder DetailViewOptions( true, nullptr);

		if (DetailsViewArgs.bShowModifiedPropertiesOption)
		{
			DetailViewOptions.AddMenuEntry( 
				LOCTEXT("ShowOnlyModified", "Show Only Modified Properties"),
				LOCTEXT("ShowOnlyModified_ToolTip", "Displays only properties which have been changed from their default"),
				FSlateIcon(),
				FUIAction( 
					FExecuteAction::CreateSP( this, &SDetailsView::OnShowOnlyModifiedClicked ),
					FCanExecuteAction(),
					FIsActionChecked::CreateSP( this, &SDetailsView::IsShowOnlyModifiedChecked )
				),
				NAME_None,
				EUserInterfaceActionType::ToggleButton 
			);
		}

		if( DetailsViewArgs.bShowDifferingPropertiesOption )
		{
			DetailViewOptions.AddMenuEntry(
				LOCTEXT("ShowOnlyDiffering", "Show Only Differing Properties"),
				LOCTEXT("ShowOnlyDiffering_ToolTip", "Displays only properties in this instance which have been changed or added from the instance being compared"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(this, &SDetailsView::OnShowOnlyDifferingClicked),
					FCanExecuteAction(),
					FIsActionChecked::CreateSP(this, &SDetailsView::IsShowOnlyDifferingChecked)
				),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);
		}

		DetailViewOptions.AddMenuEntry(
			LOCTEXT("ShowAllAdvanced", "Show All Advanced Details"),
			LOCTEXT("ShowAllAdvanced_ToolTip", "Shows all advanced detail sections in each category"),
			FSlateIcon(),
			FUIAction( 
			FExecuteAction::CreateSP( this, &SDetailsView::OnShowAllAdvancedClicked ),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP( this, &SDetailsView::IsShowAllAdvancedChecked )
				),
			NAME_None,
			EUserInterfaceActionType::ToggleButton 
		);

		DetailViewOptions.AddMenuEntry(
			LOCTEXT("ShowAllChildrenIfCategoryMatches", "Show Child On Category Match"),
			LOCTEXT("ShowAllChildrenIfCategoryMatches_ToolTip", "Shows children if their category matches the search criteria"),
			FSlateIcon(),
			FUIAction( 
				FExecuteAction::CreateSP( this, &SDetailsView::OnShowAllChildrenIfCategoryMatchesClicked ),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP( this, &SDetailsView::IsShowAllChildrenIfCategoryMatchesChecked )
			),
			NAME_None,
			EUserInterfaceActionType::ToggleButton 
			);

		DetailViewOptions.AddMenuEntry(
			LOCTEXT("CollapseAll", "Collapse All Categories"),
			LOCTEXT("CollapseAll_ToolTip", "Collapses all root level categories"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &SDetailsView::SetRootExpansionStates, /*bExpanded=*/false, /*bRecurse=*/false )));

		DetailViewOptions.AddMenuEntry(
			LOCTEXT("ExpandAll", "Expand All Categories"),
			LOCTEXT("ExpandAll_ToolTip", "Expands all root level categories"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &SDetailsView::SetRootExpansionStates, /*bExpanded=*/true, /*bRecurse=*/false )));

	FilterRow = SNew( SHorizontalBox )
		.Visibility( this, &SDetailsView::GetFilterBoxVisibility )
		+SHorizontalBox::Slot()
		.FillWidth( 1 )
		.VAlign( VAlign_Center )
		[
			// Create the search box
			SAssignNew( SearchBox, SSearchBox )
			.OnTextChanged( this, &SDetailsView::OnFilterTextChanged  )
			.AddMetaData<FTagMetaData>(TEXT("Details.Search"))
		]
		+SHorizontalBox::Slot()
		.Padding( 4.0f, 0.0f, 0.0f, 0.0f )
		.AutoWidth()
		[
			// Create the search box
			SNew( SButton )
			.OnClicked( this, &SDetailsView::OnOpenRawPropertyEditorClicked )
			.IsEnabled( this, &SDetailsView::IsPropertyEditingEnabled )
			.ToolTipText( LOCTEXT("RawPropertyEditorButtonLabel", "Open Selection in Property Matrix") )
			[
				SNew( SImage )
				.Image( FEditorStyle::GetBrush("DetailsView.EditRawProperties") )
			]
		];

	if (DetailsViewArgs.bShowOptions)
	{
		FilterRow->AddSlot()
			.HAlign(HAlign_Right)
			.AutoWidth()
			[
				SNew( SComboButton )
				.ContentPadding(0)
				.ForegroundColor( FSlateColor::UseForeground() )
				.ButtonStyle( FEditorStyle::Get(), "ToggleButton" )
				.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ViewOptions")))
				.MenuContent()
				[
					DetailViewOptions.MakeWidget()
				]
				.ButtonContent()
				[
					SNew(SImage)
					.Image( FEditorStyle::GetBrush("GenericViewButton") )
				]
			];
	}

	// Create the name area which does not change when selection changes
	SAssignNew(NameArea, SDetailNameArea, &SelectedObjects)
		// the name area is only for actors
		.Visibility(this, &SDetailsView::GetActorNameAreaVisibility)
		.OnLockButtonClicked(this, &SDetailsView::OnLockButtonClicked)
		.IsLocked(this, &SDetailsView::IsLocked)
		.ShowLockButton(DetailsViewArgs.bLockable)
		.ShowActorLabel(DetailsViewArgs.bShowActorLabel)
		// only show the selection tip if we're not selecting objects
		.SelectionTip(!DetailsViewArgs.bHideSelectionTip);

	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);

	if( !DetailsViewArgs.bCustomNameAreaLocation )
	{
		VerticalBox->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			NameArea.ToSharedRef()
		];
	}

	if( !DetailsViewArgs.bCustomFilterAreaLocation )
	{
		VerticalBox->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 2.0f)
		[
			FilterRow.ToSharedRef()
		];
	}

	VerticalBox->AddSlot()
	.FillHeight(1)
	.Padding(0)
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			ConstructTreeView(ExternalScrollbar)
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		[
			SNew(SBox)
			.WidthOverride(16.0f)
			[
				ExternalScrollbar
			]
		]
	];

	ChildSlot
	[
		VerticalBox
	];
}

TSharedRef<SDetailTree> SDetailsView::ConstructTreeView( TSharedRef<SScrollBar>& ScrollBar )
{
	check( !DetailTree.IsValid() || DetailTree.IsUnique() );

	return 
	SAssignNew( DetailTree, SDetailTree )
	.Visibility( this, &SDetailsView::GetTreeVisibility )
	.TreeItemsSource( &RootTreeNodes )
	.OnGetChildren( this, &SDetailsView::OnGetChildrenForDetailTree )
	.OnSetExpansionRecursive( this, &SDetailsView::SetNodeExpansionStateRecursive )
	.OnGenerateRow( this, &SDetailsView::OnGenerateRowForDetailTree )	
	.OnExpansionChanged( this, &SDetailsView::OnItemExpansionChanged )
	.SelectionMode( ESelectionMode::None )
	.ExternalScrollbar( ScrollBar );
}

FReply SDetailsView::OnOpenRawPropertyEditorClicked()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>( "PropertyEditor" );
	PropertyEditorModule.CreatePropertyEditorToolkit( EToolkitMode::Standalone, TSharedPtr<IToolkitHost>(), SelectedObjects );

	return FReply::Handled();
}

EVisibility SDetailsView::GetActorNameAreaVisibility() const
{
	const bool bVisible = DetailsViewArgs.NameAreaSettings != FDetailsViewArgs::HideNameArea && !bViewingClassDefaultObject;
	return bVisible ? EVisibility::Visible : EVisibility::Collapsed; 
}

EVisibility SDetailsView::GetScrollBarVisibility() const
{
	const bool bHasObjects = RootPropertyNodes.Num() > 0;
	return bHasObjects ? EVisibility::Visible : EVisibility::Collapsed; 
}

void SDetailsView::ForceRefresh()
{
	TArray< TWeakObjectPtr< UObject > > NewObjectList;

	const FRootPropertyNodeList& RootNodes = GetRootNodes();
	for(const TSharedPtr<FComplexPropertyNode>& ComplexRootNode : RootNodes)
	{
		FObjectPropertyNode* RootNode = ComplexRootNode->AsObjectNode();
		if(RootNode)
		{
			// Simply re-add the same existing objects to cause a refresh
			for(TPropObjectIterator Itor(RootNode->ObjectIterator()); Itor; ++Itor)
			{
				TWeakObjectPtr<UObject> Object = *Itor;
				if(Object.IsValid())
				{
					NewObjectList.Add(Object.Get());
				}
			}
		}
	}

	SetObjectArrayPrivate( NewObjectList );
}

void SDetailsView::MoveScrollOffset(int32 DeltaOffset)
{
	DetailTree->AddScrollOffset((float)DeltaOffset);
}

void SDetailsView::SetObjects( const TArray<UObject*>& InObjects, bool bForceRefresh/* = false*/, bool bOverrideLock/* = false*/ )
{
	if (!IsLocked() || bOverrideLock)
	{
		TArray< TWeakObjectPtr< UObject > > ObjectWeakPtrs;
		
		for( auto ObjectIter = InObjects.CreateConstIterator(); ObjectIter; ++ObjectIter )
		{
			ObjectWeakPtrs.Add( *ObjectIter );
		}

		if( bForceRefresh || ShouldSetNewObjects( ObjectWeakPtrs ) )
		{
			SetObjectArrayPrivate( ObjectWeakPtrs );
		}
	}
}

void SDetailsView::SetObjects( const TArray< TWeakObjectPtr< UObject > >& InObjects, bool bForceRefresh/* = false*/, bool bOverrideLock/* = false*/ )
{
	if (!IsLocked() || bOverrideLock)
	{
		if( bForceRefresh || ShouldSetNewObjects( InObjects ) )
		{
			SetObjectArrayPrivate( InObjects );
		}
	}
}

void SDetailsView::SetObject( UObject* InObject, bool bForceRefresh )
{
	TArray< TWeakObjectPtr< UObject > > ObjectWeakPtrs;
	ObjectWeakPtrs.Add( InObject );

	SetObjects( ObjectWeakPtrs, bForceRefresh );
}

void SDetailsView::RemoveInvalidObjects()
{
	TArray< TWeakObjectPtr< UObject > > ResetArray;

	bool bAllFound = true;
	
	for(const TSharedPtr<FComplexPropertyNode>& ComplexRootNode : RootPropertyNodes)
	{
		FObjectPropertyNode* RootPropertyNode = ComplexRootNode->AsObjectNode();
		if(RootPropertyNode)
		{
			for(TPropObjectIterator Itor(RootPropertyNode->ObjectIterator()); Itor; ++Itor)
			{
				TWeakObjectPtr<UObject> Object = *Itor;

				if(Object.IsValid() && !Object->IsPendingKill())
				{
					ResetArray.Add(Object);
				}
				else
				{
					bAllFound = false;
				}
			}
		}
	}

	if (!bAllFound)
	{
		SetObjectArrayPrivate(ResetArray);
	}
}

void SDetailsView::SetObjectPackageOverrides(const TMap<TWeakObjectPtr<UObject>, TWeakObjectPtr<UPackage>>& InMapping)
{
	for(TSharedPtr<FComplexPropertyNode>& ComplexRootNode : RootPropertyNodes)
	{
		FObjectPropertyNode* RootNode = ComplexRootNode->AsObjectNode();
		if(RootNode)
		{
			RootNode->SetObjectPackageOverrides(InMapping);
		}
	}
}

void SDetailsView::SetRootObjectCustomizationInstance(TSharedPtr<IDetailRootObjectCustomization> InRootObjectCustomization)
{
	RootObjectCustomization = InRootObjectCustomization;
	RerunCurrentFilter();
}

void SDetailsView::ClearSearch()
{
	CurrentFilter.FilterStrings.Empty();
	SearchBox->SetText(FText::GetEmpty());
	RerunCurrentFilter();
}

bool SDetailsView::ShouldSetNewObjects(const TArray< TWeakObjectPtr< UObject > >& InObjects) const
{
	bool bShouldSetObjects = false;

	const bool bHadBSPBrushSelected = SelectedActorInfo.bHaveBSPBrush;
	if( bHadBSPBrushSelected == true )
	{
		// If a BSP brush was selected we need to refresh because surface could have been selected and the object set not updated
		bShouldSetObjects = true;
	}
	else if( InObjects.Num() != GetNumObjects() )
	{
		// If the object arrays differ in size then at least one object is different so we must reset
		bShouldSetObjects = true;
	}
	else if(InObjects.Num() == 0)
	{
		// User is likely resetting details panel
		bShouldSetObjects = true;
	}
	else
	{
		// Check to see if the objects passed in are different. If not we do not need to set anything
		TSet< TWeakObjectPtr< UObject > > NewObjects;
		NewObjects.Append(InObjects);

		if(DetailsViewArgs.bAllowMultipleTopLevelObjects)
		{
			
			// For multiple top level node support, if the single object in each node is not found in the new object set
			// then we need to refresh
			for(int32 RootNodeIndex = 0; RootNodeIndex < RootPropertyNodes.Num(); ++RootNodeIndex)
			{
				FObjectPropertyNode* RootPropertyNode = RootPropertyNodes[RootNodeIndex]->AsObjectNode();
				
				if(RootPropertyNode && RootPropertyNode->GetNumObjects() > 0)
				{
					if(!NewObjects.Contains(RootPropertyNode->GetUObject(0)))
					{
						bShouldSetObjects = true;
						break;
					}
				}
				else
				{
					bShouldSetObjects = true;
					break;
				}
			}
		}
		else
		{

			ensure(RootPropertyNodes.Num() == 1);
			FObjectPropertyNode* RootPropertyNode = RootPropertyNodes[0]->AsObjectNode();
			if( RootPropertyNode )
			{
				for(TPropObjectIterator Itor(RootPropertyNode->ObjectIterator()); Itor; ++Itor)
				{
					TWeakObjectPtr<UObject> Object = *Itor;
					if(Object.IsValid() && !NewObjects.Contains(Object))
					{
						// An existing object is not in the list of new objects to set
						bShouldSetObjects = true;
						break;
					}
					else if(!Object.IsValid())
					{
						// An existing object is invalid
						bShouldSetObjects = true;
						break;
					}
				}
			}
			else
			{
				bShouldSetObjects = true;
			}
		}
	}
	
	if (!bShouldSetObjects && AssetSelectionUtils::IsAnySurfaceSelected(nullptr))
	{
		bShouldSetObjects = true;
	}

	return bShouldSetObjects;
}

int32 SDetailsView::GetNumObjects() const
{
	if(DetailsViewArgs.bAllowMultipleTopLevelObjects)
	{
		return RootPropertyNodes.Num();
	}
	else if( RootPropertyNodes.Num() > 0 && RootPropertyNodes[0]->AsObjectNode())
	{
		return RootPropertyNodes[0]->AsObjectNode()->GetNumObjects();
	}

	return 0;
}

void SDetailsView::SetObjectArrayPrivate(const TArray< TWeakObjectPtr< UObject > >& InObjects)
{
	double StartTime = FPlatformTime::Seconds();

	PreSetObject(InObjects.Num());

	// Selected actors for building SelectedActorInfo
	TArray<AActor*> SelectedRawActors;

	bViewingClassDefaultObject = InObjects.Num() > 0 ? true : false;
	bool bOwnedByLockedLevel = false;
	for( int32 ObjectIndex = 0 ; ObjectIndex < InObjects.Num(); ++ObjectIndex )
	{
		TWeakObjectPtr< UObject > Object = InObjects[ObjectIndex];

		if( Object.IsValid() )
		{
			bViewingClassDefaultObject &= Object->HasAnyFlags( RF_ClassDefaultObject );

			if(DetailsViewArgs.bAllowMultipleTopLevelObjects)
			{
				check(RootPropertyNodes.Num() == InObjects.Num());
				RootPropertyNodes[ObjectIndex]->AsObjectNode()->AddObject( Object.Get() );
			}
			else
			{
				RootPropertyNodes[0]->AsObjectNode()->AddObject( Object.Get() );
			}

			SelectedObjects.Add( Object );
			AActor* Actor = Cast<AActor>( Object.Get() );
			if( Actor )
			{
				SelectedActors.Add( Actor );
				SelectedRawActors.Add( Actor );
			}
		}
	}

	if( InObjects.Num() == 0 )
	{
		// Unlock the view automatically if we are viewing nothing
		bIsLocked = false;
	}

	// Selection changed, refresh the detail area
	if ( DetailsViewArgs.NameAreaSettings != FDetailsViewArgs::ActorsUseNameArea && DetailsViewArgs.NameAreaSettings != FDetailsViewArgs::ComponentsAndActorsUseNameArea )
	{
		NameArea->Refresh( SelectedObjects );
	}
	else
	{
		NameArea->Refresh( SelectedActors, SelectedObjects, DetailsViewArgs.NameAreaSettings );
	}
	
	// When selection changes rebuild information about the selection
	SelectedActorInfo = AssetSelectionUtils::BuildSelectedActorInfo( SelectedRawActors );

	// @todo Slate Property Window
	//SetFlags(EPropertyWindowFlags::ReadOnly, bOwnedByLockedLevel);


	PostSetObject();

	// Set the title of the window based on the objects we are viewing
	// Or call the delegate for handling when the title changed
	FString Title;

	if( GetNumObjects() == 0 )
	{
		Title = NSLOCTEXT("PropertyView", "NothingSelectedTitle", "Nothing selected").ToString();
	}
	else if( GetNumObjects() == 1 && RootPropertyNodes[0]->AsObjectNode()->GetNumObjects() > 0)
	{
		// if the object is the default metaobject for a UClass, use the UClass's name instead
		UObject* Object = RootPropertyNodes[0]->AsObjectNode()->GetUObject(0);
		FString ObjectName = Object->GetName();
		if ( Object->GetClass()->GetDefaultObject() == Object )
		{
			ObjectName = Object->GetClass()->GetName();
		}
		else
		{
			// Is this an actor?  If so, it might have a friendly name to display
			const AActor* Actor = Cast<const  AActor >( Object );
			if( Actor != nullptr)
			{
				// Use the friendly label for this actor
				ObjectName = Actor->GetActorLabel();
			}
		}

		Title = ObjectName;
	}
	else if(DetailsViewArgs.bAllowMultipleTopLevelObjects)
	{
		Title = FString::Printf(*NSLOCTEXT("PropertyView", "MultipleToLevelObjectsSelected", "%i selected").ToString(), GetNumObjects());
	}
	else
	{
		FObjectPropertyNode* RootPropertyNode = RootPropertyNodes[0]->AsObjectNode();
		Title = FString::Printf( *NSLOCTEXT("PropertyView", "MultipleSelected", "%s (%i selected)").ToString(), *RootPropertyNode->GetObjectBaseClass()->GetName(), RootPropertyNode->GetNumObjects() );
	}

	OnObjectArrayChanged.ExecuteIfBound(Title, InObjects);

	double ElapsedTime = FPlatformTime::Seconds() - StartTime;
}

void SDetailsView::ReplaceObjects( const TMap<UObject*, UObject*>& OldToNewObjectMap )
{
	TArray< TWeakObjectPtr< UObject > > NewObjectList;
	bool bObjectsReplaced = false;

	TArray< FObjectPropertyNode* > ObjectNodes;
	for(TSharedPtr<FComplexPropertyNode>& RootNode : RootPropertyNodes)
	{
		PropertyEditorHelpers::CollectObjectNodes(RootNode, ObjectNodes );
	}

	for( int32 ObjectNodeIndex = 0; ObjectNodeIndex < ObjectNodes.Num(); ++ObjectNodeIndex )
	{
		FObjectPropertyNode* CurrentNode = ObjectNodes[ObjectNodeIndex];

		// Scan all objects and look for objects which need to be replaced
		for ( TPropObjectIterator Itor( CurrentNode->ObjectIterator() ); Itor; ++Itor )
		{
			UObject* Replacement = OldToNewObjectMap.FindRef( Itor->Get() );
			if( Replacement && Replacement->GetClass() == Itor->Get()->GetClass() )
			{
				bObjectsReplaced = true;
				if( CurrentNode->IsRootNode() )
				{
					// Note: only root objects count for the new object list. Sub-Objects (i.e components count as needing to be replaced but they don't belong in the top level object list
					NewObjectList.Add( Replacement );
				}
			}
			else if( CurrentNode->IsRootNode() )
			{
				// Note: only root objects count for the new object list. Sub-Objects (i.e components count as needing to be replaced but they don't belong in the top level object list
				NewObjectList.Add( Itor->Get() );
			}
		}
	}


	if( bObjectsReplaced )
	{
		SetObjectArrayPrivate( NewObjectList );
	}

}

void SDetailsView::RemoveDeletedObjects( const TArray<UObject*>& DeletedObjects )
{
	TArray< TWeakObjectPtr< UObject > > NewObjectList;
	bool bObjectsRemoved = false;

	for(const TSharedPtr<FComplexPropertyNode>& ComplexRootNode : RootPropertyNodes)
	{
		FObjectPropertyNode* RootPropertyNode = ComplexRootNode->AsObjectNode();
		// Scan all objects and look for objects which need to be replaced
		for ( TPropObjectIterator Itor( RootPropertyNode->ObjectIterator() ); Itor; ++Itor )
		{
			if( DeletedObjects.Contains( Itor->Get() ) )
			{
				// An object we had needs to be removed
				bObjectsRemoved = true;
			}
			else
			{
				// If the deleted object list does not contain the current object, its ok to keep it in the list
				NewObjectList.Add( Itor->Get() );
			}
		}
	}

	// if any objects were replaced update the observed objects
	if( bObjectsRemoved )
	{
		SetObjectArrayPrivate( NewObjectList );
	}
}

/** Called before during SetObjectArray before we change the objects being observed */
void SDetailsView::PreSetObject(int32 InNewNumObjects)
{
	// Save existing expanded items first
	for(TSharedPtr<FComplexPropertyNode>& RootNode : RootPropertyNodes)
	{
		SaveExpandedItems(RootNode.ToSharedRef());

		RootNodesPendingKill.Add(RootNode);
		FObjectPropertyNode* RootObjectNode = RootNode->AsObjectNode();
		RootObjectNode->RemoveAllObjects();
		RootObjectNode->ClearCachedReadAddresses(true);
		RootObjectNode->ClearObjectPackageOverrides();
	}

	for( auto ExternalRootNode : ExternalRootPropertyNodes )
	{
		if( ExternalRootNode.IsValid() )
		{
			SaveExpandedItems( ExternalRootNode.Pin().ToSharedRef() );

			FComplexPropertyNode* ComplexNode = ExternalRootNode.Pin()->AsComplexNode();
			if(ComplexNode)
			{
				ComplexNode->Disconnect();
			}
		}
	}

	ExternalRootPropertyNodes.Empty();
	RootPropertyNodes.Empty(InNewNumObjects);

	if(DetailsViewArgs.bAllowMultipleTopLevelObjects)
	{
		for(int32 NewRootIndex = 0; NewRootIndex < InNewNumObjects; ++NewRootIndex)
		{
			RootPropertyNodes.Add(MakeShareable(new FObjectPropertyNode));
		}
	}
	else
	{
		RootPropertyNodes.Add(MakeShareable(new FObjectPropertyNode));
	}	


	SelectedActors.Empty();
	SelectedObjects.Empty();
}


/** Called at the end of SetObjectArray after we change the objects being observed */
void SDetailsView::PostSetObject()
{
	DestroyColorPicker();
	ColorPropertyNode = nullptr;

	FPropertyNodeInitParams InitParams;
	InitParams.ParentNode = nullptr;
	InitParams.Property = nullptr;
	InitParams.ArrayOffset = 0;
	InitParams.ArrayIndex = INDEX_NONE;
	InitParams.bAllowChildren = true;
	InitParams.bForceHiddenPropertyVisibility =  FPropertySettings::Get().ShowHiddenProperties();

	switch ( DetailsViewArgs.DefaultsOnlyVisibility )
	{
	case FDetailsViewArgs::EEditDefaultsOnlyNodeVisibility::Hide:
		InitParams.bCreateDisableEditOnInstanceNodes = false;
		break;
	case FDetailsViewArgs::EEditDefaultsOnlyNodeVisibility::Show:
		InitParams.bCreateDisableEditOnInstanceNodes = true;
		break;
	case FDetailsViewArgs::EEditDefaultsOnlyNodeVisibility::Automatic:
		InitParams.bCreateDisableEditOnInstanceNodes = HasClassDefaultObject();
		break;
	default:
		check(false);
	}

	for( TSharedPtr<FComplexPropertyNode>& ComplexRootNode : RootPropertyNodes )
	{
		FObjectPropertyNode* RootPropertyNode = ComplexRootNode->AsObjectNode();

		RootPropertyNode->InitNode( InitParams );

		// Restore existing expanded items
		RestoreExpandedItems(ComplexRootNode.ToSharedRef());
	}

	UpdatePropertyMaps();

	for( auto ExternalRootNode : ExternalRootPropertyNodes )
	{
		if( ExternalRootNode.IsValid() )
		{
			RestoreExpandedItems( ExternalRootNode.Pin().ToSharedRef() );
		}
	}

	UpdateFilteredDetails();
}

void SDetailsView::SetOnObjectArrayChanged(FOnObjectArrayChanged OnObjectArrayChangedDelegate)
{
	OnObjectArrayChanged = OnObjectArrayChangedDelegate;
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
const UClass* SDetailsView::GetBaseClass() const
{
	if( RootPropertyNodes.Num() == 1 )
	{
		return RootPropertyNodes[0]->AsObjectNode()->GetObjectBaseClass();
	}
	else
	{
		ensureMsgf(0,TEXT("GetBaseClass is not supported on multi-object details panels"));
	}

	return nullptr;
}

UClass* SDetailsView::GetBaseClass()
{
	if(RootPropertyNodes.Num() == 1)
	{
		return RootPropertyNodes[0]->AsObjectNode()->GetObjectBaseClass();
	}
	else
	{
		ensureMsgf(0, TEXT("GetBaseClass is not supported on multi-object details panels"));
	}

	return nullptr;
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

void SDetailsView::RegisterInstancedCustomPropertyLayout( UStruct* Class, FOnGetDetailCustomizationInstance DetailLayoutDelegate )
{
	RegisterInstancedCustomPropertyLayoutInternal(Class, DetailLayoutDelegate);
}

void SDetailsView::UnregisterInstancedCustomPropertyLayout( UStruct* Class )
{
	UnregisterInstancedCustomPropertyLayoutInternal(Class);
}

void SDetailsView::AddExternalRootPropertyNode( TSharedRef<FPropertyNode> ExternalRootNode )
{
	ExternalRootPropertyNodes.Add( ExternalRootNode );
}

bool SDetailsView::IsCategoryHiddenByClass( const TSharedPtr<FComplexPropertyNode>& InRootNode, FName CategoryName ) const
{
	return InRootNode->AsObjectNode()->GetHiddenCategories().Contains( CategoryName );
}

bool SDetailsView::IsConnected() const
{
	return RootPropertyNodes.Num() > 0;
}

const FSlateBrush* SDetailsView::OnGetLockButtonImageResource() const
{
	if (bIsLocked)
	{
		return FEditorStyle::GetBrush(TEXT("PropertyWindow.Locked"));
	}
	else
	{
		return FEditorStyle::GetBrush(TEXT("PropertyWindow.Unlocked"));
	}
}

#undef LOCTEXT_NAMESPACE
