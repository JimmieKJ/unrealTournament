// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "BlueprintUtilities.h"
#include "LatentActions.h"
#include "ComponentInstanceDataCache.h"
#include "Engine/LevelScriptActor.h"
#include "Engine/CullDistanceVolume.h"
#include "Components/ChildActorComponent.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Editor/UnrealEd/Public/Kismet2/BlueprintEditorUtils.h"
#include "Editor/UnrealEd/Public/Kismet2/KismetEditorUtilities.h"
#endif
#include "Engine/SimpleConstructionScript.h"

DEFINE_LOG_CATEGORY(LogBlueprintUserMessages);

DECLARE_CYCLE_STAT(TEXT("InstanceActorComponent"), STAT_InstanceActorComponent, STATGROUP_Engine);

//////////////////////////////////////////////////////////////////////////
// AActor Blueprint Stuff

static TArray<FRandomStream*> FindRandomStreams(AActor* InActor)
{
	check(InActor);
	TArray<FRandomStream*> OutStreams;
	UScriptStruct* RandomStreamStruct = TBaseStructure<FRandomStream>::Get();
	for( TFieldIterator<UStructProperty> It(InActor->GetClass()) ; It ; ++It )
	{
		UStructProperty* StructProp = *It;
		if( StructProp->Struct == RandomStreamStruct )
		{
			FRandomStream* StreamPtr = StructProp->ContainerPtrToValuePtr<FRandomStream>(InActor);
			OutStreams.Add(StreamPtr);
		}
	}
	return OutStreams;
}

#if WITH_EDITOR
void AActor::SeedAllRandomStreams()
{
	TArray<FRandomStream*> Streams = FindRandomStreams(this);
	for(int32 i=0; i<Streams.Num(); i++)
	{
		Streams[i]->GenerateNewSeed();
	}
}
#endif //WITH_EDITOR

void AActor::ResetPropertiesForConstruction()
{
	// Get class CDO
	AActor* Default = GetClass()->GetDefaultObject<AActor>();
	// RandomStream struct name to compare against
	const FName RandomStreamName(TEXT("RandomStream"));

	// We don't want to reset references to world object
	const bool bIsLevelScriptActor = IsA(ALevelScriptActor::StaticClass());

	// Iterate over properties
	for( TFieldIterator<UProperty> It(GetClass()) ; It ; ++It )
	{
		UProperty* Prop = *It;
		UStructProperty* StructProp = Cast<UStructProperty>(Prop);
		UClass* PropClass = CastChecked<UClass>(Prop->GetOuter()); // get the class that added this property

		bool const bCanEditInstanceValue = !Prop->HasAnyPropertyFlags(CPF_DisableEditOnInstance) &&
			Prop->HasAnyPropertyFlags(CPF_Edit);
		bool const bCanBeSetInBlueprints = Prop->HasAnyPropertyFlags(CPF_BlueprintVisible) && 
			!Prop->HasAnyPropertyFlags(CPF_BlueprintReadOnly);

		// First see if it is a random stream, if so reset before running construction script
		if( (StructProp != NULL) && (StructProp->Struct != NULL) && (StructProp->Struct->GetFName() == RandomStreamName) )
		{
			FRandomStream* StreamPtr =  StructProp->ContainerPtrToValuePtr<FRandomStream>(this);
			StreamPtr->Reset();
		}
		// If it is a blueprint exposed variable that is not editable per-instance, reset to default before running construction script
		else if( !bIsLevelScriptActor 
				&& !bCanEditInstanceValue
				&& bCanBeSetInBlueprints
				&& !Prop->IsA(UDelegateProperty::StaticClass()) 
				&& !Prop->IsA(UMulticastDelegateProperty::StaticClass())
				&& !Prop->ContainsInstancedObjectProperty())
		{
			Prop->CopyCompleteValue_InContainer(this, Default);
		}
	}
}

 int32 CalcComponentAttachDepth(UActorComponent* InComp, TMap<UActorComponent*, int32>& ComponentDepthMap) 
 {
	int32 ComponentDepth = 0;
	int32* CachedComponentDepth = ComponentDepthMap.Find(InComp);
	if (CachedComponentDepth)
	{
		ComponentDepth = *CachedComponentDepth;
	}
	else
	{
		if (USceneComponent* SC = Cast<USceneComponent>(InComp))
		{
			if (SC->AttachParent && SC->AttachParent->GetOwner() == InComp->GetOwner())
			{
				ComponentDepth = CalcComponentAttachDepth(SC->AttachParent, ComponentDepthMap) + 1;
			}
		}
		ComponentDepthMap.Add(InComp, ComponentDepth);
	}

	return ComponentDepth;
 }

