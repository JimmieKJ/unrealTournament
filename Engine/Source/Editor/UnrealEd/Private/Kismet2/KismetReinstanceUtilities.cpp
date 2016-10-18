// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetReinstanceUtilities.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "BlueprintEditorUtils.h"
#include "Layers/ILayers.h"
#include "ComponentInstanceDataCache.h"
#include "Engine/DynamicBlueprintBinding.h"
#include "BlueprintEditor.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/Selection.h"
#include "BlueprintEditorSettings.h"
#include "Animation/AnimInstance.h"

DECLARE_CYCLE_STAT(TEXT("Replace Instances"), EKismetReinstancerStats_ReplaceInstancesOfClass, STATGROUP_KismetReinstancer );
DECLARE_CYCLE_STAT(TEXT("Find Referencers"), EKismetReinstancerStats_FindReferencers, STATGROUP_KismetReinstancer );
DECLARE_CYCLE_STAT(TEXT("Replace References"), EKismetReinstancerStats_ReplaceReferences, STATGROUP_KismetReinstancer );
DECLARE_CYCLE_STAT(TEXT("Construct Replacements"), EKismetReinstancerStats_ReplacementConstruction, STATGROUP_KismetReinstancer );
DECLARE_CYCLE_STAT(TEXT("Update Bytecode References"), EKismetReinstancerStats_UpdateBytecodeReferences, STATGROUP_KismetReinstancer );
DECLARE_CYCLE_STAT(TEXT("Recompile Child Classes"), EKismetReinstancerStats_RecompileChildClasses, STATGROUP_KismetReinstancer );
DECLARE_CYCLE_STAT(TEXT("Replace Classes Without Reinstancing"), EKismetReinstancerStats_ReplaceClassNoReinsancing, STATGROUP_KismetReinstancer );
DECLARE_CYCLE_STAT(TEXT("Reinstance Objects"), EKismetCompilerStats_ReinstanceObjects, STATGROUP_KismetCompiler);
DECLARE_CYCLE_STAT(TEXT("Refresh Dependent Blueprints In Reinstancer"), EKismetCompilerStats_RefreshDependentBlueprintsInReinstancer, STATGROUP_KismetCompiler);
DECLARE_CYCLE_STAT(TEXT("Recreate UberGraphPersistentFrame"), EKismetCompilerStats_RecreateUberGraphPersistentFrame, STATGROUP_KismetCompiler);

struct FReplaceReferenceHelper
{
	static void IncludeCDO(UClass* OldClass, UClass* NewClass, TMap<UObject*, UObject*> &OldToNewInstanceMap, TArray<UObject*> &SourceObjects, UObject* OriginalCDO)
	{
		UObject* OldCDO = OldClass->GetDefaultObject();
		UObject* NewCDO = NewClass->GetDefaultObject();

		// Add the old->new CDO mapping into the fixup map
		OldToNewInstanceMap.Add(OldCDO, NewCDO);
		// Add in the old CDO to this pass, so CDO references are fixed up
		SourceObjects.Add(OldCDO);

		if (OriginalCDO)
		{
			OldToNewInstanceMap.Add(OriginalCDO, NewCDO);
			SourceObjects.Add(OriginalCDO);
		}
	}

	static void IncludeClass(UClass* OldClass, UClass* NewClass, TMap<UObject*, UObject*> &OldToNewInstanceMap, TArray<UObject*> &SourceObjects, TArray<UObject*> &ObjectsToReplace)
	{
		OldToNewInstanceMap.Add(OldClass, NewClass);
		SourceObjects.Add(OldClass);

		if (auto OldCDO = OldClass->GetDefaultObject(false))
		{
			ObjectsToReplace.Add(OldCDO);
		}
	}

	static void FindAndReplaceReferences(const TArray<UObject*>& SourceObjects, TSet<UObject*>* ObjectsThatShouldUseOldStuff, const TArray<UObject*>& ObjectsToReplace, const TMap<UObject*, UObject*>& OldToNewInstanceMap, const TMap<FStringAssetReference, UObject*>& ReinstancedObjectsWeakReferenceMap)
	{
		// Find everything that references these objects
		TSet<UObject *> Targets;
		{
			BP_SCOPED_COMPILER_EVENT_STAT(EKismetReinstancerStats_FindReferencers);

			TFindObjectReferencers<UObject> Referencers(SourceObjects, NULL, false);
			for (TFindObjectReferencers<UObject>::TIterator It(Referencers); It; ++It)
			{
				UObject* Referencer = It.Value();
				if (!ObjectsThatShouldUseOldStuff || !ObjectsThatShouldUseOldStuff->Contains(Referencer))
				{
					Targets.Add(Referencer);
				}
			}
		}

		{
			BP_SCOPED_COMPILER_EVENT_STAT(EKismetReinstancerStats_ReplaceReferences);

			for (TSet<UObject *>::TIterator It(Targets); It; ++It)
			{
				UObject* Obj = *It;
				if (!ObjectsToReplace.Contains(Obj)) // Don't bother trying to fix old objects, this would break them
				{
					// The class for finding and replacing weak references.
					// We can't relay on "standard" weak references replacement as
					// it depends on FStringAssetReference::ResolveObject, which
					// tries to find the object with the stored path. It is
					// impossible, cause above we deleted old actors (after
					// spawning new ones), so during objects traverse we have to
					// find FStringAssetReferences with the raw given path taken
					// before deletion of old actors and fix them.
					class ReferenceReplace : public FArchiveReplaceObjectRef < UObject >
					{
					public:
						ReferenceReplace(UObject* InSearchObject, const TMap<UObject*, UObject*>& InReplacementMap, TMap<FStringAssetReference, UObject*> InWeakReferencesMap)
							: FArchiveReplaceObjectRef<UObject>(InSearchObject, InReplacementMap, false, false, false, true), WeakReferencesMap(InWeakReferencesMap)
						{
							SerializeSearchObject();
						}

						FArchive& operator<<(FStringAssetReference& Ref) override
						{
							const UObject*const* PtrToObjPtr = WeakReferencesMap.Find(Ref);

							if (PtrToObjPtr != nullptr)
							{
								Ref = *PtrToObjPtr;
							}

							return *this;
						}

						FArchive& operator<<(FAssetPtr& Ref) override
						{
							return operator<<(Ref.GetUniqueID());
						}

					private:
						const TMap<FStringAssetReference, UObject*>& WeakReferencesMap;
					};

					ReferenceReplace ReplaceAr(Obj, OldToNewInstanceMap, ReinstancedObjectsWeakReferenceMap);
				}
			}
		}
	}
};

/////////////////////////////////////////////////////////////////////////////////
// FBlueprintCompileReinstancer

TSet<TWeakObjectPtr<UBlueprint>> FBlueprintCompileReinstancer::DependentBlueprintsToRefresh = TSet<TWeakObjectPtr<UBlueprint>>();
TSet<TWeakObjectPtr<UBlueprint>> FBlueprintCompileReinstancer::DependentBlueprintsToRecompile = TSet<TWeakObjectPtr<UBlueprint>>();
TSet<TWeakObjectPtr<UBlueprint>> FBlueprintCompileReinstancer::DependentBlueprintsToByteRecompile = TSet<TWeakObjectPtr<UBlueprint>>();
TSet<TWeakObjectPtr<UBlueprint>> FBlueprintCompileReinstancer::CompiledBlueprintsToSave = TSet<TWeakObjectPtr<UBlueprint>>();

UClass* FBlueprintCompileReinstancer::HotReloadedOldClass = nullptr;
UClass* FBlueprintCompileReinstancer::HotReloadedNewClass = nullptr;

