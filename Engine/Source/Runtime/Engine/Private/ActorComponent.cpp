// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
// ActorComponent.cpp: Actor component implementation.

#include "EnginePrivate.h"
#include "Engine/AssetUserData.h"
#include "Engine/LevelStreamingPersistent.h"
#include "Net/UnrealNetwork.h"
#include "MessageLog.h"
#include "UObjectToken.h"
#include "MapErrors.h"
#include "ComponentReregisterContext.h"
#include "Engine/SimpleConstructionScript.h"

#define LOCTEXT_NAMESPACE "ActorComponent"

DEFINE_LOG_CATEGORY(LogActorComponent);

/** Enable to log out all render state create, destroy and updatetransform events */
#define LOG_RENDER_STATE 0

/** Static var indicating activity of reregister context */
int32 FGlobalComponentReregisterContext::ActiveGlobalReregisterContextCount = 0;

FGlobalComponentReregisterContext::FGlobalComponentReregisterContext()
{
	ActiveGlobalReregisterContextCount++;

	// wait until resources are released
	FlushRenderingCommands();

	// Detach all actor components.
	for(auto* Component : TObjectRange<UActorComponent>())
	{
		new(ComponentContexts) FComponentReregisterContext(Component);
	}
}

FGlobalComponentReregisterContext::FGlobalComponentReregisterContext(const TArray<UClass*>& ExcludeComponents)
{
	ActiveGlobalReregisterContextCount++;

	// wait until resources are released
	FlushRenderingCommands();

	// Detach only actor components that are not in the excluded list
	for (auto* Component : TObjectRange<UActorComponent>())
	{
		bool bShouldReregister=true;
		for (UClass* ExcludeClass : ExcludeComponents)
		{
			if( ExcludeClass &&
				Component->IsA(ExcludeClass) )
			{
				bShouldReregister = false;
				break;
			}
		}
		if( bShouldReregister )
		{
			new(ComponentContexts) FComponentReregisterContext(Component);		
		}
	}
}

FGlobalComponentReregisterContext::FGlobalComponentReregisterContext(const TArray<AActor*>& InParentActors)
{
	ActiveGlobalReregisterContextCount++;

	// wait until resources are released
	FlushRenderingCommands();

	// Detach only actor components that are children of the actors list provided
	for (auto* Component : TObjectRange<UActorComponent>())
	{
		bool bShouldReregister=false;
		UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Component);
		if (PrimitiveComponent && PrimitiveComponent->ReplacementPrimitive.Get())
		{
			UPrimitiveComponent* ReplacementPrimitive = PrimitiveComponent->ReplacementPrimitive.Get();
			AActor* ParentActor = Cast<AActor>(ReplacementPrimitive->GetOuter());
			if (ParentActor && InParentActors.Contains(ParentActor))
			{
				bShouldReregister = true;
			}
		}
		if( bShouldReregister )
		{
			new(ComponentContexts) FComponentReregisterContext(Component);		
		}
	}
}


FGlobalComponentReregisterContext::~FGlobalComponentReregisterContext()
{
	check(ActiveGlobalReregisterContextCount > 0);
	// We empty the array now, to ensure that the FComponentReregisterContext destructors are called while ActiveGlobalReregisterContextCount still indicates activity
	ComponentContexts.Empty();
	ActiveGlobalReregisterContextCount--;
}

UActorComponent::UActorComponent(const FObjectInitializer& ObjectInitializer /*= FObjectInitializer::Get()*/)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.TickGroup = TG_DuringPhysics;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.bCanEverTick = false;

	CreationMethod = EComponentCreationMethod::Native;

	bAutoRegister = true;
	bNetAddressable = false;
}

void UActorComponent::PostInitProperties()
{
	Super::PostInitProperties();

	AActor* Owner = GetOwner();
	if (Owner)
	{
		Owner->AddOwnedComponent(this);
	}
}

void UActorComponent::PostLoad()
{
	Super::PostLoad();

	// TODO: Wrap all this up with an engine version, for now just don't do it in cooked builds since we know they are good
	if (!FPlatformProperties::RequiresCookedData())
	{
		if (bCreatedByConstructionScript_DEPRECATED)
		{
			CreationMethod = EComponentCreationMethod::SimpleConstructionScript;
		}
		else if (bInstanceComponent_DEPRECATED)
		{
			CreationMethod = EComponentCreationMethod::Instance;
		}

		if (CreationMethod == EComponentCreationMethod::SimpleConstructionScript)
		{
			UBlueprintGeneratedClass* Class = Cast/*Checked*/<UBlueprintGeneratedClass>(GetOuter()->GetClass());
			while (Class)
			{
				USimpleConstructionScript* SCS = Class->SimpleConstructionScript;
				if (SCS != nullptr && SCS->FindSCSNode(GetFName()))
				{
					break;
				}
				else
				{
					Class = Cast<UBlueprintGeneratedClass>(Class->GetSuperClass());
					if (Class == nullptr)
					{
						CreationMethod = EComponentCreationMethod::UserConstructionScript;
					}
				}
			}
		}
	}
}

