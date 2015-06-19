// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"
#include "UObject/LinkerPlaceholderClass.h"
#include "UObject/LinkerPlaceholderExportObject.h"
#include "UObject/LinkerPlaceholderFunction.h"
#include "Linker.h"
#include "PropertyTag.h"
#include "UObject/StructScriptLoader.h"
#include "UObject/UObjectThreadContext.h"

/** 
 * Defined in BlueprintSupport.cpp
 * Duplicates all fields of a class in depth-first order. It makes sure that everything contained
 * in a class is duplicated before the class itself, as well as all function parameters before the
 * function itself.
 *
 * @param	StructToDuplicate			Instance of the struct that is about to be duplicated
 * @param	Writer						duplicate writer instance to write the duplicated data to
 */
void FBlueprintSupport::DuplicateAllFields(UStruct* StructToDuplicate, FDuplicateDataWriter& Writer)
{
	// This is a very simple fake topological-sort to make sure everything contained in the class
	// is processed before the class itself is, and each function parameter is processed before the function
	if (StructToDuplicate)
	{
		// Make sure each field gets allocated into the array
		for (TFieldIterator<UField> FieldIt(StructToDuplicate, EFieldIteratorFlags::ExcludeSuper); FieldIt; ++FieldIt)
		{
			UField* Field = *FieldIt;

			// Make sure functions also do their parameters and children first
			if (UFunction* Function = dynamic_cast<UFunction*>(Field))
			{
				for (TFieldIterator<UField> FunctionFieldIt(Function, EFieldIteratorFlags::ExcludeSuper); FunctionFieldIt; ++FunctionFieldIt)
				{
					UField* InnerField = *FunctionFieldIt;
					Writer.GetDuplicatedObject(InnerField);
				}
			}

			Writer.GetDuplicatedObject(Field);
		}
	}
}

bool FBlueprintSupport::UseDeferredDependencyLoading()
{
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	static const FBoolConfigValueHelper DeferDependencyLoads(TEXT("Kismet"), TEXT("bDeferDependencyLoads"), GEngineIni);
	bool bUseDeferredDependencyLoading = DeferDependencyLoads;

	if (FPlatformProperties::RequiresCookedData())
	{
		static const FBoolConfigValueHelper DisableCookedBuildDefering(TEXT("Kismet"), TEXT("bForceDisableCookedDependencyDeferring"), GEngineIni);
		bUseDeferredDependencyLoading &= !((bool)DisableCookedBuildDefering);
	}
	return bUseDeferredDependencyLoading;
#else
	return false;
#endif
}


bool FBlueprintSupport::IsResolvingDeferredDependenciesDisabled()
{
#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
	static const FBoolConfigValueHelper NoDeferredDependencyResolves(TEXT("Kismet"), TEXT("bForceDisableDeferredDependencyResolving"), GEngineIni);
	return !UseDeferredDependencyLoading() || NoDeferredDependencyResolves;
#else 
	return false;
#endif
}

bool FBlueprintSupport::IsDeferredCDOSerializationDisabled()
{
#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
	static const FBoolConfigValueHelper NoDeferredCDOLoading(TEXT("Kismet"), TEXT("bForceDisableDeferredCDOLoading"), GEngineIni);
	return !UseDeferredDependencyLoading() || NoDeferredCDOLoading;
#else 
	return false;
#endif
}

bool FBlueprintSupport::IsDeferredExportCreationDisabled()
{
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	static const FBoolConfigValueHelper NoDeferredExports(TEXT("Kismet"), TEXT("bForceDisableDeferredExportCreation"), GEngineIni);
	return !UseDeferredDependencyLoading() || NoDeferredExports;
#else
	return false;
#endif
}

bool FBlueprintSupport::IsDeferredCDOInitializationDisabled()
{
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	static const FBoolConfigValueHelper NoDeferredCDOInit(TEXT("Kismet"), TEXT("bForceDisableDeferredCDOInitialization"), GEngineIni);
	return !UseDeferredDependencyLoading() || NoDeferredCDOInit;
#else
	return false;
#endif
}

/*******************************************************************************
 * FScopedClassDependencyGather
 ******************************************************************************/

#if WITH_EDITOR
UClass* FScopedClassDependencyGather::BatchMasterClass = NULL;
TArray<UClass*> FScopedClassDependencyGather::BatchClassDependencies;

FScopedClassDependencyGather::FScopedClassDependencyGather(UClass* ClassToGather)
	: bMasterClass(false)
{
	// Do NOT track duplication dependencies, as these are intermediate products that we don't care about
	if( !GIsDuplicatingClassForReinstancing )
	{
		if( BatchMasterClass == NULL )
		{
			// If there is no current dependency master, register this class as the master, and reset the array
			BatchMasterClass = ClassToGather;
			BatchClassDependencies.Empty();
			bMasterClass = true;
		}
		else
		{
			// This class was instantiated while another class was gathering dependencies, so record it as a dependency
			BatchClassDependencies.AddUnique(ClassToGather);
		}
	}
}

FScopedClassDependencyGather::~FScopedClassDependencyGather()
{
	// If this gatherer was the initial gatherer for the current scope, process 
	// dependencies (unless compiling on load is explicitly disabled)
	if( bMasterClass && !GForceDisableBlueprintCompileOnLoad )
	{
		auto DependencyIter = BatchClassDependencies.CreateIterator();
		// implemented as a lambda, to prevent duplicated code between 
		// BatchMasterClass and BatchClassDependencies entries
		auto RecompileClassLambda = [&DependencyIter](UClass* Class)
		{
			Class->ConditionalRecompileClass(&FUObjectThreadContext::Get().ObjLoaded);

			// because of the above call to ConditionalRecompileClass(), the 
			// specified Class gets "cleaned and sanitized" (meaning its old 
			// properties get moved to a TRASH class, and new ones are 
			// constructed in their place)... the unfortunate side-effect of 
			// this is that child classes that have already been linked are now
			// referencing TRASH inherited properties; to resolve this issue, 
			// here we go back through dependencies that were already recompiled
			// and re-link any that are sub-classes
			//
			// @TODO: this isn't the most optimal solution to this problem; we 
			//        should probably instead prevent CleanAndSanitizeClass()
			//        from running for BytecodeOnly compiles (we would then need 
			//        to block UField re-creation)... UE-14957 was created to 
			//        track this issue
			auto ReverseIt = DependencyIter;
			for (--ReverseIt; ReverseIt.GetIndex() >= 0; --ReverseIt)
			{
				UClass* ProcessedDependency = *ReverseIt;
				if (ProcessedDependency->IsChildOf(Class))
				{
					ProcessedDependency->StaticLink(/*bRelinkExistingProperties =*/true);
				}
			}
		};

		for( ; DependencyIter; ++DependencyIter )
		{
			UClass* Dependency = *DependencyIter;
			if( Dependency->ClassGeneratedBy != BatchMasterClass->ClassGeneratedBy )
			{
				RecompileClassLambda(Dependency);
			}
		}

		// Finally, recompile the master class to make sure it gets updated too
		RecompileClassLambda(BatchMasterClass);
		BatchMasterClass = NULL;
	}
}

TArray<UClass*> const& FScopedClassDependencyGather::GetCachedDependencies()
{
	return BatchClassDependencies;
}
#endif //WITH_EDITOR



/*******************************************************************************
 * FLinkerLoad
 ******************************************************************************/

// rather than littering the code with USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
// checks, let's just define DEFERRED_DEPENDENCY_CHECK for the file
#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
	#define DEFERRED_DEPENDENCY_CHECK(CheckExpr) check(CheckExpr)
#else  // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
	#define DEFERRED_DEPENDENCY_CHECK(CheckExpr)
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
 
struct FPreloadMembersHelper
{
	static void PreloadMembers(UObject* InObject)
	{
		// Collect a list of all things this element owns
		TArray<UObject*> BPMemberReferences;
		FReferenceFinder ComponentCollector(BPMemberReferences, InObject, false, true, true, true);
		ComponentCollector.FindReferences(InObject);

		// Iterate over the list, and preload everything so it is valid for refreshing
		for (TArray<UObject*>::TIterator it(BPMemberReferences); it; ++it)
		{
			UObject* CurrentObject = *it;
			if (!CurrentObject->HasAnyFlags(RF_LoadCompleted))
			{
				CurrentObject->SetFlags(RF_NeedLoad);
				if (auto Linker = CurrentObject->GetLinker())
				{
					Linker->Preload(CurrentObject);
					PreloadMembers(CurrentObject);
				}
			}
		}
	}

	static void PreloadObject(UObject* InObject)
	{
		if (InObject && !InObject->HasAnyFlags(RF_LoadCompleted))
		{
			InObject->SetFlags(RF_NeedLoad);
			if (FLinkerLoad* Linker = InObject->GetLinker())
			{
				Linker->Preload(InObject);
			}
		}
	}
};

/**
 * Regenerates/Refreshes a blueprint class
 *
 * @param	LoadClass		Instance of the class currently being loaded and which is the parent for the blueprint
 * @param	ExportObject	Current object being exported
 * @return	Returns true if regeneration was successful, otherwise false
 */
