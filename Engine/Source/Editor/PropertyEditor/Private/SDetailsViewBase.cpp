// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PropertyEditorPrivatePCH.h"
#include "SDetailsViewBase.h"
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
#include "DetailPropertyRow.h"
#include "SSearchBox.h"


SDetailsViewBase::~SDetailsViewBase()
{
	if (ThumbnailPool.IsValid())
	{
		ThumbnailPool->ReleaseResources();
	}
}


void SDetailsViewBase::OnGetChildrenForDetailTree(TSharedRef<IDetailTreeNode> InTreeNode, TArray< TSharedRef<IDetailTreeNode> >& OutChildren)
{
	InTreeNode->GetChildren(OutChildren);
}

TSharedRef<ITableRow> SDetailsViewBase::OnGenerateRowForDetailTree(TSharedRef<IDetailTreeNode> InTreeNode, const TSharedRef<STableViewBase>& OwnerTable)
{
	return InTreeNode->GenerateNodeWidget(OwnerTable, ColumnSizeData, PropertyUtilities.ToSharedRef(), DetailsViewArgs.bAllowFavoriteSystem);
}

void SDetailsViewBase::SetRootExpansionStates(const bool bExpand, const bool bRecurse)
{
	for (auto Iter = RootTreeNodes.CreateIterator(); Iter; ++Iter)
	{
		SetNodeExpansionState(*Iter, bExpand, bRecurse);
	}
}

void SDetailsViewBase::SetNodeExpansionState(TSharedRef<IDetailTreeNode> InTreeNode, bool bIsItemExpanded, bool bRecursive)
{
	TArray< TSharedRef<IDetailTreeNode> > Children;
	InTreeNode->GetChildren(Children);

	if (Children.Num())
	{
		RequestItemExpanded(InTreeNode, bIsItemExpanded);
		InTreeNode->OnItemExpansionChanged(bIsItemExpanded);

		if (bRecursive)
		{
			for (int32 ChildIndex = 0; ChildIndex < Children.Num(); ++ChildIndex)
			{
				TSharedRef<IDetailTreeNode> Child = Children[ChildIndex];

				SetNodeExpansionState(Child, bIsItemExpanded, bRecursive);
			}
		}
	}
}

void SDetailsViewBase::SetNodeExpansionStateRecursive(TSharedRef<IDetailTreeNode> InTreeNode, bool bIsItemExpanded)
{
	SetNodeExpansionState(InTreeNode, bIsItemExpanded, true);
}

void SDetailsViewBase::OnItemExpansionChanged(TSharedRef<IDetailTreeNode> InTreeNode, bool bIsItemExpanded)
{
	SetNodeExpansionState(InTreeNode, bIsItemExpanded, false);
}

FReply SDetailsViewBase::OnLockButtonClicked()
{
	bIsLocked = !bIsLocked;
	return FReply::Handled();
}

void SDetailsViewBase::HideFilterArea(bool bHide)
{
	DetailsViewArgs.bAllowSearch = !bHide;
}

static void GetPropertiesInOrderDisplayedRecursive(const TArray< TSharedRef<IDetailTreeNode> >& TreeNodes, TArray< FPropertyPath > &OutLeaves)
{
	for (auto& TreeNode : TreeNodes)
	{
		if (TreeNode->IsLeaf())
		{
			FPropertyPath Path = TreeNode->GetPropertyPath();
			// Some leaf nodes are not associated with properties, specifically the collision presets.
			// @todo doc: investigate what we can do about this, result is that for these fields
			// we can't highlight hte property in the diff tool.
			if( Path.GetNumProperties() != 0 )
			{
				OutLeaves.Push(Path);
			}
		}
		else
		{
			TArray< TSharedRef<IDetailTreeNode> > Children;
			TreeNode->GetChildren(Children);
			GetPropertiesInOrderDisplayedRecursive(Children, OutLeaves);
		}
	}
}
TArray< FPropertyPath > SDetailsViewBase::GetPropertiesInOrderDisplayed() const
{
	TArray< FPropertyPath > Ret;
	GetPropertiesInOrderDisplayedRecursive(RootTreeNodes, Ret);
	return Ret;
}

static TSharedPtr< IDetailTreeNode > FindTreeNodeFromPropertyRecursive( const TArray< TSharedRef<IDetailTreeNode> >& Nodes, const FPropertyPath& Property )
{
	for (auto& TreeNode : Nodes)
	{
		if (TreeNode->IsLeaf())
		{
			FPropertyPath tmp = TreeNode->GetPropertyPath();
			if( Property == tmp )
			{
				return TreeNode;
			}
		}
		else
		{
			TArray< TSharedRef<IDetailTreeNode> > Children;
			TreeNode->GetChildren(Children);
			auto Result = FindTreeNodeFromPropertyRecursive(Children, Property);
			if( Result.IsValid() )
			{
				return Result;
			}
		}
	}

	return TSharedPtr< IDetailTreeNode >();
}

void SDetailsViewBase::HighlightProperty(const FPropertyPath& Property)
{
	auto PrevHighlightedNodePtr = CurrentlyHighlightedNode.Pin();
	if (PrevHighlightedNodePtr.IsValid())
	{
		PrevHighlightedNodePtr->SetIsHighlighted(false);
	}

	auto NextNodePtr = FindTreeNodeFromPropertyRecursive( RootTreeNodes, Property );
	if (NextNodePtr.IsValid())
	{
		NextNodePtr->SetIsHighlighted(true);
		auto ParentCategory = NextNodePtr->GetParentCategory();
		if (ParentCategory.IsValid())
		{
			DetailTree->SetItemExpansion(ParentCategory.ToSharedRef(), true);
		}
		DetailTree->RequestScrollIntoView(NextNodePtr.ToSharedRef());
	}
	CurrentlyHighlightedNode = NextNodePtr;
}

void SDetailsViewBase::ShowAllAdvancedProperties()
{
	CurrentFilter.bShowAllAdvanced = true;
}

void SDetailsViewBase::SetOnDisplayedPropertiesChanged(FOnDisplayedPropertiesChanged InOnDisplayedPropertiesChangedDelegate)
{
	OnDisplayedPropertiesChangedDelegate = InOnDisplayedPropertiesChangedDelegate;
}

void SDetailsViewBase::RerunCurrentFilter()
{
	UpdateFilteredDetails();
}

EVisibility SDetailsViewBase::GetTreeVisibility() const
{
	for(const FDetailLayoutData& Data : DetailLayouts)
	{
		if(Data.DetailLayout.IsValid() && Data.DetailLayout->HasDetails())
		{
			return EVisibility::Visible;
		}
	}

	return EVisibility::Collapsed;
}

/** Returns the image used for the icon on the filter button */
const FSlateBrush* SDetailsViewBase::OnGetFilterButtonImageResource() const
{
	if (bHasActiveFilter)
	{
		return FEditorStyle::GetBrush(TEXT("PropertyWindow.FilterCancel"));
	}
	else
	{
		return FEditorStyle::GetBrush(TEXT("PropertyWindow.FilterSearch"));
	}
}

