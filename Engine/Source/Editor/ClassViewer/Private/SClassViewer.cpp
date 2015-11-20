// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ClassViewerPrivatePCH.h"

#include "EditorWidgets.h"
#include "Editor/ClassViewer/Private/SClassViewer.h"
#include "Editor/UnrealEd/Public/DragAndDrop/ClassDragDropOp.h"
#include "Editor/UnrealEd/Public/DragAndDrop/AssetDragDropOp.h"
#include "Editor/UnrealEd/Public/ClassIconFinder.h"
#include "Editor/ContentBrowser/Public/ContentBrowserModule.h"

#include "Kismet2/KismetEditorUtilities.h"
#include "Toolkits/AssetEditorManager.h"
#include "PackageTools.h"
#include "MessageLog.h"

#include "AssetRegistryModule.h"
#include "AssetSelection.h"
#include "AssetToolsModule.h"

#include "ClassViewerNode.h"

#include "UnloadedBlueprintData.h"

#include "EditorClassUtils.h"
#include "IDocumentation.h"

#include "PropertyEditorModule.h"
#include "PropertyHandle.h"

#include "GameProjectGenerationModule.h"

#include "SourceCodeNavigation.h"
#include "HotReloadInterface.h"
#include "SSearchBox.h"

#include "SListViewSelectorDropdownMenu.h"
#include "Engine/BlueprintGeneratedClass.h"

#define LOCTEXT_NAMESPACE "SClassViewer"

DEFINE_LOG_CATEGORY_STATIC(LogEditorClassViewer, Log, All);

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

EFilterReturn::Type FClassViewerFilterFuncs::IfInChildOfClassesSet(TSet< const UClass* >& InSet, const UClass* InClass)
{
	check(InClass);

	if(InSet.Num())
	{
		// If a class is a child of any classes on this list, it will be allowed onto the list, unless it also appears on a disallowed list.
		for( auto CurClassIt = InSet.CreateConstIterator(); CurClassIt; ++CurClassIt )
		{
			if(InClass->IsChildOf(*CurClassIt))
			{
				return EFilterReturn::Passed;
			}
		}

		return EFilterReturn::Failed;
	}

	// Since there are none on this list, return that there is no items.
	return EFilterReturn::NoItems;
}

EFilterReturn::Type FClassViewerFilterFuncs::IfInChildOfClassesSet(TSet< const UClass* >& InSet, const TSharedPtr< const IUnloadedBlueprintData > InClass)
{
	check(InClass.IsValid());

	if(InSet.Num())
	{
		// If a class is a child of any classes on this list, it will be allowed onto the list, unless it also appears on a disallowed list.
		for( auto CurClassIt = InSet.CreateConstIterator(); CurClassIt; ++CurClassIt )
		{
			if(InClass->IsChildOf(*CurClassIt))
			{
				return EFilterReturn::Passed;
			}
		}

		return EFilterReturn::Failed;
	}

	// Since there are none on this list, return that there is no items.
	return EFilterReturn::NoItems;
}

EFilterReturn::Type FClassViewerFilterFuncs::IfMatchesAllInChildOfClassesSet(TSet< const UClass* >& InSet, const UClass* InClass)
{
	check(InClass);

	if(InSet.Num())
	{
		// If a class is a child of any classes on this list, it will be allowed onto the list, unless it also appears on a disallowed list.
		for( auto CurClassIt = InSet.CreateConstIterator(); CurClassIt; ++CurClassIt )
		{
			if(!InClass->IsChildOf(*CurClassIt))
			{
				// Since it doesn't match one, it fails.
				return EFilterReturn::Failed;
			}
		}

		// It matches all of them, so it passes.
		return EFilterReturn::Passed;
	}

	// Since there are none on this list, return that there is no items.
	return EFilterReturn::NoItems;
}

EFilterReturn::Type FClassViewerFilterFuncs::IfMatchesAllInChildOfClassesSet(TSet< const UClass* >& InSet, const TSharedPtr< const IUnloadedBlueprintData > InClass)
{
	check(InClass.IsValid());

	if(InSet.Num())
	{
		// If a class is a child of any classes on this list, it will be allowed onto the list, unless it also appears on a disallowed list.
		for( auto CurClassIt = InSet.CreateConstIterator(); CurClassIt; ++CurClassIt )
		{
			if(!InClass->IsChildOf(*CurClassIt))
			{
				// Since it doesn't match one, it fails.
				return EFilterReturn::Failed;
			}
		}

		// It matches all of them, so it passes.
		return EFilterReturn::Passed;
	}

	// Since there are none on this list, return that there is no items.
	return EFilterReturn::NoItems;
}

EFilterReturn::Type FClassViewerFilterFuncs::IfMatchesAll_ObjectsSetIsAClass(TSet< const UObject* >& InSet, const UClass* InClass)
{
	check(InClass);

	if(InSet.Num())
	{
		// If a class is a child of any classes on this list, it will be allowed onto the list, unless it also appears on a disallowed list.
		for( auto CurClassIt = InSet.CreateConstIterator(); CurClassIt; ++CurClassIt )
		{
			if(!(*CurClassIt)->IsA(InClass))
			{
				// Since it doesn't match one, it fails.
				return EFilterReturn::Failed;
			}
		}

		// It matches all of them, so it passes.
		return EFilterReturn::Passed;
	}

	// Since there are none on this list, return that there is no items.
	return EFilterReturn::NoItems;
}

EFilterReturn::Type FClassViewerFilterFuncs::IfMatchesAll_ObjectsSetIsAClass(TSet< const UObject* >& InSet, const TSharedPtr< const IUnloadedBlueprintData > InClass)
{
	check(InClass.IsValid());

	if(InSet.Num())
	{
		// If a class is a child of any classes on this list, it will be allowed onto the list, unless it also appears on a disallowed list.
		for( auto CurClassIt = InSet.CreateConstIterator(); CurClassIt; ++CurClassIt )
		{
			if(!(*CurClassIt)->IsA(UBlueprintGeneratedClass::StaticClass()))
			{
				// Since it doesn't match one, it fails.
				return EFilterReturn::Failed;
			}
		}

		// It matches all of them, so it passes.
		return EFilterReturn::Passed;
	}

	// Since there are none on this list, return that there is no items.
	return EFilterReturn::NoItems;
}

EFilterReturn::Type FClassViewerFilterFuncs::IfMatchesAll_ClassesSetIsAClass(TSet< const UClass* >& InSet, const UClass* InClass)
{
	check(InClass);

	if(InSet.Num())
	{
		// If a class is a child of any classes on this list, it will be allowed onto the list, unless it also appears on a disallowed list.
		for( auto CurClassIt = InSet.CreateConstIterator(); CurClassIt; ++CurClassIt )
		{
			const UObject* Object = *CurClassIt;
			if(!Object->IsA(InClass))
			{
				// Since it doesn't match one, it fails.
				return EFilterReturn::Failed;
			}
		}

		// It matches all of them, so it passes.
		return EFilterReturn::Passed;
	}

	// Since there are none on this list, return that there is no items.
	return EFilterReturn::NoItems;
}

EFilterReturn::Type FClassViewerFilterFuncs::IfMatchesAll_ClassesSetIsAClass(TSet< const UClass* >& InSet, const TSharedPtr< const IUnloadedBlueprintData > InClass)
{
	check(InClass.IsValid());

	if(InSet.Num())
	{
		// If a class is a child of any classes on this list, it will be allowed onto the list, unless it also appears on a disallowed list.
		for( auto CurClassIt = InSet.CreateConstIterator(); CurClassIt; ++CurClassIt )
		{
			const UObject* Object = *CurClassIt;
			if(!Object->IsA(UBlueprintGeneratedClass::StaticClass()))
			{
				// Since it doesn't match one, it fails.
				return EFilterReturn::Failed;
			}
		}

		// It matches all of them, so it passes.
		return EFilterReturn::Passed;
	}

	// Since there are none on this list, return that there is no items.
	return EFilterReturn::NoItems;
}

EFilterReturn::Type FClassViewerFilterFuncs::IfMatches_ClassesSetIsAClass(TSet< const UClass* >& InSet, const UClass* InClass)
{
	check(InClass);

	if(InSet.Num())
	{
		// If a class is a child of any classes on this list, it will be allowed onto the list, unless it also appears on a disallowed list.
		for( auto CurClassIt = InSet.CreateConstIterator(); CurClassIt; ++CurClassIt )
		{
			const UObject* Object = *CurClassIt;
			if(Object->IsA(InClass))
			{
				return EFilterReturn::Passed;
			}
		}

		return EFilterReturn::Failed;
	}

	// Since there are none on this list, return that there is no items.
	return EFilterReturn::NoItems;
}

EFilterReturn::Type FClassViewerFilterFuncs::IfMatches_ClassesSetIsAClass(TSet< const UClass* >& InSet, const TSharedPtr< const IUnloadedBlueprintData > InClass)
{
	check(InClass.IsValid());

	if(InSet.Num())
	{
		// If a class is a child of any classes on this list, it will be allowed onto the list, unless it also appears on a disallowed list.
		for( auto CurClassIt = InSet.CreateConstIterator(); CurClassIt; ++CurClassIt )
		{
			const UObject* Object = *CurClassIt;
			if(Object->IsA(UBlueprintGeneratedClass::StaticClass()))
			{
				return EFilterReturn::Passed;
			}
		}

		return EFilterReturn::Failed;
	}

	// Since there are none on this list, return that there is no items.
	return EFilterReturn::NoItems;
}

EFilterReturn::Type FClassViewerFilterFuncs::IfInClassesSet(TSet< const UClass* >& InSet, const UClass* InClass)
{
	check(InClass);

	if(InSet.Num())
	{
		for( auto CurClassIt = InSet.CreateConstIterator(); CurClassIt; ++CurClassIt )
		{
			if(InClass == *CurClassIt)
			{
				return EFilterReturn::Passed;
			}
		}
		return EFilterReturn::Failed;
	}

	// Since there are none on this list, return that there is no items.
	return EFilterReturn::NoItems;
}