//* Destroys the constructed components.
void AActor::DestroyConstructedComponents()
{
	// Remove all existing components
	TInlineComponentArray<UActorComponent*> PreviouslyAttachedComponents;
	GetComponents(PreviouslyAttachedComponents);

	TMap<UActorComponent*, int32> ComponentDepthMap;

	for (UActorComponent* Component : PreviouslyAttachedComponents)
	{
		if (Component)
		{
			CalcComponentAttachDepth(Component, ComponentDepthMap);
		}
	}

	ComponentDepthMap.ValueSort([](const int32& A, const int32& B)
	{
		return A > B;
	});

	for (const TPair<UActorComponent*,int32>& ComponentAndDepth : ComponentDepthMap)
	{
		UActorComponent* Component = ComponentAndDepth.Key;
		if (Component)
		{
			bool bDestroyComponent = false;
			if (Component->IsCreatedByConstructionScript())
			{
				bDestroyComponent = true;
			}
			else
			{
				UActorComponent* OuterComponent = Component->GetTypedOuter<UActorComponent>();
				while (OuterComponent)
				{
					if (OuterComponent->IsCreatedByConstructionScript())
					{
						bDestroyComponent = true;
						break;
					}
					OuterComponent = OuterComponent->GetTypedOuter<UActorComponent>();
				}
			}

			if (bDestroyComponent)
			{
				if (Component == RootComponent)
				{
					RootComponent = NULL;
				}

				Component->DestroyComponent();

				// Rename component to avoid naming conflicts in the case where we rerun the SCS and name the new components the same way.
				FName const NewBaseName( *(FString::Printf(TEXT("TRASH_%s"), *Component->GetClass()->GetName())) );
				FName const NewObjectName = MakeUniqueObjectName(this, GetClass(), NewBaseName);
				Component->Rename(*NewObjectName.ToString(), this, REN_ForceNoResetLoaders|REN_DontCreateRedirectors|REN_NonTransactional);
			}
		}
	}
}

