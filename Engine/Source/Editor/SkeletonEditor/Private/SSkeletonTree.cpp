// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "SSkeletonTree.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "UObject/PropertyPortFlags.h"
#include "Framework/Application/SlateApplication.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/UICommandList.h"
#include "Widgets/Images/SImage.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Layout/SScrollBorder.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "EditorStyleSet.h"
#include "ActorFactories/ActorFactory.h"
#include "Exporters/Exporter.h"
#include "Sound/SoundBase.h"
#include "Editor.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"

#include "ScopedTransaction.h"
#include "BoneDragDropOp.h"
#include "SocketDragDropOp.h"
#include "SkeletonTreeCommands.h"
#include "Styling/SlateIconFinder.h"
#include "DragAndDrop/AssetDragDropOp.h"

#include "AssetSelection.h"

#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "ComponentAssetBroker.h"



#include "AnimPreviewInstance.h"

#include "MeshUtilities.h"
#include "UnrealExporter.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Framework/Commands/GenericCommands.h"
#include "Animation/BlendProfile.h"
#include "SBlendProfilePicker.h"
#include "IPersonaPreviewScene.h"
#include "IDocumentation.h"
#include "PersonaUtils.h"
#include "Misc/TextFilterExpressionEvaluator.h"
#include "SkeletonTreeBoneItem.h"
#include "SkeletonTreeSocketItem.h"
#include "SkeletonTreeAttachedAssetItem.h"
#include "SkeletonTreeVirtualBoneItem.h"

#include "BoneSelectionWidget.h"

#define LOCTEXT_NAMESPACE "SSkeletonTree"

const FName	ISkeletonTree::Columns::Name("Name");
const FName	ISkeletonTree::Columns::Retargeting("Retargeting");
const FName ISkeletonTree::Columns::BlendProfile("BlendProfile");

void SSkeletonTree::Construct(const FArguments& InArgs, const TSharedRef<FEditableSkeleton>& InEditableSkeleton, const FSkeletonTreeArgs& InSkeletonTreeArgs)
{
	BoneFilter = EBoneFilter::All;
	SocketFilter = ESocketFilter::Active;
	bShowingAdvancedOptions = false;
	bSelectingSocket = false;
	bSelectingBone = false;
	bDeselectingAll = false;

	EditableSkeleton = InEditableSkeleton;
	PreviewScene = InSkeletonTreeArgs.PreviewScene;
	IsEditable = InArgs._IsEditable;
	Builder = InArgs._Builder;
	if (!Builder.IsValid())
	{
		Builder = MakeShareable(new FSkeletonTreeBuilder(FSkeletonTreeBuilderArgs(), FOnFilterSkeletonTreeItem::CreateSP(this, &SSkeletonTree::HandleFilterSkeletonTreeItem), SharedThis(this), InSkeletonTreeArgs.PreviewScene));
	}

	TextFilterPtr = MakeShareable(new FTextFilterExpressionEvaluator(ETextFilterExpressionEvaluatorMode::BasicString));

	SetPreviewComponentSocketFilter();

	// Register delegates
	InSkeletonTreeArgs.OnPostUndo.Add(FSimpleDelegate::CreateSP(this, &SSkeletonTree::PostUndo));

	PreviewScene->RegisterOnLODChanged(FSimpleDelegate::CreateSP(this, &SSkeletonTree::OnLODSwitched));

	InEditableSkeleton->RegisterOnSkeletonHierarchyChanged(USkeleton::FOnSkeletonHierarchyChanged::CreateSP(this, &SSkeletonTree::PostUndo));

	RegisterOnObjectSelected(InSkeletonTreeArgs.OnObjectSelected);

	// Register and bind all our menu commands
	FSkeletonTreeCommands::Register();
	BindCommands();

	this->ChildSlot
	[
		SNew( SVerticalBox )
		
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding( FMargin( 0.0f, 0.0f, 0.0f, 4.0f ) )
		[
			SAssignNew( NameFilterBox, SSearchBox )
			.SelectAllTextWhenFocused( true )
			.OnTextChanged( this, &SSkeletonTree::OnFilterTextChanged )
			.HintText( LOCTEXT( "SearchBoxHint", "Search Skeleton Tree...") )
			.AddMetaData<FTagMetaData>(TEXT("SkelTree.Search"))
		]

		+ SVerticalBox::Slot()
		.Padding( FMargin( 0.0f, 4.0f, 0.0f, 0.0f ) )
		[
			SAssignNew(TreeHolder, SOverlay)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew( SHorizontalBox )

			+ SHorizontalBox::Slot()
			.Padding(0.0f, 0.0f, 2.0f, 0.0f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			.FillWidth(1.0f)
			[
				SNew(SHorizontalBox)
				
				+ SHorizontalBox::Slot()
				.Padding(0.0f, 0.0f, 2.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					SAssignNew(BlendProfilePicker, SBlendProfilePicker, GetEditableSkeleton())
					.Standalone(true)
					.OnBlendProfileSelected(this, &SSkeletonTree::OnBlendProfileSelected)
				]
			]

			+ SHorizontalBox::Slot()
			.Padding( 0.0f, 0.0f, 2.0f, 0.0f )
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			.AutoWidth()
			[
				SAssignNew( FilterComboButton, SComboButton )
				.ButtonStyle(FEditorStyle::Get(), "ToggleButton")
				.ForegroundColor(this, &SSkeletonTree::GetFilterComboButtonForegroundColor)
				.ContentPadding(0.0f)
				.OnGetMenuContent( this, &SSkeletonTree::CreateFilterMenu )
				.ToolTipText( LOCTEXT( "BoneFilterToolTip", "Change which types of bones and sockets are shown, retargeting options" ) )
				.AddMetaData<FTagMetaData>(TEXT("SkelTree.Bones"))
				.ButtonContent()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SImage).Image( FEditorStyle::GetBrush("GenericViewButton") )
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2, 0, 0, 0)
					.VAlign(VAlign_Center)
					[
						SNew( STextBlock )
						.Text( this, &SSkeletonTree::GetFilterMenuTitle )
					]
				]
			]
		]
	];

	CreateTreeColumns();

	OnLODSwitched();
}

SSkeletonTree::~SSkeletonTree()
{
	if (EditableSkeleton.IsValid())
	{
		EditableSkeleton.Pin()->UnregisterOnSkeletonHierarchyChanged(this);
	}
}