void SDetailsViewBase::EnqueueDeferredAction(FSimpleDelegate& DeferredAction)
{
	DeferredActions.Add(DeferredAction);
}

/**
* Creates the color picker window for this property view.
*
* @param Node				The slate property node to edit.
* @param bUseAlpha			Whether or not alpha is supported
*/
void SDetailsViewBase::CreateColorPickerWindow(const TSharedRef< FPropertyEditor >& PropertyEditor, bool bUseAlpha)
{
	const TSharedRef< FPropertyNode > PinnedColorPropertyNode = PropertyEditor->GetPropertyNode();
	ColorPropertyNode = PinnedColorPropertyNode;

	UProperty* Property = PinnedColorPropertyNode->GetProperty();
	check(Property);

	FReadAddressList ReadAddresses;
	PinnedColorPropertyNode->GetReadAddress(false, ReadAddresses, false);

	TArray<FLinearColor*> LinearColor;
	TArray<FColor*> DWORDColor;
	for (int32 ColorIndex = 0; ColorIndex < ReadAddresses.Num(); ++ColorIndex)
	{
		const uint8* Addr = ReadAddresses.GetAddress(ColorIndex);
		if (Addr)
		{
			if (Cast<UStructProperty>(Property)->Struct->GetFName() == NAME_Color)
			{
				DWORDColor.Add((FColor*)Addr);
			}
			else
			{
				check(Cast<UStructProperty>(Property)->Struct->GetFName() == NAME_LinearColor);
				LinearColor.Add((FLinearColor*)Addr);
			}
		}
	}

	bHasOpenColorPicker = true;

	FColorPickerArgs PickerArgs;
	PickerArgs.ParentWidget = AsShared();
	PickerArgs.bUseAlpha = bUseAlpha;
	PickerArgs.DisplayGamma = TAttribute<float>::Create(TAttribute<float>::FGetter::CreateUObject(GEngine, &UEngine::GetDisplayGamma));
	PickerArgs.ColorArray = &DWORDColor;
	PickerArgs.LinearColorArray = &LinearColor;
	PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateSP(this, &SDetailsViewBase::SetColorPropertyFromColorPicker);
	PickerArgs.OnColorPickerWindowClosed = FOnWindowClosed::CreateSP(this, &SDetailsViewBase::OnColorPickerWindowClosed);

	OpenColorPicker(PickerArgs);
}

void SDetailsViewBase::SetColorPropertyFromColorPicker(FLinearColor NewColor)
{
	const TSharedPtr< FPropertyNode > PinnedColorPropertyNode = ColorPropertyNode.Pin();
	if (ensure(PinnedColorPropertyNode.IsValid()))
	{
		UProperty* Property = PinnedColorPropertyNode->GetProperty();
		check(Property);

		FObjectPropertyNode* ObjectNode = PinnedColorPropertyNode->FindObjectItemParent();

		if (ObjectNode && ObjectNode->GetNumObjects())
		{
			FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "SetColorProperty", "Set Color Property"));

			PinnedColorPropertyNode->NotifyPreChange(Property, GetNotifyHook());

			FPropertyChangedEvent ChangeEvent(Property, EPropertyChangeType::ValueSet);
			PinnedColorPropertyNode->NotifyPostChange(ChangeEvent, GetNotifyHook());
		}
	}
}

/**
* Determines whether or not a property should be visible in the default generated detail layout
*
* @param PropertyNode	The property node to check
* @param ParentNode	The parent property node to check
* @return true if the property should be visible
*/
static bool IsVisibleStandaloneProperty(const FPropertyNode& PropertyNode, const FPropertyNode& ParentNode)
{
	const UProperty* Property = PropertyNode.GetProperty();
	const UArrayProperty* ParentArrayProperty = Cast<const UArrayProperty>(ParentNode.GetProperty());
	bool bIsVisibleStandalone = false;
	if(Property)
	{
		if(Property->IsA(UObjectPropertyBase::StaticClass()))
		{
			// Do not add this child node to the current map if its a single object property in a category (serves no purpose for UI)
			bIsVisibleStandalone = !ParentArrayProperty && (PropertyNode.GetNumChildNodes() == 0 || PropertyNode.GetNumChildNodes() > 1);
		}
		else if(Property->IsA(UArrayProperty::StaticClass()) || (Property->ArrayDim > 1 && PropertyNode.GetArrayIndex() == INDEX_NONE))
		{
			// Base array properties are always visible
			bIsVisibleStandalone = true;
		}
		else
		{
			bIsVisibleStandalone = true;
		}

	}

	return bIsVisibleStandalone;
}

void SDetailsViewBase::UpdatePropertyMaps()
{
	RootTreeNodes.Empty();

	for(FDetailLayoutData& LayoutData : DetailLayouts)
	{
		// Check uniqueness.  It is critical that detail layouts can be destroyed
		// We need to be able to create a new detail layout and properly clean up the old one in the process
		check(!LayoutData.DetailLayout.IsValid() || LayoutData.DetailLayout.IsUnique());

		// All the current customization instances need to be deleted when it is safe
		CustomizationClassInstancesPendingDelete.Append(LayoutData.CustomizationClassInstances);
	}

	FRootPropertyNodeList& RootPropertyNodes = GetRootNodes();
	
	DetailLayouts.Empty(RootPropertyNodes.Num());

	// There should be one detail layout for each root node
	DetailLayouts.AddDefaulted(RootPropertyNodes.Num());

	for(int32 RootNodeIndex = 0; RootNodeIndex < RootPropertyNodes.Num(); ++RootNodeIndex)
	{
		FDetailLayoutData& LayoutData = DetailLayouts[RootNodeIndex];
		UpdateSinglePropertyMap(RootPropertyNodes[RootNodeIndex], LayoutData);
	}
}

void SDetailsViewBase::UpdateSinglePropertyMap(TSharedPtr<FComplexPropertyNode>& InRootPropertyNode, FDetailLayoutData& LayoutData)
{
	// Reset everything
	LayoutData.ClassToPropertyMap.Empty();

	TSharedPtr<FDetailLayoutBuilderImpl> DetailLayout = MakeShareable(new FDetailLayoutBuilderImpl(InRootPropertyNode, LayoutData.ClassToPropertyMap, PropertyUtilities.ToSharedRef(), SharedThis(this)));
	LayoutData.DetailLayout = DetailLayout;

	TSharedPtr<FComplexPropertyNode> RootPropertyNode = InRootPropertyNode;
	check(RootPropertyNode.IsValid());

	bool const bEnableFavoriteSystem = GIsRequestingExit ? false : (GetDefault<UEditorExperimentalSettings>()->bEnableFavoriteSystem && DetailsViewArgs.bAllowFavoriteSystem);

	// Currently object property nodes do not provide any useful information other than being a container for its children.  We do not draw anything for them.
	// When we encounter object property nodes, add their children instead of adding them to the tree.
	UpdateSinglePropertyMapRecursive(*RootPropertyNode, LayoutData, NAME_None, RootPropertyNode.Get(), bEnableFavoriteSystem, false);

	CustomUpdatePropertyMap(LayoutData.DetailLayout);

	// Ask for custom detail layouts, unless disabled. One reason for disabling custom layouts is that the custom layouts
	// inhibit our ability to find a single property's tree node. This is problematic for the diff and merge tools, that need
	// to display and highlight each changed property for the user. We could whitelist 'good' customizations here if 
	// we can make them work with the diff/merge tools.
	if(!bDisableCustomDetailLayouts)
	{
		QueryCustomDetailLayout(LayoutData);
	}

	LayoutData.DetailLayout->GenerateDetailLayout();
}

