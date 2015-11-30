// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "AIModulePrivate.h"
#include "Misc/CoreMisc.h"
#include "Net/UnrealNetwork.h"
#include "DebugRenderSceneProxy.h"
#include "GameplayTasksComponent.h"
#include "AIModuleBasicGameplayDebuggerObject.h"
#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#endif

DEFINE_LOG_CATEGORY(LogAIModuleGameplayDebugger);

UAIModuleBasicGameplayDebuggerObject::UAIModuleBasicGameplayDebuggerObject(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UAIModuleBasicGameplayDebuggerObject::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
#if ENABLED_GAMEPLAY_DEBUGGER
	DOREPLIFETIME(UAIModuleBasicGameplayDebuggerObject, ControllerName);
	DOREPLIFETIME(UAIModuleBasicGameplayDebuggerObject, PawnName);
	DOREPLIFETIME(UAIModuleBasicGameplayDebuggerObject, DebugIcon);
	DOREPLIFETIME(UAIModuleBasicGameplayDebuggerObject, PawnClass);

	DOREPLIFETIME(UAIModuleBasicGameplayDebuggerObject, bIsUsingCharacter);
	DOREPLIFETIME(UAIModuleBasicGameplayDebuggerObject, MovementBaseInfo);
	DOREPLIFETIME(UAIModuleBasicGameplayDebuggerObject, MovementModeInfo);
	DOREPLIFETIME(UAIModuleBasicGameplayDebuggerObject, MontageInfo);

	DOREPLIFETIME(UAIModuleBasicGameplayDebuggerObject, bIsUsingPathFollowing);
	DOREPLIFETIME(UAIModuleBasicGameplayDebuggerObject, PathFollowingInfo);
	DOREPLIFETIME(UAIModuleBasicGameplayDebuggerObject, NavDataInfo);

	DOREPLIFETIME(UAIModuleBasicGameplayDebuggerObject, PathPoints);
	DOREPLIFETIME(UAIModuleBasicGameplayDebuggerObject, NextPathPointIndex);

	DOREPLIFETIME(UAIModuleBasicGameplayDebuggerObject, bIsUsingBehaviorTree);
#endif
}

void UAIModuleBasicGameplayDebuggerObject::CollectDataToReplicate(APlayerController* MyPC, AActor *SelectedActor)
{
	Super::CollectDataToReplicate(MyPC, SelectedActor);

#if ENABLED_GAMEPLAY_DEBUGGER
	APawn* MyPawn = Cast<APawn>(SelectedActor);
	PawnName = MyPawn ? MyPawn->GetHumanReadableName() : TEXT("{red}No selected pawn.");
	PawnClass = MyPawn ? MyPawn->GetClass()->GetName() : TEXT("");
	AAIController* MyController = MyPawn ? Cast<AAIController>(MyPawn->Controller) : nullptr;

	bIsUsingCharacter = MyPawn && MyPawn->IsA(ACharacter::StaticClass());

	if (MyPawn && MyPawn->IsPendingKill() == false)
	{
		CollectBasicMovementData(MyPawn);
		CollectBasicPathData(MyPawn);
		CollectBasicBehaviorData(MyPawn);
		CollectPathData(MyPawn);
		CollectBasicAnimationData(MyPawn);
	}

	if (MyController)
	{
		if (MyController->IsPendingKill() == false)
		{
			ControllerName = MyController->GetName();
			DebugIcon = MyController->GetDebugIcon();
		}
		else
		{
			ControllerName = TEXT("Controller PENDING KILL");
		}
	}
	else
	{
		ControllerName = TEXT("No Controller");
	}
#endif //ENABLED_GAMEPLAY_DEBUGGER
}

void UAIModuleBasicGameplayDebuggerObject::CollectBasicMovementData(APawn* MyPawn)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	UCharacterMovementComponent* CharMovement = Cast<UCharacterMovementComponent>(MyPawn->GetMovementComponent());
	if (CharMovement)
	{
		UPrimitiveComponent* FloorComponent = MyPawn->GetMovementBase();
		AActor* FloorActor = FloorComponent ? FloorComponent->GetOwner() : nullptr;
		MovementBaseInfo = FloorComponent ? FString::Printf(TEXT("%s.%s"), *GetNameSafe(FloorActor), *FloorComponent->GetName()) : TEXT("None");

		MovementModeInfo = CharMovement->GetMovementName();
	}
	else
	{
		MovementBaseInfo = TEXT("");
		MovementModeInfo = TEXT("");
	}