void SSkeletonTree::BindCommands()
{
	// This should not be called twice on the same instance
	check( !UICommandList.IsValid() );

	UICommandList = MakeShareable( new FUICommandList );

	FUICommandList& CommandList = *UICommandList;

	// Grab the list of menu commands to bind...
	const FSkeletonTreeCommands& MenuActions = FSkeletonTreeCommands::Get();

	// ...and bind them all

	// Bone Filter commands
	CommandList.MapAction(
		MenuActions.ShowAllBones,
		FExecuteAction::CreateSP( this, &SSkeletonTree::SetBoneFilter, EBoneFilter::All ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SSkeletonTree::IsBoneFilter, EBoneFilter::All ) );

	CommandList.MapAction(
		MenuActions.ShowMeshBones,
		FExecuteAction::CreateSP( this, &SSkeletonTree::SetBoneFilter, EBoneFilter::Mesh ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SSkeletonTree::IsBoneFilter, EBoneFilter::Mesh ) );

	CommandList.MapAction(
		MenuActions.ShowLODBones,
		FExecuteAction::CreateSP(this, &SSkeletonTree::SetBoneFilter, EBoneFilter::LOD),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SSkeletonTree::IsBoneFilter, EBoneFilter::LOD));
	
	CommandList.MapAction(
		MenuActions.ShowWeightedBones,
		FExecuteAction::CreateSP( this, &SSkeletonTree::SetBoneFilter, EBoneFilter::Weighted ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SSkeletonTree::IsBoneFilter, EBoneFilter::Weighted ) );

	CommandList.MapAction(
		MenuActions.HideBones,
		FExecuteAction::CreateSP( this, &SSkeletonTree::SetBoneFilter, EBoneFilter::None ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SSkeletonTree::IsBoneFilter, EBoneFilter::None ) );

	// Socket filter commands
	CommandList.MapAction(
		MenuActions.ShowActiveSockets,
		FExecuteAction::CreateSP( this, &SSkeletonTree::SetSocketFilter, ESocketFilter::Active ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SSkeletonTree::IsSocketFilter, ESocketFilter::Active ) );

	CommandList.MapAction(
		MenuActions.ShowMeshSockets,
		FExecuteAction::CreateSP( this, &SSkeletonTree::SetSocketFilter, ESocketFilter::Mesh ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SSkeletonTree::IsSocketFilter, ESocketFilter::Mesh ) );

	CommandList.MapAction(
		MenuActions.ShowSkeletonSockets,
		FExecuteAction::CreateSP( this, &SSkeletonTree::SetSocketFilter, ESocketFilter::Skeleton ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SSkeletonTree::IsSocketFilter, ESocketFilter::Skeleton ) );

	CommandList.MapAction(
		MenuActions.ShowAllSockets,
		FExecuteAction::CreateSP( this, &SSkeletonTree::SetSocketFilter, ESocketFilter::All ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SSkeletonTree::IsSocketFilter, ESocketFilter::All ) );

	CommandList.MapAction(
		MenuActions.HideSockets,
		FExecuteAction::CreateSP( this, &SSkeletonTree::SetSocketFilter, ESocketFilter::None ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SSkeletonTree::IsSocketFilter, ESocketFilter::None ) );

	CommandList.MapAction(
		MenuActions.ShowRetargeting,
		FExecuteAction::CreateSP(this, &SSkeletonTree::OnChangeShowingAdvancedOptions),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SSkeletonTree::IsShowingAdvancedOptions));

	CommandList.MapAction(
		MenuActions.FilteringFlattensHierarchy,
		FExecuteAction::CreateLambda([this]() { GetMutableDefault<UPersonaOptions>()->bFlattenSkeletonHierarchyWhenFiltering = !GetDefault<UPersonaOptions>()->bFlattenSkeletonHierarchyWhenFiltering; ApplyFilter(); }),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([]() { return GetDefault<UPersonaOptions>()->bFlattenSkeletonHierarchyWhenFiltering; }));

	// Socket manipulation commands
	CommandList.MapAction(
		MenuActions.AddSocket,
		FExecuteAction::CreateSP( this, &SSkeletonTree::OnAddSocket ),
		FCanExecuteAction::CreateSP( this, &SSkeletonTree::IsAddingSocketsAllowed ) );

	CommandList.MapAction(
		FGenericCommands::Get().Rename,
		FExecuteAction::CreateSP( this, &SSkeletonTree::OnRenameSocket ),
		FCanExecuteAction::CreateSP( this, &SSkeletonTree::CanRenameSelected ) );

	CommandList.MapAction(
		MenuActions.CreateMeshSocket,
		FExecuteAction::CreateSP( this, &SSkeletonTree::OnCustomizeSocket ) );

	CommandList.MapAction(
		MenuActions.RemoveMeshSocket,
		FExecuteAction::CreateSP( this, &SSkeletonTree::OnDeleteSelectedRows ) ); // Removing customization just deletes the mesh socket

	CommandList.MapAction(
		MenuActions.PromoteSocketToSkeleton,
		FExecuteAction::CreateSP( this, &SSkeletonTree::OnPromoteSocket ) ); // Adding customization just deletes the mesh socket

	CommandList.MapAction(
		MenuActions.DeleteSelectedRows,
		FExecuteAction::CreateSP( this, &SSkeletonTree::OnDeleteSelectedRows ) );

	CommandList.MapAction(
		MenuActions.CopyBoneNames,
		FExecuteAction::CreateSP( this, &SSkeletonTree::OnCopyBoneNames ) );

	CommandList.MapAction(
		MenuActions.ResetBoneTransforms,
		FExecuteAction::CreateSP(this, &SSkeletonTree::OnResetBoneTransforms ) );

	CommandList.MapAction(
		MenuActions.CopySockets,
		FExecuteAction::CreateSP( this, &SSkeletonTree::OnCopySockets ) );

	CommandList.MapAction(
		MenuActions.PasteSockets,
		FExecuteAction::CreateSP( this, &SSkeletonTree::OnPasteSockets, false ) );

	CommandList.MapAction(
		MenuActions.PasteSocketsToSelectedBone,
		FExecuteAction::CreateSP(this, &SSkeletonTree::OnPasteSockets, true));

	CommandList.MapAction(
		MenuActions.FocusCamera,
		FExecuteAction::CreateSP(this, &SSkeletonTree::HandleFocusCamera));
}

TSharedRef<ITableRow> SSkeletonTree::MakeTreeRowWidget(TSharedPtr<ISkeletonTreeItem> InInfo, const TSharedRef<STableViewBase>& OwnerTable)
{
	check( InInfo.IsValid() );
	
	return InInfo->MakeTreeRowWidget(OwnerTable, TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateLambda([this]() { return FilterText; })));
}

void SSkeletonTree::GetFilteredChildren(TSharedPtr<ISkeletonTreeItem> InInfo, TArray< TSharedPtr<ISkeletonTreeItem> >& OutChildren)
{
	check(InInfo.IsValid());
	OutChildren = InInfo->GetFilteredChildren();
}

void SSkeletonTree::CreateTreeColumns()
{
	TSharedRef<SHeaderRow> TreeHeaderRow = SNew(SHeaderRow)
		+ SHeaderRow::Column(ISkeletonTree::Columns::Name)
		.DefaultLabel(LOCTEXT("SkeletonBoneNameLabel", "Name"))
		.FillWidth(0.5f);

	if (bShowingAdvancedOptions)
	{
		TreeHeaderRow->AddColumn(
			SHeaderRow::Column(ISkeletonTree::Columns::Retargeting)
			.DefaultLabel(LOCTEXT("SkeletonBoneTranslationRetargetingLabel", "Translation Retargeting"))
			.FillWidth(0.25f)
			);
	}

	if(BlendProfilePicker->GetSelectedBlendProfileName() != NAME_None)
	{
		TreeHeaderRow->AddColumn(
			SHeaderRow::Column(ISkeletonTree::Columns::BlendProfile)
			.DefaultLabel(LOCTEXT("BlendProfileLabel", "Blend Profile Scale"))
			.FillWidth(0.25f));
	}

	TreeHolder->ClearChildren();
	TreeHolder->AddSlot()
	[
		SNew(SScrollBorder, SkeletonTreeView.ToSharedRef())
		[
			SAssignNew(SkeletonTreeView, STreeView<TSharedPtr<ISkeletonTreeItem>>)
			.TreeItemsSource(&FilteredItems)
			.OnGenerateRow(this, &SSkeletonTree::MakeTreeRowWidget)
			.OnGetChildren(this, &SSkeletonTree::GetFilteredChildren)
			.OnContextMenuOpening(this, &SSkeletonTree::CreateContextMenu)
			.OnSelectionChanged(this, &SSkeletonTree::OnSelectionChanged)
			.OnItemScrolledIntoView(this, &SSkeletonTree::OnItemScrolledIntoView)
			.OnMouseButtonDoubleClick(this, &SSkeletonTree::OnTreeDoubleClick)
			.OnSetExpansionRecursive(this, &SSkeletonTree::SetTreeItemExpansionRecursive)
			.ItemHeight(24)
			.HeaderRow
			(
				TreeHeaderRow
			)
		]
	];

	CreateFromSkeleton();
}

/** Helper struct for when we rebuild the tree because of a change to its structure */
struct FSavedSelection
{
	/** Name of the selected item */
	FName ItemName;

	/** Type of the selected item */
	FName ItemType;
};

void SSkeletonTree::CreateFromSkeleton()
{	
	// save selected items
	TArray<FSavedSelection> SavedSelections;
	TArray<TSharedPtr<ISkeletonTreeItem>> SelectedItems = SkeletonTreeView->GetSelectedItems();
	for (const TSharedPtr<ISkeletonTreeItem>& SelectedItem : SelectedItems)
	{
		SavedSelections.Add({ SelectedItem->GetRowItemName(), SelectedItem->GetTypeName() });
	}

	Items.Empty();
	LinearItems.Empty();
	FilteredItems.Empty();

	FSkeletonTreeBuilderOutput Output(Items, LinearItems);
	Builder->Build(Output);
	ApplyFilter();

	// restore selection
	for (const TSharedPtr<ISkeletonTreeItem>& Item : LinearItems)
	{
		if (Item->GetFilterResult() != ESkeletonTreeFilterResult::Hidden)
		{
			for (FSavedSelection& SavedSelection : SavedSelections)
			{
				if (Item->GetRowItemName() == SavedSelection.ItemName && Item->GetTypeName() == SavedSelection.ItemType)
				{
					SkeletonTreeView->SetItemSelection(Item, true);
					break;
				}
			}
		}
	}
}

void SSkeletonTree::ApplyFilter()
{
	TextFilterPtr->SetFilterText(FilterText);

	FilteredItems.Empty();

	FSkeletonTreeFilterArgs FilterArgs;
	FilterArgs.bWillFilter = (!FilterText.IsEmpty());
	FilterArgs.bFlattenHierarchyOnFilter = GetDefault<UPersonaOptions>()->bFlattenSkeletonHierarchyWhenFiltering;
	Builder->Filter(FilterArgs, Items, FilteredItems);

	for (TSharedPtr<ISkeletonTreeItem>& Item : LinearItems)
	{
		SkeletonTreeView->SetItemExpansion(Item, true);
	}

	SkeletonTreeView->RequestTreeRefresh();
}

class FBoneTreeSelection
{
public:
	TArray<TSharedPtr<ISkeletonTreeItem>> SelectedItems;

	TArray<TSharedPtr<FSkeletonTreeBoneItem>> SelectedBones;
	TArray<TSharedPtr<FSkeletonTreeSocketItem>> SelectedSockets;
	TArray<TSharedPtr<FSkeletonTreeAttachedAssetItem>> SelectedAssets;
	TArray<TSharedPtr<FSkeletonTreeVirtualBoneItem>> SelectedVirtualBones;