FBlueprintCompileReinstancer::FBlueprintCompileReinstancer(UClass* InClassToReinstance, bool bIsBytecodeOnly, bool bSkipGC, bool bAutoInferSaveOnCompile/* = true*/)
	: ClassToReinstance(InClassToReinstance)
	, DuplicatedClass(NULL)
	, OriginalCDO(NULL)
	, bHasReinstanced(false)
	, bSkipGarbageCollection(bSkipGC)
	, ReinstClassType(RCT_Unknown)
	, ClassToReinstanceDefaultValuesCRC(0)
	, bIsRootReinstancer(false)
	, bAllowResaveAtTheEndIfRequested(false)
{
	if( InClassToReinstance != NULL )
	{
		if (FKismetEditorUtilities::IsClassABlueprintSkeleton(ClassToReinstance))
		{
			ReinstClassType = RCT_BpSkeleton;
		}
		else if (ClassToReinstance->HasAnyClassFlags(CLASS_CompiledFromBlueprint))
		{
			ReinstClassType = RCT_BpGenerated;
		}
		else if (ClassToReinstance->HasAnyClassFlags(CLASS_Native))
		{
			ReinstClassType = RCT_Native;
		}
		bAllowResaveAtTheEndIfRequested = bAutoInferSaveOnCompile && !bIsBytecodeOnly && (ReinstClassType != RCT_BpSkeleton);

		SaveClassFieldMapping(InClassToReinstance);

		// Remember the initial CDO for the class being resinstanced
		OriginalCDO = ClassToReinstance->GetDefaultObject();

		// Duplicate the class we're reinstancing into the transient package.  We'll re-class all objects we find to point to this new class
		GIsDuplicatingClassForReinstancing = true;
		ClassToReinstance->ClassFlags |= CLASS_NewerVersionExists;
		const FName RenistanceName = MakeUniqueObjectName(GetTransientPackage(), ClassToReinstance->GetClass(), *FString::Printf(TEXT("REINST_%s"), *ClassToReinstance->GetName()));
		DuplicatedClass = (UClass*)StaticDuplicateObject(ClassToReinstance, GetTransientPackage(), RenistanceName, ~RF_Transactional); 
		// If you compile a blueprint that is part of the rootset, there's no reason for the REINST version to be part of the rootset:
		DuplicatedClass->RemoveFromRoot();

		ClassToReinstance->ClassFlags &= ~CLASS_NewerVersionExists;
		GIsDuplicatingClassForReinstancing = false;

		auto BPClassToReinstance = Cast<UBlueprintGeneratedClass>(ClassToReinstance);
		auto BPGDuplicatedClass = Cast<UBlueprintGeneratedClass>(DuplicatedClass);
		if (BPGDuplicatedClass && BPClassToReinstance && BPClassToReinstance->OverridenArchetypeForCDO)
		{
			BPGDuplicatedClass->OverridenArchetypeForCDO = BPClassToReinstance->OverridenArchetypeForCDO;
		}

		auto DuplicatedClassUberGraphFunction = BPGDuplicatedClass ? BPGDuplicatedClass->UberGraphFunction : nullptr;
		if (DuplicatedClassUberGraphFunction)
		{
			DuplicatedClassUberGraphFunction->Bind();
			DuplicatedClassUberGraphFunction->StaticLink(true);
		}

		// Bind and link the duplicate class, so that it has the proper duplicate property offsets
		DuplicatedClass->Bind();
		DuplicatedClass->StaticLink(true);

		// Copy over the ComponentNametoDefaultObjectMap, which tells CopyPropertiesForUnrelatedObjects which components are instanced and which aren't

		// Temporarily suspend the undo buffer; we don't need to record the duplicated CDO until it is fully resolved
 		ITransaction* CurrentTransaction = GUndo;
 		GUndo = NULL;
		DuplicatedClass->ClassDefaultObject = GetClassCDODuplicate(ClassToReinstance->GetDefaultObject(), DuplicatedClass->GetDefaultObjectName());

		// Restore the undo buffer
		GUndo = CurrentTransaction;
		DuplicatedClass->ClassDefaultObject->SetFlags(RF_ClassDefaultObject);
		DuplicatedClass->ClassDefaultObject->SetClass(DuplicatedClass);

		// The CDO is fully duplicated and ready to be placed in the undo buffer
		DuplicatedClass->ClassDefaultObject->Modify();

		ClassToReinstance->GetDefaultObject()->SetClass(DuplicatedClass);
		ObjectsThatShouldUseOldStuff.Add(DuplicatedClass); //CDO of REINST_ class can be used as archetype

		if( !bIsBytecodeOnly )
		{
			TArray<UObject*> ObjectsToChange;
			const bool bIncludeDerivedClasses = false;
			GetObjectsOfClass(ClassToReinstance, ObjectsToChange, bIncludeDerivedClasses);
			for (auto ObjIt = ObjectsToChange.CreateConstIterator(); ObjIt; ++ObjIt)
			{
				(*ObjIt)->SetClass(DuplicatedClass);
			}

			TArray<UClass*> ChildrenOfClass;
			GetDerivedClasses(ClassToReinstance, ChildrenOfClass);
			for ( auto ClassIt = ChildrenOfClass.CreateConstIterator(); ClassIt; ++ClassIt )
			{
				UClass* ChildClass = *ClassIt;
				UBlueprint* ChildBP = Cast<UBlueprint>(ChildClass->ClassGeneratedBy);
				if (ChildBP)
				{
					const bool bClassIsDirectlyGeneratedByTheBlueprint = (ChildBP->GeneratedClass == ChildClass)
						|| (ChildBP->SkeletonGeneratedClass == ChildClass);

					if (ChildBP->HasAnyFlags(RF_BeingRegenerated) || !bClassIsDirectlyGeneratedByTheBlueprint)
					{
						if (ChildClass->GetSuperClass() == ClassToReinstance)
						{
							ReparentChild(ChildClass);
						}

						//TODO: some stronger condition would be nice
						if (!bClassIsDirectlyGeneratedByTheBlueprint)
						{
							ObjectsThatShouldUseOldStuff.Add(ChildClass);
						}
					}
					// If this is a direct child, change the parent and relink so the property chain is valid for reinstancing
					else if( !ChildBP->HasAnyFlags(RF_NeedLoad) )
					{
						if( ChildClass->GetSuperClass() == ClassToReinstance )
						{
							ReparentChild(ChildBP);
						}

						Children.AddUnique(ChildBP);
					}
					else
					{
						// If this is a child that caused the load of their parent, relink to the REINST class so that we can still serialize in the CDO, but do not add to later processing
						ReparentChild(ChildClass);
					}
				}
			}
		}

		// Pull the blueprint that generated this reinstance target, and gather the blueprints that are dependent on it
		UBlueprint* GeneratingBP = Cast<UBlueprint>(ClassToReinstance->ClassGeneratedBy);
		check(GeneratingBP || GIsAutomationTesting);
		if(!IsReinstancingSkeleton() && GeneratingBP)
		{
			ClassToReinstanceDefaultValuesCRC = GeneratingBP->CrcLastCompiledCDO;
			Dependencies.Empty();
			FBlueprintEditorUtils::GetDependentBlueprints(GeneratingBP, Dependencies);

			// Never queue for saving when regenerating on load
			if (!GeneratingBP->bIsRegeneratingOnLoad && !IsReinstancingSkeleton())
			{
				bool const bIsLevelPackage = (UWorld::FindWorldInPackage(GeneratingBP->GetOutermost()) != nullptr);
				// we don't want to save the entire level (especially if this 
				// compile was already kicked off as a result of a level save, as it
				// could cause a recursive save)... let the "SaveOnCompile" setting 
				// only save blueprint assets
				if (!bIsLevelPackage)
				{
					CompiledBlueprintsToSave.Add(GeneratingBP);
				}
			}
		}
	}
}

void FBlueprintCompileReinstancer::SaveClassFieldMapping(UClass* InClassToReinstance)
{
	check(InClassToReinstance);

	for (UProperty* Prop = InClassToReinstance->PropertyLink; Prop && (Prop->GetOuter() == InClassToReinstance); Prop = Prop->PropertyLinkNext)
	{
		PropertyMap.Add(Prop->GetFName(), Prop);
	}

	for (auto Function : TFieldRange<UFunction>(InClassToReinstance, EFieldIteratorFlags::ExcludeSuper))
	{
		FunctionMap.Add(Function->GetFName(),Function);
	}
}

void FBlueprintCompileReinstancer::GenerateFieldMappings(TMap<UObject*, UObject*>& FieldMapping)
{
	check(ClassToReinstance);

	FieldMapping.Empty();

	for (auto& Prop : PropertyMap)
	{
		FieldMapping.Add(Prop.Value, FindField<UProperty>(ClassToReinstance, *Prop.Key.ToString()));
	}

	for (auto& Func : FunctionMap)
	{
		UFunction* NewFunction = ClassToReinstance->FindFunctionByName(Func.Key, EIncludeSuperFlag::ExcludeSuper);
		FieldMapping.Add(Func.Value, NewFunction);
	}

	UObject* NewCDO = ClassToReinstance->GetDefaultObject();
	FieldMapping.Add(OriginalCDO, NewCDO);
}

void FBlueprintCompileReinstancer::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AllowEliminatingReferences(false);
	Collector.AddReferencedObject(OriginalCDO);
	Collector.AddReferencedObject(DuplicatedClass);
	Collector.AllowEliminatingReferences(true);

	// it's ok for these to get GC'd, but it is not ok for the memory to be reused (after a GC), 
	// for that reason we cannot allow these to be freed during the life of this reinstancer
	// 
	// for example, we saw this as a problem in UpdateBytecodeReferences() - if the GC'd function 
	// memory was used for a new (unrelated) function, then we were replacing references to the 
	// new function (bad), as well as any old stale references (both were using the same memory address)
	Collector.AddReferencedObjects(FunctionMap);
	Collector.AddReferencedObjects(PropertyMap);
}

void FBlueprintCompileReinstancer::OptionallyRefreshNodes(UBlueprint* CurrentBP)
{
	if (HotReloadedNewClass)
	{
		UPackage* const Package = Cast<UPackage>(CurrentBP->GetOutermost());
		const bool bStartedWithUnsavedChanges = Package != nullptr ? Package->IsDirty() : true;

		FBlueprintEditorUtils::RefreshExternalBlueprintDependencyNodes(CurrentBP, HotReloadedNewClass);

		if (Package != nullptr && Package->IsDirty() && !bStartedWithUnsavedChanges)
		{
			Package->SetDirtyFlag(false);
		}
	}
}

FBlueprintCompileReinstancer::~FBlueprintCompileReinstancer()
{
	if (bIsRootReinstancer && bAllowResaveAtTheEndIfRequested)
	{
		if (CompiledBlueprintsToSave.Num() > 0)
		{
			if ( !IsRunningCommandlet() && !GIsAutomationTesting )
			{
				TArray<UPackage*> PackagesToSave;
				for (TWeakObjectPtr<UBlueprint> BPPtr : CompiledBlueprintsToSave)
				{
					if (BPPtr.IsValid())
					{
						UBlueprint* BP = BPPtr.Get();

						UBlueprintEditorSettings* Settings = GetMutableDefault<UBlueprintEditorSettings>();
						const bool bShouldSaveOnCompile = ((Settings->SaveOnCompile == SoC_Always) || ((Settings->SaveOnCompile == SoC_SuccessOnly) && (BP->Status == BS_UpToDate)));

						if (bShouldSaveOnCompile)
						{
							PackagesToSave.Add(BP->GetOutermost());
						}
					}
				}

				FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, /*bCheckDirty =*/true, /*bPromptToSave =*/false);
			}
			CompiledBlueprintsToSave.Empty();		
		}
	}
}

class FReinstanceFinalizer : public TSharedFromThis<FReinstanceFinalizer>
{
public:
	TSharedPtr<FBlueprintCompileReinstancer> Reinstancer;
	TArray<UObject*> ObjectsToReplace;
	TArray<UObject*> ObjectsToFinalize;
	TSet<UObject*> SelectedObjecs;
	UClass* ClassToReinstance;

	FReinstanceFinalizer(UClass* InClassToReinstance) : ClassToReinstance(InClassToReinstance)
	{
		check(ClassToReinstance);
	}

