// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/** 
 * This set of functions contains blueprint related UObject functionality.
 */
struct FBlueprintSupport
{
	/** 
	 * Defined in BlueprintSupport.cpp
	 * Duplicates all fields of a struct in depth-first order. It makes sure that everything contained
	 * in a class is duplicated before the struct itself, as well as all function parameters before the
	 * function itself.
	 *
	 * @param	StructToDuplicate			Instance of the struct that is about to be duplicated
	 * @param	Writer						duplicate writer instance to write the duplicated data to
	 */
	static void DuplicateAllFields(class UStruct* StructToDuplicate, class FDuplicateDataWriter& Writer);

	/** 
	 * A series of query functions that we can use to easily gate-off/disable 
	 * aspects of the deferred loading (mostly for testing purposes). 
	 */
	static bool UseDeferredDependencyLoading();
	static bool IsDeferredExportCreationDisabled();
	static bool IsDeferredCDOInitializationDisabled();
};

#if WITH_EDITOR
/**
 * This is a helper struct that allows us to gather all previously unloaded class dependencies of a UClass
 * The first time we create a new UClass object in FLinkerLoad::CreateExport(), we register it as a dependency
 * master.  Any subsequent UClasses that are created for the first time during the preload of that class are
 * added to the list as potential cyclic referencers.  We then step over the list at the end of the load, and
 * recompile any classes that may depend on each other a second time to ensure that that functions and properties
 * are properly resolved
 */
struct COREUOBJECT_API FScopedClassDependencyGather
{
public:
	FScopedClassDependencyGather(UClass* ClassToGather);
	~FScopedClassDependencyGather();

	/**
	 * Post load, some systems would like an easy list of dependencies. This will
	 * retrieve that latest BatchClassDependencies (filled with dependencies from
	 * the last loaded class).
	 * 
	 * @return The most recent array of tracked dependencies.
	 */
	static TArray<UClass*> const& GetCachedDependencies();

private:
	/** Whether or not this dependency gather is the dependency master, and thus should process all dependencies in the destructor */
	bool bMasterClass;	

	/** The current class that is gathering potential dependencies in this scope */
	static UClass* BatchMasterClass;

	/** List of dependencies (i.e. UClasses that have been newly instantiated) in the scope of this dependency gather */
	static TArray<UClass*> BatchClassDependencies;

	FScopedClassDependencyGather();
};

/**
 * Stores info about BPCG, UDS, UDE, etc.. converted into native code.
 *
 * Native entities (generated from BP items) have "ReplaceConverted" with original object path.
 * This path is used to fix linker while loading.
 * 
 * This struct is used only when the code converted from BP is compiled in an editor build. 
 * It's usually only for a test/development purpose.
 */
struct COREUOBJECT_API FReplaceConvertedAssetManager : public FGCObject
{
private:
	TMap<FString, UObject*> ReplaceMap;
	bool bIsEnabled;

	FReplaceConvertedAssetManager();

	void GatherOriginalPathsOfConvertedAssets();

public:

	static FReplaceConvertedAssetManager& Get();

	void SetEnabled(bool InEnabled)
	{
		bIsEnabled = InEnabled;
		if (bIsEnabled)
		{
			GatherOriginalPathsOfConvertedAssets();
		}
	}

	bool IsEnabled()
	{
		return bIsEnabled;
	}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	UPackage* FindPackageReplacement(const FString& OriginalPathName) const;

	UObject* FindReplacement(const FString& OriginalPathName) const;

	static void OnModulesChanged(FName ModuleThatChanged, EModuleChangeReason ReasonForChange);
};

/**
 * The struct is used while saving cooked package to find replacements of converted BP assets.
 */
struct COREUOBJECT_API FReplaceCookedBPGC : public FGCObject
{
private:
	UPackage* NativeScriptPackage;
	TMap<FString, UObject*> ReplaceMap;
	TSet<FName> ConvertedPackagesNames;
	bool bEnabled;

	FReplaceCookedBPGC();

	void AdditionalStubFieldInitialization(UField* Stub);

public:
	static FReplaceCookedBPGC& Get();
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	// INITIALIZATION
	void Initialize(const FString& NativePackageName);
	void AddConvertedPackageName(FName PackageName);
	template<typename FieldType>
	void AddConvertedFieldStub(const FString& OriginalPathName, const FName Name)
	{
		UObject*& NativeObj = ReplaceMap.FindOrAdd(OriginalPathName);
		if (!NativeObj)
		{
			check(NativeScriptPackage);
			FieldType* NewField = NewObject<FieldType>(NativeScriptPackage, Name);
			AdditionalStubFieldInitialization(NewField);
			NewField->Bind();
			NativeObj = NewField;
		}
	}
	void AddConvertedFieldStub(UObject* Object);

