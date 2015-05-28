// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HUD.cpp: Heads up Display related functionality
=============================================================================*/
#include "GameplayDebuggerPrivate.h"
#include "GameplayDebuggerSettings.h"
#include "Net/UnrealNetwork.h"
#include "GameplayDebuggingComponent.h"
#include "GameplayDebuggingHUDComponent.h"
#include "GameplayDebuggingControllerComponent.h"
#include "CanvasItem.h"
#include "AI/Navigation/NavigationSystem.h"

#include "AITypes.h"
#include "AISystem.h"
#include "GenericTeamAgentInterface.h"
#include "Perception/AIPerceptionComponent.h"
#include "AIController.h"


#include "EnvironmentQuery/EnvQueryTypes.h"
#include "Engine/Texture2D.h"
#include "Regex.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"					// Game play timers
#include "RenderUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogHUD, Log, All);


AGameplayDebuggingHUDComponent::AGameplayDebuggingHUDComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, EngineShowFlags(EShowFlagInitMode::ESFIM_Game)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	World = NULL;
	SetTickableWhenPaused(true);

#	if WITH_EDITORONLY_DATA
	SetIsTemporarilyHiddenInEditor(true);
	SetActorHiddenInGame(false);
	bHiddenEdLevel = true;
	bHiddenEdLayer = true;
	bHiddenEd = true;
	bEditable = false;
#	endif
#endif
}

void AGameplayDebuggingHUDComponent::Render()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	EngineShowFlags = Canvas && Canvas->SceneView && Canvas->SceneView->Family ? Canvas->SceneView->Family->EngineShowFlags : FEngineShowFlags(GIsEditor ? EShowFlagInitMode::ESFIM_Editor : EShowFlagInitMode::ESFIM_Game);
	PrintAllData();
#endif
}

AGameplayDebuggingReplicator* AGameplayDebuggingHUDComponent::GetDebuggingReplicator()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (CachedDebuggingReplicator.IsValid() && CachedDebuggingReplicator->GetLocalPlayerOwner() == PlayerOwner)
	{
		return CachedDebuggingReplicator.Get();
	}
	
	for (TActorIterator<AGameplayDebuggingReplicator> It(GetWorld()); It; ++It)
	{
		AGameplayDebuggingReplicator* Replicator = *It;
		if (!Replicator->IsPendingKill() && Replicator->GetLocalPlayerOwner() == PlayerOwner)
		{
			CachedDebuggingReplicator = Replicator;
			return Replicator;
		}
	}
#endif
	return NULL;
}

void AGameplayDebuggingHUDComponent::GetKeyboardDesc(TArray<FDebugCategoryView>& Categories) 
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	Categories.Add(FDebugCategoryView(EAIDebugDrawDataView::NavMesh, TEXT("NavMesh")));
	Categories.Add(FDebugCategoryView(EAIDebugDrawDataView::Basic, TEXT("Basic")));
	Categories.Add(FDebugCategoryView(EAIDebugDrawDataView::BehaviorTree, TEXT("BehaviorTree")));
	Categories.Add(FDebugCategoryView(EAIDebugDrawDataView::EQS, TEXT("EQS")));
	Categories.Add(FDebugCategoryView(EAIDebugDrawDataView::Perception, TEXT("Perception")));
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void AGameplayDebuggingHUDComponent::PrintAllData()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	// Allow child hud components to position the displayed info
	DefaultContext = FPrintContext(GEngine->GetSmallFont(), Canvas, DebugInfoStartX, DebugInfoStartY);;
	DefaultContext.FontRenderInfo.bEnableShadow = true;

	if (DefaultContext.Canvas != NULL)
	{
		float XL, YL;
		const FString ToolName = FString::Printf(TEXT("Gameplay Debug Tool [Timestamp: %05.03f]"), GetWorld()->TimeSeconds);
		CalulateStringSize(DefaultContext, DefaultContext.Font, ToolName, XL, YL);
		PrintString(DefaultContext, FColorList::White, ToolName, DefaultContext.Canvas->ClipX / 2.0f - XL / 2.0f, 0);
	}

	const float MenuX = DefaultContext.CursorX;
	const float MenuY = DefaultContext.CursorY;

	UGameplayDebuggingComponent* DebugComponent = NULL;
	if (GetDebuggingReplicator())
	{
		DebugComponent = GetDebuggingReplicator()->GetDebugComponent();
	}
				
	if (DebugComponent && DebugComponent->GetSelectedActor())
	{
		APlayerController* const MyPC = Cast<APlayerController>(PlayerOwner);
		DrawDebugComponentData(MyPC, DebugComponent);
	}

	if (DefaultContext.Canvas && DefaultContext.Canvas->SceneView && DefaultContext.Canvas->SceneView->Family && DefaultContext.Canvas->SceneView->Family->EngineShowFlags.Game)
	{
		DrawMenu(MenuX, MenuY, DebugComponent);
	}
#endif
}