EFilterReturn::Type FClassViewerFilterFuncs::IfInClassesSet(TSet< const UClass* >& InSet, const TSharedPtr< const IUnloadedBlueprintData > InClass)
{
	check(InClass.IsValid());

	if(InSet.Num())
	{
		for( auto CurClassIt = InSet.CreateConstIterator(); CurClassIt; ++CurClassIt )
		{
			const TSharedPtr<const FUnloadedBlueprintData> UnloadedBlueprintData = StaticCastSharedPtr<const FUnloadedBlueprintData>(InClass);
			if(*UnloadedBlueprintData->GetClassViewerNode().Pin()->GetClassName() == (*CurClassIt)->GetName())
			{
				return EFilterReturn::Passed;
			}
		}
		return EFilterReturn::Failed;
	}

	// Since there are none on this list, return that there is no items.
	return EFilterReturn::NoItems;
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

// A hack to make IMPLEMENT_COMPARE_CONSTREF with a templated type
typedef TSharedPtr<FClassViewerNode> FClassViewerNodeSharedPtr;

FORCEINLINE bool CompareFClassViewerNodes( const FClassViewerNodeSharedPtr& A, const FClassViewerNodeSharedPtr& B )
{
	check(A.IsValid());
	check(B.IsValid());

	// Pull out the FString, for ease of reading.
	FString AString = *A->GetClassName().Get();
	FString BString = *B->GetClassName().Get();

	return *A->GetClassName().Get() < *B->GetClassName().Get();
}

class FClassHierarchy
{
public:
	FClassHierarchy();
	~FClassHierarchy();

	/** Populates the class hierarchy tree, pulling all the loaded and unloaded classes into a master tree. */
	void PopulateClassHierarchy();
	void PopulateClassHierarchy(const FAssetData& InAssetData) { PopulateClassHierarchy(); }

	/** Recursive function to sort a tree.
	 *	@param InOutRootNode						The current node to sort.
	 */	
	void SortChildren( TSharedPtr< FClassViewerNode >& InRootNode );

	/** Checks if a particular class is placeable.
	 *	@return The ObjectClassRoot for building a duplicate tree using.
	 */
	const TSharedPtr< FClassViewerNode > GetObjectRootNode() const
	{
		// This node should always be valid.
		check(ObjectClassRoot.IsValid())

		return ObjectClassRoot;
	}

	/** Finds the parent of a node, recursively going deeper into the hierarchy.
	 *	@param InRootNode							The current class node to examine.
	 *	@param InParentClassName					The classname to look for.
	 *	@param InParentClass						The parent class to look for.
	 *
	 *	@return The parent node.
	 */
	TSharedPtr< FClassViewerNode > FindParent(const TSharedPtr< FClassViewerNode >& InRootNode, FName InParentClassname, const UClass* InParentClass);

	/** Updates the Class of a node. Uses the generated class package name to find the node.
	 *	@param InGeneratedClassPackageName			The name of the generated class package to find the node for.
	 *	@param InNewClass							The class to update the node with.
	*/
	void UpdateClassInNode(const FString& InGeneratedClassPackageName, UClass* InNewClass, UBlueprint* InNewBluePrint );

private:
	/** Recursive function to build a tree, will not filter.
	 *	@param InOutRootNode						The node that this function will add the children of to the tree.
	 *  @param PackageNameToAssetDataMap			The asset registry map of blueprint package names to blueprint data
	 */	
	void AddChildren_NoFilter( TSharedPtr< FClassViewerNode >& InOutRootNode, const TMultiMap<FName,FAssetData>& BlueprintPackageToAssetDataMap );

	/** Called when hot reload has finished */
	void OnHotReload( bool bWasTriggeredAutomatically );

	/** Finds the node, recursively going deeper into the hierarchy. Does so by comparing generated class package names.
	 *	@param InGeneratedClassPackageName			The name of the generated class package to find the node for.
	 *
	 *	@return The node.
	 */
	TSharedPtr< FClassViewerNode > FindNodeByGeneratedClassPackageName(const TSharedPtr< FClassViewerNode >& InRootNode, const FString& InGeneratedClassPackageName);

	/** 
	 * Loads the tag data for an unloaded blueprint asset.
	 *
	 * @param InOutClassViewerNode		The node to save all the data into.
	 * @param InAssetData				The asset data to pull the tags from.
	 */
	void LoadUnloadedTagData(TSharedPtr<FClassViewerNode>& InOutClassViewerNode, const FAssetData& InAssetData);

	/**
	 * Finds the UClass and UBlueprint for the passed in node, utilizing unloaded data to find it.
	 *
	 * @param InOutClassNode		The node to find the class and fill out.
	 */
	void FindClass(TSharedPtr< FClassViewerNode > InOutClassNode);

	/** 
	 * Recursively searches through the hierarchy to find and remove the asset. Used when deleting assets.
	 *
	 * @param InRootNode		The node to start the search with.
	 * @param InAssetPackage	The package name of the asset to delete.
	 *
	 * @return Returns true if the asset was found and deleted successfully.
	 */
	bool FindAndRemoveNodeByPackageName(const TSharedPtr< FClassViewerNode >& InRootNode, const FString& InAssetPackage);

	/** Callback registered to the Asset Registry to be notified when an asset is added. */
	void AddAsset(const FAssetData& InAddedAssetData);

	/** Callback registered to the Asset Registry to be notified when an asset is removed. */
	void RemoveAsset(const FAssetData& InRemovedAssetData);
private:
	/** The "Object" class node that is used as a rooting point for the Class Viewer. */
	TSharedPtr< FClassViewerNode > ObjectClassRoot;

	/** Handles to various registered RequestPopulateClassHierarchy delegates */
	FDelegateHandle OnFilesLoadedRequestPopulateClassHierarchyDelegateHandle;
	FDelegateHandle OnBlueprintCompiledRequestPopulateClassHierarchyDelegateHandle;
	FDelegateHandle OnClassPackageLoadedOrUnloadedRequestPopulateClassHierarchyDelegateHandle;
};

namespace ClassViewer
{
	namespace Helpers
	{
		DECLARE_MULTICAST_DELEGATE( FPopulateClassViewer );

		/** The class hierarchy that manages the unfiltered class tree for the Class Viewer. */
		static TSharedPtr< FClassHierarchy > ClassHierarchy;

		/** Used to inform any registered Class Viewers to refresh. */
		static FPopulateClassViewer PopulateClassviewerDelegate;

		/** true if the Class Hierarchy should be populated. */
		static bool bPopulateClassHierarchy;

		// Pre-declare these functions.
		static bool CheckIfBlueprintBase( TSharedPtr< FClassViewerNode> InNode );
		static UBlueprint* GetBlueprint( UClass* InClass );
		static void UpdateClassInNode(const FString& InGeneratedClassPackageName, UClass* InNewClass, UBlueprint* InNewBluePrint );

		/** Util class to checks if a particular class can be made into a Blueprint, ignores deprecation
		 *
		 * @param InClass					The class to verify can be made into a Blueprint
		 * @return							TRUE if the class can be made into a Blueprint
		 */
		bool CanCreateBlueprintOfClass_IgnoreDeprecation(UClass* InClass)
		{
			// Temporarily remove the deprecated flag so we can check if it is valid for
			bool bIsClassDeprecated = InClass->HasAnyClassFlags(CLASS_Deprecated);
			InClass->ClassFlags &= ~CLASS_Deprecated;

			bool bCanCreateBlueprintOfClass = FKismetEditorUtilities::CanCreateBlueprintOfClass( InClass );

			// Reassign the deprecated flag if it was previously assigned
			if(bIsClassDeprecated)
			{
				InClass->ClassFlags |= CLASS_Deprecated;
			}

			return bCanCreateBlueprintOfClass;
		}

		/** Checks if a particular class is a brush.
		 *	@param InClass				The Class to check.
		 *	@return Returns true if the class is a brush.
		 */
		static bool IsBrush(const UClass* InClass)
		{
			return InClass->IsChildOf( ABrush::StaticClass() );
		}

		/** Checks if a particular class is placeable.
		 *	@param InClass				The Class to check.
		 *	@return Returns true if the class is placeable.
		 */
		static bool IsPlaceable(const UClass* InClass)
		{
			return !InClass->HasAnyClassFlags(CLASS_Abstract | CLASS_NotPlaceable) && InClass->IsChildOf<AActor>();
		}

		/** Checks if a particular class is abstract.
		 *	@param InClass				The Class to check.
		 *	@return Returns true if the class is abstract.
		 */
		static bool IsAbstract(const UClass* InClass)
		{
			return InClass->HasAnyClassFlags(CLASS_Abstract);
		}

		/** Checks if the class is allowed under the init options of the class viewer currently building it's tree/list.
		 *	@param InInitOptions		The class viewer's options, holds the AllowedClasses and DisallowedClasses.
		 *	@param InClass				The class to test against.
		 */
		static bool IsClassAllowed( const FClassViewerInitializationOptions& InInitOptions, const TWeakObjectPtr<UClass> InClass )
		{
			if(InInitOptions.ClassFilter.IsValid())
			{
				return InInitOptions.ClassFilter->IsClassAllowed(InInitOptions, InClass.Get(), MakeShareable(new FClassViewerFilterFuncs));
			}

			return true;
		}

		/** Checks if the unloaded class is allowed under the init options of the class viewer currently building it's tree/list.
		 *	@param InInitOptions		The class viewer's options, holds the AllowedClasses and DisallowedClasses.
		 *	@param InNode				The node to pull the unloaded class from.
		 */
		bool IsClassAllowed_UnloadedBlueprint(const FClassViewerInitializationOptions& InInitOptions, TSharedPtr< FClassViewerNode > InNode )
		{
			if(InInitOptions.ClassFilter.IsValid() && InNode->UnloadedBlueprintData.IsValid())
			{
				return InInitOptions.ClassFilter->IsUnloadedClassAllowed(InInitOptions, InNode->UnloadedBlueprintData.ToSharedRef(), MakeShareable(new FClassViewerFilterFuncs));
			}

			return true;
		}

		/** Checks if the TestString passes the filter.
		 *	@param InTestString				The string to test against the filter.
		 *	@param InFilterSearchTerms		A list of terms to filter with.
		 *
		 *	@return	true if it passes the filter.
		 */
		static bool PassesFilter( const FString& InTestString, const TArray<FString>& InFilterSearchTerms )
		{
			bool bPassesFilter = false;

			bPassesFilter = !InFilterSearchTerms.Num();

			for( auto CurSearchTermIndex = 0; !bPassesFilter && CurSearchTermIndex < InFilterSearchTerms.Num(); ++CurSearchTermIndex )
			{
				if(InTestString.Contains(InFilterSearchTerms[CurSearchTermIndex]))
				{
					bPassesFilter = true;
					break;
				}
			}

			return bPassesFilter;
		}

		/** Will create the instance of FClassHierarchy and populate the class hierarchy tree. */
		static void ConstructClassHierarchy()
		{
			if(!ClassHierarchy.IsValid())
			{
				ClassHierarchy = MakeShareable(new FClassHierarchy);

				// When created, populate the hierarchy.
				GWarn->BeginSlowTask( LOCTEXT("RebuildingClassHierarchy", "Rebuilding Class Hierarchy"), true );
				ClassHierarchy->PopulateClassHierarchy();
				GWarn->EndSlowTask();
			}
		}

		/** Cleans up the Class Hierarchy */
		static void DestroyClassHierachy()
		{
			ClassHierarchy.Reset();
		}

		/** Will populate the class hierarchy tree if previously requested. */
		static void PopulateClassHierarchy()
		{
			if(bPopulateClassHierarchy)
			{
				bPopulateClassHierarchy = false;

				GWarn->BeginSlowTask( LOCTEXT("RebuildingClassHierarchy", "Rebuilding Class Hierarchy"), true );
				ClassHierarchy->PopulateClassHierarchy();
				GWarn->EndSlowTask();
			}
		}

		/** Will enable the Class Hierarchy to be populated next Tick. */
		static void RequestPopulateClassHierarchy()
		{
			bPopulateClassHierarchy = true;
		}

		/** Refreshes all registered instances of Class Viewer/Pickers. */
		static void RefreshAll()
		{
			ClassViewer::Helpers::PopulateClassviewerDelegate.Broadcast();
		}

		/** Recursive function to build a tree, filtering out nodes based on the InitOptions and filter search terms.
		 *	@param InInitOptions						The class viewer's options, holds the AllowedClasses and DisallowedClasses.
		 *	@param InOutRootNode						The node that this function will add the children of to the tree.
		 *	@param InRootClassIndex						The index of the root node.
		 *	@param InFilterSearchTerms					A list of terms to filter with.
		 *	@param bInOnlyActors						Filter option to remove non-actor classes.
		 *	@param bInOnlyPlaceables					Filter option to remove non-placeable classes.
		 *	@param bInOnlyBlueprintBases				Filter option to remove non-blueprint base classes.
		 *	@param bInShowUnloadedBlueprints			Filter option to not remove unloaded blueprints due to class filter options.
		 *
		 *	@return Returns true if the child passed the filter.
		 */		
		static bool AddChildren_Tree(const FClassViewerInitializationOptions& InInitOptions, TSharedPtr< FClassViewerNode >& InOutRootNode, const TSharedPtr< FClassViewerNode >& InOriginalRootNode, const TArray<FString>& InFilterSearchTerms, bool bInOnlyActors, bool bInOnlyPlaceables, bool bInOnlyBlueprintBases, bool bInShowUnloadedBlueprints )
		{
			if (bInOnlyActors && *InOriginalRootNode->GetClassName().Get() != FString(TEXT("Actor")))
			{
				InOutRootNode->bPassesFilter = false;
				return false;
			}

			bool bChildrenPassesFilter(false);
			bool bReturnPassesFilter(false);

			bool bPassesBlueprintBaseFilter = !bInOnlyBlueprintBases || CheckIfBlueprintBase(InOriginalRootNode);
			bool bIsUnloadedBlueprint = !InOriginalRootNode->Class.IsValid();
			bool bPassesPlaceableFilter = false;

			// When in picker mode, brushes are valid "placeable" actors
			if (bInOnlyPlaceables && InInitOptions.Mode == EClassViewerMode::ClassPicker && IsBrush(InOriginalRootNode->Class.Get()) && IsPlaceable(InOriginalRootNode->Class.Get()))
			{
				bPassesPlaceableFilter = true;
			}
			else
			{
				bPassesPlaceableFilter = !bInOnlyPlaceables || InOriginalRootNode->IsClassPlaceable();
			}

			// There are few options for filtering an unloaded blueprint, if it matches with this filter, it passes.
			if(bIsUnloadedBlueprint)
			{
				if(bInShowUnloadedBlueprints)
				{
					bReturnPassesFilter = InOutRootNode->bPassesFilter = bPassesPlaceableFilter && bPassesBlueprintBaseFilter && IsClassAllowed_UnloadedBlueprint(InInitOptions, InOriginalRootNode) && PassesFilter(*InOriginalRootNode->GetClassName().Get(), InFilterSearchTerms);
				}
			}
			else
			{
				bReturnPassesFilter = InOutRootNode->bPassesFilter = bPassesPlaceableFilter && bPassesBlueprintBaseFilter && IsClassAllowed(InInitOptions, InOriginalRootNode->Class) && PassesFilter(*InOriginalRootNode->GetClassName().Get(), InFilterSearchTerms);
			}

			TArray< TSharedPtr< FClassViewerNode > >& ChildList = InOriginalRootNode->GetChildrenList();
			for(int32 ChildIdx = 0; ChildIdx < ChildList.Num(); ChildIdx++)
			{
				TSharedPtr< FClassViewerNode > NewNode = MakeShareable( new FClassViewerNode( *ChildList[ChildIdx].Get() ) );

				bReturnPassesFilter |= bChildrenPassesFilter = AddChildren_Tree(InInitOptions, NewNode, ChildList[ChildIdx], InFilterSearchTerms, false, /* bInOnlyActors - false so that anything below Actor is added */
					bInOnlyPlaceables, bInOnlyBlueprintBases, bInShowUnloadedBlueprints);
				if(bChildrenPassesFilter)
				{
					InOutRootNode->AddChild(NewNode);
				}

			}

			return bReturnPassesFilter;
		}

		/** Builds the class tree.
		 *	@param InInitOptions						The class viewer's options, holds the AllowedClasses and DisallowedClasses.
		 *	@param InOutRootNode						The node to root the tree to.
		 *	@param InFilterSearchTerms					A list of terms to filter with.
		 *	@param bInOnlyPlaceables					Filter option to remove non-placeable classes.
		 *	@param bInOnlyActors						Filter option to root the tree in the "Actor" class.
		 *	@param bInOnlyBlueprintBases				Filter option to remove non-blueprint base classes.
		 *	@param bInShowUnloadedBlueprints			Filter option to not remove unloaded blueprints due to class filter options.
		 *
		 *	@return A fully built tree.
		 */
		static void GetClassTree(const FClassViewerInitializationOptions& InInitOptions, TSharedPtr< FClassViewerNode >& InOutRootNode, const TArray<FString>& InFilterSearchTerms, bool bInOnlyPlaceables, bool bInOnlyActors, bool bInOnlyBlueprintBases, bool bInShowUnloadedBlueprints )
		{
			const TSharedPtr< FClassViewerNode > ObjectClassRoot = ClassHierarchy->GetObjectRootNode();

			// Duplicate the node, it will have no children.
			InOutRootNode = MakeShareable(new FClassViewerNode(*ObjectClassRoot));

			if(bInOnlyActors)
			{
				for(int32 ClassIdx = 0; ClassIdx < ObjectClassRoot->GetChildrenList().Num(); ClassIdx++)
				{
					TSharedPtr<FClassViewerNode> ChildNode = MakeShareable(new FClassViewerNode(*ObjectClassRoot->GetChildrenList()[ClassIdx].Get()));
					if (AddChildren_Tree(InInitOptions, ChildNode, ObjectClassRoot->GetChildrenList()[ClassIdx], InFilterSearchTerms, true, bInOnlyPlaceables, bInOnlyBlueprintBases, bInShowUnloadedBlueprints))
						InOutRootNode->AddChild(ChildNode);
				}
			}
			else
			{
				AddChildren_Tree(InInitOptions, InOutRootNode, ObjectClassRoot, InFilterSearchTerms, false, bInOnlyPlaceables, bInOnlyBlueprintBases, bInShowUnloadedBlueprints);				
			}
		}

		/** Recursive function to build the list, filtering out nodes based on the InitOptions and filter search terms.
		 *	@param InInitOptions						The class viewer's options, holds the AllowedClasses and DisallowedClasses.
		 *	@param InOutRootNode						The node that this function will add the children of to the tree.
		 *	@param InRootClassIndex						The index of the root node.
		 *	@param InFilterSearchTerms					A list of terms to filter with.
		 *	@param bInOnlyActors						Filter option to remove non-actor classes.
		 *	@param bInOnlyPlaceables					Filter option to remove non-placeable classes.
		 *	@param bInOnlyBlueprintBases				Filter option to remove non-blueprint base classes.
		 *	@param bInShowUnloadedBlueprints			Filter option to not remove unloaded blueprints due to class filter options.
		 *
		 *	@return Returns true if the child passed the filter.
		 */
		static void AddChildren_List( const FClassViewerInitializationOptions& InInitOptions, TArray< TSharedPtr< FClassViewerNode > >& InOutNodeList, const TSharedPtr< FClassViewerNode >& InOriginalRootNode, const TArray<FString>& InFilterSearchTerms, bool bInOnlyActors, bool bInOnlyPlaceables, bool bInOnlyBlueprintBases, bool bInShowUnloadedBlueprints )
		{
			if (bInOnlyActors && *InOriginalRootNode->GetClassName().Get() != FString(TEXT("Actor")))
			{
				return;
			}

			bool bPassesBlueprintBaseFilter = !bInOnlyBlueprintBases || CheckIfBlueprintBase(InOriginalRootNode);
			bool bIsUnloadedBlueprint = !InOriginalRootNode->Class.IsValid();
			bool bPassesPlaceableFilter = false;

			// When in picker mode, brushes are valid "placeable" actors
			if( bInOnlyPlaceables && InInitOptions.Mode == EClassViewerMode::ClassPicker && IsBrush(InOriginalRootNode->Class.Get()) && IsPlaceable(InOriginalRootNode->Class.Get()))
			{
				bPassesPlaceableFilter = true;
			}
			else
			{
				bPassesPlaceableFilter = !bInOnlyPlaceables || InOriginalRootNode->IsClassPlaceable();
			}

			TSharedPtr< FClassViewerNode > NewNode = MakeShareable( new FClassViewerNode( *InOriginalRootNode.Get() ) );
			
			// There are few options for filtering an unloaded blueprint, if it matches with this filter, it passes.
			if(bIsUnloadedBlueprint)
			{
				if(bInShowUnloadedBlueprints)
				{
					NewNode->bPassesFilter = bPassesPlaceableFilter && bPassesBlueprintBaseFilter && IsClassAllowed_UnloadedBlueprint(InInitOptions, InOriginalRootNode) && PassesFilter(*InOriginalRootNode->GetClassName().Get(), InFilterSearchTerms);
				}
			}
			else
			{
				NewNode->bPassesFilter = bPassesPlaceableFilter && bPassesBlueprintBaseFilter && IsClassAllowed(InInitOptions, InOriginalRootNode->Class) && PassesFilter(*InOriginalRootNode->GetClassName().Get(), InFilterSearchTerms);
			}

			if(NewNode->bPassesFilter)
			{
				InOutNodeList.Add(NewNode);
			}

			NewNode->PropertyHandle = InInitOptions.PropertyHandle;

			TArray< TSharedPtr< FClassViewerNode > >& ChildList = InOriginalRootNode->GetChildrenList();
			for(int32 ChildIdx = 0; ChildIdx < ChildList.Num(); ChildIdx++)
			{
				AddChildren_List(InInitOptions, InOutNodeList, ChildList[ChildIdx], InFilterSearchTerms, false, /* bInOnlyActors - false so that anything below Actor is added */
					bInOnlyPlaceables, bInOnlyBlueprintBases, bInShowUnloadedBlueprints);
			}
		}	

		/** Builds the class list.
		 *	@param InInitOptions						The class viewer's options, holds the AllowedClasses and DisallowedClasses.
		 *	@param InOutNodeList						The list to add all the nodes to.
		 *	@param InFilterSearchTerms					A list of terms to filter with.
		 *	@param bInOnlyPlaceables					Filter option to remove non-placeable classes.
		 *	@param bInOnlyActors						Filter option to root the tree in the "Actor" class.
		 *	@param bInOnlyBlueprintBases				Filter option to remove non-blueprint base classes.
		 *	@param bInShowUnloadedBlueprints			Filter option to not remove unloaded blueprints due to class filter options.
		 *
		 *	@return A fully built list.
		 */
		static void GetClassList(const FClassViewerInitializationOptions& InInitOptions, TArray< TSharedPtr< FClassViewerNode > >& InOutNodeList, const TArray<FString>& InFilterSearchTerms, bool bInOnlyPlaceables, bool bInOnlyActors, bool bInOnlyBlueprintBases, bool bInShowUnloadedBlueprints)
		{
			const TSharedPtr< FClassViewerNode > ObjectClassRoot = ClassHierarchy->GetObjectRootNode();

			// If the option to see the object root class is set, add it to the list, proceed normally from there so the actor's only filter continues to work.
			if(InInitOptions.bShowObjectRootClass)
			{
				TSharedPtr< FClassViewerNode > NewNode = MakeShareable( new FClassViewerNode( *ObjectClassRoot.Get() ) );
				NewNode->bPassesFilter = IsClassAllowed(InInitOptions, ObjectClassRoot->Class) && PassesFilter(*ObjectClassRoot->GetClassName().Get(), InFilterSearchTerms);
				
				if(NewNode->bPassesFilter)
				{
					InOutNodeList.Add(NewNode);
				}

				NewNode->PropertyHandle = InInitOptions.PropertyHandle;
			}

			TArray< TSharedPtr< FClassViewerNode > >& ChildList = ObjectClassRoot->GetChildrenList();
			for(int32 ObjectChildIndex = 0; ObjectChildIndex < ChildList.Num(); ObjectChildIndex++)
			{
				AddChildren_List(InInitOptions, InOutNodeList, ChildList[ObjectChildIndex], InFilterSearchTerms, bInOnlyActors, bInOnlyPlaceables, bInOnlyBlueprintBases, bInShowUnloadedBlueprints);			
			}
		}

		/** Retrieves the blueprint for a class index.
		 *	@param InClass							The class whose blueprint is desired.
		 *
		 *	@return									The blueprint associated with the class index.
		 */
		static UBlueprint* GetBlueprint( UClass* InClass )
		{
			if( InClass->ClassGeneratedBy && InClass->ClassGeneratedBy->IsA(UBlueprint::StaticClass()) )
			{
				return Cast<UBlueprint>(InClass->ClassGeneratedBy);
			}

			return NULL;
		}

		/** Retrieves a few items of information on the given UClass (retrieved via the InClassIndex). 
		 *	@param InClass							The class to gather info of.
		 *	@param bInOutIsBlueprintBase			true if the class is a blueprint.
		 *	@param bInOutHasBlueprint				true if the class has a blueprint.
		 *
		 *	@return									The blueprint associated with the class index.
		 */
		static void GetClassInfo( TWeakObjectPtr<UClass> InClass, bool& bInOutIsBlueprintBase, bool& bInOutHasBlueprint )
		{
			if (UClass* Class = InClass.Get())
			{
				bInOutIsBlueprintBase = CanCreateBlueprintOfClass_IgnoreDeprecation( Class );
				bInOutHasBlueprint = Class->ClassGeneratedBy != NULL;
			}
			else
			{
				bInOutIsBlueprintBase = false;
				bInOutHasBlueprint = false;
			}
		}

		/** Checks if a node is a blueprint base or not.
		 *	@param	InNode					The node to check if it is a blueprint base.
		 *
		 *	@return							true if the class is a blueprint.
		 */
		static bool CheckIfBlueprintBase( TSharedPtr< FClassViewerNode> InNode )
		{
			// If there is no class, it may be an unloaded blueprint.
			if(UClass* Class = InNode->Class.Get())
			{
				return CanCreateBlueprintOfClass_IgnoreDeprecation(Class);
			}
			else if(InNode->bIsBPNormalType)
			{
				bool bAllowDerivedBlueprints = false;
				GConfig->GetBool(TEXT("Kismet"), TEXT("AllowDerivedBlueprints"), /*out*/ bAllowDerivedBlueprints, GEngineIni);

				return bAllowDerivedBlueprints;
			}

			return false;
		}

		/** Recursively loads the entire chain of blueprints because children need their parents to be loaded.
		 *	@param	InRootNode				The current node being examined.
		 *	@param	InBlueprint				The blueprint to add.
		 */
		static void AddBlueprintChainToHierarchy(TSharedPtr< FClassViewerNode > InRootNode, UObject* InBlueprint)
		{
			TSharedPtr< FClassViewerNode > ParentNode = ClassHierarchy->FindParent(ClassHierarchy->GetObjectRootNode(), InRootNode->ParentClassname, NULL);

			if( ParentNode.IsValid() && !ParentNode->GeneratedClassPackage.IsEmpty() )
			{
				UPackage* Package = LoadPackage(NULL, *ParentNode->GeneratedClassPackage, LOAD_NoRedirects );
				Package->FullyLoad();
				UObject* ParentObject = FindObject<UObject>(Package, *ParentNode->AssetName);

				if(ParentObject->IsA(UBlueprint::StaticClass()))
				{
					AddBlueprintChainToHierarchy(ParentNode, ParentObject);
				}
			}
		}

		/**
		 * Creates a blueprint from a class.
		 *
		 * @param	InCreationClass			The class to create the blueprint from.
		 * @param	InParentContent			The content to parent the STextEntryPopup to. 
		 */
		static void CreateBlueprint(const FString& InBlueprintName, UClass* InCreationClass)
		{
			if(InCreationClass == NULL || !FKismetEditorUtilities::CanCreateBlueprintOfClass(InCreationClass))
			{
				FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "InvalidClassToMakeBlueprintFrom", "Invalid class to make a Blueprint of."));
				return;
			}

			// Get the full name of where we want to create the physics asset.
			FString PackageName = InBlueprintName;

			// Then find/create it.
			UPackage* Package = CreatePackage(NULL, *PackageName);
			check(Package);

			// Handle fully loading packages before creating new objects.
			TArray<UPackage*> TopLevelPackages;
			TopLevelPackages.Add( Package->GetOutermost() );
			if( !PackageTools::HandleFullyLoadingPackages( TopLevelPackages, NSLOCTEXT("UnrealEd", "CreateANewObject", "Create a new object") ) )
			{
				// Can't load package
				return;
			}

			FName BPName(*FPackageName::GetLongPackageAssetName(PackageName));

			if(PromptUserIfExistingObject(BPName.ToString(), PackageName,FString(), Package))
			{
				// Create and init a new Blueprint
				UBlueprint* NewBP = FKismetEditorUtilities::CreateBlueprint(InCreationClass, Package, BPName, BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), FName("ClassViewer"));
				if(NewBP)
				{
					FAssetEditorManager::Get().OpenEditorForAsset(NewBP);

					// Notify the asset registry
					FAssetRegistryModule::AssetCreated(NewBP);

					// Mark the package dirty...
					Package->MarkPackageDirty();
				}
			}

			// All viewers must refresh.
			RefreshAll();
		}

		/**
		 * Creates a SaveAssetDialog for specifying the path for the new blueprint
		 */
		static void OpenCreateBlueprintDialog(UClass* InCreationClass)
		{
			// Determine default path for the Save Asset dialog
			FString DefaultPath;
			const FString DefaultDirectory = FEditorDirectories::Get().GetLastDirectory(ELastDirectory::NEW_ASSET);
			FPackageName::TryConvertFilenameToLongPackageName(DefaultDirectory, DefaultPath);

			if (DefaultPath.IsEmpty())
			{
				DefaultPath = TEXT("/Game/Blueprints");
			}

			// Determine default filename for the Save Asset dialog
			check(InCreationClass != nullptr);
			const FString ClassName = InCreationClass->ClassGeneratedBy ? InCreationClass->ClassGeneratedBy->GetName() : InCreationClass->GetName();
			FString DefaultName = LOCTEXT("PrefixNew", "New").ToString() + ClassName;

			FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
			FString UniquePackageName;
			FString UniqueAssetName;
			AssetToolsModule.Get().CreateUniqueAssetName(DefaultPath / DefaultName, TEXT(""), UniquePackageName, UniqueAssetName);
			DefaultName = FPaths::GetCleanFilename(UniqueAssetName);

			// Initialize SaveAssetDialog config
			FSaveAssetDialogConfig SaveAssetDialogConfig;
			SaveAssetDialogConfig.DialogTitleOverride = LOCTEXT("CreateBlueprintDialogTitle", "Create Blueprint Class");
			SaveAssetDialogConfig.DefaultPath = DefaultPath;
			SaveAssetDialogConfig.DefaultAssetName = DefaultName;
			SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;

			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			FString SaveObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);
			if (!SaveObjectPath.IsEmpty())
			{
				const FString PackageName = FPackageName::ObjectPathToPackageName(SaveObjectPath);
				const FString PackageFilename = FPackageName::LongPackageNameToFilename(PackageName);
				const FString PackagePath = FPaths::GetPath(PackageFilename);

				CreateBlueprint(PackageName, InCreationClass);
				FEditorDirectories::Get().SetLastDirectory(ELastDirectory::NEW_ASSET, PackagePath);
			}
		}

		/** Returns the tooltip to display when attempting to derive a Blueprint */
		FText GetCreateBlueprintTooltip(UClass* InCreationClass)
		{
			if(InCreationClass->HasAnyClassFlags(CLASS_Deprecated))
			{
				return LOCTEXT("ClassViewerMenuCreateDeprecatedBlueprint_Tooltip", "Class is deprecated!");
			}
			else
			{
				return LOCTEXT("ClassViewerMenuCreateBlueprint_Tooltip", "Creates a Blueprint Class using this class as a base.");
			}
		}

		/** Returns TRUE if you can derive a Blueprint */
		bool CanOpenCreateBlueprintDialog(UClass* InCreationClass)
		{
			return !InCreationClass->HasAnyClassFlags(CLASS_Deprecated);
		}

		/**
		 * Creates a class wizard for creating a new C++ class
		 *
		 * @param	InParentContent			The content to parent the STextEntryPopup to. 
		 */
		static void OpenCreateCPlusPlusClassWizard(UClass* InCreationClass)
		{
			FGameProjectGenerationModule::Get().OpenAddCodeToProjectDialog(
				FAddToProjectConfig()
				.ParentClass(InCreationClass)
				.ParentWindow(FGlobalTabmanager::Get()->GetRootWindow())
			);
		}

		/**
		 * Creates a blueprint from a class.
		 *
		 * @param	InOutClassNode			Class node to pull what class to load and to update information in.
		 */
		static void LoadClass(TSharedPtr< FClassViewerNode > InOutClassNode)
		{
			GWarn->BeginSlowTask(LOCTEXT("LoadPackage", "Loading Package..."), true);

			UPackage* Package = LoadPackage(NULL, *InOutClassNode->GeneratedClassPackage, LOAD_NoRedirects );
			if(Package)
			{
				Package->FullyLoad();

				UObject* Object = FindObject<UObject>(Package, *InOutClassNode->AssetName);

				GWarn->EndSlowTask();

				// Check if this item is a blueprint.
				if( Object->IsA(UBlueprint::StaticClass()) )
				{
					InOutClassNode->Blueprint = Cast<UBlueprint>(Object);
					InOutClassNode->Class = Cast<UClass>(InOutClassNode->Blueprint->GeneratedClass);

					// Tell the original node to update so when a refresh happens it will still know about the newly loaded class.
					ClassViewer::Helpers::UpdateClassInNode(InOutClassNode->GeneratedClassPackage, InOutClassNode->Class.Get(), InOutClassNode->Blueprint.Get() );

					// Adds the entire hierarchy of Blueprints to the EditorClassHierarchy so they will continue to appear when a full rebuild of the tree happens.
					ClassViewer::Helpers::AddBlueprintChainToHierarchy(InOutClassNode, Object);
				}
				else if (UClass* Class = Cast<UClass>(Object))
				{
					InOutClassNode->Blueprint = Cast<UBlueprint>(Class->ClassGeneratedBy);
					InOutClassNode->Class = Class;

					// Tell the original node to update so when a refresh happens it will still know about the newly loaded class.
					ClassViewer::Helpers::UpdateClassInNode(InOutClassNode->GeneratedClassPackage, InOutClassNode->Class.Get(), InOutClassNode->Blueprint.Get() );

					// Adds the entire hierarchy of Blueprints to the EditorClassHierarchy so they will continue to appear when a full rebuild of the tree happens.
					ClassViewer::Helpers::AddBlueprintChainToHierarchy(InOutClassNode, Object);
				}
				else
				{
					InOutClassNode->Class = Object->GetClass();
				}
			}
			else
			{
				GWarn->EndSlowTask();

				// Check to see if the class can be found, if it can't, notify that the package failed to load.
				//if(!FindClass(InOutClassNode))
				{
					FMessageLog EditorErrors("EditorErrors");
					EditorErrors.Error(LOCTEXT("PackageLoadFail", "Package Load Failed"));
					EditorErrors.Info(FText::FromString(InOutClassNode->GeneratedClassPackage));
					EditorErrors.Notify(LOCTEXT("PackageLoadFail", "Package Load Failed"));
				}
			}
		}

		/**
		 * Opens a blueprint.
		 *
		 * @param	InBlueprint			The blueprint to open.
		 */
		static void OpenBlueprintTool(UBlueprint* InBlueprint)
		{
			if( InBlueprint != NULL )
			{
				FAssetEditorManager::Get().OpenEditorForAsset(InBlueprint);
			}
		}

		/**
		 * Opens a class's source file.
		 *
		 * @param	InClass		The class to open source for.
		 */
		static void OpenClassHeaderFileInIDE(UClass* InClass)
		{
			if( InClass != NULL )
			{
				FString ClassHeaderPath;
				if( FSourceCodeNavigation::FindClassHeaderPath( InClass, ClassHeaderPath ) && IFileManager::Get().FileSize( *ClassHeaderPath ) != INDEX_NONE )
				{
					FString AbsoluteHeaderPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*ClassHeaderPath);
					FSourceCodeNavigation::OpenSourceFile( AbsoluteHeaderPath );
				}
			}
		}

		/**
		 * Finds the blueprint or class in the content browser. Blueprint prioritized because if there is a blueprint we want to find that.
		 *
		 * @param	InBlueprint			The blueprint to find.
		 * @param	InClass				The class to find.
		 */
		static void FindInContentBrowser(UBlueprint* InBlueprint, UClass* InClass)
		{
			// If there is a blueprint, use the blueprint instead of the class. Otherwise it will not fully find the requested object.
			if(InBlueprint)
			{
				TArray<UObject*> Objects;
				Objects.Add(InBlueprint);
				GEditor->SyncBrowserToObjects(Objects);
			}
			else if (InClass)
			{
				TArray<UObject*> Objects;
				Objects.Add(InClass);
				GEditor->SyncBrowserToObjects(Objects);
			}
		}

		/** Updates the Class of a node. Uses the generated class package name to find the node.
		*	@param InGeneratedClassPackageName			The name of the generated class package to find the node for.
		*	@param InNewClass							The class to update the node with.
		*/
		static void UpdateClassInNode(const FString& InGeneratedClassPackageName, UClass* InNewClass, UBlueprint* InNewBluePrint )
		{
			ClassHierarchy->UpdateClassInNode(InGeneratedClassPackageName, InNewClass, InNewBluePrint );
		}

		static TSharedRef<SWidget> CreateMenu(UClass* Class, const bool bIsBlueprint, const bool bHasBlueprint)
		{
			// Empty list of commands.
			TSharedPtr< FUICommandList > Commands;

			const bool bShouldCloseWindowAfterMenuSelection = true;	// Set the menu to automatically close when the user commits to a choice
			FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, Commands);
			{
				if (bIsBlueprint)
				{
					TAttribute<FText>::FGetter DynamicTooltipGetter;
					DynamicTooltipGetter.BindStatic(&ClassViewer::Helpers::GetCreateBlueprintTooltip, Class);
					TAttribute<FText> DynamicTooltipAttribute = TAttribute<FText>::Create(DynamicTooltipGetter);

					MenuBuilder.AddMenuEntry(
						LOCTEXT("ClassViewerMenuCreateBlueprint", "Create Blueprint Class..."),
						DynamicTooltipAttribute,
						FSlateIcon(),
						FUIAction(
							FExecuteAction::CreateStatic(&ClassViewer::Helpers::OpenCreateBlueprintDialog, Class),
							FCanExecuteAction::CreateStatic(&ClassViewer::Helpers::CanOpenCreateBlueprintDialog, Class)
							)
						);
				}

				if (bHasBlueprint)
				{
					MenuBuilder.BeginSection("ClassViewerDropDownHasBlueprint");
					{
						FUIAction Action(FExecuteAction::CreateStatic(&ClassViewer::Helpers::OpenBlueprintTool, ClassViewer::Helpers::GetBlueprint(Class)));
						MenuBuilder.AddMenuEntry(LOCTEXT("ClassViewerMenuEditBlueprint", "Edit Blueprint Class..."), LOCTEXT("ClassViewerMenuEditBlueprint_Tooltip", "Open the Blueprint Class in the editor."), FSlateIcon(), Action);
					}
					MenuBuilder.EndSection();

					MenuBuilder.BeginSection("ClassViewerDropDownHasBlueprint2");
					{
						FUIAction Action(FExecuteAction::CreateStatic(&ClassViewer::Helpers::FindInContentBrowser, ClassViewer::Helpers::GetBlueprint(Class), Class));
						MenuBuilder.AddMenuEntry(LOCTEXT("ClassViewerMenuFindContent", "Find in Content Browser..."), LOCTEXT("ClassViewerMenuFindContent_Tooltip", "Find in Content Browser"), FSlateIcon(), Action);
					}
					MenuBuilder.EndSection();
				}
				else
				{
					MenuBuilder.BeginSection("ClassViewerIsCode");
					{
						FUIAction Action(FExecuteAction::CreateStatic(&ClassViewer::Helpers::OpenClassHeaderFileInIDE, Class));
						MenuBuilder.AddMenuEntry(LOCTEXT("ClassViewerMenuOpenCPlusPlusClass", "Open C++ Header..."), LOCTEXT("ClassViewerMenuOpenCPlusPlusClass_Tooltip", "Open the header file for this class in the IDE."), FSlateIcon(), Action);
					}
					{
						FUIAction Action(FExecuteAction::CreateStatic(&ClassViewer::Helpers::OpenCreateCPlusPlusClassWizard, Class));
						MenuBuilder.AddMenuEntry(LOCTEXT("ClassViewerMenuCreateCPlusPlusClass", "Create New C++ Class..."), LOCTEXT("ClassViewerMenuCreateCPlusPlusClass_Tooltip", "Creates a new C++ class using this class as a base."), FSlateIcon(), Action);
					}
					MenuBuilder.EndSection();
				}
			}

			return MenuBuilder.MakeWidget();
		}


	} // namespace Helpers
} // namespace ClassViewer

