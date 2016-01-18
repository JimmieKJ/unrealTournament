// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "AIModulePrivate.h"
#include "Misc/CoreMisc.h"
#include "Net/UnrealNetwork.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "EnvironmentQuery/EQSRenderingComponent.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "DebugRenderSceneProxy.h"
#include "EQSGameplayDebuggerObject.h"
#if WITH_EDITOR
#	include "Editor/EditorEngine.h"
#endif

#if ENABLED_GAMEPLAY_DEBUGGER && USE_EQS_DEBUGGER
	static TAutoConsoleVariable<int32> CVarEQSDetailsOnHUD(
		TEXT("ai.gd.EQSOnHUD"),
		1,
		TEXT("Enable or disable EQS details table on screen.\n")
		TEXT(" 0: Disable details\n")
		TEXT(" 1: Enable details (default)"),
		ECVF_Cheat);
#endif

UEQSGameplayDebuggerObject::UEQSGameplayDebuggerObject(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer), MaxEQSQueries(5)
{
#if ENABLED_GAMEPLAY_DEBUGGER && USE_EQS_DEBUGGER
	ItemDescriptionWidth = 312.f; // 200.0f;
	ItemScoreWidth = 50.0f;
	TestScoreWidth = 100.0f;
	LastDataCaptureTime = 0;
#endif
}

void UEQSGameplayDebuggerObject::OnCycleDetailsView()
{
#if ENABLED_GAMEPLAY_DEBUGGER && USE_EQS_DEBUGGER
	if (++CurrentEQSIndex >= EQSLocalData.Num())
	{
		CurrentEQSIndex = 0;
	}

	MarkRenderStateDirty();
#endif //ENABLED_GAMEPLAY_DEBUGGER
}

const FEnvQueryResult* UEQSGameplayDebuggerObject::GetQueryResult() const
{
#if ENABLED_GAMEPLAY_DEBUGGER && USE_EQS_DEBUGGER
	return CachedQueryInstance.Get();
#endif
	return nullptr;
}

const FEnvQueryInstance* UEQSGameplayDebuggerObject::GetQueryInstance() const
{
#if ENABLED_GAMEPLAY_DEBUGGER && USE_EQS_DEBUGGER
	return CachedQueryInstance.Get();
#endif
	return nullptr;
}

void UEQSGameplayDebuggerObject::OnDataReplicationComplited(const TArray<uint8>& ReplicatedData)
{
#if ENABLED_GAMEPLAY_DEBUGGER && USE_EQS_DEBUGGER
	// decode scoring data
	FMemoryReader ArReader(ReplicatedData);
	ArReader << EQSLocalData;

	MarkRenderStateDirty();
#endif
}

void UEQSGameplayDebuggerObject::CollectDataToReplicate(APlayerController* MyPC, AActor *SelectedActor)
{
	Super::CollectDataToReplicate(MyPC, SelectedActor);
#if	ENABLED_GAMEPLAY_DEBUGGER && USE_EQS_DEBUGGER
	if (GetWorld()->TimeSeconds - LastDataCaptureTime < 2)
	{
		return;
	}

	UWorld* World = GetWorld();
	UEnvQueryManager* QueryManager = World ? UEnvQueryManager::GetCurrent(World) : NULL;
	const AActor* Owner = SelectedActor;

	if (QueryManager == NULL || Owner == NULL)
	{
		return;
	}

	LastDataCaptureTime = GetWorld()->TimeSeconds;

	auto AllQueries = QueryManager->GetDebugger().GetAllQueriesForOwner(Owner);
	const class APawn* OwnerAsPawn = Cast<class APawn>(Owner);
	if (OwnerAsPawn != NULL && OwnerAsPawn->GetController())
	{
		const auto& AllControllerQueries = QueryManager->GetDebugger().GetAllQueriesForOwner(OwnerAsPawn->GetController());
		AllQueries.Append(AllControllerQueries);
	}
	struct FEnvQueryInfoSort
	{
		FORCEINLINE bool operator()(const FEQSDebugger::FEnvQueryInfo& A, const FEQSDebugger::FEnvQueryInfo& B) const
		{
			return (A.Timestamp < B.Timestamp);
		}
	};
	TArray<FEQSDebugger::FEnvQueryInfo> QueriesToSort = AllQueries;
	QueriesToSort.Sort(FEnvQueryInfoSort()); //sort queries by timestamp
	QueriesToSort.SetNum(FMath::Min<int32>(MaxEQSQueries, AllQueries.Num()));

	for (int32 Index = AllQueries.Num() - 1; Index >= 0; --Index)
	{
		auto &CurrentQuery = AllQueries[Index];
		if (QueriesToSort.Find(CurrentQuery) == INDEX_NONE)
		{
			AllQueries.RemoveAt(Index);
		}
	}


	EQSLocalData.Reset();
	for (int32 Index = 0; Index < FMath::Min<int32>(MaxEQSQueries, AllQueries.Num()); ++Index)
	{
		EQSDebug::FQueryData* CurrentLocalData = NULL;
		CachedQueryInstance = AllQueries[Index].Instance;
		const float CachedTimestamp = AllQueries[Index].Timestamp;

		if (!CurrentLocalData)
		{
			EQSLocalData.AddZeroed();
			CurrentLocalData = &EQSLocalData[EQSLocalData.Num() - 1];
		}

		if (CachedQueryInstance.IsValid())
		{
			UEnvQueryDebugHelpers::QueryToDebugData(*CachedQueryInstance, *CurrentLocalData);
		}
		CurrentLocalData->Timestamp = AllQueries[Index].Timestamp;
	}

	TArray<uint8> UncompressedBuffer;
	FMemoryWriter ArWriter(UncompressedBuffer);
	ArWriter << EQSLocalData;

	RequestDataReplication(TEXT("EQSData"), UncompressedBuffer);
#endif
}