bool FLinkerLoad::RegenerateBlueprintClass(UClass* LoadClass, UObject* ExportObject)
{
	// determine if somewhere further down the callstack, we're already in this
	// function for this class
	bool const bAlreadyRegenerating = LoadClass->ClassGeneratedBy->HasAnyFlags(RF_BeingRegenerated);
	// Flag the class source object, so we know we're already in the process of compiling this class
	LoadClass->ClassGeneratedBy->SetFlags(RF_BeingRegenerated);

	// Cache off the current CDO, and specify the CDO for the load class 
	// manually... do this before we Preload() any children members so that if 
	// one of those preloads subsequently ends up back here for this class, 
	// then the ExportObject is carried along and used in the eventual RegenerateClass() call
	UObject* CurrentCDO = ExportObject;
	check(!bAlreadyRegenerating || (LoadClass->ClassDefaultObject == ExportObject));
	LoadClass->ClassDefaultObject = CurrentCDO;

	// Finish loading the class here, so we have all the appropriate data to copy over to the new CDO
	TArray<UObject*> AllChildMembers;
	GetObjectsWithOuter(LoadClass, /*out*/ AllChildMembers);
	for (int32 Index = 0; Index < AllChildMembers.Num(); ++Index)
	{
		UObject* Member = AllChildMembers[Index];
		Preload(Member);
	}

	// if this was subsequently regenerated from one of the above preloads, then 
	// we don't have to finish this off, it was already done
	bool const bWasSubsequentlyRegenerated = !LoadClass->ClassGeneratedBy->HasAnyFlags(RF_BeingRegenerated);
	// @TODO: find some other condition to block this if we've already  
	//        regenerated the class (not just if we've regenerated the class 
	//        from an above Preload(Member))... UBlueprint::RegenerateClass() 
	//        has an internal conditional to block getting into it again, but we
	//        can't check UBlueprint members from this module
	if (!bWasSubsequentlyRegenerated)
	{
		Preload(LoadClass);

		LoadClass->StaticLink(true);
		Preload(CurrentCDO);

		// Load the class config values
		if( LoadClass->HasAnyClassFlags( CLASS_Config ))
		{
			CurrentCDO->LoadConfig( LoadClass );
		}

		// Make sure that we regenerate any parent classes first before attempting to build a child
		TArray<UClass*> ClassChainOrdered;
		{
			// Just ordering the class hierarchy from root to leafs:
			UClass* ClassChain = LoadClass->GetSuperClass();
			while (ClassChain && ClassChain->ClassGeneratedBy)
			{
				// O(n) insert, but n is tiny because this is a class hierarchy...
				ClassChainOrdered.Insert(ClassChain, 0);
				ClassChain = ClassChain->GetSuperClass();
			}
		}
		for (auto Class : ClassChainOrdered)
		{
			UObject* BlueprintObject = Class->ClassGeneratedBy;
			if (BlueprintObject && BlueprintObject->HasAnyFlags(RF_BeingRegenerated))
			{
				// Always load the parent blueprint here in case there is a circular dependency. This will
				// ensure that the blueprint is fully serialized before attempting to regenerate the class.
				FPreloadMembersHelper::PreloadObject(BlueprintObject);
			
				FPreloadMembersHelper::PreloadMembers(BlueprintObject);
				// recurse into this function for this parent class; 
				// 'ClassDefaultObject' should be the class's original ExportObject
				RegenerateBlueprintClass(Class, Class->ClassDefaultObject);
			}
		}

		{
			UObject* BlueprintObject = LoadClass->ClassGeneratedBy;
			// Preload the blueprint to make sure it has all the data the class needs for regeneration
			FPreloadMembersHelper::PreloadObject(BlueprintObject);

			UClass* RegeneratedClass = BlueprintObject->RegenerateClass(LoadClass, CurrentCDO, FUObjectThreadContext::Get().ObjLoaded);
			if (RegeneratedClass)
			{
				BlueprintObject->ClearFlags(RF_BeingRegenerated);
				// Fix up the linker so that the RegeneratedClass is used
				LoadClass->ClearFlags(RF_NeedLoad | RF_NeedPostLoad);
			}
		}
	}

	bool const bSuccessfulRegeneration = !LoadClass->ClassGeneratedBy->HasAnyFlags(RF_BeingRegenerated);
	// if this wasn't already flagged as regenerating when we first entered this 
	// function, the clear it ourselves.
	if (!bAlreadyRegenerating)
	{
		LoadClass->ClassGeneratedBy->ClearFlags(RF_BeingRegenerated);
	}

	return bSuccessfulRegeneration;
}

/** Helper utility function to convert UObject pointers to FLinkerPlaceholderBase* */
template<class PlaceholderType>
static FLinkerPlaceholderBase* PlaceholderCast(UObject* PlaceholderObj)
{
	return CastChecked<PlaceholderType>(PlaceholderObj, ECastCheckedType::NullAllowed);
}

bool FLinkerLoad::DeferPotentialCircularImport(const int32 Index)
{
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	if (!FBlueprintSupport::UseDeferredDependencyLoading())
	{
		return false;
	}

	//--------------------------------------
	// Phase 1: Stub in Dependencies
	//--------------------------------------

	FObjectImport& Import = ImportMap[Index];
	if (Import.XObject != nullptr)
	{
		bool const bIsPlaceHolderClass = Import.XObject->IsA<ULinkerPlaceholderClass>();
		return bIsPlaceHolderClass;
	}

	if ((LoadFlags & LOAD_DeferDependencyLoads) && !IsImportNative(Index))
	{
		if (UObject* ClassPackage = FindObject<UPackage>(/*Outer =*/nullptr, *Import.ClassPackage.ToString()))
		{
			if (const UClass* ImportClass = FindObject<UClass>(ClassPackage, *Import.ClassName.ToString()))
			{
				UObject* PlaceholderOuter = LinkerRoot;
				FString  PlaceholderNamePrefix = TEXT("PLACEHOLDER_");
				UClass*  PlaceholderType = nullptr;
#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
				ULinkerPlaceholderClass* Outer = nullptr;
#endif //USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
				FLinkerPlaceholderBase* (*PlaceholderCastFunc)(UObject* RawUObject) = nullptr;

				bool const bIsBlueprintClass = ImportClass->IsChildOf<UClass>();
				// @TODO: if we could see if the related package is created 
				//        (without loading it further), AND it already has this 
				//        import loaded, then we can probably return that rather 
				//        than allocating a placeholder
				if (bIsBlueprintClass)
				{
					PlaceholderNamePrefix = TEXT("PLACEHOLDER-CLASS_");
					PlaceholderType = ULinkerPlaceholderClass::StaticClass();
					PlaceholderCastFunc = &PlaceholderCast<ULinkerPlaceholderClass>;
				}
				else if (ImportClass->IsChildOf<UFunction>())
				{		
					if (Import.OuterIndex.IsImport())
					{
						const int32 OuterImportIndex = Import.OuterIndex.ToImport();
						// @TODO: if the sole reason why we have ULinkerPlaceholderFunction 
						//        is that it's outer is a placeholder, then we 
						//        could instead log it (with the placeholder) as 
						//        a referencer, and then move the function later
						if (DeferPotentialCircularImport(OuterImportIndex))
						{
							PlaceholderNamePrefix = TEXT("PLACEHOLDER-FUNCTION_");
							PlaceholderOuter = ImportMap[OuterImportIndex].XObject;
							PlaceholderType  = ULinkerPlaceholderFunction::StaticClass();
							PlaceholderCastFunc = &PlaceholderCast<ULinkerPlaceholderFunction>;
#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
							Outer = dynamic_cast<ULinkerPlaceholderClass*>(PlaceholderOuter);
							check(Outer);
#endif //USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
						}
					}
				}

				if (PlaceholderType != nullptr)
				{
					FName PlaceholderName(*FString::Printf(TEXT("%s_%s"), *PlaceholderNamePrefix, *Import.ObjectName.ToString()));
					PlaceholderName = MakeUniqueObjectName(PlaceholderOuter, PlaceholderType, PlaceholderName);

					UStruct* PlaceholderObj = NewObject<UStruct>(PlaceholderOuter, PlaceholderType, PlaceholderName, RF_Public | RF_Transient);
					// store the import index in the placeholder, so we can 
					// easily look it up in the import map, given the 
					// placeholder (needed, to find the corresponding import for 
					// ResolvingDeferredPlaceholder)
					DEFERRED_DEPENDENCY_CHECK(PlaceholderCastFunc != nullptr);
					if (PlaceholderCastFunc != nullptr)
					{
						if (FLinkerPlaceholderBase* AsPlaceholder = PlaceholderCastFunc(PlaceholderObj))
						{
							AsPlaceholder->PackageIndex = FPackageIndex::FromImport(Index);
						}
					}				
					// make sure the class is fully formed (has its 
					// ClassAddReferencedObjects/ClassConstructor members set)
					PlaceholderObj->Bind();
					PlaceholderObj->StaticLink(/*bRelinkExistingProperties =*/true);
#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
					if (Outer)
					{
						Outer->AddChildObject(PlaceholderObj);
					}
#endif //USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS

					Import.XObject = PlaceholderObj;
				}
			}
		}

		// not the best way to check this (but we don't have ObjectFlags on an 
		// import), but we don't want non-native (blueprint) CDO refs slipping 
		// through... we've only seen these needed when serializing a class's 
		// bytecode, and we resolved that by deferring script serialization
		DEFERRED_DEPENDENCY_CHECK(!Import.ObjectName.ToString().StartsWith("Default__"));
	}
	return (Import.XObject != nullptr);
#else // !USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	return false;
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
}