void AActor::RerunConstructionScripts()
{
	checkf(!HasAnyFlags(RF_ClassDefaultObject), TEXT("RerunConstructionScripts should never be called on a CDO as it can mutate the transient data on the CDO which then propagates to instances!"));

	FEditorScriptExecutionGuard ScriptGuard;
	// don't allow (re)running construction scripts on dying actors
	bool bAllowReconstruction = !IsPendingKill() && !HasAnyFlags(RF_BeginDestroyed|RF_FinishDestroyed);
#if WITH_EDITOR
	if(bAllowReconstruction && GIsEditor)
	{
		// Generate the blueprint hierarchy for this actor
		TArray<UBlueprint*> ParentBPStack;
		bAllowReconstruction = UBlueprint::GetBlueprintHierarchyFromClass(GetClass(), ParentBPStack);
		if(bAllowReconstruction)
		{
			for(int i = ParentBPStack.Num() - 1; i > 0 && bAllowReconstruction; --i)
			{
				const UBlueprint* ParentBP = ParentBPStack[i];
				if(ParentBP && ParentBP->bBeingCompiled)
				{
					// don't allow (re)running construction scripts if a parent BP is being compiled
					bAllowReconstruction = false;
				}
			}
		}
	}
#endif
	if(bAllowReconstruction)
	{
		// Set global flag to let system know we are reconstructing blueprint instances
		TGuardValue<bool> GuardTemplateNameFlag(GIsReconstructingBlueprintInstances, true);

		// Temporarily suspend the undo buffer; we don't need to record reconstructed component objects into the current transaction
		ITransaction* CurrentTransaction = GUndo;
		GUndo = NULL;
		
		// Create cache to store component data across rerunning construction scripts
#if WITH_EDITOR
		FActorTransactionAnnotation* ActorTransactionAnnotation = CurrentTransactionAnnotation.Get();
#endif
		FComponentInstanceDataCache* InstanceDataCache;
		
		FTransform OldTransform = FTransform::Identity;
		FName  SocketName;
		AActor* Parent = nullptr;
		USceneComponent* AttachParentComponent = nullptr;

		bool bUseRootComponentProperties = true;

		// Struct to store info about attached actors
		struct FAttachedActorInfo
		{
			AActor* AttachedActor;
			FName AttachedToSocket;
			bool bSetRelativeTransform;
			FTransform RelativeTransform;
		};

		// Save info about attached actors
		TArray<FAttachedActorInfo> AttachedActorInfos;

#if WITH_EDITOR
		if (ActorTransactionAnnotation)
		{
			InstanceDataCache = &ActorTransactionAnnotation->ComponentInstanceData;

			if (ActorTransactionAnnotation->bRootComponentDataCached)
			{
				OldTransform = ActorTransactionAnnotation->RootComponentData.Transform;
				Parent = ActorTransactionAnnotation->RootComponentData.AttachedParentInfo.Actor.Get();
				if (Parent)
				{
					USceneComponent* AttachParent = ActorTransactionAnnotation->RootComponentData.AttachedParentInfo.AttachParent.Get();
					AttachParentComponent = (AttachParent ? AttachParent : FindObjectFast<USceneComponent>(Parent, ActorTransactionAnnotation->RootComponentData.AttachedParentInfo.AttachParentName));
					SocketName = ActorTransactionAnnotation->RootComponentData.AttachedParentInfo.SocketName;
					DetachRootComponentFromParent();
				}

				for (const auto& CachedAttachInfo : ActorTransactionAnnotation->RootComponentData.AttachedToInfo)
				{
					AActor* AttachedActor = CachedAttachInfo.Actor.Get();
					if (AttachedActor)
					{
						FAttachedActorInfo Info;
						Info.AttachedActor = AttachedActor;
						Info.AttachedToSocket = CachedAttachInfo.SocketName;
						Info.bSetRelativeTransform = true;
						Info.RelativeTransform = CachedAttachInfo.RelativeTransform;
						AttachedActorInfos.Add(Info);

						AttachedActor->DetachRootComponentFromParent();
					}
				}

				bUseRootComponentProperties = false;
			}
		}
		else
#endif
		{
			InstanceDataCache = new FComponentInstanceDataCache(this);

			// If there are attached objects detach them and store the socket names
			TArray<AActor*> AttachedActors;
			GetAttachedActors(AttachedActors);

			for (AActor* AttachedActor : AttachedActors)
			{
				// We don't need to detach child actors, that will be handled by component tear down
				if (!AttachedActor->ParentComponent.IsValid())
				{
					USceneComponent* EachRoot = AttachedActor->GetRootComponent();
					// If the component we are attached to is about to go away...
					if (EachRoot && EachRoot->GetAttachParent() && EachRoot->GetAttachParent()->IsCreatedByConstructionScript())
					{
						// Save info about actor to reattach
						FAttachedActorInfo Info;
						Info.AttachedActor = AttachedActor;
						Info.AttachedToSocket = EachRoot->GetAttachSocketName();
						Info.bSetRelativeTransform = false;
						AttachedActorInfos.Add(Info);

						// Now detach it
						AttachedActor->Modify();
						EachRoot->DetachFromParent(true);
					}
				}
				else
				{
					check(AttachedActor->ParentComponent->GetOwner() == this);
				}
			}
		}

		if (bUseRootComponentProperties && RootComponent != nullptr)
		{
			// Do not need to detach if root component is not going away
			if (RootComponent->GetAttachParent() != NULL && RootComponent->IsCreatedByConstructionScript())
			{
				Parent = RootComponent->GetAttachParent()->GetOwner();
				// Root component should never be attached to another component in the same actor!
				if (Parent == this)
				{
					UE_LOG(LogActor, Warning, TEXT("RerunConstructionScripts: RootComponent (%s) attached to another component in this Actor (%s)."), *RootComponent->GetPathName(), *Parent->GetPathName());
					Parent = NULL;
				}
				AttachParentComponent = RootComponent->GetAttachParent();
				SocketName = RootComponent->GetAttachSocketName();
				//detach it to remove any scaling 
				RootComponent->DetachFromParent(true);
			}

			// Update component transform and remember it so it can be reapplied to any new root component which exists after construction.
			// (Component transform may be stale if we are here following an Undo)
			RootComponent->UpdateComponentToWorld();
			OldTransform = RootComponent->ComponentToWorld;
		}

#if WITH_EDITOR
		// Save the current construction script-created components by name
		TMap<const FName, UObject*> DestroyedComponentsByName;
		TInlineComponentArray<UActorComponent*> PreviouslyAttachedComponents;
		GetComponents(PreviouslyAttachedComponents);
		for (auto Component : PreviouslyAttachedComponents)
		{
			if (Component)
			{
				if (Component->IsCreatedByConstructionScript())
				{

					DestroyedComponentsByName.Add(Component->GetFName(), Component);
				}
				else
				{
					UActorComponent* OuterComponent = Component->GetTypedOuter<UActorComponent>();
					while (OuterComponent)
					{
						if (OuterComponent->IsCreatedByConstructionScript())
						{
							DestroyedComponentsByName.Add(Component->GetFName(), Component);
							break;
						}
						OuterComponent = OuterComponent->GetTypedOuter<UActorComponent>();
					}
				}
			}
		}
#endif

		// Destroy existing components
		DestroyConstructedComponents();

		// Reset random streams
		ResetPropertiesForConstruction();

		// Exchange net roles before running construction scripts
		UWorld *OwningWorld = GetWorld();
		if (OwningWorld && !OwningWorld->IsServer())
		{
			ExchangeNetRoles(true);
		}

		// Run the construction scripts
		ExecuteConstruction(OldTransform, InstanceDataCache);

		if(Parent)
		{
			USceneComponent* ChildRoot = GetRootComponent();
			if (AttachParentComponent == nullptr)
			{
				AttachParentComponent = Parent->GetRootComponent();
			}
			if (ChildRoot != nullptr && AttachParentComponent != nullptr)
			{
				ChildRoot->AttachTo(AttachParentComponent, SocketName, EAttachLocation::KeepWorldPosition);
			}
		}

		// If we had attached children reattach them now - unless they are already attached
		for(FAttachedActorInfo& Info : AttachedActorInfos)
		{
			// If this actor is no longer attached to anything, reattach
			if (!Info.AttachedActor->IsPendingKill() && Info.AttachedActor->GetAttachParentActor() == nullptr)
			{
				USceneComponent* ChildRoot = Info.AttachedActor->GetRootComponent();
				if (ChildRoot && ChildRoot->GetAttachParent() != RootComponent)
				{
					ChildRoot->AttachTo(RootComponent, Info.AttachedToSocket, EAttachLocation::KeepWorldPosition);
					if (Info.bSetRelativeTransform)
					{
						ChildRoot->SetRelativeTransform(Info.RelativeTransform);
					}
					ChildRoot->UpdateComponentToWorld();
				}
			}
		}

		// Restore the undo buffer
		GUndo = CurrentTransaction;

#if WITH_EDITOR
		// Create the mapping of old->new components and notify the editor of the replacements
		TMap<UObject*, UObject*> OldToNewComponentMapping;

		TInlineComponentArray<UActorComponent*> NewComponents;
		GetComponents(NewComponents);
		for (auto NewComp : NewComponents)
		{
			const FName NewCompName = NewComp->GetFName();
			if (DestroyedComponentsByName.Contains(NewCompName))
			{
				OldToNewComponentMapping.Add(DestroyedComponentsByName[NewCompName], NewComp);
			}
		}

		if (GEditor && (OldToNewComponentMapping.Num() > 0))
		{
			GEditor->NotifyToolsOfObjectReplacement(OldToNewComponentMapping);
		}

		if (ActorTransactionAnnotation)
		{
			CurrentTransactionAnnotation = NULL;
		}
		else
#endif
		{
			delete InstanceDataCache;
		}

	}
}