	void Finalize()
	{
		if (!ensure(Reinstancer.IsValid()))
		{
			return;
		}
		check(ClassToReinstance);

		const bool bIsActor = ClassToReinstance->IsChildOf<AActor>();
		if (bIsActor)
		{
			for (auto Obj : ObjectsToFinalize)
			{
				auto Actor = CastChecked<AActor>(Obj);

				UWorld* World = Actor->GetWorld();
				if (World)
				{
					// Remove any pending latent actions, as the compiled script code may have changed, and thus the
					// cached LinkInfo data may now be invalid. This could happen in the fast path, since the original
					// Actor instance will not be replaced in that case, and thus might still have latent actions pending.
					World->GetLatentActionManager().RemoveActionsForObject(Actor);

					Actor->ReregisterAllComponents();
					Actor->RerunConstructionScripts();

					if (SelectedObjecs.Contains(Obj))
					{
						GEditor->SelectActor(Actor, /*bInSelected =*/true, /*bNotify =*/true, false, true);
					}
				}
			}
		}

		const bool bIsAnimInstance = ClassToReinstance->IsChildOf<UAnimInstance>();
		//UAnimBlueprintGeneratedClass* AnimClass = Cast<UAnimBlueprintGeneratedClass>(ClassToReinstance);
		if(bIsAnimInstance)
		{
			for(auto Obj : ObjectsToFinalize)
			{
				if(USkeletalMeshComponent* SkelComponent = Cast<USkeletalMeshComponent>(Obj->GetOuter()))
				{
					// This snippet catches all of the exposed value handlers that will have invalid UFunctions
					// and clears the init flag so they will be reinitialized on the next call to InitAnim.
					// Unknown whether there are other unreachable properties so currently clearing the anim
					// instance below
					// #TODO investigate reinstancing anim blueprints to correctly catch all deep references

					//UAnimInstance* ActiveInstance = SkelComponent->GetAnimInstance();
					//if(AnimClass && ActiveInstance)
					//{
					//	for(UStructProperty* NodeProp : AnimClass->AnimNodeProperties)
					//	{
					//		// Guaranteed to have only FAnimNode_Base pointers added during compilation
					//		FAnimNode_Base* AnimNode = NodeProp->ContainerPtrToValuePtr<FAnimNode_Base>(ActiveInstance);
					//
					//		AnimNode->EvaluateGraphExposedInputs.bInitialized = false;
					//	}
					//}

					// Clear out the script instance on the component to force a rebuild during initialization.
					// This is necessary to correctly reinitialize certain properties that still reference the 
					// old class as they are unreachable during reinstancing.
					SkelComponent->AnimScriptInstance = nullptr;
					SkelComponent->InitAnim(true);
				}
			}
		}

		Reinstancer->FinalizeFastReinstancing(ObjectsToReplace);
	}
};

TSharedPtr<FReinstanceFinalizer> FBlueprintCompileReinstancer::ReinstanceFast()
{
	UE_LOG(LogBlueprint, Log, TEXT("BlueprintCompileReinstancer: Doing a fast path refresh on class '%s'."), *GetPathNameSafe(ClassToReinstance));

	TSharedPtr<FReinstanceFinalizer> Finalizer = MakeShareable(new FReinstanceFinalizer(ClassToReinstance));
	Finalizer->Reinstancer = SharedThis(this);

	GetObjectsOfClass(DuplicatedClass, Finalizer->ObjectsToReplace, /*bIncludeDerivedClasses=*/ false);

	const bool bIsActor = ClassToReinstance->IsChildOf<AActor>();
	const bool bIsAnimInstance = ClassToReinstance->IsChildOf<UAnimInstance>();
	const bool bIsComponent = ClassToReinstance->IsChildOf<UActorComponent>();
	for (auto Obj : Finalizer->ObjectsToReplace)
	{
		UE_LOG(LogBlueprint, Log, TEXT("  Fast path is refreshing (not replacing) %s"), *Obj->GetFullName());

		if ((!Obj->IsTemplate() || bIsComponent) && !Obj->IsPendingKill())
		{
			if (bIsActor && Obj->IsSelected())
			{
				Finalizer->SelectedObjecs.Add(Obj);
			}

			Obj->SetClass(ClassToReinstance);

			Finalizer->ObjectsToFinalize.Push(Obj);
		}
	}

	return Finalizer;
}

void FBlueprintCompileReinstancer::FinalizeFastReinstancing(TArray<UObject*>& ObjectsToReplace)
{
	TArray<UObject*> SourceObjects;
	TMap<UObject*, UObject*> OldToNewInstanceMap;
	TMap<FStringAssetReference, UObject*> ReinstancedObjectsWeakReferenceMap;
	FReplaceReferenceHelper::IncludeCDO(DuplicatedClass, ClassToReinstance, OldToNewInstanceMap, SourceObjects, OriginalCDO);

	if (IsClassObjectReplaced())
	{
		FReplaceReferenceHelper::IncludeClass(DuplicatedClass, ClassToReinstance, OldToNewInstanceMap, SourceObjects, ObjectsToReplace);
	}

	FReplaceReferenceHelper::FindAndReplaceReferences(SourceObjects, &ObjectsThatShouldUseOldStuff, ObjectsToReplace, OldToNewInstanceMap, ReinstancedObjectsWeakReferenceMap);

	if (ClassToReinstance->IsChildOf<UActorComponent>())
	{
		// ReplaceInstancesOfClass() handles this itself, if we had to re-instance
		ReconstructOwnerInstances(ClassToReinstance);
	}
}

void FBlueprintCompileReinstancer::CompileChildren()
{
	BP_SCOPED_COMPILER_EVENT_STAT(EKismetReinstancerStats_RecompileChildClasses);

	// Reparent all dependent blueprints, and recompile to ensure that they get reinstanced with the new memory layout
	for (auto ChildBP = Children.CreateIterator(); ChildBP; ++ChildBP)
	{
		UBlueprint* BP = *ChildBP;
		if (BP->ParentClass == ClassToReinstance || BP->ParentClass == DuplicatedClass)
		{
			ReparentChild(BP);

			// avoid the skeleton compile if we don't need it - if the class 
			// we're reinstancing is a Blueprint class, then we assume sub-class
			// skeletons were kept in-sync (updated/reinstanced when the parent 
			// was updated); however, if this is a native class (like when hot-
			// reloading), then we want to make sure to update the skel as well
			const bool bSkeletonUpToDate = !ClassToReinstance->HasAnyClassFlags(CLASS_Native);
			FKismetEditorUtilities::CompileBlueprint(BP, false, bSkipGarbageCollection, false, nullptr, bSkeletonUpToDate, false);
		}
		else if (IsReinstancingSkeleton())
		{
			const bool bForceRegeneration = true;
			FKismetEditorUtilities::GenerateBlueprintSkeleton(BP, bForceRegeneration);
		}
	}
}

TSharedPtr<FReinstanceFinalizer> FBlueprintCompileReinstancer::ReinstanceInner(bool bForceAlwaysReinstance)
{
	TSharedPtr<FReinstanceFinalizer> Finalizer;
	if (ClassToReinstance && DuplicatedClass)
	{
		static const FBoolConfigValueHelper ReinstanceOnlyWhenNecessary(TEXT("Kismet"), TEXT("bReinstanceOnlyWhenNecessary"), GEngineIni);
		bool bShouldReinstance = true;
		// See if we need to do a full reinstance or can do the faster refresh path (when enabled or no values were modified, and the structures match)
		if (ReinstanceOnlyWhenNecessary && !bForceAlwaysReinstance)
		{
			BP_SCOPED_COMPILER_EVENT_STAT(EKismetReinstancerStats_ReplaceClassNoReinsancing);

			const UBlueprintGeneratedClass* BPClassA = Cast<const UBlueprintGeneratedClass>(DuplicatedClass);
			const UBlueprintGeneratedClass* BPClassB = Cast<const UBlueprintGeneratedClass>(ClassToReinstance);
			const UBlueprint* BP = Cast<const UBlueprint>(ClassToReinstance->ClassGeneratedBy);

			const bool bTheSameDefaultValues = (BP != nullptr) && (ClassToReinstanceDefaultValuesCRC != 0) && (BP->CrcLastCompiledCDO == ClassToReinstanceDefaultValuesCRC);
			const bool bTheSameLayout = (BPClassA != nullptr) && (BPClassB != nullptr) && FStructUtils::TheSameLayout(BPClassA, BPClassB, true);
			const bool bAllowedToDoFastPath = bTheSameDefaultValues && bTheSameLayout;
			if (bAllowedToDoFastPath)
			{
				Finalizer = ReinstanceFast();
				bShouldReinstance = false;
			}
		}

		if (bShouldReinstance)
		{
			UE_LOG(LogBlueprint, Log, TEXT("BlueprintCompileReinstancer: Doing a full reinstance on class '%s'"), *GetPathNameSafe(ClassToReinstance));
			ReplaceInstancesOfClass(DuplicatedClass, ClassToReinstance, OriginalCDO, &ObjectsThatShouldUseOldStuff, IsClassObjectReplaced(), ShouldPreserveRootComponentOfReinstancedActor());
		}
	}
	return Finalizer;
}

void FBlueprintCompileReinstancer::ListDependentBlueprintsToRefresh(const TArray<UBlueprint*>& DependentBPs)
{
	for (auto& Element : DependentBPs)
	{
		DependentBlueprintsToRefresh.Add(Element);
	}
}

void FBlueprintCompileReinstancer::EnlistDependentBlueprintToRecompile(UBlueprint* BP, bool bBytecodeOnly)
{
	if (IsValid(BP))
	{
		if (bBytecodeOnly)
		{
			DependentBlueprintsToByteRecompile.Add(BP);
		}
		else
		{
			DependentBlueprintsToRecompile.Add(BP);
		}
	}
}

void FBlueprintCompileReinstancer::BlueprintWasRecompiled(UBlueprint* BP, bool bBytecodeOnly)
{
	if (IsValid(BP))
	{
		DependentBlueprintsToRefresh.Remove(BP);
		DependentBlueprintsToByteRecompile.Remove(BP);
		if (!bBytecodeOnly)
		{
			DependentBlueprintsToRecompile.Remove(BP);
		}
	}
}

extern UNREALED_API FSecondsCounterData BlueprintCompileAndLoadTimerData;