	FBoneTreeSelection(TArray<TSharedPtr<ISkeletonTreeItem>> InSelectedItems) : SelectedItems(InSelectedItems)
	{
		for ( auto ItemIt = SelectedItems.CreateConstIterator(); ItemIt; ++ItemIt )
		{
			TSharedPtr<ISkeletonTreeItem> Item = *(ItemIt);

			if (Item->IsOfType<FSkeletonTreeBoneItem>())
			{
				SelectedBones.Add(StaticCastSharedPtr<FSkeletonTreeBoneItem>(Item));
			}
			else if (Item->IsOfType<FSkeletonTreeSocketItem>())
			{
				SelectedSockets.Add(StaticCastSharedPtr<FSkeletonTreeSocketItem>(Item));
			}
			else if (Item->IsOfType<FSkeletonTreeAttachedAssetItem>())
			{
				SelectedAssets.Add(StaticCastSharedPtr<FSkeletonTreeAttachedAssetItem>(Item));
			}
			else if (Item->IsOfType<FSkeletonTreeVirtualBoneItem>())
			{
				SelectedVirtualBones.Add(StaticCastSharedPtr<FSkeletonTreeVirtualBoneItem>(Item));
			}
		}
	}

	bool IsMultipleItemsSelected() const
	{
		return SelectedItems.Num() > 1;
	}

	bool IsSingleItemSelected() const
	{
		return SelectedItems.Num() == 1;
	}

	template<typename ItemType>
	bool IsSingleOfTypeSelected() const
	{
		if(IsSingleItemSelected())
		{
			return SelectedItems[0]->IsOfType<ItemType>();
		}
		return false;
	}

	TSharedPtr<ISkeletonTreeItem> GetSingleSelectedItem()
	{
		check(IsSingleItemSelected());
		return SelectedItems[0];
	}

	template<typename ItemType>
	bool HasSelectedOfType() const
	{
		for (const TSharedPtr<ISkeletonTreeItem>& SelectedItem : SelectedItems)
		{
			if (SelectedItem->IsOfType<ItemType>())
			{
				return true;
			}
		}

		return false;
	}
};

