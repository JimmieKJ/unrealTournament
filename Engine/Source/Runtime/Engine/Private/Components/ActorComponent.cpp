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
#include "ComponentRecreateRenderStateContext.h"
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
}

FGlobalComponentReregisterContext::~FGlobalComponentReregisterContext()
{
	check(ActiveGlobalReregisterContextCount > 0);
	// We empty the array now, to ensure that the FComponentReregisterContext destructors are called while ActiveGlobalReregisterContextCount still indicates activity
	ComponentContexts.Empty();
	ActiveGlobalReregisterContextCount--;
}

FGlobalComponentRecreateRenderStateContext::FGlobalComponentRecreateRenderStateContext()
{
	// wait until resources are released
	FlushRenderingCommands();

	// recreate render state for all components.
	for (auto* Component : TObjectRange<UActorComponent>())
	{
		new(ComponentContexts) FComponentRecreateRenderStateContext(Component);
	}
}

FGlobalComponentRecreateRenderStateContext::~FGlobalComponentRecreateRenderStateContext()
{
	ComponentContexts.Empty();
}


UActorComponent::UActorComponent(const FObjectInitializer& ObjectInitializer /*= FObjectInitializer::Get()*/)
	: Super(ObjectInitializer)
{
	Owner = GetTypedOuter<AActor>();

	PrimaryComponentTick.TickGroup = TG_DuringPhysics;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.bCanEverTick = false;

	CreationMethod = EComponentCreationMethod::Native;

	bAutoRegister = true;
	bNetAddressable = false;
	bEditableWhenInherited = true;
}

void UActorComponent::PostInitProperties()
{
	Super::PostInitProperties();

	if (Owner)
	{
		Owner->AddOwnedComponent(this);
	}
}

void UActorComponent::PostLoad()
{
	Super::PostLoad();

	if (GetLinkerUE4Version() < VER_UE4_ACTOR_COMPONENT_CREATION_METHOD)
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
			UBlueprintGeneratedClass* Class = CastChecked<UBlueprintGeneratedClass>(GetOuter()->GetClass());
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

	if (CreationMethod == EComponentCreationMethod::SimpleConstructionScript)
	{
		if ((GetLinkerUE4Version() < VER_UE4_TRACK_UCS_MODIFIED_PROPERTIES) && !HasAnyFlags(RF_ClassDefaultObject))
		{
			DetermineUCSModifiedProperties();
		}
	}
	else
	{
		// For a brief period of time we were inadvertently storing these for all components, need to clear it out
		UCSModifiedProperties.Empty();
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
		Owner = GetTypedOuter<AActor>();
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

			TArray<UObject*> Children;
			GetObjectsWithOuter(this, Children);

			for (UObject* Child : Children)
			{
				if (UActorComponent* ChildComponent = Cast<UActorComponent>(Child))
				{
					ChildComponent->Owner = Owner;
					if (OldOwner)
					{
						OldOwner->RemoveOwnedComponent(ChildComponent);
					}
					if (Owner)
					{
						Owner->AddOwnedComponent(ChildComponent);
					}
				}
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
	if (AActor* MyOwner = GetOwner())
	{
		if (GetClass()->HasAnyClassFlags(CLASS_Deprecated))
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("ComponentName"), FText::FromString(GetName()));
			Arguments.Add(TEXT("OwnerName"), FText::FromString(MyOwner->GetName()));
			FMessageLog("MapCheck").Warning()
				->AddToken(FUObjectToken::Create(MyOwner))
				->AddToken(FTextToken::Create(FText::Format( LOCTEXT( "MapCheck_Message_DeprecatedClass", "{ComponentName}::{OwnerName} is obsolete and must be removed (Class is deprecated)" ), Arguments ) ) )
				->AddToken(FMapErrorToken::Create(FMapErrors::DeprecatedClass));
		}

		if (GetClass()->HasAnyClassFlags(CLASS_Abstract))
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("ComponentName"), FText::FromString(GetName()));
			Arguments.Add(TEXT("OwnerName"), FText::FromString(MyOwner->GetName()));
			FMessageLog("MapCheck").Warning()
				->AddToken(FUObjectToken::Create(MyOwner))
				->AddToken(FTextToken::Create(FText::Format( LOCTEXT( "MapCheck_Message_AbstractClass", "{ComponentName}::{OwnerName} is obsolete and must be removed (Class is abstract)" ), Arguments ) ))
				->AddToken(FMapErrorToken::Create(FMapErrors::AbstractClass));
		}
	}
}
#endif

