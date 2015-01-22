// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "AssetDeleteModel.h"

#include "ObjectTools.h"
#include "AssetRegistryModule.h"
#include "AssetEditorManager.h"

#define LOCTEXT_NAMESPACE "FAssetDeleteModel"

FAssetDeleteModel::FAssetDeleteModel( const TArray<UObject*>& InObjectsToDelete )
	: State( StartScanning )
	, bPendingObjectsCanBeReplaced(false)
	, bIsAnythingReferencedInMemoryByNonUndo(false)
	, bIsAnythingReferencedInMemoryByUndo(false)
	, PendingDeleteIndex(0)
	, ObjectsDeleted(0)
{
	// Purge unclaimed objects.
	CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );

	// Create a pending delete object for each object needing to be deleted so that we can store
	// reference tracing information.
	for ( UObject* ObjectToDelete : InObjectsToDelete )
	{
		AddObjectToDelete( ObjectToDelete );
	}
}

FAssetDeleteModel::~FAssetDeleteModel()
{
}

void FAssetDeleteModel::AddObjectToDelete( UObject* InObject )
{
	PendingDeletes.Add(MakeShareable(new FPendingDelete(InObject)));

	PrepareToDelete(InObject);

	SetState(StartScanning);
}

void FAssetDeleteModel::SetState( EState NewState )
{
	if ( State != NewState )
	{
		State = NewState;
		if ( StateChanged.IsBound() )
		{
			StateChanged.Broadcast( NewState );
		}
	}
}

void FAssetDeleteModel::Tick( const float InDeltaTime )
{
	switch ( State )
	{
	case Waiting:
		break;
	case StartScanning:
		OnDiskReferences = TSet<FName>();
		bIsAnythingReferencedInMemoryByNonUndo = false;
		bIsAnythingReferencedInMemoryByUndo = false;
		PendingDeleteIndex = 0;

		SetState(Scanning);
		break;
	case Scanning:
		if ( PendingDeleteIndex < PendingDeletes.Num() )
		{
			TSharedPtr<FPendingDelete>& PendingDelete = PendingDeletes[PendingDeleteIndex];

			PendingDelete->CheckForReferences();

			// Add references are not part of the objects being deleted.
			int NonPendingDeletedExternalOnDiskReferences = 0;
			for ( FName& Reference : PendingDelete->DiskReferences )
			{
				if ( !IsAssetInPendingDeletes( Reference ) )
				{
					OnDiskReferences.Add( Reference );
					NonPendingDeletedExternalOnDiskReferences++;
				}
			}
			PendingDelete->RemainingDiskReferences = NonPendingDeletedExternalOnDiskReferences;

			// Count up all the external references
			int32 NonPendingDeletedExternalInMemoryReferences = 0;

			for ( const FReferencerInformation& Reference : PendingDelete->MemoryReferences.ExternalReferences )
			{
				if ( !IsObjectInPendingDeletes( Reference.Referencer ) )
				{
					NonPendingDeletedExternalInMemoryReferences++;
				}
			}
			PendingDelete->RemainingMemoryReferences = NonPendingDeletedExternalInMemoryReferences;

			bIsAnythingReferencedInMemoryByNonUndo |= PendingDelete->RemainingMemoryReferences > 0;
			bIsAnythingReferencedInMemoryByUndo |= PendingDelete->IsReferencedInMemoryByUndo();

			PendingDeleteIndex++;
		}
		else
		{
			SetState(UpdateActions);
		}
		break;
	case UpdateActions:
		bPendingObjectsCanBeReplaced = ComputeCanReplaceReferences();
		SetState(Finished);
		break;
	case Finished:
		break;
	}
}

bool FAssetDeleteModel::CanDelete() const
{
	return !CanForceDelete();
}

bool FAssetDeleteModel::DoDelete()
{
	if ( !CanDelete() )
	{
		return false;
	}

	TArray<UObject*> ObjectsToDelete;
	for ( TSharedPtr< FPendingDelete >& PendingDelete : PendingDeletes )
	{
		ObjectsToDelete.Add( PendingDelete->GetObject() );
	}

	ObjectsDeleted = ObjectTools::DeleteObjectsUnchecked(ObjectsToDelete);

	return true;
}