/** 
 * A helper utility for tracking exports whose classes we're currently running
 * through ForceRegenerateClass(). This is primarily relied upon to help prevent
 * infinite recursion since ForceRegenerateClass() doesn't do anything to 
 * progress the state of the linker.
 */
struct FResolvingExportTracker : TThreadSingleton<FResolvingExportTracker>
{
public:
	/**  */
	void FlagLinkerExportAsResolving(FLinkerLoad* Linker, int32 ExportIndex)
	{
		ResolvingExports.FindOrAdd(Linker).Add(ExportIndex);
	}

	/**  */
	bool IsLinkerExportBeingResolved(FLinkerLoad* Linker, int32 ExportIndex) const
	{
		if (auto* ExportIndices = ResolvingExports.Find(Linker))
		{
			return ExportIndices->Contains(ExportIndex);
		}
		return false;
	}

	/**  */
	void FlagExportClassAsFullyResolved(FLinkerLoad* Linker, int32 ExportIndex)
	{
		if (auto* ExportIndices = ResolvingExports.Find(Linker))
		{
			ExportIndices->Remove(ExportIndex);
			if (ExportIndices->Num() == 0)
			{
				ResolvingExports.Remove(Linker);
			}
		}
	}

#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
	void FlagFullExportResolvePassComplete(FLinkerLoad* Linker)
	{
		FullyResolvedLinkers.Add(Linker);
	}

	bool HasPerformedFullExportResolvePass(FLinkerLoad* Linker)
	{
		return FullyResolvedLinkers.Contains(Linker);
	}
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS

	void Reset(FLinkerLoad* Linker)
	{
		ResolvingExports.Remove(Linker);
#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
		FullyResolvedLinkers.Remove(Linker);
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
	}

private:
	/**  */
	TMap< FLinkerLoad*, TSet<int32> > ResolvingExports;

#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
	TSet<FLinkerLoad*> FullyResolvedLinkers;
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
};

/** 
 * A helper struct that adds and removes its linker/export combo from the 
 * thread's FResolvingExportTracker (based off the scope it was declared within).
 */
struct FScopedResolvingExportTracker
{
public: 
	FScopedResolvingExportTracker(FLinkerLoad* Linker, int32 ExportIndex)
		: TrackedLinker(Linker), TrackedExport(ExportIndex)
	{
		FResolvingExportTracker::Get().FlagLinkerExportAsResolving(Linker, ExportIndex);
	}

	~FScopedResolvingExportTracker()
	{
		FResolvingExportTracker::Get().FlagExportClassAsFullyResolved(TrackedLinker, TrackedExport);
	}

private:
	FLinkerLoad* TrackedLinker;
	int32        TrackedExport;
};

bool FLinkerLoad::DeferExportCreation(const int32 Index)
{
	FObjectExport& Export = ExportMap[Index];

#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	if (!FBlueprintSupport::UseDeferredDependencyLoading() || FBlueprintSupport::IsDeferredExportCreationDisabled())
	{
		return false;
	}

	if ((Export.Object != nullptr) || !Export.ClassIndex.IsImport())
	{
		return false;
	}

	UClass* LoadClass = GetExportLoadClass(Index);
	if (LoadClass == nullptr)
	{
		return false;
	}

	ULinkerPlaceholderClass* AsPlaceholderClass = Cast<ULinkerPlaceholderClass>(LoadClass);
	bool const bIsPlaceholderClass = (AsPlaceholderClass != nullptr);

	FLinkerLoad* ClassLinker = LoadClass->GetLinker();
	if ( !bIsPlaceholderClass && ((ClassLinker == nullptr) || !ClassLinker->IsBlueprintFinalizationPending()) )
	{
		return false;
	}

	bool const bIsLoadingExportClass = (LoadFlags & LOAD_DeferDependencyLoads) ||
		IsBlueprintFinalizationPending();
	// if we're not in the process of "loading/finalizing" this package's 
	// Blueprint class, then we're either running this before the linker has got 
	// to that class, or we're finished and in the midst of regenerating that 
	// class... either way, we don't have to defer the export (as long as we 
	// make sure the export's class is fully regenerated... presumably it is in 
	// the midst of doing so somewhere up the callstack)
	if (!bIsLoadingExportClass)
	{
		DEFERRED_DEPENDENCY_CHECK(!IsExportBeingResolved(Index));
		FScopedResolvingExportTracker ReentranceGuard(this, Index);

		// we want to be very careful, since we haven't filled in the export yet,
		// we could get stuck in a recursive loop here (force-finalizing the 
		// class here ends us back 
		ForceRegenerateClass(LoadClass);
		return false;
	}
	// we haven't come across a scenario where this happens, but if this hits 
	// then we're deferring exports that will NEVER be resolved (possibly 
	// stemming from CDO serialization in FLinkerLoad::ResolveDeferredExports)
	DEFERRED_DEPENDENCY_CHECK(!FResolvingExportTracker::Get().HasPerformedFullExportResolvePass(this));
	
	UPackage* PlaceholderOuter = LinkerRoot;
	UClass*   PlaceholderType  = ULinkerPlaceholderExportObject::StaticClass();

	FString ClassName = LoadClass->GetName();
	//ClassName.RemoveFromEnd("_C");	
	FName PlaceholderName(*FString::Printf(TEXT("PLACEHOLDER-INST_of_%s"), *ClassName));
	PlaceholderName = MakeUniqueObjectName(PlaceholderOuter, PlaceholderType, PlaceholderName);

	ULinkerPlaceholderExportObject* Placeholder = NewObject<ULinkerPlaceholderExportObject>(PlaceholderOuter, PlaceholderType, PlaceholderName, RF_Public | RF_Transient);
	Placeholder->PackageIndex = FPackageIndex::FromExport(Index);

	Export.Object = Placeholder;
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING

	return true;
}

int32 FLinkerLoad::FindCDOExportIndex(UClass* LoadClass)
{
	DEFERRED_DEPENDENCY_CHECK(LoadClass->GetLinker() == this);
	int32 const ClassExportIndex = LoadClass->GetLinkerIndex();

	// @TODO: the cdo SHOULD be listed after the class in the ExportMap, so we 
	//        could start with ClassExportIndex to save on some cycles
	for (int32 ExportIndex = 0; ExportIndex < ExportMap.Num(); ++ExportIndex)
	{
		FObjectExport& Export = ExportMap[ExportIndex];
		if ((Export.ObjectFlags & RF_ClassDefaultObject) && Export.ClassIndex.IsExport() && (Export.ClassIndex.ToExport() == ClassExportIndex))
		{
			return ExportIndex;
		}
	}
	return INDEX_NONE;
}

/**
 * This utility struct helps track blueprint structs/linkers that are currently 
 * in the middle of a call to ResolveDeferredDependencies(). This can be used  
 * to know if a dependency's resolve needs to be finished (to avoid unwanted 
 * placeholder references ending up in script-code).
 */
struct FUnresolvedStructTracker
{
public:
	/** Marks the specified struct (and its linker) as "resolving" for the lifetime of this instance */
	FUnresolvedStructTracker(UStruct* LoadStruct)
		: TrackedStruct(LoadStruct)
	{
		DEFERRED_DEPENDENCY_CHECK((LoadStruct != nullptr) && (LoadStruct->GetLinker() != nullptr));
		FScopeLock UnresolvedStructsLock(&UnresolvedStructsCritical);
		UnresolvedStructs.Add(LoadStruct);
	}

	/** Removes the wrapped struct from the "resolving" set (it has been fully "resolved") */
	~FUnresolvedStructTracker()
	{
		// even if another FUnresolvedStructTracker added this struct earlier,  
		// we want the most nested one removing it from the set (because this 
		// means the struct is fully resolved, even if we're still in the middle  
		// of a ResolveDeferredDependencies() call further up the stack)
		FScopeLock UnresolvedStructsLock(&UnresolvedStructsCritical);
		UnresolvedStructs.Remove(TrackedStruct);
	}

	/**
	 * Checks to see if the specified import object is a blueprint class/struct 
	 * that is currently in the midst of resolving (and hasn't completed that  
	 * resolve elsewhere in some nested call).
	 * 
	 * @param  ImportObject    The object you wish to check.
	 * @return True if the specified object is a class/struct that hasn't been fully resolved yet (otherwise false).
	 */
	static bool IsImportStructUnresolved(UObject* ImportObject)
	{
		FScopeLock UnresolvedStructsLock(&UnresolvedStructsCritical);
		return UnresolvedStructs.Contains(ImportObject);
	}