/** Delegate used with the Class Viewer in 'class picking' mode.  You'll bind a delegate when the
	class viewer widget is created, which will be fired off when the selected class is double clicked */
DECLARE_DELEGATE_OneParam( FOnClassItemDoubleClickDelegate, TSharedPtr<FClassViewerNode> );

/** The item used for visualizing the class in the tree. */
class SClassItem : public STableRow< TSharedPtr<FString> >
{
public:
	
	SLATE_BEGIN_ARGS( SClassItem )
		: _ClassName()
		, _bIsPlaceable(false)
		, _bIsInClassViewer( true )
		, _bDynamicClassLoading( true )
		, _HighlightText(NULL)
		, _TextColor(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f))
		{}

		/** The classname this item contains. */
		SLATE_ARGUMENT( TSharedPtr<FString>, ClassName )
		/** true if this item is a placeable object. */
		SLATE_ARGUMENT( bool, bIsPlaceable )
		/** true if this item is in a Class Viewer (as opposed to a Class Picker) */
		SLATE_ARGUMENT( bool, bIsInClassViewer )
		/** true if this item should allow dynamic class loading */
		SLATE_ARGUMENT( bool, bDynamicClassLoading )
		/** The text this item should highlight, if any. */
		SLATE_ARGUMENT( const FText*, HighlightText )
		/** The color text this item will use. */
		SLATE_ARGUMENT( FSlateColor, TextColor )
		/** The node this item is associated with. */
		SLATE_ARGUMENT( TSharedPtr<FClassViewerNode>, AssociatedNode)
		/** the delegate for handling double clicks outside of the SClassItem */
		SLATE_ARGUMENT( FOnClassItemDoubleClickDelegate, OnClassItemDoubleClicked )
		/** On Class Picked callback. */
		SLATE_EVENT( FOnDragDetected, OnDragDetected )

	SLATE_END_ARGS()

	/**
	 * Construct the widget
	 *
	 * @param InArgs   A declaration from which to construct the widget
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView )
	{
		ClassName = InArgs._ClassName;
		bIsClassPlaceable = InArgs._bIsPlaceable;
		bIsInClassViewer = InArgs._bIsInClassViewer;
		bDynamicClassLoading = InArgs._bDynamicClassLoading;
		AssociatedNode = InArgs._AssociatedNode;
		OnDoubleClicked = InArgs._OnClassItemDoubleClicked;

		bool bIsBlueprint(false);
		bool bHasBlueprint(false);

		//SetEnabled( AssociatedNode->Restrictions.Num() == 0 );

		ClassViewer::Helpers::GetClassInfo(AssociatedNode->Class, bIsBlueprint, bHasBlueprint);
		
		struct Local
		{

			static TSharedPtr<SToolTip> GetToolTip(TSharedPtr<FClassViewerNode> AssociatedNode)
			{
				TSharedPtr<SToolTip> ToolTip;
				if( AssociatedNode->PropertyHandle.IsValid() && AssociatedNode->IsRestricted() )
				{
					FText RestrictionToolTip;
					AssociatedNode->PropertyHandle->GenerateRestrictionToolTip(*AssociatedNode->GetClassName(),RestrictionToolTip);

					ToolTip = IDocumentation::Get()->CreateToolTip(RestrictionToolTip, nullptr, "", "");
				}
				else if (UClass* Class = AssociatedNode->Class.Get())
				{
					UPackage*  Package  = Class->GetOutermost();
					UMetaData* MetaData = Package->GetMetaData();

					ToolTip = FEditorClassUtils::GetTooltip(Class);
				}

				return ToolTip;
			}
		};

		bool bIsRestricted = AssociatedNode->IsRestricted();

		const FSlateBrush* ClassIcon = FClassIconFinder::FindIconForClass(AssociatedNode->Class.Get());
		
		this->ChildSlot
		[
			SNew(SHorizontalBox)
				
			+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew( SExpanderArrow, SharedThis(this) )
				]

			+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding( 0.0f, 2.0f, 6.0f, 2.0f )
				[
					SNew( SImage )
					.Image(	ClassIcon )
					.Visibility( ClassIcon != FEditorStyle::GetDefaultBrush()? EVisibility::Visible : EVisibility::Collapsed )
				]

			+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding( 0.0f, 3.0f, 6.0f, 3.0f )
				.VAlign(VAlign_Center)
				[
					SNew( STextBlock )
						.Text( FText::FromString(*ClassName.Get()) )
						.HighlightText(*InArgs._HighlightText)
						.ColorAndOpacity( this, &SClassItem::GetTextColor)
						.ToolTip(Local::GetToolTip(AssociatedNode))
						.IsEnabled(!bIsRestricted)
				]

			+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.Padding( 0.0f, 0.0f, 6.0f, 0.0f )
				[
					SNew( SComboButton )
						.ContentPadding(FMargin(2.0f))
						.Visibility(this, &SClassItem::ShowOptions)
						.OnGetMenuContent(this, &SClassItem::GenerateDropDown)
				]
		];
		
		TextColor = InArgs._TextColor;

		UE_LOG(LogEditorClassViewer, VeryVerbose, TEXT("CLASS [%s]"), **ClassName);


		STableRow< TSharedPtr<FString> >::ConstructInternal(
			STableRow::FArguments()
				.ShowSelection(true)
				.OnDragDetected(InArgs._OnDragDetected),
			InOwnerTableView
		);	
	}

private:
	FReply OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent )
	{
		// If in a Class Viewer and it has not been loaded, load the class when double-left clicking.
		if ( bIsInClassViewer )
		{
			if( bDynamicClassLoading && AssociatedNode->Class == NULL && AssociatedNode->UnloadedBlueprintData.IsValid() && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
			{
				ClassViewer::Helpers::LoadClass(AssociatedNode);
			}
			// If there is a blueprint, open it. Otherwise try to open the class header.
			if(AssociatedNode->Blueprint.IsValid())
			{
				ClassViewer::Helpers::OpenBlueprintTool(AssociatedNode->Blueprint.Get());
			}
			else
			{
				ClassViewer::Helpers::OpenClassHeaderFileInIDE(AssociatedNode->Class.Get());
			}
		}
		else
		{
			OnDoubleClicked.ExecuteIfBound( AssociatedNode );
		}
		return FReply::Handled();
	}

	EVisibility ShowOptions() const
	{
		// If it's in viewer mode, show the options combo button.
		if(bIsInClassViewer)
		{
			bool bIsBlueprint(false);
			bool bHasBlueprint(false);

			ClassViewer::Helpers::GetClassInfo(AssociatedNode->Class, bIsBlueprint, bHasBlueprint);

			return (bIsBlueprint || AssociatedNode->Blueprint.IsValid())? EVisibility::Visible : EVisibility::Collapsed;
		}

		return EVisibility::Collapsed;
	}

	/**
	 * Generates the drop down menu for the item.
	 *
	 * @return		The drop down menu widget.
	 */
	TSharedRef<SWidget> GenerateDropDown()
	{
		if (UClass* Class = AssociatedNode->Class.Get())
		{
			bool bIsBlueprint(false);
			bool bHasBlueprint(false);

			ClassViewer::Helpers::GetClassInfo(Class, bIsBlueprint, bHasBlueprint);
			bHasBlueprint = AssociatedNode->Blueprint.IsValid();
			return ClassViewer::Helpers::CreateMenu(Class, bIsBlueprint, bHasBlueprint);
		}

		return SNullWidget::NullWidget;
	}

	/** Returns the text color for the item based on if it is selected or not. */
	FSlateColor GetTextColor() const
	{
		const TSharedPtr< ITypedTableView< TSharedPtr<FString> > > OwnerWidget = OwnerTablePtr.Pin();
		const TSharedPtr<FString>* MyItem = OwnerWidget->Private_ItemFromWidget( this );
		const bool bIsSelected = OwnerWidget->Private_IsItemSelected( *MyItem );

		if(bIsSelected)
		{
			return FSlateColor::UseForeground();
		}

		return TextColor;
	}