void UEQSGameplayDebuggerObject::DrawEQSItemDetails(int32 ItemIdx, AActor* SelectedActor)
{
#if ENABLED_GAMEPLAY_DEBUGGER && USE_EQS_DEBUGGER
	const float PosY = DefaultContext.CursorY + 1.0f;
	float PosX = DefaultContext.CursorX;

	const int32 EQSIndex = EQSLocalData.Num() > 0 ? FMath::Clamp(CurrentEQSIndex, 0, EQSLocalData.Num() - 1) : INDEX_NONE;
	auto& CurrentLocalData = EQSLocalData[EQSIndex];
	const EQSDebug::FItemData& ItemData = CurrentLocalData.Items[ItemIdx];

	PrintString(DefaultContext, FColor::White, ItemData.Desc, PosX, PosY);
	PosX += ItemDescriptionWidth;

	FString ScoreDesc = FString::Printf(TEXT("%.2f"), ItemData.TotalScore);
	PrintString(DefaultContext, FColor::Yellow, ScoreDesc, PosX, PosY);
	PosX += ItemScoreWidth;

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
		PosX += TestScoreWidth;
	}
#endif //USE_EQS_DEBUGGER
}

void UEQSGameplayDebuggerObject::DrawCollectedData(APlayerController* MyPC, AActor *SelectedActor)
{
	Super::DrawCollectedData(MyPC, SelectedActor);

#if ENABLED_GAMEPLAY_DEBUGGER && USE_EQS_DEBUGGER
	PrintString(DefaultContext, TEXT("\n{green}EQS {white}[Use + key to switch query]\n"));

	if (EQSLocalData.Num() == 0)
	{
		return;
	}

	const int32 EQSIndex = EQSLocalData.Num() > 0 ? FMath::Clamp(CurrentEQSIndex, 0, EQSLocalData.Num() - 1) : INDEX_NONE;
	if (!EQSLocalData.IsValidIndex(EQSIndex))
	{
		return;
	}

	{
		int32 Index = 0;
		PrintString(DefaultContext, TEXT("{white}Queries: "));
		for (auto CurrentQuery : EQSLocalData)
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

	auto& CurrentLocalData = EQSLocalData[EQSIndex];

	/** find and draw item selection */
	int32 BestItemIndex = INDEX_NONE;
	{
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
			DrawDebugSphere(GetWorld(), CurrentLocalData.RenderDebugHelpers[BestItemIndex].Location, CurrentLocalData.RenderDebugHelpers[BestItemIndex].Radius, 8, FColor::Red, false);
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
		, CurrentLocalData.Timestamp, GetWorld()->GetTimeSeconds() - CurrentLocalData.Timestamp
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
	const IConsoleVariable* CVarEQSOnHUD = IConsoleManager::Get().FindConsoleVariable(TEXT("ai.gd.EQSOnHUD"));
	const bool bEQSOnHUD = !CVarEQSOnHUD || CVarEQSOnHUD->GetInt();
	if (CurrentLocalData.NumValidItems > 0 && bEQSOnHUD)
	{
		// draw test weights for best X items
		const int32 NumItems = CurrentLocalData.Items.Num();
		
		FCanvasTileItem TileItem(FVector2D(0, 0), GWhiteTexture, FVector2D(DefaultContext.Canvas->SizeX, RowHeight), FLinearColor::Black);
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
			HeaderX += ItemDescriptionWidth;

			PrintString(DefaultContext, FColor::White, TEXT("Score"), HeaderX, HeaderY);
			HeaderX += ItemScoreWidth;

			for (int32 TestIdx = 0; TestIdx < NumTests; TestIdx++)
			{
				PrintString(DefaultContext, FColor::White, FString::Printf(TEXT("Test %d"), TestIdx), HeaderX, HeaderY);
				HeaderX += TestScoreWidth;
			}

			DefaultContext.CursorY += RowHeight;
		}

		// valid items
		for (int32 Idx = 0; Idx < NumItems; Idx++)
		{
			TileItem.SetColor((Idx % 2) ? ColorOdd : ColorEven);
			DrawItem(DefaultContext, TileItem, 0, DefaultContext.CursorY);

			DrawEQSItemDetails(Idx, SelectedActor);
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

#endif //USE_EQS_DEBUGGER
}

class FDebugRenderSceneProxy* UEQSGameplayDebuggerObject::CreateSceneProxy(const UPrimitiveComponent* InComponent, UWorld* World, AActor* SelectedActor)
{
#if ENABLED_GAMEPLAY_DEBUGGER && USE_EQS_DEBUGGER
	if (/*IsClientEQSSceneProxyEnabled() &&*/ SelectedActor != NULL)
	{
		const int32 EQSIndex = EQSLocalData.Num() > 0 ? FMath::Clamp(CurrentEQSIndex, 0, EQSLocalData.Num() - 1) : INDEX_NONE;
		if (EQSLocalData.IsValidIndex(EQSIndex))
		{
			auto& CurrentLocalData = EQSLocalData[EQSIndex];

			FString ViewFlagName = TEXT("Game");
#if WITH_EDITOR
			UEditorEngine* EEngine = Cast<UEditorEngine>(GEngine);
			if (EEngine && EEngine->bIsSimulatingInEditor)
			{
				ViewFlagName = TEXT("DebugAI");
			}
#endif
			return new FEQSSceneProxy(InComponent, ViewFlagName, CurrentLocalData.SolidSpheres, CurrentLocalData.Texts);
		}
	}
#endif // USE_EQS_DEBUGGER

	return nullptr;
}