void SDetailsViewBase::UpdateSinglePropertyMapRecursive(FPropertyNode& InNode, FDetailLayoutData& LayoutData, FName CurCategory, FComplexPropertyNode* CurObjectNode, bool bEnableFavoriteSystem, bool bUpdateFavoriteSystemOnly)
{
	FDetailLayoutBuilderImpl& DetailLayout = *LayoutData.DetailLayout;

	UProperty* ParentProperty = InNode.GetProperty();
	UStructProperty* ParentStructProp = Cast<UStructProperty>(ParentProperty);
	for(int32 ChildIndex = 0; ChildIndex < InNode.GetNumChildNodes(); ++ChildIndex)
	{
		//Use the original value for each child
		bool LocalUpdateFavoriteSystemOnly = bUpdateFavoriteSystemOnly;

		TSharedPtr<FPropertyNode> ChildNodePtr = InNode.GetChildNode(ChildIndex);
		FPropertyNode& ChildNode = *ChildNodePtr;
		UProperty* Property = ChildNode.GetProperty();

		{
			FObjectPropertyNode* ObjNode = ChildNode.AsObjectNode();
			FCategoryPropertyNode* CategoryNode = ChildNode.AsCategoryNode();
			if(ObjNode)
			{
				// Currently object property nodes do not provide any useful information other than being a container for its children.  We do not draw anything for them.
				// When we encounter object property nodes, add their children instead of adding them to the tree.
				UpdateSinglePropertyMapRecursive(ChildNode, LayoutData, CurCategory, ObjNode, bEnableFavoriteSystem, LocalUpdateFavoriteSystemOnly);
			}
			else if(CategoryNode)
			{
				if(!LocalUpdateFavoriteSystemOnly)
				{
					FName InstanceName = NAME_None;
					FName CategoryName = CurCategory;
					FString CategoryDelimiterString;
					CategoryDelimiterString.AppendChar(FPropertyNodeConstants::CategoryDelimiterChar);
					if(CurCategory != NAME_None && CategoryNode->GetCategoryName().ToString().Contains(CategoryDelimiterString))
					{
						// This property is child of another property so add it to the parent detail category
						FDetailCategoryImpl& CategoryImpl = DetailLayout.DefaultCategory(CategoryName);
						CategoryImpl.AddPropertyNode(ChildNodePtr.ToSharedRef(), InstanceName);
					}
				}

				// For category nodes, we just set the current category and recurse through the children
				UpdateSinglePropertyMapRecursive(ChildNode, LayoutData, CategoryNode->GetCategoryName(), CurObjectNode, bEnableFavoriteSystem, LocalUpdateFavoriteSystemOnly);
			}
			else
			{
				// Whether or not the property can be visible in the default detail layout
				bool bVisibleByDefault = IsVisibleStandaloneProperty(ChildNode, InNode);

				// Whether or not the property is a struct
				UStructProperty* StructProperty = Cast<UStructProperty>(Property);

				bool bIsStruct = StructProperty != NULL;

				static FName ShowOnlyInners("ShowOnlyInnerProperties");

				bool bIsChildOfCustomizedStruct = false;
				bool bIsCustomizedStruct = false;

				const UStruct* Struct = StructProperty ? StructProperty->Struct : NULL;
				const UStruct* ParentStruct = ParentStructProp ? ParentStructProp->Struct : NULL;
				if(Struct || ParentStruct)
				{
					FPropertyEditorModule& ParentPlugin = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
					if(Struct)
					{
						bIsCustomizedStruct = ParentPlugin.IsCustomizedStruct(Struct, SharedThis(this));
					}

					if(ParentStruct)
					{
						bIsChildOfCustomizedStruct = ParentPlugin.IsCustomizedStruct(ParentStruct, SharedThis(this));
					}
				}

				// Whether or not to push out struct properties to their own categories or show them inside an expandable struct 
				bool bPushOutStructProps = bIsStruct && !bIsCustomizedStruct && !ParentStructProp && Property->HasMetaData(ShowOnlyInners);

				// Is the property edit inline new 
				const bool bIsEditInlineNew = SPropertyEditorEditInline::Supports(&ChildNode, ChildNode.GetArrayIndex());

				// Is this a property of an array
				bool bIsChildOfArray = PropertyEditorHelpers::IsChildOfArray(ChildNode);

				// Edit inline new properties should be visible by default
				bVisibleByDefault |= bIsEditInlineNew;

				// Children of arrays are not visible directly,
				bVisibleByDefault &= !bIsChildOfArray;

				FPropertyAndParent PropertyAndParent(*Property, ParentProperty);
				const bool bIsUserVisible = IsPropertyVisible(PropertyAndParent);

				// Inners of customized in structs should not be taken into consideration for customizing.  They are not designed to be individually customized when their parent is already customized
				if(!bIsChildOfCustomizedStruct && !LocalUpdateFavoriteSystemOnly)
				{
					// Add any object classes with properties so we can ask them for custom property layouts later
					LayoutData.ClassesWithProperties.Add(Property->GetOwnerStruct());
				}

				// If there is no outer object then the class is the object root and there is only one instance
				FName InstanceName = NAME_None;
				if(CurObjectNode && CurObjectNode->GetParentNode())
				{
					InstanceName = CurObjectNode->GetParentNode()->GetProperty()->GetFName();
				}
				else if(ParentStructProp)
				{
					InstanceName = ParentStructProp->GetFName();
				}

				// Do not add children of customized in struct properties or arrays
				if(!bIsChildOfCustomizedStruct && !bIsChildOfArray && !LocalUpdateFavoriteSystemOnly)
				{
					// Get the class property map
					FClassInstanceToPropertyMap& ClassInstanceMap = LayoutData.ClassToPropertyMap.FindOrAdd(Property->GetOwnerStruct()->GetFName());

					FPropertyNodeMap& PropertyNodeMap = ClassInstanceMap.FindOrAdd(InstanceName);

					if(!PropertyNodeMap.ParentProperty)
					{
						PropertyNodeMap.ParentProperty = CurObjectNode;
					}
					else
					{
						ensure(PropertyNodeMap.ParentProperty == CurObjectNode);
					}

					checkSlow(!PropertyNodeMap.Contains(Property->GetFName()));

					PropertyNodeMap.Add(Property->GetFName(), ChildNodePtr);
				}
				bool bCanDisplayFavorite = false;
				if(bVisibleByDefault && bIsUserVisible && !bPushOutStructProps)
				{
					FName CategoryName = CurCategory;
					// For properties inside a struct, add them to their own category unless they just take the name of the parent struct.  
					// In that case push them to the parent category
					FName PropertyCatagoryName = FObjectEditorUtils::GetCategoryFName(Property);
					if(!ParentStructProp || (PropertyCatagoryName != ParentStructProp->Struct->GetFName()))
					{
						CategoryName = PropertyCatagoryName;
					}

					if(!LocalUpdateFavoriteSystemOnly)
					{
						if(IsPropertyReadOnly(PropertyAndParent))
						{
							ChildNode.SetNodeFlags(EPropertyNodeFlags::IsReadOnly, true);
						}

						// Add a property to the default category
						FDetailCategoryImpl& CategoryImpl = DetailLayout.DefaultCategory(CategoryName);
						CategoryImpl.AddPropertyNode(ChildNodePtr.ToSharedRef(), InstanceName);
					}

					bCanDisplayFavorite = true;
					if(bEnableFavoriteSystem)
					{
						if(bIsCustomizedStruct)
						{
							bCanDisplayFavorite = false;
							//CustomizedStruct child are not categorize since they are under an object but we have to put them in favorite category if the user want to favorite them
							LocalUpdateFavoriteSystemOnly = true;
						}
						else if(ChildNodePtr->IsFavorite())
						{
							//Find or create the favorite category, we have to duplicate favorite property row under this category
							FString CategoryFavoritesName = TEXT("Favorites");
							FName CatFavName = *CategoryFavoritesName;
							FDetailCategoryImpl& CategoryFavImpl = DetailLayout.DefaultCategory(CatFavName);
							CategoryFavImpl.SetSortOrder(0);
							CategoryFavImpl.SetCategoryAsSpecialFavorite();

							//Add the property to the favorite
							FObjectPropertyNode *RootObjectParent = ChildNodePtr->FindRootObjectItemParent();
							FName RootInstanceName = NAME_None;
							if(RootObjectParent != nullptr)
							{
								RootInstanceName = RootObjectParent->GetObjectBaseClass()->GetFName();
							}

							if(LocalUpdateFavoriteSystemOnly)
							{
								if(IsPropertyReadOnly(PropertyAndParent))
								{
									ChildNode.SetNodeFlags(EPropertyNodeFlags::IsReadOnly, true);
								}
								else
								{
									//If the parent has a condition that is not met, make the child as readonly
									FDetailLayoutCustomization ParentTmpCustomization;
									ParentTmpCustomization.PropertyRow = MakeShareable(new FDetailPropertyRow(InNode.AsShared(), CategoryFavImpl.AsShared()));
									if(ParentTmpCustomization.PropertyRow->GetPropertyEditor()->IsPropertyEditingEnabled() == false)
									{
										ChildNode.SetNodeFlags(EPropertyNodeFlags::IsReadOnly, true);
									}
								}
							}

							//Duplicate the row
							CategoryFavImpl.AddPropertyNode(ChildNodePtr.ToSharedRef(), RootInstanceName);
						}

						if(bIsStruct)
						{
							LocalUpdateFavoriteSystemOnly = true;
						}
					}
				}
				ChildNodePtr->SetCanDisplayFavorite(bCanDisplayFavorite);

				bool bRecurseIntoChildren =
					!bIsChildOfCustomizedStruct // Don't recurse into built in struct children, we already know what they are and how to display them
					&&  !bIsCustomizedStruct // Don't recurse into customized structs
					&&	!bIsChildOfArray // Do not recurse into arrays, the children are drawn by the array property parent
					&&	!bIsEditInlineNew // Edit inline new children are not supported for customization yet
					&&	bIsUserVisible // Properties must be allowed to be visible by a user if they are not then their children are not visible either
					&& (!bIsStruct || bPushOutStructProps); //  Only recurse into struct properties if they are going to be displayed as standalone properties in categories instead of inside an expandable area inside a category

				if(bRecurseIntoChildren || LocalUpdateFavoriteSystemOnly)
				{
					// Built in struct properties or children of arras 
					UpdateSinglePropertyMapRecursive(ChildNode, LayoutData, CurCategory, CurObjectNode, bEnableFavoriteSystem, LocalUpdateFavoriteSystemOnly);
				}
			}
		}
	}
}

