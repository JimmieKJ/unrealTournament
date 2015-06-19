// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshBatch.h"
#include "BlueprintGeneratedClass.generated.h"


class UEdGraphPin;


USTRUCT()
struct FNodeToCodeAssociation
{
	GENERATED_USTRUCT_BODY()

public:
	TWeakObjectPtr<UEdGraphNode> Node;
	TWeakObjectPtr<UFunction> Scope;
	int32 Offset;
public:
	FNodeToCodeAssociation()
	: Node(nullptr)
	, Scope(nullptr)
	, Offset(0)
	{
	}

	FNodeToCodeAssociation(UEdGraphNode* InNode, UFunction* InFunction, int32 InOffset)
		: Node(InNode)
		, Scope(InFunction)
		, Offset(InOffset)
	{
	}
};


USTRUCT()
struct FDebuggingInfoForSingleFunction
{
	GENERATED_USTRUCT_BODY()

public:
	// Reverse map from code offset to source node
	TMap< int32, TWeakObjectPtr<UEdGraphNode> > LineNumberToSourceNodeMap;

	// Reverse map from code offset to macro source node
	TMap< int32, TWeakObjectPtr<UEdGraphNode> > LineNumberToMacroSourceNodeMap;

	// Reverse map from code offset to macro instance node(s)
	TMultiMap< int32, TWeakObjectPtr<UEdGraphNode> > LineNumberToMacroInstanceNodeMap;

	// Reverse map from code offset to exec pin
	TMap< int32, TWeakObjectPtr<UEdGraphPin> > LineNumberToPinMap;

public:
	FDebuggingInfoForSingleFunction()
	{
	}
};


USTRUCT()
struct FPointerToUberGraphFrame
{
	GENERATED_USTRUCT_BODY()

public:
	uint8* RawPointer;

	FPointerToUberGraphFrame() 
		: RawPointer(nullptr)
	{}

	~FPointerToUberGraphFrame()
	{
		check(!RawPointer);
	}
};


template<>
struct TStructOpsTypeTraits<FPointerToUberGraphFrame> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithZeroConstructor = true,
		WithCopy = false,
	};
};


//////////////////////////////////////////////////////////////////////////
// TSimpleRingBuffer

template<typename ElementType>
class TSimpleRingBuffer
{
public:
	TSimpleRingBuffer(int32 MaxItems)
		: WriteIndex(0)
	{
		Storage.Empty(MaxItems);
	}

	int32 Num() const
	{
		return Storage.Num();
	}

	ElementType& operator()(int32 i)
	{
		// First element is at WriteIndex-1, second at 2, etc...
		const int32 RingIndex = (WriteIndex - 1 - i);
		const int32 WrappedRingIndex = (RingIndex < 0) ? (RingIndex + ArrayMax()) : RingIndex;

		return Storage[WrappedRingIndex];
	}

	const ElementType& operator()(int32 i) const
	{
		// First element is at WriteIndex-1, second at 2, etc...
		const int32 RingIndex = (WriteIndex - 1 - i);
		const int32 WrappedRingIndex = (RingIndex < 0) ? (RingIndex + ArrayMax()) : RingIndex;

		return Storage[WrappedRingIndex];
	}

	int32 ArrayMax() const
	{
		return Storage.GetSlack() + Storage.Num();
	}

	// Note: Doesn't call the constructor on objects stored in it; make sure to properly initialize the memory returned by WriteNewElementUninitialized
	ElementType& WriteNewElementUninitialized()
	{
		const int32 OldWriteIndex = WriteIndex;
		WriteIndex = (WriteIndex + 1) % ArrayMax();

		if (Storage.GetSlack())
		{
			Storage.AddUninitialized(1);
		}
		return Storage[OldWriteIndex];
	}