bool UActorComponent::IsOwnerSelected() const
{
	AActor* MyOwner = GetOwner();
	return MyOwner && MyOwner->IsSelected();
}

UWorld* UActorComponent::GetWorld() const
{
	UWorld* ComponentWorld = World;
	if (ComponentWorld == nullptr)
	{
		AActor* MyOwner = GetOwner();
		// If we don't have a world yet, it may be because we haven't gotten registered yet, but we can try to look at our owner
		if (MyOwner && !MyOwner->HasAnyFlags(RF_ClassDefaultObject))
		{
			ComponentWorld = MyOwner->GetWorld();
		}

		if( ComponentWorld == nullptr )
		{
			// As a fallback check the outer of this component for a world. In some cases components are spawned directly in the world
			ComponentWorld = Cast<UWorld>(GetOuter());
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
	AActor* MyOwner = GetOwner();
	return MyOwner ? MyOwner->GetNetMode() : NM_Standalone;
}

FSceneInterface* UActorComponent::GetScene() const
{
	return (World ? World->Scene : NULL);
}

ULevel* UActorComponent::GetComponentLevel() const
{
	// For model components Level is outer object
	AActor* MyOwner = GetOwner();
	return (MyOwner ? CastChecked<ULevel>(MyOwner->GetOuter()) : Cast<ULevel>( GetOuter() ) );
}

bool UActorComponent::ComponentIsInLevel(const ULevel *TestLevel) const
{
	return (GetComponentLevel() == TestLevel);
}

bool UActorComponent::ComponentIsInPersistentLevel(bool bIncludeLevelStreamingPersistent) const
{
	ULevel* MyLevel = GetComponentLevel();
	UWorld* MyWorld = GetWorld();

	if (MyLevel == NULL || MyWorld == NULL)
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
	if (bHasBegunPlay)
	{
		EndPlay(EEndPlayReason::Destroyed);
	}

	// Ensure that we call UninitializeComponent before we destroy this component
	if (bHasBeenInitialized)
	{
		UninitializeComponent();
	}

	ExecuteUnregisterEvents();

	// Ensure that we call OnComponentDestroyed before we destroy this component
	if (bHasBeenCreated)
	{
		OnComponentDestroyed();
	}

	World = NULL;

	// Remove from the parent's OwnedComponents list
	if (AActor* MyOwner = GetOwner())
	{
		MyOwner->RemoveOwnedComponent(this);
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
	AActor* MyOwner = GetOwner();
	return (MyOwner ? MyOwner->GetFunctionCallspace(Function, Parameters, Stack) : FunctionCallspace::Local);
}

bool UActorComponent::CallRemoteFunction( UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack )
{
	if (AActor* MyOwner = GetOwner())
	{
		UNetDriver* NetDriver = MyOwner->GetNetDriver();
		if (NetDriver)
		{
			NetDriver->ProcessRemoteFunction(MyOwner, Function, Parameters, OutParms, Stack, this);
			return true;
		}
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
	AActor* MyOwner = GetOwner();
	if (MyOwner && IsCreatedByConstructionScript())
	{
		return MyOwner->Modify(bAlwaysMarkDirty);
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

void UActorComponent::PreEditUndo()
{
	Super::PreEditUndo();

	Owner = nullptr;
	bCanUseCachedOwner = false;
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
		Owner = GetTypedOuter<AActor>();
		bCanUseCachedOwner = true;

		// Let the component be properly registered, after it was restored.
		if (Owner)
		{
			Owner->AddOwnedComponent(this);
		}

		TArray<UObject*> Children;
		GetObjectsWithOuter(this, Children);

		for (UObject* Child : Children)
		{
			if (UActorComponent* ChildComponent = Cast<UActorComponent>(Child))
			{
				if (ChildComponent->Owner)
				{
					ChildComponent->Owner->RemoveOwnedComponent(ChildComponent);
				}
				ChildComponent->Owner = Owner;
				if (Owner)
				{
					Owner->AddOwnedComponent(ChildComponent);
				}
			}
		}

		if (GetWorld())
		{
			GetWorld()->UpdateActorComponentEndOfFrameUpdateState(this);
		}
	}
	Super::PostEditUndo();
}

void UActorComponent::ConsolidatedPostEditChange(const FPropertyChangedEvent& PropertyChangedEvent)
{
	FComponentReregisterContext* ReregisterContext = EditReregisterContexts.FindRef(this);
	if(ReregisterContext)
	{
		delete ReregisterContext;
		EditReregisterContexts.Remove(this);

		AActor* MyOwner = GetOwner();
		if ( MyOwner && !MyOwner->IsTemplate() && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive )
		{
			MyOwner->RerunConstructionScripts();
		}
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
	ConsolidatedPostEditChange(PropertyChangedEvent);

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UActorComponent::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	ConsolidatedPostEditChange(PropertyChangedEvent);

	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}


#endif // WITH_EDITOR

void UActorComponent::OnRegister()
{
	checkf(!HasAnyFlags(RF_Unreachable), TEXT("%s"), *GetDetailedInfo());
	checkf(!GetOuter()->IsTemplate(), TEXT("'%s' (%s)"), *GetOuter()->GetFullName(), *GetDetailedInfo());
	checkf(!IsTemplate(), TEXT("'%s' (%s)"), *GetOuter()->GetFullName(), *GetDetailedInfo() );
	checkf(World, TEXT("OnRegister: %s to %s"), *GetDetailedInfo(), GetOwner() ? *GetOwner()->GetFullName() : TEXT("*** No Owner ***") );
	checkf(!bRegistered, TEXT("OnRegister: %s to %s"), *GetDetailedInfo(), GetOwner() ? *GetOwner()->GetFullName() : TEXT("*** No Owner ***") );
	checkf(!IsPendingKill(), TEXT("OnRegister: %s to %s"), *GetDetailedInfo(), GetOwner() ? *GetOwner()->GetFullName() : TEXT("*** No Owner ***") );

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

	bHasBeenInitialized = true;
}

void UActorComponent::UninitializeComponent()
{
	check(bHasBeenInitialized);

	bHasBeenInitialized = false;
}

void UActorComponent::BeginPlay()
{
	check(bRegistered);
	check(!bHasBegunPlay);

	ReceiveBeginPlay();

	bHasBegunPlay = true;
}

void UActorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	check(bHasBegunPlay);

	// If we're already pending kill blueprints don't get to be notified
	if (!HasAnyFlags(RF_BeginDestroyed))
	{
		ReceiveEndPlay(EndPlayReason);
	}

	bHasBegunPlay = false;
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
	if(TickFunction->bCanEverTick && !IsTemplate())
	{
		AActor* MyOwner = GetOwner();
		if (!MyOwner || !MyOwner->IsTemplate())
		{
			ULevel* ComponentLevel = (MyOwner ? MyOwner->GetLevel() : GetWorld()->PersistentLevel);
			TickFunction->SetTickFunctionEnable(TickFunction->bStartWithTickEnabled);
			TickFunction->RegisterTickFunction(ComponentLevel);
			return true;
		}
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

	AActor* MyOwner = GetOwner();
	checkSlow(MyOwner == nullptr || MyOwner->OwnsComponent(this));

	if (MyOwner && MyOwner->GetClass()->HasAnyClassFlags(CLASS_NewerVersionExists))
	{
		UE_LOG(LogActorComponent, Log, TEXT("RegisterComponentWithWorld: Owner belongs to a DEADCLASS"));
		return;
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// Can only register with an Actor if we are created within one
	if(MyOwner)
	{
		checkf(!MyOwner->HasAnyFlags(RF_Unreachable), TEXT("%s"), *GetFullName());
		// can happen with undo because the owner will be restored "next"
		//checkf(!MyOwner->IsPendingKill(), TEXT("%s"), *GetFullName());

		if(InWorld != MyOwner->GetWorld())
		{
			// The only time you should specify a scene that is not Owner->GetWorld() is when you don't have an Actor
			UE_LOG(LogActorComponent, Log, TEXT("RegisterComponentWithWorld: (%s) Specifying a world, but an Owner Actor found, and InWorld is not GetOwner()->GetWorld()"), *GetPathName());
		}
	}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	World = InWorld;

	ExecuteRegisterEvents();
	RegisterAllComponentTickFunctions(true);

	if (MyOwner == nullptr || MyOwner->IsActorInitialized())
	{
		if (!bHasBeenInitialized && bWantsInitializeComponent)
		{
			InitializeComponent();
		}
	}

	if (MyOwner && MyOwner->HasActorBegunPlay())
	{
		if (bWantsBeginPlay)
		{
			BeginPlay();
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
	AActor* MyOwner = GetOwner();
	if (ensure(MyOwner && MyOwner->GetWorld()))
	{
		RegisterComponentWithWorld(MyOwner->GetWorld());
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

	RegisterAllComponentTickFunctions(false);
	ExecuteUnregisterEvents();

	World = NULL;
}

void UActorComponent::DestroyComponent(bool bPromoteChildren/*= false*/)
{
	if (bHasBegunPlay)
	{
		EndPlay(EEndPlayReason::Destroyed);
	}

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
	if(AActor* MyOwner = GetOwner())
	{
		if (IsCreatedByConstructionScript())
		{
			MyOwner->BlueprintCreatedComponents.Remove(this);
		}
		else
		{
			MyOwner->RemoveInstanceComponent(this);
		}
		MyOwner->RemoveOwnedComponent(this);
		if (MyOwner->GetRootComponent() == this)
		{
			MyOwner->SetRootComponent(NULL);
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
	AActor* MyOwner = GetOwner();
	if (Object == this || MyOwner == NULL || MyOwner == Object)
	{
		DestroyComponent();
	}
	else
	{
		// TODO: Put in Message Log
		UE_LOG(LogActorComponent, Error, TEXT("May not destroy component %s owned by %s."), *GetFullName(), *MyOwner->GetFullName());
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
		AActor* MyOwner = GetOwner();
		//@optimization, I imagine this is all unnecessary in a shipping game with no editor
		if (TickType != LEVELTICK_ViewportsOnly || 
			(bTickInEditor && TickType == LEVELTICK_ViewportsOnly) ||
			(MyOwner && MyOwner->ShouldTickIfViewportsOnly())
			)
		{
			const float TimeDilation = (MyOwner ? MyOwner->CustomTimeDilation : 1.f);
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

	UWorld* ComponentWorld = GetWorld();
	if (ComponentWorld)
	{
		ComponentWorld->MarkActorComponentForNeededEndOfFrameUpdate(this, RequiresGameThreadEndOfFrameUpdates());
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

	UWorld* ComponentWorld = GetWorld();
	if (ComponentWorld)
	{
		// by convention, recreates are always done on the gamethread
		ComponentWorld->MarkActorComponentForNeededEndOfFrameUpdate(this, true);
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

bool UActorComponent::IsOwnerRunningUserConstructionScript() const
{
	AActor* MyOwner = GetOwner();
	return (MyOwner && MyOwner->bRunningUserConstructionScript);
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

	if (AActor* MyOwner = GetOwner())
	{
		MyOwner->UpdateReplicatedComponent( this );
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
	AActor* MyOwner = GetOwner();
	return (MyOwner ? MyOwner->Role.GetValue() : ROLE_None);
}

bool UActorComponent::IsNetSimulating() const
{
	return GetIsReplicated() && GetOwnerRole() != ROLE_Authority;
}

void UActorComponent::GetLifetimeReplicatedProps( TArray< FLifetimeProperty > & OutLifetimeProps ) const
{
	UBlueprintGeneratedClass* BPClass = Cast<UBlueprintGeneratedClass>(GetClass());
	if (BPClass != NULL)
	{
		BPClass->GetLifetimeBlueprintReplicationList(OutLifetimeProps);
	}

	DOREPLIFETIME( UActorComponent, bIsActive );
	DOREPLIFETIME( UActorComponent, bReplicates );
}

void UActorComponent::OnRep_IsActive()
{
	SetComponentTickEnabled(bIsActive);
}

bool UActorComponent::IsEditableWhenInherited() const
{
	bool bCanEdit = bEditableWhenInherited;
	if (bCanEdit)
	{
#if WITH_EDITOR
		if (CreationMethod == EComponentCreationMethod::Native && !IsTemplate())
		{
			bCanEdit = FComponentEditorUtils::CanEditNativeComponent(this);
		}
		else
#endif
			if (CreationMethod == EComponentCreationMethod::UserConstructionScript)
		{
			bCanEdit = false;
		}
	}
	return bCanEdit;
}

void UActorComponent::DetermineUCSModifiedProperties()
{
	UCSModifiedProperties.Empty();

	if (CreationMethod == EComponentCreationMethod::SimpleConstructionScript)
	{
		class FComponentPropertySkipper : public FArchive
		{
		public:
			FComponentPropertySkipper()
				: FArchive()
			{
				ArIsSaving = true;
			}

			virtual bool ShouldSkipProperty(const UProperty* InProperty) const override
			{
				return (    InProperty->HasAnyPropertyFlags(CPF_Transient | CPF_ContainsInstancedReference | CPF_InstancedReference)
						|| !InProperty->HasAnyPropertyFlags(CPF_Edit | CPF_Interp));
			}
		} PropertySkipper;

		UClass* ComponentClass = GetClass();
		UObject* ComponentArchetype = GetArchetype();

		for (TFieldIterator<UProperty> It(ComponentClass); It; ++It)
		{
			UProperty* Property = *It;
			if( Property->ShouldSerializeValue(PropertySkipper) )
			{
				for( int32 Idx=0; Idx<Property->ArrayDim; Idx++ )
				{
					uint8* DataPtr      = Property->ContainerPtrToValuePtr           <uint8>((uint8*)this, Idx);
					uint8* DefaultValue = Property->ContainerPtrToValuePtrForDefaults<uint8>(ComponentClass, (uint8*)ComponentArchetype, Idx);
					if (!Property->Identical( DataPtr, DefaultValue))
					{
						UCSModifiedProperties.Add(FSimpleMemberReference());
						FMemberReference::FillSimpleMemberReference<UProperty>(Property, UCSModifiedProperties.Last());
						break;
					}
				}
			}
		}
	}
}

void UActorComponent::GetUCSModifiedProperties(TSet<const UProperty*>& ModifiedProperties) const
{
	for (const FSimpleMemberReference& MemberReference : UCSModifiedProperties)
	{
		ModifiedProperties.Add(FMemberReference::ResolveSimpleMemberReference<UProperty>(MemberReference));
	}
}


#undef LOCTEXT_NAMESPACE