void SDetailsViewBase::OnColorPickerWindowClosed(const TSharedRef<SWindow>& Window)
{
	const TSharedPtr< FPropertyNode > PinnedColorPropertyNode = ColorPropertyNode.Pin();
	if (ensure(PinnedColorPropertyNode.IsValid()))
	{
		UProperty* Property = PinnedColorPropertyNode->GetProperty();
		if (Property && PropertyUtilities.IsValid())
		{
			FPropertyChangedEvent ChangeEvent(Property, EPropertyChangeType::ArrayAdd);
			PinnedColorPropertyNode->FixPropertiesInEvent(ChangeEvent);
			PropertyUtilities->NotifyFinishedChangingProperties(ChangeEvent);
		}
	}

	// A color picker window is no longer open
	bHasOpenColorPicker = false;
	ColorPropertyNode.Reset();
}


void SDetailsViewBase::SetIsPropertyVisibleDelegate(FIsPropertyVisible InIsPropertyVisible)
{
	IsPropertyVisibleDelegate = InIsPropertyVisible;
}

void SDetailsViewBase::SetIsPropertyReadOnlyDelegate(FIsPropertyReadOnly InIsPropertyReadOnly)
{
	IsPropertyReadOnlyDelegate = InIsPropertyReadOnly;
}

void SDetailsViewBase::SetIsPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled IsPropertyEditingEnabled)
{
	IsPropertyEditingEnabledDelegate = IsPropertyEditingEnabled;
}

bool SDetailsViewBase::IsPropertyEditingEnabled() const
{
	// If the delegate is not bound assume property editing is enabled, otherwise ask the delegate
	return !IsPropertyEditingEnabledDelegate.IsBound() || IsPropertyEditingEnabledDelegate.Execute();
}

void SDetailsViewBase::SetKeyframeHandler( TSharedPtr<class IDetailKeyframeHandler> InKeyframeHandler )
{
	KeyframeHandler = InKeyframeHandler;
}

TSharedPtr<IDetailKeyframeHandler> SDetailsViewBase::GetKeyframeHandler() 
{
	return KeyframeHandler;
}

void SDetailsViewBase::SetExtensionHandler(TSharedPtr<class IDetailPropertyExtensionHandler> InExtensionHandler)
{
	ExtensionHandler = InExtensionHandler;
}