	/**
	 * Checks to see if the specified linker is associated with any of the 
	 * unresolved structs that this is currently tracking.
	 *
	 * NOTE: This could return false, even if the linker is in a 
	 *       ResolveDeferredDependencies() call futher up the callstack... in 
	 *       that scenario, the associated struct was fully resolved by a 
	 *       subsequent call to the same function (for the same linker/struct).
	 * 
	 * @param  Linker	The linker you want to check.
	 * @return True if the specified linker is in the midst of an unfinished ResolveDeferredDependencies() call (otherwise false).
	 */
	static bool IsAssociatedStructUnresolved(const FLinkerLoad* Linker)
	{
		FScopeLock UnresolvedStructsLock(&UnresolvedStructsCritical);
		for (UObject* UnresolvedObj : UnresolvedStructs)
		{
			// each unresolved struct should have a linker set on it, because 
			// they would have had to go through Preload()
			if (UnresolvedObj->GetLinker() == Linker)
			{
				return true;
			}
		}
		return false;
	}

	/**  */
	static void Reset(const FLinkerLoad* Linker)
	{
		FScopeLock UnresolvedStructsLock(&UnresolvedStructsCritical);
		TArray<UObject*> ToRemove;
		for (UObject* UnresolvedObj : UnresolvedStructs)
		{
			if (UnresolvedObj->GetLinker() == Linker)
			{
				ToRemove.Add(UnresolvedObj);
			}
		}
		for (UObject* ResetingObj : ToRemove)
		{
			UnresolvedStructs.Remove(ResetingObj);
		}
	}

private:
	/** The struct that is currently being "resolved" */
	UStruct* TrackedStruct;

	/** 
	 * A set of blueprint structs & classes that are currently being "resolved"  
	 * by ResolveDeferredDependencies() (using UObject* instead of UStruct, so
	 * we don't have to cast import objects before checking for their presence).
	 */
	static TSet<UObject*> UnresolvedStructs;
	static FCriticalSection UnresolvedStructsCritical;
};
/** A global set that tracks structs currently being ran through (and unfinished by) FLinkerLoad::ResolveDeferredDependencies() */
TSet<UObject*> FUnresolvedStructTracker::UnresolvedStructs;
FCriticalSection FUnresolvedStructTracker::UnresolvedStructsCritical;

UPackage* LoadPackageInternal(UPackage* InOuter, const TCHAR* InLongPackageName, uint32 LoadFlags, FLinkerLoad* ImportLinker);

void FLinkerLoad::ResolveDeferredDependencies(UStruct* LoadStruct)
{
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	//--------------------------------------
	// Phase 2: Resolve Dependency Stubs
	//--------------------------------------
	TGuardValue<uint32> LoadFlagsGuard(LoadFlags, (LoadFlags & ~LOAD_DeferDependencyLoads));

#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
	static int32 RecursiveDepth = 0;
	int32 const MeasuredDepth = RecursiveDepth;
	TGuardValue<int32> RecursiveDepthGuard(RecursiveDepth, RecursiveDepth + 1);

	DEFERRED_DEPENDENCY_CHECK( (LoadStruct != nullptr) && (LoadStruct->GetLinker() == this) );
	DEFERRED_DEPENDENCY_CHECK( LoadStruct->HasAnyFlags(RF_LoadCompleted) );
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS

	// scoped block to manage the lifetime of ScopedResolveTracker, so that 
	// this resolve is only tracked for the duration of resolving all its 
	// placeholder classes, all member struct's placholders, and its parent's
	{
		FUnresolvedStructTracker ScopedResolveTracker(LoadStruct);

		// this function (for this linker) could be reentrant (see where we 
		// recursively call ResolveDeferredDependencies() for super-classes below);  
		// if that's the case, then we want to finish resolving the pending class 
		// before we continue on
		if (ResolvingDeferredPlaceholder != nullptr)
		{
			int32 ResolvedRefCount = ResolveDependencyPlaceholder(ResolvingDeferredPlaceholder, Cast<UClass>(LoadStruct));
			// @TODO: can we reliably count on this resolving some dependencies?... 
			//        if so, check verify that!
			ResolvingDeferredPlaceholder = nullptr;
		}

		// because this loop could recurse (and end up finishing all of this for
		// us), we check HasUnresolvedDependencies() so we can early out  
		// from this loop in that situation (the loop has been finished elsewhere)
		for (int32 ImportIndex = 0; ImportIndex < ImportMap.Num() && HasUnresolvedDependencies(); ++ImportIndex)
		{
			FObjectImport& Import = ImportMap[ImportIndex];

			const FLinkerLoad* SourceLinker = (Import.SourceLinker != nullptr) ? Import.SourceLinker :
				(Import.XObject != nullptr) ? Import.XObject->GetLinker() : nullptr;

			const UPackage* SourcePackage = (SourceLinker != nullptr) ? SourceLinker->LinkerRoot : nullptr;
			// this package may not have introduced any (possible) cyclic 
			// dependencies, but it still could have been deferred (kept from
			// fully loading... we need to make sure metadata gets loaded, etc.)
			if ((SourcePackage != nullptr) && !SourcePackage->HasAnyFlags(RF_WasLoaded))
			{
				uint32 InternalLoadFlags = LoadFlags & (LOAD_NoVerify | LOAD_NoWarn | LOAD_Quiet);
				// make sure LoadAllObjects() is called for this package
				LoadPackageInternal(/*Outer =*/nullptr, *SourceLinker->Filename, InternalLoadFlags, this);
			}

			if (ULinkerPlaceholderClass* PlaceholderClass = Cast<ULinkerPlaceholderClass>(Import.XObject))
			{
				DEFERRED_DEPENDENCY_CHECK(PlaceholderClass->PackageIndex.ToImport() == ImportIndex);

				// NOTE: we don't check that this resolve successfully replaced any
				//       references (by the return value), because this resolve 
				//       could have been re-entered and completed by a nested call
				//       to the same function (for the same placeholder)
				ResolveDependencyPlaceholder(PlaceholderClass, Cast<UClass>(LoadStruct));
			}
			else if (ULinkerPlaceholderFunction* PlaceholderFunction = Cast<ULinkerPlaceholderFunction>(Import.XObject))
			{
				if (ULinkerPlaceholderClass* PlaceholderOwner = Cast<ULinkerPlaceholderClass>(PlaceholderFunction->GetOwnerClass()))
				{
					ResolveDependencyPlaceholder(PlaceholderOwner, Cast<UClass>(LoadStruct));
				}

				DEFERRED_DEPENDENCY_CHECK(PlaceholderFunction->PackageIndex.ToImport() == ImportIndex);
				ResolveDependencyPlaceholder(PlaceholderFunction, Cast<UClass>(LoadStruct)); 
			}
			else if (UScriptStruct* StructObj = Cast<UScriptStruct>(Import.XObject))
			{
				// in case this is a user defined struct, we have to resolve any 
				// deferred dependencies in the struct 
				if (Import.SourceLinker != nullptr)
				{
					Import.SourceLinker->ResolveDeferredDependencies(StructObj);
				}
			}
		}

		if (UStruct* SuperStruct = LoadStruct->GetSuperStruct())
		{
			FLinkerLoad* SuperLinker = SuperStruct->GetLinker();
			// NOTE: there is no harm in calling this when the super is not 
			//       "actively resolving deferred dependencies"... this condition  
			//       just saves on wasted ops, looping over the super's ImportMap
			if ((SuperLinker != nullptr) && SuperLinker->HasUnresolvedDependencies())
			{
				// a resolve could have already been started up the stack, and in turn  
				// started loading a different package that resulted in another (this) 
				// resolve beginning... in that scenario, the original resolve could be 
				// for this class's super and we want to make sure that finishes before
				// we regenerate this class (else the generated script code could end up 
				// with unwanted placeholder references; ones that would have been
				// resolved by the super's linker)
				SuperLinker->ResolveDeferredDependencies(SuperStruct);
			}
		}

	// close the scope on ScopedResolveTracker (so LoadClass doesn't appear to 
	// be resolving through the rest of this function)
	}

	// @TODO: don't know if we need this, but could be good to have (as class 
	//        regeneration is about to force load a lot of this), BUT! this 
	//        doesn't work for map packages (because this would load the level's
	//        ALevelScriptActor instance BEFORE the class has been regenerated)
	//LoadAllObjects();

#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
	for (TObjectIterator<ULinkerPlaceholderClass> PlaceholderIt; PlaceholderIt && !FBlueprintSupport::IsResolvingDeferredDependenciesDisabled(); ++PlaceholderIt)
	{
		ULinkerPlaceholderClass* PlaceholderClass = *PlaceholderIt;
		if (PlaceholderClass->GetOuter() == LinkerRoot)
		{
			// there shouldn't be any deferred dependencies (belonging to this 
			// linker) that need to be resolved by this point
			DEFERRED_DEPENDENCY_CHECK(!PlaceholderClass->HasKnownReferences());
		}
	}
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
}

bool FLinkerLoad::HasUnresolvedDependencies() const
{
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	// checking (ResolvingDeferredPlaceholder != nullptr) is not sufficient, 
	// because the linker could be in the midst of a nested resolve (for a 
	// struct, or super... see FLinkerLoad::ResolveDeferredDependencies)
	bool bIsClassExportUnresolved = FUnresolvedStructTracker::IsAssociatedStructUnresolved(this);

	// (ResolvingDeferredPlaceholder != nullptr) should imply 
	// bIsClassExportUnresolved is true (but not the other way around)
	DEFERRED_DEPENDENCY_CHECK((ResolvingDeferredPlaceholder == nullptr) || bIsClassExportUnresolved);
	
	return bIsClassExportUnresolved;

#else  // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	return false;
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
}

