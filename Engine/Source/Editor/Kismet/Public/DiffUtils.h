// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "PropertyPath.h"
#include "IAssetTypeActions.h"

struct FResolvedProperty
{
	explicit FResolvedProperty()
	: Object(nullptr)
	, Property(nullptr)
	{
	}

	FResolvedProperty(const UObject* InObject, const UProperty* InProperty)
		: Object(InObject)
		, Property(InProperty)
	{
	}

	inline bool operator==(const FResolvedProperty& RHS) const
	{
		return Object == RHS.Object && Property == RHS.Property;
	}

	inline bool operator!=(const FResolvedProperty& RHS) const { return !(*this == RHS); }

	const UObject* Object;
	const UProperty* Property;
};

/**
 * FPropertySoftPath is a string of identifiers used to identify a single member of a UObject. It is primarily
 * used when comparing unrelated UObjects for Diffing and Merging, but can also be used as a key select
 * a property in a SDetailsView.
 */
struct FPropertySoftPath
{
	FPropertySoftPath()
	{
	}

	FPropertySoftPath(TArray<FName> InPropertyChain)
		: PropertyChain(InPropertyChain)
	{
	}

	FPropertySoftPath( FPropertyPath InPropertyPath )
	{
		for( int32 i = 0, end = InPropertyPath.GetNumProperties(); i != end; ++i )
		{
			PropertyChain.Push( InPropertyPath.GetPropertyInfo(i).Property->GetFName() );
		}
	}

	FResolvedProperty Resolve(const UObject* Object) const;
	FPropertyPath ResolvePath(const UObject* Object) const;
	FName LastPropertyName() const;

	inline bool operator==(FPropertySoftPath const& RHS) const
	{
		return PropertyChain == RHS.PropertyChain;
	}

	inline bool operator!=(FPropertySoftPath const& RHS ) const
	{
		return !(*this == RHS);
	}
private:
	friend uint32 GetTypeHash( FPropertySoftPath const& Path );
	TArray<FName> PropertyChain;
};

struct FSCSIdentifier
{
	FName Name;
	TArray< int32 > TreeLocation;
};

struct FSCSResolvedIdentifier
{
	FSCSIdentifier Identifier;
	const UObject* Object;
};

FORCEINLINE bool operator==( const FSCSIdentifier& A, const FSCSIdentifier& B )
{
	return A.Name == B.Name && A.TreeLocation == B.TreeLocation;
}

FORCEINLINE bool operator!=(const FSCSIdentifier& A, const FSCSIdentifier& B)
{
	return !(A == B);
}

FORCEINLINE FName FPropertySoftPath::LastPropertyName() const
{
	if( PropertyChain.Num() != 0 )
	{
		return PropertyChain.Last();
	}
	return FName();
}

FORCEINLINE uint32 GetTypeHash( FPropertySoftPath const& Path )
{
	uint32 Ret = 0;
	for( const auto& ProperytName : Path.PropertyChain )
	{
		Ret = Ret ^ GetTypeHash(ProperytName);
	}
	return Ret;
}

// Trying to restrict us to this typedef because I'm a little skeptical about hashing FPropertySoftPath safely
typedef TSet< FPropertySoftPath > FPropertySoftPathSet;

namespace EPropertyDiffType
{
	enum Type
	{
		Invalid,

		PropertyAddedToA,
		PropertyAddedToB,
		PropertyValueChanged,
	};
}

struct FSingleObjectDiffEntry
{
	FSingleObjectDiffEntry()
		: Identifier()
		, DiffType(EPropertyDiffType::Invalid)
	{
	}

	FSingleObjectDiffEntry(const FPropertySoftPath& InIdentifier, EPropertyDiffType::Type InDiffType )
		: Identifier(InIdentifier)
		, DiffType(InDiffType)
	{
	}

	FPropertySoftPath Identifier;
	EPropertyDiffType::Type DiffType;
};

namespace ETreeDiffType
{
	enum Type
	{
		NODE_ADDED,
		NODE_REMOVED,
		NODE_PROPERTY_CHANGED,
		NODE_MOVED,
		/** We could potentially try to identify hierarchy reorders separately from add/remove */
	};
}

struct FSCSDiffEntry
{
	FSCSDiffEntry( const FSCSIdentifier& InIdentifier, ETreeDiffType::Type InDiffType, const FSingleObjectDiffEntry& InPropertyDiff )
		: TreeIdentifier(InIdentifier)
		, DiffType(InDiffType)
		, PropertyDiff(InPropertyDiff)
	{
	}

	FSCSIdentifier TreeIdentifier;
	ETreeDiffType::Type DiffType;
	FSingleObjectDiffEntry PropertyDiff;
};

struct FSCSDiffRoot
{
	// use indices in FSCSIdentifier::TreeLocation to find hierarchy..
	TArray< FSCSDiffEntry > Entries;
};

namespace DiffUtils
{
	KISMET_API const UObject* GetCDO(const UBlueprint* ForBlueprint);
	KISMET_API void CompareUnrelatedObjects(const UObject* A, const UObject* B, TArray<FSingleObjectDiffEntry>& OutDifferingProperties);
	KISMET_API void CompareUnrelatedSCS(const UBlueprint* Old, const TArray< FSCSResolvedIdentifier >& OldHierarchy, const UBlueprint* New, const TArray< FSCSResolvedIdentifier >& NewHierarchy, FSCSDiffRoot& OutDifferingEntries );
	KISMET_API bool Identical(const FResolvedProperty& AProp, const FResolvedProperty& BProp);
	TArray<FPropertySoftPath> GetVisiblePropertiesInOrderDeclared(const UObject* ForObj, const TArray<FName>& Scope = TArray<FName>());