TSharedPtr<IDetailPropertyExtensionHandler> SDetailsViewBase::GetExtensionHandler()
{
	return ExtensionHandler;
}

void SDetailsViewBase::SetGenericLayoutDetailsDelegate(FOnGetDetailCustomizationInstance OnGetGenericDetails)
{
	GenericLayoutDelegate = OnGetGenericDetails;
}

void SDetailsViewBase::RefreshRootObjectVisibility()
{
	RerunCurrentFilter();
}

TSharedPtr<FAssetThumbnailPool> SDetailsViewBase::GetThumbnailPool() const
{
	if (!ThumbnailPool.IsValid())
	{
		// Create a thumbnail pool for the view if it doesnt exist.  This does not use resources of no thumbnails are used
		ThumbnailPool = MakeShareable(new FAssetThumbnailPool(50, TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &SDetailsView::IsHovered))));
	}

	return ThumbnailPool;
}

void SDetailsViewBase::NotifyFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent)
{
	OnFinishedChangingPropertiesDelegate.Broadcast(PropertyChangedEvent);
}

void SDetailsViewBase::RequestItemExpanded(TSharedRef<IDetailTreeNode> TreeNode, bool bExpand)
{
	// Don't change expansion state if its already in that state
	if (DetailTree->IsItemExpanded(TreeNode) != bExpand)
	{
		FilteredNodesRequestingExpansionState.Add(TreeNode, bExpand);
	}
}

void SDetailsViewBase::RefreshTree()
{
	if (OnDisplayedPropertiesChangedDelegate.IsBound())
	{
		OnDisplayedPropertiesChangedDelegate.Execute();
	}

	DetailTree->RequestTreeRefresh();
}

void SDetailsViewBase::SaveCustomExpansionState(const FString& NodePath, bool bIsExpanded)
{
	if (bIsExpanded)
	{
		ExpandedDetailNodes.Add(NodePath);
	}
	else
	{
		ExpandedDetailNodes.Remove(NodePath);
	}
}

bool SDetailsViewBase::GetCustomSavedExpansionState(const FString& NodePath) const
{
	return ExpandedDetailNodes.Contains(NodePath);
}

bool SDetailsViewBase::IsPropertyVisible( const FPropertyAndParent& PropertyAndParent ) const
{
	return IsPropertyVisibleDelegate.IsBound() ? IsPropertyVisibleDelegate.Execute(PropertyAndParent) : true;
}

bool SDetailsViewBase::IsPropertyReadOnly( const FPropertyAndParent& PropertyAndParent ) const
{
	return IsPropertyReadOnlyDelegate.IsBound() ? IsPropertyReadOnlyDelegate.Execute(PropertyAndParent) : false;
}

TSharedPtr<IPropertyUtilities> SDetailsViewBase::GetPropertyUtilities()
{
	return PropertyUtilities;
}

void SDetailsViewBase::OnShowOnlyModifiedClicked()
{
	CurrentFilter.bShowOnlyModifiedProperties = !CurrentFilter.bShowOnlyModifiedProperties;

	UpdateFilteredDetails();
}

void SDetailsViewBase::OnShowAllAdvancedClicked()
{
	CurrentFilter.bShowAllAdvanced = !CurrentFilter.bShowAllAdvanced;

	UpdateFilteredDetails();
}

void SDetailsViewBase::OnShowOnlyDifferingClicked()
{
	CurrentFilter.bShowOnlyDiffering = !CurrentFilter.bShowOnlyDiffering;

	UpdateFilteredDetails();
}

void SDetailsViewBase::OnShowAllChildrenIfCategoryMatchesClicked()
{
	CurrentFilter.bShowAllChildrenIfCategoryMatches = !CurrentFilter.bShowAllChildrenIfCategoryMatches;

	UpdateFilteredDetails();
}

/** Called when the filter text changes.  This filters specific property nodes out of view */
void SDetailsViewBase::OnFilterTextChanged(const FText& InFilterText)
{
	FString InFilterString = InFilterText.ToString();
	InFilterString.Trim().TrimTrailing();

	// Was the filter just cleared
	bool bFilterCleared = InFilterString.Len() == 0 && CurrentFilter.FilterStrings.Num() > 0;

	FilterView(InFilterString);

}

TSharedPtr<SWidget> SDetailsViewBase::GetNameAreaWidget()
{
	return DetailsViewArgs.bCustomNameAreaLocation ? NameArea : nullptr;
}

TSharedPtr<SWidget> SDetailsViewBase::GetFilterAreaWidget()
{
	return DetailsViewArgs.bCustomFilterAreaLocation ? FilterRow : nullptr;
}

TSharedPtr<class FUICommandList> SDetailsViewBase::GetHostCommandList() const
{
	return DetailsViewArgs.HostCommandList;
}

/** 
 * Hides or shows properties based on the passed in filter text
 * 
 * @param InFilterText	The filter text
 */
void SDetailsViewBase::FilterView(const FString& InFilterText)
{
	TArray<FString> CurrentFilterStrings;

	FString ParseString = InFilterText;
	// Remove whitespace from the front and back of the string
	ParseString.Trim();
	ParseString.TrimTrailing();
	ParseString.ParseIntoArray(CurrentFilterStrings, TEXT(" "), true);

	bHasActiveFilter = CurrentFilterStrings.Num() > 0;

	CurrentFilter.FilterStrings = CurrentFilterStrings;

	UpdateFilteredDetails();
}

void SDetailsViewBase::QueryLayoutForClass(FDetailLayoutData& LayoutData, UStruct* Class)
{
	LayoutData.DetailLayout->SetCurrentCustomizationClass(Class, NAME_None);

	FPropertyEditorModule& ParentPlugin = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FCustomDetailLayoutNameMap& GlobalCustomLayoutNameMap = ParentPlugin.ClassNameToDetailLayoutNameMap;

	// Check the instanced map first
	FDetailLayoutCallback* Callback = InstancedClassToDetailLayoutMap.Find(Class);

	if (!Callback)
	{
		// callback wasn't found in the per instance map, try the global instances instead
		Callback = GlobalCustomLayoutNameMap.Find(Class->GetFName());
	}

	if (Callback && Callback->DetailLayoutDelegate.IsBound())
	{
		// Create a new instance of the custom detail layout for the current class
		TSharedRef<IDetailCustomization> CustomizationInstance = Callback->DetailLayoutDelegate.Execute();

		// Ask for details immediately
		CustomizationInstance->CustomizeDetails(*LayoutData.DetailLayout);

		// Save the instance from destruction until we refresh
		LayoutData.CustomizationClassInstances.Add(CustomizationInstance);
	}
}