bool UActorComponent::Rename( const TCHAR* InName, UObject* NewOuter, ERenameFlags Flags )
{
	bRoutedPostRename = false;

	const FName OldName = GetFName();
	const UObject* OldOuter = GetOuter();
	
	const bool bRenameSuccessful = Super::Rename(InName, NewOuter, Flags);
	
	const bool bMoved = (OldName != GetFName()) || (OldOuter != GetOuter());
	if (!bRoutedPostRename && ((Flags & REN_Test) == 0) && bMoved)
	{
		UE_LOG(LogActorComponent, Fatal, TEXT("%s failed to route PostRename.  Please call Super::PostRename() in your <className>::PostRename() function. "), *GetFullName() );
	}

	return bRenameSuccessful;
}

void UActorComponent::PostRename(UObject* OldOuter, const FName OldName)
{
	Super::PostRename(OldOuter, OldName);

	if (OldOuter != GetOuter())
	{
		AActor* Owner = GetOwner();
		AActor* OldOwner = (OldOuter->IsA<AActor>() ? CastChecked<AActor>(OldOuter) : OldOuter->GetTypedOuter<AActor>());

		if (Owner != OldOwner)
		{
			if (OldOwner)
			{
				OldOwner->RemoveOwnedComponent(this);
			}
			if (Owner)
			{
				Owner->AddOwnedComponent(this);
			}
		}
	}

	bRoutedPostRename = true;
}

bool UActorComponent::IsCreatedByConstructionScript() const
{
	return ((CreationMethod == EComponentCreationMethod::SimpleConstructionScript) || (CreationMethod == EComponentCreationMethod::UserConstructionScript));
}

#if WITH_EDITOR
void UActorComponent::CheckForErrors()
{
	AActor* Owner = GetOwner();

	if (Owner != NULL && GetClass()->HasAnyClassFlags(CLASS_Deprecated))
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("ComponentName"), FText::FromString(GetName()));
		Arguments.Add(TEXT("OwnerName"), FText::FromString(Owner->GetName()));
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(Owner))
			->AddToken(FTextToken::Create(FText::Format( LOCTEXT( "MapCheck_Message_DeprecatedClass", "{ComponentName}::{OwnerName} is obsolete and must be removed (Class is deprecated)" ), Arguments ) ) )
			->AddToken(FMapErrorToken::Create(FMapErrors::DeprecatedClass));
	}

	if (Owner != NULL && GetClass()->HasAnyClassFlags(CLASS_Abstract))
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("ComponentName"), FText::FromString(GetName()));
		Arguments.Add(TEXT("OwnerName"), FText::FromString(Owner->GetName()));
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(Owner))
			->AddToken(FTextToken::Create(FText::Format( LOCTEXT( "MapCheck_Message_AbstractClass", "{ComponentName}::{OwnerName} is obsolete and must be removed (Class is abstract)" ), Arguments ) ))
			->AddToken(FMapErrorToken::Create(FMapErrors::AbstractClass));
	}
}
#endif

bool UActorComponent::IsOwnerSelected() const
{
	AActor* Owner = GetOwner();
	return Owner && Owner->IsSelected();
}

AActor* UActorComponent::GetOwner() const
{
	// walk up outer chain to find an Actor
	UObject* Obj = GetOuter();
	while(Obj != NULL)
	{
		AActor* Actor = Cast<AActor>(Obj);
		if(Actor != NULL)
		{
			return Actor;
		}

		Obj = Obj->GetOuter();
	}

	return NULL;
}

UWorld* UActorComponent::GetWorld() const
{
	UWorld* ComponentWorld = World;
	if (ComponentWorld == NULL)
	{
		// If we don't have a world yet, it may be because we haven't gotten registered yet, but we can try to look at our owner
		AActor* Owner = GetOwner();
		if (Owner && !Owner->HasAnyFlags(RF_ClassDefaultObject))
		{
			ComponentWorld = Owner->GetWorld();
		}

		if( ComponentWorld == NULL )
		{
			// As a fallback check the outer of this component for a world. In some cases components are spawned directly in the world
			return Cast<UWorld>( GetOuter() );
		}
	}
	return ComponentWorld;
}

bool UActorComponent::ComponentHasTag(FName Tag) const
{
	return (Tag != NAME_None) && ComponentTags.Contains(Tag);
}


ENetMode UActorComponent::GetNetMode() const
{
	AActor *Owner = GetOwner();
	return Owner ? Owner->GetNetMode() : NM_Standalone;
}

FSceneInterface* UActorComponent::GetScene() const
{
	return (World ? World->Scene : NULL);
}

ULevel* UActorComponent::GetComponentLevel() const
{
	// For model components Level is outer object
	ULevel* ComponentLevel = Cast<ULevel>( GetOuter() );
	
	if (ComponentLevel == NULL)
	{
		AActor* Actor = GetOwner();
		if (Actor)
		{
			ComponentLevel = CastChecked<ULevel>( Actor->GetOuter() );
		}
	}
	
	return ComponentLevel;
}

bool UActorComponent::ComponentIsInLevel(const ULevel *TestLevel) const
{
	return (GetComponentLevel() == TestLevel);
}


bool UActorComponent::ComponentIsInPersistentLevel(bool bIncludeLevelStreamingPersistent) const
{
	ULevel* MyLevel = GetComponentLevel();
	AActor* MyActor = GetOwner();
	UWorld* MyWorld = GetWorld();

	if(MyActor == NULL || MyLevel == NULL || MyWorld == NULL)
	{
		return false;
	}

	return ( (MyLevel == MyWorld->PersistentLevel) || ( bIncludeLevelStreamingPersistent && MyWorld->StreamingLevels.Num() > 0 &&
														Cast<ULevelStreamingPersistent>(MyWorld->StreamingLevels[0]) != NULL &&
														MyWorld->StreamingLevels[0]->GetLoadedLevel() == MyLevel ) );
}