void FBlueprintCompileReinstancer::ReinstanceObjects(bool bForceAlwaysReinstance)
{
	FSecondsCounterScope Timer(BlueprintCompileAndLoadTimerData);

	BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_ReinstanceObjects);
	
	// Make sure we only reinstance classes once!
	static TArray<TSharedRef<FBlueprintCompileReinstancer>> QueueToReinstance;
	TSharedRef<FBlueprintCompileReinstancer> SharedThis = AsShared();
	bool bAlreadyQueued = QueueToReinstance.Contains(SharedThis);

	// We may already be reinstancing this class, this happens when a dependent blueprint has a compile error and we try to reinstance the stub:
	for (const auto& Entry : QueueToReinstance)
	{
		if (Entry->ClassToReinstance == SharedThis->ClassToReinstance)
		{
			bAlreadyQueued = true;
		}
	}

	if (!bAlreadyQueued && !bHasReinstanced)
	{
		QueueToReinstance.Push(SharedThis);

		if (ClassToReinstance && DuplicatedClass)
		{
			CompileChildren();
		}

		if (QueueToReinstance.Num() && (QueueToReinstance[0] == SharedThis))
		{
			// Mark it as the source reinstancer, no other reinstancer can get here until this Blueprint finishes compiling
			bIsRootReinstancer = true;

			TSet<TWeakObjectPtr<UBlueprint>> CompiledBlueprints;
			// Blueprints will enqueue dirty and erroring dependents, in case those states would be 
			// fixed up by having this dependency compiled first. However, this can result in an 
			// infinite loop where two Blueprints with errors (unrelated to each other) keep 
			// perpetually queuing the other. 
			//
			// To guard against this, we track the recompiled dependents (in order) and break the 
			// cycle when we see that we've already compiled a dependent after its dependency
			TArray<UBlueprint*> OrderedRecompiledDependents;

			TSet<TWeakObjectPtr<UBlueprint>> RecompilationQueue = DependentBlueprintsToRecompile;
			// empty the public facing queue so we can discern between old and new elements (added 
			// as the result of subsequent recompiles) 
			DependentBlueprintsToRecompile.Empty();

			while (RecompilationQueue.Num()) 
			{
				auto Iter = RecompilationQueue.CreateIterator();
				TWeakObjectPtr<UBlueprint> BPPtr = *Iter;
				Iter.RemoveCurrent();
				if (UBlueprint* BP = BPPtr.Get())
				{
					if (IsReinstancingSkeleton())
					{
						const bool bForceRegeneration = true;
						FKismetEditorUtilities::GenerateBlueprintSkeleton(BP, bForceRegeneration);
					}
					else
					{
						// it's unsafe to GC in the middle of reinstancing because there may be other reinstancers still alive with references to 
						// otherwise unreferenced classes:
						const bool bSkipGC = true;
						// Full compiles first recompile all skeleton classes, so they are up-to-date
						const bool bSkeletonUpToDate = true;
						FKismetEditorUtilities::CompileBlueprint(BP, false, bSkipGC, false, nullptr, bSkeletonUpToDate, true);
						CompiledBlueprints.Add(BP);
					}

					OrderedRecompiledDependents.Add(BP);

					// if this BP compiled with an error, then I don't see any reason why we 
					// should attempt to recompile its dependencies; if a subsequent recompile 
					// would fix this up, then it'll get re-injected into the queue when that happens
					if (BP->Status != EBlueprintStatus::BS_Error)
					{
						for (TWeakObjectPtr<UBlueprint>& DependentPtr : DependentBlueprintsToRecompile)
						{
							if (!DependentPtr.IsValid())
							{
								continue;
							}
							UBlueprint* NewDependent = DependentPtr.Get();

							int32 DependentIndex = OrderedRecompiledDependents.FindLast(NewDependent);
							if (DependentIndex != INDEX_NONE)
							{
								// even though we just pushed BP into the list and know that it
								// exists as the last entry, we want to see if it was compiled 
								// earlier (once before 'NewDependent'); so we use Find() to search 
								// out the first entry
								int32 RecompilingBpIndex = OrderedRecompiledDependents.Find(BP);
								if (RecompilingBpIndex != INDEX_NONE && RecompilingBpIndex < DependentIndex)
								{
									// we've already recompiled this Blueprint once before (here in 
									// this loop), already after its dependency has been compiled too;
									// so, to avoid a potential infinite loop we cannot keep trying 
									// to compile this
									//
									// NOTE: this may result in some a compiler error that would have 
									//       been resolved in another subsequent compile (for example: 
									//       B depends on A, A is compiled, A has an error, B compiles 
									//       with an error as a result, C compiles and enqueues A as a 
									//       dependent, A is recompiled without error now, B is not 
									//       enqueued again because its already recompiled after A)
									// 
									// the true fix is to restructure the compiler so that these sort 
									// of scenarios don't happen - until then, this is a fair trade 
									// off... fallback to a byte code compile instead
									DependentBlueprintsToByteRecompile.Add(DependentPtr);
									continue;
								}
							}
							RecompilationQueue.Add(DependentPtr);
						}
					}
					DependentBlueprintsToRecompile.Empty();
				}
			}

			TArray<UBlueprint*> OrderedBytecodeRecompile;

			while (DependentBlueprintsToByteRecompile.Num())
			{
				auto Iter = DependentBlueprintsToByteRecompile.CreateIterator();
				if (UBlueprint* BP = Iter->Get())
				{
					OrderedBytecodeRecompile.Add(BP);
				}
				Iter.RemoveCurrent();
			}

			// Make sure we compile classes that are deeper in the class hierarchy later
			// than ones that are higher:
			OrderedBytecodeRecompile.Sort(
				[](const UBlueprint& LHS, const UBlueprint& RHS)
				{
					int32 LHS_Depth = 0;
					int32 RHS_Depth = 0;

					UStruct* Iter = LHS.ParentClass;
					while (Iter)
					{
						LHS_Depth += 1;
						Iter = Iter->GetSuperStruct();
					}

					Iter = RHS.ParentClass;
					while (Iter)
					{
						RHS_Depth += 1;
						Iter = Iter->GetSuperStruct();
					}

					// use name as tie breaker, just so we're stable
					// across editor sessions:
					return LHS_Depth != RHS_Depth ? (LHS_Depth < RHS_Depth) : LHS.GetName() < RHS.GetName();
				}
			);

			DependentBlueprintsToByteRecompile.Empty();

			for (int I = 0; I != OrderedBytecodeRecompile.Num(); ++I)
			{
				UBlueprint* BP = OrderedBytecodeRecompile[I];
				FKismetEditorUtilities::RecompileBlueprintBytecode(BP, nullptr, true);
				ensure(0 == DependentBlueprintsToRecompile.Num());
				CompiledBlueprints.Add(BP);
			}


			if (!IsReinstancingSkeleton())
			{
				TGuardValue<bool> ReinstancingGuard(GIsReinstancing, true);

				TArray<TSharedPtr<FReinstanceFinalizer>> Finalizers;

				// All children were recompiled. It's safe to reinstance.
				for (int32 Idx = 0; Idx < QueueToReinstance.Num(); ++Idx)
				{
					auto Finalizer = QueueToReinstance[Idx]->ReinstanceInner(bForceAlwaysReinstance);
					if (Finalizer.IsValid())
					{
						Finalizers.Push(Finalizer);
					}
					QueueToReinstance[Idx]->bHasReinstanced = true;
				}
				QueueToReinstance.Empty();

				for (auto Finalizer : Finalizers)
				{
					if (Finalizer.IsValid())
					{
						Finalizer->Finalize();
					}
				}

				for (auto CompiledBP : CompiledBlueprints)
				{
					CompiledBP->BroadcastCompiled();
				}

				{
					BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_RefreshDependentBlueprintsInReinstancer);
					for (auto BPPtr : DependentBlueprintsToRefresh)
					{
						if (BPPtr.IsValid())
						{
							BPPtr->BroadcastChanged();
						}
					}
					DependentBlueprintsToRefresh.Empty();
				}

				if (GEditor)
				{
					GEditor->BroadcastBlueprintCompiled();
				}
			}
			else
			{
				QueueToReinstance.Empty();
				DependentBlueprintsToRefresh.Empty();
			}
		}
	}
}



void FBlueprintCompileReinstancer::UpdateBytecodeReferences()
{
	BP_SCOPED_COMPILER_EVENT_STAT(EKismetReinstancerStats_UpdateBytecodeReferences);

	if(ClassToReinstance != NULL)
	{
		TMap<UObject*, UObject*> FieldMappings;
		GenerateFieldMappings(FieldMappings);

		// Determine whether or not we will be updating references for an Animation Blueprint class.
		const bool bIsAnimBlueprintClass = !!Cast<UAnimBlueprint>(ClassToReinstance->ClassGeneratedBy);

		for( auto DependentBP = Dependencies.CreateIterator(); DependentBP; ++DependentBP )
		{
			UClass* BPClass = (*DependentBP)->GeneratedClass;

			// Skip cases where the class is junk, or haven't finished serializing in yet
			// Note that BPClass can be null for blueprints that can no longer be compiled:
			if (!BPClass
				|| (BPClass == ClassToReinstance)
				|| (BPClass->GetOutermost() == GetTransientPackage()) 
				|| BPClass->HasAnyClassFlags(CLASS_NewerVersionExists)
				|| (BPClass->ClassGeneratedBy && BPClass->ClassGeneratedBy->HasAnyFlags(RF_NeedLoad|RF_BeingRegenerated)) )
			{
				continue;
			}

			BPClass->ClearFunctionMapsCaches();

			// Ensure that Animation Blueprint child class dependencies are always re-linked, as the child may reference properties generated during
			// compilation of the parent class, which will have shifted to a TRASHCLASS Outer at this point (see UAnimBlueprintGeneratedClass::Link()).
			if(bIsAnimBlueprintClass && BPClass->IsChildOf(ClassToReinstance))
			{
				BPClass->StaticLink(true);
			}

			bool bBPWasChanged = false;
			// For each function defined in this blueprint, run through the bytecode, and update any refs from the old properties to the new
			for( TFieldIterator<UFunction> FuncIter(BPClass, EFieldIteratorFlags::ExcludeSuper); FuncIter; ++FuncIter )
			{
				UFunction* CurrentFunction = *FuncIter;
				if( CurrentFunction->Script.Num() > 0 )
				{
					FArchiveReplaceObjectRef<UObject> ReplaceAr(CurrentFunction, FieldMappings, /*bNullPrivateRefs=*/ false, /*bIgnoreOuterRef=*/ true, /*bIgnoreArchetypeRef=*/ true);
					bBPWasChanged |= (0 != ReplaceAr.GetCount());
				}
			}

			FArchiveReplaceObjectRef<UObject> ReplaceInBPAr(*DependentBP, FieldMappings, false, true, true);
			if (ReplaceInBPAr.GetCount())
			{
				bBPWasChanged = true;
				UE_LOG(LogBlueprint, Log, TEXT("UpdateBytecodeReferences: %d references from %s was replaced in BP %s"), ReplaceInBPAr.GetCount(), *GetPathNameSafe(ClassToReinstance), *GetPathNameSafe(*DependentBP));
			}

			auto CompiledBlueprint = UBlueprint::GetBlueprintFromClass(ClassToReinstance);
			if (bBPWasChanged && CompiledBlueprint && !CompiledBlueprint->bIsRegeneratingOnLoad)
			{
				DependentBlueprintsToRefresh.Add(*DependentBP);
			}
		}
	}
}