void SDetailsViewBase::QueryCustomDetailLayout(FDetailLayoutData& LayoutData)
{
	FPropertyEditorModule& ParentPlugin = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	// Get the registered classes that customize details
	FCustomDetailLayoutNameMap& GlobalCustomLayoutNameMap = ParentPlugin.ClassNameToDetailLayoutNameMap;

	UStruct* BaseStruct = LayoutData.DetailLayout->GetRootNode()->GetBaseStructure();

	LayoutData.CustomizationClassInstances.Empty();

	//Ask for generic details not specific to an object being viewed 
	if (GenericLayoutDelegate.IsBound())
	{
		// Create a new instance of the custom detail layout for the current class
		TSharedRef<IDetailCustomization> CustomizationInstance = GenericLayoutDelegate.Execute();

		// Ask for details immediately
		CustomizationInstance->CustomizeDetails(*LayoutData.DetailLayout);

		// Save the instance from destruction until we refresh
		LayoutData.CustomizationClassInstances.Add(CustomizationInstance);
	}


	// Sort them by query order.  @todo not good enough
	struct FCompareFDetailLayoutCallback
	{
		FORCEINLINE bool operator()(const FDetailLayoutCallback& A, const FDetailLayoutCallback& B) const
		{
			return A.Order < B.Order;
		}
	};

	TMap< TWeakObjectPtr<UStruct>, FDetailLayoutCallback*> FinalCallbackMap;

	for (auto ClassIt = LayoutData.ClassesWithProperties.CreateConstIterator(); ClassIt; ++ClassIt)
	{
		// Must be a class
		UClass* Class = Cast<UClass>(ClassIt->Get());
		if (!Class)
		{
			continue;
		}

		// Check the instanced map first
		FDetailLayoutCallback* Callback = InstancedClassToDetailLayoutMap.Find(Class);

		if (!Callback)
		{
			// callback wasn't found in the per instance map, try the global instances instead
			Callback = GlobalCustomLayoutNameMap.Find(Class->GetFName());
		}

		if (Callback)
		{
			FinalCallbackMap.Add(Class, Callback);
		}
	}

	FinalCallbackMap.ValueSort(FCompareFDetailLayoutCallback());

	TSet<UStruct*> QueriedClasses;

	if (FinalCallbackMap.Num() > 0)
	{
		// Ask each class that we have properties for to customize its layout
		for (auto LayoutIt(FinalCallbackMap.CreateConstIterator()); LayoutIt; ++LayoutIt)
		{
			const TWeakObjectPtr<UStruct> WeakClass = LayoutIt.Key();

			if (WeakClass.IsValid())
			{
				UStruct* Class = WeakClass.Get();

				FClassInstanceToPropertyMap& InstancedPropertyMap = LayoutData.ClassToPropertyMap.FindChecked(Class->GetFName());
				for (FClassInstanceToPropertyMap::TIterator InstanceIt(InstancedPropertyMap); InstanceIt; ++InstanceIt)
				{
					FName Key = InstanceIt.Key();
					LayoutData.DetailLayout->SetCurrentCustomizationClass(Class, Key);

					const FOnGetDetailCustomizationInstance& DetailDelegate = LayoutIt.Value()->DetailLayoutDelegate;

					if (DetailDelegate.IsBound())
					{
						QueriedClasses.Add(Class);

						// Create a new instance of the custom detail layout for the current class
						TSharedRef<IDetailCustomization> CustomizationInstance = DetailDelegate.Execute();

						// Ask for details immediately
						CustomizationInstance->CustomizeDetails(*LayoutData.DetailLayout);

						// Save the instance from destruction until we refresh
						LayoutData.CustomizationClassInstances.Add(CustomizationInstance);
					}
				}
			}
		}
	}

	// Ensure that the base class and its parents are always queried
	TSet<UStruct*> ParentClassesToQuery;
	if (BaseStruct && !QueriedClasses.Contains(BaseStruct))
	{
		ParentClassesToQuery.Add(BaseStruct);
		LayoutData.ClassesWithProperties.Add(BaseStruct);
	}

	// Find base classes of queried classes that were not queried and add them to the query list
	// this supports cases where a parent class has no properties but still wants to add customization
	for (auto QueriedClassIt = LayoutData.ClassesWithProperties.CreateConstIterator(); QueriedClassIt; ++QueriedClassIt)
	{
		UStruct* ParentStruct = (*QueriedClassIt)->GetSuperStruct();

		while (ParentStruct && ParentStruct->IsA(UClass::StaticClass()) && !QueriedClasses.Contains(ParentStruct) && !LayoutData.ClassesWithProperties.Contains(ParentStruct))
		{
			ParentClassesToQuery.Add(ParentStruct);
			ParentStruct = ParentStruct->GetSuperStruct();
		}
	}

	// Query extra base classes
	for (auto ParentIt = ParentClassesToQuery.CreateConstIterator(); ParentIt; ++ParentIt)
	{
		UClass* ParentClass = Cast<UClass>(*ParentIt);
		if (ParentClass)
		{
			QueryLayoutForClass(LayoutData, ParentClass);
		}
	}
}

EVisibility SDetailsViewBase::GetFilterBoxVisibility() const
{
	// Visible if we allow search and we have anything to search otherwise collapsed so it doesn't take up room
	return (DetailsViewArgs.bAllowSearch && IsConnected()) ? EVisibility::Visible : EVisibility::Collapsed;
}

bool SDetailsViewBase::SupportsKeyboardFocus() const
{
	return DetailsViewArgs.bSearchInitialKeyFocus && SearchBox->SupportsKeyboardFocus() && GetFilterBoxVisibility() == EVisibility::Visible;
}

FReply SDetailsViewBase::OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent)
{
	FReply Reply = FReply::Handled();

	if (InFocusEvent.GetCause() != EFocusCause::Cleared)
	{
		Reply.SetUserFocus(SearchBox.ToSharedRef(), InFocusEvent.GetCause());
	}

	return Reply;
}