#endif //ENABLED_GAMEPLAY_DEBUGGER
}

void UAIModuleBasicGameplayDebuggerObject::CollectBasicPathData(APawn* MyPawn)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(MyPawn->GetWorld());
	AAIController* MyAIController = Cast<AAIController>(MyPawn->GetController());

	const ANavigationData* NavData = MyAIController && NavSys ? NavSys->GetNavDataForProps(MyAIController->GetNavAgentPropertiesRef()) : nullptr;
	if (NavData)
	{
		NavDataInfo = NavData->GetConfig().Name.ToString();
	}

	UPathFollowingComponent* PFC = MyAIController ? MyAIController->GetPathFollowingComponent() : nullptr;
	bIsUsingPathFollowing = (PFC != nullptr);

	if (PFC)
	{
		TArray<FString> Tokens;
		TArray<EPathFollowingDebugTokens::Type> Flags;
		if (PFC)
		{
			PFC->GetDebugStringTokens(Tokens, Flags);
		}

		PathFollowingInfo = FString();
		for (int32 Idx = 0; Idx < Tokens.Num(); Idx++)
		{
			switch (Flags[Idx])
			{
			case EPathFollowingDebugTokens::Description:
				PathFollowingInfo += Tokens[Idx];
				break;

			case EPathFollowingDebugTokens::ParamName:
				PathFollowingInfo += TEXT(", {yellow}");
				PathFollowingInfo += Tokens[Idx];
				PathFollowingInfo += TEXT(":");
				break;

			case EPathFollowingDebugTokens::PassedValue:
				PathFollowingInfo += TEXT("{yellow}");
				PathFollowingInfo += Tokens[Idx];
				break;

			case EPathFollowingDebugTokens::FailedValue:
				PathFollowingInfo += TEXT("{orange}");
				PathFollowingInfo += Tokens[Idx];
				break;

			default:
				break;
			}
		}
	}
	else
	{
		PathFollowingInfo = TEXT("");
	}
#endif //ENABLED_GAMEPLAY_DEBUGGER
}

void UAIModuleBasicGameplayDebuggerObject::CollectBasicBehaviorData(APawn* MyPawn)
{
#if ENABLED_GAMEPLAY_DEBUGGER && ENABLE_VISUAL_LOG
	AAIController* MyAIController = Cast<AAIController>(MyPawn->GetController());
	UBehaviorTreeComponent* BTC = MyAIController ? Cast<UBehaviorTreeComponent>(MyAIController->BrainComponent) : nullptr;
	bIsUsingBehaviorTree = (BTC != nullptr);

	if (BTC)
	{
		CurrentAITask = BTC->DescribeActiveTasks();
		CurrentAIState = BTC->IsRunning() ? TEXT("Running") : BTC->IsPaused() ? TEXT("Paused") : TEXT("Inactive");
		CurrentAIAssets = BTC->DescribeActiveTrees();
	}
	else
	{
		CurrentAITask = TEXT("");
		CurrentAIState = TEXT("");
		CurrentAIAssets = TEXT("");
	}

	UGameplayTasksComponent* GTComponent = MyPawn->FindComponentByClass<UGameplayTasksComponent>();
	if (GTComponent)
	{
		GameplayTasksState = FString::Printf(TEXT("Ticking Tasks: %s\nTask Queue: %s"), *GTComponent->GetTickingTasksDescription(), *GTComponent->GetTasksPriorityQueueDescription());
	}
	else
	{
		GameplayTasksState = TEXT("");
	}
#endif //ENABLED_GAMEPLAY_DEBUGGER
}

void UAIModuleBasicGameplayDebuggerObject::CollectBasicAnimationData(APawn* MyPawn)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	ACharacter* MyChar = Cast<ACharacter>(MyPawn);
	MontageInfo = MyChar ? GetNameSafe(MyChar->GetCurrentMontage()) : TEXT("");