TSharedPtr< SWidget > SSkeletonTree::CreateContextMenu()
{
	const FSkeletonTreeCommands& Actions = FSkeletonTreeCommands::Get();

	FBoneTreeSelection BoneTreeSelection(SkeletonTreeView->GetSelectedItems());

	const bool CloseAfterSelection = true;
	FMenuBuilder MenuBuilder( CloseAfterSelection, UICommandList );
	{
		if(BoneTreeSelection.HasSelectedOfType<FSkeletonTreeAttachedAssetItem>() || BoneTreeSelection.HasSelectedOfType<FSkeletonTreeSocketItem>() || BoneTreeSelection.HasSelectedOfType<FSkeletonTreeVirtualBoneItem>())
		{
			MenuBuilder.BeginSection("SkeletonTreeSelectedItemsActions", LOCTEXT( "SelectedActions", "Selected Item Actions" ) );
			MenuBuilder.AddMenuEntry( Actions.DeleteSelectedRows );
			MenuBuilder.EndSection();
		}

		if(BoneTreeSelection.HasSelectedOfType<FSkeletonTreeBoneItem>())
		{
			MenuBuilder.BeginSection("SkeletonTreeBonesAction", LOCTEXT( "BoneActions", "Selected Bone Actions" ) );
			MenuBuilder.AddMenuEntry( Actions.CopyBoneNames );
			MenuBuilder.AddMenuEntry( Actions.ResetBoneTransforms );

			if(BoneTreeSelection.IsSingleOfTypeSelected<FSkeletonTreeBoneItem>())
			{
				MenuBuilder.AddMenuEntry( Actions.AddSocket );
				MenuBuilder.AddMenuEntry( Actions.PasteSockets );
				MenuBuilder.AddMenuEntry( Actions.PasteSocketsToSelectedBone );
			}

			MenuBuilder.AddSubMenu(LOCTEXT("AddVirtualBone", "Add Virtual Bone"),
				LOCTEXT("AddVirtualBone_ToolTip", "Adds a virtual bone to the skeleton."),
				FNewMenuDelegate::CreateSP(this, &SSkeletonTree::FillVirtualBoneSubmenu, BoneTreeSelection.SelectedBones));

			MenuBuilder.EndSection();

			UBlendProfile* const SelectedBlendProfile = BlendProfilePicker->GetSelectedBlendProfile();
			if(SelectedBlendProfile && BoneTreeSelection.IsSingleOfTypeSelected<FSkeletonTreeBoneItem>())
			{
				TSharedPtr<FSkeletonTreeBoneItem> BoneItem = BoneTreeSelection.SelectedBones[0];

				FName BoneName = BoneItem->GetAttachName();
				const USkeleton& Skeleton = GetEditableSkeletonInternal()->GetSkeleton();
				int32 BoneIndex = Skeleton.GetReferenceSkeleton().FindBoneIndex(BoneName);

				float CurrentBlendScale = SelectedBlendProfile->GetBoneBlendScale(BoneIndex);

				MenuBuilder.BeginSection("SkeletonTreeBlendProfileScales", LOCTEXT("BlendProfileContextOptions", "Blend Profile"));
				{
					FUIAction RecursiveSetScales;
					RecursiveSetScales.ExecuteAction = FExecuteAction::CreateSP(this, &SSkeletonTree::RecursiveSetBlendProfileScales, CurrentBlendScale);
					
					MenuBuilder.AddMenuEntry
						(
						FText::Format(LOCTEXT("RecursiveSetBlendScales_Label", "Recursively Set Blend Scales To {0}"), FText::AsNumber(CurrentBlendScale)),
						LOCTEXT("RecursiveSetBlendScales_ToolTip", "Sets all child bones to use the same blend profile scale as the selected bone"),
						FSlateIcon(),
						RecursiveSetScales
						);
				}
				MenuBuilder.EndSection();
			}

			if(bShowingAdvancedOptions)
			{
				MenuBuilder.BeginSection("SkeletonTreeBoneTranslationRetargeting", LOCTEXT("BoneTranslationRetargetingHeader", "Bone Translation Retargeting"));
				{
					FUIAction RecursiveRetargetingSkeletonAction = FUIAction(FExecuteAction::CreateSP(this, &SSkeletonTree::SetBoneTranslationRetargetingModeRecursive, EBoneTranslationRetargetingMode::Skeleton));
					FUIAction RecursiveRetargetingAnimationAction = FUIAction(FExecuteAction::CreateSP(this, &SSkeletonTree::SetBoneTranslationRetargetingModeRecursive, EBoneTranslationRetargetingMode::Animation));
					FUIAction RecursiveRetargetingAnimationScaledAction = FUIAction(FExecuteAction::CreateSP(this, &SSkeletonTree::SetBoneTranslationRetargetingModeRecursive, EBoneTranslationRetargetingMode::AnimationScaled));
					FUIAction RecursiveRetargetingAnimationRelativeAction = FUIAction(FExecuteAction::CreateSP(this, &SSkeletonTree::SetBoneTranslationRetargetingModeRecursive, EBoneTranslationRetargetingMode::AnimationRelative));

					MenuBuilder.AddMenuEntry
						(LOCTEXT("SetTranslationRetargetingSkeletonChildrenAction", "Recursively Set Translation Retargeting Skeleton")
						, LOCTEXT("BoneTranslationRetargetingSkeletonToolTip", "Use translation from Skeleton.")
						, FSlateIcon()
						, RecursiveRetargetingSkeletonAction
						);

					MenuBuilder.AddMenuEntry
						(LOCTEXT("SetTranslationRetargetingAnimationChildrenAction", "Recursively Set Translation Retargeting Animation")
						, LOCTEXT("BoneTranslationRetargetingAnimationToolTip", "Use translation from animation.")
						, FSlateIcon()
						, RecursiveRetargetingAnimationAction
						);

					MenuBuilder.AddMenuEntry
						(LOCTEXT("SetTranslationRetargetingAnimationScaledChildrenAction", "Recursively Set Translation Retargeting AnimationScaled")
						, LOCTEXT("BoneTranslationRetargetingAnimationScaledToolTip", "Use translation from animation, scale length by Skeleton's proportions.")
						, FSlateIcon()
						, RecursiveRetargetingAnimationScaledAction
						);

					MenuBuilder.AddMenuEntry
						(LOCTEXT("SetTranslationRetargetingAnimationRelativeChildrenAction", "Recursively Set Translation Retargeting AnimationRelative")
						, LOCTEXT("BoneTranslationRetargetingAnimationRelativeToolTip", "Use relative translation from animation similar to an additive animation.")
						, FSlateIcon()
						, RecursiveRetargetingAnimationRelativeAction
						);
				}
				MenuBuilder.EndSection();
			}

			MenuBuilder.BeginSection("SkeletonTreeBoneReductionForLOD", LOCTEXT("BoneReductionHeader", "LOD Bone Reduction"));
			{
				MenuBuilder.AddSubMenu(
					LOCTEXT("SkeletonTreeBoneReductionForLOD_RemoveSelectedFromLOD", "Remove Selected..."),
					FText::GetEmpty(),
					FNewMenuDelegate::CreateStatic(&SSkeletonTree::CreateMenuForBoneReduction, this, LastCachedLODForPreviewMeshComponent, true)
					);

				MenuBuilder.AddSubMenu(
					LOCTEXT("SkeletonTreeBoneReductionForLOD_RemoveChildrenFromLOD", "Remove Children..."),
					FText::GetEmpty(),
					FNewMenuDelegate::CreateStatic(&SSkeletonTree::CreateMenuForBoneReduction, this, LastCachedLODForPreviewMeshComponent, false)
					);
			}
			MenuBuilder.EndSection();
		}

		if(BoneTreeSelection.HasSelectedOfType<FSkeletonTreeSocketItem>())
		{
			MenuBuilder.BeginSection("SkeletonTreeSocketsActions", LOCTEXT( "SocketActions", "Selected Socket Actions" ) );

			MenuBuilder.AddMenuEntry( Actions.CopySockets );

			if(BoneTreeSelection.IsSingleOfTypeSelected<FSkeletonTreeSocketItem>())
			{
				MenuBuilder.AddMenuEntry( FGenericCommands::Get().Rename, NAME_None, LOCTEXT("RenameSocket_Label", "Rename Socket"), LOCTEXT("RenameSocket_Tooltip", "Rename this socket") );

				TSharedPtr<FSkeletonTreeSocketItem> SocketItem = BoneTreeSelection.SelectedSockets[0];

				if (SocketItem->IsSocketCustomized() && SocketItem->GetParentType() == ESocketParentType::Mesh )
				{
					MenuBuilder.AddMenuEntry( Actions.RemoveMeshSocket );
				}

				// If the socket is on the skeleton, we have a valid mesh
				// and there isn't one of the same name on the mesh, we can customize it
				if (SocketItem->CanCustomizeSocket() )
				{
					if (SocketItem->GetParentType() == ESocketParentType::Skeleton )
					{
						MenuBuilder.AddMenuEntry( Actions.CreateMeshSocket );
					}
					else if (SocketItem->GetParentType() == ESocketParentType::Mesh )
					{
						// If a socket is on the mesh only, then offer to promote it to the skeleton
						MenuBuilder.AddMenuEntry( Actions.PromoteSocketToSkeleton );
					}
				}
			}

			MenuBuilder.EndSection();
		}

		MenuBuilder.BeginSection("SkeletonTreeAttachedAssets", LOCTEXT( "AttachedAssetsActionsHeader", "Attached Assets Actions" ) );

		if ( BoneTreeSelection.IsSingleItemSelected() )
		{
			MenuBuilder.AddSubMenu(	LOCTEXT( "AttachNewAsset", "Add Preview Asset" ),
				LOCTEXT ( "AttachNewAsset_ToolTip", "Attaches an asset to this part of the skeleton. Assets can also be dragged onto the skeleton from a content browser to attach" ),
				FNewMenuDelegate::CreateSP( this, &SSkeletonTree::FillAttachAssetSubmenu, BoneTreeSelection.GetSingleSelectedItem() ) );
		}

		FUIAction RemoveAllAttachedAssets = FUIAction(	FExecuteAction::CreateSP( this, &SSkeletonTree::OnRemoveAllAssets ),
			FCanExecuteAction::CreateSP( this, &SSkeletonTree::CanRemoveAllAssets ));

		MenuBuilder.AddMenuEntry( LOCTEXT( "RemoveAllAttachedAssets", "Remove All Attached Assets" ),
			LOCTEXT ( "RemoveAllAttachedAssets_ToolTip", "Removes all the attached assets from the skeleton and mesh." ),
			FSlateIcon(), RemoveAllAttachedAssets );

		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}

void SSkeletonTree::FillVirtualBoneSubmenu(FMenuBuilder& MenuBuilder, TArray<TSharedPtr<FSkeletonTreeBoneItem>> SourceBones)
{
	const bool bShowVirtualBones = false;
	TSharedRef<SWidget> MenuContent = SNew(SBoneTreeMenu, GetEditableSkeletonInternal())
		.bShowVirtualBones(false)
		.Title(LOCTEXT("TargetBonePickerTitle", "Pick Target Bone..."))
		.OnBoneSelectionChanged(this, &SSkeletonTree::OnVirtualTargetBonePicked, SourceBones);
	MenuBuilder.AddWidget(MenuContent, FText::GetEmpty(), true);
}

void SSkeletonTree::OnVirtualTargetBonePicked(FName TargetBoneName, TArray<TSharedPtr<FSkeletonTreeBoneItem>> SourceBones)
{
	FSlateApplication::Get().DismissAllMenus();

	for (const TSharedPtr<FSkeletonTreeBoneItem>& SourceBone : SourceBones)
	{
		if(SourceBone.IsValid())
		{
			FName SourceBoneName = SourceBone->GetRowItemName();
			if(!GetEditableSkeletonInternal()->HandleAddVirtualBone(SourceBoneName, TargetBoneName))
			{
				UE_LOG(LogAnimation, Log, TEXT("Could not create space switch bone from %s to %s, it already exists"), *SourceBoneName.ToString(), *TargetBoneName.ToString());
			}
			else
			{
				CreateFromSkeleton();
			}
		}
	}

}

void SSkeletonTree::CreateMenuForBoneReduction(FMenuBuilder& MenuBuilder, SSkeletonTree * Widget, int32 LODIndex, bool bIncludeSelected)
{
	MenuBuilder.AddMenuEntry
		(FText::FromString(FString::Printf(TEXT("From LOD %d and below"), LODIndex))
		, FText::FromString(FString::Printf(TEXT("Remove Selected %s from current LOD %d and all lower LODs"), (bIncludeSelected) ? TEXT("bones") : TEXT("children"), LODIndex))
		, FSlateIcon()
		, FUIAction(FExecuteAction::CreateSP(Widget, &SSkeletonTree::RemoveFromLOD, LODIndex, bIncludeSelected, true))
		);

	MenuBuilder.AddMenuEntry
		(FText::FromString(FString::Printf(TEXT("From LOD %d only"), LODIndex))
		, FText::FromString(FString::Printf(TEXT("Remove selected %s from current LOD %d only"), (bIncludeSelected) ? TEXT("bones") : TEXT("children"), LODIndex))
		, FSlateIcon()
		, FUIAction(FExecuteAction::CreateSP(Widget, &SSkeletonTree::RemoveFromLOD, LODIndex, bIncludeSelected, false))
		);
}


void SSkeletonTree::SetBoneTranslationRetargetingModeRecursive(EBoneTranslationRetargetingMode::Type NewRetargetingMode)
{
	TArray<FName> BoneNames;
	FBoneTreeSelection TreeSelection(SkeletonTreeView->GetSelectedItems());
	for (const TSharedPtr<FSkeletonTreeBoneItem>& Item : TreeSelection.SelectedBones)
	{
		BoneNames.Add(Item->GetRowItemName());
	}

	GetEditableSkeletonInternal()->SetBoneTranslationRetargetingModeRecursive(BoneNames, NewRetargetingMode);
}

void SSkeletonTree::RemoveFromLOD(int32 LODIndex, bool bIncludeSelected, bool bIncludeBelowLODs)
{
	// we cant do this without a preview scene
	if (!GetPreviewScene().IsValid())
	{
		return;
	}

	UDebugSkelMeshComponent* PreviewMeshComponent = GetPreviewScene()->GetPreviewMeshComponent();
	if (!PreviewMeshComponent->SkeletalMesh)
	{
		return;
	}

	// ask users you can't undo this change, and warn them
	const FText Message(LOCTEXT("RemoveBonesFromLODWarning", "This action can't be undone. Would you like to continue?"));
	if (FMessageDialog::Open(EAppMsgType::YesNo, Message) == EAppReturnType::Yes)
	{
		FBoneTreeSelection TreeSelection(SkeletonTreeView->GetSelectedItems());
		const FReferenceSkeleton& RefSkeleton = GetEditableSkeletonInternal()->GetSkeleton().GetReferenceSkeleton();

		TArray<FName> BonesToRemove;

		for (const TSharedPtr<FSkeletonTreeBoneItem>& Item : TreeSelection.SelectedBones)
		{
			FName BoneName = Item->GetRowItemName();
			int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneName);
			if (BoneIndex != INDEX_NONE)
			{
				if (bIncludeSelected)
				{
					PreviewMeshComponent->SkeletalMesh->AddBoneToReductionSetting(LODIndex, BoneName);
					BonesToRemove.AddUnique(BoneName);
				}
				else
				{
					for (int32 ChildIndex = BoneIndex + 1; ChildIndex < RefSkeleton.GetRawBoneNum(); ++ChildIndex)
					{
						if (RefSkeleton.GetParentIndex(ChildIndex) == BoneIndex)
						{
							FName ChildBoneName = RefSkeleton.GetBoneName(ChildIndex);
							PreviewMeshComponent->SkeletalMesh->AddBoneToReductionSetting(LODIndex, ChildBoneName);
							BonesToRemove.AddUnique(ChildBoneName);
						}
					}
				}
			}
		}

		int32 TotalLOD = PreviewMeshComponent->SkeletalMesh->LODInfo.Num();
		IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");

		if (bIncludeBelowLODs)
		{
			for (int32 Index = LODIndex + 1; Index < TotalLOD; ++Index)
			{
				MeshUtilities.RemoveBonesFromMesh(PreviewMeshComponent->SkeletalMesh, Index, &BonesToRemove);
				PreviewMeshComponent->SkeletalMesh->AddBoneToReductionSetting(Index, BonesToRemove);
			}
		}

		// remove from current LOD
		MeshUtilities.RemoveBonesFromMesh(PreviewMeshComponent->SkeletalMesh, LODIndex, &BonesToRemove);
		// update UI to reflect the change
		OnLODSwitched();
	}
}