	ElementType& WriteNewElementInitialized()
	{
		const int32 OldWriteIndex = WriteIndex;
		WriteIndex = (WriteIndex + 1) % ArrayMax();

		if (Storage.GetSlack())
		{
			Storage.Add(ElementType());
		}
		else
		{
			Storage[OldWriteIndex] = ElementType();
		}
		return Storage[OldWriteIndex];
	}

private:
	TArray<ElementType> Storage;
	int32 WriteIndex;
};

USTRUCT()
struct ENGINE_API FBlueprintDebugData
{
	GENERATED_USTRUCT_BODY()
	FBlueprintDebugData()
	{
	}

	~FBlueprintDebugData()
	{ }
#if WITH_EDITORONLY_DATA

protected:
	// Lookup table from UUID to nodes that were allocated that UUID
	TMap<int32, TWeakObjectPtr<UEdGraphNode> > DebugNodesAllocatedUniqueIDsMap;

	// Lookup table from impure node to entry in DebugNodeLineNumbers
	TMultiMap<TWeakObjectPtr<UEdGraphNode>, int32> DebugNodeIndexLookup;

	// List of debug site information for each node that ended up contributing to codegen
	//   This contains a tracepoint for each impure node after all pure antecedent logic has executed but before the impure function call
	//   It does *not* contain the wire tracepoint placed after the impure function call
	TArray<struct FNodeToCodeAssociation> DebugNodeLineNumbers;

	// Acceleration structure for execution wire highlighting at runtime
	TMap<TWeakObjectPtr<UFunction>, FDebuggingInfoForSingleFunction> PerFunctionLineNumbers;

	// Map from pins or nodes to class properties they created
	TMap<TWeakObjectPtr<UObject>, UProperty*> DebugPinToPropertyMap;

public:

	// Returns the UEdGraphNode associated with the UUID, or nullptr if there isn't one.
	UEdGraphNode* FindNodeFromUUID(int32 UUID) const
	{
		if (const TWeakObjectPtr<UEdGraphNode>* pParentNode = DebugNodesAllocatedUniqueIDsMap.Find(UUID))
		{
			return pParentNode->Get();
		}
	
		return nullptr;
	}

	bool IsValid() const
	{
		return DebugNodeLineNumbers.Num() > 0;
	}

	// Finds the UEdGraphNode associated with the code location Function+CodeOffset, or nullptr if there isn't one
	UEdGraphNode* FindSourceNodeFromCodeLocation(UFunction* Function, int32 CodeOffset, bool bAllowImpreciseHit) const
	{
		if (const FDebuggingInfoForSingleFunction* pFuncInfo = PerFunctionLineNumbers.Find(Function))
		{
			UEdGraphNode* Result = pFuncInfo->LineNumberToSourceNodeMap.FindRef(CodeOffset).Get();

			if ((Result == nullptr) && bAllowImpreciseHit)
			{
				for (int32 TrialOffset = CodeOffset + 1; (Result == nullptr) && (TrialOffset < Function->Script.Num()); ++TrialOffset)
				{
					Result = pFuncInfo->LineNumberToSourceNodeMap.FindRef(TrialOffset).Get();
				}
			}

			return Result;
		}

		return nullptr;
	}

	// Finds the macro source node associated with the code location Function+CodeOffset, or nullptr if there isn't one
	UEdGraphNode* FindMacroSourceNodeFromCodeLocation(UFunction* Function, int32 CodeOffset) const
	{
		if (const FDebuggingInfoForSingleFunction* pFuncInfo = PerFunctionLineNumbers.Find(Function))
		{
			return pFuncInfo->LineNumberToMacroSourceNodeMap.FindRef(CodeOffset).Get();
		}

		return nullptr;
	}