#endif //ENABLED_GAMEPLAY_DEBUGGER
}

void UAIModuleBasicGameplayDebuggerObject::DrawCollectedData(APlayerController* MyPC, AActor *SelectedActor)
{
	Super::DrawCollectedData(MyPC, SelectedActor);

#if ENABLED_GAMEPLAY_DEBUGGER
	UWorld *ActiveWorld = GetWorld();
	if (ActiveWorld)
	{
		const FVector SelectedActorLoc = SelectedActor ? SelectedActor->GetActorLocation() + FVector(0, 0, SelectedActor->GetSimpleCollisionHalfHeight()) : DebugTools::InvalidLocation;
		for (FConstPawnIterator Iterator = ActiveWorld->GetPawnIterator(); Iterator; ++Iterator)
		{
			APawn* CurrentPawn = Cast<APawn>(*Iterator);
			AAIController* CurrentController = CurrentPawn != nullptr ? Cast<AAIController>(CurrentPawn->GetController()) : nullptr;
			if (CurrentController)
			{
				if (CurrentController->IsPendingKill() == false)
				{
					DebugIcon = CurrentController->GetDebugIcon();
					if (DebugIcon.Len())
					{
						UTexture2D* RegularIcon = (UTexture2D*)StaticLoadObject(UTexture2D::StaticClass(), NULL, *DebugIcon, NULL, LOAD_NoWarn | LOAD_Quiet, NULL);
						if (RegularIcon)
						{
							FCanvasIcon Icon = UCanvas::MakeIcon(RegularIcon);
							if (Icon.Texture)
							{
								const FVector ScreenLoc = UGameplayDebuggerHelper::ProjectLocation(DefaultContext, CurrentPawn->GetActorLocation() + FVector(0, 0, CurrentPawn->GetSimpleCollisionHalfHeight()));
								const FVector SelectedActorScreenLoc = SelectedActor != nullptr ? UGameplayDebuggerHelper::ProjectLocation(DefaultContext, SelectedActorLoc) : FVector::ZeroVector;
								const float DesiredIconSize = SelectedActorScreenLoc == ScreenLoc ? 32.0f : 16.f;
								DrawIcon(OverHeadContext, FColor::White, Icon, ScreenLoc.X, ScreenLoc.Y - DesiredIconSize, DesiredIconSize / Icon.Texture->GetSurfaceWidth());
							}
						}
					}
				}
			}
		}

		if (SelectedActorLoc != DebugTools::InvalidLocation)
		{
			DrawOverHeadInformation(UGameplayDebuggerHelper::GetDefaultContext(), UGameplayDebuggerHelper::GetOverHeadContext(), SelectedActorLoc);
			DrawBasicData(UGameplayDebuggerHelper::GetDefaultContext(), UGameplayDebuggerHelper::GetOverHeadContext());
		}
	}
#endif
}

void UAIModuleBasicGameplayDebuggerObject::DrawBasicData(UGameplayDebuggerBaseObject::FPrintContext& DefaultContext, UGameplayDebuggerBaseObject::FPrintContext& OverHeadContext)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	UFont* OldFont = DefaultContext.Font.Get();
	DefaultContext.Font = GEngine->GetMediumFont();
	PrintString(DefaultContext, FString::Printf(TEXT("Controller Name: {yellow}%s\n"), *ControllerName));
	DefaultContext.Font = OldFont;

	PrintString(DefaultContext, FString::Printf(TEXT("Pawn Name: {yellow}%s{white}, Pawn Class: {yellow}%s\n"), *PawnName, *PawnClass));

	// movement
	if (bIsUsingCharacter)
	{
		PrintString(DefaultContext, FString::Printf(TEXT("Movement Mode: {yellow}%s{white}, Base: {yellow}%s\n"), *MovementModeInfo, *MovementBaseInfo));
		PrintString(DefaultContext, FString::Printf(TEXT("NavData: {yellow}%s{white}, Path following: {yellow}%s\n"), *NavDataInfo, *PathFollowingInfo));
	}

	// logic
	if (bIsUsingBehaviorTree)
	{
		PrintString(DefaultContext, FString::Printf(TEXT("Behavior: {yellow}%s{white}, Tree: {yellow}%s\n"), *CurrentAIState, *CurrentAIAssets));
		PrintString(DefaultContext, FString::Printf(TEXT("Active task: {yellow}%s\n"), *CurrentAITask));
	}

	// animation
	if (bIsUsingCharacter)
	{
		PrintString(DefaultContext, FString::Printf(TEXT("Montage: {yellow}%s\n"), *MontageInfo));
	}

	// putting gameplay tasks' stuff last since it can expand heavily
	PrintString(DefaultContext, FString::Printf(TEXT("GameplayTasks:\n{yellow}%s\n"), *GameplayTasksState));