/** Ticks the property view.  This function performs a data consistency check */
void SDetailsViewBase::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	for (int32 i = 0; i < CustomizationClassInstancesPendingDelete.Num(); ++i)
	{
		ensure(CustomizationClassInstancesPendingDelete[i].IsUnique());
	}

	// Release any pending kill nodes.
	for ( TSharedPtr<FComplexPropertyNode>& PendingKillNode : RootNodesPendingKill )
	{
		if ( PendingKillNode.IsValid() )
		{
			PendingKillNode->Disconnect();
			PendingKillNode.Reset();
		}
	}
	RootNodesPendingKill.Empty();

	// Empty all the customization instances that need to be deleted
	CustomizationClassInstancesPendingDelete.Empty();

	FRootPropertyNodeList& RootPropertyNodes = GetRootNodes();

	for(TSharedPtr<FComplexPropertyNode>& RootPropertyNode : RootPropertyNodes)
	{
		check(RootPropertyNode.IsValid());

		// Purge any objects that are marked pending kill from the object list
		if(auto ObjectRoot = RootPropertyNode->AsObjectNode())
		{
			ObjectRoot->PurgeKilledObjects();
		}

		if(DeferredActions.Num() > 0)
		{		
			// Any deferred actions are likely to cause the node  tree to be at least partially rebuilt
			// Save the expansion state of existing nodes so we can expand them later
			SaveExpandedItems(RootPropertyNode.ToSharedRef());
		}
	}

	if (DeferredActions.Num() > 0)
	{
		// Execute any deferred actions
		for (int32 ActionIndex = 0; ActionIndex < DeferredActions.Num(); ++ActionIndex)
		{
			DeferredActions[ActionIndex].ExecuteIfBound();
		}
		DeferredActions.Empty();
	}

	TSharedPtr<FComplexPropertyNode> LastRootPendingKill;
	if (RootNodesPendingKill.Num() > 0 )
	{
		LastRootPendingKill = RootNodesPendingKill.Last();
	}

	bool bValidateExternalNodes = true;

	int32 FoundIndex = RootPropertyNodes.Find(LastRootPendingKill);
	if (FoundIndex != INDEX_NONE)
	{ 
		// Reaquire the root property nodes.  It may have been changed by the deferred actions if something like a blueprint editor forcefully resets a details panel during a posteditchange
		ForceRefresh();

		// All objects are being reset, no need to validate external nodes
		bValidateExternalNodes = false;
	}
	else
	{
		for(TSharedPtr<FComplexPropertyNode>& RootPropertyNode : RootPropertyNodes)
		{
			if(RootPropertyNode == LastRootPendingKill)
			{
				RestoreExpandedItems(RootPropertyNode.ToSharedRef());
			}

			EPropertyDataValidationResult Result = RootPropertyNode->EnsureDataIsValid();
			if(Result == EPropertyDataValidationResult::PropertiesChanged || Result == EPropertyDataValidationResult::EditInlineNewValueChanged)
			{
				RestoreExpandedItems(RootPropertyNode.ToSharedRef());
				UpdatePropertyMaps();
				UpdateFilteredDetails();
			}
			else if(Result == EPropertyDataValidationResult::ArraySizeChanged)
			{
				RestoreExpandedItems(RootPropertyNode.ToSharedRef());
				UpdateFilteredDetails();
			}
			else if(Result == EPropertyDataValidationResult::ObjectInvalid)
			{
				ForceRefresh();
				break;

				// All objects are being reset, no need to validate external nodes
				bValidateExternalNodes = false;
			}
		}
	}

	if (bValidateExternalNodes)
	{
		for (int32 NodeIndex = 0; NodeIndex < ExternalRootPropertyNodes.Num(); ++NodeIndex)
		{
			TSharedPtr<FPropertyNode> PropertyNode = ExternalRootPropertyNodes[NodeIndex].Pin();

			if (PropertyNode.IsValid())
			{
				EPropertyDataValidationResult Result = PropertyNode->EnsureDataIsValid();
				if (Result == EPropertyDataValidationResult::PropertiesChanged || Result == EPropertyDataValidationResult::EditInlineNewValueChanged)
				{
					RestoreExpandedItems(PropertyNode.ToSharedRef());
					UpdatePropertyMaps();
					UpdateFilteredDetails();
					// Note this will invalidate all the external root nodes so there is no need to continue
					ExternalRootPropertyNodes.Empty();
					break;
				}
				else if (Result == EPropertyDataValidationResult::ArraySizeChanged)
				{
					RestoreExpandedItems(PropertyNode.ToSharedRef());
					UpdateFilteredDetails();
				}
			}
			else
			{
				// Remove the current node if it is no longer valid
				ExternalRootPropertyNodes.RemoveAt(NodeIndex);
				--NodeIndex;
			}
		}
	}

	for(FDetailLayoutData& LayoutData : DetailLayouts)
	{
		if(LayoutData.DetailLayout.IsValid())
		{
			LayoutData.DetailLayout->Tick(InDeltaTime);
		}
	}

	if (!ColorPropertyNode.IsValid() && bHasOpenColorPicker)
	{
		// Destroy the color picker window if the color property node has become invalid
		DestroyColorPicker();
		bHasOpenColorPicker = false;
	}


	if (FilteredNodesRequestingExpansionState.Num() > 0)
	{
		// change expansion state on the nodes that request it
		for (TMap<TSharedRef<IDetailTreeNode>, bool >::TConstIterator It(FilteredNodesRequestingExpansionState); It; ++It)
		{
			DetailTree->SetItemExpansion(It.Key(), It.Value());
		}

		FilteredNodesRequestingExpansionState.Empty();
	}
}

/**
* Recursively gets expanded items for a node
*
* @param InPropertyNode			The node to get expanded items from
* @param OutExpandedItems	List of expanded items that were found
*/
void GetExpandedItems(TSharedPtr<FPropertyNode> InPropertyNode, TArray<FString>& OutExpandedItems)
{
	if (InPropertyNode->HasNodeFlags(EPropertyNodeFlags::Expanded))
	{
		const bool bWithArrayIndex = true;
		FString Path;
		Path.Empty(128);
		InPropertyNode->GetQualifiedName(Path, bWithArrayIndex);

		OutExpandedItems.Add(Path);
	}

	for (int32 ChildIndex = 0; ChildIndex < InPropertyNode->GetNumChildNodes(); ++ChildIndex)
	{
		GetExpandedItems(InPropertyNode->GetChildNode(ChildIndex), OutExpandedItems);
	}

}

/**
* Recursively sets expanded items for a node
*
* @param InNode			The node to set expanded items on
* @param OutExpandedItems	List of expanded items to set
*/
void SetExpandedItems(TSharedPtr<FPropertyNode> InPropertyNode, const TSet<FString>& InExpandedItems)
{
	if (InExpandedItems.Num() > 0)
	{
		const bool bWithArrayIndex = true;
		FString Path;
		Path.Empty(128);
		InPropertyNode->GetQualifiedName(Path, bWithArrayIndex);

		if (InExpandedItems.Contains(Path))
		{
			InPropertyNode->SetNodeFlags(EPropertyNodeFlags::Expanded, true);
		}

		for (int32 NodeIndex = 0; NodeIndex < InPropertyNode->GetNumChildNodes(); ++NodeIndex)
		{
			SetExpandedItems(InPropertyNode->GetChildNode(NodeIndex), InExpandedItems);
		}
	}
}