	// USAGE
	bool IsEnabled() const
	{
		return bEnabled;
	}
	UObject* FindReplacementStub(UObject* Object);

	// The object is of a convertible type, and the object is not listed in excluded assets.
	bool CouldBeConverted(const UObject* Object) const;
	bool PackageShouldNotBeSaved(const UPackage* InOuter) const;

	DECLARE_DELEGATE_RetVal_OneParam(bool, FIsTargetedForConversionQuery, const UObject*);
	FIsTargetedForConversionQuery& OnQueryIfAssetIsTargetedForConversion() { return IsTargetedForConversionDelegate; }

private: 
	FIsTargetedForConversionQuery IsTargetedForConversionDelegate;
};

#endif //WITH_EDITOR

/** 
 * A helper struct for storing FObjectInitializers that were not run on 
 * Blueprint CDO's post-construction (presumably because that CDO's super had 
 * not been fully serialized yet). 
 * 
 * This was designed to hold onto FObjectInitializers until a later point, when 
 * they can properly be ran (presumably in FLinkerLoad::ResolveDeferredExports,
 * after the super has been finalized).
 */
struct FDeferredObjInitializerTracker : TThreadSingleton<FDeferredObjInitializerTracker>
{
public:
	FDeferredObjInitializerTracker() : ResolvingClass(nullptr) {}

	/** Stores a copy of the specified FObjectInitializer and returns a pointer to it (could be null if a corresponding class could not be determined). */
	static FObjectInitializer* Add(const FObjectInitializer& DeferringInitializer);

	/** Looks up a FObjectInitializer that was deferred for the specified class (an FObjectInitializer for that class's CDO). */
	static FObjectInitializer* Find(UClass* LoadClass);

	/** Checks to see if the specified class has been logged as deferred (meaning its CDO hasn't had FObjectInitializer::PostConstructInit() ran on it yet). */
	static bool IsCdoDeferred(UClass* LoadClass);

	/** Determines if the specified sub-object should have its Preload() skipped; if so, this will cache the sub-object and return true. */
	static bool DeferSubObjectPreload(UObject* SubObject);

	/** Destroys any FObjectInitializers that were cached corresponding to the specified class. */
	static void Remove(UClass* LoadClass);

	/** Runs FObjectInitializer::PostConstructInit() on the specified class's CDO (if it was deferred), and preloads any sub-objects that were skipped. */
	static bool ResolveDeferredInitialization(UClass* LoadClass);

	/**  */
	static void ResolveDeferredSubObjects(UObject* CDO);

	/**  */
	static void ResolveDeferredSubClassObjects(UClass* SuperClass);

private:
	/** A map that tracks the relationship between Blueprint classes and FObjectInitializers for their CDOs */
	TMap<UClass*, FObjectInitializer> DeferredInitializers;
	/** Track default sub-objects that had their Preload() skipped, because the owning CDO's initialization should happen first */
	TMultiMap<UClass*, UObject*> DeferredSubObjects;
	/** Used to keep ResolveDeferredSubObjects() from re-adding sub-objects via DeferSubObjectPreload() */
	UClass* ResolvingClass;
	/** Tracks sub-classes that have had their CDO deferred as a result of the super not being fully serialized */
	TMultiMap<UClass*, UClass*> SuperClassMap;
};


/**
 *	Stores info about dependencies of native classes converted from BPs
 */
struct COREUOBJECT_API FConvertedBlueprintsDependencies
{
	typedef void(*GetDependenciesNamesFunc)(TArray<FName>&);

private:

	TMap<FName, GetDependenciesNamesFunc> ClassNameToGetter;

public:

#if WITH_EDITOR

	typedef UField*(*StaticCreateClassFunc)();

	TArray<StaticCreateClassFunc> CreateClassFunctions;

	void RegisterClass(StaticCreateClassFunc CreateClass)
	{
		CreateClassFunctions.Add(CreateClass);
	}

#endif //WITH_EDITOR

	static FConvertedBlueprintsDependencies& Get();

	bool AnyClassRegistered() const
	{
		return ClassNameToGetter.Num() > 0;
	}

	void RegisterClass(FName ClassName, GetDependenciesNamesFunc GetAssets);

	/** Get all assets paths necessary for the class with the given class name and all converted classes that dependencies. */
	void GetAssets(FName ClassName, TArray<FName>& OutPackagePaths) const;
};