void AActor::ExecuteConstruction(const FTransform& Transform, const FComponentInstanceDataCache* InstanceDataCache, bool bIsDefaultTransform)
{
	check(!IsPendingKill());
	check(!HasAnyFlags(RF_BeginDestroyed|RF_FinishDestroyed));

	// ensure that any existing native root component gets this new transform
	// we can skip this in the default case as the given transform will be the root component's transform
	if (RootComponent && !bIsDefaultTransform)
	{
		RootComponent->SetWorldTransform(Transform);
	}

	// Generate the parent blueprint hierarchy for this actor, so we can run all the construction scripts sequentially
	TArray<const UBlueprintGeneratedClass*> ParentBPClassStack;
	const bool bErrorFree = UBlueprintGeneratedClass::GetGeneratedClassesHierarchy(GetClass(), ParentBPClassStack);

	TArray<const UDynamicClass*> ParentDynamicClassStack;
	for (UClass* ClassIt = GetClass(); ClassIt; ClassIt = ClassIt->GetSuperClass())
	{
		if (UDynamicClass* DynamicClass = Cast<UDynamicClass>(ClassIt))
		{
			ParentDynamicClassStack.Add(DynamicClass);
		}
	}
	for (int32 i = ParentDynamicClassStack.Num() - 1; i >= 0; i--)
	{
		UBlueprintGeneratedClass::CreateComponentsForActor(ParentDynamicClassStack[i], this);
	}

	// If this actor has a blueprint lineage, go ahead and run the construction scripts from least derived to most
	if( (ParentBPClassStack.Num() > 0)  )
	{
		if( bErrorFree )
		{
			// Prevent user from spawning actors in User Construction Script
			TGuardValue<bool> AutoRestoreISCS(GetWorld()->bIsRunningConstructionScript, true);
			for( int32 i = ParentBPClassStack.Num() - 1; i >= 0; i-- )
			{
				const UBlueprintGeneratedClass* CurrentBPGClass = ParentBPClassStack[i];
				check(CurrentBPGClass);
				if(CurrentBPGClass->SimpleConstructionScript)
				{
					CurrentBPGClass->SimpleConstructionScript->ExecuteScriptOnActor(this, Transform, bIsDefaultTransform);
				}
				// Now that the construction scripts have been run, we can create timelines and hook them up
				UBlueprintGeneratedClass::CreateComponentsForActor(CurrentBPGClass, this);
			}

			// If we passed in cached data, we apply it now, so that the UserConstructionScript can use the updated values
			if(InstanceDataCache)
			{
				InstanceDataCache->ApplyToActor(this, ECacheApplyPhase::PostSimpleConstructionScript);
			}

#if WITH_EDITOR
			bool bDoUserConstructionScript;
			GConfig->GetBool(TEXT("Kismet"), TEXT("bTurnOffEditorConstructionScript"), bDoUserConstructionScript, GEngineIni);
			if (!GIsEditor || !bDoUserConstructionScript)
#endif
			{
				// Then run the user script, which is responsible for calling its own super, if desired
				ProcessUserConstructionScript();
			}

			// Since re-run construction scripts will never be run and we want to keep dynamic spawning fast, don't spend time
			// determining the UCS modified properties in game worlds
			if (!GetWorld()->IsGameWorld())
			{
				for (UActorComponent* Component : GetComponents())
				{
					if (Component)
					{
						Component->DetermineUCSModifiedProperties();
					}
				}
			}

			// Bind any delegates on components			
			UBlueprintGeneratedClass::BindDynamicDelegates(GetClass(), this); // We have a BP stack, we must have a UBlueprintGeneratedClass...

			// Apply any cached data procedural components
			// @TODO Don't re-apply to components we already applied to above
			if (InstanceDataCache)
			{
				InstanceDataCache->ApplyToActor(this, ECacheApplyPhase::PostUserConstructionScript);
			}
		}
		else
		{
			// Disaster recovery mode; create a dummy billboard component to retain the actor location
			// until the compile error can be fixed
			if (RootComponent == NULL)
			{
				UBillboardComponent* BillboardComponent = NewObject<UBillboardComponent>(this);
				BillboardComponent->SetFlags(RF_Transactional);
				BillboardComponent->CreationMethod = EComponentCreationMethod::SimpleConstructionScript;
#if WITH_EDITOR
				BillboardComponent->Sprite = (UTexture2D*)(StaticLoadObject(UTexture2D::StaticClass(), NULL, TEXT("/Engine/EditorResources/BadBlueprintSprite.BadBlueprintSprite"), NULL, LOAD_None, NULL));
#endif
				BillboardComponent->SetRelativeTransform(Transform);

				SetRootComponent(BillboardComponent);
				FinishAndRegisterComponent(BillboardComponent);
			}
		}
	}
	else
	{
#if WITH_EDITOR
		bool bDoUserConstructionScript;
		GConfig->GetBool(TEXT("Kismet"), TEXT("bTurnOffEditorConstructionScript"), bDoUserConstructionScript, GEngineIni);
		if (!GIsEditor || !bDoUserConstructionScript)
#endif
		{
			// Then run the user script, which is responsible for calling its own super, if desired
			ProcessUserConstructionScript();
		}
		UBlueprintGeneratedClass::BindDynamicDelegates(GetClass(), this);
	}

	GetWorld()->UpdateCullDistanceVolumes(this);

	// Now run virtual notification
	OnConstruction(Transform);
}

