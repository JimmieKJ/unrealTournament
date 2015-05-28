// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintEditorPrivatePCH.h"

#include "DiffUtils.h"
#include "EditorCategoryUtils.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "IAssetTypeActions.h"
#include "ObjectEditorUtils.h"
#include "ISourceControlModule.h"

static const UProperty* Resolve( const UStruct* Class, FName PropertyName )
{
	for (TFieldIterator<UProperty> PropertyIt(Class); PropertyIt; ++PropertyIt)
	{
		if( PropertyIt->GetFName() == PropertyName )
		{
			return *PropertyIt;
		}
	}

	return nullptr;
}

static FPropertySoftPathSet GetPropertyNameSet(const UObject* ForObj)
{
	return FPropertySoftPathSet(DiffUtils::GetVisiblePropertiesInOrderDeclared(ForObj));
}

FResolvedProperty FPropertySoftPath::Resolve(const UObject* Object) const
{
	// dig into the object, finding nested objects, etc:
	const UProperty* Property = nullptr;
	for( int32 i = 0; i < PropertyChain.Num(); ++i )
	{
		const UProperty* NextProperty = ::Resolve(Object->GetClass(), PropertyChain[i]);
		if( NextProperty )
		{
			Property = NextProperty;
			if (const UObjectProperty* ObjectProperty = Cast<UObjectProperty>(Property))
			{
				Object = ObjectProperty->GetObjectPropertyValue(Property->ContainerPtrToValuePtr<UObject*>(Object));
			}
			else
			{
				break;
			}
		}
		else
		{
			break;
		}
	}

	return FResolvedProperty(Object, Property);
}

FPropertyPath FPropertySoftPath::ResolvePath(const UObject* Object) const
{
	const UStruct* ClassOverride = nullptr;
	FPropertyPath Ret;
	for( const auto& Property : PropertyChain )
	{
		const UProperty* ResolvedProperty = ::Resolve(ClassOverride ? ClassOverride : Object->GetClass(), Property);
		if (const UObjectProperty* ObjectProperty = Cast<UObjectProperty>(ResolvedProperty))
		{
			// At the moment the cast of Object to void* just avoids an overzealous assert in ContainerPtrToValuePtr (for some of our
			// properties, the outer is a UStruct, not a UClass). At some point I want to support diffing of individual array elements, 
			// and that will necessitate the container be a void* anyway:
			const UObject* const* BaseObject = reinterpret_cast<const UObject* const*>( ObjectProperty->ContainerPtrToValuePtr<void>( static_cast<const void*>(Object)) );
			if( BaseObject && *BaseObject )
			{
				Object = *BaseObject;
			}
			ClassOverride = nullptr;
		}
		else if( const UStructProperty* StructProperty = Cast<UStructProperty>(ResolvedProperty) )
		{
			ClassOverride = StructProperty->Struct;
		}
		FPropertyInfo Info = { ResolvedProperty, -1 };
		Ret.AddProperty(Info);
	}
	return Ret;
}

const UObject* DiffUtils::GetCDO(const UBlueprint* ForBlueprint)
{
	if (!ForBlueprint
		|| !ForBlueprint->GeneratedClass)
	{
		return NULL;
	}

	return ForBlueprint->GeneratedClass->ClassDefaultObject;
}

void DiffUtils::CompareUnrelatedObjects(const UObject* A, const UObject* B, TArray<FSingleObjectDiffEntry>& OutDifferingProperties)
{
	FPropertySoftPathSet PropertiesInA = GetPropertyNameSet(A);
	FPropertySoftPathSet PropertiesInB = GetPropertyNameSet(B);

	// any properties in A that aren't in B are differing:
	auto AddedToA = PropertiesInA.Difference(PropertiesInB).Array();
	for( const auto& Entry : AddedToA )
	{
		OutDifferingProperties.Push(FSingleObjectDiffEntry( Entry, EPropertyDiffType::PropertyAddedToA ));
	}

	// and the converse:
	auto AddedToB = PropertiesInB.Difference(PropertiesInA).Array();
	for (const auto& Entry : AddedToB)
	{
		OutDifferingProperties.Push(FSingleObjectDiffEntry( Entry, EPropertyDiffType::PropertyAddedToB ));
	}

	// for properties in common, dig out the uproperties and determine if they're identical:
	if (A && B)
	{
		FPropertySoftPathSet Common = PropertiesInA.Intersect(PropertiesInB);
		for (const auto& PropertyName : Common)
		{
			FResolvedProperty AProp = PropertyName.Resolve(A);
			FResolvedProperty BProp = PropertyName.Resolve(B);

			check(AProp != FResolvedProperty() && BProp != FResolvedProperty());
			if (!DiffUtils::Identical(AProp, BProp))
			{
				OutDifferingProperties.Push(FSingleObjectDiffEntry( PropertyName, EPropertyDiffType::PropertyValueChanged ));
			}
		}
	}
}