FString UActorComponent::GetReadableName() const
{
	FString Result = GetNameSafe(GetOwner()) + TEXT(".") + GetName();
	UObject const *Add = AdditionalStatObject();
	if (Add)
	{
		Result += TEXT(" ");
		Add->AppendName(Result);
	}
	return Result;
}

void UActorComponent::BeginDestroy()
{
	// Ensure that we call UninitializeComponent before we destroy this component
	if (bHasBeenInitialized)
	{
		UninitializeComponent();
	}

	ExecuteUnregisterEvents();

	// Ensure that we call OnComponentDestroyed before we destroy this component
	if(bHasBeenCreated)
	{
		OnComponentDestroyed();
	}

	World = NULL;

	// Remove from the parent's OwnedComponents list
	AActor* Owner = GetOwner();
	if (Owner)
	{
		Owner->RemoveOwnedComponent(this);
	}

	Super::BeginDestroy();
}


bool UActorComponent::NeedsLoadForClient() const
{
	check(GetOuter());
	return (GetOuter()->NeedsLoadForClient() && Super::NeedsLoadForClient());
}


bool UActorComponent::NeedsLoadForServer() const
{
	check(GetOuter());
	return (GetOuter()->NeedsLoadForServer() && Super::NeedsLoadForServer());
}

int32 UActorComponent::GetFunctionCallspace( UFunction* Function, void* Parameters, FFrame* Stack )
{
	AActor* Owner = GetOwner();
	if (Owner == NULL)
	{
		return FunctionCallspace::Local;
	}
	return Owner->GetFunctionCallspace(Function, Parameters, Stack);
}

bool UActorComponent::CallRemoteFunction( UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack )
{
	AActor* Owner = GetOwner();
	if (Owner == NULL)
	{
		return false;
	}
	
	UNetDriver* NetDriver = Owner->GetNetDriver();
	if (NetDriver)
	{
		NetDriver->ProcessRemoteFunction(Owner, Function, Parameters, OutParms, Stack, this);
		return true;
	}

	return false;
}

/** FComponentReregisterContexts for components which have had PreEditChange called but not PostEditChange. */
static TMap<UActorComponent*,FComponentReregisterContext*> EditReregisterContexts;

#if WITH_EDITOR
bool UActorComponent::Modify( bool bAlwaysMarkDirty/*=true*/ )
{
	// If this is a construction script component we don't store them in the transaction buffer.  Instead, mark
	// the Actor as modified so that we store of the transaction annotation that has the component properties stashed
	if (IsCreatedByConstructionScript() && GetOwner())
	{
		return GetOwner()->Modify(bAlwaysMarkDirty);
	}

	return Super::Modify(bAlwaysMarkDirty);
}

void UActorComponent::PreEditChange(UProperty* PropertyThatWillChange)
{
	Super::PreEditChange(PropertyThatWillChange);

	if(IsRegistered())
	{
		// The component or its outer could be pending kill when calling PreEditChange when applying a transaction.
		// Don't do do a full recreate in this situation, and instead simply detach.
		if( !IsPendingKill() )
		{
			check(!EditReregisterContexts.Find(this));
			EditReregisterContexts.Add(this,new FComponentReregisterContext(this));
		}
		else
		{
			ExecuteUnregisterEvents();
			World = NULL;
		}
	}

	// Flush rendering commands to ensure the rendering thread processes the component detachment before it is modified.
	FlushRenderingCommands();
}

void UActorComponent::PostEditUndo()
{
	// Objects marked pending kill don't call PostEditChange() from UObject::PostEditUndo(),
	// so they can leave an EditReregisterContexts entry around if they are deleted by an undo action.
	if( IsPendingKill() )
	{
		// The reregister context won't bother attaching components that are 'pending kill'. 
		FComponentReregisterContext* ReregisterContext = EditReregisterContexts.FindRef(this);
		if(ReregisterContext)
		{
			delete ReregisterContext;
			EditReregisterContexts.Remove(this);
		}
	}
	else
	{
		//Let the component be properly registered, after it was restored.
		AActor* Owner = GetOwner();
		if (Owner)
		{
			Owner->AddOwnedComponent(this);
		}
	}
	Super::PostEditUndo();
}

void UActorComponent::ConsolidatedPostEditChange()
{
	FComponentReregisterContext* ReregisterContext = EditReregisterContexts.FindRef(this);
	if(ReregisterContext)
	{
		delete ReregisterContext;
		EditReregisterContexts.Remove(this);
	}

	// The component or its outer could be pending kill when calling PostEditChange when applying a transaction.
	// Don't do do a full recreate in this situation, and instead simply detach.
	if( IsPendingKill() )
	{
		// @todo UE4 james should this call UnregisterComponent instead to remove itself from the RegisteredComponents array on the owner?
		ExecuteUnregisterEvents();
		World = NULL;
	}
}

void UActorComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	ConsolidatedPostEditChange();

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UActorComponent::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	ConsolidatedPostEditChange();

	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}