void AGameplayDebuggingHUDComponent::DrawMenu(const float X, const float Y, class UGameplayDebuggingComponent* DebugComponent)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	const float OldX = DefaultContext.CursorX;
	const float OldY = DefaultContext.CursorY;

	if (DefaultContext.Canvas != NULL)
	{
		TArray<FDebugCategoryView> Categories;
		GetKeyboardDesc(Categories);

		UFont* OldFont = DefaultContext.Font;
		DefaultContext.Font = GEngine->GetMediumFont();

		TArray<float> CategoriesWidth;
		CategoriesWidth.AddZeroed(Categories.Num());
		float TotalWidth = 0.0f, MaxHeight = 0.0f;

		FString ActivationKeyDisplayName = TEXT("'");
		FString ActivationKeyName = TEXT("Apostrophe");

		APlayerController* const MyPC = Cast<APlayerController>(PlayerOwner);
		UGameplayDebuggingControllerComponent*  GDC = GetDebuggingReplicator()->FindComponentByClass<UGameplayDebuggingControllerComponent>();
		if (GDC)
		{
			ActivationKeyDisplayName = GDC->GetActivationKey().Key.GetDisplayName().ToString();
			ActivationKeyName = GDC->GetActivationKey().Key.GetFName().ToString();
		}

		const FString KeyDesc = ActivationKeyName != ActivationKeyDisplayName ? FString::Printf(TEXT("(%s key)"), *ActivationKeyName) : TEXT("");
		FString HeaderDesc = FString::Printf(TEXT("Tap %s %s to close, use Numpad numbers to toggle categories"), *ActivationKeyDisplayName, *KeyDesc);
		if (UGameplayDebuggerSettings::StaticClass()->GetDefaultObject<UGameplayDebuggerSettings>()->UseAlternateKeys())
		{
			HeaderDesc = FString::Printf(TEXT("Tap %s %s to close, use Alt + number to toggle categories"), *ActivationKeyDisplayName, *KeyDesc);
		}

		float HeaderWidth = 0.0f;
		CalulateStringSize(DefaultContext, DefaultContext.Font, HeaderDesc, HeaderWidth, MaxHeight);

		for (int32 i = 0; i < Categories.Num(); i++)
		{
			Categories[i].Desc = FString::Printf(TEXT("%d:%s "), i, *Categories[i].Desc);

			float StrHeight = 0.0f;
			CalulateStringSize(DefaultContext, DefaultContext.Font, Categories[i].Desc, CategoriesWidth[i], StrHeight);
			TotalWidth += CategoriesWidth[i];
			MaxHeight = FMath::Max(MaxHeight, StrHeight);
		}

		{
			const int32 DebugCameraIndex = Categories.Add(FDebugCategoryView());
			CategoriesWidth.AddZeroed(1);
			Categories[DebugCameraIndex].Desc = FString::Printf(TEXT(" %s[Tab]: %s  "), GDC && GDC->GetDebugCameraController().IsValid() ? TEXT("{Green}") : TEXT("{White}"), TEXT("Debug Camera"));
			float StrHeight = 0.0f;
			CalulateStringSize(DefaultContext, DefaultContext.Font, Categories[DebugCameraIndex].Desc, CategoriesWidth[DebugCameraIndex], StrHeight);
			TotalWidth += CategoriesWidth[DebugCameraIndex];
			MaxHeight = FMath::Max(MaxHeight, StrHeight);
		}

		TotalWidth = FMath::Max(TotalWidth, HeaderWidth);

		FCanvasTileItem TileItem(FVector2D(10, 10), GWhiteTexture, FVector2D(TotalWidth + 20, MaxHeight + 20), FColor(0, 0, 0, 20));
		TileItem.BlendMode = SE_BLEND_Translucent;
		DrawItem(DefaultContext, TileItem, MenuStartX, MenuStartY);

		PrintString(DefaultContext, FColorList::LightBlue, HeaderDesc, MenuStartX + 2.f, MenuStartY + 2.f);

		float XPos = MenuStartX + 20.f;
		for (int32 i = 0; i < Categories.Num(); i++)
		{
			const bool bIsActive = GameplayDebuggerSettings(GetDebuggingReplicator()).CheckFlag(Categories[i].View) ? true : false;
			const bool bIsDisabled = Categories[i].View == EAIDebugDrawDataView::NavMesh ? false : (DebugComponent && DebugComponent->GetSelectedActor() ? false: true);

			PrintString(DefaultContext, bIsDisabled ? (bIsActive ? FColorList::DarkGreen  : FColorList::LightGrey) : (bIsActive ? FColorList::Green : FColorList::White), Categories[i].Desc, XPos, MenuStartY + MaxHeight + 2.f);
			XPos += CategoriesWidth[i];
		}
		DefaultContext.Font = OldFont;
	}

	if ((!DebugComponent || !DebugComponent->GetSelectedActor()) && GetWorld()->GetNetMode() == NM_Client)
	{
		PrintString(DefaultContext, "\n{red}No Pawn selected - waiting for data to replicate from server. {green}Press and hold ' to select Pawn \n");
	}

	DefaultContext.CursorX = OldX;
	DefaultContext.CursorY = OldY;
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void AGameplayDebuggingHUDComponent::DrawDebugComponentData(APlayerController* MyPC, class UGameplayDebuggingComponent *DebugComponent)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	AActor* SelectedActor = DebugComponent->GetSelectedActor();
	const bool bDrawFullData = GetDebuggingReplicator()->GetSelectedActorToDebug() == SelectedActor;
	const FVector ScreenLoc = SelectedActor ? ProjectLocation(DefaultContext, SelectedActor->GetActorLocation() + FVector(0.f, 0.f, SelectedActor->GetSimpleCollisionHalfHeight())) : FVector::ZeroVector;

	OverHeadContext = FPrintContext(GEngine->GetSmallFont(), Canvas, ScreenLoc.X, ScreenLoc.Y);
	DefaultContext.CursorY += 20;

	FGameplayDebuggerSettings DebuggerSettings = GameplayDebuggerSettings(GetDebuggingReplicator());
	bool bForceOverhead = false;