void SDetailsViewBase::SaveExpandedItems( TSharedRef<FPropertyNode> StartNode )
{
	UStruct* BestBaseStruct = StartNode->FindComplexParent()->GetBaseStructure();

	TArray<FString> ExpandedPropertyItems;
	GetExpandedItems(StartNode, ExpandedPropertyItems);

	// Handle spaces in expanded node names by wrapping them in quotes
	for( FString& String : ExpandedPropertyItems )
	{
		String.InsertAt(0, '"');
		String.AppendChar('"');
	}

	TArray<FString> ExpandedCustomItems = ExpandedDetailNodes.Array();

	// Expanded custom items may have spaces but SetSingleLineArray doesnt support spaces (treats it as another element in the array)
	// Append a '|' after each element instead
	FString ExpandedCustomItemsString;
	for (auto It = ExpandedDetailNodes.CreateConstIterator(); It; ++It)
	{
		ExpandedCustomItemsString += *It;
		ExpandedCustomItemsString += TEXT(",");
	}

	//while a valid class, and we're either the same as the base class (for multiple actors being selected and base class is AActor) OR we're not down to AActor yet)
	for (UStruct* Struct = BestBaseStruct; Struct && ((BestBaseStruct == Struct) || (Struct != AActor::StaticClass())); Struct = Struct->GetSuperStruct())
	{
		if (StartNode->GetNumChildNodes() > 0)
		{
			bool bShouldSave = ExpandedPropertyItems.Num() > 0;
			if (!bShouldSave)
			{
				TArray<FString> DummyExpandedPropertyItems;
				GConfig->GetSingleLineArray(TEXT("DetailPropertyExpansion"), *Struct->GetName(), DummyExpandedPropertyItems, GEditorPerProjectIni);
				bShouldSave = DummyExpandedPropertyItems.Num() > 0;
			}

			if (bShouldSave)
			{
				GConfig->SetSingleLineArray(TEXT("DetailPropertyExpansion"), *Struct->GetName(), ExpandedPropertyItems, GEditorPerProjectIni);
			}
		}
	}

	if (DetailLayouts.Num() > 0 && BestBaseStruct)
	{
		bool bShouldSave = !ExpandedCustomItemsString.IsEmpty();
		if (!bShouldSave)
		{
			FString DummyExpandedCustomItemsString;
			GConfig->GetString(TEXT("DetailCustomWidgetExpansion"), *BestBaseStruct->GetName(), DummyExpandedCustomItemsString, GEditorPerProjectIni);
			bShouldSave = !DummyExpandedCustomItemsString.IsEmpty();
		}
		if (bShouldSave)
		{
			GConfig->SetString(TEXT("DetailCustomWidgetExpansion"), *BestBaseStruct->GetName(), *ExpandedCustomItemsString, GEditorPerProjectIni);
		}
	}
}

void SDetailsViewBase::RestoreExpandedItems(TSharedRef<FPropertyNode> InitialStartNode)
{
	TSharedPtr<FPropertyNode> StartNode = InitialStartNode;

	ExpandedDetailNodes.Empty();

	FString ExpandedCustomItems;

	UStruct* BestBaseStruct = StartNode->FindComplexParent()->GetBaseStructure();

	//while a valid class, and we're either the same as the base class (for multiple actors being selected and base class is AActor) OR we're not down to AActor yet)
	TArray<FString> DetailPropertyExpansionStrings;
	for (UStruct* Struct = BestBaseStruct; Struct && ((BestBaseStruct == Struct) || (Struct != AActor::StaticClass())); Struct = Struct->GetSuperStruct())
	{
		GConfig->GetSingleLineArray(TEXT("DetailPropertyExpansion"), *Struct->GetName(), DetailPropertyExpansionStrings, GEditorPerProjectIni);
	}

	TSet<FString> ExpandedPropertyItems;
	ExpandedPropertyItems.Append(DetailPropertyExpansionStrings);
	SetExpandedItems(StartNode, ExpandedPropertyItems);

	if (BestBaseStruct)
	{
		GConfig->GetString(TEXT("DetailCustomWidgetExpansion"), *BestBaseStruct->GetName(), ExpandedCustomItems, GEditorPerProjectIni);
		TArray<FString> ExpandedCustomItemsArray;
		ExpandedCustomItems.ParseIntoArray(ExpandedCustomItemsArray, TEXT(","), true);

		ExpandedDetailNodes.Append(ExpandedCustomItemsArray);
	}
}

void SDetailsViewBase::UpdateFilteredDetails()
{
	RootTreeNodes.Reset();

	FDetailNodeList InitialRootNodeList;
	
	NumVisbleTopLevelObjectNodes = 0;
	FRootPropertyNodeList& RootPropertyNodes = GetRootNodes();

	for(int32 RootNodeIndex = 0; RootNodeIndex < RootPropertyNodes.Num(); ++RootNodeIndex)
	{
		TSharedPtr<FComplexPropertyNode>& RootPropertyNode = RootPropertyNodes[RootNodeIndex];
		if(RootPropertyNode.IsValid())
		{
			RootPropertyNode->FilterNodes(CurrentFilter.FilterStrings);
			RootPropertyNode->ProcessSeenFlags(true);

			for(int32 NodeIndex = 0; NodeIndex < ExternalRootPropertyNodes.Num(); ++NodeIndex)
			{
				TSharedPtr<FPropertyNode> PropertyNode = ExternalRootPropertyNodes[NodeIndex].Pin();

				if(PropertyNode.IsValid())
				{
					PropertyNode->FilterNodes(CurrentFilter.FilterStrings);
					PropertyNode->ProcessSeenFlags(true);
				}
			}

			TSharedPtr<FDetailLayoutBuilderImpl>& DetailLayout = DetailLayouts[RootNodeIndex].DetailLayout;
			if(DetailLayout.IsValid())
			{
				DetailLayout->FilterDetailLayout(CurrentFilter);
			}

			FDetailNodeList& LayoutRoots = DetailLayout->GetRootTreeNodes();
			if(LayoutRoots.Num() > 0)
			{
				// A top level object nodes has a non-filtered away root so add one to the total number we have
				++NumVisbleTopLevelObjectNodes;

				InitialRootNodeList.Append(LayoutRoots);
			}
		
		}
	}


	// for multiple top level object we need to do a secondary pass on top level object nodes after we have determined if there is any nodes visible at all.  If there are then we ask the details panel if it wants to show childen
	for(TSharedRef<class IDetailTreeNode> RootNode : InitialRootNodeList)
	{
		if(RootNode->ShouldShowOnlyChildren())
		{
			RootNode->GetChildren(RootTreeNodes);
		}
		else
		{
			RootTreeNodes.Add(RootNode);
		}
	}

	RefreshTree();
}



void SDetailsViewBase::RegisterInstancedCustomPropertyLayoutInternal(UStruct* Class, FOnGetDetailCustomizationInstance DetailLayoutDelegate)
{
	check( Class );

	FDetailLayoutCallback Callback;
	Callback.DetailLayoutDelegate = DetailLayoutDelegate;
	// @todo: DetailsView: Fix me: this specifies the order in which detail layouts should be queried
	Callback.Order = InstancedClassToDetailLayoutMap.Num();

	InstancedClassToDetailLayoutMap.Add( Class, Callback );	
}

void SDetailsViewBase::UnregisterInstancedCustomPropertyLayoutInternal(UStruct* Class)
{
	check( Class );

	InstancedClassToDetailLayoutMap.Remove( Class );	
}