#endif // WITH_EDITOR

void UActorComponent::OnRegister()
{
	AActor* Owner = GetOwner();
	checkf(!HasAnyFlags(RF_Unreachable), TEXT("%s"), *GetDetailedInfo());
	checkf(!GetOuter()->IsTemplate(), TEXT("'%s' (%s)"), *GetOuter()->GetFullName(), *GetDetailedInfo());
	checkf(!IsTemplate(), TEXT("'%s' (%s)"), *GetOuter()->GetFullName(), *GetDetailedInfo() );
	checkf(World, TEXT("OnRegister: %s to %s"), *GetDetailedInfo(), Owner ? *Owner->GetFullName() : TEXT("*** No Owner ***") );
	checkf(!bRegistered, TEXT("OnRegister: %s to %s"), *GetDetailedInfo(), Owner ? *Owner->GetFullName() : TEXT("*** No Owner ***") );
	checkf(!IsPendingKill(), TEXT("OnRegister: %s to %s"), *GetDetailedInfo(), Owner ? *Owner->GetFullName() : TEXT("*** No Owner ***") );

	bRegistered = true;

	UpdateComponentToWorld();

	if (bAutoActivate)
	{
		Activate(true);
	}
}

void UActorComponent::OnUnregister()
{
	check(bRegistered);
	bRegistered = false;
}

void UActorComponent::InitializeComponent()
{
	check(bRegistered);
	check(!bHasBeenInitialized);

	ReceiveInitializeComponent();

	bHasBeenInitialized = true;
}

void UActorComponent::UninitializeComponent()
{
	check(bHasBeenInitialized);

	// If we're already pending kill blueprints don't get to be notified
	if (!HasAnyFlags(RF_BeginDestroyed))
	{
		ReceiveUninitializeComponent();
	}

	bHasBeenInitialized = false;
}

FActorComponentInstanceData* UActorComponent::GetComponentInstanceData() const
{
	FActorComponentInstanceData* InstanceData = new FActorComponentInstanceData(this);

	if (!InstanceData->ContainsSavedProperties())
	{
		delete InstanceData;
		InstanceData = nullptr;
	}

	return InstanceData;
}

FName UActorComponent::GetComponentInstanceDataType() const
{
	static const FName ActorComponentInstanceDataTypeName(TEXT("ActorComponentInstanceData"));
	return ActorComponentInstanceDataTypeName;
}

void FActorComponentTickFunction::ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	if (Target && !Target->HasAnyFlags(RF_PendingKill | RF_Unreachable))
	{
		FScopeCycleCounterUObject ComponentScope(Target);
		FScopeCycleCounterUObject AdditionalScope(Target->AdditionalStatObject());
	    checkSlow(Target && (!EnableParent || Target->IsPendingKill() || ((FActorTickFunction*)EnableParent)->Target == Target->GetOwner())); // components that get renamed into other outers will have this wrong and hence will not necessarily tick after their actor, or use their actor as an enable parent
	    Target->ConditionalTickComponent(DeltaTime, TickType, *this);	
	}
}

FString FActorComponentTickFunction::DiagnosticMessage()
{
	return Target->GetFullName() + TEXT("[TickComponent]");
}

bool UActorComponent::SetupActorComponentTickFunction(struct FTickFunction* TickFunction)
{
	AActor* Owner = GetOwner();
	if(TickFunction->bCanEverTick && !IsTemplate() && (!Owner || !Owner->IsTemplate()))
	{
		ULevel* ComponentLevel = (Owner ? Owner->GetLevel() : GetWorld()->PersistentLevel);
		TickFunction->SetTickFunctionEnable(TickFunction->bStartWithTickEnabled);
		TickFunction->RegisterTickFunction(ComponentLevel);
		return true;
	}
	return false;
}

void UActorComponent::SetComponentTickEnabled(bool bEnabled)
{
	if (!IsTemplate() && PrimaryComponentTick.bCanEverTick)
	{
		PrimaryComponentTick.SetTickFunctionEnable(bEnabled);
	}
}

void UActorComponent::SetComponentTickEnabledAsync(bool bEnabled)
{
	if (!IsTemplate() && PrimaryComponentTick.bCanEverTick)
	{
		DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.SetComponentTickEnabledAsync"),
			STAT_FSimpleDelegateGraphTask_SetComponentTickEnabledAsync,
			STATGROUP_TaskGraphTasks);

		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateUObject(this, &UActorComponent::SetComponentTickEnabled, bEnabled),
			GET_STATID(STAT_FSimpleDelegateGraphTask_SetComponentTickEnabledAsync), NULL, ENamedThreads::GameThread
		);
	}
}

bool UActorComponent::IsComponentTickEnabled() const
{
	return PrimaryComponentTick.IsTickFunctionEnabled();
}

static UActorComponent* GTestRegisterComponentTickFunctions = NULL;

void UActorComponent::RegisterComponentTickFunctions(bool bRegister)
{
	if(bRegister)
	{
		if (SetupActorComponentTickFunction(&PrimaryComponentTick))
		{
			PrimaryComponentTick.Target = this;
		}
	}
	else
	{
		if(PrimaryComponentTick.IsTickFunctionRegistered())
		{
			PrimaryComponentTick.UnRegisterTickFunction();
		}
	}

	GTestRegisterComponentTickFunctions = this; // we will verify the super call chain is intact. Don't not copy paste this to a derived class!
}