/** 
 * Utility struct that represents a single replacement actor. Used to cache off
 * attachment info for the old actor (the one being replaced), that will be
 * used later for the new actor (after all instances have been replaced).
 */
struct FActorReplacementHelper
{
	/** NOTE: this detaches OldActor from all child/parent attachments. */
	FActorReplacementHelper(AActor* InNewActor, AActor* OldActor)
		: NewActor(InNewActor)
		, TargetWorldTransform(FTransform::Identity)
		, TargetAttachParent(nullptr)
		, TargetParentComponent(nullptr)
		, bSelectNewActor(OldActor->IsSelected())
	{
		CachedActorData = StaticCastSharedPtr<AActor::FActorTransactionAnnotation>(OldActor->GetTransactionAnnotation());
		CacheAttachInfo(OldActor);

		for (UActorComponent* OldActorComponent : OldActor->GetComponents())
		{
			if (OldActorComponent)
			{
				OldActorComponentNameMap.Add(OldActorComponent->GetFName(), OldActorComponent);
			}
		}
	}

	/**
	 * Runs construction scripts on the new actor and then finishes it off by
	 * attaching it to the same attachments that its predecessor was set with. 
	 */
	void Finalize(const TMap<UObject*, UObject*>& OldToNewInstanceMap, TSet<UObject*>* ObjectsThatShouldUseOldStuff, const TArray<UObject*>& ObjectsToReplace, const TMap<FStringAssetReference, UObject*>& ReinstancedObjectsWeakReferenceMap);


private:
	/**
	 * Stores off the old actor's children, and its AttachParent; so that we can
	 * apply them to the new actor in Finalize().
	 *
	 * NOTE: this detaches OldActor from all child/parent attachments.
	 *
	 * @param  OldActor		The actor whose attachment setup you want reproduced.
	 */
	void CacheAttachInfo(const AActor* OldActor);

	/**
	 * Stores off the old actor's children; so that in Finalize(), we may 
	 * reattach them under the new actor.
	 *
	 * @param  OldActor		The actor whose child actors you want moved.
	 */
	void CacheChildAttachments(const AActor* OldActor);

	/**
	 * Takes the cached child actors, as well as the old AttachParent, and sets
	 * up the new actor so that its attachment hierarchy reflects the old actor
	 * that it is replacing.
	 *
	 * @param OldToNewInstanceMap Mapping of reinstanced objects.
	 */
	void ApplyAttachments(const TMap<UObject*, UObject*>& OldToNewInstanceMap);
	
	/**
	 * Takes the cached child actors, and attaches them under the new actor.
	 *
	 * @param  RootComponent	The new actor's root, which the child actors should attach to.
	 * @param  OldToNewInstanceMap	Mapping of reinstanced objects. Used for when child and parent actor are of the same type (and thus parent may have been reinstanced, so we can't reattach to the old instance).
	 */
	void AttachChildActors(USceneComponent* RootComponent, const TMap<UObject*, UObject*>& OldToNewInstanceMap);

private:
	AActor*          NewActor;
	FTransform       TargetWorldTransform;
	AActor*          TargetAttachParent;
	USceneComponent* TargetParentComponent; // should belong to TargetAttachParent
	FName            TargetAttachSocket;
	bool             bSelectNewActor;

	/** Info store about attached actors */
	struct FAttachedActorInfo
	{
		AActor* AttachedActor;
		FName   AttachedToSocket;
	};
	TArray<FAttachedActorInfo> PendingChildAttachments;

	/** Holds actor component data, etc. that we use to apply */
	TSharedPtr<AActor::FActorTransactionAnnotation> CachedActorData;

	TMap<FName, UActorComponent*> OldActorComponentNameMap;
};

void FActorReplacementHelper::Finalize(const TMap<UObject*, UObject*>& OldToNewInstanceMap, TSet<UObject*>* ObjectsThatShouldUseOldStuff, const TArray<UObject*>& ObjectsToReplace, const TMap<FStringAssetReference, UObject*>& ReinstancedObjectsWeakReferenceMap)
{
	
	// because this is an editor context it's important to use this execution guard
	FEditorScriptExecutionGuard ScriptGuard;

	// run the construction script, which will use the properties we just copied over
	if (NewActor->CurrentTransactionAnnotation.IsValid())
	{
		NewActor->CurrentTransactionAnnotation->ComponentInstanceData.FindAndReplaceInstances(OldToNewInstanceMap);
		NewActor->RerunConstructionScripts();
	}
	else if (CachedActorData.IsValid())
	{
		CachedActorData->ComponentInstanceData.FindAndReplaceInstances(OldToNewInstanceMap);
		const bool bErrorFree = NewActor->ExecuteConstruction(TargetWorldTransform, &CachedActorData->ComponentInstanceData);
		if (!bErrorFree)
		{
			// Save off the cached actor data for once the blueprint has been fixed so we can reapply it
			NewActor->CurrentTransactionAnnotation = CachedActorData;
		}
	}
	else
	{
		FComponentInstanceDataCache DummyComponentData;
		NewActor->ExecuteConstruction(TargetWorldTransform, &DummyComponentData);
	}	

	// make sure that the actor is properly hidden if it's in a hidden sublevel:
	bool bIsInHiddenLevel = false;
	if (ULevel* Level = NewActor->GetLevel())
	{
		bIsInHiddenLevel = !Level->bIsVisible;
	}

	if (bIsInHiddenLevel)
	{
		NewActor->bHiddenEdLevel = true;
		NewActor->MarkComponentsRenderStateDirty();
	}

	if (TargetAttachParent)
	{
		UObject* const* NewTargetAttachParent = OldToNewInstanceMap.Find(TargetAttachParent);
		if (NewTargetAttachParent)
		{
			TargetAttachParent = CastChecked<AActor>(*NewTargetAttachParent);
		}
	}
	if (TargetParentComponent)
	{
		UObject* const* NewTargetParentComponent = OldToNewInstanceMap.Find(TargetParentComponent);
		if (NewTargetParentComponent && *NewTargetParentComponent)
		{
			TargetParentComponent = CastChecked<USceneComponent>(*NewTargetParentComponent);
		}
	}

	ApplyAttachments(OldToNewInstanceMap);

	if (bSelectNewActor)
	{
		GEditor->SelectActor(NewActor, /*bInSelected =*/true, /*bNotify =*/true);
	}

	TMap<UObject*, UObject*> ConstructedComponentReplacementMap;
	for (UActorComponent* NewActorComponent : NewActor->GetComponents())
	{
		if (NewActorComponent)
		{
			if (UActorComponent** OldActorComponent = OldActorComponentNameMap.Find(NewActorComponent->GetFName()))
			{
				ConstructedComponentReplacementMap.Add(*OldActorComponent, NewActorComponent);
			}
		}
	}
	GEditor->NotifyToolsOfObjectReplacement(ConstructedComponentReplacementMap);

	// Make array of component subobjects that have been reinstanced as part of the new Actor.
	TArray<UObject*> SourceObjects;
	ConstructedComponentReplacementMap.GenerateKeyArray(SourceObjects);

	// Find and replace any outstanding references to the old Actor's component subobject instances that exist outside of the old Actor instance.
	// Note: This will typically be references held by the Editor's transaction buffer - we need to find and replace those as well since we also do this for the old->new Actor instance.
	FReplaceReferenceHelper::FindAndReplaceReferences(SourceObjects, ObjectsThatShouldUseOldStuff, ObjectsToReplace, ConstructedComponentReplacementMap, ReinstancedObjectsWeakReferenceMap);
	
	// Destroy actor and clear references.
	NewActor->Modify();
	if (GEditor->Layers.IsValid()) // ensure(NULL != GEditor->Layers) ?? While cooking the Layers is NULL.
	{
		GEditor->Layers->InitializeNewActorLayers(NewActor);
	}
}

void FActorReplacementHelper::CacheAttachInfo(const AActor* OldActor)
{
	CacheChildAttachments(OldActor);

	if (USceneComponent* OldRootComponent = OldActor->GetRootComponent())
	{
		if (OldRootComponent->GetAttachParent() != nullptr)
		{
			TargetAttachParent = OldRootComponent->GetAttachParent()->GetOwner();
			// Root component should never be attached to another component in the same actor!
			if (TargetAttachParent == OldActor)
			{
				UE_LOG(LogBlueprint, Warning, TEXT("ReplaceInstancesOfClass: RootComponent (%s) attached to another component in this Actor (%s)."), *OldRootComponent->GetPathName(), *TargetAttachParent->GetPathName());
				TargetAttachParent = nullptr;
			}

			TargetAttachSocket    = OldRootComponent->GetAttachSocketName();
			TargetParentComponent = OldRootComponent->GetAttachParent();
			
			// detach it to remove any scaling
			OldRootComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		}

		// Save off transform
		TargetWorldTransform = OldRootComponent->ComponentToWorld;
		TargetWorldTransform.SetTranslation(OldRootComponent->GetComponentLocation()); // take into account any custom location
	}
}

void FActorReplacementHelper::CacheChildAttachments(const AActor* OldActor)
{
	TArray<AActor*> AttachedActors;
	OldActor->GetAttachedActors(AttachedActors);

	// if there are attached objects detach them and store the socket names
	for (AActor* AttachedActor : AttachedActors)
	{
		USceneComponent* AttachedActorRoot = AttachedActor->GetRootComponent();
		if (AttachedActorRoot && AttachedActorRoot->GetAttachParent())
		{
			// Save info about actor to reattach
			FAttachedActorInfo Info;
			Info.AttachedActor = AttachedActor;
			Info.AttachedToSocket = AttachedActorRoot->GetAttachSocketName();
			PendingChildAttachments.Add(Info);

			// Now detach it
			AttachedActorRoot->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		}
	}
}

void FActorReplacementHelper::ApplyAttachments(const TMap<UObject*, UObject*>& OldToNewInstanceMap)
{
	USceneComponent* NewRootComponent = NewActor->GetRootComponent();
	if (NewRootComponent == nullptr)
	{
		return;
	}
	// attach the new instance to original parent
	if (TargetAttachParent != nullptr)
	{
		if (TargetParentComponent == nullptr)
		{
			TargetParentComponent = TargetAttachParent->GetRootComponent();
		}
		else
		{
			NewRootComponent->AttachToComponent(TargetParentComponent, FAttachmentTransformRules::KeepWorldTransform, TargetAttachSocket);
		}
	}

	AttachChildActors(NewRootComponent, OldToNewInstanceMap);
}

