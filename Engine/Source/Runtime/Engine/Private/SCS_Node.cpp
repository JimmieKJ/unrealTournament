// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "BlueprintUtilities.h"
#include "LatentActions.h"
#if WITH_EDITOR
#include "Kismet2/BlueprintEditorUtils.h"
#endif
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/InheritableComponentHandler.h"

//////////////////////////////////////////////////////////////////////////
// USCS_Node

USCS_Node::USCS_Node(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsFalseRoot_DEPRECATED = false;
	bIsNative_DEPRECATED = false;

	bIsParentComponentNative = false;

#if WITH_EDITOR
	EditorComponentInstance = NULL;
#endif
}

UActorComponent* USCS_Node::GetActualComponentTemplate(UBlueprintGeneratedClass* ActualBPGC) const
{
	UActorComponent* OverridenComponentTemplate = nullptr;
	static const FBoolConfigValueHelper EnableInheritableComponents(TEXT("Kismet"), TEXT("bEnableInheritableComponents"), GEngineIni);
	if (EnableInheritableComponents)
	{
		const USimpleConstructionScript* SCS = GetSCS();
		if (SCS != ActualBPGC->SimpleConstructionScript)
		{
			const FComponentKey ComponentKey(this);
			
			do
			{
				auto InheritableComponentHandler = ActualBPGC->GetInheritableComponentHandler();
				if (InheritableComponentHandler)
				{
					OverridenComponentTemplate = InheritableComponentHandler->GetOverridenComponentTemplate(ComponentKey);
				}

				ActualBPGC = Cast<UBlueprintGeneratedClass>(ActualBPGC->GetSuperClass());

			} while (!OverridenComponentTemplate && ActualBPGC && SCS != ActualBPGC->SimpleConstructionScript);
		}
	}
	return OverridenComponentTemplate ? OverridenComponentTemplate : ComponentTemplate;
}

const FBlueprintCookedComponentInstancingData* USCS_Node::GetActualComponentTemplateData(UBlueprintGeneratedClass* ActualBPGC) const
{
	const FBlueprintCookedComponentInstancingData* OverridenComponentTemplateData = nullptr;
	static const FBoolConfigValueHelper EnableInheritableComponents(TEXT("Kismet"), TEXT("bEnableInheritableComponents"), GEngineIni);
	if (EnableInheritableComponents)
	{
		const USimpleConstructionScript* SCS = GetSCS();
		if (SCS != ActualBPGC->SimpleConstructionScript)
		{
			const FComponentKey ComponentKey(this);
			
			do
			{
				auto InheritableComponentHandler = ActualBPGC->GetInheritableComponentHandler();
				if (InheritableComponentHandler)
				{
					OverridenComponentTemplateData = InheritableComponentHandler->GetOverridenComponentTemplateData(ComponentKey);
				}

				ActualBPGC = Cast<UBlueprintGeneratedClass>(ActualBPGC->GetSuperClass());

			} while (!OverridenComponentTemplateData && ActualBPGC && SCS != ActualBPGC->SimpleConstructionScript);
		}
	}

	return OverridenComponentTemplateData ? OverridenComponentTemplateData : &CookedComponentInstancingData;
}