	// Finds the macro instance node(s) associated with the code location Function+CodeOffset. The returned set can be empty.
	void FindMacroInstanceNodesFromCodeLocation(UFunction* Function, int32 CodeOffset, TArray<UEdGraphNode*>& MacroInstanceNodes) const
	{
		MacroInstanceNodes.Empty();
		if (const FDebuggingInfoForSingleFunction* pFuncInfo = PerFunctionLineNumbers.Find(Function))
		{
			TArray<TWeakObjectPtr<UEdGraphNode>> MacroInstanceNodePtrs;
			pFuncInfo->LineNumberToMacroInstanceNodeMap.MultiFind(CodeOffset, MacroInstanceNodePtrs);
			for(auto MacroInstanceNodePtrIt = MacroInstanceNodePtrs.CreateConstIterator(); MacroInstanceNodePtrIt; ++MacroInstanceNodePtrIt)
			{
				TWeakObjectPtr<UEdGraphNode> MacroInstanceNodePtr = *MacroInstanceNodePtrIt;
				if(UEdGraphNode* MacroInstanceNode = MacroInstanceNodePtr.Get())
				{
					MacroInstanceNodes.AddUnique(MacroInstanceNode);
				}
			}
		}
	}

	// Finds the macro source node associated with the code location Function+CodeOffset, or nullptr if there isn't one
	UEdGraphPin* FindExecPinFromCodeLocation(UFunction* Function, int32 CodeOffset) const
	{
		if (const FDebuggingInfoForSingleFunction* pFuncInfo = PerFunctionLineNumbers.Find(Function))
		{
			return pFuncInfo->LineNumberToPinMap.FindRef(CodeOffset).Get();
		}

		return nullptr;
	}

	// Finds the breakpoint injection site(s) in bytecode if any were associated with the given node
	void FindBreakpointInjectionSites(UEdGraphNode* Node, TArray<uint8*>& InstallSites) const
	{
		TArray<int32> RecordIndices;
		DebugNodeIndexLookup.MultiFind(Node, RecordIndices, true);
		for(int i = 0; i < RecordIndices.Num(); ++i)
		{
			const FNodeToCodeAssociation& Record = DebugNodeLineNumbers[RecordIndices[i]];
			if (UFunction* Scope = Record.Scope.Get())
			{
				InstallSites.Add(&(Scope->Script[Record.Offset]));
			}
		}
	}

	// Looks thru the debugging data for any class variables associated with the node
	UProperty* FindClassPropertyForPin(const UEdGraphPin* Pin) const
	{
		UProperty* PropertyPtr = DebugPinToPropertyMap.FindRef(Pin);
		if ((PropertyPtr == nullptr) && (Pin->Direction == EGPD_Input) && (Pin->LinkedTo.Num() > 0))
		{
			// Try checking the other side of the connection
			PropertyPtr = DebugPinToPropertyMap.FindRef(Pin->LinkedTo[0]);
		}

		return PropertyPtr;
	}

	// Looks thru the debugging data for any class variables associated with the node (e.g., temporary variables or timelines)
	UProperty* FindClassPropertyForNode(const UEdGraphNode* Node) const
	{
		return DebugPinToPropertyMap.FindRef(Node);
	}

	// Adds a debug record for a source node and destination in the bytecode of a specified function
	void RegisterNodeToCodeAssociation(UEdGraphNode* TrueSourceNode, UEdGraphNode* MacroSourceNode, const TArray<TWeakObjectPtr<UEdGraphNode>>& MacroInstanceNodes, UFunction* InFunction, int32 CodeOffset, bool bBreakpointSite)
	{
		//@TODO: Nasty expansion behavior during compile time
		if (bBreakpointSite)
		{
			new (DebugNodeLineNumbers) FNodeToCodeAssociation(TrueSourceNode, InFunction, CodeOffset);
			DebugNodeIndexLookup.Add(TrueSourceNode, DebugNodeLineNumbers.Num() - 1);
		}

		FDebuggingInfoForSingleFunction& PerFuncInfo = PerFunctionLineNumbers.FindOrAdd(InFunction);
		PerFuncInfo.LineNumberToSourceNodeMap.Add(CodeOffset, TrueSourceNode);

		if (MacroSourceNode)
		{
			PerFuncInfo.LineNumberToMacroSourceNodeMap.Add(CodeOffset, MacroSourceNode);
		}

		for(auto MacroInstanceNodePtrIt = MacroInstanceNodes.CreateConstIterator(); MacroInstanceNodePtrIt; ++MacroInstanceNodePtrIt)
		{
			PerFuncInfo.LineNumberToMacroInstanceNodeMap.Add(CodeOffset, *MacroInstanceNodePtrIt);
		}
	}