#endif //ENABLED_GAMEPLAY_DEBUGGER
}

void UAIModuleBasicGameplayDebuggerObject::DrawOverHeadInformation(UGameplayDebuggerBaseObject::FPrintContext& DefaultContext, UGameplayDebuggerBaseObject::FPrintContext& OverHeadContext, FVector SelectedActorLoc)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	if (OverHeadContext.Canvas->SceneView == NULL || OverHeadContext.Canvas->SceneView->ViewFrustum.IntersectBox(SelectedActorLoc, FVector::ZeroVector) == false)
	{
		return;
	}

	const FVector ScreenLoc = OverHeadContext.Canvas->Project(SelectedActorLoc);
	static const FVector2D FontScale(1.f, 1.f);
	UFont* Font = GEngine->GetSmallFont();

	float TextXL = 0.f;
	float YL = 0.f;
	FString ObjectName = FString::Printf(TEXT("{yellow}%s {white}(%s)"), *ControllerName, *PawnName);
	CalulateStringSize(OverHeadContext, OverHeadContext.Font.Get(), ObjectName, TextXL, YL);

	bool bDrawFullOverHead = SelectedActorLoc != DebugTools::InvalidLocation;
	if (bDrawFullOverHead)
	{
		OverHeadContext.DefaultX -= (0.5f*TextXL*FontScale.X);
		OverHeadContext.DefaultY -= (1.2f*YL*FontScale.Y);
		OverHeadContext.CursorX = OverHeadContext.DefaultX;
		OverHeadContext.CursorY = OverHeadContext.DefaultY;
	}

	if (bDrawFullOverHead)
	{
		OverHeadContext.FontRenderInfo.bEnableShadow = bDrawFullOverHead;
		PrintString(OverHeadContext, bDrawFullOverHead ? FColor::White : FColor(255, 255, 255, 128), FString::Printf(TEXT("%s\n"), *ObjectName));
		OverHeadContext.FontRenderInfo.bEnableShadow = false;
	}

	FEngineShowFlags EngineShowFlags = OverHeadContext.Canvas->SceneView && OverHeadContext.Canvas->SceneView->Family ? OverHeadContext.Canvas->SceneView->Family->EngineShowFlags : FEngineShowFlags(GIsEditor ? EShowFlagInitMode::ESFIM_Editor : EShowFlagInitMode::ESFIM_Game);
	if (EngineShowFlags.DebugAI || EngineShowFlags.Game)
	{
		DrawPath();
	}
#endif //ENABLED_GAMEPLAY_DEBUGGER
}

void UAIModuleBasicGameplayDebuggerObject::DrawPath()
{
#if ENABLED_GAMEPLAY_DEBUGGER
	static const FColor Grey(100, 100, 100);
	static const FColor PathColor(192, 192, 192);

	const int32 NumPathVerts = PathPoints.Num();

	for (int32 VertIdx = 0; VertIdx < NumPathVerts - 1; ++VertIdx)
	{
		FVector const VertLoc =PathPoints[VertIdx] + NavigationDebugDrawing::PathOffset;
		DrawDebugSolidBox(GetWorld(), VertLoc, NavigationDebugDrawing::PathNodeBoxExtent, VertIdx < int32(NextPathPointIndex) ? Grey : PathColor, false);

		// draw line to next loc
		FVector const NextVertLoc = PathPoints[VertIdx + 1] + NavigationDebugDrawing::PathOffset;
		DrawDebugLine(GetWorld(), VertLoc, NextVertLoc, VertIdx < int32(NextPathPointIndex) ? Grey : PathColor, false
			, -1.f, 0
			, NavigationDebugDrawing::PathLineThickness);
	}

	// draw last vert
	if (NumPathVerts > 0)
	{
		DrawDebugBox(GetWorld(), PathPoints[NumPathVerts - 1] + NavigationDebugDrawing::PathOffset, FVector(15.f), Grey, false);
	}

#endif //ENABLED_GAMEPLAY_DEBUGGER
}