void SSkeletonTree::OnCopyBoneNames()
{
	FBoneTreeSelection TreeSelection(SkeletonTreeView->GetSelectedItems());

	if( TreeSelection.SelectedBones.Num() > 0 )
	{
		FString BoneNames;
		for (const TSharedPtr<FSkeletonTreeBoneItem>& Item : TreeSelection.SelectedBones)
		{
			FName BoneName = Item->GetRowItemName();

			BoneNames += BoneName.ToString();
			BoneNames += "\r\n";
		}
		FPlatformMisc::ClipboardCopy( *BoneNames );
	}
}

void SSkeletonTree::OnResetBoneTransforms()
{
	if (GetPreviewScene().IsValid())
	{
		UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();
		check(PreviewComponent);
		UAnimPreviewInstance* PreviewInstance = PreviewComponent->PreviewInstance;
		check(PreviewInstance);

		FBoneTreeSelection TreeSelection(SkeletonTreeView->GetSelectedItems());

		if (TreeSelection.SelectedBones.Num() > 0)
		{
			bool bModified = false;
			GEditor->BeginTransaction(LOCTEXT("SkeletonTree_ResetBoneTransforms", "Reset Bone Transforms"));

			for (const TSharedPtr<FSkeletonTreeBoneItem>& Item : TreeSelection.SelectedBones)
			{
				FName BoneName = Item->GetRowItemName();
				const FAnimNode_ModifyBone* ModifiedBone = PreviewInstance->FindModifiedBone(BoneName);
				if (ModifiedBone != nullptr)
				{
					if (!bModified)
					{
						PreviewInstance->SetFlags(RF_Transactional);
						PreviewInstance->Modify();
						bModified = true;
					}

					PreviewInstance->RemoveBoneModification(BoneName);
				}
			}

			GEditor->EndTransaction();
		}
	}
}

void SSkeletonTree::OnCopySockets() const
{
	FBoneTreeSelection TreeSelection(SkeletonTreeView->GetSelectedItems());

	int32 NumSocketsToCopy = TreeSelection.SelectedSockets.Num();
	if ( NumSocketsToCopy > 0 )
	{
		FString SocketsDataString;

		for (const TSharedPtr<FSkeletonTreeSocketItem>& Item : TreeSelection.SelectedSockets)
		{
			SocketsDataString += SerializeSocketToString(Item->GetSocket(), Item->GetParentType());
		}

		FString CopyString = FString::Printf( TEXT("%s\nNumSockets=%d\n%s"), *FEditableSkeleton::SocketCopyPasteHeader, NumSocketsToCopy, *SocketsDataString );

		FPlatformMisc::ClipboardCopy( *CopyString );
	}
}

FString SSkeletonTree::SerializeSocketToString( USkeletalMeshSocket* Socket, ESocketParentType ParentType) const
{
	FString SocketString;

	SocketString += FString::Printf( TEXT( "IsOnSkeleton=%s\n" ), ParentType == ESocketParentType::Skeleton ? TEXT( "1" ) : TEXT( "0" ) );

	FStringOutputDevice Buffer;
	const FExportObjectInnerContext Context;
	UExporter::ExportToOutputDevice( &Context, Socket, nullptr, Buffer, TEXT( "copy" ), 0, PPF_Copy, false );
	SocketString += Buffer;

	return SocketString;
}

void SSkeletonTree::OnPasteSockets(bool bPasteToSelectedBone)
{
	FBoneTreeSelection TreeSelection(SkeletonTreeView->GetSelectedItems());

	// Pasting sockets should only work if there is just one bone selected
	if ( TreeSelection.IsSingleOfTypeSelected<FSkeletonTreeBoneItem>() )
	{
		FName DestBoneName = bPasteToSelectedBone ? TreeSelection.GetSingleSelectedItem()->GetRowItemName() : NAME_None;
		USkeletalMesh* SkeletalMesh = GetPreviewScene().IsValid() ? GetPreviewScene()->GetPreviewMeshComponent()->SkeletalMesh : nullptr;
		GetEditableSkeletonInternal()->HandlePasteSockets(DestBoneName, SkeletalMesh);

		CreateFromSkeleton();
	}
}

void SSkeletonTree::OnAddSocket()
{
	// This adds a socket to the currently selected bone in the SKELETON, not the MESH.
	FBoneTreeSelection TreeSelection(SkeletonTreeView->GetSelectedItems());

	// Can only add a socket to one bone
	if (TreeSelection.IsSingleOfTypeSelected<FSkeletonTreeBoneItem>())
	{
		FName BoneName = TreeSelection.GetSingleSelectedItem()->GetRowItemName();
		USkeletalMeshSocket* NewSocket = GetEditableSkeletonInternal()->HandleAddSocket(BoneName);
		CreateFromSkeleton();

		FSelectedSocketInfo SocketInfo(NewSocket, true);
		SetSelectedSocket(SocketInfo);

		// now let us choose the socket name
		for (TSharedPtr<ISkeletonTreeItem>& Item : LinearItems)
		{
			if (Item->IsOfType<FSkeletonTreeSocketItem>())
			{
				if (Item->GetRowItemName() == NewSocket->SocketName)
				{
					OnRenameSocket();
					break;
				}
			}
		}
	}
}

void SSkeletonTree::OnCustomizeSocket()
{
	// This should only be called on a skeleton socket, it copies the 
	// socket to the mesh so the user can edit it separately
	FBoneTreeSelection TreeSelection(SkeletonTreeView->GetSelectedItems());

	if(TreeSelection.IsSingleOfTypeSelected<FSkeletonTreeSocketItem>())
	{
		USkeletalMeshSocket* SocketToCustomize = StaticCastSharedPtr<FSkeletonTreeSocketItem>(TreeSelection.GetSingleSelectedItem())->GetSocket();
		USkeletalMesh* SkeletalMesh = GetPreviewScene().IsValid() ? GetPreviewScene()->GetPreviewMeshComponent()->SkeletalMesh : nullptr;
		GetEditableSkeletonInternal()->HandleCustomizeSocket(SocketToCustomize, SkeletalMesh);
		CreateFromSkeleton();
	}
}

void SSkeletonTree::OnPromoteSocket()
{
	// This should only be called on a mesh socket, it copies the 
	// socket to the skeleton so all meshes can use it
	FBoneTreeSelection TreeSelection(SkeletonTreeView->GetSelectedItems());

	// Can only customize one socket (CreateContextMenu() should prevent this firing!)
	if(TreeSelection.IsSingleOfTypeSelected<FSkeletonTreeSocketItem>())
	{
		USkeletalMeshSocket* SocketToPromote = StaticCastSharedPtr<FSkeletonTreeSocketItem>(TreeSelection.GetSingleSelectedItem())->GetSocket();
		GetEditableSkeletonInternal()->HandlePromoteSocket(SocketToPromote);
		CreateFromSkeleton();
	}
}

void SSkeletonTree::FillAttachAssetSubmenu(FMenuBuilder& MenuBuilder, const TSharedPtr<ISkeletonTreeItem> TargetItem)
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	TArray<UClass*> FilterClasses = FComponentAssetBrokerage::GetSupportedAssets(USceneComponent::StaticClass());

	//Clean up the selection so it is relevant to Persona
	FilterClasses.RemoveSingleSwap(UBlueprint::StaticClass(), false); //Child actor components broker gives us blueprints which isn't wanted
	FilterClasses.RemoveSingleSwap(USoundBase::StaticClass(), false); //No sounds wanted

	FAssetPickerConfig AssetPickerConfig;
	AssetPickerConfig.Filter.bRecursiveClasses = true;

	for(int i = 0; i < FilterClasses.Num(); ++i)
	{
		AssetPickerConfig.Filter.ClassNames.Add(FilterClasses[i]->GetFName());
	}


	AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateSP(this, &SSkeletonTree::OnAssetSelectedFromPicker, TargetItem);

	TSharedRef<SWidget> MenuContent = SNew(SBox)
	.WidthOverride(384)
	.HeightOverride(500)
	[
		ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
	];
	MenuBuilder.AddWidget( MenuContent, FText::GetEmpty(), true);
}

void SSkeletonTree::OnAssetSelectedFromPicker(const FAssetData& AssetData, const TSharedPtr<ISkeletonTreeItem> TargetItem)
{
	FSlateApplication::Get().DismissAllMenus();
	TArray<FAssetData> Assets;
	Assets.Add(AssetData);

	AttachAssets(TargetItem.ToSharedRef(), Assets);
}

void  SSkeletonTree::OnRemoveAllAssets()
{
	GetEditableSkeletonInternal()->HandleRemoveAllAssets(GetPreviewScene());

	CreateFromSkeleton();
}

bool SSkeletonTree::CanRemoveAllAssets() const
{
	USkeletalMesh* SkeletalMesh = GetPreviewScene().IsValid() ? GetPreviewScene()->GetPreviewMeshComponent()->SkeletalMesh : nullptr;

	const bool bHasPreviewAttachedObjects = GetEditableSkeletonInternal()->GetSkeleton().PreviewAttachedAssetContainer.Num() > 0;
	const bool bHasMeshPreviewAttachedObjects = ( SkeletalMesh && SkeletalMesh->PreviewAttachedAssetContainer.Num() );

	return bHasPreviewAttachedObjects || bHasMeshPreviewAttachedObjects;
}