void AActor::ProcessUserConstructionScript()
{
	// Set a flag that this actor is currently running UserConstructionScript.
	bRunningUserConstructionScript = true;
	UserConstructionScript();
	bRunningUserConstructionScript = false;

	// Validate component mobility after UCS execution
	TInlineComponentArray<USceneComponent*> SceneComponents;
	GetComponents(SceneComponents);
	for (auto SceneComponent : SceneComponents)
	{
		// A parent component can't be more mobile than its children, so we check for that here and adjust as needed.
		if(SceneComponent != RootComponent && SceneComponent->GetAttachParent() != nullptr && SceneComponent->GetAttachParent()->Mobility > SceneComponent->Mobility)
		{
			if(SceneComponent->IsA<UStaticMeshComponent>())
			{
				// SMCs can't be stationary, so always set them (and any children) to be movable
				SceneComponent->SetMobility(EComponentMobility::Movable);
			}
			else
			{
				// Set the new component (and any children) to be at least as mobile as its parent
				SceneComponent->SetMobility(SceneComponent->GetAttachParent()->Mobility);
			}
		}
	}
}

void AActor::FinishAndRegisterComponent(UActorComponent* Component)
{
	Component->RegisterComponent();
	BlueprintCreatedComponents.Add(Component);
}

UActorComponent* AActor::CreateComponentFromTemplate(UActorComponent* Template, const FString& InName)
{
	return CreateComponentFromTemplate(Template, FName(*InName));
}

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> CVarLogBlueprintComponentInstanceCalls(
	TEXT("LogBlueprintComponentInstanceCalls"),
	0,
	TEXT("Log Blueprint Component instance calls; debugging."));