void UAIModuleBasicGameplayDebuggerObject::CollectPathData(APawn* MyPawn)
{
#if ENABLED_GAMEPLAY_DEBUGGER && ENABLE_VISUAL_LOG
	bool bRefreshRendering = false;
	UWorld* World = MyPawn->GetWorld();

	if (AAIController *MyController = Cast<AAIController>(MyPawn->Controller))
	{
		if (MyController->PathFollowingComponent && MyController->PathFollowingComponent->GetPath().IsValid())
		{
			NextPathPointIndex = MyController->PathFollowingComponent->GetNextPathIndex();

			const FNavPathSharedPtr& NewPath = MyController->PathFollowingComponent->GetPath();
			if (CurrentPath.HasSameObject(NewPath.Get()) == false || NewPath->GetTimeStamp() > LastStoredPathTimeStamp)
			{
				LastStoredPathTimeStamp = NewPath->GetTimeStamp();

				FVisualLogEntry Snapshot;
				NewPath->DescribeSelfToVisLog(&Snapshot);
				PathCorridorPolygons.Reset(Snapshot.ElementsToDraw.Num());
				FPathCorridorPolygons Polygon;
				for (FVisualLogShapeElement& CurrentShape : Snapshot.ElementsToDraw)
				{
					if (CurrentShape.GetType() == EVisualLoggerShapeElement::Polygon)
					{
						Polygon.Color = CurrentShape.GetFColor();
						Polygon.Points.Reset();
						Polygon.Points.Append(CurrentShape.Points);
						PathCorridorPolygons.Add(Polygon);
					}
				}

				PathPoints.Reset();
				for (int32 Index = 0; Index < NewPath->GetPathPoints().Num(); ++Index)
				{
					PathPoints.Add(NewPath->GetPathPoints()[Index].Location);
				}
				CurrentPath = NewPath;

				if (PathCorridorPolygons.Num() && World && World->GetNetMode() != NM_Client)
				{
					PathCorridorData.Reset();
					TArray<uint8> HelpBuffer;
					FMemoryWriter ArWriter(HelpBuffer);
					int32 NumPolygons = PathCorridorPolygons.Num();
					ArWriter << NumPolygons;
					for (const FPathCorridorPolygons& CurrentPoly : PathCorridorPolygons)
					{
						FColor Color = CurrentPoly.Color;
						ArWriter << Color;
						int32 NumVerts = CurrentPoly.Points.Num();
						ArWriter << NumVerts;
						for (auto Vert : CurrentPoly.Points)
						{
							ArWriter << Vert;
						}
					}

					RequestDataReplication(TEXT("PathCorridor"), HelpBuffer, EGameplayDebuggerReplicate::WithCompressed);
				}
				bRefreshRendering = true;
			}
		}
		else
		{
			NextPathPointIndex = INDEX_NONE;
			CurrentPath = NULL;
			PathPoints.Reset();
			PathCorridorPolygons.Reset();
		}
	}

	if (bRefreshRendering && World && World->GetNetMode() != NM_DedicatedServer)
	{
		MarkRenderStateDirty();
	}
#endif //ENABLED_GAMEPLAY_DEBUGGER
}

void UAIModuleBasicGameplayDebuggerObject::OnDataReplicationComplited(const TArray<uint8>& ReplicatedData)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	FMemoryReader ArReader(ReplicatedData);
	int32 NumPolygons = 0;
	ArReader << NumPolygons;
	PathCorridorPolygons.Reset(NumPolygons);
	for (int32 PolyIndex = 0; PolyIndex < NumPolygons; ++PolyIndex)
	{
		FPathCorridorPolygons Polygon;
		FColor Color;
		ArReader << Polygon.Color;

		int32 NumVerts = 0;
		ArReader << NumVerts;
		for (int32 VertIndex = 0; VertIndex < NumVerts; ++VertIndex)
		{
			FVector CurrentVertex;
			ArReader << CurrentVertex;
			Polygon.Points.Add(CurrentVertex);
		}
		if (NumVerts > 2)
		{
			PathCorridorPolygons.Add(Polygon);
		}
	}
	MarkRenderStateDirty();