UActorComponent* USCS_Node::ExecuteNodeOnActor(AActor* Actor, USceneComponent* ParentComponent, const FTransform* RootTransform, bool bIsDefaultTransform)
{
	check(Actor != nullptr);
	check((ParentComponent != nullptr && !ParentComponent->IsPendingKill()) || (RootTransform != nullptr)); // must specify either a parent component or a world transform

	// Create a new component instance based on the template
	UActorComponent* NewActorComp = nullptr;
	auto ActualBPGC = Cast<UBlueprintGeneratedClass>(Actor->GetClass());
	auto ActualComponentTemplateData = FPlatformProperties::RequiresCookedData() ? GetActualComponentTemplateData(ActualBPGC) : nullptr;
	if (ActualComponentTemplateData && ActualComponentTemplateData->bIsValid)
	{
		// Use cooked instancing data if valid (fast path).
		NewActorComp = Actor->CreateComponentFromTemplateData(ActualComponentTemplateData, VariableName);
	}
	else if (UActorComponent* ActualComponentTemplate = GetActualComponentTemplate(ActualBPGC))
	{
		NewActorComp = Actor->CreateComponentFromTemplate(ActualComponentTemplate, VariableName);
	}

	if(NewActorComp != nullptr)
	{
		NewActorComp->CreationMethod = EComponentCreationMethod::SimpleConstructionScript;
		// SCS created components are net addressable
		NewActorComp->SetNetAddressable();

		// Special handling for scene components
		USceneComponent* NewSceneComp = Cast<USceneComponent>(NewActorComp);
		if (NewSceneComp != nullptr)
		{
			// If NULL is passed in, we are the root, so set transform and assign as RootComponent on Actor, similarly if the 
			// NewSceneComp is the ParentComponent then we are the root component. This happens when the root component is recycled
			// by StaticAllocateObject.
			if (ParentComponent == nullptr || (ParentComponent && ParentComponent->IsPendingKill()) || ParentComponent == NewSceneComp)
			{
				FTransform WorldTransform = *RootTransform;
				if(bIsDefaultTransform)
				{
					// Note: We use the scale vector from the component template when spawning (to match what happens with a native root)
					WorldTransform.SetScale3D(NewSceneComp->RelativeScale3D);
				}
				NewSceneComp->SetWorldTransform(WorldTransform);
				Actor->SetRootComponent(NewSceneComp);
			}
			// Otherwise, attach to parent component passed in
			else
			{
				NewSceneComp->SetupAttachment(ParentComponent, AttachToName);
			}
		}

		// Call function to notify component it has been created
		NewActorComp->OnComponentCreated();

		if (NewActorComp->GetIsReplicated())
		{
			// Make sure this component is added to owning actor's replicated list.
			NewActorComp->SetIsReplicated(true);
		}

		// If we want to save this to a property, do it here
		FName VarName = GetVariableName();
		if (VarName != NAME_None)
		{
			UClass* ActorClass = Actor->GetClass();
			if (UObjectPropertyBase* Prop = FindField<UObjectPropertyBase>(ActorClass, VarName))
			{
				Prop->SetObjectPropertyValue_InContainer(Actor, NewActorComp);
			}
			else
			{
				UE_LOG(LogBlueprint, Log, TEXT("ExecuteNodeOnActor: Couldn't find property '%s' on '%s"), *VarName.ToString(), *Actor->GetName());
#if WITH_EDITOR
				// If we're constructing editable components in the SCS editor, set the component instance corresponding to this node for editing purposes
				USimpleConstructionScript* SCS = GetSCS();
				if(SCS != nullptr && (SCS->IsConstructingEditorComponents() || SCS->GetComponentEditorActorInstance() == Actor))
				{
					EditorComponentInstance = NewSceneComp;
				}
#endif
			}
		}

		// Determine the parent component for our children (it's still our parent if we're a non-scene component)
		USceneComponent* ParentSceneComponentOfChildren = (NewSceneComp != nullptr) ? NewSceneComp : ParentComponent;

		// If we made a component, go ahead and process our children
		for (int32 NodeIdx = 0; NodeIdx < ChildNodes.Num(); NodeIdx++)
		{
			USCS_Node* Node = ChildNodes[NodeIdx];
			check(Node != nullptr);
			Node->ExecuteNodeOnActor(Actor, ParentSceneComponentOfChildren, nullptr, false);
		}
	}

	return NewActorComp;
}

TArray<USCS_Node*> USCS_Node::GetAllNodes()
{
	TArray<USCS_Node*> AllNodes;

	//  first add ourself
	AllNodes.Add(this);

	// then add each child (including all their children)
	for(int32 ChildIdx=0; ChildIdx<ChildNodes.Num(); ChildIdx++)
	{
		USCS_Node* ChildNode = ChildNodes[ChildIdx];
		check(ChildNode != NULL);
		AllNodes.Append( ChildNode->GetAllNodes() );
	}

	return AllNodes;
}