void UActorComponent::RegisterAllComponentTickFunctions(bool bRegister)
{
	check(GTestRegisterComponentTickFunctions == NULL);
	// Components don't have tick functions until they are registered with the world
	if (bRegistered)
	{
		RegisterComponentTickFunctions(bRegister);
		checkf(GTestRegisterComponentTickFunctions == this, TEXT("Failed to route component RegisterTickFunctions (%s)"), *GetFullName());
		GTestRegisterComponentTickFunctions = NULL;
	}
}

void UActorComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	check(bRegistered);

	ReceiveTick(DeltaTime);
}

void UActorComponent::RegisterComponentWithWorld(UWorld* InWorld)
{
	checkf(!HasAnyFlags(RF_Unreachable), TEXT("%s"), *GetFullName());

	if(IsPendingKill())
	{
		UE_LOG(LogActorComponent, Log, TEXT("RegisterComponentWithWorld: (%s) Trying to register component with IsPendingKill() == true. Aborting."), *GetPathName());
		return;
	}

	// If the component was already registered, do nothing
	if(IsRegistered())
	{
		UE_LOG(LogActorComponent, Log, TEXT("RegisterComponentWithWorld: (%s) Already registered. Aborting."), *GetPathName());
		return;
	}

	if(InWorld == NULL)
	{
		//UE_LOG(LogActorComponent, Log, TEXT("RegisterComponentWithWorld: (%s) NULL InWorld specified. Aborting."), *GetPathName());
		return;
	}

	// If not registered, should not have a scene
	checkf(World == NULL, TEXT("%s"), *GetFullName());

	// Follow outer chain to see if we can find an Actor
	AActor* Owner = GetOwner();

	checkSlow(Owner == NULL || Owner->OwnsComponent(this));

	if ((Owner != NULL) && Owner->GetClass()->HasAnyClassFlags(CLASS_NewerVersionExists))
	{
		UE_LOG(LogActorComponent, Log, TEXT("RegisterComponentWithWorld: Owner belongs to a DEADCLASS"));
		return;
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// Can only register with an Actor if we are created within one
	if(Owner != NULL)
	{
		checkf(!Owner->HasAnyFlags(RF_Unreachable), TEXT("%s"), *GetFullName());
		// can happen with undo because the owner will be restored "next"
		//checkf(!Owner->IsPendingKill(), TEXT("%s"), *GetFullName());

		if(InWorld != Owner->GetWorld())
		{
			// The only time you should specify a scene that is not GetOwner()->GetWorld() is when you don't have an Actor
			UE_LOG(LogActorComponent, Log, TEXT("RegisterComponentWithWorld: (%s) Specifying a world, but an Owner Actor found, and InWorld is not GetOwner()->GetWorld()"), *GetPathName());
		}
	}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	World = InWorld;

	ExecuteRegisterEvents();
	RegisterAllComponentTickFunctions(true);

	if (Owner == nullptr || Owner->bActorInitialized)
	{
		if (!bHasBeenInitialized && bWantsInitializeComponent)
		{
			InitializeComponent();
		}
	}

	// If this is a blueprint created component and it has component children they can miss getting registered in some scenarios
	if (IsCreatedByConstructionScript())
	{
		TArray<UObject*> Children;
		GetObjectsWithOuter(this, Children, true, RF_PendingKill);

		for (UObject* Child : Children)
		{
			UActorComponent* ChildComponent = Cast<UActorComponent>(Child);
			if (ChildComponent && !ChildComponent->IsRegistered())
			{
				ChildComponent->RegisterComponentWithWorld(InWorld);
			}
		}

	}
}

void UActorComponent::RegisterComponent()
{
	if (ensure(GetOwner() && GetOwner()->GetWorld()))
	{
		RegisterComponentWithWorld(GetOwner()->GetWorld());
	}
}

void UActorComponent::UnregisterComponent()
{
	// Do nothing if not registered
	if(!IsRegistered())
	{
		UE_LOG(LogActorComponent, Log, TEXT("UnregisterComponent: (%s) Not registered. Aborting."), *GetPathName());
		return;
	}

	// If registered, should have a world
	checkf(World != NULL, TEXT("%s"), *GetFullName());

	// Notify the texture streaming system
	const UPrimitiveComponent* Primitive = Cast<const UPrimitiveComponent>(this);
	if ( Primitive )
	{
		IStreamingManager::Get().NotifyPrimitiveDetached( Primitive );
	}

	AActor* Owner = GetOwner(); // Get Owner while we are still registered

	RegisterAllComponentTickFunctions(false);
	ExecuteUnregisterEvents();

	World = NULL;
}

void UActorComponent::DestroyComponent(bool bPromoteChildren/*= false*/)
{
	// Ensure that we call UninitializeComponent before we destroy this component
	if (bHasBeenInitialized)
	{
		UninitializeComponent();
	}

	// Unregister if registered
	if(IsRegistered())
	{
		UnregisterComponent();
	}

	// Then remove from Components array, if we have an Actor
	AActor* Owner = GetOwner();
	if(Owner != NULL)
	{
		if (IsCreatedByConstructionScript())
		{
			Owner->BlueprintCreatedComponents.Remove(this);
		}
		else
		{
			Owner->RemoveInstanceComponent(this);
		}
		Owner->RemoveOwnedComponent(this);
		if (Owner->GetRootComponent() == this)
		{
			Owner->SetRootComponent(NULL);
		}
	}

	// Tell the component it is being destroyed
	OnComponentDestroyed();

	// Finally mark pending kill, to NULL out any other refs
	MarkPendingKill();
}

void UActorComponent::OnComponentCreated()
{
	ensure(!bHasBeenCreated);
	bHasBeenCreated = true;
}

void UActorComponent::OnComponentDestroyed()
{
	// @TODO: Would be nice to ensure(bHasBeenCreated), but there are still many places where components are created without calling OnComponentCreated
	bHasBeenCreated = false;
}

void UActorComponent::K2_DestroyComponent(UObject* Object)
{
	AActor* Owner = GetOwner();
	if (Owner == NULL || Owner == Object)
	{
		DestroyComponent();
	}
	else
	{
		// TODO: Put in Message Log
		UE_LOG(LogActorComponent, Error, TEXT("May not destroy component %s owned by %s."), *GetFullName(), *Owner->GetFullName());
	}
}

void UActorComponent::CreateRenderState_Concurrent()
{
	check(IsRegistered());
	check(World->Scene);
	check(!bRenderStateCreated);
	bRenderStateCreated = true;

	bRenderStateDirty = false;
	bRenderTransformDirty = false;
	bRenderDynamicDataDirty = false;

#if LOG_RENDER_STATE
	UE_LOG(LogActorComponent, Log, TEXT("CreateRenderState_Concurrent: %s"), *GetPathName());
#endif
}

void UActorComponent::SendRenderTransform_Concurrent()
{
	check(bRenderStateCreated);
	bRenderTransformDirty = false;

#if LOG_RENDER_STATE
	UE_LOG(LogActorComponent, Log, TEXT("SendRenderTransform_Concurrent: %s"), *GetPathName());
#endif
}

void UActorComponent::SendRenderDynamicData_Concurrent()
{
	check(bRenderStateCreated);
	bRenderDynamicDataDirty = false;

#if LOG_RENDER_STATE
	UE_LOG(LogActorComponent, Log, TEXT("SendRenderDynamicData_Concurrent: %s"), *GetPathName());
#endif
}

void UActorComponent::DestroyRenderState_Concurrent()
{
	check(bRenderStateCreated);
	bRenderStateCreated = false;

#if LOG_RENDER_STATE
	UE_LOG(LogActorComponent, Log, TEXT("DestroyRenderState_Concurrent: %s"), *GetPathName());
#endif
}

void UActorComponent::CreatePhysicsState()
{
	check(IsRegistered());
	check(ShouldCreatePhysicsState());
	check(World->GetPhysicsScene());
	check(!bPhysicsStateCreated);
	bPhysicsStateCreated = true;
}

void UActorComponent::DestroyPhysicsState()
{
	ensure(bPhysicsStateCreated);
	bPhysicsStateCreated = false;
}


void UActorComponent::ExecuteRegisterEvents()
{
	if(!bRegistered)
	{
		OnRegister();
		checkf(bRegistered, TEXT("Failed to route OnRegister (%s)"), *GetFullName());
	}

	if(FApp::CanEverRender() && !bRenderStateCreated && World->Scene)
	{
		CreateRenderState_Concurrent();
		checkf(bRenderStateCreated, TEXT("Failed to route CreateRenderState_Concurrent (%s)"), *GetFullName());
	}

	if(!bPhysicsStateCreated && World->GetPhysicsScene() && ShouldCreatePhysicsState())
	{
		CreatePhysicsState();
		checkf(bPhysicsStateCreated, TEXT("Failed to route CreatePhysicsState (%s)"), *GetFullName());
	}
}


void UActorComponent::ExecuteUnregisterEvents()
{
	if(bPhysicsStateCreated)
	{
		check(bRegistered); // should not have physics state unless we are registered
		DestroyPhysicsState();
		checkf(!bPhysicsStateCreated, TEXT("Failed to route DestroyPhysicsState (%s)"), *GetFullName());
		checkf(!HasValidPhysicsState(), TEXT("Failed to destroy physics state (%s)"), *GetFullName());
	}

	if(bRenderStateCreated)
	{
		check(bRegistered);
		DestroyRenderState_Concurrent();
		checkf(!bRenderStateCreated, TEXT("Failed to route DestroyRenderState_Concurrent (%s)"), *GetFullName());
	}

	if(bRegistered)
	{
		OnUnregister();
		checkf(!bRegistered, TEXT("Failed to route OnUnregister (%s)"), *GetFullName());
	}
}

void UActorComponent::ReregisterComponent()
{
	if(!IsRegistered())
	{
		UE_LOG(LogActorComponent, Log, TEXT("ReregisterComponent: (%s) Not currently registered. Aborting."), *GetPathName());
		return;
	}

	FComponentReregisterContext(this);
}

void UActorComponent::RecreateRenderState_Concurrent()
{
	if(bRenderStateCreated)
	{
		check(IsRegistered()); // Should never have render state unless registered
		DestroyRenderState_Concurrent();
		checkf(!bRenderStateCreated, TEXT("Failed to route DestroyRenderState_Concurrent (%s)"), *GetFullName());
	}

	if(IsRegistered() && World->Scene)
	{
		CreateRenderState_Concurrent();
		checkf(bRenderStateCreated, TEXT("Failed to route CreateRenderState_Concurrent (%s)"), *GetFullName());
	}
}

void UActorComponent::RecreatePhysicsState()
{
	if(bPhysicsStateCreated)
	{
		check(IsRegistered()); // Should never have physics state unless registered
		DestroyPhysicsState();
		checkf(!bPhysicsStateCreated, TEXT("Failed to route DestroyPhysicsState (%s)"), *GetFullName());
		checkf(!HasValidPhysicsState(), TEXT("Failed to destroy physics state (%s)"), *GetFullName());
	}

	if (IsRegistered() && World->GetPhysicsScene() && ShouldCreatePhysicsState())
	{
		CreatePhysicsState();
		checkf(bPhysicsStateCreated, TEXT("Failed to route CreatePhysicsState (%s)"), *GetFullName());
	}
}

void UActorComponent::ConditionalTickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction &ThisTickFunction)
{
	if(bRegistered && !IsPendingKill())
	{
		//@optimization, I imagine this is all unnecessary in a shipping game with no editor
		AActor* Owner = GetOwner();
		if (TickType != LEVELTICK_ViewportsOnly || 
			(bTickInEditor && TickType == LEVELTICK_ViewportsOnly) ||
			(Owner && Owner->ShouldTickIfViewportsOnly())
			)
		{
			const float TimeDilation = (Owner ? Owner->CustomTimeDilation : 1.f);
			TickComponent(DeltaTime * TimeDilation, TickType, &ThisTickFunction);
		}
	}
}