void FActorReplacementHelper::AttachChildActors(USceneComponent* RootComponent, const TMap<UObject*, UObject*>& OldToNewInstanceMap)
{
	// if we had attached children reattach them now - unless they are already attached
	for (FAttachedActorInfo& Info : PendingChildAttachments)
	{
		// Check for a reinstanced attachment, and redirect to the new instance if found
		AActor* NewAttachedActor = Cast<AActor>(OldToNewInstanceMap.FindRef(Info.AttachedActor));
		if (NewAttachedActor)
		{
			Info.AttachedActor = NewAttachedActor;
		}

		// If this actor is no longer attached to anything, reattach
		if (!Info.AttachedActor->IsPendingKill() && Info.AttachedActor->GetAttachParentActor() == nullptr)
		{
			USceneComponent* ChildRoot = Info.AttachedActor->GetRootComponent();
			if (ChildRoot && ChildRoot->GetAttachParent() != RootComponent)
			{
				ChildRoot->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform, Info.AttachedToSocket);
				ChildRoot->UpdateComponentToWorld();
			}
		}
	}
}

// 
namespace InstancedPropertyUtils
{
	typedef TMap<FName, UObject*> FInstancedPropertyMap;

	/** 
	 * Aids in finding instanced property values that will not be duplicated nor
	 * copied in CopyPropertiesForUnRelatedObjects().
	 */
	class FArchiveInstancedSubObjCollector : public FArchiveUObject
	{
	public:
		//----------------------------------------------------------------------
		FArchiveInstancedSubObjCollector(UObject* TargetObj, FInstancedPropertyMap& PropertyMapOut, bool bAutoSerialize = true)
			: Target(TargetObj)
			, InstancedPropertyMap(PropertyMapOut)
		{
			ArIsObjectReferenceCollector = true;
			ArIsPersistent = false;
			ArIgnoreArchetypeRef = false;

			if (bAutoSerialize)
			{
				RunSerialization();
			}
		}

		//----------------------------------------------------------------------
		FArchive& operator<<(UObject*& Obj)
		{
			if (Obj != nullptr)
			{
				UProperty* SerializingProperty = GetSerializedProperty();
				const bool bHasInstancedValue = SerializingProperty->HasAnyPropertyFlags(CPF_PersistentInstance);

				// default sub-objects are handled by CopyPropertiesForUnrelatedObjects()
				if (bHasInstancedValue && !Obj->IsDefaultSubobject())
				{
					
					UObject* ObjOuter = Obj->GetOuter();
					bool bIsSubObject = (ObjOuter == Target);
					// @TODO: handle nested sub-objects when we're more clear on 
					//        how this'll affect the makeup of the reinstanced object
// 					while (!bIsSubObject && (ObjOuter != nullptr))
// 					{
// 						ObjOuter = ObjOuter->GetOuter();
// 						bIsSubObject |= (ObjOuter == Target);
// 					}

					if (bIsSubObject)
					{
						InstancedPropertyMap.Add(SerializingProperty->GetFName(), Obj);
					}
				}
			}
			return *this;
		}

		//----------------------------------------------------------------------
		void RunSerialization()
		{
			InstancedPropertyMap.Empty();
			if (Target != nullptr)
			{
				Target->Serialize(*this);
			}
		}

	private:
		UObject* Target;
		FInstancedPropertyMap& InstancedPropertyMap;
	};

	/** 
	 * Duplicates and assigns instanced property values that may have been 
	 * missed by CopyPropertiesForUnRelatedObjects().
	 */
	class FArchiveInsertInstancedSubObjects : public FArchiveUObject
	{
	public:
		//----------------------------------------------------------------------
		FArchiveInsertInstancedSubObjects(UObject* TargetObj, const FInstancedPropertyMap& OldInstancedSubObjs, bool bAutoSerialize = true)
			: TargetCDO(TargetObj->GetClass()->GetDefaultObject())
			, Target(TargetObj)
			, OldInstancedSubObjects(OldInstancedSubObjs)
		{
			ArIsObjectReferenceCollector = true;
			ArIsModifyingWeakAndStrongReferences = true;

			if (bAutoSerialize)
			{
				RunSerialization();
			}
		}

		//----------------------------------------------------------------------
		FArchive& operator<<(UObject*& Obj)
		{
			if (Obj == nullptr)
			{
				UProperty* SerializingProperty = GetSerializedProperty();
				if (UObject* const* OldInstancedObjPtr = OldInstancedSubObjects.Find(SerializingProperty->GetFName()))
				{
					const UObject* OldInstancedObj = *OldInstancedObjPtr;
					check(SerializingProperty->HasAnyPropertyFlags(CPF_PersistentInstance));

					UClass* TargetClass = TargetCDO->GetClass();
					// @TODO: Handle nested instances when we have more time to flush this all out  
					if ( TargetClass->IsChildOf(SerializingProperty->GetOwnerClass()) )
					{
						UObjectPropertyBase* SerializingObjProperty = CastChecked<UObjectPropertyBase>(SerializingProperty);
						// being extra careful, not to create our own instanced version when we expect one from the CDO
						if (SerializingObjProperty->GetObjectPropertyValue_InContainer(TargetCDO) == nullptr)
						{
							// @TODO: What if the instanced object is of the same type 
							//        that we're currently reinstancing
							Obj = StaticDuplicateObject(OldInstancedObj, Target);// NewObject<UObject>(Target, OldInstancedObj->GetClass()->GetAuthoritativeClass(), OldInstancedObj->GetFName());
						}
					}
				}
			}
			return *this;
		}

		//----------------------------------------------------------------------
		void RunSerialization()
		{
			if ((Target != nullptr) && (OldInstancedSubObjects.Num() != 0))
			{
				Target->Serialize(*this);
			}
		}

	private:
		UObject* TargetCDO;
		UObject* Target;
		const FInstancedPropertyMap& OldInstancedSubObjects;
	};
}

void FBlueprintCompileReinstancer::ReplaceInstancesOfClass(UClass* OldClass, UClass* NewClass, UObject*	OriginalCDO, TSet<UObject*>* ObjectsThatShouldUseOldStuff, bool bClassObjectReplaced, bool bPreserveRootComponent)
{
	TMap<UClass*, UClass*> OldToNewClassMap;
	OldToNewClassMap.Add(OldClass, NewClass);
	ReplaceInstancesOfClass_Inner(OldToNewClassMap, OriginalCDO, ObjectsThatShouldUseOldStuff, bClassObjectReplaced, bPreserveRootComponent);
}

void FBlueprintCompileReinstancer::BatchReplaceInstancesOfClass(TMap<UClass*, UClass*>& InOldToNewClassMap, TSet<UObject*>* ObjectsThatShouldUseOldStuff, bool bClassObjectReplaced, bool bPreserveRootComponent)
{
	if (InOldToNewClassMap.Num() == 0)
	{
		return;
	}

	ReplaceInstancesOfClass_Inner(InOldToNewClassMap, nullptr, ObjectsThatShouldUseOldStuff, bClassObjectReplaced, bPreserveRootComponent);
}