bool SSkeletonTree::CanRenameSelected() const
{
	FBoneTreeSelection TreeSelection(SkeletonTreeView->GetSelectedItems());
	return TreeSelection.IsSingleOfTypeSelected<FSkeletonTreeSocketItem>();
}

void SSkeletonTree::OnRenameSocket()
{
	FBoneTreeSelection TreeSelection(SkeletonTreeView->GetSelectedItems());

	if(TreeSelection.IsSingleOfTypeSelected<FSkeletonTreeSocketItem>())
	{
		SkeletonTreeView->RequestScrollIntoView(TreeSelection.GetSingleSelectedItem());
		DeferredRenameRequest = TreeSelection.GetSingleSelectedItem();
	}
}

void SSkeletonTree::OnSelectionChanged(TSharedPtr<ISkeletonTreeItem> Selection, ESelectInfo::Type SelectInfo)
{
	if( Selection.IsValid() )
	{
		//Get all the selected items
		FBoneTreeSelection TreeSelection(SkeletonTreeView->GetSelectedItems());

		if (GetPreviewScene().IsValid())
		{
			UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();
			if (TreeSelection.SelectedItems.Num() > 0 && PreviewComponent)
			{
				// pick the first settable bone from the selection
				for (auto ItemIt = TreeSelection.SelectedItems.CreateConstIterator(); ItemIt; ++ItemIt)
				{
					TSharedPtr<ISkeletonTreeItem> Item = *(ItemIt);

					// Test SelectInfo so we don't end up in an infinite loop due to delegates calling each other
					if (SelectInfo != ESelectInfo::Direct && (Item->IsOfType<FSkeletonTreeBoneItem>() || Item->IsOfType<FSkeletonTreeVirtualBoneItem>()))
					{
						FName BoneName = Item->GetRowItemName();

						// Get bone index
						int32 BoneIndex = PreviewComponent->GetBoneIndex(BoneName);
						if (BoneIndex != INDEX_NONE)
						{
							GetPreviewScene()->SetSelectedBone(BoneName);
							OnObjectSelectedMulticast.Broadcast(nullptr);
							break;
						}
					}
					// Test SelectInfo so we don't end up in an infinite loop due to delegates calling each other
					else if (SelectInfo != ESelectInfo::Direct && Item->IsOfType<FSkeletonTreeSocketItem>())
					{
						TSharedPtr<FSkeletonTreeSocketItem> SocketItem = StaticCastSharedPtr<FSkeletonTreeSocketItem>(Item);
						USkeletalMeshSocket* Socket = SocketItem->GetSocket();
						FSelectedSocketInfo SocketInfo(Socket, SocketItem->GetParentType() == ESocketParentType::Skeleton);
						GetPreviewScene()->SetSelectedSocket(SocketInfo);

						OnObjectSelectedMulticast.Broadcast(SocketInfo.Socket);
					}
					else if (Item->IsOfType<FSkeletonTreeAttachedAssetItem>())
					{
						GetPreviewScene()->DeselectAll();

						OnObjectSelectedMulticast.Broadcast(nullptr);
					}
				}
				PreviewComponent->PostInitMeshObject(PreviewComponent->MeshObject);
			}
		}
	}
	else
	{
		if (GetPreviewScene().IsValid())
		{
			// Tell the preview scene if the user ctrl-clicked the selected bone/socket to de-select it
			GetPreviewScene()->DeselectAll();
		}

		OnObjectSelectedMulticast.Broadcast(nullptr);
	}
}

void SSkeletonTree::AttachAssets(const TSharedRef<ISkeletonTreeItem>& TargetItem, const TArray<FAssetData>& AssetData)
{
	bool bAllAssetWereLoaded = true;
	TArray<UObject*> DroppedObjects;
	for (int32 AssetIdx = 0; AssetIdx < AssetData.Num(); ++AssetIdx)
	{
		UObject* Object = AssetData[AssetIdx].GetAsset();
		if ( Object != NULL )
		{
			DroppedObjects.Add( Object );
		}
		else
		{
			bAllAssetWereLoaded = false;
		}
	}

	if(bAllAssetWereLoaded)
	{
		FName AttachToName = TargetItem->GetAttachName();
		bool bAttachToMesh = TargetItem->IsOfType<FSkeletonTreeSocketItem>() &&
			StaticCastSharedRef<FSkeletonTreeSocketItem>(TargetItem)->GetParentType() == ESocketParentType::Mesh;
		
		GetEditableSkeletonInternal()->HandleAttachAssets(DroppedObjects, AttachToName, bAttachToMesh, GetPreviewScene());
		CreateFromSkeleton();
	}
}

void SSkeletonTree::OnItemScrolledIntoView( TSharedPtr<ISkeletonTreeItem> InItem, const TSharedPtr<ITableRow>& InWidget)
{
	if(DeferredRenameRequest.IsValid())
	{
		DeferredRenameRequest->RequestRename();
		DeferredRenameRequest.Reset();
	}
}

void SSkeletonTree::OnTreeDoubleClick( TSharedPtr<ISkeletonTreeItem> InItem )
{
	InItem->OnItemDoubleClicked();
}

void SSkeletonTree::SetTreeItemExpansionRecursive(TSharedPtr< ISkeletonTreeItem > TreeItem, bool bInExpansionState) const
{
	SkeletonTreeView->SetItemExpansion(TreeItem, bInExpansionState);

	// Recursively go through the children.
	for (auto It = TreeItem->GetChildren().CreateIterator(); It; ++It)
	{
		SetTreeItemExpansionRecursive(*It, bInExpansionState);
	}
}

void SSkeletonTree::PostUndo()
{
	// Rebuild the tree view whenever we undo a change to the skeleton
	CreateTreeColumns();

	if (GetPreviewScene().IsValid())
	{
		GetPreviewScene()->DeselectAll();
	}
}

void SSkeletonTree::OnFilterTextChanged( const FText& SearchText )
{
	FilterText = SearchText;

	ApplyFilter();
}

void SSkeletonTree::SetSkeletalMesh(USkeletalMesh* NewSkeletalMesh)
{
	if (GetPreviewScene().IsValid())
	{
		GetPreviewScene()->SetPreviewMesh(NewSkeletalMesh);
	}

	CreateFromSkeleton();
}

void SSkeletonTree::SetSelectedSocket( const FSelectedSocketInfo& SocketInfo )
{
	if (!bSelectingSocket)
	{
		TGuardValue<bool> RecursionGuard(bSelectingSocket, true);

		// This function is called when something else selects a socket (i.e. *NOT* the user clicking on a row in the treeview)
		// For example, this would be called if user clicked a socket hit point in the preview window

		// Firstly, find which row (if any) contains the socket requested
		for (auto SkeletonRowIt = LinearItems.CreateConstIterator(); SkeletonRowIt; ++SkeletonRowIt)
		{
			TSharedPtr<ISkeletonTreeItem> SkeletonRow = *(SkeletonRowIt);

			if (SkeletonRow->IsOfType<FSkeletonTreeSocketItem>() && StaticCastSharedPtr<FSkeletonTreeSocketItem>(SkeletonRow)->GetSocket() == SocketInfo.Socket)
			{
				SkeletonTreeView->SetSelection(SkeletonRow);
				SkeletonTreeView->RequestScrollIntoView(SkeletonRow);
			}
		}

		if (GetPreviewScene().IsValid())
		{
			GetPreviewScene()->SetSelectedSocket(SocketInfo);
		}

		OnObjectSelectedMulticast.Broadcast(SocketInfo.Socket);
	}
}

void SSkeletonTree::SetSelectedBone( const FName& BoneName )
{
	if (!bSelectingBone)
	{
		TGuardValue<bool> RecursionGuard(bSelectingBone, true);
		// This function is called when something else selects a bone (i.e. *NOT* the user clicking on a row in the treeview)
		// For example, this would be called if user clicked a bone hit point in the preview window

		// Find which row (if any) contains the bone requested
		for (auto SkeletonRowIt = LinearItems.CreateConstIterator(); SkeletonRowIt; ++SkeletonRowIt)
		{
			TSharedPtr<ISkeletonTreeItem> SkeletonRow = *(SkeletonRowIt);

			if (SkeletonRow->IsOfType<FSkeletonTreeBoneItem>() && SkeletonRow->GetRowItemName() == BoneName)
			{
				SkeletonTreeView->SetSelection(SkeletonRow);
				SkeletonTreeView->RequestScrollIntoView(SkeletonRow);
			}
		}

		if (GetPreviewScene().IsValid())
		{
			GetPreviewScene()->SetSelectedBone(BoneName);
		}

		OnObjectSelectedMulticast.Broadcast(nullptr);
	}
}

void SSkeletonTree::DeselectAll()
{
	if (!bDeselectingAll)
	{
		TGuardValue<bool> RecursionGuard(bDeselectingAll, true);
		SkeletonTreeView->ClearSelection();

		if (GetPreviewScene().IsValid())
		{
			GetPreviewScene()->DeselectAll();
		}

		OnObjectSelectedMulticast.Broadcast(nullptr);
	}
}