private:

	/** The class name for which this item is associated with. */
	TSharedPtr<FString> ClassName;

	/** true if this class is placeable. */
	bool bIsClassPlaceable;

	/** true if in a Class Viewer (as opposed to a Class Picker). */
	bool bIsInClassViewer;

	/** true if dynamic class loading is permitted. */
	bool bDynamicClassLoading;

	/** The text color for this item. */
	FSlateColor TextColor;

	/** The Class Viewer Node this item is associated with. */
	TSharedPtr< FClassViewerNode > AssociatedNode;

	/** the on Double Clicked delegate */
	FOnClassItemDoubleClickDelegate OnDoubleClicked;
};

static void OnModulesChanged(FName ModuleThatChanged, EModuleChangeReason ReasonForChange)
{
	ClassViewer::Helpers::RequestPopulateClassHierarchy();
}

FClassHierarchy::FClassHierarchy()
{
	// Register with the Asset Registry to be informed when it is done loading up files.
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	OnFilesLoadedRequestPopulateClassHierarchyDelegateHandle = AssetRegistryModule.Get().OnFilesLoaded().AddStatic( ClassViewer::Helpers::RequestPopulateClassHierarchy );
	AssetRegistryModule.Get().OnAssetAdded().AddRaw( this, &FClassHierarchy::AddAsset);
	AssetRegistryModule.Get().OnAssetRemoved().AddRaw( this, &FClassHierarchy::RemoveAsset );

	// Register to have Populate called when doing a Hot Reload.
	IHotReloadInterface& HotReloadSupport = FModuleManager::LoadModuleChecked<IHotReloadInterface>("HotReload");
	HotReloadSupport.OnHotReload().AddRaw( this, &FClassHierarchy::OnHotReload );

	// Register to have Populate called when a Blueprint is compiled.
	OnBlueprintCompiledRequestPopulateClassHierarchyDelegateHandle            = GEditor->OnBlueprintCompiled().AddStatic(ClassViewer::Helpers::RequestPopulateClassHierarchy);
	OnClassPackageLoadedOrUnloadedRequestPopulateClassHierarchyDelegateHandle = GEditor->OnClassPackageLoadedOrUnloaded().AddStatic(ClassViewer::Helpers::RequestPopulateClassHierarchy);

	FModuleManager::Get().OnModulesChanged().AddStatic(&OnModulesChanged);
}