bool FAssetDeleteModel::CanForceDelete() const
{
	// We can only force delete when they are still referenced in memory or still referenced on disk.
	return bIsAnythingReferencedInMemoryByNonUndo || (OnDiskReferences.Num() > 0);
}

bool FAssetDeleteModel::DoForceDelete()
{
	if ( !CanForceDelete() )
	{
		return false;
	}

	TArray<UObject*> ObjectsToForceDelete;
	for ( TSharedPtr< FPendingDelete >& PendingDelete : PendingDeletes )
	{
		ObjectsToForceDelete.Add(PendingDelete->GetObject());
	}

	ObjectsDeleted = ObjectTools::ForceDeleteObjects(ObjectsToForceDelete, false);

	return true;
}

bool FAssetDeleteModel::ComputeCanReplaceReferences()
{
	TArray<UObject*> PendingDeletedObjects;
	for ( const TSharedPtr< FPendingDelete >& PendingDelete : PendingDeletes )
	{
		PendingDeletedObjects.Add(PendingDelete->GetObject());
	}

	return ObjectTools::AreObjectsOfEquivalantType( PendingDeletedObjects );
}

bool FAssetDeleteModel::CanReplaceReferences() const
{
	return bPendingObjectsCanBeReplaced;
}

bool FAssetDeleteModel::CanReplaceReferencesWith( const FAssetData& InAssetData ) const
{
	// First make sure that it's not an object we're preparing to delete
	if ( IsAssetInPendingDeletes( InAssetData.PackageName ) )
	{
		return true;
	}

	const UClass* FirstPendingDelete = PendingDeletes[0]->GetObject()->GetClass();
	const UClass* AssetDataClass = InAssetData.GetClass();

	// If the class isn't loaded we can't compare them, so just return true that we'll be
	// filtering it from the list.
	if ( AssetDataClass == nullptr )
	{
		return true;
	}

	// Only show objects that are of replaceable because their classes are compatible.
	return !ObjectTools::AreClassesInterchangeable( FirstPendingDelete, AssetDataClass );
}

bool FAssetDeleteModel::DoReplaceReferences(const FAssetData& ReplaceReferencesWith )
{
	if ( !CanReplaceReferences() )
	{
		return false;
	}

	// Find which object the user has elected to be the "object to consolidate to"
	UObject* ObjectToConsolidateTo = ReplaceReferencesWith.GetAsset();
	check( ObjectToConsolidateTo );

	TArray<UObject*> FinalConsolidationObjects;
	for ( TSharedPtr< FPendingDelete >& PendingDelete : PendingDeletes )
	{
		FinalConsolidationObjects.Add(PendingDelete->GetObject());
	}

	// The consolidation action clears the array, so we need to save the count.
	int32 ObjectsBeingDeletedCount = FinalConsolidationObjects.Num();

	// Perform the object consolidation
	bool bShowDeleteConfirmation = false;
	ObjectTools::FConsolidationResults ConsResults = ObjectTools::ConsolidateObjects( ObjectToConsolidateTo, FinalConsolidationObjects, bShowDeleteConfirmation );

	// If the consolidation went off successfully with no failed objects, prompt the user to checkout/save the packages dirtied by the operation
	if ( ConsResults.DirtiedPackages.Num() > 0 && ConsResults.FailedConsolidationObjs.Num() == 0)
	{
		FEditorFileUtils::PromptForCheckoutAndSave( ConsResults.DirtiedPackages, false, true );
	}
	// If the consolidation resulted in failed (partially consolidated) objects, do not save, and inform the user no save attempt was made
	else if ( ConsResults.FailedConsolidationObjs.Num() > 0)
	{
		//DisplayMessage( true, LOCTEXT( "Consolidate_WarningPartial", "Not all objects could be consolidated, no save has occurred" ).ToString() );
	}

	ObjectsDeleted = ObjectsBeingDeletedCount - ( ConsResults.FailedConsolidationObjs.Num() + ConsResults.InvalidConsolidationObjs.Num() );

	return true;
}

bool FAssetDeleteModel::IsObjectInPendingDeletes( const UObject* InObject ) const
{
	for ( const TSharedPtr< FPendingDelete >& PendingDelete : PendingDeletes )
	{
		if ( PendingDelete->IsObjectContained(InObject) )
		{
			return true;
		}
	}

	return false;
}