void DiffUtils::CompareUnrelatedSCS(const UBlueprint* Old, const TArray< FSCSResolvedIdentifier >& OldHierarchy, const UBlueprint* New, const TArray< FSCSResolvedIdentifier >& NewHierarchy, FSCSDiffRoot& OutDifferingEntries )
{
	const auto FindEntry = [](TArray< FSCSResolvedIdentifier > const& InArray, const FSCSIdentifier* Value) -> const FSCSResolvedIdentifier*
	{
		for (const auto& Node : InArray)
		{
			if (Node.Identifier.Name == Value->Name )
			{
				return &Node;
			}
		}
		return nullptr;
	};

	for (const auto& OldNode : OldHierarchy)
	{
		const FSCSResolvedIdentifier* NewEntry = FindEntry(NewHierarchy, &OldNode.Identifier);

		if (NewEntry != nullptr)
		{
			// did a property change?
			TArray<FSingleObjectDiffEntry> DifferingProperties;
			DiffUtils::CompareUnrelatedObjects(OldNode.Object, NewEntry->Object, DifferingProperties);
			for (const auto& Property : DifferingProperties)
			{
				FSCSDiffEntry Diff = { OldNode.Identifier, ETreeDiffType::NODE_PROPERTY_CHANGED, Property };
				OutDifferingEntries.Entries.Push(Diff);
			}

			// did it move?
			if( NewEntry->Identifier.TreeLocation != OldNode.Identifier.TreeLocation )
			{
				FSCSDiffEntry Diff = { OldNode.Identifier, ETreeDiffType::NODE_MOVED, FSingleObjectDiffEntry() };
				OutDifferingEntries.Entries.Push(Diff);
			}

			// no change! Do nothing.
		}
		else
		{
			// not found in the new data, must have been deleted:
			FSCSDiffEntry Entry = { OldNode.Identifier, ETreeDiffType::NODE_REMOVED, FSingleObjectDiffEntry() };
			OutDifferingEntries.Entries.Push( Entry );
		}
	}

	for (const auto& NewNode : NewHierarchy)
	{
		const FSCSResolvedIdentifier* OldEntry = FindEntry(OldHierarchy, &NewNode.Identifier);

		if (OldEntry == nullptr)
		{
			FSCSDiffEntry Entry = { NewNode.Identifier, ETreeDiffType::NODE_ADDED, FSingleObjectDiffEntry() };
			OutDifferingEntries.Entries.Push( Entry );
		}
	}
}

bool DiffUtils::Identical(const FResolvedProperty& AProp, const FResolvedProperty& BProp)
{
	const void* AValue = AProp.Property->ContainerPtrToValuePtr<void>(AProp.Object);
	const void* BValue = BProp.Property->ContainerPtrToValuePtr<void>(BProp.Object);

	return AProp.Property->Identical(AValue, BValue, PPF_DeepComparison);
}

TArray<FPropertySoftPath> DiffUtils::GetVisiblePropertiesInOrderDeclared(const UObject* ForObj, const TArray<FName>& Scope /*= TArray<FName>()*/)
{
	TArray<FPropertySoftPath> Ret;
	if (ForObj)
	{
		const UClass* Class = ForObj->GetClass();
		TSet<FString> HiddenCategories = FEditorCategoryUtils::GetHiddenCategories(Class);
		for (TFieldIterator<UProperty> PropertyIt(Class); PropertyIt; ++PropertyIt)
		{
			FName CategoryName = FObjectEditorUtils::GetCategoryFName(*PropertyIt);
			if (!HiddenCategories.Contains(CategoryName.ToString()))
			{
				if (PropertyIt->PropertyFlags&CPF_Edit)
				{
					TArray<FName> NewPath(Scope);
					NewPath.Push(PropertyIt->GetFName());
					if (const UObjectProperty* ObjectProperty = Cast<UObjectProperty>(*PropertyIt))
					{
						const UObject* const* BaseObject = reinterpret_cast<const UObject* const*>( ObjectProperty->ContainerPtrToValuePtr<void>(ForObj) );
						if (BaseObject && *BaseObject)
						{
							Ret.Append( GetVisiblePropertiesInOrderDeclared(*BaseObject, NewPath) );
						}
					}
					else
					{
						Ret.Push(NewPath);
					}
				}
			}
		}
	}
	return Ret;
}