FClassHierarchy::~FClassHierarchy()
{
	// Unregister with the Asset Registry to be informed when it is done loading up files.
	if( FModuleManager::Get().IsModuleLoaded( TEXT("AssetRegistry") ) )
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		AssetRegistryModule.Get().OnFilesLoaded().Remove(OnFilesLoadedRequestPopulateClassHierarchyDelegateHandle);
		AssetRegistryModule.Get().OnAssetAdded().RemoveAll( this );
		AssetRegistryModule.Get().OnAssetRemoved().RemoveAll( this );

		// Unregister to have Populate called when doing a Hot Reload.
		IHotReloadInterface& HotReloadSupport = FModuleManager::LoadModuleChecked<IHotReloadInterface>("HotReload");
		HotReloadSupport.OnHotReload().RemoveAll( this );

		// Unregister to have Populate called when a Blueprint is compiled.
		GEditor->OnBlueprintCompiled().Remove(OnBlueprintCompiledRequestPopulateClassHierarchyDelegateHandle);
		GEditor->OnClassPackageLoadedOrUnloaded().Remove(OnClassPackageLoadedOrUnloadedRequestPopulateClassHierarchyDelegateHandle);
	}

	FModuleManager::Get().OnModulesChanged().RemoveAll(this);
}

static TSharedPtr< FClassViewerNode > CreateNodeForClass(UClass* Class, const TMultiMap<FName, FAssetData>& BlueprintPackageToAssetDataMap)
{
	// Create the new node so it can be passed to AddChildren, fill it in with if it is placeable, abstract, and/or a brush.
	TSharedPtr< FClassViewerNode > NewNode = MakeShareable(new FClassViewerNode(Class->GetName(), Class->GetDisplayNameText().ToString()));
	NewNode->Blueprint = ClassViewer::Helpers::GetBlueprint(Class);
	NewNode->Class = Class;

	// Retrieve all blueprint classes
	TArray<FAssetData> BlueprintList;
	BlueprintPackageToAssetDataMap.MultiFind(NewNode->Class->GetOuterUPackage()->GetFName(), BlueprintList);

	// Check if the Asset Registry found anything, it should, but check.
	if ( BlueprintList.Num() )
	{
		// Grab the generated class name and check it before assigning. Objects that haven't been saved since this has started to be exported do not have the information.
		if (auto GeneratedClassnamePtr = BlueprintList[0].TagsAndValues.Find(FName("GeneratedClass")))
		{
			NewNode->GeneratedClassname = FName(**GeneratedClassnamePtr);
		}
	}

	return NewNode;
}

void FClassHierarchy::OnHotReload( bool bWasTriggeredAutomatically )
{
	ClassViewer::Helpers::RequestPopulateClassHierarchy();
}

void FClassHierarchy::AddChildren_NoFilter( TSharedPtr< FClassViewerNode >& InOutRootNode, const TMultiMap<FName, FAssetData>& BlueprintPackageToAssetDataMap )
{
	UClass* RootClass = UObject::StaticClass();

	ObjectClassRoot = MakeShareable(new FClassViewerNode(RootClass->GetName(), RootClass->GetDisplayNameText().ToString()));
	ObjectClassRoot->Class = RootClass;

	TMap< UClass*, TSharedPtr< FClassViewerNode > > Nodes;

	Nodes.Add(RootClass, ObjectClassRoot);

	TSet<UClass*> Visited;
	Visited.Add(RootClass);

	// Go through all of the classes children and see if they should be added to the list.
	for ( TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt )
	{
		UClass* CurrentClass = *ClassIt;

		// Ignore deprecated and temporary trash classes.
		if (CurrentClass->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists) || 
			FKismetEditorUtilities::IsClassABlueprintSkeleton(CurrentClass))
		{
			continue;
		}
		
		TSharedPtr<FClassViewerNode>& Entry = Nodes.FindOrAdd(CurrentClass);
		if ( Visited.Contains(CurrentClass) )
		{
			continue;
		}
		else
		{
			while ( CurrentClass->GetSuperClass() != NULL )
			{
				TSharedPtr<FClassViewerNode>& ParentEntry = Nodes.FindOrAdd(CurrentClass->GetSuperClass());
				if ( !ParentEntry.IsValid() )
				{
					ParentEntry = CreateNodeForClass(CurrentClass->GetSuperClass(), BlueprintPackageToAssetDataMap);
				}

				TSharedPtr<FClassViewerNode>& MyEntry = Nodes.FindOrAdd(CurrentClass);
				if ( !MyEntry.IsValid() )
				{
					MyEntry = CreateNodeForClass(CurrentClass, BlueprintPackageToAssetDataMap);
				}

				if ( !Visited.Contains(CurrentClass) )
				{
					ParentEntry->AddChild(MyEntry);
					Visited.Add(CurrentClass);
				}

				CurrentClass = CurrentClass->GetSuperClass();
			}
		}
	}
}

TSharedPtr< FClassViewerNode > FClassHierarchy::FindParent(const TSharedPtr< FClassViewerNode >& InRootNode, FName InParentClassname, const UClass* InParentClass)
{
	// Check if the current node is the parent classname that is being searched for.
	if(InRootNode->GeneratedClassname == InParentClassname)
	{
		// Return the node if it is the correct parent, this ends the recursion.
		return InRootNode;
	}
	else
	{
		// If a class does not have a generated classname, we look up the parent class and compare.
		const UClass* ParentClass = InParentClass;

		if(const UClass* RootClass = InRootNode->Class.Get())
		{
			if(ParentClass == RootClass)
			{
				return InRootNode;
			}
		}

	}

	TSharedPtr< FClassViewerNode > ReturnNode;

	// Search the children recursively, one of them might have the parent.
	for(int32 ChildClassIndex = 0; !ReturnNode.IsValid() && ChildClassIndex < InRootNode->GetChildrenList().Num(); ChildClassIndex++)
	{
		// Check the child, then check the return to see if it is valid. If it is valid, end the recursion.
		ReturnNode = FindParent(InRootNode->GetChildrenList()[ChildClassIndex], InParentClassname, InParentClass);

		if(ReturnNode.IsValid())
		{
			break;
		}
	}

	return ReturnNode;
}

TSharedPtr< FClassViewerNode > FClassHierarchy::FindNodeByGeneratedClassPackageName(const TSharedPtr< FClassViewerNode >& InRootNode, const FString& InGeneratedClassPackageName)
{
	if(InRootNode->GeneratedClassPackage == InGeneratedClassPackageName)
	{
		return InRootNode;
	}

	TSharedPtr< FClassViewerNode > ReturnNode;

	// Search the children recursively, one of them might have the parent.
	for(int32 ChildClassIndex = 0; !ReturnNode.IsValid() && ChildClassIndex < InRootNode->GetChildrenList().Num(); ChildClassIndex++)
	{
		// Check the child, then check the return to see if it is valid. If it is valid, end the recursion.
		ReturnNode = FindNodeByGeneratedClassPackageName(InRootNode->GetChildrenList()[ChildClassIndex], InGeneratedClassPackageName);

		if(ReturnNode.IsValid())
		{
			break;
		}
	}

	return ReturnNode;
}

void FClassHierarchy::UpdateClassInNode(const FString& InGeneratedClassPackageName, UClass* InNewClass,  UBlueprint* InNewBluePrint )
{
	TSharedPtr< FClassViewerNode > Node = FindNodeByGeneratedClassPackageName(ObjectClassRoot, InGeneratedClassPackageName);

	if( Node.IsValid() )
	{
		Node->Class = InNewClass;
		Node->Blueprint = InNewBluePrint;
	}
}

bool FClassHierarchy::FindAndRemoveNodeByPackageName(const TSharedPtr< FClassViewerNode >& InRootNode, const FString& InAssetPackage)
{
	bool bReturnValue = false;

	// Search the children recursively, one of them might have the parent.
	for(int32 ChildClassIndex = 0; ChildClassIndex < InRootNode->GetChildrenList().Num(); ChildClassIndex++)
	{
		if(InRootNode->GetChildrenList()[ChildClassIndex]->GeneratedClassPackage == InAssetPackage)
		{
			InRootNode->GetChildrenList().RemoveAt(ChildClassIndex);
			return true;
		}
		// Check the child, then check the return to see if it is valid. If it is valid, end the recursion.
		bReturnValue = FindAndRemoveNodeByPackageName(InRootNode->GetChildrenList()[ChildClassIndex], InAssetPackage);

		if(bReturnValue)
		{
			break;
		}
	}
	return bReturnValue;
}

void FClassHierarchy::RemoveAsset(const FAssetData& InRemovedAssetData)
{
	if(FindAndRemoveNodeByPackageName(ObjectClassRoot, InRemovedAssetData.PackageName.ToString()))
	{
		// All viewers must refresh.
		ClassViewer::Helpers::RefreshAll();
	}
}