void SSkeletonTree::NotifyUser( FNotificationInfo& NotificationInfo )
{
	TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification( NotificationInfo );
	if ( Notification.IsValid() )
	{
		Notification->SetCompletionState( SNotificationItem::CS_Fail );
	}
}

TSharedRef< SWidget > SSkeletonTree::CreateFilterMenu()
{
	const FSkeletonTreeCommands& Actions = FSkeletonTreeCommands::Get();

	const bool CloseAfterSelection = true;
	FMenuBuilder MenuBuilder( CloseAfterSelection, UICommandList );

	MenuBuilder.BeginSection("Bones", LOCTEXT("OptionsMenuHeading", "Options"));
	{
		MenuBuilder.AddMenuEntry(Actions.ShowRetargeting);
		MenuBuilder.AddMenuEntry(Actions.FilteringFlattensHierarchy);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Bones", LOCTEXT( "BonesMenuHeading", "Bones" ) );
	{
		MenuBuilder.AddMenuEntry( Actions.ShowAllBones );
		MenuBuilder.AddMenuEntry( Actions.ShowMeshBones );
		MenuBuilder.AddMenuEntry(Actions.ShowLODBones);
		MenuBuilder.AddMenuEntry( Actions.ShowWeightedBones );
		MenuBuilder.AddMenuEntry( Actions.HideBones );
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Sockets", LOCTEXT("SocketsMenuHeading", "Sockets"));
	{
		MenuBuilder.AddMenuEntry(Actions.ShowActiveSockets);
		MenuBuilder.AddMenuEntry(Actions.ShowMeshSockets);
		MenuBuilder.AddMenuEntry(Actions.ShowSkeletonSockets);
		MenuBuilder.AddMenuEntry(Actions.ShowAllSockets);
		MenuBuilder.AddMenuEntry(Actions.HideSockets);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SSkeletonTree::SetBoneFilter( EBoneFilter InBoneFilter )
{
	check( InBoneFilter < EBoneFilter::Count );
	BoneFilter = InBoneFilter;

	ApplyFilter();
}

bool SSkeletonTree::IsBoneFilter( EBoneFilter InBoneFilter ) const
{
	return BoneFilter == InBoneFilter;
}

void SSkeletonTree::SetSocketFilter( ESocketFilter InSocketFilter )
{
	check( InSocketFilter < ESocketFilter::Count );
	SocketFilter = InSocketFilter;

	SetPreviewComponentSocketFilter();

	ApplyFilter();
}

void SSkeletonTree::SetPreviewComponentSocketFilter() const
{
	// Set the socket filter in the debug skeletal mesh component so the viewport can share the filter settings
	if (GetPreviewScene().IsValid())
	{
		UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();

		bool bAllOrActive = (SocketFilter == ESocketFilter::All || SocketFilter == ESocketFilter::Active);

		if (PreviewComponent)
		{
			PreviewComponent->bMeshSocketsVisible = bAllOrActive || SocketFilter == ESocketFilter::Mesh;
			PreviewComponent->bSkeletonSocketsVisible = bAllOrActive || SocketFilter == ESocketFilter::Skeleton;
		}
	}
}

bool SSkeletonTree::IsSocketFilter( ESocketFilter InSocketFilter ) const
{
	return SocketFilter == InSocketFilter;
}

FText SSkeletonTree::GetFilterMenuTitle() const
{
	FText BoneFilterMenuText;

	switch ( BoneFilter )
	{
	case EBoneFilter::All:
		BoneFilterMenuText = LOCTEXT( "BoneFilterMenuAll", "All Bones" );
		break;

	case EBoneFilter::Mesh:
		BoneFilterMenuText = LOCTEXT( "BoneFilterMenuMesh", "Mesh Bones" );
		break;

	case EBoneFilter::LOD:
		BoneFilterMenuText = LOCTEXT("BoneFilterMenuLOD", "LOD Bones");
		break;

	case EBoneFilter::Weighted:
		BoneFilterMenuText = LOCTEXT( "BoneFilterMenuWeighted", "Weighted Bones" );
		break;

	case EBoneFilter::None:
		BoneFilterMenuText = LOCTEXT( "BoneFilterMenuHidden", "Bones Hidden" );
		break;

	default:
		// Unknown mode
		check( 0 );
		break;
	}

	FText SocketFilterMenuText;

	switch (SocketFilter)
	{
	case ESocketFilter::Active:
		SocketFilterMenuText = LOCTEXT("SocketFilterMenuActive", "Active Sockets");
		break;

	case ESocketFilter::Mesh:
		SocketFilterMenuText = LOCTEXT("SocketFilterMenuMesh", "Mesh Sockets");
		break;

	case ESocketFilter::Skeleton:
		SocketFilterMenuText = LOCTEXT("SocketFilterMenuSkeleton", "Skeleton Sockets");
		break;

	case ESocketFilter::All:
		SocketFilterMenuText = LOCTEXT("SocketFilterMenuAll", "All Sockets");
		break;

	case ESocketFilter::None:
		SocketFilterMenuText = LOCTEXT("SocketFilterMenuHidden", "Sockets Hidden");
		break;

	default:
		// Unknown mode
		check(0);
		break;
	}

	return FText::Format(LOCTEXT("FilterMenuLabelFormat", "{0}, {1}"), BoneFilterMenuText, SocketFilterMenuText);
}

bool SSkeletonTree::IsAddingSocketsAllowed() const
{
	if ( SocketFilter == ESocketFilter::Skeleton ||
		SocketFilter == ESocketFilter::Active ||
		SocketFilter == ESocketFilter::All )
	{
		return true;
	}

	return false;
}

FReply SSkeletonTree::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	if ( UICommandList->ProcessCommandBindings( InKeyEvent ) )
	{
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SSkeletonTree::OnDeleteSelectedRows()
{
	FBoneTreeSelection TreeSelection(SkeletonTreeView->GetSelectedItems());

	if(TreeSelection.HasSelectedOfType<FSkeletonTreeAttachedAssetItem>() || TreeSelection.HasSelectedOfType<FSkeletonTreeSocketItem>() || TreeSelection.HasSelectedOfType<FSkeletonTreeVirtualBoneItem>())
	{
		FScopedTransaction Transaction( LOCTEXT( "SkeletonTreeDeleteSelected", "Delete selected sockets/meshes/bones from skeleton tree" ) );

		DeleteAttachedAssets( TreeSelection.SelectedAssets );
		DeleteSockets( TreeSelection.SelectedSockets );
		DeleteVirtualBones( TreeSelection.SelectedVirtualBones );

		CreateFromSkeleton();
	}
}

void SSkeletonTree::DeleteAttachedAssets(const TArray<TSharedPtr<FSkeletonTreeAttachedAssetItem>>& InDisplayedAttachedAssetInfos)
{
	DeselectAll();

	TArray<FPreviewAttachedObjectPair> AttachedObjects;
	for(const TSharedPtr<FSkeletonTreeAttachedAssetItem>& AttachedAssetInfo : InDisplayedAttachedAssetInfos)
	{
		FPreviewAttachedObjectPair Pair;
		Pair.SetAttachedObject(AttachedAssetInfo->GetAsset());
		Pair.AttachedTo = AttachedAssetInfo->GetParentName();

		AttachedObjects.Add(Pair);
	}

	GetEditableSkeletonInternal()->HandleDeleteAttachedAssets(AttachedObjects, GetPreviewScene());
}

void SSkeletonTree::DeleteSockets(const TArray<TSharedPtr<FSkeletonTreeSocketItem>>& InDisplayedSocketInfos)
{
	DeselectAll();

	TArray<FSelectedSocketInfo> SocketInfo;

	for (const TSharedPtr<FSkeletonTreeSocketItem>& DisplayedSocketInfo : InDisplayedSocketInfos)
	{
		USkeletalMeshSocket* SocketToDelete = DisplayedSocketInfo->GetSocket();
		SocketInfo.Add(FSelectedSocketInfo(SocketToDelete, DisplayedSocketInfo->GetParentType() == ESocketParentType::Skeleton));
	}

	GetEditableSkeletonInternal()->HandleDeleteSockets(SocketInfo, GetPreviewScene());
}

void SSkeletonTree::DeleteVirtualBones(const TArray<TSharedPtr<FSkeletonTreeVirtualBoneItem>>& InDisplayedVirtualBoneInfos)
{
	DeselectAll();

	TArray<FName> VirtualBoneInfo;

	for (const TSharedPtr<FSkeletonTreeVirtualBoneItem>& DisplayedVirtualBoneInfo : InDisplayedVirtualBoneInfos)
	{
		VirtualBoneInfo.Add(DisplayedVirtualBoneInfo->GetRowItemName());
	}

	GetEditableSkeletonInternal()->HandleDeleteVirtualBones(VirtualBoneInfo, GetPreviewScene());
}

void SSkeletonTree::OnChangeShowingAdvancedOptions()
{
	bShowingAdvancedOptions = !bShowingAdvancedOptions;
	CreateTreeColumns();
}

bool SSkeletonTree::IsShowingAdvancedOptions() const
{
	return bShowingAdvancedOptions;
}

UBlendProfile* SSkeletonTree::GetSelectedBlendProfile()
{
	return BlendProfilePicker->GetSelectedBlendProfile();
}

FName SSkeletonTree::GetSelectedBlendProfileName() const
{
	return BlendProfilePicker->GetSelectedBlendProfileName();
}

void SSkeletonTree::OnBlendProfileSelected(UBlendProfile* NewProfile)
{
	CreateTreeColumns();
}

void SSkeletonTree::RecursiveSetBlendProfileScales(float InScaleToSet)
{
	UBlendProfile* SelectedBlendProfile = BlendProfilePicker->GetSelectedBlendProfile();
	if(SelectedBlendProfile)
	{
		TArray<FName> BoneNames;
		FBoneTreeSelection TreeSelection(SkeletonTreeView->GetSelectedItems());

		for(TSharedPtr<FSkeletonTreeBoneItem>& SelectedBone : TreeSelection.SelectedBones)
		{
			BoneNames.Add(SelectedBone->GetRowItemName());
		}

		GetEditableSkeletonInternal()->RecursiveSetBlendProfileScales(SelectedBlendProfile->GetFName(), BoneNames, InScaleToSet);

		CreateTreeColumns();
	}
}

void SSkeletonTree::HandleTreeRefresh()
{
	CreateFromSkeleton();
}

void SSkeletonTree::PostRenameSocket(UObject* InAttachedObject, const FName& InOldName, const FName& InNewName)
{
	TSharedPtr<IPersonaPreviewScene> LinkedPreviewScene = GetPreviewScene();
	if (LinkedPreviewScene.IsValid())
	{
		LinkedPreviewScene->RemoveAttachedObjectFromPreviewComponent(InAttachedObject, InOldName);
		LinkedPreviewScene->AttachObjectToPreviewComponent(InAttachedObject, InNewName);
	}
}

void SSkeletonTree::PostDuplicateSocket(UObject* InAttachedObject, const FName& InSocketName)
{
	TSharedPtr<IPersonaPreviewScene> LinkedPreviewScene = GetPreviewScene();
	if (LinkedPreviewScene.IsValid())
	{
		LinkedPreviewScene->AttachObjectToPreviewComponent(InAttachedObject, InSocketName);
	}

	CreateFromSkeleton();
}

void RecursiveSetLODChange(UDebugSkelMeshComponent* PreviewComponent, TSharedPtr<ISkeletonTreeItem> TreeRow)
{
	if (TreeRow->IsOfType<FSkeletonTreeBoneItem>())
	{
		StaticCastSharedPtr<FSkeletonTreeBoneItem>(TreeRow)->CacheLODChange(PreviewComponent);
	}
	
	for (auto& Child : TreeRow->GetChildren())
	{
		RecursiveSetLODChange(PreviewComponent, Child);
	}
}

void SSkeletonTree::OnLODSwitched()
{
	if (GetPreviewScene().IsValid())
	{
		UDebugSkelMeshComponent* PreviewComponent = GetPreviewScene()->GetPreviewMeshComponent();

		if (PreviewComponent)
		{
			LastCachedLODForPreviewMeshComponent = PreviewComponent->PredictedLODLevel;

			if (BoneFilter == EBoneFilter::Weighted || BoneFilter == EBoneFilter::LOD)
			{
				CreateFromSkeleton();
			}
			else
			{
				for (TSharedPtr<ISkeletonTreeItem>& Item : Items)
				{
					RecursiveSetLODChange(PreviewComponent, Item);
				}
			}
		}
	}
}

void SSkeletonTree::DuplicateAndSelectSocket(const FSelectedSocketInfo& SocketInfoToDuplicate, const FName& NewParentBoneName /*= FName()*/)
{
	USkeletalMesh* SkeletalMesh = GetPreviewScene().IsValid() ? GetPreviewScene()->GetPreviewMeshComponent()->SkeletalMesh : nullptr;
	USkeletalMeshSocket* NewSocket = GetEditableSkeleton()->DuplicateSocket(SocketInfoToDuplicate, NewParentBoneName, SkeletalMesh);

	if (GetPreviewScene().IsValid())
	{
		GetPreviewScene()->SetSelectedSocket(FSelectedSocketInfo(NewSocket, SocketInfoToDuplicate.bSocketIsOnSkeleton));
	}

	CreateFromSkeleton();

	FSelectedSocketInfo NewSocketInfo(NewSocket, SocketInfoToDuplicate.bSocketIsOnSkeleton);
	SetSelectedSocket(NewSocketInfo);
}

FSlateColor SSkeletonTree::GetFilterComboButtonForegroundColor() const
{
	static const FName InvertedForegroundName("InvertedForeground");
	static const FName DefaultForegroundName("DefaultForeground");

	if (FilterComboButton.IsValid())
	{
		return FilterComboButton->IsHovered() ? FEditorStyle::GetSlateColor(InvertedForegroundName) : FEditorStyle::GetSlateColor(DefaultForegroundName);
	}
	return FSlateColor::UseForeground();
}

void SSkeletonTree::HandleFocusCamera()
{
	if (GetPreviewScene().IsValid())
	{
		GetPreviewScene()->FocusViews();
	}
}

/** Filter utility class */
class FSkeletonTreeFilterContext : public ITextFilterExpressionContext
{
public:
	explicit FSkeletonTreeFilterContext(const FName& InName)
		: Name(InName)
	{
	}

	virtual bool TestBasicStringExpression(const FTextFilterString& InValue, const ETextFilterTextComparisonMode InTextComparisonMode) const override
	{
		return TextFilterUtils::TestBasicStringExpression(Name.ToString(), InValue, InTextComparisonMode);
	}

	virtual bool TestComplexExpression(const FName& InKey, const FTextFilterString& InValue, const ETextFilterComparisonOperation InComparisonOperation, const ETextFilterTextComparisonMode InTextComparisonMode) const override
	{
		return false;
	}

private:
	FName Name;
};

ESkeletonTreeFilterResult SSkeletonTree::HandleFilterSkeletonTreeItem(const TSharedPtr<class ISkeletonTreeItem>& InItem)
{
	ESkeletonTreeFilterResult Result = ESkeletonTreeFilterResult::Shown;

	if (!FilterText.IsEmpty())
	{
		if (TextFilterPtr->TestTextFilter(FSkeletonTreeFilterContext(InItem->GetRowItemName())))
		{
			Result = ESkeletonTreeFilterResult::ShownHighlighted;
		}
		else
		{
			Result = ESkeletonTreeFilterResult::Hidden;
		}
	}


	if (InItem->IsOfType<FSkeletonTreeBoneItem>())
	{
		TSharedPtr<FSkeletonTreeBoneItem> BoneItem = StaticCastSharedPtr<FSkeletonTreeBoneItem>(InItem);

		if (BoneFilter == EBoneFilter::None)
		{
			Result = ESkeletonTreeFilterResult::Hidden;
		}
		else if (GetPreviewScene().IsValid())
		{
			UDebugSkelMeshComponent* PreviewMeshComponent = GetPreviewScene()->GetPreviewMeshComponent();
			if (PreviewMeshComponent)
			{
				int32 BoneMeshIndex = PreviewMeshComponent->GetBoneIndex(BoneItem->GetRowItemName());

				// Remove non-mesh bones if we're filtering
				if ((BoneFilter == EBoneFilter::Mesh || BoneFilter == EBoneFilter::Weighted || BoneFilter == EBoneFilter::LOD) &&
					BoneMeshIndex == INDEX_NONE)
				{
					Result = ESkeletonTreeFilterResult::Hidden;
				}

				// Remove non-vertex-weighted bones if we're filtering
				if (BoneFilter == EBoneFilter::Weighted && !BoneItem->IsBoneWeighted(BoneMeshIndex, PreviewMeshComponent))
				{
					Result = ESkeletonTreeFilterResult::Hidden;
				}

				// Remove non-vertex-weighted bones if we're filtering
				if (BoneFilter == EBoneFilter::LOD && !BoneItem->IsBoneRequired(BoneMeshIndex, PreviewMeshComponent))
				{
					Result = ESkeletonTreeFilterResult::Hidden;
				}
			}
		}
	}
	else if (InItem->IsOfType<FSkeletonTreeSocketItem>())
	{
		TSharedPtr<FSkeletonTreeSocketItem> SocketItem = StaticCastSharedPtr<FSkeletonTreeSocketItem>(InItem);

		if (SocketFilter == ESocketFilter::None)
		{
			Result = ESkeletonTreeFilterResult::Hidden;
		}

		// Remove non-mesh sockets if we're filtering
		if ((SocketFilter == ESocketFilter::Mesh || SocketFilter == ESocketFilter::None) && SocketItem->GetParentType() == ESocketParentType::Skeleton)
		{
			Result = ESkeletonTreeFilterResult::Hidden;
		}

		// Remove non-skeleton sockets if we're filtering
		if ((SocketFilter == ESocketFilter::Skeleton || SocketFilter == ESocketFilter::None) && SocketItem->GetParentType() == ESocketParentType::Mesh)
		{
			Result = ESkeletonTreeFilterResult::Hidden;
		}

		if (SocketFilter == ESocketFilter::Active && SocketItem->GetParentType() == ESocketParentType::Mesh && SocketItem->IsSocketCustomized())
		{
			// Don't add the skeleton socket if it's already added for the mesh
			Result = ESkeletonTreeFilterResult::Hidden;
		}
	}

	return Result;
}

#undef LOCTEXT_NAMESPACE