	KISMET_API TArray<FPropertyPath> ResolveAll(const UObject* Object, const TArray<FPropertySoftPath>& InSoftProperties);
	KISMET_API TArray<FPropertyPath> ResolveAll(const UObject* Object, const TArray<FSingleObjectDiffEntry>& InDifferences);
}

DECLARE_DELEGATE(FOnDiffEntryFocused);
DECLARE_DELEGATE_RetVal(TSharedRef<SWidget>, FGenerateDiffEntryWidget);

class FBlueprintDifferenceTreeEntry
{
public:
	FBlueprintDifferenceTreeEntry( FOnDiffEntryFocused InOnFocus, FGenerateDiffEntryWidget InGenerateWidget, TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> > InChildren ) 
		: OnFocus(InOnFocus)
		, GenerateWidget(InGenerateWidget)
		, Children(InChildren) 
	{
		check( InGenerateWidget.IsBound() );
	}

	/** The FBlueprintDifferenceTreeEntry used to display a message to the user explaining that there are no differences: */
	KISMET_API static TSharedPtr<FBlueprintDifferenceTreeEntry> NoDifferencesEntry();
	/** The FBlueprintDifferenceTreeEntry used to label the defaults category: */
	KISMET_API static TSharedPtr<FBlueprintDifferenceTreeEntry> CreateDefaultsCategoryEntry(FOnDiffEntryFocused FocusCallback, const TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& Children, bool bHasDifferences );
	KISMET_API static TSharedPtr<FBlueprintDifferenceTreeEntry> CreateDefaultsCategoryEntryForMerge(FOnDiffEntryFocused FocusCallback, const TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& Children, bool bHasRemoteDifferences, bool bHasLocalDifferences, bool bHasConflicts );
	KISMET_API static TSharedPtr<FBlueprintDifferenceTreeEntry> CreateComponentsCategoryEntry(FOnDiffEntryFocused FocusCallback, const TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& Children, bool bHasDifferences);
	KISMET_API static TSharedPtr<FBlueprintDifferenceTreeEntry> CreateComponentsCategoryEntryForMerge(FOnDiffEntryFocused FocusCallback, const TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& Children, bool bHasRemoteDifferences, bool bHasLocalDifferences, bool bHasConflicts);
	
	FOnDiffEntryFocused OnFocus;
	FGenerateDiffEntryWidget GenerateWidget;
	TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> > Children;
};

namespace DiffTreeView
{
	KISMET_API TSharedRef< STreeView<TSharedPtr< FBlueprintDifferenceTreeEntry > > > CreateTreeView(TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >* DifferencesList);
	KISMET_API int32 CurrentDifference( TSharedRef< STreeView<TSharedPtr< FBlueprintDifferenceTreeEntry > > > TreeView, const TArray< TSharedPtr<class FBlueprintDifferenceTreeEntry> >& Differences );
	KISMET_API void HighlightNextDifference(TSharedRef< STreeView<TSharedPtr< FBlueprintDifferenceTreeEntry > > > TreeView, const TArray< TSharedPtr<class FBlueprintDifferenceTreeEntry> >& Differences, const TArray< TSharedPtr<class FBlueprintDifferenceTreeEntry> >& RootDifferences);
	KISMET_API void HighlightPrevDifference(TSharedRef< STreeView<TSharedPtr< FBlueprintDifferenceTreeEntry > > > TreeView, const TArray< TSharedPtr<class FBlueprintDifferenceTreeEntry> >& Differences, const TArray< TSharedPtr<class FBlueprintDifferenceTreeEntry> >& RootDifferences);
	KISMET_API bool HasNextDifference(TSharedRef< STreeView<TSharedPtr< FBlueprintDifferenceTreeEntry > > > TreeView, const TArray< TSharedPtr<class FBlueprintDifferenceTreeEntry> >& Differences);
	KISMET_API bool HasPrevDifference(TSharedRef< STreeView<TSharedPtr< FBlueprintDifferenceTreeEntry > > > TreeView, const TArray< TSharedPtr<class FBlueprintDifferenceTreeEntry> >& Differences);
}

namespace DiffViewUtils
{
	KISMET_API FLinearColor LookupColor( bool bDiffers, bool bConflicts = false );
	KISMET_API FLinearColor Differs();
	KISMET_API FLinearColor Identical();
	KISMET_API FLinearColor Missing();
	KISMET_API FLinearColor Conflicting();

	KISMET_API FText PropertyDiffMessage(FSingleObjectDiffEntry Difference, FText ObjectName);
	KISMET_API FText SCSDiffMessage(const FSCSDiffEntry& Difference, FText ObjectName);
	KISMET_API FText GetPanelLabel( const UBlueprint* Blueprint, const FRevisionInfo& Revision, FText Label );

	KISMET_API SHorizontalBox::FSlot& Box(bool bIsPresent, FLinearColor Color);
}