bool FAssetDeleteModel::IsAssetInPendingDeletes( const FName& PackageName ) const
{
	for ( const TSharedPtr< FPendingDelete >& PendingDelete : PendingDeletes )
	{
		if ( PendingDelete->IsAssetContained(PackageName) )
		{
			return true;
		}
	}

	return false;
}

float FAssetDeleteModel::GetProgress() const
{
	return PendingDeleteIndex / (float)PendingDeletes.Num();
}

FText FAssetDeleteModel::GetProgressText() const
{
	if ( PendingDeleteIndex < PendingDeletes.Num() )
	{
		return FText::FromString(PendingDeletes[PendingDeleteIndex]->GetObject()->GetName());
	}
	else
	{
		return LOCTEXT( "Done", "Done!" );
	}
}

bool FAssetDeleteModel::GoToNextReferenceInLevel() const
{
	// Clear the current selection
	GEditor->SelectNone(false, false);

	UWorld* RepresentingWorld = NULL;

	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if ( Context.WorldType == EWorldType::PIE )
		{
			RepresentingWorld = Context.World();
			break;
		}
		else if ( Context.WorldType == EWorldType::Editor )
		{
			RepresentingWorld = Context.World();
		}
	}

	// If there is no world we definitely can't find any actors in the level
	if ( !RepresentingWorld )
	{
		return false;
	}

	bool bRepresentingPlayWorld = RepresentingWorld->WorldType == EWorldType::PIE;

	// @todo I'm not sure what should be done if PIE is active...
	if ( bRepresentingPlayWorld )
	{
		return false;
	}

	// Go over every pending deleted object, and for all of its references go to the first
	// one in the current world.
	for ( const TSharedPtr<FPendingDelete>& PendingDelete : PendingDeletes )
	{
		for ( const FReferencerInformation& Reference : PendingDelete->MemoryReferences.ExternalReferences )
		{
			const AActor* ReferencingActor = Cast<const AActor>(Reference.Referencer);

			// If the referencer isn't an actor, it might be a component.
			if ( !ReferencingActor )
			{
				const UActorComponent* ReferencingComponent = Cast<const UActorComponent>(Reference.Referencer);
				if ( ReferencingComponent )
				{
					ReferencingActor = ReferencingComponent->GetOwner();
				}
			}

			if ( ReferencingActor )
			{
				if ( ReferencingActor->GetWorld() == RepresentingWorld )
				{
					GEditor->SelectActor(const_cast<AActor*>( ReferencingActor ), true, true, true);

					// Point the camera at it
					GUnrealEd->Exec(ReferencingActor->GetWorld(), TEXT("CAMERA ALIGN ACTIVEVIEWPORTONLY"));

					return true;
				}
			}
		}
	}
	
	return false;
}

int32 FAssetDeleteModel::GetDeletedObjectCount() const
{
	return ObjectsDeleted;
}

void FAssetDeleteModel::PrepareToDelete(UObject* InObject)
{
	if ( InObject->IsA<UObjectRedirector>() )
	{
		// Add all redirectors found in this package to the redirectors to delete list.
		// All redirectors in this package should be fixed up.
		UPackage* RedirectorPackage = InObject->GetOutermost();
		TArray<UObject*> AssetsInRedirectorPackage;
		
		GetObjectsWithOuter(RedirectorPackage, AssetsInRedirectorPackage, /*bIncludeNestedObjects=*/false);
		UMetaData* PackageMetaData = NULL;
		bool bContainsAtLeastOneOtherAsset = false;

		for ( auto ObjIt = AssetsInRedirectorPackage.CreateConstIterator(); ObjIt; ++ObjIt )
		{
			if ( UObjectRedirector* Redirector = Cast<UObjectRedirector>(*ObjIt) )
			{
				Redirector->RemoveFromRoot();
			}
			else if ( UMetaData* MetaData = Cast<UMetaData>(*ObjIt) )
			{
				PackageMetaData = MetaData;
			}
			else
			{
				bContainsAtLeastOneOtherAsset = true;
			}
		}

		if ( !bContainsAtLeastOneOtherAsset )
		{
			RedirectorPackage->RemoveFromRoot();
			ULinkerLoad* Linker = ULinkerLoad::FindExistingLinkerForPackage(RedirectorPackage);
			if ( Linker )
			{
				Linker->RemoveFromRoot();
			}

			// @todo we shouldnt be worrying about metadata objects here, ObjectTools::CleanUpAfterSuccessfulDelete should
			if ( PackageMetaData )
			{
				PackageMetaData->RemoveFromRoot();
				PendingDeletes.AddUnique(MakeShareable(new FPendingDelete(PackageMetaData)));
			}
		}
	}
}