TArray<FPropertyPath> DiffUtils::ResolveAll(const UObject* Object, const TArray<FPropertySoftPath>& InSoftProperties)
{
	TArray< FPropertyPath > Ret;
	for (const auto& Path : InSoftProperties)
	{
		Ret.Push(Path.ResolvePath(Object));
	}
	return Ret;
}

TArray<FPropertyPath> DiffUtils::ResolveAll(const UObject* Object, const TArray<FSingleObjectDiffEntry>& InDifferences)
{
	TArray< FPropertyPath > Ret;
	for (const auto& Difference : InDifferences)
	{
		Ret.Push(Difference.Identifier.ResolvePath(Object));
	}
	return Ret;
}

TSharedPtr<FBlueprintDifferenceTreeEntry> FBlueprintDifferenceTreeEntry::NoDifferencesEntry()
{
	// This just generates a widget that tells the user that no differences were detected. Without this
	// the treeview displaying differences is confusing when no differences are present because it is not obvious
	// that the control is a treeview (a treeview with no children looks like a listview).
	const auto GenerateWidget = []() -> TSharedRef<SWidget>
	{
		return SNew(STextBlock)
			.ColorAndOpacity(FLinearColor::FLinearColor(.7f, .7f, .7f))
			.TextStyle(FEditorStyle::Get(), TEXT("BlueprintDif.ItalicText"))
			.Text(NSLOCTEXT("FBlueprintDifferenceTreeEntry", "NoDifferencesLabel", "No differences detected..."));
	};

	return TSharedPtr<FBlueprintDifferenceTreeEntry>(new FBlueprintDifferenceTreeEntry(
		FOnDiffEntryFocused()
		, FGenerateDiffEntryWidget::CreateStatic(GenerateWidget)
		, TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >()
	) );
}

TSharedPtr<FBlueprintDifferenceTreeEntry> FBlueprintDifferenceTreeEntry::CreateDefaultsCategoryEntry(FOnDiffEntryFocused FocusCallback, const TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& Children, bool bHasDifferences)
{
	const auto CreateDefaultsRootEntry = [](FLinearColor Color) -> TSharedRef<SWidget>
	{
		return SNew(STextBlock)
			.ToolTipText(NSLOCTEXT("FBlueprintDifferenceTreeEntry", "DefaultsTooltip", "The list of changes made in the Defaults panel"))
			.ColorAndOpacity(Color)
			.Text(NSLOCTEXT("FBlueprintDifferenceTreeEntry", "DefaultsLabel", "Defaults"));
	};

	return TSharedPtr<FBlueprintDifferenceTreeEntry>(new FBlueprintDifferenceTreeEntry(
		FocusCallback
		, FGenerateDiffEntryWidget::CreateStatic(CreateDefaultsRootEntry, DiffViewUtils::LookupColor(bHasDifferences) )
		, Children
	));
}

TSharedPtr<FBlueprintDifferenceTreeEntry> FBlueprintDifferenceTreeEntry::CreateDefaultsCategoryEntryForMerge(FOnDiffEntryFocused FocusCallback, const TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& Children, bool bHasRemoteDifferences, bool bHasLocalDifferences, bool bHasConflicts)
{
	const auto CreateDefaultsRootEntry = [](bool bInHasRemoteDifferences, bool bInHasLocalDifferences, bool bInHasConflicts) -> TSharedRef<SWidget>
	{
		const FLinearColor BaseColor = DiffViewUtils::LookupColor(bInHasRemoteDifferences || bInHasLocalDifferences, bInHasConflicts);
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				SNew(STextBlock)
				.ToolTipText(NSLOCTEXT("FBlueprintDifferenceTreeEntry", "DefaultsTooltip", "The list of changes made in the Defaults panel"))
				.ColorAndOpacity(BaseColor)
				.Text(NSLOCTEXT("FBlueprintDifferenceTreeEntry", "DefaultsLabel", "Defaults"))
			]
			+ DiffViewUtils::Box(true, DiffViewUtils::LookupColor(bInHasRemoteDifferences, bInHasConflicts))
			+ DiffViewUtils::Box(true, BaseColor)
			+ DiffViewUtils::Box(true, DiffViewUtils::LookupColor(bInHasLocalDifferences, bInHasConflicts));
	};

	return TSharedPtr<FBlueprintDifferenceTreeEntry>(new FBlueprintDifferenceTreeEntry(
		FocusCallback
		, FGenerateDiffEntryWidget::CreateStatic(CreateDefaultsRootEntry, bHasRemoteDifferences, bHasLocalDifferences, bHasConflicts)
		, Children
		));
}