void FClassHierarchy::AddAsset(const FAssetData& InAddedAssetData)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	if ( !AssetRegistryModule.Get().IsLoadingAssets() )
	{
		TArray<FName> AncestorClassNames;
		AssetRegistryModule.Get().GetAncestorClassNames(InAddedAssetData.AssetClass, AncestorClassNames);

		if( AncestorClassNames.Contains(UBlueprintCore::StaticClass()->GetFName()) )
		{
			// Make sure that the node does not already exist. There is a bit of double adding going on at times and this prevents it.
			if(!FindNodeByGeneratedClassPackageName(ObjectClassRoot, InAddedAssetData.PackageName.ToString()).IsValid())
			{
				TSharedPtr< FClassViewerNode > NewNode;
				LoadUnloadedTagData(NewNode, InAddedAssetData);

				// Find the blueprint if it's loaded.
				FindClass(NewNode);

				// Resolve the parent's class name locally and use it to find the parent's class.
				FString ParentClassName = NewNode->ParentClassname.ToString();
				UObject* Outer(NULL);
				ResolveName(Outer, ParentClassName, false, false);

				UClass* ParentClass = FindObject<UClass>(ANY_PACKAGE, *ParentClassName);
				TSharedPtr< FClassViewerNode > ParentNode = FindParent(ObjectClassRoot, NewNode->ParentClassname, ParentClass); 
				if(ParentNode.IsValid())
				{
					ParentNode->AddChild(NewNode);
				
					// Make sure the children are properly sorted.
					SortChildren(ObjectClassRoot);

					// All Viewers must repopulate.
					ClassViewer::Helpers::RefreshAll();
				}
			}
		}
	}
}

void FClassHierarchy::SortChildren( TSharedPtr< FClassViewerNode >& InRootNode )
{
	TArray< TSharedPtr< FClassViewerNode > >& ChildList = InRootNode->GetChildrenList();
	for(int32 ChildIndex = 0; ChildIndex < ChildList.Num(); ChildIndex++)
	{
		// Setup the parent weak pointer, useful for going up the tree for unloaded blueprints.
		ChildList[ChildIndex]->ParentNode = InRootNode;

		// Check the child, then check the return to see if it is valid. If it is valid, end the recursion.
		SortChildren(ChildList[ChildIndex]);
	}

	// Sort the children.
	ChildList.Sort(CompareFClassViewerNodes);
}

void FClassHierarchy::FindClass(TSharedPtr< FClassViewerNode > InOutClassNode)
{
	UPackage* Package = FindPackage(NULL, *InOutClassNode->GeneratedClassPackage );
	if (Package)
	{
		UObject* Object = FindObject<UObject>(Package, *InOutClassNode->AssetName);

		if (Object)
		{
			// Check if this item is a blueprint.
			if (Object->IsA(UBlueprint::StaticClass()))
			{
				InOutClassNode->Blueprint = Cast<UBlueprint>(Object);
				InOutClassNode->Class = Cast<UClass>(InOutClassNode->Blueprint->GeneratedClass);
			}
			else if (UClass* Class = Cast<UClass>(Object))
			{
				InOutClassNode->Blueprint = Cast<UBlueprint>(Class->ClassGeneratedBy);
				InOutClassNode->Class = Class;
			}
			else
			{
				InOutClassNode->Class = Object->GetClass();
			}
		}
	}
}

void FClassHierarchy::LoadUnloadedTagData(TSharedPtr<FClassViewerNode>& InOutClassViewerNode, const FAssetData& InAssetData)
{
	const FString GeneratedClassPackage = InAssetData.PackageName.ToString();

	const FString* GeneratedClassname = InAssetData.TagsAndValues.Find("GeneratedClass");
	const FString* ParentClassname = InAssetData.TagsAndValues.Find("ParentClass");
	const FString* BlueprintType = InAssetData.TagsAndValues.Find("BlueprintType");
	const FString AssetName = InAssetData.AssetName.ToString();

	// Create the viewer node.
	InOutClassViewerNode = MakeShareable(new FClassViewerNode(AssetName, AssetName));
			
	// It is an unloaded blueprint, so we need to create the structure that will hold the data.
	TSharedPtr<FUnloadedBlueprintData> UnloadedBlueprintData = MakeShareable( new FUnloadedBlueprintData(InOutClassViewerNode) );
	InOutClassViewerNode->UnloadedBlueprintData = UnloadedBlueprintData;

	// Get the class flags.
	const FString* ClassFlags = InAssetData.TagsAndValues.Find("ClassFlags");
	if(ClassFlags)
	{
		InOutClassViewerNode->UnloadedBlueprintData->SetClassFlags(FCString::Atoi(**ClassFlags));
	}

	const FString* ImplementedInterfaces = InAssetData.TagsAndValues.Find("ImplementedInterfaces");
	if(ImplementedInterfaces)
	{
		FString FullInterface;
		FString RemainingString;
		FString InterfaceName;
		FString CurrentString = *ImplementedInterfaces;
		while(CurrentString.Split(TEXT(","), &FullInterface, &RemainingString))
		{
			if(FullInterface.Split(TEXT("."), &CurrentString, &InterfaceName, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
			{
				if(!CurrentString.StartsWith(TEXT("Graphs=(")))
				{
					UnloadedBlueprintData->AddImplementedInterfaces(InterfaceName);
				}
			}
			CurrentString = RemainingString;
		}
	}

	InOutClassViewerNode->GeneratedClassPackage = GeneratedClassPackage;

	InOutClassViewerNode->ParentClassname = NAME_None;
	if(ParentClassname)
	{
		InOutClassViewerNode->ParentClassname = FName(**ParentClassname);
	}

	if(GeneratedClassname)
	{
		InOutClassViewerNode->GeneratedClassname = FName(**GeneratedClassname);
	}

	if(BlueprintType && *BlueprintType == TEXT("BPType_Normal"))
	{
		InOutClassViewerNode->bIsBPNormalType = true;
	}

	InOutClassViewerNode->AssetName = AssetName;
}

void FClassHierarchy::PopulateClassHierarchy()
{
	TArray< TSharedPtr< FClassViewerNode > > RootLevelClasses;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	// Retrieve all blueprint classes 
	TArray<FAssetData> BlueprintList;

	FARFilter Filter;
	Filter.ClassNames.Add(UBlueprint::StaticClass()->GetFName());
	Filter.ClassNames.Add(UAnimBlueprint::StaticClass()->GetFName());
	Filter.ClassNames.Add(UBlueprintGeneratedClass::StaticClass()->GetFName());

	// Include any Blueprint based objects as well, this includes things like Blutilities, UMG, and GameplayAbility objects
	Filter.bRecursiveClasses = true;
	AssetRegistryModule.Get().GetAssets(Filter, BlueprintList);

	TMultiMap<FName,FAssetData> BlueprintPackageToAssetDataMap;
	for ( int32 AssetIdx = 0; AssetIdx < BlueprintList.Num(); ++AssetIdx )
	{
		TSharedPtr< FClassViewerNode > NewNode;
		LoadUnloadedTagData(NewNode, BlueprintList[AssetIdx]);
		RootLevelClasses.Add(NewNode);

		// Find the blueprint if it's loaded.
		FindClass(NewNode);


		BlueprintPackageToAssetDataMap.Add(BlueprintList[AssetIdx].PackageName, BlueprintList[AssetIdx]);
	}

	AddChildren_NoFilter(ObjectClassRoot, BlueprintPackageToAssetDataMap);

	RootLevelClasses.Add(ObjectClassRoot);

	// Second pass to link them to parents.
	for (int32 CurrentNodeIdx = 0; CurrentNodeIdx < RootLevelClasses.Num(); ++CurrentNodeIdx)
	{
		if(RootLevelClasses[CurrentNodeIdx]->ParentClassname != NAME_None)
		{
			// Resolve the parent's class name locally and use it to find the parent's class.
			FString ParentClassName = RootLevelClasses[CurrentNodeIdx]->ParentClassname.ToString();
			UObject* Outer(NULL);
			ResolveName(Outer, ParentClassName, false, false);
			const UClass* ParentClass = FindObject<UClass>(ANY_PACKAGE, *ParentClassName);

			for (int32 SearchNodeIdx = 0; SearchNodeIdx < RootLevelClasses.Num(); ++SearchNodeIdx)
			{
				TSharedPtr< FClassViewerNode > ParentNode = FindParent(RootLevelClasses[SearchNodeIdx], RootLevelClasses[CurrentNodeIdx]->ParentClassname, ParentClass); 
				if(ParentNode.IsValid())
				{
					// AddUniqueChild makes sure that when a node was generated one by EditorClassHierarchy and one from LoadUnloadedTagData - the proper one is selected
					ParentNode->AddUniqueChild(RootLevelClasses[CurrentNodeIdx]);	
					RootLevelClasses.RemoveAtSwap(CurrentNodeIdx);
					--CurrentNodeIdx;
					break;
				}
			}

		}
	}

	// Recursively sort the children.
	SortChildren(ObjectClassRoot);
	
	// All viewers must refresh.
	ClassViewer::Helpers::RefreshAll();
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SClassViewer::Construct(const FArguments& InArgs, const FClassViewerInitializationOptions& InInitOptions )
{
	bNeedsRefresh = true;

	InitOptions = InInitOptions;

	OnClassPicked = InArgs._OnClassPickedDelegate;

	bSaveExpansionStates = true;
	bPendingSetExpansionStates = false;

	bEnableClassDynamicLoading = InInitOptions.bEnableClassDynamicLoading;

	EVisibility HeaderVisibility = (this->InitOptions.Mode == EClassViewerMode::ClassBrowsing)? EVisibility::Visible : EVisibility::Collapsed;

	// Set these values to the user specified settings.
	bIsActorsOnly = InitOptions.bIsActorsOnly | InitOptions.bIsPlaceableOnly;
	bIsPlaceableOnly = InitOptions.bIsPlaceableOnly;
	bIsBlueprintBaseOnly = InitOptions.bIsBlueprintBaseOnly;
	bShowUnloadedBlueprints = InitOptions.bShowUnloadedBlueprints;
	bool bHasTitle = InitOptions.ViewerTitleString.IsEmpty() == false;

	// If set to default, decide what display mode to use.
	if( InitOptions.DisplayMode == EClassViewerDisplayMode::DefaultView )
	{
		// By default the Browser uses the tree view, the Picker the list. The option is available to users to force to another display mode when creating the Class Browser/Picker.
		if( InitOptions.Mode == EClassViewerMode::ClassBrowsing )
		{
			InitOptions.DisplayMode = EClassViewerDisplayMode::TreeView;
		}
		else
		{
			InitOptions.DisplayMode = EClassViewerDisplayMode::ListView;
		}
	}

	// Build the top menu.
	TSharedRef< FUICommandList > CommandList(new FUICommandList());
	FMenuBarBuilder MenuBarBuilder( CommandList );
	{
		if( InitOptions.Mode == EClassViewerMode::ClassBrowsing )
		{
			MenuBarBuilder.AddPullDownMenu( LOCTEXT("Filters", "Filters"), LOCTEXT("Filters_Tooltip", "Filter options for the Class Viewer."), FNewMenuDelegate::CreateRaw( this, &SClassViewer::FillFilterEntries ) );
		}
		
		// Only want the options this menu generates if in tree view.
		if( InitOptions.Mode == EClassViewerMode::ClassBrowsing && InitOptions.DisplayMode == EClassViewerDisplayMode::TreeView )
		{
			MenuBarBuilder.AddPullDownMenu( LOCTEXT("View", "View"), LOCTEXT("Tree_Tooltip", "Tree options for the Class Viewer."), FNewMenuDelegate::CreateRaw( this, &SClassViewer::FillTreeEntries ) );
		}
	}

	// Create the asset discovery indicator
	FEditorWidgetsModule& EditorWidgetsModule = FModuleManager::LoadModuleChecked<FEditorWidgetsModule>("EditorWidgets");
	TSharedRef<SWidget> AssetDiscoveryIndicator = EditorWidgetsModule.CreateAssetDiscoveryIndicator(EAssetDiscoveryIndicatorScaleMode::Scale_Vertical);
	FOnContextMenuOpening OnContextMenuOpening;
	if ( InitOptions.Mode == EClassViewerMode::ClassBrowsing )
	{
		OnContextMenuOpening = FOnContextMenuOpening::CreateSP(this, &SClassViewer::BuildMenuWidget);
	}

	// Holds the bulk of the class viewer's sub-widgets, to be added to the widget after construction
	TSharedPtr< SWidget > ClassViewerContent;

	SAssignNew(ClassViewerContent, SVerticalBox)
	+SVerticalBox::Slot()
	.AutoHeight()
	[
		MenuBarBuilder.MakeWidget()
	]

	+SVerticalBox::Slot()
	.AutoHeight()
	.Padding( 1.0f, 0.0f, 1.0f, 0.0f )
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Visibility(bHasTitle ? EVisibility::Visible : EVisibility::Collapsed)
			.ColorAndOpacity(FEditorStyle::GetColor("MultiboxHookColor"))
			.Text(InitOptions.ViewerTitleString)
		]
	]
	+SVerticalBox::Slot()
	.AutoHeight()
	[
		SAssignNew(SearchBox, SSearchBox)
			.OnTextChanged( this, &SClassViewer::OnFilterTextChanged )
			.OnTextCommitted( this, &SClassViewer::OnFilterTextCommitted )
	]

	+SVerticalBox::Slot()
	.AutoHeight()
	[
		SNew(SSeparator)
			.Visibility(HeaderVisibility)
	]

	+SVerticalBox::Slot()
	.FillHeight(1.0f)
	[
		SNew(SOverlay)

		+SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SVerticalBox)

			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SAssignNew(ClassTree, STreeView<TSharedPtr< FClassViewerNode > >)
				.Visibility(InitOptions.DisplayMode == EClassViewerDisplayMode::TreeView ? EVisibility::Visible : EVisibility::Collapsed)
	
				.SelectionMode(ESelectionMode::Single)

				.TreeItemsSource( &RootTreeItems )
				// Called to child items for any given parent item
				.OnGetChildren( this, &SClassViewer::OnGetChildrenForClassViewerTree )

				// Called to handle recursively expanding/collapsing items
				.OnSetExpansionRecursive(this, &SClassViewer::SetAllExpansionStates_Helper )

				// Generates the actual widget for a tree item
				.OnGenerateRow( this, &SClassViewer::OnGenerateRowForClassViewer ) 

				// Generates the right click menu.
				.OnContextMenuOpening( OnContextMenuOpening )

				// Find out when the user selects something in the tree
				.OnSelectionChanged( this, &SClassViewer::OnClassViewerSelectionChanged )

				// Called when the expansion state of an item changes
				.OnExpansionChanged( this, &SClassViewer::OnClassViewerExpansionChanged )

				// Allow for some spacing between items with a larger item height.
				.ItemHeight(20.0f)

				.HeaderRow
				(
					SNew(SHeaderRow)
					.Visibility(EVisibility::Collapsed)
					+ SHeaderRow::Column(TEXT("Class"))
					.DefaultLabel(NSLOCTEXT("ClassViewer", "Class", "Class"))
				)
			]

			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SAssignNew(ClassList, SListView<TSharedPtr< FClassViewerNode > >)
					.Visibility(InitOptions.DisplayMode == EClassViewerDisplayMode::ListView ? EVisibility::Visible : EVisibility::Collapsed)

					.SelectionMode(ESelectionMode::Single)

					.ListItemsSource( &RootTreeItems )

					// Generates the actual widget for a tree item
					.OnGenerateRow( this, &SClassViewer::OnGenerateRowForClassViewer ) 

					// Generates the right click menu.
					.OnContextMenuOpening( OnContextMenuOpening )

					// Find out when the user selects something in the tree
					.OnSelectionChanged( this, &SClassViewer::OnClassViewerSelectionChanged )

					// Allow for some spacing between items with a larger item height.
					.ItemHeight(20.0f)

					.HeaderRow
					(
						SNew(SHeaderRow)
						.Visibility(EVisibility::Collapsed)
						+ SHeaderRow::Column(TEXT("Class"))
						.DefaultLabel(NSLOCTEXT("ClassViewer", "Class", "Class"))
					)

			]
		]

		+SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(24, 0, 24, 0))
		[
			// Asset discovery indicator
			AssetDiscoveryIndicator
		]
	];

	// When using a class picker in list-view mode, the widget will auto-focus the search box
	// and allow the up and down arrow keys to navigate and enter to pick without using the mouse ever
	if ( InitOptions.Mode == EClassViewerMode::ClassPicker && InitOptions.DisplayMode == EClassViewerDisplayMode::ListView )
	{
		this->ChildSlot
			[
				SNew(SListViewSelectorDropdownMenu<TSharedPtr<FClassViewerNode>>, SearchBox, ClassList)
				[
					ClassViewerContent.ToSharedRef()
				]
			];
	}
	else
	{
		this->ChildSlot
			[
				ClassViewerContent.ToSharedRef()
			];
	}

	// Construct the class hierarchy.
	ClassViewer::Helpers::ConstructClassHierarchy();

	// Only want filter options enabled in browsing mode.
	if( this->InitOptions.Mode == EClassViewerMode::ClassBrowsing )
	{
		// Default the "Only Placeable" checkbox to be checked, it will check "Only Actors"
		MenuPlaceableOnly_Execute();
	}

	ClassViewer::Helpers::PopulateClassviewerDelegate.AddSP(this, &SClassViewer::Refresh);

	// Request delayed setting of focus to the search box
	bPendingFocusNextFrame = true;
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