// FPendingDelete
//-----------------------------------------------------------------

FPendingDelete::FPendingDelete(UObject* InObject)
	: RemainingDiskReferences(0)
	, RemainingMemoryReferences(0)
	, Object(InObject)
	, bReferencesChecked(false)
	, bIsReferencedInMemoryByNonUndo(false)
	, bIsReferencedInMemoryByUndo(false)
	, bIsInternal(false)
{
	// Blueprints actually contain 3 assets, the UBlueprint, GeneratedClass and SkeletonGeneratedClass
	// we need to add them all to the pending deleted objects so that the memory reference detection system
	// can know those references don't count towards the in memory references.
	if ( InObject->IsA(UBlueprint::StaticClass()) )
	{
		UBlueprint* BlueprintObj = Cast<UBlueprint>(InObject);

		if ( BlueprintObj->GeneratedClass != NULL )
		{
			InternalObjects.Add(BlueprintObj->GeneratedClass);
		}

		if ( BlueprintObj->SkeletonGeneratedClass != NULL )
		{
			InternalObjects.Add(BlueprintObj->SkeletonGeneratedClass);
		}
	}

	FAssetData InAssetData(InObject);
	// Filter out any non assets
	if ( !InAssetData.IsUAsset() )
	{
		bIsInternal = true;
	}
}

bool FPendingDelete::IsObjectContained(const UObject* InObject) const
{
	const UObject* InObjectParent = InObject;

	// If the objects are in the same package then it should be safe to delete them since the package
	// will be marked for garbage collection.
	if ( Object->GetOutermost() == InObject->GetOutermost() )
	{
		return true;
	}

	// We need to check if the object or any of it's parents are children of the object being deleted, and so
	// can safely be ignored.
	while ( InObjectParent != NULL )
	{
		if ( Object == InObjectParent )
		{
			return true;
		}

		// Also check if it's a child of any of the internal objects.
		for ( int32 InternalIndex = 0; InternalIndex < InternalObjects.Num(); InternalIndex++ )
		{
			if ( InternalObjects[InternalIndex] == InObjectParent )
			{
				return true;
			}
		}

		InObjectParent = InObjectParent->GetOuter();
	}

	return false;
}

bool FPendingDelete::IsAssetContained(const FName& PackageName) const
{
	if ( Object->GetOutermost()->GetFName() == PackageName )
	{
		return true;
	}

	return false;
}

void FPendingDelete::CheckForReferences()
{
	if ( bReferencesChecked )
	{
		return;
	}

	bReferencesChecked = true;

	// Load the asset registry module
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>( TEXT( "AssetRegistry" ) );

	AssetRegistryModule.Get().GetReferencers(Object->GetOutermost()->GetFName(), DiskReferences);

	// Check and see whether we are referenced by any objects that won't be garbage collected (*including* the undo buffer)
	FReferencerInformationList ReferencesIncludingUndo;
	bool bReferencedInMemoryOrUndoStack = IsReferenced(Object, GARBAGE_COLLECTION_KEEPFLAGS, true, &ReferencesIncludingUndo);

	// Determine the in-memory references, *excluding* the undo buffer
	GEditor->Trans->DisableObjectSerialization();
	bIsReferencedInMemoryByNonUndo = IsReferenced(Object, GARBAGE_COLLECTION_KEEPFLAGS, true, &MemoryReferences);
	GEditor->Trans->EnableObjectSerialization();

	// see if this object is the transaction buffer - set a flag so we know we need to clear the undo stack
	const int32 TotalReferenceCount = ReferencesIncludingUndo.ExternalReferences.Num() + ReferencesIncludingUndo.InternalReferences.Num();
	const int32 NonUndoReferenceCount = MemoryReferences.ExternalReferences.Num() + MemoryReferences.InternalReferences.Num();

	bIsReferencedInMemoryByUndo = TotalReferenceCount > NonUndoReferenceCount;
}

bool FPendingDelete::operator == ( const FPendingDelete& Other ) const
{
	return Object == Other.Object;
}

#undef LOCTEXT_NAMESPACE