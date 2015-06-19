// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#ifndef __KismetReinstanceUtilities_h__
#define __KismetReinstanceUtilities_h__

#pragma once

#include "Engine.h"

DECLARE_STATS_GROUP(TEXT("Kismet Reinstancer"), STATGROUP_KismetReinstancer, STATCAT_Advanced);

class FReinstanceFinalizer;

class UNREALED_API FBlueprintCompileReinstancer : public TSharedFromThis<FBlueprintCompileReinstancer>, public FGCObject
{
public:
	/**
	 * CDO duplicates provider delegate type.
	 */
	DECLARE_DELEGATE_RetVal_TwoParams(UObject*, FCDODuplicatesProvider, UClass*, FName);

	/**
	 * Gets CDO duplicates provider delegate.
	 */
	static FCDODuplicatesProvider& GetCDODuplicatesProviderDelegate();

protected:

	static TSet<TWeakObjectPtr<UBlueprint>> DependentBlueprintsToRefresh;
	static TSet<TWeakObjectPtr<UBlueprint>> DependentBlueprintsToRecompile;
	static TSet<TWeakObjectPtr<UBlueprint>> DependentBlueprintsToByteRecompile;

	static UClass* HotReloadedOldClass;
	static UClass* HotReloadedNewClass;

	/** Reference to the class we're actively reinstancing */
	UClass* ClassToReinstance;

	/** Reference to the duplicate of ClassToReinstance, which all previous instances are now instances of */
	UClass* DuplicatedClass;

	/** The original CDO object for the class being actively reinstanced */
	UObject*	OriginalCDO;

	/** Children of this blueprint, which will need to be recompiled and relinked temporarily to maintain the class layout */
	TArray<UBlueprint*> Children;

	/** Bytecode dependent blueprints, which will need to be updated after the compilation */
	TArray<UBlueprint*> Dependencies;

	/** Mappings from old fields before recompilation to their new equivalents */
	TMap<FName, UProperty*> PropertyMap;
	TMap<FName, UFunction*> FunctionMap;

	/** Whether or not this resinstancer has already reinstanced  */
	bool bHasReinstanced;

	/** Don't call GC */
	bool bSkipGarbageCollection;

	/** Cached value, true if reinstancing a skeleton class or not */
	bool bIsReinstancingSkeleton;

	uint32 ClassToReinstanceDefaultValuesCRC;

	/** Objects that should keep reference to old class */
	TSet<UObject*> ObjectsThatShouldUseOldStuff;

public:
	// FSerializableObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	// End of FSerializableObject interface

	static void OptionallyRefreshNodes(UBlueprint* BP);

	void ListDependentBlueprintsToRefresh(const TArray<UBlueprint*>& DependentBPs);
	void EnlistDependentBlueprintToRecompile(UBlueprint* BP, bool bBytecodeOnly);
	void BlueprintWasRecompiled(UBlueprint* BP, bool bBytecodeOnly);

	static TSharedPtr<FBlueprintCompileReinstancer> Create(UClass* InClassToReinstance, bool bIsBytecodeOnly = false, bool bSkipGC = false)
	{
		return MakeShareable(new FBlueprintCompileReinstancer(InClassToReinstance, bIsBytecodeOnly, bSkipGC));
	}

	virtual ~FBlueprintCompileReinstancer();

	/** Saves a mapping of field names to their UField equivalents, so we can remap any bytecode that references them later */
	void SaveClassFieldMapping(UClass* InClassToReinstance);

	/** Helper to gather mappings from the old class's fields to the new class's version */
	void GenerateFieldMappings(TMap<UObject*, UObject*>& FieldMapping);

	/** Reinstances all objects in the ObjectReinstancingMap */
	void ReinstanceObjects(bool bForceAlwaysReinstance = false);

	/** Updates references to properties and functions of the class that has in the bytecode of dependent blueprints */
	void UpdateBytecodeReferences();

	/** Worker function to replace all instances of OldClass with a new instance of NewClass */
	static void ReplaceInstancesOfClass(UClass* OldClass, UClass* NewClass, UObject* OriginalCDO = NULL, TSet<UObject*>* ObjectsThatShouldUseOldStuff = NULL, bool bClassObjectReplaced = false, bool bPreserveRootComponent = true);

	/**
	 * When re-instancing a component, we have to make sure all instance owners' 
	 * construction scripts are re-ran (in-case modifying the component alters 
	 * the construction of the actor).
	 * 
	 * @param  ComponentClass    Identifies the component that was altered (used to find all its instances, and thusly all instance owners).
	 */
	static void ReconstructOwnerInstances(TSubclassOf<UActorComponent> ComponentClass);
	
	/** Verify that all instances of the duplicated class have been replaced and collected */
	void VerifyReplacement();

	virtual bool IsClassObjectReplaced() const { return false; }

	void FinalizeFastReinstancing(TArray<UObject*>& ObjectsToReplace);
protected:

	TSharedPtr<FReinstanceFinalizer> ReinstanceInner(bool bForceAlwaysReinstance);

	TSharedPtr<FReinstanceFinalizer> ReinstanceFast();

	void CompileChildren();

	/** Default constructor, can only be used by derived classes */
	FBlueprintCompileReinstancer()
		: ClassToReinstance(NULL)
		, DuplicatedClass(NULL)
		, OriginalCDO(NULL)
		, bHasReinstanced(false)
		, bSkipGarbageCollection(false)
		, ClassToReinstanceDefaultValuesCRC(0)
	{}

	/** Sets the reinstancer up to work on every object of the specified class */
	FBlueprintCompileReinstancer(UClass* InClassToReinstance, bool bIsBytecodeOnly = false, bool bSkipGC = false);

	/** Reparents the specified blueprint or class to be the duplicated class in order to allow properties to be copied from the previous CDO to the new one */
	void ReparentChild(UBlueprint* ChildBP);
	void ReparentChild(UClass* ChildClass);

	/** Determine whether reinstancing actors should preserve the root component of the new actor */
	virtual bool ShouldPreserveRootComponentOfReinstancedActor() const { return true; }

private:
	/**
	 * Gets class CDO duplicate from cache (or creates it if not available or not
	 * during hot-reload) for given class.
	 *
	 * @param Class Class to get (or create) CDO for.
	 * @param Name The name that CDO has to be renamed with (or created with).
	 */
	static UObject* GetClassCDODuplicate(UClass* Class, FName Name);
};


#endif//__KismetEditorUtilities_h__