	void RegisterPinToCodeAssociation(UEdGraphPin const* ExecPin, UFunction* InFunction, int32 CodeOffset)
	{
		FDebuggingInfoForSingleFunction& PerFuncInfo = PerFunctionLineNumbers.FindOrAdd(InFunction);
		PerFuncInfo.LineNumberToPinMap.Add(CodeOffset, ExecPin);
	}

	// Registers an association between an object (pin or node typically) and an associated class member property
	void RegisterClassPropertyAssociation(class UObject* TrueSourceObject, class UProperty* AssociatedProperty)
	{
		DebugPinToPropertyMap.Add(TrueSourceObject, AssociatedProperty);
	}

	// Registers an association between a UUID and a node
	void RegisterUUIDAssociation(UEdGraphNode* TrueSourceNode, int32 UUID)
	{
		DebugNodesAllocatedUniqueIDsMap.Add(UUID, TrueSourceNode);
	}

	// Returns the object that caused the specified property to be created (can return nullptr if the association is unknown)
	UObject* FindObjectThatCreatedProperty(class UProperty* AssociatedProperty) const
	{
		if (const TWeakObjectPtr<UObject>* pValue = DebugPinToPropertyMap.FindKey(AssociatedProperty))
		{
			return pValue->Get();
		}
		else
		{
			return nullptr;
		}
	}

	void GenerateReversePropertyMap(TMap<UProperty*, UObject*>& PropertySourceMap)
	{
		for (TMap<TWeakObjectPtr<UObject>, UProperty*>::TIterator MapIt(DebugPinToPropertyMap); MapIt; ++MapIt)
		{
			if (UObject* SourceObj = MapIt.Key().Get())
			{
				PropertySourceMap.Add(MapIt.Value(), SourceObj);
			}
		}
	}
#endif
};

USTRUCT()
struct ENGINE_API FEventGraphFastCallPair
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	UFunction* FunctionToPatch;

	UPROPERTY()
	int32 EventGraphCallOffset;
};

UCLASS()
class ENGINE_API UBlueprintGeneratedClass : public UClass
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(AssetRegistrySearchable)
	int32	NumReplicatedProperties;

	/** Array of objects containing information for dynamically binding delegates to functions in this blueprint */
	UPROPERTY()
	TArray<class UDynamicBlueprintBinding*> DynamicBindingObjects;

	/** Array of component template objects, used by AddComponent function */
	UPROPERTY()
	TArray<class UActorComponent*> ComponentTemplates;

	/** Array of templates for timelines that should be created */
	UPROPERTY()
	TArray<class UTimelineTemplate*> Timelines;

	/** 'Simple' construction script - graph of components to instance */
	UPROPERTY()
	class USimpleConstructionScript* SimpleConstructionScript;

	/** Stores data to override (in children classes) components (created by SCS) from parent classes */
	UPROPERTY()
	class UInheritableComponentHandler* InheritableComponentHandler;

	UPROPERTY()
	UStructProperty* UberGraphFramePointerProperty;

	UPROPERTY()
	UFunction* UberGraphFunction;

	// This is a list of event graph call function nodes that are simple (no argument) thunks into the event graph (typically used for animation delegates, etc...)
	// It is a deprecated list only used for backwards compatibility prior to VER_UE4_SERIALIZE_BLUEPRINT_EVENTGRAPH_FASTCALLS_IN_UFUNCTION.
	UPROPERTY()
	TArray<FEventGraphFastCallPair> FastCallPairs_DEPRECATED;

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient)
	UObject* OverridenArchetypeForCDO;