void FBlueprintCompileReinstancer::ReplaceInstancesOfClass_Inner(TMap<UClass*, UClass*>& InOldToNewClassMap, UObject* InOriginalCDO, TSet<UObject*>* ObjectsThatShouldUseOldStuff, bool bClassObjectReplaced, bool bPreserveRootComponent)
{
	// If there is an original CDO, we are only reinstancing a single class
	check((InOriginalCDO != nullptr && InOldToNewClassMap.Num() == 1) || InOriginalCDO == nullptr); // (InOldToNewClassMap.Num() > 1 && InOriginalCDO == nullptr) || (InOldToNewClassMap.Num() == 1 && InOriginalCDO != nullptr));

	if (InOldToNewClassMap.Num() == 0)
	{
		return;
	}

	USelection* SelectedActors;
	bool bSelectionChanged = false;
	TArray<UObject*> ObjectsToReplace;
	const bool bLogConversions = false; // for debugging

	// Map of old objects to new objects
	TMap<UObject*, UObject*> OldToNewInstanceMap;

	TMap<FStringAssetReference, UObject*> ReinstancedObjectsWeakReferenceMap;

	// actors being replace
	TArray<FActorReplacementHelper> ReplacementActors;

	// A list of objects (e.g. Blueprints) that potentially have editors open that we need to refresh
	TArray<UObject*> PotentialEditorsForRefreshing;

	// A list of component owners that need their construction scripts re-ran (because a component of theirs has been reinstanced)
	TSet<AActor*> OwnersToRerunConstructionScript;

	// Set global flag to let system know we are reconstructing blueprint instances
	TGuardValue<bool> GuardTemplateNameFlag(GIsReconstructingBlueprintInstances, true);

	struct FObjectRemappingHelper
	{
		void OnObjectsReplaced(const TMap<UObject*, UObject*>& InReplacedObjects)
		{
			ReplacedObjects.Append(InReplacedObjects);
		}

		TMap<UObject*, UObject*> ReplacedObjects;
	} ObjectRemappingHelper;

	FDelegateHandle OnObjectsReplacedHandle = GEditor->OnObjectsReplaced().AddRaw(&ObjectRemappingHelper, &FObjectRemappingHelper::OnObjectsReplaced);
	{
		SelectedActors = GEditor->GetSelectedActors();
		SelectedActors->BeginBatchSelectOperation();
		SelectedActors->Modify();

		for (TPair<UClass*, UClass*> OldToNewClass : InOldToNewClassMap)
		{
			UClass* OldClass = OldToNewClass.Key;
			UClass* NewClass = OldToNewClass.Value;
			check(OldClass && NewClass);
			check(OldClass != NewClass || GIsHotReload);

			{ BP_SCOPED_COMPILER_EVENT_STAT(EKismetReinstancerStats_ReplaceInstancesOfClass);

				const bool bIncludeDerivedClasses = false;
				GetObjectsOfClass(OldClass, ObjectsToReplace, bIncludeDerivedClasses);

				// Then fix 'real' (non archetype) instances of the class
				for (int32 OldObjIndex = 0; OldObjIndex < ObjectsToReplace.Num(); ++OldObjIndex)
				{
					UObject* OldObject = ObjectsToReplace[OldObjIndex];
					// Skip non-archetype instances, EXCEPT for component templates
					const bool bIsComponent = NewClass->IsChildOf(UActorComponent::StaticClass());
					if ((!bIsComponent && OldObject->IsTemplate()) || OldObject->IsPendingKill())
					{
						//OldObject->SetClass(NewClass);
						continue;
					}

					UBlueprint* DebuggingBlueprint = nullptr;
					// ObjectsToReplace is not in any garunteed order; so below there may be a 
					// scenario where one object in ObjectsToReplace needs to come before another; 
					// in that scenario we swap the two in ObjectsToReplace and finish the loop 
					// with a new OldObject - to support that we need to re-cache info about the 
					// (new) old object before it is destroyed (hence why this is all in a lambda,  
					// for reuse).
					auto CacheOldObjectState = [&DebuggingBlueprint](UObject* Replacee)
					{
						// clear in case it has already been set, and we're re-executing this
						DebuggingBlueprint = nullptr;

						if (UBlueprint* OldObjBlueprint = Cast<UBlueprint>(Replacee->GetClass()->ClassGeneratedBy))
						{
							if (OldObjBlueprint->GetObjectBeingDebugged() == Replacee)
							{
								DebuggingBlueprint = OldObjBlueprint;
							}
						}
					};
					CacheOldObjectState(OldObject);

					AActor*  OldActor = Cast<AActor>(OldObject);
					UObject* NewUObject = nullptr;
					// if the object to replace is an actor...
					if (OldActor != nullptr && OldActor->GetLevel())
					{
						FVector  Location = FVector::ZeroVector;
						FRotator Rotation = FRotator::ZeroRotator;
						if (USceneComponent* OldRootComponent = OldActor->GetRootComponent())
						{
							// We need to make sure that the ComponentToWorld transform is up to date, but we don't want to run any initialization logic
							// so we silence the update, cache it off, revert the change (so no events are raised), and then directly update the transform
							// with the value calculated in ConditionalUpdateComponentToWorld:
							FScopedMovementUpdate SilenceMovement(OldRootComponent);

							OldRootComponent->ConditionalUpdateComponentToWorld();
							FTransform OldComponentToWorld = OldRootComponent->ComponentToWorld;
							SilenceMovement.RevertMove();

							OldRootComponent->ComponentToWorld = OldComponentToWorld;
							Location = OldActor->GetActorLocation();
							Rotation = OldActor->GetActorRotation();
						}

						// If this actor was spawned from an Archetype, we spawn the new actor from the new version of that archetype
						UObject* OldArchetype = OldActor->GetArchetype();
						UWorld*  World = OldActor->GetWorld();
						AActor*  NewArchetype = Cast<AActor>(OldToNewInstanceMap.FindRef(OldArchetype));
						// Check that either this was an instance of the class directly, or we found a new archetype for it
						check(OldArchetype == OldClass->GetDefaultObject() || NewArchetype);

						// Spawn the new actor instance, in the same level as the original, but deferring running the construction script until we have transferred modified properties
						ULevel*  ActorLevel = OldActor->GetLevel();
						UClass** MappedClass = InOldToNewClassMap.Find(OldActor->GetClass());
						UClass*  SpawnClass = MappedClass ? *MappedClass : NewClass;

						FActorSpawnParameters SpawnInfo;
						SpawnInfo.OverrideLevel = ActorLevel;
						SpawnInfo.Template = NewArchetype;
						SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
						SpawnInfo.bDeferConstruction = true;
						SpawnInfo.Name = OldActor->GetFName();

						// Temporarily remove the deprecated flag so we can respawn the Blueprint in the level
						const bool bIsClassDeprecated = SpawnClass->HasAnyClassFlags(CLASS_Deprecated);
						SpawnClass->ClassFlags &= ~CLASS_Deprecated;

						OldActor->UObject::Rename(nullptr, OldObject->GetOuter(), REN_DoNotDirty | REN_DontCreateRedirectors);
						AActor* NewActor = World->SpawnActor(SpawnClass, &Location, &Rotation, SpawnInfo);
						
						if (OldActor->CurrentTransactionAnnotation.IsValid())
						{
							NewActor->CurrentTransactionAnnotation = OldActor->CurrentTransactionAnnotation;
						}

						// Reassign the deprecated flag if it was previously assigned
						if (bIsClassDeprecated)
						{
							SpawnClass->ClassFlags |= CLASS_Deprecated;
						}

						check(NewActor != nullptr);
						NewUObject = NewActor;
						// store the new actor for the second pass (NOTE: this detaches 
						// OldActor from all child/parent attachments)
						//
						// running the NewActor's construction-script is saved for that 
						// second pass (because the construction-script may reference 
						// another instance that hasn't been replaced yet).
						ReplacementActors.Add(FActorReplacementHelper(NewActor, OldActor));

						ReinstancedObjectsWeakReferenceMap.Add(OldObject, NewUObject);

						OldActor->DestroyConstructedComponents(); // don't want to serialize components from the old actor
																  // Unregister native components so we don't copy any sub-components they generate for themselves (like UCameraComponent does)
						OldActor->UnregisterAllComponents();

						// Unregister any native components, might have cached state based on properties we are going to overwrite
						NewActor->UnregisterAllComponents();

						UEngine::FCopyPropertiesForUnrelatedObjectsParams Params;
						Params.bPreserveRootComponent = bPreserveRootComponent;
						UEngine::CopyPropertiesForUnrelatedObjects(OldActor, NewActor, Params);

						// reset properties/streams
						NewActor->ResetPropertiesForConstruction();
						// register native components
						NewActor->RegisterAllComponents();

						// 
						// clean up the old actor (unselect it, remove it from the world, etc.)...

						if (OldActor->IsSelected())
						{
							GEditor->SelectActor(OldActor, /*bInSelected =*/false, /*bNotify =*/false);
							bSelectionChanged = true;
						}
						if (GEditor->Layers.IsValid()) // ensure(NULL != GEditor->Layers) ?? While cooking the Layers is NULL.
						{
							GEditor->Layers->DisassociateActorFromLayers(OldActor);
						}

						World->EditorDestroyActor(OldActor, /*bShouldModifyLevel =*/true);
						OldToNewInstanceMap.Add(OldActor, NewActor);
					}
					else
					{
						// If the old object was spawned from an archetype (i.e. not the CDO), we must use the new version of that archetype as the template object when constructing the new instance.
						UObject* OldArchetype = OldObject->GetArchetype();
						UObject* NewArchetype = OldToNewInstanceMap.FindRef(OldArchetype);

						bool bArchetypeReinstanced = (OldArchetype == OldClass->GetDefaultObject()) || (NewArchetype != nullptr);
						// if we don't have a updated archetype to spawn from, we need to update/reinstance it
						while (!bArchetypeReinstanced)
						{
							int32 ArchetypeIndex = ObjectsToReplace.Find(OldArchetype);
							if (ArchetypeIndex != INDEX_NONE)
							{
								if (ensure(ArchetypeIndex > OldObjIndex))
								{
									// if this object has an archetype, but it hasn't been 
									// reinstanced yet (but is queued to) then we need to swap out 
									// the two, and reinstance the archetype first
									ObjectsToReplace.Swap(ArchetypeIndex, OldObjIndex);
									OldObject = ObjectsToReplace[OldObjIndex];
									check(OldObject == OldArchetype);

									// update any info that we stored regarding the previous OldObject
									CacheOldObjectState(OldObject);
									
									OldArchetype = OldObject->GetArchetype();
									NewArchetype = OldToNewInstanceMap.FindRef(OldArchetype);
									bArchetypeReinstanced = (OldArchetype == OldClass->GetDefaultObject()) || (NewArchetype != nullptr);
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
						// Check that either this was an instance of the class directly, or we found a new archetype for it
						ensureMsgf(bArchetypeReinstanced, TEXT("Reinstancing non-actor (%s); failed to resolve archetype object - property values may be lost."), *OldObject->GetPathName());

						EObjectFlags OldFlags = OldObject->GetFlags();

						FName OldName(OldObject->GetFName());
						OldObject->Rename(NULL, OldObject->GetOuter(), REN_DoNotDirty | REN_DontCreateRedirectors);
						NewUObject = NewObject<UObject>(OldObject->GetOuter(), NewClass, OldName, RF_NoFlags, NewArchetype);
						check(NewUObject != nullptr);

						auto FlagMask = RF_Public | RF_ArchetypeObject | RF_Transactional | RF_Transient | RF_TextExportTransient | RF_InheritableComponentTemplate; //TODO: what about RF_RootSet and RF_Standalone ?
						NewUObject->SetFlags(OldFlags & FlagMask);

						InstancedPropertyUtils::FInstancedPropertyMap InstancedPropertyMap;
						InstancedPropertyUtils::FArchiveInstancedSubObjCollector  InstancedSubObjCollector(OldObject, InstancedPropertyMap);
						UEditorEngine::CopyPropertiesForUnrelatedObjects(OldObject, NewUObject);
						InstancedPropertyUtils::FArchiveInsertInstancedSubObjects InstancedSubObjSpawner(NewUObject, InstancedPropertyMap);

						if (UAnimInstance* AnimTree = Cast<UAnimInstance>(NewUObject))
						{
							// Initialising the anim instance isn't enough to correctly set up the skeletal mesh again in a
							// paused world, need to initialise the skeletal mesh component that contains the anim instance.
							if (USkeletalMeshComponent* SkelComponent = Cast<USkeletalMeshComponent>(AnimTree->GetOuter()))
							{
								SkelComponent->InitAnim(true);
							}
						}

						UWorld* RegisteredWorld = nullptr;
						bool bWasRegistered = false;
						if (bIsComponent)
						{
							UActorComponent* OldComponent = CastChecked<UActorComponent>(OldObject);
							if (OldComponent->IsRegistered())
							{
								bWasRegistered = true;
								RegisteredWorld = OldComponent->GetWorld();
								OldComponent->UnregisterComponent();
							}
						}

						OldObject->RemoveFromRoot();
						OldObject->MarkPendingKill();

						OldToNewInstanceMap.Add(OldObject, NewUObject);

						if (bIsComponent)
						{
							UActorComponent* Component = CastChecked<UActorComponent>(NewUObject);
							AActor* OwningActor = Component->GetOwner();
							if (OwningActor)
							{
								OwningActor->ResetOwnedComponents();

								// Check to see if they have an editor that potentially needs to be refreshed
								if (OwningActor->GetClass()->ClassGeneratedBy)
								{
									PotentialEditorsForRefreshing.AddUnique(OwningActor->GetClass()->ClassGeneratedBy);
								}

								// we need to keep track of actor instances that need 
								// their construction scripts re-ran (since we've just 
								// replaced a component they own)

								// Skipping CDOs as CSs are not allowed for them.
								if (!OwningActor->HasAnyFlags(RF_ClassDefaultObject))
								{
									OwnersToRerunConstructionScript.Add(OwningActor);
								}
							}

							if (bWasRegistered)
							{
								if (RegisteredWorld && OwningActor == nullptr)
								{
									// Thumbnail components are added to a World without an actor, so we must special case their
									// REINST to register them with the world again.
									// The old thumbnail component is GC'd and will ensure if all it's attachments are not released
									// @TODO: This special case can breakdown if the nature of thumbnail components changes and could
									// use a cleanup later.
									if (OldObject->GetOutermost() == GetTransientPackage())
									{
										if (USceneComponent* SceneComponent = Cast<USceneComponent>(OldObject))
										{
											FDirectAttachChildrenAccessor::Get(SceneComponent).Empty();
											SceneComponent->SetupAttachment(nullptr);
										}
									}

									Component->RegisterComponentWithWorld(RegisteredWorld);
								}
								else
								{
									Component->RegisterComponent();
								}
							}
						}
					}

					// If this original object came from a blueprint and it was in the selected debug set, change the debugging to the new object.
					if (DebuggingBlueprint && NewUObject)
					{
						DebuggingBlueprint->SetObjectBeingDebugged(NewUObject);
					}

					if (bLogConversions)
					{
						UE_LOG(LogBlueprint, Log, TEXT("Converted instance '%s' to '%s'"), *OldObject->GetPathName(), *NewUObject->GetPathName());
					}
				}
			}
		}
	}
	GEditor->OnObjectsReplaced().Remove(OnObjectsReplacedHandle);

	// Now replace any pointers to the old archetypes/instances with pointers to the new one
	TArray<UObject*> SourceObjects;
	TArray<UObject*> DstObjects;

	OldToNewInstanceMap.GenerateKeyArray(SourceObjects);
	OldToNewInstanceMap.GenerateValueArray(DstObjects); // Also look for references in new spawned objects.

	SourceObjects.Append(DstObjects);
	
	if (InOriginalCDO)
	{
		check(InOldToNewClassMap.Num() == 1);
		for (TPair<UClass*, UClass*> OldToNewClass : InOldToNewClassMap)
		{
			UClass* OldClass = OldToNewClass.Key;
			UClass* NewClass = OldToNewClass.Value;
			check(OldClass && NewClass);
			check(OldClass != NewClass || GIsHotReload);

			FReplaceReferenceHelper::IncludeCDO(OldClass, NewClass, OldToNewInstanceMap, SourceObjects, InOriginalCDO);

			if (bClassObjectReplaced)
			{
				FReplaceReferenceHelper::IncludeClass(OldClass, NewClass, OldToNewInstanceMap, SourceObjects, ObjectsToReplace);
			}
		}
	}

	FReplaceReferenceHelper::FindAndReplaceReferences(SourceObjects, ObjectsThatShouldUseOldStuff, ObjectsToReplace, OldToNewInstanceMap, ReinstancedObjectsWeakReferenceMap);

	{ BP_SCOPED_COMPILER_EVENT_STAT(EKismetReinstancerStats_ReplacementConstruction);

		// the process of setting up new replacement actors is split into two 
		// steps (this here, is the second)...
		// 
		// the "finalization" here runs the replacement actor's construction-
		// script and is left until late to account for a scenario where the 
		// construction-script attempts to modify another instance of the 
		// same class... if this were to happen above, in the ObjectsToReplace 
		// loop, then accessing that other instance would cause an assert in 
		// UProperty::ContainerPtrToValuePtrInternal() (which appropriatly 
		// complains that the other instance's type doesn't match because it 
		// hasn't been replaced yet... that's why we wait until after 
		// FArchiveReplaceObjectRef to run construction-scripts).
		for (FActorReplacementHelper& ReplacementActor : ReplacementActors)
		{
			ReplacementActor.Finalize(ObjectRemappingHelper.ReplacedObjects, ObjectsThatShouldUseOldStuff, ObjectsToReplace, ReinstancedObjectsWeakReferenceMap);
		}
	}

	SelectedActors->EndBatchSelectOperation();
	if (bSelectionChanged && GEditor)
	{
		GEditor->NoteSelectionChange();
	}

	if (GEditor)
	{
		// Refresh any editors for objects that we've updated components for
		for (auto BlueprintAsset : PotentialEditorsForRefreshing)
		{
			FBlueprintEditor* BlueprintEditor = static_cast<FBlueprintEditor*>(FAssetEditorManager::Get().FindEditorForAsset(BlueprintAsset, /*bFocusIfOpen =*/false));
			if (BlueprintEditor)
			{
				BlueprintEditor->RefreshEditors();
			}
		}
	}

	// in the case where we're replacing component instances, we need to make 
	// sure to re-run their owner's construction scripts
	for (AActor* ActorInstance : OwnersToRerunConstructionScript)
	{
		ActorInstance->RerunConstructionScripts();
	}
}

void FBlueprintCompileReinstancer::ReconstructOwnerInstances(TSubclassOf<UActorComponent> ComponentClass)
{
	if (ComponentClass == nullptr)
	{
		return;
	}

	TArray<UObject*> ComponentInstances;
	GetObjectsOfClass(ComponentClass, ComponentInstances, /*bIncludeDerivedClasses =*/false);

	TSet<AActor*> OwnerInstances;
	for (UObject* ComponentObj : ComponentInstances)
	{
	
		UActorComponent* Component = CastChecked<UActorComponent>(ComponentObj);
			
		if (AActor* OwningActor = Component->GetOwner())
		{
			// we don't just rerun construction here, because we could end up 
			// doing it twice for the same actor (if it had multiple components 
			// of this kind), so we put that off as a secondary pass
			OwnerInstances.Add(OwningActor);
		}
	}

	for (AActor* ComponentOwner : OwnerInstances)
	{
		ComponentOwner->RerunConstructionScripts();
	}
}

void FBlueprintCompileReinstancer::VerifyReplacement()
{
	TArray<UObject*> SourceObjects;

	// Find all instances of the old class
	for( TObjectIterator<UObject> it; it; ++it )
	{
		UObject* CurrentObj = *it;

		if( (CurrentObj->GetClass() == DuplicatedClass) )
		{
			SourceObjects.Add(CurrentObj);
		}
	}

	// For each instance, track down references
	if( SourceObjects.Num() > 0 )
	{
		TFindObjectReferencers<UObject> Referencers(SourceObjects, NULL, false);
		for (TFindObjectReferencers<UObject>::TIterator It(Referencers); It; ++It)
		{
			UObject* CurrentObject = It.Key();
			UObject* ReferencedObj = It.Value();
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("- Object %s is referencing %s ---"), *CurrentObject->GetName(), *ReferencedObj->GetName());
		}
	}
}
void FBlueprintCompileReinstancer::ReparentChild(UBlueprint* ChildBP)
{
	check(ChildBP);

	UClass* SkeletonClass = ChildBP->SkeletonGeneratedClass;
	UClass* GeneratedClass = ChildBP->GeneratedClass;

	const bool ReparentGeneratedOnly = (ReinstClassType == RCT_BpGenerated);
	if( !ReparentGeneratedOnly && SkeletonClass )
	{
		ReparentChild(SkeletonClass);
	}

	const bool ReparentSkelOnly = (ReinstClassType == RCT_BpSkeleton);
	if( !ReparentSkelOnly && GeneratedClass )
	{
		ReparentChild(GeneratedClass);
	}
}

void FBlueprintCompileReinstancer::ReparentChild(UClass* ChildClass)
{
	check(ChildClass && ClassToReinstance && DuplicatedClass && ChildClass->GetSuperClass());
	bool bIsReallyAChild = ChildClass->GetSuperClass() == ClassToReinstance || ChildClass->GetSuperClass() == DuplicatedClass;
	const auto SuperClassBP = Cast<UBlueprint>(ChildClass->GetSuperClass()->ClassGeneratedBy);
	if (SuperClassBP && !bIsReallyAChild)
	{
		bIsReallyAChild |= (SuperClassBP->SkeletonGeneratedClass == ClassToReinstance) || (SuperClassBP->SkeletonGeneratedClass == DuplicatedClass);
		bIsReallyAChild |= (SuperClassBP->GeneratedClass == ClassToReinstance) || (SuperClassBP->GeneratedClass == DuplicatedClass);
	}
	check(bIsReallyAChild);

	ChildClass->AssembleReferenceTokenStream();
	ChildClass->SetSuperStruct(DuplicatedClass);
	ChildClass->Bind();
	ChildClass->StaticLink(true);
}

UObject* FBlueprintCompileReinstancer::GetClassCDODuplicate(UObject* CDO, FName Name)
{
	UObject* DupCDO = nullptr;

	FCDODuplicatesProvider& CDODupProvider = GetCDODuplicatesProviderDelegate();

	if (!CDODupProvider.IsBound() || (DupCDO = CDODupProvider.Execute(CDO, Name)) == nullptr)
	{
		GIsDuplicatingClassForReinstancing = true;
		DupCDO = (UObject*)StaticDuplicateObject(CDO, GetTransientPackage(), Name);
		GIsDuplicatingClassForReinstancing = false;
	}

	return DupCDO;
}

FBlueprintCompileReinstancer::FCDODuplicatesProvider& FBlueprintCompileReinstancer::GetCDODuplicatesProviderDelegate()
{
	static FCDODuplicatesProvider Delegate;
	return Delegate;
}

FRecreateUberGraphFrameScope::FRecreateUberGraphFrameScope(UClass* InClass, bool bRecreate)
	: RecompiledClass(InClass)
{
	if (bRecreate && ensure(RecompiledClass))
	{
		BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_RecreateUberGraphPersistentFrame);

		const bool bIncludeDerivedClasses = true;
		GetObjectsOfClass(RecompiledClass, Objects, bIncludeDerivedClasses, RF_NoFlags);

		for (auto Obj : Objects)
		{
			RecompiledClass->DestroyPersistentUberGraphFrame(Obj);
		}
	}
}

FRecreateUberGraphFrameScope::~FRecreateUberGraphFrameScope()
{
	BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_RecreateUberGraphPersistentFrame);
	for (auto Obj : Objects)
	{
		if (IsValid(Obj))
		{
			RecompiledClass->CreatePersistentUberGraphFrame(Obj, false);
		}
	}
}