TSharedPtr<FBlueprintDifferenceTreeEntry> FBlueprintDifferenceTreeEntry::CreateComponentsCategoryEntry(FOnDiffEntryFocused FocusCallback, const TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& Children, bool bHasDifferences)
{
	const auto CreateComponentsRootEntry = [](FLinearColor Color) -> TSharedRef<SWidget>
	{
		return SNew(STextBlock)
			.ToolTipText(NSLOCTEXT("FBlueprintDifferenceTreeEntry", "SCSTooltip", "The list of changes made in the Components panel"))
			.ColorAndOpacity(Color)
			.Text(NSLOCTEXT("FBlueprintDifferenceTreeEntry", "SCSLabel", "Components"));
	};

	return TSharedPtr<FBlueprintDifferenceTreeEntry>(new FBlueprintDifferenceTreeEntry(
		FocusCallback
		, FGenerateDiffEntryWidget::CreateStatic(CreateComponentsRootEntry, DiffViewUtils::LookupColor(bHasDifferences))
		, Children
		));
}

TSharedPtr<FBlueprintDifferenceTreeEntry> FBlueprintDifferenceTreeEntry::CreateComponentsCategoryEntryForMerge(FOnDiffEntryFocused FocusCallback, const TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& Children, bool bHasRemoteDifferences, bool bHasLocalDifferences, bool bHasConflicts)
{
	const auto CreateComponentsRootEntry = [](bool bInHasRemoteDifferences, bool bInHasLocalDifferences, bool bInHasConflicts) -> TSharedRef<SWidget>
	{
		const FLinearColor BaseColor = DiffViewUtils::LookupColor(bInHasRemoteDifferences || bInHasLocalDifferences, bInHasConflicts);
		return  SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				SNew(STextBlock)
				.ToolTipText(NSLOCTEXT("FBlueprintDifferenceTreeEntry", "SCSTooltip", "The list of changes made in the Components panel"))
				.ColorAndOpacity(BaseColor)
				.Text(NSLOCTEXT("FBlueprintDifferenceTreeEntry", "SCSLabel", "Components"))
			]
			+ DiffViewUtils::Box(true, DiffViewUtils::LookupColor(bInHasRemoteDifferences, bInHasConflicts))
			+ DiffViewUtils::Box(true, BaseColor)
			+ DiffViewUtils::Box(true, DiffViewUtils::LookupColor(bInHasLocalDifferences, bInHasConflicts));
	};

	return TSharedPtr<FBlueprintDifferenceTreeEntry>(new FBlueprintDifferenceTreeEntry(
		FocusCallback
		, FGenerateDiffEntryWidget::CreateStatic(CreateComponentsRootEntry, bHasRemoteDifferences, bHasLocalDifferences, bHasConflicts)
		, Children
	));
}

TSharedRef< STreeView<TSharedPtr< FBlueprintDifferenceTreeEntry > > > DiffTreeView::CreateTreeView(TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >* DifferencesList)
{
	const auto RowGenerator = [](TSharedPtr< FBlueprintDifferenceTreeEntry > Entry, const TSharedRef<STableViewBase>& Owner) -> TSharedRef< ITableRow >
	{
		return SNew(STableRow<TSharedPtr<FBlueprintDifferenceTreeEntry> >, Owner)
			[
				Entry->GenerateWidget.Execute()
			];
	};

	const auto ChildrenAccessor = [](TSharedPtr<FBlueprintDifferenceTreeEntry> InTreeItem, TArray< TSharedPtr< FBlueprintDifferenceTreeEntry > >& OutChildren, TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >* MasterList)
	{
		OutChildren = InTreeItem->Children;
	};

	const auto Selector = [](TSharedPtr<FBlueprintDifferenceTreeEntry> InTreeItem, ESelectInfo::Type Type)
	{
		if (InTreeItem.IsValid())
		{
			InTreeItem->OnFocus.ExecuteIfBound();
		}
	};

	return SNew(STreeView< TSharedPtr< FBlueprintDifferenceTreeEntry > >)
		.OnGenerateRow(STreeView< TSharedPtr< FBlueprintDifferenceTreeEntry > >::FOnGenerateRow::CreateStatic(RowGenerator))
		.OnGetChildren(STreeView< TSharedPtr< FBlueprintDifferenceTreeEntry > >::FOnGetChildren::CreateStatic(ChildrenAccessor, DifferencesList))
		.OnSelectionChanged(STreeView< TSharedPtr< FBlueprintDifferenceTreeEntry > >::FOnSelectionChanged::CreateStatic(Selector))
		.TreeItemsSource(DifferencesList);
}