#endif

UActorComponent* AActor::CreateComponentFromTemplate(UActorComponent* Template, const FName InName)
{
	SCOPE_CYCLE_COUNTER(STAT_InstanceActorComponent);

	UActorComponent* NewActorComp = nullptr;
	if (Template != nullptr)
	{
#if !UE_BUILD_SHIPPING
		const double StartTime = FPlatformTime::Seconds();
#endif
		// Resolve any name conflicts.
		CheckComponentInstanceName(InName);

		//Make sure, that the name of the instance is different than the name of the template. Otherwise, the template could be handled as an archetype of the instance.
		const FName NewComponentName = (InName != NAME_None) ? InName : MakeUniqueObjectName(this, Template->GetClass(), Template->GetFName());
		ensure(NewComponentName != Template->GetFName());

		// Note we aren't copying the the RF_ArchetypeObject flag. Also note the result is non-transactional by default.
		NewActorComp = (UActorComponent*)StaticDuplicateObject(Template, this, NewComponentName, RF_AllFlags & ~(RF_ArchetypeObject | RF_Transactional | RF_WasLoaded | RF_Public | RF_InheritableComponentTemplate));

		// Handle post-creation tasks.
		PostCreateBlueprintComponent(NewActorComp);

#if !UE_BUILD_SHIPPING
		if (CVarLogBlueprintComponentInstanceCalls.GetValueOnGameThread())
		{
			UE_LOG(LogBlueprint, Log, TEXT("%s: CreateComponentFromTemplate() - %s \'%s\' completed in %.02g ms"), *GetName(), *Template->GetClass()->GetName(), *NewComponentName.ToString(), (FPlatformTime::Seconds() - StartTime) * 1000.0);
		}
#endif
	}
	return NewActorComp;
}