int32 FLinkerLoad::ResolveDependencyPlaceholder(FLinkerPlaceholderBase* PlaceholderIn, UClass* ReferencingClass)
{
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
	if (FBlueprintSupport::IsResolvingDeferredDependenciesDisabled())
	{
		return 0;
	}
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS

	TGuardValue<uint32>  LoadFlagsGuard(LoadFlags, (LoadFlags & ~LOAD_DeferDependencyLoads));
	TGuardValue<FLinkerPlaceholderBase*> ResolvingClassGuard(ResolvingDeferredPlaceholder, PlaceholderIn);

	UObject* PlaceholderObj = PlaceholderIn->GetPlaceholderAsUObject();
	DEFERRED_DEPENDENCY_CHECK(PlaceholderObj != nullptr);
	DEFERRED_DEPENDENCY_CHECK(PlaceholderObj->GetOutermost() == LinkerRoot);
	DEFERRED_DEPENDENCY_CHECK(PlaceholderIn->PackageIndex.IsImport());
	
	int32 const ImportIndex = PlaceholderIn->PackageIndex.ToImport();
	FObjectImport& Import = ImportMap[ImportIndex];
	
	UObject* RealImportObj = nullptr;
	if ((Import.XObject != nullptr) && (Import.XObject != PlaceholderObj))
	{
		DEFERRED_DEPENDENCY_CHECK(ResolvingDeferredPlaceholder == PlaceholderIn);
		RealImportObj = Import.XObject;
	}
	else 
	{
		// clear the placeholder from the import, so that a call to CreateImport()
		// properly fills it in
		Import.XObject = nullptr;
		// NOTE: this is a possible point of recursion... CreateImport() could 
		//       continue to load a package already started up the stack and you 
		//       could end up in another ResolveDependencyPlaceholder() for some  
		//       other placeholder before this one has completely finished resolving
		RealImportObj = CreateImport(ImportIndex);
	}

#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
	UFunction* AsFunction = Cast<UFunction>(RealImportObj);
	// it's ok if super functions come in not fully loaded (missing 
	// RF_LoadCompleted... meaning it's in the middle of serializing in somewhere 
	// up the stack); the function will be forcefully ran through Preload(), 
	// when we regenerate the super class (see FRegenerationHelper::ForcedLoadMembers)
	bool const bIsSuperFunction = (AsFunction != nullptr) && (ReferencingClass != nullptr) && ReferencingClass->IsChildOf(AsFunction->GetOwnerClass());

	DEFERRED_DEPENDENCY_CHECK(RealImportObj != PlaceholderObj);
	DEFERRED_DEPENDENCY_CHECK(RealImportObj == nullptr || bIsSuperFunction || RealImportObj->HasAnyFlags(RF_LoadCompleted));
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS

	int32 ReplacementCount = 0;
	if (ReferencingClass != nullptr)
	{
		// @TODO: roll this into ULinkerPlaceholderClass's ResolveAllPlaceholderReferences()
		for (FImplementedInterface& Interface : ReferencingClass->Interfaces)
		{
			if (Interface.Class == PlaceholderObj)
			{
				++ReplacementCount;
				Interface.Class = CastChecked<UClass>(RealImportObj, ECastCheckedType::NullAllowed);
			}
		}
	}

	// make sure that we know what utilized this placeholder class... right now
	// we only expect UObjectProperties/UClassProperties/UInterfaceProperties/
	// FImplementedInterfaces to, but something else could have requested the 
	// class without logging itself with the placeholder... if the placeholder
	// doesn't have any known references (and it hasn't already been resolved in
	// some recursive call), then there is something out there still using this
	// placeholder class
	DEFERRED_DEPENDENCY_CHECK( (ReplacementCount > 0) || PlaceholderIn->HasKnownReferences() || PlaceholderIn->HasBeenFullyResolved() );

	ReplacementCount += PlaceholderIn->ResolveAllPlaceholderReferences(RealImportObj);

#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
	// @TODO: not an actual method, but would be nice to circumvent the need for bIsAsyncLoadRef below
	//FAsyncObjectsReferencer::Get().RemoveObject(PlaceholderObj);

	// there should not be any references left to this placeholder class 
	// (if there is, then we didn't log that referencer with the placeholder)
	FReferencerInformationList UnresolvedReferences;
	bool const bIsReferenced = false;// IsReferenced(PlaceholderObj, RF_NoFlags, /*bCheckSubObjects =*/false, &UnresolvedReferences);

	// when we're running with async loading there may be an acceptable 
	// reference left in FAsyncObjectsReferencer (which reports its refs  
	// through FGCObject::GGCObjectReferencer)... since garbage collection can  
	// be ran during async loading, FAsyncObjectsReferencer is in charge of  
	// holding onto objects that are spawned during the process (to ensure 
	// they're not thrown away prematurely)
	bool const bIsAsyncLoadRef = (UnresolvedReferences.ExternalReferences.Num() == 1) &&
		PlaceholderObj->HasAnyFlags(RF_AsyncLoading) && (UnresolvedReferences.ExternalReferences[0].Referencer == FGCObject::GGCObjectReferencer);

	DEFERRED_DEPENDENCY_CHECK(!bIsReferenced || bIsAsyncLoadRef);
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS


	return ReplacementCount;
#else  // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	return 0;
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
}

void FLinkerLoad::FinalizeBlueprint(UClass* LoadClass)
{
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	if (!FBlueprintSupport::UseDeferredDependencyLoading())
	{
		return;
	}
	DEFERRED_DEPENDENCY_CHECK(LoadClass->HasAnyFlags(RF_LoadCompleted));

	//--------------------------------------
	// Phase 3: Finalize (serialize CDO & regenerate class)
	//--------------------------------------
	TGuardValue<uint32> LoadFlagsGuard(LoadFlags, LoadFlags & ~LOAD_DeferDependencyLoads);

	// we can get in a state where a sub-class is getting finalized here, before
	// its super-class has been "finalized" (like when the super's 
	// ResolveDeferredDependencies() ends up Preloading a sub-class), so we
	// want to make sure that its properly finalized before this sub-class is
	// (so we can have a properly formed sub-class)
	if (UClass* SuperClass = LoadClass->GetSuperClass())
	{
		FLinkerLoad* SuperLinker = SuperClass->GetLinker();
		if ((SuperLinker != nullptr) && SuperLinker->IsBlueprintFinalizationPending())
		{
			SuperLinker->FinalizeBlueprint(SuperClass);
		}
	}

	// at this point, we're sure that LoadClass doesn't contain any class 
	// placeholders (because ResolveDeferredDependencies() was ran on it); 
	// however, once we get to regenerating/compiling the blueprint, the graph 
	// (nodes, pins, etc.) could bring in new dependencies that weren't part of 
	// the class... this would normally be all fine and well, but in complicated 
	// dependency chains those graph-dependencies could already be in the middle 
	// of resolving themselves somewhere up the stack... if we just continue  
	// along and let the blueprint compile, then it could end up with  
	// placeholder refs in its script code (which it bad); we need to make sure
	// that all dependencies don't have any placeholder classes left in them
	// 
	// we don't want this to be part of ResolveDeferredDependencies() 
	// because we don't want this to count as a linker's "class resolution 
	// phase"; at this point, it is ok if other blueprints compile with refs to  
	// this LoadClass since it doesn't have any placeholders left in it (we also 
	// don't want this linker externally claiming that it has resolving left to 
	// do, otherwise other linkers could want to finish this off when they don't
	// have to)... we do however need it here in FinalizeBlueprint(), because
	// we need it ran for any super-classes before we regen
	for (int32 ImportIndex = 0; ImportIndex < ImportMap.Num() && IsBlueprintFinalizationPending(); ++ImportIndex)
	{
		// first, make sure every import object is available... just because 
		// it isn't present in the map already, doesn't mean it isn't in the 
		// middle of a resolve (the CreateImport() brings in an export 
		// object from another package, which could be resolving itself)... 
		// 
		// don't fret, all these imports were bound to get created sooner or 
		// later (like when the blueprint was regenerated)
		//
		// NOTE: this is a possible root point for recursion... accessing a 
		//       separate package could continue its loading process which
		//       in turn, could end us back in this function before we ever  
		//       returned from this
		FObjectImport& Import = ImportMap[ImportIndex];
		UObject* ImportObject = CreateImport(ImportIndex);

		// see if this import is currently being resolved (presumably somewhere 
		// up the callstack)... if it is, we need to ensure that this dependency 
		// is fully resolved before we get to regenerating the blueprint (else,
		// we could end up with placeholder classes in our script-code)
		if (FUnresolvedStructTracker::IsImportStructUnresolved(ImportObject))
		{
			// because it is tracked by FUnresolvedStructTracker, it must be a struct
			DEFERRED_DEPENDENCY_CHECK(Cast<UStruct>(ImportObject) != nullptr);
			auto SourceLinker = FindExistingLinkerForImport(ImportIndex);
			if (SourceLinker)
			{
				SourceLinker->ResolveDeferredDependencies((UStruct*)ImportObject);
			}
		}
	}

	// the above loop could have caused some recursion... if it ended up 
	// finalizing a sub-class (of LoadClass), then that would have finalized
	// this as well; so, before we continue, make sure that this didn't already 
	// get fully executed in some nested call
	if (IsBlueprintFinalizationPending())
	{
		ResolveDeferredExports(LoadClass);

		FObjectExport& CDOExport = ExportMap[DeferredCDOIndex];
		// clear this so IsBlueprintFinalizationPending() doesn't report true
		DeferredCDOIndex = INDEX_NONE;

#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
		// at this point there should not be any instances of the Blueprint 
		// (else, we'd have to re-instance and that is too expensive an 
		// operation to have at load time)
		TArray<UObject*> ClassInstances;
		GetObjectsOfClass(LoadClass, ClassInstances, /*bIncludeDerivedClasses =*/true);

		for (UObject* ClassInst : ClassInstances)
		{
			// in the case that we do end up with instances, use this to find 
			// where they are referenced (to help sleuth out when/where they 
			// were created)
			FReferencerInformationList InstanceReferences;
			bool const bIsReferenced = false;// IsReferenced(ClassInst, RF_NoFlags, /*bCheckSubObjects =*/false, &InstanceReferences);
			DEFERRED_DEPENDENCY_CHECK(!bIsReferenced);
		}
		DEFERRED_DEPENDENCY_CHECK(ClassInstances.Num() == 0);

		UClass* BlueprintClass = Cast<UClass>(IndexToObject(CDOExport.ClassIndex));
		DEFERRED_DEPENDENCY_CHECK(BlueprintClass == LoadClass);
		DEFERRED_DEPENDENCY_CHECK(BlueprintClass->HasAnyClassFlags(CLASS_CompiledFromBlueprint));
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS

		// for cooked builds (we skip script serialization for editor builds), 
		// certain scripts can contain references to external dependencies; and 
		// since the script is serialized in with the class (functions) we want 
		// those dependencies deferred until now (we expect this to be the right 
		// spot, because in editor builds, with RegenerateBlueprintClass(), this 
		// is where script code is regenerated)
		FStructScriptLoader::ResolveDeferredScriptLoads(this);

		DEFERRED_DEPENDENCY_CHECK(LoadClass->GetOutermost() != GetTransientPackage());
		// just in case we choose to enable the deferred dependency loading for 
		// cooked builds... we want to keep from regenerating in that scenario
		if (!LoadClass->bCooked)
		{
			UObject* OldCDO = LoadClass->ClassDefaultObject;
			if (RegenerateBlueprintClass(LoadClass, CDOExport.Object))
			{
				// emulate class CDO serialization (RegenerateBlueprintClass() could 
				// have a side-effect where it overwrites the class's CDO; so we 
				// want to make sure that we don't overwrite that new CDO with a 
				// stale one)
				if (OldCDO == LoadClass->ClassDefaultObject)
				{
					LoadClass->ClassDefaultObject = CDOExport.Object;
				}
			}
		}
	}
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
}