#if !WITH_EDITOR
	bForceOverhead = bDrawFullData;
#endif

	if (DebuggerSettings.CheckFlag(EAIDebugDrawDataView::OverHead) || bForceOverhead)
	{
		DrawOverHeadInformation(MyPC, DebugComponent);
	}

	if (DebuggerSettings.CheckFlag(EAIDebugDrawDataView::NavMesh))
	{
		DrawNavMeshSnapshot(MyPC, DebugComponent);
	}

	if (DebugComponent->GetSelectedActor() && bDrawFullData)
	{
		if (DebuggerSettings.CheckFlag(EAIDebugDrawDataView::Basic) /*|| EngineShowFlags.DebugAI*/)
		{
			DrawBasicData(MyPC, DebugComponent);
		}

		if (DebuggerSettings.CheckFlag(EAIDebugDrawDataView::BehaviorTree))
		{
			DrawBehaviorTreeData(MyPC, DebugComponent);
		}

		if (DebuggerSettings.CheckFlag(EAIDebugDrawDataView::EQS))
		{
			bool bEnabledEnvironmentQueryEd = true;
			if (GConfig)
			{
				GConfig->GetBool(TEXT("EnvironmentQueryEd"), TEXT("EnableEnvironmentQueryEd"), bEnabledEnvironmentQueryEd, GEngineIni);
			}
			if (bEnabledEnvironmentQueryEd)
			{
				DrawEQSData(MyPC, DebugComponent);
			}
		}

		if (DebuggerSettings.CheckFlag(EAIDebugDrawDataView::Perception) /*|| EngineShowFlags.DebugAI*/)
		{
			DrawPerception(MyPC, DebugComponent);
		}

		DrawGameSpecificView(MyPC, DebugComponent);
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void AGameplayDebuggingHUDComponent::DrawPath(APlayerController* MyPC, class UGameplayDebuggingComponent *DebugComponent)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	static const FColor Grey(100,100,100);
	static const FColor PathColor(192,192,192);

	const int32 NumPathVerts = DebugComponent->PathPoints.Num();
	UWorld* DrawWorld = GetWorld();

	for (int32 VertIdx=0; VertIdx < NumPathVerts-1; ++VertIdx)
	{
		FVector const VertLoc = DebugComponent->PathPoints[VertIdx] + NavigationDebugDrawing::PathOffset;
		DrawDebugSolidBox(DrawWorld, VertLoc, NavigationDebugDrawing::PathNodeBoxExtent, VertIdx < int32(DebugComponent->NextPathPointIndex) ? Grey : PathColor, false);

		// draw line to next loc
		FVector const NextVertLoc = DebugComponent->PathPoints[VertIdx+1] + NavigationDebugDrawing::PathOffset;
		DrawDebugLine(DrawWorld, VertLoc, NextVertLoc, VertIdx < int32(DebugComponent->NextPathPointIndex) ? Grey : PathColor, false
			, -1.f, 0
			, NavigationDebugDrawing::PathLineThickness);
	}

	// draw last vert
	if (NumPathVerts > 0)
	{
		DrawDebugBox(DrawWorld, DebugComponent->PathPoints[NumPathVerts-1] + NavigationDebugDrawing::PathOffset, FVector(15.f), Grey, false);
	}

#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void AGameplayDebuggingHUDComponent::DrawOverHeadInformation(APlayerController* PC, class UGameplayDebuggingComponent *DebugComponent)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	APawn* MyPawn = Cast<APawn>(DebugComponent->GetSelectedActor());
	const FVector Loc3d = MyPawn ? MyPawn->GetActorLocation() + FVector(0.f, 0.f, MyPawn->GetSimpleCollisionHalfHeight()) : FVector::ZeroVector;

	if (OverHeadContext.Canvas->SceneView == NULL || OverHeadContext.Canvas->SceneView->ViewFrustum.IntersectBox(Loc3d, FVector::ZeroVector) == false)
	{
		return;
	}

	const FVector ScreenLoc = OverHeadContext.Canvas->Project(Loc3d);
	static const FVector2D FontScale(1.f, 1.f);
	UFont* Font = GEngine->GetSmallFont();

	float TextXL = 0.f;
	float YL = 0.f;
	FString ObjectName = FString::Printf( TEXT("{yellow}%s {white}(%s)"), *DebugComponent->ControllerName, *DebugComponent->PawnName);
	CalulateStringSize(OverHeadContext, OverHeadContext.Font, ObjectName, TextXL, YL);

	bool bDrawFullOverHead = GetDebuggingReplicator()->GetSelectedActorToDebug() == MyPawn;
	float IconXLocation = OverHeadContext.DefaultX;
	float IconYLocation = OverHeadContext.DefaultY;
	if (bDrawFullOverHead)
	{
		OverHeadContext.DefaultX -= (0.5f*TextXL*FontScale.X);
		OverHeadContext.DefaultY -= (1.2f*YL*FontScale.Y);
		IconYLocation = OverHeadContext.DefaultY;
		OverHeadContext.CursorX = OverHeadContext.DefaultX;
		OverHeadContext.CursorY = OverHeadContext.DefaultY;
	}

	if (DebugComponent->DebugIcon.Len() > 0)
	{
		UTexture2D* RegularIcon = (UTexture2D*)StaticLoadObject(UTexture2D::StaticClass(), NULL, *DebugComponent->DebugIcon, NULL, LOAD_NoWarn | LOAD_Quiet, NULL);
		if (RegularIcon)
		{
			FCanvasIcon Icon = UCanvas::MakeIcon(RegularIcon);
			if (Icon.Texture)
			{
				const float DesiredIconSize = bDrawFullOverHead ? 32.f : 16.f;
				DrawIcon(OverHeadContext, FColor::White, Icon, IconXLocation, IconYLocation - DesiredIconSize, DesiredIconSize / Icon.Texture->GetSurfaceWidth());
			}
		}
	}

	if (bDrawFullOverHead)
	{
		OverHeadContext.FontRenderInfo.bEnableShadow = bDrawFullOverHead;
		PrintString(OverHeadContext, bDrawFullOverHead ? FColor::White : FColor(255, 255, 255, 128), FString::Printf(TEXT("%s\n"), *ObjectName));
		OverHeadContext.FontRenderInfo.bEnableShadow = false;
	}

	if (EngineShowFlags.DebugAI)
	{
		DrawPath(PC, DebugComponent);
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void AGameplayDebuggingHUDComponent::DrawBasicData(APlayerController* PC, class UGameplayDebuggingComponent *DebugComponent)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	PrintString(DefaultContext, TEXT("\n{R=0,G=255,B=0,A=255}BASIC DATA\n"));

	UFont* OldFont = DefaultContext.Font;
	DefaultContext.Font = GEngine->GetMediumFont();
	PrintString(DefaultContext, FString::Printf(TEXT("Controller Name: {yellow}%s\n"), *DebugComponent->ControllerName));
	DefaultContext.Font = OldFont;

	PrintString(DefaultContext, FString::Printf(TEXT("Pawn Name: {yellow}%s{white}, Pawn Class: {yellow}%s\n"), *DebugComponent->PawnName, *DebugComponent->PawnClass));

	// movement
	if (DebugComponent->bIsUsingCharacter)
	{
		PrintString(DefaultContext, FString::Printf(TEXT("Movement Mode: {yellow}%s{white}, Base: {yellow}%s\n"), *DebugComponent->MovementModeInfo, *DebugComponent->MovementBaseInfo));
		PrintString(DefaultContext, FString::Printf(TEXT("NavData: {yellow}%s{white}, Path following: {yellow}%s\n"), *DebugComponent->NavDataInfo, *DebugComponent->PathFollowingInfo));
	}

	// logic
	if (DebugComponent->bIsUsingBehaviorTree)
	{
		PrintString(DefaultContext, FString::Printf(TEXT("Behavior: {yellow}%s{white}, Tree: {yellow}%s\n"), *DebugComponent->CurrentAIState, *DebugComponent->CurrentAIAssets));
		PrintString(DefaultContext, FString::Printf(TEXT("Active task: {yellow}%s\n"), *DebugComponent->CurrentAITask));
	}

	// ability + animation
	if (DebugComponent->bIsUsingAbilities && DebugComponent->bIsUsingCharacter)
	{
		PrintString(DefaultContext, FString::Printf(TEXT("Ability: {yellow}%s{white}, Montage: {yellow}%s\n"), *DebugComponent->AbilityInfo, *DebugComponent->MontageInfo));
	}
	else if (DebugComponent->bIsUsingCharacter)
	{
		PrintString(DefaultContext, FString::Printf(TEXT("Montage: {yellow}%s\n"), *DebugComponent->MontageInfo));
	}
	else if (DebugComponent->bIsUsingAbilities)
	{
		PrintString(DefaultContext, FString::Printf(TEXT("Ability: {yellow}%s\n"), *DebugComponent->AbilityInfo));
	}

	DrawPath(PC, DebugComponent);
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void AGameplayDebuggingHUDComponent::DrawBehaviorTreeData(APlayerController* PC, class UGameplayDebuggingComponent *DebugComponent)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	PrintString(DefaultContext, TEXT("\n{green}BEHAVIOR TREE\n"));
	PrintString(DefaultContext, FString::Printf(TEXT("Brain Component: {yellow}%s\n"), *DebugComponent->BrainComponentName));
	PrintString(DefaultContext, DebugComponent->BrainComponentString);
	PrintString(DefaultContext, FColor::White, DebugComponent->BlackboardString, 600.0f, 40.0f);
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void AGameplayDebuggingHUDComponent::DrawEQSData(APlayerController* PC, class UGameplayDebuggingComponent *DebugComponent)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) && WITH_EQS
	PrintString(DefaultContext, TEXT("\n{green}EQS {white}[Use + key to switch query]\n"));

	if (DebugComponent->EQSLocalData.Num() == 0)
	{
		return;
	}

	const int32 EQSIndex = DebugComponent->EQSLocalData.Num() > 0 ? FMath::Clamp(DebugComponent->CurrentEQSIndex, 0, DebugComponent->EQSLocalData.Num() - 1) : INDEX_NONE;
	if (!DebugComponent->EQSLocalData.IsValidIndex(EQSIndex))
	{
		return;
	}

	{
		int32 Index = 0;
		PrintString(DefaultContext, TEXT("{white}Queries: "));
		for (auto CurrentQuery : DebugComponent->EQSLocalData)
		{
			if (EQSIndex == Index)
			{
				PrintString(DefaultContext, FString::Printf(TEXT("{green}%s, "), *CurrentQuery.Name));
			}
			else
			{
				PrintString(DefaultContext, FString::Printf(TEXT("{yellow}%s, "), *CurrentQuery.Name));
			}
			Index++;
		}
		PrintString(DefaultContext, TEXT("\n"));
	}

	auto& CurrentLocalData = DebugComponent->EQSLocalData[EQSIndex];

	/** find and draw item selection */
	int32 BestItemIndex = INDEX_NONE;
	{
		APlayerController* const MyPC = Cast<APlayerController>(PlayerOwner);
		FVector CamLocation;
		FVector FireDir;
		if (!MyPC->GetSpectatorPawn())
		{
			FRotator CamRotation;
			MyPC->GetPlayerViewPoint(CamLocation, CamRotation);
			FireDir = CamRotation.Vector();
		}
		else
		{
			FireDir = DefaultContext.Canvas->SceneView->GetViewDirection();
			CamLocation = DefaultContext.Canvas->SceneView->ViewMatrices.ViewOrigin;
		}

		float bestAim = 0;
		for (int32 Index = 0; Index < CurrentLocalData.RenderDebugHelpers.Num(); ++Index)
		{
			auto& CurrentItem = CurrentLocalData.RenderDebugHelpers[Index];

			const FVector AimDir = CurrentItem.Location - CamLocation;
			float FireDist = AimDir.SizeSquared();

			FireDist = FMath::Sqrt(FireDist);
			float newAim = FireDir | AimDir;
			newAim = newAim / FireDist;
			if (newAim > bestAim)
			{
				BestItemIndex = Index;
				bestAim = newAim;
			}
		}

		if (BestItemIndex != INDEX_NONE)
		{
			DrawDebugSphere(World, CurrentLocalData.RenderDebugHelpers[BestItemIndex].Location, CurrentLocalData.RenderDebugHelpers[BestItemIndex].Radius, 8, FColor::Red, false);
			int32 FailedTestIndex = CurrentLocalData.RenderDebugHelpers[BestItemIndex].FailedTestIndex;
			if (FailedTestIndex != INDEX_NONE)
			{
				PrintString(DefaultContext, FString::Printf(TEXT("{red}Selected item failed with test %d: {yellow}%s {LightBlue}(%s)\n")
					, FailedTestIndex
					, *CurrentLocalData.Tests[FailedTestIndex].ShortName
					, *CurrentLocalData.Tests[FailedTestIndex].Detailed
					));
				PrintString(DefaultContext, FString::Printf(TEXT("{white}'%s' with score %3.3f\n\n"), *CurrentLocalData.RenderDebugHelpers[BestItemIndex].AdditionalInformation, CurrentLocalData.RenderDebugHelpers[BestItemIndex].FailedScore));
			}
		}
	}

	PrintString(DefaultContext, FString::Printf(TEXT("{white}Timestamp: {yellow}%.3f (%.2fs ago)\n")
		, CurrentLocalData.Timestamp, PC->GetWorld()->GetTimeSeconds() - CurrentLocalData.Timestamp
		));
	PrintString(DefaultContext, FString::Printf(TEXT("{white}Query ID: {yellow}%d\n")
		, CurrentLocalData.Id
		));

	PrintString(DefaultContext, FString::Printf(TEXT("{white}Query contains %d options: "), CurrentLocalData.Options.Num()));
	for (int32 OptionIndex = 0; OptionIndex < CurrentLocalData.Options.Num(); ++OptionIndex)
	{
		if (OptionIndex == CurrentLocalData.UsedOption)
		{
			PrintString(DefaultContext, FString::Printf(TEXT("{green}%s, "), *CurrentLocalData.Options[OptionIndex]));
		}
		else
		{
			PrintString(DefaultContext, FString::Printf(TEXT("{yellow}%s, "), *CurrentLocalData.Options[OptionIndex]));
		}
	}
	PrintString(DefaultContext, TEXT("\n"));

	const float RowHeight = 20.0f;
	const int32 NumTests = CurrentLocalData.Tests.Num();
	if (CurrentLocalData.NumValidItems > 0 && GetDebuggingReplicator()->EnableEQSOnHUD )
	{
		// draw test weights for best X items
		const int32 NumItems = CurrentLocalData.Items.Num();

		FCanvasTileItem TileItem(FVector2D(0, 0), GWhiteTexture, FVector2D(Canvas->SizeX, RowHeight), FLinearColor::Black);
		FLinearColor ColorOdd(0, 0, 0, 0.6f);
		FLinearColor ColorEven(0, 0, 0.4f, 0.4f);
		TileItem.BlendMode = SE_BLEND_Translucent;

		// table header		
		{
			DefaultContext.CursorY += RowHeight;
			const float HeaderY = DefaultContext.CursorY + 3.0f;
			TileItem.SetColor(ColorOdd);
			DrawItem(DefaultContext, TileItem, 0, DefaultContext.CursorY);

			float HeaderX = DefaultContext.CursorX;
			PrintString(DefaultContext, FColor::Yellow, FString::Printf(TEXT("Num items: %d"), CurrentLocalData.NumValidItems), HeaderX, HeaderY);
			HeaderX += 200.0f;

			PrintString(DefaultContext, FColor::White, TEXT("Score"), HeaderX, HeaderY);
			HeaderX += 50.0f;

			for (int32 TestIdx = 0; TestIdx < NumTests; TestIdx++)
			{
				PrintString(DefaultContext, FColor::White, FString::Printf(TEXT("Test %d"), TestIdx), HeaderX, HeaderY);
				HeaderX += 100.0f;
			}

			DefaultContext.CursorY += RowHeight;
		}

		// valid items
		for (int32 Idx = 0; Idx < NumItems; Idx++)
		{
			TileItem.SetColor((Idx % 2) ? ColorOdd : ColorEven);
			DrawItem(DefaultContext, TileItem, 0, DefaultContext.CursorY);

			DrawEQSItemDetails(Idx, DebugComponent);
			DefaultContext.CursorY += RowHeight;
		}
		DefaultContext.CursorY += RowHeight;
	}

	// test description
	PrintString(DefaultContext, TEXT("All tests from used option:\n"));
	for (int32 TestIdx = 0; TestIdx < NumTests; TestIdx++)
	{
		FString TestDesc = FString::Printf(TEXT("{white}Test %d = {yellow}%s {LightBlue}(%s)\n"), TestIdx,
			*CurrentLocalData.Tests[TestIdx].ShortName,
			*CurrentLocalData.Tests[TestIdx].Detailed);

		PrintString(DefaultContext, TestDesc);
	}

#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void AGameplayDebuggingHUDComponent::DrawEQSItemDetails(int32 ItemIdx, class UGameplayDebuggingComponent *DebugComponent)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) && WITH_EQS
	const float PosY = DefaultContext.CursorY + 1.0f;
	float PosX = DefaultContext.CursorX;

	const int32 EQSIndex = DebugComponent->EQSLocalData.Num() > 0 ? FMath::Clamp(DebugComponent->CurrentEQSIndex, 0, DebugComponent->EQSLocalData.Num() - 1) : INDEX_NONE;
	auto& CurrentLocalData = DebugComponent->EQSLocalData[EQSIndex];
	const EQSDebug::FItemData& ItemData = CurrentLocalData.Items[ItemIdx];

	PrintString(DefaultContext, FColor::White, ItemData.Desc, PosX, PosY);
	PosX += 200.0f;

	FString ScoreDesc = FString::Printf(TEXT("%.2f"), ItemData.TotalScore);
	PrintString(DefaultContext, FColor::Yellow, ScoreDesc, PosX, PosY);
	PosX += 50.0f;

	FCanvasTileItem ActiveTileItem(FVector2D(0, PosY + 15.0f), GWhiteTexture, FVector2D(0, 2.0f), FLinearColor::Yellow);
	FCanvasTileItem BackTileItem(FVector2D(0, PosY + 15.0f), GWhiteTexture, FVector2D(0, 2.0f), FLinearColor(0.1f, 0.1f, 0.1f));
	const float BarWidth = 80.0f;

	const int32 NumTests = ItemData.TestScores.Num();

	float TotalWeightedScore = 0.0f;
	for (int32 Idx = 0; Idx < NumTests; Idx++)
	{
		TotalWeightedScore += ItemData.TestScores[Idx];
	}

	for (int32 Idx = 0; Idx < NumTests; Idx++)
	{
		const float ScoreW = ItemData.TestScores[Idx];
		const float ScoreN = ItemData.TestValues[Idx];
		FString DescScoreW = FString::Printf(TEXT("%.2f"), ScoreW);
		FString DescScoreN = (ScoreN == UEnvQueryTypes::SkippedItemValue) ? TEXT("SKIP") : FString::Printf(TEXT("%.2f"), ScoreN);
		FString TestDesc = DescScoreW + FString(" {LightBlue}") + DescScoreN;

		float Pct = (TotalWeightedScore > KINDA_SMALL_NUMBER) ? (ScoreW / TotalWeightedScore) : 0.0f;
		ActiveTileItem.Position.X = PosX;
		ActiveTileItem.Size.X = BarWidth * Pct;
		BackTileItem.Position.X = PosX + ActiveTileItem.Size.X;
		BackTileItem.Size.X = FMath::Max(BarWidth * (1.0f - Pct), 0.0f);

		DrawItem(DefaultContext, ActiveTileItem, ActiveTileItem.Position.X, ActiveTileItem.Position.Y);
		DrawItem(DefaultContext, BackTileItem, BackTileItem.Position.X, BackTileItem.Position.Y);

		PrintString(DefaultContext, FColor::Green, TestDesc, PosX, PosY);
		PosX += 100.0f;
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST) && WITH_EQS
}