UActorComponent* AActor::CreateComponentFromTemplateData(const FBlueprintCookedComponentInstancingData* TemplateData, const FName InName)
{
	SCOPE_CYCLE_COUNTER(STAT_InstanceActorComponent);

	// Component instance data loader implementation.
	class FBlueprintComponentInstanceDataLoader : public FObjectReader
	{
	public:
		FBlueprintComponentInstanceDataLoader(const TArray<uint8>& InSrcBytes, const FCustomPropertyListNode* InPropertyList)
			:FObjectReader(const_cast<TArray<uint8>&>(InSrcBytes))
		{
			ArCustomPropertyList = InPropertyList;
			ArUseCustomPropertyList = true;
			ArWantBinaryPropertySerialization = true;
		}
	};

	UActorComponent* NewActorComp = nullptr;
	if (TemplateData != nullptr && TemplateData->ComponentTemplateClass != nullptr)	// some components (e.g. UTextRenderComponent) are not loaded on a server (or client). Handle that gracefully, but we ideally shouldn't even get here (see UEBP-175).
	{
#if !UE_BUILD_SHIPPING
		const double StartTime = FPlatformTime::Seconds();
#endif
		// Resolve any name conflicts.
		CheckComponentInstanceName(InName);

		//Make sure, that the name of the instance is different than the name of the template. Otherwise, the template could be handled as an archetype of the instance.
		const FName NewComponentName = (InName != NAME_None) ? InName : MakeUniqueObjectName(this, TemplateData->ComponentTemplateClass, TemplateData->ComponentTemplateName);
		ensure(NewComponentName != TemplateData->ComponentTemplateName);

		// Note we aren't copying the the RF_ArchetypeObject flag. Also note the result is non-transactional by default.
		NewActorComp = NewObject<UActorComponent>(
			this,
			TemplateData->ComponentTemplateClass,
			NewComponentName,
			EObjectFlags(TemplateData->ComponentTemplateFlags) & ~(RF_ArchetypeObject | RF_Transactional | RF_WasLoaded | RF_Public | RF_InheritableComponentTemplate)
		);

		// Load cached data into the new instance.
		FBlueprintComponentInstanceDataLoader ComponentInstanceDataLoader(TemplateData->GetCachedPropertyDataForSerialization(), TemplateData->GetCachedPropertyListForSerialization());
		NewActorComp->Serialize(ComponentInstanceDataLoader);

		// Handle post-creation tasks.
		PostCreateBlueprintComponent(NewActorComp);

#if !UE_BUILD_SHIPPING
		if (CVarLogBlueprintComponentInstanceCalls.GetValueOnGameThread())
		{
			UE_LOG(LogBlueprint, Log, TEXT("%s: CreateComponentFromTemplateData() - %s \'%s\' completed in %.02g ms"), *GetName(), *TemplateData->ComponentTemplateClass->GetName(), *NewComponentName.ToString(), (FPlatformTime::Seconds() - StartTime) * 1000.0);
		}
#endif
	}
	return NewActorComp;
}