#endif
}

FDebugRenderSceneProxy* UAIModuleBasicGameplayDebuggerObject::CreateSceneProxy(const UPrimitiveComponent* InComponent, UWorld* World, AActor* SelectedActor)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	class FPathDebugRenderSceneProxy : public FDebugRenderSceneProxy
	{
	public:
		FPathDebugRenderSceneProxy(const UPrimitiveComponent* InPrimitiveCComponent, const FString &InViewFlagName)
			: FDebugRenderSceneProxy(InPrimitiveCComponent)
		{
			DrawType = FDebugRenderSceneProxy::SolidAndWireMeshes;
			DrawAlpha = 90;
			ViewFlagName = InViewFlagName;
			ViewFlagIndex = uint32(FEngineShowFlags::FindIndexByName(*ViewFlagName));
		}


		FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
		{
			FPrimitiveViewRelevance Result;
			Result.bDrawRelevance = View->Family->EngineShowFlags.GetSingleFlag(ViewFlagIndex);// IsShown(View);
			Result.bDynamicRelevance = true;
			// ideally the TranslucencyRelevance should be filled out by the material, here we do it conservative
			Result.bSeparateTranslucencyRelevance = Result.bNormalTranslucencyRelevance = IsShown(View);
			return Result;
		}

	};

	const bool bDrawFullData = SelectedActor != NULL;
	if (bDrawFullData /*&& ShouldReplicateData(EAIDebugDrawDataView::Basic)*/)
	{
		TArray<FDebugRenderSceneProxy::FMesh> Meshes;
		TArray<FDebugRenderSceneProxy::FDebugLine> Lines;
		for (FPathCorridorPolygons& CurrentPoly : PathCorridorPolygons)
		{

			if (CurrentPoly.Points.Num() > 2)
			{
				int32 LastIndex = 0;
				FVector FirstVertex = CurrentPoly.Points[0];
				FDebugRenderSceneProxy::FMesh TestMesh;
				TestMesh.Color = CurrentPoly.Color;// FColor::Green;
				for (int32 Index = 1; Index < CurrentPoly.Points.Num() - 1; Index += 1)
				{
					TestMesh.Vertices.Add(FDynamicMeshVertex(FirstVertex));
					TestMesh.Vertices.Add(FDynamicMeshVertex(CurrentPoly.Points[Index + 0]));
					TestMesh.Vertices.Add(FDynamicMeshVertex(CurrentPoly.Points[Index + 1]));
					TestMesh.Indices.Add(LastIndex++);
					TestMesh.Indices.Add(LastIndex++);
					TestMesh.Indices.Add(LastIndex++);
				}
				Meshes.Add(TestMesh);
			}

			for (int32 VIdx = 0; VIdx < CurrentPoly.Points.Num(); VIdx++)
			{
				Lines.Add(FDebugRenderSceneProxy::FDebugLine(
					CurrentPoly.Points[VIdx],
					CurrentPoly.Points[(VIdx + 1) % CurrentPoly.Points.Num()],
					CurrentPoly.Color,
					2)
					);
			}
		}

		FString ViewFlagName = TEXT("Game");
#if WITH_EDITOR
		UEditorEngine* EEngine = Cast<UEditorEngine>(GEngine);
		if (EEngine && EEngine->bIsSimulatingInEditor)
		{
			ViewFlagName = TEXT("DebugAI");
		}
#endif
		FPathDebugRenderSceneProxy *DebugSceneProxy = new FPathDebugRenderSceneProxy(InComponent, ViewFlagName);
		DebugSceneProxy->Lines = Lines;
		DebugSceneProxy->Meshes = Meshes;
		return DebugSceneProxy;
	}
#endif
	return nullptr;
}