void FLinkerLoad::ResolveDeferredExports(UClass* LoadClass)
{
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING

	DEFERRED_DEPENDENCY_CHECK(DeferredCDOIndex != INDEX_NONE);
	FObjectExport& CDOExport = ExportMap[DeferredCDOIndex];

	UObject* BlueprintCDO = CDOExport.Object;
	DEFERRED_DEPENDENCY_CHECK(BlueprintCDO != nullptr);

#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
	static TSet<FLinkerLoad*> ResolvingExportsTracker;
	// the first half of this function CANNOT be re-entrant (specifically for 
	// this linker)
	//
	// if this ends up being a problem, then we have two blueprints that 
	// rely on an instances of each other... this is an unexpected problem, and 
	// we'll have to re-instance on load (an operation we'd rather avoid,  
	// because of how slow it is)
	DEFERRED_DEPENDENCY_CHECK(!ResolvingExportsTracker.Contains(this));
	ResolvingExportsTracker.Add(this);
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS

	if (!FBlueprintSupport::IsDeferredExportCreationDisabled())
	{
		// we may have circumvented an export creation or two to avoid instantiating
		// an BP object before its class has been finalized (to avoid costly re-
		// instancing operations when the class ultimately finalizes)... so here, we
		// find those skipped exports and properly create them (before we finalize 
		// our own class)
		for (int32 ExportIndex = 0; ExportIndex < ExportMap.Num(); ++ExportIndex)
		{
			FObjectExport& Export = ExportMap[ExportIndex];
			if (ULinkerPlaceholderExportObject* PlaceholderExport = Cast<ULinkerPlaceholderExportObject>(Export.Object))
			{
				DEFERRED_DEPENDENCY_CHECK(Export.ClassIndex.IsImport());
				UClass* ExportClass = GetExportLoadClass(ExportIndex);
				DEFERRED_DEPENDENCY_CHECK((ExportClass != nullptr) && !ExportClass->HasAnyClassFlags(CLASS_Intrinsic) && ExportClass->HasAnyClassFlags(CLASS_CompiledFromBlueprint));
				FLinkerLoad* ClassLinker = ExportClass->GetLinker();
				DEFERRED_DEPENDENCY_CHECK((ClassLinker != nullptr) && (ClassLinker != this));

				{
					// make sure we're not already in ForceRegenerateClass() for
					// this export (that could cause some bad infinite recursion)
					DEFERRED_DEPENDENCY_CHECK(!IsExportBeingResolved(ExportIndex));
					FScopedResolvingExportTracker ForceRegenGuard(this, ExportIndex);

					// make sure this export's class is fully regenerated before  
					// we instantiate it (so we don't have to re-inst on load)
					ForceRegenerateClass(ExportClass);
				}

				// replace the placeholder with the proper object instance
				Export.Object = nullptr;
				UObject* ExportObj = CreateExport(ExportIndex);

				// NOTE: we don't count how many references were resolved (and 
				//       assert on it), because this could have only been created as 
				//       part of the LoadAllObjects() pass (not for any specific 
				//       container object).
				PlaceholderExport->ResolveAllPlaceholderReferences(ExportObj);
				PlaceholderExport->MarkPendingKill();

				// if we hadn't used a ULinkerPlaceholderExportObject in place of 
				// the expected export, then someone may have wanted it preloaded
				Preload(ExportObj);

#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
				FReferencerInformationList UnresolvedReferences;
				UObject* PlaceholderObj = PlaceholderExport;
				bool const bIsReferenced = false;// IsReferenced(PlaceholderObj, RF_NoFlags, /*bCheckSubObjects =*/false, &UnresolvedReferences);
				DEFERRED_DEPENDENCY_CHECK(!bIsReferenced);
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
			}
		}
	}

#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
	// this helps catch any placeholder export objects that may be created 
	// between now and when DeferredCDOIndex is cleared (they won't be resolved,
	// so that is a problem!)
	FResolvingExportTracker::Get().FlagFullExportResolvePassComplete(this);
	// we can be re-entrant from here on (in-fact, we expect to with CDO 
	// serialization)
	ResolvingExportsTracker.Remove(this);

	for (TObjectIterator<ULinkerPlaceholderExportObject> PlaceholderIt; PlaceholderIt; ++PlaceholderIt)
	{
		ULinkerPlaceholderExportObject* PlaceholderObj = *PlaceholderIt;
		DEFERRED_DEPENDENCY_CHECK((PlaceholderObj->GetOuter() != LinkerRoot) || PlaceholderObj->IsPendingKill());
	}

	if (!FBlueprintSupport::IsDeferredCDOSerializationDisabled())
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
	{
		// NOTE: this doesn't work... mainly because of how we handle the case  
		//       where SuperClass->HasAnyClassFlags(CLASS_NewerVersionExists) is 
		//       true; at a surface level it works, up until GC is ran (the old
		//       CDO is still tied to the active class, and the CDO's 
		//       DestroyNonNativeProperties() attempts to call the class's
		//       DestroyPersistentUberGraphFrame()... this is why 
		//       ResolveDeferredSubClassObjects() was introduced instead
// 		UClass* SuperClass = LoadClass->GetSuperClass();
// 		// if this class's CDO had its initialization deferred (presumably 
// 		// because its super's CDO hadn't been fully serialized), then we want 
// 		// to make sure it is ran here before we serialize in the CDO (so it 
// 		// gets stocked with inherited values first, that the Preload() may or
// 		// may not override)
// 		if (!FPlatformProperties::RequiresCookedData() && FDeferredObjInitializerTracker::IsCdoDeferred(LoadClass))
// 		{
// 			// if LoadClass's super has been regenerated (presumably since we 
// 			// deferred its CDO initialization), then we need to throw out the
// 			// CDO and recreate a new one... we do this because the super's
// 			// layout could have changed (and property offsets are now different)
// 			// for the deferred initialization to work, inherited property 
// 			// offsets have to match (FObjectInitializer uses one property to 
// 			// copy values between the CDOs)
// 			if (SuperClass->HasAnyClassFlags(CLASS_NewerVersionExists))
// 			{
// 				// reset the class's super and relink (so that the class's 
// 				// property chain reflects the super's regenerated layout)
// 				UClass* RegenedSuperClass = SuperClass->GetAuthoritativeClass();
// 				LoadClass->SetSuperStruct(RegenedSuperClass);
// 				LoadClass->StaticLink(/*bRelinkExistingProperties =*/true);
// 				SuperClass = RegenedSuperClass;
// 
// 				UObject* OldCDO = BlueprintCDO;
// 				DEFERRED_DEPENDENCY_CHECK(LoadClass->GetDefaultObject(/*bCreateIfNeeded =*/false) == BlueprintCDO);
// 
// 				// we have to move the OldCDO out of its package, otherwise 
// 				// when we attempt to create a new one StaticAllocateObject() 
// 				// will just return us this stale one
// 				UPackage* TransientPackage = GetTransientPackage();
// 				const FName TransientName = MakeUniqueObjectName(TransientPackage, LoadClass, *FString::Printf(TEXT("CYCLIC_%s"), *OldCDO->GetName()));
// 				const ERenameFlags RenameFlags = (REN_DontCreateRedirectors | REN_ForceNoResetLoaders | REN_NonTransactional | REN_DoNotDirty);
// 				OldCDO->Rename(*TransientName.ToString(), TransientPackage, RenameFlags);
// 
// 				// have to reset the LOAD_DeferDependencyLoads flag so that 
// 				// CreateExport() doesn't try to regenerate the Blueprint on the spot
// 				const uint32 DeferDependencyFlag = (LoadFlags & LOAD_DeferDependencyLoads);
// 				LoadFlags |= LOAD_DeferDependencyLoads;
// 				
// 				// recreate the CDO from scratch (going through CreateExport() instead
// 				// of GetDefaultObject() so that it gets properly marked with RF_NeedLoad, etc.)
// 				LoadClass->ClassDefaultObject = nullptr;
// 				CDOExport.Object = nullptr;
// 				UObject* NewCDO = CreateExport(DeferredCDOIndex);
// 				DEFERRED_DEPENDENCY_CHECK(NewCDO != OldCDO);
// 				LoadClass->ClassDefaultObject = NewCDO;
// 
// 				// restore (clear) the LOAD_DeferDependencyLoads flag
// 				LoadFlags = (LoadFlags & ~LOAD_DeferDependencyLoads) | DeferDependencyFlag;
// 
// 				BlueprintCDO = NewCDO;
// 				// NOTE: since we're recreating the CDO, CreateExport() will take care of initializting
// 				//       it for us (the cached FObjectInitializer is no longer needed), but we do still
// 				//       need to load (and re-parent) deferred sub-objects
// 				FDeferredObjInitializerTracker::ResolveDeferredSubObjects(NewCDO);
// 				FDeferredObjInitializerTracker::Remove(LoadClass);
// 			}
// 			else
// 			{
// 				FDeferredObjInitializerTracker::ResolveDeferredInitialization(LoadClass);
// 			}
// 		}
// 		else
// 		{
//  			DEFERRED_DEPENDENCY_CHECK(!SuperClass->HasAnyClassFlags(CLASS_NewerVersionExists));
//  			FDeferredObjInitializerTracker::ResolveDeferredInitialization(LoadClass);
// 		}

		// have to prematurely set the CDO's linker so we can force a Preload()/
		// Serialization of the CDO before we regenerate the Blueprint class
		{
			const EObjectFlags OldFlags = BlueprintCDO->GetFlags();
			BlueprintCDO->ClearFlags(RF_NeedLoad | RF_NeedPostLoad);
			BlueprintCDO->SetLinker(this, DeferredCDOIndex, /*bShouldDetatchExisting =*/false);
			BlueprintCDO->SetFlags(OldFlags);
		}
		DEFERRED_DEPENDENCY_CHECK(BlueprintCDO->GetClass() == LoadClass);

		// should load the CDO (ensuring that it has been serialized in by the 
		// time we get to class regeneration)
		//
		// NOTE: this is point where circular dependencies could reveal 
		//       themselves, as the CDO could depend on a class not listed in 
		//       the package's imports
		//
		// NOTE: how we don't guard against re-entrant behavior... if the CDO 
		//       has already been "finalized", then its RF_NeedLoad flag would 
		//       be cleared (and this will do nothing the 2nd time around)
		Preload(BlueprintCDO);

		// sub-classes of this Blueprint could have had their CDO's 
		// initialization deferred (this occurs when the sub-class CDO is 
		// created before this super CDO has been fully serialized; we do this
		// because the  sub-class's CDO would not have been initialized with 
		// accurate values)
		//
		// in that case, the sub-class CDOs are waiting around until their 
		// super CDO is fully loaded (which is now)... we want to do this here, 
		// before this (super) Blueprint gets regenerated, because after it's
		// regenerated the class layout (and property offsets) may no longer 
		// match the layout that sub-class CDOs were constructed with (making 
		// property copying dangerous)
		FDeferredObjInitializerTracker::ResolveDeferredSubClassObjects(LoadClass);

		DEFERRED_DEPENDENCY_CHECK(BlueprintCDO->HasAnyFlags(RF_LoadCompleted));
	}
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
}