UActorComponent* AActor::AddComponent(FName TemplateName, bool bManualAttachment, const FTransform& RelativeTransform, const UObject* ComponentTemplateContext)
{
	UActorComponent* Template = nullptr;
	FBlueprintCookedComponentInstancingData* TemplateData = nullptr;
	for (UClass* TemplateOwnerClass = (ComponentTemplateContext != nullptr) ? ComponentTemplateContext->GetClass() : GetClass()
		; TemplateOwnerClass && !Template && !TemplateData
		; TemplateOwnerClass = TemplateOwnerClass->GetSuperClass())
	{
		if (auto BPGC = Cast<UBlueprintGeneratedClass>(TemplateOwnerClass))
		{
			// Use cooked instancing data if available (fast path).
			if (FPlatformProperties::RequiresCookedData())
			{
				TemplateData = BPGC->CookedComponentInstancingData.Find(TemplateName);
			}
			
			if (!TemplateData || !TemplateData->bIsValid)
			{
				Template = BPGC->FindComponentTemplateByName(TemplateName);
			}
		}
		else if (auto DynamicClass = Cast<UDynamicClass>(TemplateOwnerClass))
		{
			UObject** FoundTemplatePtr = DynamicClass->ComponentTemplates.FindByPredicate([=](UObject* Obj) -> bool
			{
				return Obj && Obj->IsA<UActorComponent>() && (Obj->GetFName() == TemplateName);
			});
			Template = (nullptr != FoundTemplatePtr) ? Cast<UActorComponent>(*FoundTemplatePtr) : nullptr;
		}
	}

	bool bIsSceneComponent = false;
	UActorComponent* NewActorComp = TemplateData ? CreateComponentFromTemplateData(TemplateData) : CreateComponentFromTemplate(Template);
	if(NewActorComp != nullptr)
	{
		// Call function to notify component it has been created
		NewActorComp->OnComponentCreated();
		
		// The user has the option of doing attachment manually where they have complete control or via the automatic rule
		// that the first component added becomes the root component, with subsequent components attached to the root.
		USceneComponent* NewSceneComp = Cast<USceneComponent>(NewActorComp);
		if(NewSceneComp != nullptr)
		{
			if (!bManualAttachment)
			{
				if (RootComponent == nullptr)
				{
					RootComponent = NewSceneComp;
				}
				else
				{
					NewSceneComp->AttachTo(RootComponent);
				}
			}

			NewSceneComp->SetRelativeTransform(RelativeTransform);

			bIsSceneComponent = true;
		}

		// Register component, which will create physics/rendering state, now component is in correct position
		NewActorComp->RegisterComponent();

		UWorld* World = GetWorld();
		if (!bRunningUserConstructionScript && World && bIsSceneComponent)
		{
			UPrimitiveComponent* NewPrimitiveComponent = Cast<UPrimitiveComponent>(NewActorComp);
			if (NewPrimitiveComponent && ACullDistanceVolume::CanBeAffectedByVolumes(NewPrimitiveComponent))
			{
				World->UpdateCullDistanceVolumes(this, NewPrimitiveComponent);
			}
		}
	}

	return NewActorComp;
}

void AActor::CheckComponentInstanceName(const FName InName)
{
	// If there is a Component with this name already (almost certainly because it is an Instance component), we need to rename it out of the way
	if (!InName.IsNone())
	{
		UObject* ConflictingObject = FindObjectFast<UObject>(this, InName);
		if (ConflictingObject && ConflictingObject->IsA<UActorComponent>() && CastChecked<UActorComponent>(ConflictingObject)->CreationMethod == EComponentCreationMethod::Instance)
		{
			// Try and pick a good name
			FString ConflictingObjectName = ConflictingObject->GetName();
			int32 CharIndex = ConflictingObjectName.Len() - 1;
			while (FChar::IsDigit(ConflictingObjectName[CharIndex]))
			{
				--CharIndex;
			}
			int32 Counter = 0;
			if (CharIndex < ConflictingObjectName.Len() - 1)
			{
				Counter = FCString::Atoi(*ConflictingObjectName.RightChop(CharIndex + 1));
				ConflictingObjectName = ConflictingObjectName.Left(CharIndex + 1);
			}
			FString NewObjectName;
			do
			{
				NewObjectName = ConflictingObjectName + FString::FromInt(++Counter);

			} while (FindObjectFast<UObject>(this, *NewObjectName) != nullptr);

			ConflictingObject->Rename(*NewObjectName, this);
		}
	}
}

void AActor::PostCreateBlueprintComponent(UActorComponent* NewActorComp)
{
	if (NewActorComp)
	{
		NewActorComp->CreationMethod = EComponentCreationMethod::UserConstructionScript;

		// Need to do this so component gets saved - Components array is not serialized
		BlueprintCreatedComponents.Add(NewActorComp);
	}
}