#endif //WITH_EDITOR

	/** 
	 * Gets an array of all BPGeneratedClasses (including InClass as 0th element) parents of given generated class 
	 *
	 * @param InClass				The class to get the blueprint lineage for
	 * @param OutBlueprintParents	Array with the blueprints used to generate this class and its parents.  0th = this, Nth = least derived BP-based parent
	 * @return						true if there were no status errors in any of the parent blueprints, otherwise false
	 */
	static bool GetGeneratedClassesHierarchy(const UClass* InClass, TArray<const UBlueprintGeneratedClass*>& OutBPGClasses);

	UInheritableComponentHandler* GetInheritableComponentHandler(const bool bCreateIfNecessary = false);

	/** Find the object in the TemplateObjects array with the supplied name */
	UActorComponent* FindComponentTemplateByName(const FName& TemplateName) const;

	/** Create Timeline objects for this Actor based on the Timelines array*/
	virtual void CreateComponentsForActor(AActor* Actor) const;

	// UObject interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	virtual void PostInitProperties() override;
	// End UObject interface
	
	// UClass interface
#if WITH_EDITOR
	virtual UClass* GetAuthoritativeClass() override;
	virtual void ConditionalRecompileClass(TArray<UObject*>* ObjLoaded) override;
	virtual UObject* GetArchetypeForCDO() const override;
#endif //WITH_EDITOR
	virtual bool IsFunctionImplementedInBlueprint(FName InFunctionName) const override;
	virtual uint8* GetPersistentUberGraphFrame(UObject* Obj, UFunction* FuncToCheck) const override;
	virtual void CreatePersistentUberGraphFrame(UObject* Obj, bool bCreateOnlyIfEmpty = false) const override;
	virtual void DestroyPersistentUberGraphFrame(UObject* Obj) const override;
	virtual void Link(FArchive& Ar, bool bRelinkExistingProperties) override;
	virtual void PurgeClass(bool bRecompilingOnLoad) override;
	virtual void Bind() override;
	virtual void GetRequiredPreloadDependencies(TArray<UObject*>& DependenciesOut) override;
	virtual UObject* FindArchetype(UClass* ArchetypeClass, const FName ArchetypeName) const override;
	// End UClass interface

	static void AddReferencedObjectsInUbergraphFrame(UObject* InThis, FReferenceCollector& Collector);

	static FName GetUberGraphFrameName();
	static bool UsePersistentUberGraphFrame();

#if WITH_EDITORONLY_DATA
	FBlueprintDebugData DebugData;


	FBlueprintDebugData& GetDebugData()
	{
		return DebugData;
	}
#endif

	/** Bind functions on supplied actor to delegates */
	void BindDynamicDelegates(UObject* InInstance) const;

	/** Unbind functions on supplied actor from delegates tied to a specific property */
	void UnbindDynamicDelegatesForProperty(UObject* InInstance, const UObjectProperty* InObjectProperty);
	
	// Finds the desired dynamic binding object for this blueprint generated class
	UDynamicBlueprintBinding* GetDynamicBindingObject(UClass* Class) const;

	/** called to gather blueprint replicated properties */
	virtual void GetLifetimeBlueprintReplicationList(TArray<class FLifetimeProperty>& OutLifetimeProps) const;
	/** called prior to replication of an instance of this BP class */
	virtual void InstancePreReplication(class IRepChangedPropertyTracker& ChangedPropertyTracker) const
	{
		UBlueprintGeneratedClass* SuperBPClass = Cast<UBlueprintGeneratedClass>(GetSuperStruct());
		if (SuperBPClass != NULL)
		{
			SuperBPClass->InstancePreReplication(ChangedPropertyTracker);
		}
	}
};
