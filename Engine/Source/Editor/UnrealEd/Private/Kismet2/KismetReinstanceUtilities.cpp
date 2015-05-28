// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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

DECLARE_CYCLE_STAT(TEXT("Replace Instances"), EKismetReinstancerStats_ReplaceInstancesOfClass, STATGROUP_KismetReinstancer );
DECLARE_CYCLE_STAT(TEXT("Find Referencers"), EKismetReinstancerStats_FindReferencers, STATGROUP_KismetReinstancer );
DECLARE_CYCLE_STAT(TEXT("Replace References"), EKismetReinstancerStats_ReplaceReferences, STATGROUP_KismetReinstancer );
DECLARE_CYCLE_STAT(TEXT("Construct Replacements"), EKismetReinstancerStats_ReplacementConstruction, STATGROUP_KismetReinstancer );
DECLARE_CYCLE_STAT(TEXT("Update Bytecode References"), EKismetReinstancerStats_UpdateBytecodeReferences, STATGROUP_KismetReinstancer );
DECLARE_CYCLE_STAT(TEXT("Recompile Child Classes"), EKismetReinstancerStats_RecompileChildClasses, STATGROUP_KismetReinstancer );
DECLARE_CYCLE_STAT(TEXT("Replace Classes Without Reinstancing"), EKismetReinstancerStats_ReplaceClassNoReinsancing, STATGROUP_KismetReinstancer );
DECLARE_CYCLE_STAT(TEXT("Reinstance Objects"), EKismetCompilerStats_ReinstanceObjects, STATGROUP_KismetCompiler);
DECLARE_CYCLE_STAT(TEXT("Refresh Dependent Blueprints In Reinstancer"), EKismetCompilerStats_RefreshDependentBlueprintsInReinstancer, STATGROUP_KismetCompiler);

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

UClass* FBlueprintCompileReinstancer::HotReloadedOldClass = nullptr;
UClass* FBlueprintCompileReinstancer::HotReloadedNewClass = nullptr;