TSharedRef<SWidget> SClassViewer::GetContent()
{
	return SharedThis( this );
}

SClassViewer::~SClassViewer()
{
	ClassViewer::Helpers::PopulateClassviewerDelegate.RemoveAll(this);
}

void SClassViewer::ClearSelection()
{
	ClassTree->ClearSelection();
}

void SClassViewer::OnGetChildrenForClassViewerTree( TSharedPtr<FClassViewerNode> InParent, TArray< TSharedPtr< FClassViewerNode > >& OutChildren )
{
	// Simply return the children, it's already setup.
	OutChildren = InParent->GetChildrenList();
}

void SClassViewer::OnClassViewerSelectionChanged( TSharedPtr<FClassViewerNode> Item, ESelectInfo::Type SelectInfo )
{
	// Do not act on selection change when it is for navigation
	if(SelectInfo == ESelectInfo::OnNavigation)
	{
		return;
	}

	// Sometimes the item is not valid anymore due to filtering.
	if(Item.IsValid() == false || Item->IsRestricted())
	{
		return;
	}

	if(InitOptions.Mode == EClassViewerMode::ClassBrowsing)
	{
		// Allows the user to right click in the level editor and select to place the selected class.
		GUnrealEd->SetCurrentClass( Item->Class.Get() );
	}
	else
	{
		UClass* Class = Item->Class.Get();

		// If the class is NULL and UnloadedBlueprintData is valid then attempt to load it. UnloadedBlueprintData is invalid in the case of a "None" item.
		if ( bEnableClassDynamicLoading && !Class && Item->UnloadedBlueprintData.IsValid() )
		{
			ClassViewer::Helpers::LoadClass( Item );
		}

		// Check if the item passes the filter, parent items might be displayed but filtered out and thus not desired to be selected.
		if ( ( Item->Class.IsValid() || !Class ))
		{
			if( Item->bPassesFilter )
			{
				OnClassPicked.ExecuteIfBound( Item->Class.Get() );
			}
			else
			{
				OnClassPicked.ExecuteIfBound( NULL );
			}
		}
	}
}

void SClassViewer::OnClassViewerExpansionChanged(TSharedPtr<FClassViewerNode> Item, bool bExpanded)
{
	// Sometimes the item is not valid anymore due to filtering.
	if (Item.IsValid() == false || Item->IsRestricted())
	{
		return;
	}

	ExpansionStateMap.Add(*(Item->GetClassName()), bExpanded);
}

TSharedPtr< SWidget > SClassViewer::BuildMenuWidget()
{
	bool bIsBlueprint;
	bool bHasBlueprint;
	TArray< TSharedPtr< FClassViewerNode > > SelectedList;

	// Based upon which mode the viewer is in, pull the selected item.
	if( InitOptions.DisplayMode == EClassViewerDisplayMode::TreeView )
	{
		SelectedList = ClassTree->GetSelectedItems();
	}
	else
	{
		SelectedList = ClassList->GetSelectedItems();
	}

	// If there is no selected item, return a null widget.
	if(SelectedList.Num() == 0)
	{
		return SNullWidget::NullWidget;
	}

	// If it is NOT stale, it has not been set (meaning it was never valid but now is invalid).
	if( bEnableClassDynamicLoading && !SelectedList[0]->Class.IsStale() && !SelectedList[0]->Class.IsValid() && SelectedList[0]->UnloadedBlueprintData.IsValid() )
	{
		ClassViewer::Helpers::LoadClass(SelectedList[0]);

		// Populate the tree/list so any changes to previously unloaded classes will be reflected.
		Refresh();
	}

	// Get the class and it's info.
	RightClickClass = SelectedList[0]->Class.Get();
	RightClickBlueprint = SelectedList[0]->Blueprint.Get();
	ClassViewer::Helpers::GetClassInfo(RightClickClass, bIsBlueprint, bHasBlueprint);
	
	if(RightClickBlueprint)
	{
		bHasBlueprint = true;
	}

	return ClassViewer::Helpers::CreateMenu(RightClickClass, bIsBlueprint, bHasBlueprint);
}

TSharedRef< ITableRow > SClassViewer::OnGenerateRowForClassViewer( TSharedPtr<FClassViewerNode> Item, const TSharedRef< STableViewBase >& OwnerTable )
{	
	// If the item was accepted by the filter, leave it bright, otherwise dim it.
	float AlphaValue = Item->bPassesFilter? 1.0f : 0.5f;
	TSharedRef< SClassItem > ReturnRow = SNew(SClassItem, OwnerTable)
		.ClassName(Item->GetClassName(InitOptions.bShowDisplayNames))
		.bIsPlaceable(Item->IsClassPlaceable())
		.HighlightText(&SearchBox->GetText())
		.TextColor(Item->IsClassPlaceable()? FLinearColor(0.2f, 0.4f, 0.6f, AlphaValue) : FLinearColor(1.0f, 1.0f, 1.0f, AlphaValue))
		.AssociatedNode(Item)
		.bIsInClassViewer( InitOptions.Mode == EClassViewerMode::ClassBrowsing )
		.bDynamicClassLoading( bEnableClassDynamicLoading )
		.OnDragDetected(this, &SClassViewer::OnDragDetected)
		.OnClassItemDoubleClicked(FOnClassItemDoubleClickDelegate::CreateSP(this, &SClassViewer::ToggleExpansionState_Helper));

	if ( !Item->GeneratedClassPackage.IsEmpty() )
	{
		ReturnRow->SetToolTipText(FText::FromString(Item->GeneratedClassPackage));
	}

	// Expand the item if needed.
	if (!bPendingSetExpansionStates)
	{
		bool* bIsExpanded = ExpansionStateMap.Find(*(Item->GetClassName()));
		if (bIsExpanded && *bIsExpanded)
		{
			bPendingSetExpansionStates = true;
		}
	}

	return ReturnRow;
}

const TArray< TSharedPtr< FClassViewerNode > > SClassViewer::GetSelectedItems() const
{
	if ( InitOptions.DisplayMode == EClassViewerDisplayMode::ListView )
	{
		return ClassList->GetSelectedItems();
	}

	return ClassTree->GetSelectedItems();
}

void SClassViewer::ExpandRootNodes()
{
	for (int32 NodeIdx = 0; NodeIdx < RootTreeItems.Num(); ++NodeIdx)
	{
		ExpansionStateMap.Add(*(RootTreeItems[NodeIdx]->GetClassName()), true);
		ClassTree->SetItemExpansion(RootTreeItems[NodeIdx], true);
	}
}

FReply SClassViewer::OnDragDetected( const FGeometry& Geometry, const FPointerEvent& PointerEvent )
{
	if(InitOptions.Mode == EClassViewerMode::ClassBrowsing)
	{
		const TArray< TSharedPtr< FClassViewerNode > > SelectedItems = GetSelectedItems();

		if ( SelectedItems.Num() > 0 && SelectedItems[0].IsValid() )
		{
			TSharedRef< FClassViewerNode > Item = SelectedItems[0].ToSharedRef();

			// If there is no class then we must spawn an FUnloadedClassDragDropOp so the class will be loaded when dropped.
			if ( UClass* Class = Item->Class.Get() )
			{
				// Spawn a loaded blueprint just like any other asset from the Content Browser.
				if ( Item->Blueprint.IsValid() )
				{
					TArray<FAssetData> InAssetData;
					InAssetData.Add(FAssetData(Item->Blueprint.Get()));
					return FReply::Handled().BeginDragDrop(FAssetDragDropOp::New(InAssetData));
				}
				else
				{
					// Add the UClass associated with this item to the drag event being spawned.
					return FReply::Handled().BeginDragDrop(FClassDragDropOp::New(TWeakObjectPtr<UClass>(Class)));
				}	
			}
			else
			{
				return FReply::Handled().BeginDragDrop(FUnloadedClassDragDropOp::New(FClassPackageData(Item->AssetName, Item->GeneratedClassPackage)));
			}
		}
	}

	return FReply::Unhandled();
}

void SClassViewer::OnOpenBlueprintTool()
{
	ClassViewer::Helpers::OpenBlueprintTool(RightClickBlueprint);
}

void SClassViewer::FindInContentBrowser()
{
	ClassViewer::Helpers::FindInContentBrowser(RightClickBlueprint, RightClickClass);
}

void SClassViewer::OnFilterTextChanged( const FText& InFilterText )
{
	// If the filter was empty, we want it to save the expansion states so when it returns to empty it can restore them.
	bSaveExpansionStates = !FilterSearchTerms.Num();

	FilterSearchTerms.Empty();

	FString CurrentFilterText = InFilterText.ToString();

	// Sanitize, parse and split the filter text into search terms
	CurrentFilterText.Trim().TrimTrailing();
	//FilterSearchTerms.Reset();
	{
		bool bIsWithinQuotedSection = false;
		FString NewSearchTerm;
		for( auto CurChar : CurrentFilterText )
		{
			// Keep an eye out for double-quotes.  We want to retain whitespace within a search term if
			// it has double-quotes around it
			if( CurChar == TCHAR('\"') )
			{
				// Toggle whether we're within a quoted section, but don't bother adding the quote character
				bIsWithinQuotedSection = !bIsWithinQuotedSection;
			}
			else if( bIsWithinQuotedSection || !FChar::IsWhitespace( CurChar ) )
			{
				// Add the character!
				NewSearchTerm.AppendChar( CurChar );
			}
			else
			{
				// Encountered whitespace, so add the search term up to here
				if( NewSearchTerm.Len() > 0 )
				{
					FilterSearchTerms.Add( NewSearchTerm );
					NewSearchTerm = FString();
				}
			}
		}

		// Encountered EOL, so add the search term up to here
		if( NewSearchTerm.Len() > 0 )
		{
			FilterSearchTerms.Add( NewSearchTerm );
			NewSearchTerm = FString();
		}

		if( bIsWithinQuotedSection )
		{
			// User forgot to terminate their double-quoted section.  No big deal.
		}
	}

	// Repopulate the list to show only what has not been filtered out.
	Refresh();
}

void SClassViewer::OnFilterTextCommitted(const FText& InText, ETextCommit::Type CommitInfo)
{
	if (CommitInfo == ETextCommit::OnEnter)
	{
		if (InitOptions.Mode == EClassViewerMode::ClassPicker)
		{
			TArray< TSharedPtr< FClassViewerNode > > SelectedList = ClassList->GetSelectedItems();
			TSharedPtr< FClassViewerNode > FirstSelected;

			UClass* Class = NULL;
			
			if (SelectedList.Num() > 0)
			{
				FirstSelected = SelectedList[0];
				Class = FirstSelected->Class.Get();

				// If the class is NULL and UnloadedBlueprintData is valid then attempt to load it. UnloadedBlueprintData is invalid in the case of a "None" item.
				if ( bEnableClassDynamicLoading && Class == nullptr && FirstSelected->UnloadedBlueprintData.IsValid())
				{
					ClassViewer::Helpers::LoadClass(FirstSelected);
					Class = FirstSelected->Class.Get();
				}

				// Check if the item passes the filter, parent items might be displayed but filtered out and thus not desired to be selected.
				if (Class && FirstSelected->bPassesFilter == true)
				{
					OnClassPicked.ExecuteIfBound(Class);
				}
			}
		}
	}
}

bool SClassViewer::Menu_CanExecute() const
{
	return true;
}