bool FLinkerLoad::IsBlueprintFinalizationPending() const
{
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	return (DeferredCDOIndex != INDEX_NONE);
#else  // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	return false;
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
}

bool FLinkerLoad::ForceRegenerateClass(UClass* ImportClass)
{
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	if (FLinkerLoad* ClassLinker = ImportClass->GetLinker())
	{
		//
		// BE VERY CAREFUL with this! if these following statements are called 
		// in the wrong place, we could end up infinitely recursing

		Preload(ImportClass);
		DEFERRED_DEPENDENCY_CHECK(ImportClass->HasAnyFlags(RF_LoadCompleted));

		if (ClassLinker->HasUnresolvedDependencies())
		{
			ClassLinker->ResolveDeferredDependencies(ImportClass);
		}
		if (ClassLinker->IsBlueprintFinalizationPending())
		{
			ClassLinker->FinalizeBlueprint(ImportClass);
		}
		return true;
	}
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	return false;
}

bool FLinkerLoad::IsExportBeingResolved(int32 ExportIndex)
{
	FObjectExport& Export = ExportMap[ExportIndex];
	bool bIsExportClassBeingForceRegened = FResolvingExportTracker::Get().IsLinkerExportBeingResolved(this, ExportIndex);

	FPackageIndex OuterIndex = Export.OuterIndex;
	// since child exports require their outers be set upon creation, then those 
	// too count as being "resolved"... so here we check this export's outers too
	while (!bIsExportClassBeingForceRegened && !OuterIndex.IsNull())
	{
		DEFERRED_DEPENDENCY_CHECK(OuterIndex.IsExport());
		int32 OuterExportIndex = OuterIndex.ToExport();

		if (OuterExportIndex != INDEX_NONE)
		{
			FObjectExport& OuterExport = ExportMap[OuterExportIndex];
			bIsExportClassBeingForceRegened |= FResolvingExportTracker::Get().IsLinkerExportBeingResolved(this, OuterExportIndex);

			OuterIndex = OuterExport.OuterIndex;
		}
		else
		{
			break;
		}
	}
	return bIsExportClassBeingForceRegened;
}

bool FLinkerLoad::HasPerformedFullExportResolvePass()
{
#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
	return FResolvingExportTracker::Get().HasPerformedFullExportResolvePass(this);
#else 
	return false;
#endif
	
}

void FLinkerLoad::ResetDeferredLoadingState()
{
#if USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
	DeferredCDOIndex = INDEX_NONE;
	ResolvingDeferredPlaceholder = nullptr;
	LoadFlags &= ~(LOAD_DeferDependencyLoads);

	FResolvingExportTracker::Get().Reset(this);
	FUnresolvedStructTracker::Reset(this);
#endif // USE_CIRCULAR_DEPENDENCY_LOAD_DEFERRING
}

/*******************************************************************************
 * UObject
 ******************************************************************************/

/** 
 * Returns whether this object is contained in or part of a blueprint object
 */
bool UObject::IsInBlueprint() const
{
	// Exclude blueprint classes as they may be regenerated at any time
	// Need to exclude classes, CDOs, and their subobjects
	const UObject* TestObject = this;
 	while (TestObject)
 	{
 		const UClass *ClassObject = dynamic_cast<const UClass*>(TestObject);
		if (ClassObject 
			&& ClassObject->HasAnyClassFlags(CLASS_CompiledFromBlueprint) 
			&& ClassObject->ClassGeneratedBy)
 		{
 			return true;
 		}
		else if (TestObject->HasAnyFlags(RF_ClassDefaultObject) 
			&& TestObject->GetClass() 
			&& TestObject->GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint) 
			&& TestObject->GetClass()->ClassGeneratedBy)
 		{
 			return true;
 		}
 		TestObject = TestObject->GetOuter();
 	}

	return false;
}

/** 
 *  Destroy properties that won't be destroyed by the native destructor
 */