void UActorComponent::SetTickGroup(ETickingGroup NewTickGroup)
{
	PrimaryComponentTick.TickGroup = NewTickGroup;
}


void UActorComponent::AddTickPrerequisiteActor(AActor* PrerequisiteActor)
{
	if (PrimaryComponentTick.bCanEverTick && PrerequisiteActor && PrerequisiteActor->PrimaryActorTick.bCanEverTick)
	{
		PrimaryComponentTick.AddPrerequisite(PrerequisiteActor, PrerequisiteActor->PrimaryActorTick);
	}
}

void UActorComponent::AddTickPrerequisiteComponent(UActorComponent* PrerequisiteComponent)
{
	if (PrimaryComponentTick.bCanEverTick && PrerequisiteComponent && PrerequisiteComponent->PrimaryComponentTick.bCanEverTick)
	{
		PrimaryComponentTick.AddPrerequisite(PrerequisiteComponent, PrerequisiteComponent->PrimaryComponentTick);
	}
}

void UActorComponent::RemoveTickPrerequisiteActor(AActor* PrerequisiteActor)
{
	if (PrerequisiteActor)
	{
		PrimaryComponentTick.RemovePrerequisite(PrerequisiteActor, PrerequisiteActor->PrimaryActorTick);
	}
}

void UActorComponent::RemoveTickPrerequisiteComponent(UActorComponent* PrerequisiteComponent)
{
	if (PrerequisiteComponent)
	{
		PrimaryComponentTick.RemovePrerequisite(PrerequisiteComponent, PrerequisiteComponent->PrimaryComponentTick);
	}
}