void USCS_Node::AddChildNode(USCS_Node* InNode, bool bAddToAllNodes)
{
	if (InNode != NULL && !ChildNodes.Contains(InNode))
	{
		Modify();

		ChildNodes.Add(InNode);
		if (bAddToAllNodes)
		{
			FSCSAllNodesHelper::Add(GetSCS(), InNode);
		}
	}
}

void USCS_Node::RemoveChildNodeAt(int32 ChildIndex, bool bRemoveFromAllNodes)
{
	if (ChildIndex >= 0 && ChildIndex < ChildNodes.Num())
	{
		Modify();

		USCS_Node* ChildNode = ChildNodes[ChildIndex];
		ChildNodes.RemoveAt(ChildIndex);
		if (bRemoveFromAllNodes)
		{
			FSCSAllNodesHelper::Remove(GetSCS(), ChildNode);
		}
	}
}

void USCS_Node::RemoveChildNode(USCS_Node* InNode, bool bRemoveFromAllNodes)
{
	Modify();
	if (ChildNodes.Remove(InNode) != INDEX_NONE && bRemoveFromAllNodes)
	{
		FSCSAllNodesHelper::Remove(GetSCS(), InNode);
	}
}

void USCS_Node::MoveChildNodes(USCS_Node* SourceNode, int32 InsertLocation)
{
	if (SourceNode)
	{
		Modify();
		SourceNode->Modify();

		USimpleConstructionScript* SourceSCS = SourceNode->GetSCS();
		USimpleConstructionScript* MySCS = GetSCS();
		if (SourceSCS != MySCS)
		{
			for (USCS_Node* SCSNode : SourceNode->ChildNodes)
			{
				FSCSAllNodesHelper::Remove(SourceSCS, SCSNode);
				FSCSAllNodesHelper::Add(MySCS, SCSNode);
			}
		}
		if (InsertLocation == INDEX_NONE)
		{
			ChildNodes.Append(SourceNode->ChildNodes);
		}
		else
		{
			ChildNodes.Insert(SourceNode->ChildNodes, InsertLocation);
		}
		SourceNode->ChildNodes.Empty();
	}
}

TArray<const USCS_Node*> USCS_Node::GetAllNodes() const
{
	TArray<const USCS_Node*> AllNodes;

	//  first add ourself
	AllNodes.Add(this);

	// then add each child (including all their children)
	for(int32 ChildIdx=0; ChildIdx<ChildNodes.Num(); ChildIdx++)
	{
		const USCS_Node* ChildNode = ChildNodes[ChildIdx];
		check(ChildNode != NULL);
		AllNodes.Append( ChildNode->GetAllNodes() );
	}

	return AllNodes;
}

bool USCS_Node::IsChildOf(USCS_Node* TestParent)
{
	TArray<USCS_Node*> AllNodes;
	if(TestParent != NULL)
	{
		AllNodes = TestParent->GetAllNodes();
	}
	return AllNodes.Contains(this);
}

void USCS_Node::PreloadChain()
{
	if( HasAnyFlags(RF_NeedLoad) )
	{
		GetLinker()->Preload(this);
	}

	if (ComponentTemplate && ComponentTemplate->HasAnyFlags(RF_NeedLoad))
	{
		ComponentTemplate->GetLinker()->Preload(ComponentTemplate);
	}

	for( TArray<USCS_Node*>::TIterator ChildIt(ChildNodes); ChildIt; ++ChildIt )
	{
		USCS_Node* CurrentChild = *ChildIt;
		if( CurrentChild )
		{
			CurrentChild->PreloadChain();
		}
	}
}

bool USCS_Node::IsRootNode() const
{
	USimpleConstructionScript* SCS = GetSCS();
	return(SCS->GetRootNodes().Contains(const_cast<USCS_Node*>(this)));
}