int32 DiffTreeView::CurrentDifference(TSharedRef< STreeView<TSharedPtr< FBlueprintDifferenceTreeEntry > > > TreeView, const TArray< TSharedPtr<class FBlueprintDifferenceTreeEntry> >& Differences)
{
	auto SelectedItems = TreeView->GetSelectedItems();
	if (SelectedItems.Num() == 0)
	{
		return INDEX_NONE;
	}

	for (int32 Iter = 0; Iter < SelectedItems.Num(); ++Iter)
	{
		int32 Index = Differences.Find(SelectedItems[Iter]);
		if (Index != INDEX_NONE)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

void DiffTreeView::HighlightNextDifference(TSharedRef< STreeView<TSharedPtr< FBlueprintDifferenceTreeEntry > > > TreeView, const TArray< TSharedPtr<class FBlueprintDifferenceTreeEntry> >& Differences, const TArray< TSharedPtr<class FBlueprintDifferenceTreeEntry> >& RootDifferences)
{
	int32 CurrentIndex = CurrentDifference(TreeView, Differences);

	auto Next = Differences[CurrentIndex + 1];
	// we have to manually expand our parent:
	for (auto& Test : RootDifferences)
	{
		if (Test->Children.Contains(Next))
		{
			TreeView->SetItemExpansion(Test, true);
			break;
		}
	}

	TreeView->SetSelection(Next);
	TreeView->RequestScrollIntoView(Next);
}

void DiffTreeView::HighlightPrevDifference(TSharedRef< STreeView<TSharedPtr< FBlueprintDifferenceTreeEntry > > > TreeView, const TArray< TSharedPtr<class FBlueprintDifferenceTreeEntry> >& Differences, const TArray< TSharedPtr<class FBlueprintDifferenceTreeEntry> >& RootDifferences)
{
	int32 CurrentIndex = CurrentDifference(TreeView, Differences);

	auto Prev = Differences[CurrentIndex - 1];
	// we have to manually expand our parent:
	for (auto& Test : RootDifferences)
	{
		if (Test->Children.Contains(Prev))
		{
			TreeView->SetItemExpansion(Test, true);
			break;
		}
	}

	TreeView->SetSelection(Prev);
	TreeView->RequestScrollIntoView(Prev);
}

bool DiffTreeView::HasNextDifference(TSharedRef< STreeView<TSharedPtr< FBlueprintDifferenceTreeEntry > > > TreeView, const TArray< TSharedPtr<class FBlueprintDifferenceTreeEntry> >& Differences)
{
	int32 CurrentIndex = CurrentDifference(TreeView, Differences);
	return Differences.IsValidIndex(CurrentIndex + 1);
}

bool DiffTreeView::HasPrevDifference(TSharedRef< STreeView<TSharedPtr< FBlueprintDifferenceTreeEntry > > > TreeView, const TArray< TSharedPtr<class FBlueprintDifferenceTreeEntry> >& Differences)
{
	int32 CurrentIndex = CurrentDifference(TreeView, Differences);
	return Differences.IsValidIndex(CurrentIndex - 1);
}

FLinearColor DiffViewUtils::LookupColor(bool bDiffers, bool bConflicts)
{
	if( bConflicts )
	{
		return DiffViewUtils::Conflicting();
	}
	else if( bDiffers )
	{
		return DiffViewUtils::Differs();
	}
	else
	{
		return DiffViewUtils::Identical();
	}
}

FLinearColor DiffViewUtils::Differs()
{
	// yellow color
	return FLinearColor(0.85f,0.71f,0.25f);
}

FLinearColor DiffViewUtils::Identical()
{
	return FLinearColor::White;
}

FLinearColor DiffViewUtils::Missing()
{
	// blue color
	return FLinearColor(0.3f,0.3f,1.f);
}

FLinearColor DiffViewUtils::Conflicting()
{
	// red color
	return FLinearColor(1.0f,0.2f,0.3f);
}

FText DiffViewUtils::PropertyDiffMessage(FSingleObjectDiffEntry Difference, FText ObjectName)
{
	FText Message;
	FString PropertyName = Difference.Identifier.LastPropertyName().GetPlainNameString();
	switch (Difference.DiffType)
	{
	case EPropertyDiffType::PropertyAddedToA:
		Message = FText::Format(NSLOCTEXT("DiffViewUtils", "PropertyValueChange", "{0} removed from {1}"), FText::FromString(PropertyName), ObjectName);
		break;
	case EPropertyDiffType::PropertyAddedToB:
		Message = FText::Format(NSLOCTEXT("DiffViewUtils", "PropertyValueChange", "{0} added to {1}"), FText::FromString(PropertyName), ObjectName);
		break;
	case EPropertyDiffType::PropertyValueChanged:
		Message = FText::Format(NSLOCTEXT("DiffViewUtils", "PropertyValueChange", "{0} changed value in {1}"), FText::FromString(PropertyName), ObjectName);
		break;
	}
	return Message;
}

FText DiffViewUtils::SCSDiffMessage(const FSCSDiffEntry& Difference, FText ObjectName)
{
	const FText NodeName = FText::FromName(Difference.TreeIdentifier.Name);
	FText Text;
	switch (Difference.DiffType)
	{
	case ETreeDiffType::NODE_ADDED:
		Text = FText::Format(NSLOCTEXT("DiffViewUtils", "NodeAdded", "Added Node {0} to {1}"), NodeName, ObjectName);
		break;
	case ETreeDiffType::NODE_REMOVED:
		Text = FText::Format(NSLOCTEXT("DiffViewUtils", "NodeAdded", "Removed Node {0} from {1}"), NodeName, ObjectName);
		break;
	case ETreeDiffType::NODE_PROPERTY_CHANGED:
		Text = FText::Format(NSLOCTEXT("DiffViewUtils", "NodeAdded", "{0} on {1}"), DiffViewUtils::PropertyDiffMessage(Difference.PropertyDiff, NodeName), ObjectName);
		break;
	case ETreeDiffType::NODE_MOVED:
		Text = FText::Format(NSLOCTEXT("DiffViewUtils", "NodeAdded", "Moved Node {0} in {1}"), NodeName, ObjectName);
		break;
	}
	return Text;
}

FText DiffViewUtils::GetPanelLabel(const UBlueprint* Blueprint, const FRevisionInfo& Revision, FText Label )
{
	if( !Revision.Revision.IsEmpty() )
	{
		FText RevisionData;
		
		if(ISourceControlModule::Get().GetProvider().UsesChangelists())
		{
			RevisionData = FText::Format(NSLOCTEXT("DiffViewUtils", "RevisionData", "Revision {0} - CL {1} - {2}")
				, FText::FromString(Revision.Revision)
				, FText::AsNumber(Revision.Changelist, &FNumberFormattingOptions::DefaultNoGrouping())
				, FText::FromString(Revision.Date.ToString(TEXT("%m/%d/%Y"))));
		}
		else
		{
			RevisionData = FText::Format(NSLOCTEXT("DiffViewUtils", "RevisionDataNoChangelist", "Revision {0} - {1}")
				, FText::FromString(Revision.Revision)
				, FText::FromString(Revision.Date.ToString(TEXT("%m/%d/%Y"))));		
		}

		return FText::Format( NSLOCTEXT("DiffViewUtils", "RevisionLabel", "{0}\n{1}\n{2}")
			, Label
			, FText::FromString( Blueprint->GetName() )
			, RevisionData );
	}
	else
	{
		if( Blueprint )
		{
			return FText::Format( NSLOCTEXT("DiffViewUtils", "RevisionLabel", "{0}\n{1}\n{2}")
				, Label
				, FText::FromString( Blueprint->GetName() )
				, NSLOCTEXT("DiffViewUtils", "LocalRevisionLabel", "Local Revision" ));
		}

		return NSLOCTEXT("DiffViewUtils", "NoBlueprint", "None" );
	}
}

SHorizontalBox::FSlot& DiffViewUtils::Box(bool bIsPresent, FLinearColor Color)
{
	return SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.MaxWidth(8.0f)
		[
			SNew(SImage)
			.ColorAndOpacity(Color)
			.Image(bIsPresent ? FEditorStyle::GetBrush("BlueprintDif.HasGraph") : FEditorStyle::GetBrush("BlueprintDif.MissingGraph"))
		];
};