void UActorComponent::DoDeferredRenderUpdates_Concurrent()
{
	checkf(!HasAnyFlags(RF_Unreachable), TEXT("%s"), *GetFullName());
	checkf(!IsTemplate(), TEXT("%s"), *GetFullName());
	checkf(!IsPendingKill(), TEXT("%s"), *GetFullName());

	if(!IsRegistered())
	{
		UE_LOG(LogActorComponent, Log, TEXT("UpdateComponent: (%s) Not registered, Aborting."), *GetPathName());
		return;
	}

	if(bRenderStateDirty)
	{
		SCOPE_CYCLE_COUNTER(STAT_PostTickComponentRecreate);
		RecreateRenderState_Concurrent();
		checkf(!bRenderStateDirty, TEXT("Failed to route CreateRenderState_Concurrent (%s)"), *GetFullName());
	}
	else
	{
		SCOPE_CYCLE_COUNTER(STAT_PostTickComponentLW);
		if(bRenderTransformDirty)
		{
			// Update the component's transform if the actor has been moved since it was last updated.
			SendRenderTransform_Concurrent();
		}

		if(bRenderDynamicDataDirty)
		{
			SendRenderDynamicData_Concurrent();
		}
	}
}


void UActorComponent::MarkRenderStateDirty()
{
	// If registered and has a render state to make as dirty
	if((!bRenderStateDirty || !GetWorld()) && IsRegistered() && bRenderStateCreated)
	{
		// Flag as dirty
		bRenderStateDirty = true;
		MarkForNeededEndOfFrameRecreate();
	}
}


void UActorComponent::MarkRenderTransformDirty()
{
	if (IsRegistered() && bRenderStateCreated)
	{
		bRenderTransformDirty = true;
		MarkForNeededEndOfFrameUpdate();
	}
}


void UActorComponent::MarkRenderDynamicDataDirty()
{
	// If registered and has a render state to make as dirty
	if(IsRegistered() && bRenderStateCreated)
	{
		// Flag as dirty
		bRenderDynamicDataDirty = true;
		MarkForNeededEndOfFrameUpdate();
	}
}