FName USCS_Node::GetVariableName() const
{
	// Name specified
	if(VariableName != NAME_None)
	{
		return VariableName;
	}
	// Not specified, make variable based on template name
	// Note that since SCS_Nodes should all have auto generated names, this code shouldn't be hit unless
	// the auto naming code fails.
	else if(ComponentTemplate != NULL)
	{
		FString VarString = FString::Printf(TEXT("%s_Var"), *ComponentTemplate->GetName());
		return FName(*VarString);
	}
	else
	{
		return NAME_None;
	}
}

void USCS_Node::NameWasModified()
{
	if(ComponentTemplate != nullptr)
	{
		// Ensure that the template name stays in sync with the variable name; otherwise, new SCS nodes for the same component type will recycle the subobject rather than create a new instance.
		ComponentTemplate->Rename(*(VariableName.ToString() + TEXT("_GEN_VARIABLE")), nullptr, REN_DontCreateRedirectors|REN_ForceNoResetLoaders);
	}

	OnNameChangedExternal.ExecuteIfBound(VariableName);
}

void USCS_Node::SetOnNameChanged( const FSCSNodeNameChanged& OnChange )
{
	OnNameChangedExternal = OnChange;
}

int32 USCS_Node::FindMetaDataEntryIndexForKey(const FName& Key)
{
	for(int32 i=0; i<MetaDataArray.Num(); i++)
	{
		if(MetaDataArray[i].DataKey == Key)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

FString USCS_Node::GetMetaData(const FName& Key)
{
	int32 EntryIndex = FindMetaDataEntryIndexForKey(Key);
	check(EntryIndex != INDEX_NONE);
	return MetaDataArray[EntryIndex].DataValue;
}

void USCS_Node::SetMetaData(const FName& Key, const FString& Value)
{
	int32 EntryIndex = FindMetaDataEntryIndexForKey(Key);
	if(EntryIndex != INDEX_NONE)
	{
		MetaDataArray[EntryIndex].DataValue = Value;
	}
	else
	{
		MetaDataArray.Add( FBPVariableMetaDataEntry(Key, Value) );
	}
}

void USCS_Node::RemoveMetaData(const FName& Key)
{
	int32 EntryIndex = FindMetaDataEntryIndexForKey(Key);
	if(EntryIndex != INDEX_NONE)
	{
		MetaDataArray.RemoveAt(EntryIndex);
	}
}

void USCS_Node::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	ValidateGuid();
#endif

	// If valid, load cooked component instancing data.
	if (ComponentTemplate && CookedComponentInstancingData.bIsValid)
	{
		CookedComponentInstancingData.LoadCachedPropertyDataForSerialization(ComponentTemplate);
	}
}

#if WITH_EDITOR
void USCS_Node::SetParent(USCS_Node* InParentNode)
{
	check(InParentNode != NULL);
	check(InParentNode->GetSCS() != NULL);
	check(InParentNode->GetSCS()->GetBlueprint() != NULL);
	check(InParentNode->GetSCS()->GetBlueprint()->GeneratedClass != NULL);

	const FName NewParentComponentOrVariableName = InParentNode->VariableName;
	const FName NewParentComponentOwnerClassName = InParentNode->GetSCS()->GetBlueprint()->GeneratedClass->GetFName();

	// Only modify if it differs from current
	if(bIsParentComponentNative
		|| ParentComponentOrVariableName != NewParentComponentOrVariableName
		|| ParentComponentOwnerClassName != NewParentComponentOwnerClassName)
	{
		Modify();

		bIsParentComponentNative = false;
		ParentComponentOrVariableName = NewParentComponentOrVariableName;
		ParentComponentOwnerClassName = NewParentComponentOwnerClassName;
	}
}

void USCS_Node::SetParent(USceneComponent* InParentComponent)
{
	check(InParentComponent != NULL);

	const FName NewParentComponentOrVariableName = InParentComponent->GetFName();
	const FName NewParentComponentOwnerClassName = NAME_None;

	// Only modify if it differs from current
	if(!bIsParentComponentNative
		|| ParentComponentOrVariableName != NewParentComponentOrVariableName
		|| ParentComponentOwnerClassName != NewParentComponentOwnerClassName)
	{
		Modify();

		bIsParentComponentNative = true;
		ParentComponentOrVariableName = NewParentComponentOrVariableName;
		ParentComponentOwnerClassName = NewParentComponentOwnerClassName;
	}
}

USceneComponent* USCS_Node::GetParentComponentTemplate(UBlueprint* InBlueprint) const
{
	USceneComponent* ParentComponentTemplate = NULL;
	if(ParentComponentOrVariableName != NAME_None)
	{
		check(InBlueprint != NULL && InBlueprint->GeneratedClass != NULL);

		// If the parent component template is found in the 'Components' array of the CDO (i.e. native)
		if(bIsParentComponentNative)
		{
			// Access the Blueprint CDO
			AActor* CDO = InBlueprint->GeneratedClass->GetDefaultObject<AActor>();
			if(CDO != NULL)
			{
				// Find the component template in the CDO that matches the specified name
				TInlineComponentArray<USceneComponent*> Components;
				CDO->GetComponents(Components);

				for(auto CompIt = Components.CreateIterator(); CompIt; ++CompIt)
				{
					USceneComponent* CompTemplate = *CompIt;
					if(CompTemplate->GetFName() == ParentComponentOrVariableName)
					{
						// Found a match; this is our parent, we're done
						ParentComponentTemplate = CompTemplate;
						break;
					}
				}
			}
		}
		// Otherwise the parent component template is found in a parent Blueprint's SCS tree (i.e. non-native)
		else
		{
			// Get the Blueprint hierarchy
			TArray<UBlueprint*> ParentBPStack;
			UBlueprint::GetBlueprintHierarchyFromClass(InBlueprint->GeneratedClass, ParentBPStack);

			// Find the parent Blueprint in the hierarchy
			for(int32 StackIndex = ParentBPStack.Num() - 1; StackIndex > 0; --StackIndex)
			{
				UBlueprint* ParentBlueprint = ParentBPStack[StackIndex];
				if(ParentBlueprint != NULL
					&& ParentBlueprint->SimpleConstructionScript != NULL
					&& ParentBlueprint->GeneratedClass->GetFName() == ParentComponentOwnerClassName)
				{
					// Find the SCS node with a variable name that matches the specified name
					TArray<USCS_Node*> ParentSCSNodes = ParentBlueprint->SimpleConstructionScript->GetAllNodes();
					for(int32 ParentNodeIndex = 0; ParentNodeIndex < ParentSCSNodes.Num(); ++ParentNodeIndex)
					{
						USceneComponent* CompTemplate = Cast<USceneComponent>(ParentSCSNodes[ParentNodeIndex]->ComponentTemplate);
						if(CompTemplate != NULL && ParentSCSNodes[ParentNodeIndex]->VariableName == ParentComponentOrVariableName)
						{
							// Found a match; this is our parent, we're done
							ParentComponentTemplate = Cast<USceneComponent>(ParentSCSNodes[ParentNodeIndex]->GetActualComponentTemplate(Cast<UBlueprintGeneratedClass>(InBlueprint->GeneratedClass)));
							break;
						}
					}
				}
			}
		}
	}

	return ParentComponentTemplate;
}

void USCS_Node::ValidateGuid()
{
	// Backward compatibility:
	// The guid for the node should be always the same (event when it's not saved). 
	// The guid is created in an deterministic way using persistent name.
	if (!VariableGuid.IsValid() && (VariableName != NAME_None))
	{
		const FString HashString = VariableName.ToString();
		ensure(HashString.Len());

		const uint32 BufferLength = HashString.Len() * sizeof(HashString[0]);
		uint32 HashBuffer[5];
		FSHA1::HashBuffer(*HashString, BufferLength, reinterpret_cast<uint8*>(HashBuffer));
		VariableGuid = FGuid(HashBuffer[1], HashBuffer[2], HashBuffer[3], HashBuffer[4]);
	}
}

#endif