void AGameplayDebuggingHUDComponent::DrawPerception(APlayerController* PC, class UGameplayDebuggingComponent *DebugComponent)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (!DebugComponent)
	{
		return;
	}

	//@FIXME: It have to be changed to only draw data collected by Debugging Component, just moved functionality from FN for now
	APawn* MyPawn = Cast<APawn>(DebugComponent->GetSelectedActor());
	if (MyPawn)
	{
		AAIController* BTAI = Cast<AAIController>(MyPawn->GetController());
		if (BTAI)
		{
			// standalone only
			if (BTAI->GetAIPerceptionComponent() && DefaultContext.Canvas != NULL)
			{
				BTAI->GetAIPerceptionComponent()->DrawDebugInfo(DefaultContext.Canvas);

				const FVector AILocation = MyPawn->GetActorLocation();
				const FVector Facing = MyPawn->GetActorForwardVector();

				UAIPerceptionSystem* PerceptionSys = UAIPerceptionSystem::GetCurrent(this);
				if (PerceptionSys)
				{
					PrintString(DefaultContext, FColor::Green, TEXT("\nPERCEPTION COMPONENT\n"));
					PrintString(DefaultContext, FString::Printf(TEXT("Draw Colors:")));
					
					FString PerceptionLegend = PerceptionSys->GetPerceptionDebugLegend();
					PrintString(DefaultContext, *PerceptionLegend);
				}
				
				if (PC && PC->GetPawn())
				{
					const float DistanceFromPlayer = (MyPawn->GetActorLocation() - PC->GetPawn()->GetActorLocation()).Size();
					const float DistanceFromSensor = DebugComponent->SensingComponentLocation != FVector::ZeroVector ? (DebugComponent->SensingComponentLocation - PC->GetPawn()->GetActorLocation()).Size() : -1;
					PrintString(DefaultContext, FString::Printf(TEXT("Distance Sensor-PlayerPawn: %.1f\n"), DistanceFromSensor));
					PrintString(DefaultContext, FString::Printf(TEXT("Distance Pawn-PlayerPawn: %.1f\n"), DistanceFromPlayer));
				}
			}
		}
	}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void AGameplayDebuggingHUDComponent::DrawNavMeshSnapshot(APlayerController* PC, class UGameplayDebuggingComponent *DebugComponent)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (DebugComponent && DebugComponent->NavmeshRepData.Num())
	{
		UGameplayDebuggingControllerComponent*  GDC = PC ? PC->FindComponentByClass<UGameplayDebuggingControllerComponent>() : NULL;
		FString NextUpdateDesc;
		if (GDC)
		{
			const float TimeLeft = GDC->GetUpdateNavMeshTimeRemaining();
			NextUpdateDesc = FString::Printf(TEXT(", next update: {yellow}%.1fs"), TimeLeft);
		}

		PrintString(DefaultContext, FString::Printf(TEXT("\n\n{green}Showing NavMesh (%.1fkB)%s\n"),
			DebugComponent->NavmeshRepData.Num() / 1024.0f, *NextUpdateDesc));
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

//////////////////////////////////////////////////////////////////////////
void AGameplayDebuggingHUDComponent::PrintString(FPrintContext& Context, const FString& InString )
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	class FParserHelper
	{
		enum TokenType
		{
			OpenTag,
			CloseTag,
			NewLine,
			EndOfString,
			RegularChar,
		};

		enum Tag
		{
			DefinedColor,
			OtherColor,
			ErrorTag,
		};

		uint8 ReadToken()
		{
			uint8 OutToken = RegularChar;
			TCHAR ch = Index < DataString.Len() ? DataString[Index] : '\0';
			switch(ch)
			{
			case '\0':
				OutToken = EndOfString;
				break;
			case '{':
				OutToken = OpenTag;
				Index++;
				break;
			case '}':
				OutToken = CloseTag;
				Index++;
				break;
			case '\n':
				OutToken = NewLine;
				Index++;
				break;
			default:
				OutToken = RegularChar;
				break;
			}
			return OutToken;
		}

		uint32 ParseTag( FString& OutData )
		{
			FString TagString;
			int32 OryginalIndex = Index;
			uint8 token = ReadToken();
			while (token != EndOfString && token != CloseTag)
			{
				if (token == RegularChar)
				{
					TagString.AppendChar(DataString[Index++]);
				}
				token = ReadToken();
			}

			int OutTag = ErrorTag;

			if (token != CloseTag)
			{
				Index = OryginalIndex;
				OutData = FString::Printf( TEXT("{%s"), *TagString);
				OutData.AppendChar(DataString[Index-1]);
				return OutTag;
			}

			if (GColorList.IsValidColorName(*TagString.ToLower()))
			{
				OutTag = DefinedColor;
				OutData = TagString;
			}
			else
			{
				FColor Color;
				if (Color.InitFromString(TagString))
				{
					OutTag = OtherColor;
					OutData = TagString;
				}
				else
				{
					OutTag = ErrorTag;
					OutData = FString::Printf( TEXT("{%s"), *TagString);
					OutData.AppendChar(DataString[Index-1]);
				}
			}
			//Index++;
			return OutTag;
		}

		struct StringNode
		{
			FString String;
			FColor Color;
			bool bNewLine;
			StringNode() : Color(FColor::White), bNewLine(false) {}
		};
		int32 Index;
		FString DataString;
	public:
		TArray<StringNode> Strings;

		void ParseString(const FString& StringToParse)
		{
			Index = 0;
			DataString = StringToParse;
			Strings.Add(StringNode());
			if (Index >= DataString.Len())
				return;

			uint8 Token = ReadToken();
			while (Token != EndOfString)
			{
				switch (Token)
				{
				case RegularChar:
					Strings[Strings.Num()-1].String.AppendChar( DataString[Index++] );
					break;
				case NewLine:
					Strings.Add(StringNode());
					Strings[Strings.Num()-1].bNewLine = true;
					Strings[Strings.Num()-1].Color = Strings[Strings.Num()-2].Color;
					break;
				case EndOfString:
					break;
				case OpenTag:
					{
						FString OutData;
						switch (ParseTag(OutData))
						{
						case DefinedColor:
							{
								int32 i = Strings.Add(StringNode());
								Strings[i].Color = GColorList.GetFColorByName(*OutData.ToLower());
							}
							break;
						case OtherColor:
							{
								FColor NewColor; 
								if (NewColor.InitFromString( OutData ))
								{
									int32 i = Strings.Add(StringNode());
									Strings[i].Color = NewColor;
									break;
								}
							}
						default:
							Strings[Strings.Num()-1].String += OutData;
							break;
						}
					}
					break;
				}
				Token = ReadToken();
			}
		}
	};

	FParserHelper Helper;
	Helper.ParseString( InString );

	float YMovement = 0;
	float XL = 0.f, YL = 0.f;
	CalulateStringSize(Context, Context.Font, TEXT("X"), XL, YL);

	for (int32 Index=0; Index < Helper.Strings.Num(); ++Index)
	{
		if (Index > 0 && Helper.Strings[Index].bNewLine)
		{
			if (Context.Canvas != NULL && YMovement + Context.CursorY > Context.Canvas->ClipY)
			{
				Context.DefaultX += Context.Canvas->ClipX / 2;
				Context.CursorX = Context.DefaultX;
				Context.CursorY = Context.DefaultY;
				YMovement = 0;
			}
			YMovement += YL;
			Context.CursorX = Context.DefaultX;
		}
		const FString Str = Helper.Strings[Index].String;

		if (Str.Len() > 0 && Context.Canvas != NULL)
		{
			float SizeX, SizeY;
			CalulateStringSize(Context, Context.Font, Str, SizeX, SizeY);

			FCanvasTextItem TextItem( FVector2D::ZeroVector, FText::FromString(Str), Context.Font, FLinearColor::White );
			if (Context.FontRenderInfo.bEnableShadow)
			{
				TextItem.EnableShadow( FColor::Black, FVector2D( 1, 1 ) );
			}

			TextItem.SetColor(Helper.Strings[Index].Color);
			DrawItem( Context, TextItem, Context.CursorX, YMovement + Context.CursorY );
			Context.CursorX += SizeX;
		}
	}

	Context.CursorY += YMovement;
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void AGameplayDebuggingHUDComponent::PrintString(FPrintContext& Context, const FColor& InColor, const FString& InString )
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	PrintString(Context, FString::Printf(TEXT("{%s}%s"), *InColor.ToString(), *InString));
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void AGameplayDebuggingHUDComponent::PrintString(FPrintContext& Context, const FColor& InColor, const FString& InString, float X, float Y )
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	const float OldX = Context.CursorX, OldY = Context.CursorY;
	const float OldDX = Context.DefaultX, OldDY = Context.DefaultY;

	Context.DefaultX = Context.CursorX = X;
	Context.DefaultY = Context.CursorY = Y;
	PrintString(Context, InColor, InString);

	Context.CursorX = OldX;
	Context.CursorY = OldY;
	Context.DefaultX = OldDX;
	Context.DefaultY = OldDY;
#endif
}