void UActorComponent::MarkForNeededEndOfFrameUpdate()
{
	if (bNeverNeedsRenderUpdate)
	{
		return;
	}

	if(GetWorld())
	{
		GetWorld()->MarkActorComponentForNeededEndOfFrameUpdate(this, RequiresGameThreadEndOfFrameUpdates());
	}
	else if (!HasAnyFlags(RF_Unreachable))
	{
		// we don't have a world, do it right now.
		DoDeferredRenderUpdates_Concurrent();
	}
}

void UActorComponent::MarkForNeededEndOfFrameRecreate()
{
	if (bNeverNeedsRenderUpdate)
	{
		return;
	}

	if (GetWorld())
	{
		// by convention, recreates are always done on the gamethread
		GetWorld()->MarkActorComponentForNeededEndOfFrameUpdate(this, true);
	}
	else if (!HasAnyFlags(RF_Unreachable))
	{
		// we don't have a world, do it right now.
		DoDeferredRenderUpdates_Concurrent();
	}
}

bool UActorComponent::RequiresGameThreadEndOfFrameUpdates() const
{
	return false;
}

void UActorComponent::Activate(bool bReset)
{
	if (bReset || ShouldActivate()==true)
	{
		SetComponentTickEnabled(true);
		bIsActive = true;
	}
}

void UActorComponent::Deactivate()
{
	if (ShouldActivate()==false)
	{
		SetComponentTickEnabled(false);
		bIsActive = false;
	}
}

bool UActorComponent::ShouldActivate() const
{
	// if not active, should activate
	return !bIsActive;
}

void UActorComponent::SetActive(bool bNewActive, bool bReset)
{
	// if it wants to activate
	if (bNewActive)
	{
		// make sure to check if it should activate
		Activate(bReset);	
	}
	// otherwise, make sure it shouldn't activate
	else 
	{
		Deactivate();
	}
}

void UActorComponent::ToggleActive()
{
	SetActive(!bIsActive);
}

bool UActorComponent::IsActive() const
{
	return bIsActive;
}

void UActorComponent::SetTickableWhenPaused(bool bTickableWhenPaused)
{
	PrimaryComponentTick.bTickEvenWhenPaused = bTickableWhenPaused;
}

bool UActorComponent::IsRunningUserConstructionScript() const
{
	const AActor* Owner = GetOwner();
	return Owner != nullptr && Owner->bRunningUserConstructionScript;
}

void UActorComponent::AddAssetUserData(UAssetUserData* InUserData)
{
	if (InUserData != NULL)
	{
		UAssetUserData* ExistingData = GetAssetUserDataOfClass(InUserData->GetClass());
		if (ExistingData != NULL)
		{
			AssetUserData.Remove(ExistingData);
		}
		AssetUserData.Add(InUserData);
	}
}

UAssetUserData* UActorComponent::GetAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != NULL && Datum->IsA(InUserDataClass))
		{
			return Datum;
		}
	}
	return NULL;
}

void UActorComponent::RemoveUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != NULL && Datum->IsA(InUserDataClass))
		{
			AssetUserData.RemoveAt(DataIdx);
			return;
		}
	}
}

void UActorComponent::SetNetAddressable()
{
	bNetAddressable = true;
}

bool UActorComponent::IsNameStableForNetworking() const
{
	/** 
	 * IsNameStableForNetworking means a component can be referred to its path name (relative to owning AActor*) over the network
	 *
	 * Components are net addressable if:
	 *	-They are Default Subobjects (created in C++ constructor)
	 *	-They were loaded directly from a package (placed in map actors)
	 *	-They were explicitly set to bNetAddressable (blueprint components created by SCS)
	 */

	return bNetAddressable || Super::IsNameStableForNetworking();
}

bool UActorComponent::IsSupportedForNetworking() const
{
	return IsNameStableForNetworking() || GetIsReplicated();
}

void UActorComponent::SetIsReplicated(bool ShouldReplicate)
{
	check(GetComponentClassCanReplicate()); // Only certain component classes can replicate!
	bReplicates = ShouldReplicate;

	AActor* Owner = GetOwner();
	if (Owner)
	{
		Owner->UpdateReplicatedComponent( this );
	}
}

bool UActorComponent::GetIsReplicated() const
{
	return bReplicates;
}

bool UActorComponent::ReplicateSubobjects(class UActorChannel *Channel, class FOutBunch *Bunch, FReplicationFlags *RepFlags)
{
	return false;
}

bool UActorComponent::GetComponentClassCanReplicate() const
{
	return true;
}

ENetRole UActorComponent::GetOwnerRole() const
{
	AActor *Actor = GetOwner();
	if (!Actor)
	{
		return ROLE_None;
	}
	return Actor->Role;
}

bool UActorComponent::IsNetSimulating() const
{
	return GetIsReplicated() && GetOwnerRole() != ROLE_Authority;
}

void UActorComponent::GetLifetimeReplicatedProps( TArray< FLifetimeProperty > & OutLifetimeProps ) const
{
	DOREPLIFETIME( UActorComponent, bIsActive );
	DOREPLIFETIME( UActorComponent, bReplicates );
}

void UActorComponent::OnRep_IsActive()
{
	SetComponentTickEnabled(bIsActive);
}


#undef LOCTEXT_NAMESPACE