void UObject::DestroyNonNativeProperties()
{
	// Destroy properties that won't be destroyed by the native destructor
#if USE_UBER_GRAPH_PERSISTENT_FRAME
	GetClass()->DestroyPersistentUberGraphFrame(this);
#endif
	{
		for (UProperty* P = GetClass()->DestructorLink; P; P = P->DestructorLinkNext)
		{
			P->DestroyValue_InContainer(this);
		}
	}
}

/*******************************************************************************
 * FObjectInitializer
 ******************************************************************************/

/** 
 * Initializes a non-native property, according to the initialization rules. If the property is non-native
 * and does not have a zero constructor, it is initialized with the default value.
 * @param	Property			Property to be initialized
 * @param	Data				Default data
 * @return	Returns true if that property was a non-native one, otherwise false
 */
bool FObjectInitializer::InitNonNativeProperty(UProperty* Property, UObject* Data)
{
	if (!Property->GetOwnerClass()->HasAnyClassFlags(CLASS_Native | CLASS_Intrinsic)) // if this property belongs to a native class, it was already initialized by the class constructor
	{
		if (!Property->HasAnyPropertyFlags(CPF_ZeroConstructor)) // this stuff is already zero
		{
			Property->InitializeValue_InContainer(Data);
		}
		return true;
	}
	else
	{
		// we have reached a native base class, none of the rest of the properties will need initialization
		return false;
	}
}

/*******************************************************************************
 * FDeferredObjInitializerTracker
 ******************************************************************************/

FObjectInitializer* FDeferredObjInitializerTracker::Add(const FObjectInitializer& DeferringInitializer)
{
	UObject* InitingObj = DeferringInitializer.GetObj();
	DEFERRED_DEPENDENCY_CHECK((InitingObj != nullptr) && InitingObj->HasAnyFlags(RF_ClassDefaultObject));

	UClass* LoadClass = (InitingObj != nullptr) ? InitingObj->GetClass() : nullptr;
	if (LoadClass != nullptr)
	{
		FDeferredObjInitializerTracker& ThreadInst = FDeferredObjInitializerTracker::Get();
		auto& SuperClassMap = ThreadInst.SuperClassMap;

		UClass* SuperClass = LoadClass->GetSuperClass();
		SuperClassMap.AddUnique(SuperClass, LoadClass);
#if USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
		UObject* SuperCDO = SuperClass->GetDefaultObject(/*bCreateIfNeeded =*/false);
		DEFERRED_DEPENDENCY_CHECK( SuperCDO && (SuperCDO->HasAnyFlags(RF_NeedLoad) || SuperCDO->HasAnyFlags(RF_LoadCompleted) || IsCdoDeferred(SuperClass)) );
#endif // USE_DEFERRED_DEPENDENCY_CHECK_VERIFICATION_TESTS
		
		auto& InitializersCache = ThreadInst.DeferredInitializers;
		DEFERRED_DEPENDENCY_CHECK(InitializersCache.Find(LoadClass) == nullptr); // did we try to init the CDO twice?
		DEFERRED_DEPENDENCY_CHECK(LoadClass->GetLinker()->LoadFlags & LOAD_DeferDependencyLoads);

		// NOTE: we copy the FObjectInitializer, because it is most likely being destroyed
		FObjectInitializer& DeferredCopy = InitializersCache.Add(LoadClass, DeferringInitializer);

		return &DeferredCopy;
	}
	return nullptr;
}

FObjectInitializer* FDeferredObjInitializerTracker::Find(UClass* LoadClass)
{
	FDeferredObjInitializerTracker& ThreadInst = FDeferredObjInitializerTracker::Get();
	return ThreadInst.DeferredInitializers.Find(LoadClass);
}

bool FDeferredObjInitializerTracker::IsCdoDeferred(UClass* LoadClass)
{
	return (Find(LoadClass) != nullptr);
}

bool FDeferredObjInitializerTracker::DeferSubObjectPreload(UObject* SubObject)
{
	DEFERRED_DEPENDENCY_CHECK(SubObject->HasAnyFlags(RF_DefaultSubObject));

	UObject* CdoOuter = SubObject->GetOuter();
	UClass* OuterClass = CdoOuter->GetClass();

	FDeferredObjInitializerTracker& ThreadInst = FDeferredObjInitializerTracker::Get();
	if (IsCdoDeferred(OuterClass) && (OuterClass != ThreadInst.ResolvingClass))
	{
		DEFERRED_DEPENDENCY_CHECK(CdoOuter->HasAnyFlags(RF_ClassDefaultObject));

		UObject* SubObjTemplate = SubObject->GetArchetype();
		// check that this is an inherited sub-object (that it is defined on the 
		// parent class)... we only need to defer Preload() for inherited 
		// components because they're what gets filled out in the CDO's InitSubobjectProperties()
		if (SubObjTemplate && (SubObjTemplate->GetOuter() != CdoOuter))
		{
			
			ThreadInst.DeferredSubObjects.AddUnique(OuterClass, SubObject);

			return true;
		}
	}
	return false;
}

void FDeferredObjInitializerTracker::Remove(UClass* LoadClass)
{
	FDeferredObjInitializerTracker& ThreadInst = FDeferredObjInitializerTracker::Get();
	ThreadInst.DeferredInitializers.Remove(LoadClass);
	ThreadInst.DeferredSubObjects.Remove(LoadClass);
	ThreadInst.SuperClassMap.RemoveSingle(LoadClass->GetSuperClass(), LoadClass);
}

bool FDeferredObjInitializerTracker::ResolveDeferredInitialization(UClass* LoadClass)
{
	if (FObjectInitializer* DeferredInitializer = FDeferredObjInitializerTracker::Find(LoadClass))
	{
		FDeferredObjInitializerTracker& ThreadInst = FDeferredObjInitializerTracker::Get();
		TGuardValue<UClass*> ResolvingGuard(ThreadInst.ResolvingClass, LoadClass);

		DEFERRED_DEPENDENCY_CHECK(!LoadClass->GetSuperClass()->HasAnyClassFlags(CLASS_NewerVersionExists));
		// initializes and instances CDO properties (copies inherited values 
		// from the super's CDO)
		FScriptIntegrationObjectHelper::PostConstructInitObject(*DeferredInitializer);

		ResolveDeferredSubObjects(LoadClass->GetDefaultObject());
		FDeferredObjInitializerTracker::Remove(LoadClass);

		return true;
	}
	return false;
}

void FDeferredObjInitializerTracker::ResolveDeferredSubObjects(UObject* CDO)
{
	DEFERRED_DEPENDENCY_CHECK(CDO->HasAnyFlags(RF_ClassDefaultObject));
	UClass* LoadClass = CDO->GetClass();

	FDeferredObjInitializerTracker& ThreadInst = FDeferredObjInitializerTracker::Get();
	TArray<UObject*> DeferredSubObjects;
	ThreadInst.DeferredSubObjects.MultiFind(LoadClass, DeferredSubObjects);

	// guard to keep sub-object preloads from ending up back in DeferSubObjectPreload()
	TGuardValue<UClass*> ResolvingGuard(ThreadInst.ResolvingClass, LoadClass);

	FLinkerLoad* ClassLinker = LoadClass->GetLinker();
	DEFERRED_DEPENDENCY_CHECK(ClassLinker != nullptr);
	if (ClassLinker != nullptr)
	{
		// this all needs to happen after PostConstructInitObject() (in 
		// ResolveDeferredInitialization), since InitSubObjectProperties() is 
		// invoked there (which is where we fill this sub-object with values 
		// from the super)... here we account for any Preload() calls that were 
		// skipped on account of the deferred CDO initialization
		for (UObject* SubObj : DeferredSubObjects)
		{
			// no longer need to handle this case, because we're no longer 
			// attempting to recreate the CDO object in 
			// FLinkerLoad::ResolveDeferredExports() (when the super class has
			// been regenerated)
// 			if (SubObj->GetOuter() != CDO)
// 			{
// 				// we may have had to recreate the CDO (because its super has had 
// 				// its layout altered through regeneration); see 
// 				// FLinkerLoad::ResolveDeferredExports() for more context
// 				//
// 				// @TODO: Are we sure that DeferredSubObjects would have all the
// 				//        sub-objects that we need to over?
// 				// 
// 				// @TODO: Are we sure these are the right rename clags to use?
// 				const ERenameFlags RenameFlags = (REN_DontCreateRedirectors | REN_ForceNoResetLoaders | REN_NonTransactional | REN_DoNotDirty);
// 				SubObj->Rename(nullptr, CDO, RenameFlags);
// 			}
			DEFERRED_DEPENDENCY_CHECK(SubObj->GetOuter() == CDO);
			ClassLinker->Preload(SubObj);
		}
	}

	ThreadInst.DeferredSubObjects.Remove(LoadClass);
}

void FDeferredObjInitializerTracker::ResolveDeferredSubClassObjects(UClass* SuperClass)
{
	FDeferredObjInitializerTracker& ThreadInst = FDeferredObjInitializerTracker::Get();
	TArray<UClass*> DeferredSubClasses;
	ThreadInst.SuperClassMap.MultiFind(SuperClass, DeferredSubClasses);

	for (UClass* SubClass : DeferredSubClasses)
	{
		ResolveDeferredInitialization(SubClass);
	}
}

// don't want other files ending up with this internal define
#undef DEFERRED_DEPENDENCY_CHECK