FBlueprintCompileReinstancer::FBlueprintCompileReinstancer(UClass* InClassToReinstance, bool bIsBytecodeOnly, bool bSkipGC)
	: ClassToReinstance(InClassToReinstance)
	, DuplicatedClass(NULL)
	, OriginalCDO(NULL)
	, bHasReinstanced(false)
	, bSkipGarbageCollection(bSkipGC)
	, ClassToReinstanceDefaultValuesCRC(0)
{
	if( InClassToReinstance != NULL )
	{
		bIsReinstancingSkeleton = FKismetEditorUtilities::IsClassABlueprintSkeleton(ClassToReinstance);

		SaveClassFieldMapping(InClassToReinstance);

		// Remember the initial CDO for the class being resinstanced
		OriginalCDO = ClassToReinstance->GetDefaultObject();

		// Duplicate the class we're reinstancing into the transient package.  We'll re-class all objects we find to point to this new class
		GIsDuplicatingClassForReinstancing = true;
		ClassToReinstance->ClassFlags |= CLASS_NewerVersionExists;
		const FName RenistanceName = MakeUniqueObjectName(GetTransientPackage(), ClassToReinstance->GetClass(), *FString::Printf(TEXT("REINST_%s"), *ClassToReinstance->GetName()));
		DuplicatedClass = (UClass*)StaticDuplicateObject(ClassToReinstance, GetTransientPackage(), *RenistanceName.ToString(), ~RF_Transactional); 

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
		
		DuplicatedClass->ClassDefaultObject = GetClassCDODuplicate(ClassToReinstance, DuplicatedClass->GetDefaultObjectName());

		DuplicatedClass->ClassDefaultObject->SetFlags(RF_ClassDefaultObject);
		DuplicatedClass->ClassDefaultObject->SetClass(DuplicatedClass);

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
		if(GeneratingBP)
		{
			ClassToReinstanceDefaultValuesCRC = GeneratingBP->CrcLastCompiledCDO;
			FBlueprintEditorUtils::GetDependentBlueprints(GeneratingBP, Dependencies);
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
	Collector.AddReferencedObject(OriginalCDO);
	Collector.AddReferencedObject(DuplicatedClass);
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
				Actor->ReregisterAllComponents();
				Actor->RerunConstructionScripts();

				if (SelectedObjecs.Contains(Obj))
				{
					GEditor->SelectActor(Actor, /*bInSelected =*/true, /*bNotify =*/true, false, true);
				}
			}
		}

		const bool bIsAnimInstance = ClassToReinstance->IsChildOf<UAnimInstance>();
		if (bIsAnimInstance)
		{
			for (auto Obj : ObjectsToFinalize)
			{
				// Initialising the anim instance isn't enough to correctly set up the skeletal mesh again in a
				// paused world, need to initialise the skeletal mesh component that contains the anim instance.
				if (USkeletalMeshComponent* SkelComponent = Cast<USkeletalMeshComponent>(Obj->GetOuter()))
				{
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

			FKismetEditorUtilities::CompileBlueprint(BP, false, bSkipGarbageCollection);
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

void FBlueprintCompileReinstancer::ReinstanceObjects(bool bForceAlwaysReinstance)
{
	BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_ReinstanceObjects);
	
	// Make sure we only reinstance classes once!
	static TArray<TSharedRef<FBlueprintCompileReinstancer>> QueueToReinstance;
	TSharedRef<FBlueprintCompileReinstancer> SharedThis = AsShared();
	const bool bAlreadyQueued = QueueToReinstance.Contains(SharedThis);
	if (!bAlreadyQueued && !bHasReinstanced)
	{
		QueueToReinstance.Push(SharedThis);

		if (ClassToReinstance && DuplicatedClass)
		{
			CompileChildren();
		}

		if (QueueToReinstance.Num() && (QueueToReinstance[0] == SharedThis))
		{
			while (DependentBlueprintsToRecompile.Num())
			{
				auto Iter = DependentBlueprintsToRecompile.CreateIterator();
				TWeakObjectPtr<UBlueprint> BPPtr = *Iter;
				Iter.RemoveCurrent();
				if (auto BP = BPPtr.Get())
				{
					FKismetEditorUtilities::CompileBlueprint(BP, false, bSkipGarbageCollection);
				}
			}

			while (DependentBlueprintsToByteRecompile.Num())
			{
				auto Iter = DependentBlueprintsToByteRecompile.CreateIterator();
				TWeakObjectPtr<UBlueprint> BPPtr = *Iter;
				Iter.RemoveCurrent();
				if (auto BP = BPPtr.Get())
				{
					FKismetEditorUtilities::RecompileBlueprintBytecode(BP);
				}
			}

			ensure(0 == DependentBlueprintsToRecompile.Num());

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

		for( auto DependentBP = Dependencies.CreateIterator(); DependentBP; ++DependentBP )
		{
			UClass* BPClass = (*DependentBP)->GeneratedClass;

			// Skip cases where the class is junk, or haven't finished serializing in yet
			if( (BPClass == ClassToReinstance)
				|| (BPClass->GetOutermost() == GetTransientPackage()) 
				|| BPClass->HasAnyClassFlags(CLASS_NewerVersionExists)
				|| (BPClass->ClassGeneratedBy && BPClass->ClassGeneratedBy->HasAnyFlags(RF_NeedLoad|RF_BeingRegenerated)) )
			{
				continue;
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
	void Finalize(const TMap<UObject*, UObject*>& OldToNewInstanceMap);


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

void FActorReplacementHelper::Finalize(const TMap<UObject*, UObject*>& OldToNewInstanceMap)
{
	
	// because this is an editor context it's important to use this execution guard
	FEditorScriptExecutionGuard ScriptGuard;

	// run the construction script, which will use the properties we just copied over
	if (CachedActorData.IsValid())
	{
		CachedActorData->ComponentInstanceData.FindAndReplaceInstances(OldToNewInstanceMap);
		NewActor->ExecuteConstruction(TargetWorldTransform, &CachedActorData->ComponentInstanceData);
	}
	else
	{
		FComponentInstanceDataCache DummyComponentData;
		NewActor->ExecuteConstruction(TargetWorldTransform, &DummyComponentData);
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
		if (OldRootComponent->AttachParent != nullptr)
		{
			TargetAttachParent = OldRootComponent->AttachParent->GetOwner();
			// Root component should never be attached to another component in the same actor!
			if (TargetAttachParent == OldActor)
			{
				UE_LOG(LogBlueprint, Warning, TEXT("ReplaceInstancesOfClass: RootComponent (%s) attached to another component in this Actor (%s)."), *OldRootComponent->GetPathName(), *TargetAttachParent->GetPathName());
				TargetAttachParent = nullptr;
			}

			TargetAttachSocket    = OldRootComponent->AttachSocketName;
			TargetParentComponent = OldRootComponent->AttachParent;
			
			// detach it to remove any scaling
			OldRootComponent->DetachFromParent(/*bMaintainWorldPosition =*/true);
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
		if (AttachedActorRoot && AttachedActorRoot->AttachParent)
		{
			// Save info about actor to reattach
			FAttachedActorInfo Info;
			Info.AttachedActor = AttachedActor;
			Info.AttachedToSocket = AttachedActorRoot->AttachSocketName;
			PendingChildAttachments.Add(Info);

			// Now detach it
			AttachedActorRoot->DetachFromParent(/*bMaintainWorldPosition =*/true);
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
			NewRootComponent->AttachTo(TargetParentComponent, TargetAttachSocket, EAttachLocation::KeepWorldPosition);
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
			if (ChildRoot && ChildRoot->AttachParent != RootComponent)
			{
				ChildRoot->AttachTo(RootComponent, Info.AttachedToSocket, EAttachLocation::KeepWorldPosition);
				ChildRoot->UpdateComponentToWorld();
			}
		}
	}
}

void FBlueprintCompileReinstancer::ReplaceInstancesOfClass(UClass* OldClass, UClass* NewClass, UObject*	OriginalCDO, TSet<UObject*>* ObjectsThatShouldUseOldStuff, bool bClassObjectReplaced, bool bPreserveRootComponent)
{
	USelection* SelectedActors;
	bool bSelectionChanged = false;
	TArray<UObject*> ObjectsToReplace;
	const bool bLogConversions = false; // for debugging

	// Map of old objects to new objects
	TMap<UObject*, UObject*> OldToNewInstanceMap;
	TMap<UClass*, UClass*>   OldToNewClassMap;
	OldToNewClassMap.Add(OldClass, NewClass);

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

	FDelegateHandle OnObjectsReplacedHandle = GEditor->OnObjectsReplaced().AddRaw(&ObjectRemappingHelper,&FObjectRemappingHelper::OnObjectsReplaced);

	{ BP_SCOPED_COMPILER_EVENT_STAT(EKismetReinstancerStats_ReplaceInstancesOfClass);


		const bool bIncludeDerivedClasses = false;
		GetObjectsOfClass(OldClass, ObjectsToReplace, bIncludeDerivedClasses);
	
		SelectedActors = GEditor->GetSelectedActors();
		SelectedActors->BeginBatchSelectOperation();
		SelectedActors->Modify();
		
		// Then fix 'real' (non archetype) instances of the class
		for (UObject* OldObject : ObjectsToReplace)
		{
			// Skip non-archetype instances, EXCEPT for component templates
			const bool bIsComponent = NewClass->IsChildOf(UActorComponent::StaticClass());
			if ((!bIsComponent && OldObject->IsTemplate()) || OldObject->IsPendingKill())
			{
				continue;
			}

			UBlueprint* CorrespondingBlueprint  = Cast<UBlueprint>(OldObject->GetClass()->ClassGeneratedBy);
			UObject*    OldBlueprintDebugObject = nullptr;
			// If this object is being debugged, cache it off so we can preserve the 'object being debugged' association
			if ((CorrespondingBlueprint != nullptr) && (CorrespondingBlueprint->GetObjectBeingDebugged() == OldObject))
			{
				OldBlueprintDebugObject = OldObject;
			}

			AActor*  OldActor = Cast<AActor>(OldObject);
			UObject* NewUObject = nullptr;
			// if the object to replace is an actor...
			if (OldActor != nullptr)
			{
				FVector  Location = FVector::ZeroVector;
				FRotator Rotation = FRotator::ZeroRotator;
				if (USceneComponent* OldRootComponent = OldActor->GetRootComponent())
				{
					Location = OldActor->GetActorLocation();
					Rotation = OldActor->GetActorRotation();
				}

				// If this actor was spawned from an Archetype, we spawn the new actor from the new version of that archetype
				UObject* OldArchetype = OldActor->GetArchetype();
				UWorld*  World        = OldActor->GetWorld();
				AActor*  NewArchetype = Cast<AActor>(OldToNewInstanceMap.FindRef(OldArchetype));
				// Check that either this was an instance of the class directly, or we found a new archetype for it
				check(OldArchetype == OldClass->GetDefaultObject() || NewArchetype);

				// Spawn the new actor instance, in the same level as the original, but deferring running the construction script until we have transferred modified properties
				ULevel*  ActorLevel  = OldActor->GetLevel();
				UClass** MappedClass = OldToNewClassMap.Find(OldActor->GetClass());
				UClass*  SpawnClass  = MappedClass ? *MappedClass : NewClass;

				FActorSpawnParameters SpawnInfo;
				SpawnInfo.OverrideLevel      = ActorLevel;
				SpawnInfo.Template           = NewArchetype;
				SpawnInfo.bNoCollisionFail   = true;
				SpawnInfo.bDeferConstruction = true;

				// Temporarily remove the deprecated flag so we can respawn the Blueprint in the level
				const bool bIsClassDeprecated = SpawnClass->HasAnyClassFlags(CLASS_Deprecated);
				SpawnClass->ClassFlags &= ~CLASS_Deprecated;

				AActor* NewActor = World->SpawnActor(SpawnClass, &Location, &Rotation, SpawnInfo);
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
				auto OldFlags = OldObject->GetFlags();

				FName OldName(OldObject->GetFName());
				OldObject->Rename(NULL, OldObject->GetOuter(), REN_DoNotDirty | REN_DontCreateRedirectors);
				NewUObject = NewObject<UObject>(OldObject->GetOuter(), NewClass, OldName);
				check(NewUObject != nullptr);

				auto FlagMask = RF_Public | RF_ArchetypeObject | RF_Transactional | RF_Transient | RF_TextExportTransient | RF_InheritableComponentTemplate; //TODO: what about RF_RootSet and RF_Standalone ?
				NewUObject->SetFlags(OldFlags & FlagMask);

				UEditorEngine::CopyPropertiesForUnrelatedObjects(OldObject, NewUObject);

				if (UAnimInstance* AnimTree = Cast<UAnimInstance>(NewUObject))
				{
					// Initialising the anim instance isn't enough to correctly set up the skeletal mesh again in a
					// paused world, need to initialise the skeletal mesh component that contains the anim instance.
					if (USkeletalMeshComponent* SkelComponent = Cast<USkeletalMeshComponent>(AnimTree->GetOuter()))
					{
						SkelComponent->InitAnim(true);
					}
				}

				OldObject->RemoveFromRoot();
				OldObject->MarkPendingKill();

				OldToNewInstanceMap.Add(OldObject, NewUObject);

				if (bIsComponent)
				{
					UActorComponent* Component = Cast<UActorComponent>(NewUObject);
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
				}
			}

			// If this original object came from a blueprint and it was in the selected debug set, change the debugging to the new object.
			if ((CorrespondingBlueprint) && (OldBlueprintDebugObject) && (NewUObject))
			{
				CorrespondingBlueprint->SetObjectBeingDebugged(NewUObject);
			}

			if (bLogConversions)
			{
				UE_LOG(LogBlueprint, Log, TEXT("Converted instance '%s' to '%s'"), *OldObject->GetPathName(), *NewUObject->GetPathName());
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

	FReplaceReferenceHelper::IncludeCDO(OldClass, NewClass, OldToNewInstanceMap, SourceObjects, OriginalCDO);
	
	if (bClassObjectReplaced)
	{
		FReplaceReferenceHelper::IncludeClass(OldClass, NewClass, OldToNewInstanceMap, SourceObjects, ObjectsToReplace);
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
			ReplacementActor.Finalize(ObjectRemappingHelper.ReplacedObjects);
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

	if( bIsReinstancingSkeleton && SkeletonClass )
	{
		ReparentChild(SkeletonClass);
	}

	if( !bIsReinstancingSkeleton && GeneratedClass )
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

UObject* FBlueprintCompileReinstancer::GetClassCDODuplicate(UClass* Class, FName Name)
{
	UObject* DupCDO = nullptr;

	FCDODuplicatesProvider& CDODupProvider = GetCDODuplicatesProviderDelegate();

	if (!CDODupProvider.IsBound() || (DupCDO = CDODupProvider.Execute(Class, Name)) == nullptr)
	{
		GIsDuplicatingClassForReinstancing = true;
		DupCDO = (UObject*)StaticDuplicateObject(Class->GetDefaultObject(), GetTransientPackage(), *Name.ToString());
		GIsDuplicatingClassForReinstancing = false;
	}

	return DupCDO;
}

FBlueprintCompileReinstancer::FCDODuplicatesProvider& FBlueprintCompileReinstancer::GetCDODuplicatesProviderDelegate()
{
	static FCDODuplicatesProvider Delegate;
	return Delegate;
}