void AGameplayDebuggingHUDComponent::CalulateStringSize(const AGameplayDebuggingHUDComponent::FPrintContext& DefaultContext, UFont* Font, const FString& InString, float& OutX, float& OutY )
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	FString String = InString;
	const FRegexPattern ElementRegexPattern(TEXT("\\{.*?\\}"));
	FRegexMatcher ElementRegexMatcher(ElementRegexPattern, String);

	while (ElementRegexMatcher.FindNext())
	{
		int32 AttributeListBegin = ElementRegexMatcher.GetCaptureGroupBeginning(0);
		int32 AttributeListEnd = ElementRegexMatcher.GetCaptureGroupEnding(0);
		String.RemoveAt(AttributeListBegin, AttributeListEnd - AttributeListBegin);
		ElementRegexMatcher = FRegexMatcher(ElementRegexPattern, String);
	}

	OutX = OutY = 0;
	if (DefaultContext.Canvas != NULL)
	{
		DefaultContext.Canvas->StrLen(Font != NULL ? Font : DefaultContext.Font, String, OutX, OutY);
	}
#endif
}

void AGameplayDebuggingHUDComponent::CalulateTextSize(const AGameplayDebuggingHUDComponent::FPrintContext& DefaultContext, UFont* Font, const FText& InText, float& OutX, float& OutY)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	CalulateStringSize(DefaultContext, Font, InText.ToString(), OutX, OutY);
#endif
}

FVector AGameplayDebuggingHUDComponent::ProjectLocation(const AGameplayDebuggingHUDComponent::FPrintContext& Context, const FVector& Location)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (Context.Canvas != NULL)
	{
		return Context.Canvas->Project(Location);
	}
#endif
	return FVector();
}

void AGameplayDebuggingHUDComponent::DrawItem(const AGameplayDebuggingHUDComponent::FPrintContext& DefaultContext, class FCanvasItem& Item, float X, float Y )
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (DefaultContext.Canvas)
	{
		DefaultContext.Canvas->DrawItem( Item, FVector2D( X, Y ) );
	}
#endif
}

void AGameplayDebuggingHUDComponent::DrawIcon(const AGameplayDebuggingHUDComponent::FPrintContext& DefaultContext, const FColor& InColor, const FCanvasIcon& Icon, float X, float Y, float Scale)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (DefaultContext.Canvas)
	{
		DefaultContext.Canvas->SetDrawColor(InColor);
		DefaultContext.Canvas->DrawIcon(Icon, X, Y, Scale);
	}
#endif
}