void SClassViewer::MenuActorsOnly_Execute()
{
	bIsActorsOnly = !bIsActorsOnly;

	// "Placeable Only" cannot be true when "Actors Only" is false.
	if(!bIsActorsOnly)
	{
		bIsPlaceableOnly = false;
	}

	Refresh();
}

bool SClassViewer::MenuActorsOnly_IsChecked() const
{
	return bIsActorsOnly;
}

void SClassViewer::MenuPlaceableOnly_Execute()
{
	bIsPlaceableOnly = !bIsPlaceableOnly;

	// "Actors Only" must be true when "Placeable Only" is true.
	if(bIsPlaceableOnly)
	{
		bIsActorsOnly = true;
	}

	Refresh();
}

bool SClassViewer::MenuPlaceableOnly_IsChecked() const
{
	return bIsPlaceableOnly;
}

void SClassViewer::MenuBlueprintBasesOnly_Execute()
{
	bIsBlueprintBaseOnly = !bIsBlueprintBaseOnly;

	Refresh();
}

bool SClassViewer::MenuBlueprintBasesOnly_IsChecked() const
{
	return bIsBlueprintBaseOnly;
}

void SClassViewer::FillFilterEntries( FMenuBuilder& MenuBuilder )
{
	MenuBuilder.BeginSection("ClassViewerFilterEntries");
	{
		MenuBuilder.AddMenuEntry( LOCTEXT("ActorsOnly", "Actors Only"), LOCTEXT( "ActorsOnly_Tooltip", "Filter the Class Viewer to show only actors" ), FSlateIcon(), FUIAction(FExecuteAction::CreateRaw(this, &SClassViewer::MenuActorsOnly_Execute), FCanExecuteAction::CreateRaw(this, &SClassViewer::Menu_CanExecute), FIsActionChecked::CreateRaw(this, &SClassViewer::MenuActorsOnly_IsChecked)), NAME_None, EUserInterfaceActionType::Check );
		MenuBuilder.AddMenuEntry( LOCTEXT("PlaceableOnly", "Placeable Only"), LOCTEXT( "PlaceableOnly_Tooltip", "Filter the Class Viewer to show only placeable actors." ), FSlateIcon(), FUIAction(FExecuteAction::CreateRaw(this, &SClassViewer::MenuPlaceableOnly_Execute), FCanExecuteAction::CreateRaw(this, &SClassViewer::Menu_CanExecute), FIsActionChecked::CreateRaw(this, &SClassViewer::MenuPlaceableOnly_IsChecked)), NAME_None, EUserInterfaceActionType::Check );
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("ClassViewerFilterEntries2");
	{
		MenuBuilder.AddMenuEntry( LOCTEXT("BlueprintsOnly", "Blueprint Class Bases Only"), LOCTEXT( "BlueprinsOnly_Tooltip", "Filter the Class Viewer to show only base blueprint classes." ), FSlateIcon(), FUIAction(FExecuteAction::CreateRaw(this, &SClassViewer::MenuBlueprintBasesOnly_Execute), FCanExecuteAction::CreateRaw(this, &SClassViewer::Menu_CanExecute), FIsActionChecked::CreateRaw(this, &SClassViewer::MenuBlueprintBasesOnly_IsChecked)), NAME_None, EUserInterfaceActionType::Check );
	}
	MenuBuilder.EndSection();
}

void SClassViewer::FillTreeEntries( FMenuBuilder& MenuBuilder )
{
	MenuBuilder.AddMenuEntry( LOCTEXT("ExpandAll", "Expand All"), LOCTEXT( "ExpandAll_Tooltip", "Expands the entire tree" ), FSlateIcon(), FUIAction( FExecuteAction::CreateSP(this, &SClassViewer::SetAllExpansionStates, bool(true)) ), NAME_None, EUserInterfaceActionType::Button );
	MenuBuilder.AddMenuEntry( LOCTEXT("CollapseAll", "Collapse All"), LOCTEXT( "CollapseAll_Tooltip", "Collapses the entire tree" ), FSlateIcon(), FUIAction( FExecuteAction::CreateSP(this, &SClassViewer::SetAllExpansionStates, bool(false)) ), NAME_None, EUserInterfaceActionType::Button );
}

void SClassViewer::SetAllExpansionStates(bool bInExpansionState)
{
	// Go through all the items in the root of the tree and recursively visit their children to set every item in the tree.
	for(int32 ChildIndex = 0; ChildIndex < RootTreeItems.Num(); ChildIndex++)
	{
		SetAllExpansionStates_Helper( RootTreeItems[ChildIndex], bInExpansionState );
	}
}

void SClassViewer::SetAllExpansionStates_Helper(TSharedPtr< FClassViewerNode > InNode, bool bInExpansionState)
{
	ClassTree->SetItemExpansion(InNode, bInExpansionState);

	// Recursively go through the children.
	for(int32 ChildIndex = 0; ChildIndex < InNode->GetChildrenList().Num(); ChildIndex++)
	{
		SetAllExpansionStates_Helper( InNode->GetChildrenList()[ChildIndex], bInExpansionState );
	}
}

void SClassViewer::ToggleExpansionState_Helper(TSharedPtr< FClassViewerNode > InNode)
{
	bool bExpanded = ClassTree->IsItemExpanded( InNode );
	ClassTree->SetItemExpansion(InNode, !bExpanded);
}

bool SClassViewer::ExpandFilteredInNodes(TSharedPtr<FClassViewerNode> InNode)
{
	bool bShouldExpand(InNode->bPassesFilter);

	for(int32 ChildIdx = 0; ChildIdx < InNode->GetChildrenList().Num(); ChildIdx++)
	{
		bShouldExpand |= ExpandFilteredInNodes(InNode->GetChildrenList()[ChildIdx]);
	}

	if(bShouldExpand)
	{
		ClassTree->SetItemExpansion(InNode, true);
	}

	return bShouldExpand;
}

void SClassViewer::MapExpansionStatesInTree( TSharedPtr<FClassViewerNode> InItem )
{
	ExpansionStateMap.Add( *(InItem->GetClassName()), ClassTree->IsItemExpanded( InItem ) );

	// Map out all the children, this will be done recursively.
	for( int32 ChildIdx(0); ChildIdx < InItem->GetChildrenList().Num(); ++ChildIdx )
	{
		MapExpansionStatesInTree( InItem->GetChildrenList()[ChildIdx] );
	}
}

void SClassViewer::SetExpansionStatesInTree( TSharedPtr<FClassViewerNode> InItem )
{
	bool* bIsExpanded = ExpansionStateMap.Find( *(InItem->GetClassName()) );
	if( bIsExpanded )
	{
		ClassTree->SetItemExpansion( InItem, *bIsExpanded );

		// No reason to set expansion states if the parent is not expanded, it does not seem to do anything.
		if( *bIsExpanded )
		{
			for( int32 ChildIdx(0); ChildIdx < InItem->GetChildrenList().Num(); ++ChildIdx )
			{
				SetExpansionStatesInTree( InItem->GetChildrenList()[ChildIdx] );
			}
		}
	}
	else
	{
		// Default to no expansion.
		ClassTree->SetItemExpansion( InItem, false );
	}
}

void SClassViewer::Populate()
{
	bPendingSetExpansionStates = false;

	// If showing a class tree, we may need to save expansion states.
	if( ClassTree->GetVisibility() == EVisibility::Visible )
	{
		if( bSaveExpansionStates )
		{
			for( int32 ChildIdx(0); ChildIdx < RootTreeItems.Num(); ++ChildIdx )
			{
				// Check if the item is actually expanded or if it's only expanded because it is root level.
				bool* bIsExpanded = ExpansionStateMap.Find( *(RootTreeItems[ChildIdx]->GetClassName()) );
				if((bIsExpanded && !*bIsExpanded) || !bIsExpanded)
				{
					ClassTree->SetItemExpansion( RootTreeItems[ChildIdx], false );
				}

				// Recursively map out the expansion state of the tree-node.
				MapExpansionStatesInTree( RootTreeItems[ChildIdx] );
			}
		}

		// This is set to false before the call to populate when it is not desired.
		bSaveExpansionStates = true;
	}

	// Empty the tree out so it can be redone.
	RootTreeItems.Empty();

	// Based on if the list or tree is visible we create what will be displayed differently.
	if(ClassTree->GetVisibility() == EVisibility::Visible)
	{
		// The root node for the tree, will be "Object" which we will skip.
		TSharedPtr<FClassViewerNode> RootNode;

		// Get the class tree, passing in certain filter options.
		ClassViewer::Helpers::GetClassTree(InitOptions, RootNode, FilterSearchTerms, MenuPlaceableOnly_IsChecked(), MenuActorsOnly_IsChecked(), MenuBlueprintBasesOnly_IsChecked(), bShowUnloadedBlueprints );

		// Check if we will restore expansion states, we will not if there is filtering happening.
		const bool bRestoreExpansionState = !FilterSearchTerms.Num();

		if(InitOptions.bShowObjectRootClass)
		{
			RootTreeItems.Add(RootNode);

			if( bRestoreExpansionState )
			{
				SetExpansionStatesInTree( RootNode );
			}

			// Expand any items that pass the filter.
			if(FilterSearchTerms.Num() > 0)
			{
				ExpandFilteredInNodes(RootNode);
			}
		}
		else
		{
			// Add all the children of the "Object" root.
			for(int32 ChildIndex = 0; ChildIndex < RootNode->GetChildrenList().Num(); ChildIndex++)
			{
				RootTreeItems.Add(RootNode->GetChildrenList()[ChildIndex]);
				if( bRestoreExpansionState )
				{
					SetExpansionStatesInTree( RootTreeItems[ChildIndex] );
				}

				// Expand any items that pass the filter.
				if(FilterSearchTerms.Num() > 0)
				{
					ExpandFilteredInNodes(RootNode->GetChildrenList()[ChildIndex]);
				}
			}
		}

		// Only display this option if the user wants it and in Picker Mode.
		if(InitOptions.bShowNoneOption && InitOptions.Mode == EClassViewerMode::ClassPicker)
		{
			// @todo - It would seem smart to add this in before the other items, since it needs to be on top. However, that causes strange issues with saving/restoring expansion states. 
			// This is likely not very efficient since the list can have hundreds and even thousands of items.
			RootTreeItems.Insert(CreateNoneOption(), 0);
		}

		// Now that new items are in the tree, we need to request a refresh.
		ClassTree->RequestTreeRefresh();
	}
	else
	{
		// Get the class list, passing in certain filter options.
		ClassViewer::Helpers::GetClassList(InitOptions, RootTreeItems, FilterSearchTerms, MenuPlaceableOnly_IsChecked(), MenuActorsOnly_IsChecked(), MenuBlueprintBasesOnly_IsChecked(), bShowUnloadedBlueprints);

		// Sort the list alphabetically.
		struct FCompareFClassViewerNode
		{
			FORCEINLINE bool operator()( TSharedPtr<FClassViewerNode> A, TSharedPtr<FClassViewerNode> B ) const
			{
				check(A.IsValid());
				check(B.IsValid());
				// Pull out the FString, for ease of reading.
				const FString& AString = *A->GetClassName().Get();
				const FString& BString = *B->GetClassName().Get();
				return AString < BString;
			}
		};
		RootTreeItems.Sort( FCompareFClassViewerNode() );

		// Only display this option if the user wants it and in Picker Mode.
		if(InitOptions.bShowNoneOption && InitOptions.Mode == EClassViewerMode::ClassPicker)
		{
			// @todo - It would seem smart to add this in before the other items, since it needs to be on top. However, that causes strange issues with saving/restoring expansion states. 
			// This is likely not very efficient since the list can have hundreds and even thousands of items.
			RootTreeItems.Insert(CreateNoneOption(), 0);
		}

		// Now that new items are in the list, we need to request a refresh.
		ClassList->RequestListRefresh();
	}
}

FReply SClassViewer::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	// Forward key down to class tree
	return ClassTree->OnKeyDown(MyGeometry,InKeyEvent);
}


FReply SClassViewer::OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent )
{
	if (RootTreeItems.Num() > 0)
	{
		ClassTree->SetItemSelection(RootTreeItems[0], true, ESelectInfo::OnMouseClick);
		ClassTree->SetItemExpansion(RootTreeItems[0], true);
		OnClassViewerSelectionChanged(RootTreeItems[0],ESelectInfo::OnMouseClick);
	}

	FSlateApplication::Get().SetKeyboardFocus(SearchBox.ToSharedRef(), EFocusCause::SetDirectly);
	
	return FReply::Unhandled();
}

bool SClassViewer::SupportsKeyboardFocus() const 
{
	return true;
}

void SClassViewer::DestroyClassHierarchy()
{
	ClassViewer::Helpers::DestroyClassHierachy();
}

TSharedPtr<FClassViewerNode> SClassViewer::CreateNoneOption()
{
	TSharedPtr<FClassViewerNode> NoneItem = MakeShareable( new FClassViewerNode("None", "None") );

	// The item "passes" the filter so it does not appear grayed out.
	NoneItem->bPassesFilter = true;

	return NoneItem;
}

void SClassViewer::Refresh()
{
	bNeedsRefresh = true;
}

void SClassViewer::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{	
	// Will populate the class hierarchy as needed.
	ClassViewer::Helpers::PopulateClassHierarchy();

	// Move focus to search box
	if (bPendingFocusNextFrame && SearchBox.IsValid())
	{
		FWidgetPath WidgetToFocusPath;
		FSlateApplication::Get().GeneratePathToWidgetUnchecked( SearchBox.ToSharedRef(), WidgetToFocusPath );
		FSlateApplication::Get().SetKeyboardFocus( WidgetToFocusPath, EFocusCause::SetDirectly );
		bPendingFocusNextFrame = false;
	}

	if (bNeedsRefresh)
	{
		bNeedsRefresh = false;
		Populate();

		if (InitOptions.bExpandRootNodes)
		{
			ExpandRootNodes();
		}
	}

	if (bPendingSetExpansionStates)
	{
		check(RootTreeItems.Num() > 0);
		SetExpansionStatesInTree(RootTreeItems[0]);
		bPendingSetExpansionStates = false;
	}
}

bool SClassViewer::IsClassAllowed(const UClass* InClass) const
{
	return ClassViewer::Helpers::IsClassAllowed(InitOptions, InClass);
}

#undef LOCTEXT_NAMESPACE
