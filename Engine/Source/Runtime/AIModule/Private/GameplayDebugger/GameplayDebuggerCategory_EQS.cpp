// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"

#include "EnvironmentQuery/EnvQueryManager.h"
#include "EnvironmentQuery/EQSRenderingComponent.h"
#include "GameplayDebuggerCategory_EQS.h"
#include "Engine/Canvas.h"
#include "DrawDebugHelpers.h"

#if WITH_GAMEPLAY_DEBUGGER

static TAutoConsoleVariable<int32> CVarEQSDetailsOnHUD(
	TEXT("ai.debug.EQSOnHUD"),
	0,
	TEXT("Enable or disable EQS details table on screen.\n")
	TEXT(" 0: Disable details\n")
	TEXT(" 1: Enable details (default)"),
	ECVF_Cheat);

FGameplayDebuggerCategory_EQS::FGameplayDebuggerCategory_EQS()
{
	MaxQueries = 5;
	MaxItemTableRows = 20;
	ShownQueryIndex = 0;
	CollectDataInterval = 2.0f;

	BindKeyPress(TEXT("Multiply"), this, &FGameplayDebuggerCategory_EQS::CycleShownQueries);

#if USE_EQS_DEBUGGER
	SetDataPackReplication<FRepData>(&DataPack);
#endif
}

TSharedRef<FGameplayDebuggerCategory> FGameplayDebuggerCategory_EQS::MakeInstance()
{
	return MakeShareable(new FGameplayDebuggerCategory_EQS());
}

#if USE_EQS_DEBUGGER
void FGameplayDebuggerCategory_EQS::FRepData::Serialize(FArchive& Ar)
{
	Ar << QueryDebugData;
}
#endif // USE_EQS_DEBUGGER

void FGameplayDebuggerCategory_EQS::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
#if USE_EQS_DEBUGGER
	UWorld* World = OwnerPC->GetWorld();
	UEnvQueryManager* QueryManager = World ? UEnvQueryManager::GetCurrent(World) : nullptr;
	if (QueryManager == nullptr)
	{
		return;
	}

	TArray<FEQSDebugger::FEnvQueryInfo> AuthQueryData = QueryManager->GetDebugger().GetAllQueriesForOwner(DebugActor);

	APawn* DebugPawn = Cast<APawn>(DebugActor);
	if (DebugPawn && DebugPawn->GetController())
	{
		TArray<FEQSDebugger::FEnvQueryInfo>& AuthControllerQueryData = QueryManager->GetDebugger().GetAllQueriesForOwner(DebugPawn->GetController());
		AuthQueryData.Append(AuthControllerQueryData);
	}

	struct FEnvQuerySortNewFirst
	{
		bool operator()(const FEQSDebugger::FEnvQueryInfo& A, const FEQSDebugger::FEnvQueryInfo& B) const
		{
			return (A.Timestamp < B.Timestamp);
		}
	};

	AuthQueryData.Sort(FEnvQuerySortNewFirst());

	for (int32 Idx = 0; Idx < AuthQueryData.Num() && DataPack.QueryDebugData.Num() < MaxQueries; Idx++)
	{
		FEnvQueryInstance* QueryInstance = AuthQueryData[Idx].Instance.Get();
		if (QueryInstance)
		{
			EQSDebug::FQueryData DebugItem;
			UEnvQueryDebugHelpers::QueryToDebugData(*QueryInstance, DebugItem);

			DataPack.QueryDebugData.Add(DebugItem);
		}
	}
#endif // USE_EQS_DEBUGGER
}

void FGameplayDebuggerCategory_EQS::OnDataPackReplicated(int32 DataPackId)
{
	MarkRenderStateDirty();

#if USE_EQS_DEBUGGER
	if (!DataPack.QueryDebugData.IsValidIndex(ShownQueryIndex))
	{
		ShownQueryIndex = 0;
	}
#endif
}

FDebugRenderSceneProxy* FGameplayDebuggerCategory_EQS::CreateSceneProxy(const UPrimitiveComponent* InComponent)
{
#if USE_EQS_DEBUGGER
	if (DataPack.QueryDebugData.IsValidIndex(ShownQueryIndex))
	{
		const EQSDebug::FQueryData& QueryData = DataPack.QueryDebugData[ShownQueryIndex];
		const FString ViewFlagName = GetSceneProxyViewFlag();
		return new FEQSSceneProxy(InComponent, ViewFlagName, QueryData.SolidSpheres, QueryData.Texts);
	}
#endif // USE_EQS_DEBUGGER

	return nullptr;
}

void FGameplayDebuggerCategory_EQS::DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext)
{
#if USE_EQS_DEBUGGER

	const FString HeaderDesc = (DataPack.QueryDebugData.Num() > 1) ?
		FString::Printf(TEXT("Queries: {yellow}%d{white}, press {yellow}[*]{white} to cycle through"), DataPack.QueryDebugData.Num()) :
		FString::Printf(TEXT("Queries: {yellow}%d"), DataPack.QueryDebugData.Num());

	CanvasContext.Print(HeaderDesc);
	if (DataPack.QueryDebugData.Num() == 0)
	{
		return;
	}

	FString QueryDesc(TEXT("\t"));
	for (int32 Idx = 0; Idx < DataPack.QueryDebugData.Num(); Idx++)
	{
		QueryDesc += (Idx == ShownQueryIndex) ? TEXT("[{green}") : TEXT("[");
		QueryDesc += DataPack.QueryDebugData[Idx].Name;
		QueryDesc += (Idx == ShownQueryIndex) ? TEXT("{white}] ") : TEXT("] ");
	}
	CanvasContext.Print(QueryDesc);

	if (DataPack.QueryDebugData.IsValidIndex(ShownQueryIndex))
	{
		const EQSDebug::FQueryData& ShownQueryData = DataPack.QueryDebugData[ShownQueryIndex];

		CanvasContext.MoveToNewLine();
		CanvasContext.Printf(TEXT("Timestamp: {yellow}%.3f (~ %.2fs ago)"), ShownQueryData.Timestamp, OwnerPC->GetWorld()->TimeSince(ShownQueryData.Timestamp));
		CanvasContext.Printf(TEXT("Id: {yellow}%d"), ShownQueryData.Id);

		FString OptionsDesc(TEXT("Options: "));
		for (int32 Idx = 0; Idx < ShownQueryData.Options.Num(); Idx++)
		{
			QueryDesc += (Idx == ShownQueryData.UsedOption) ? TEXT("[{green}") : TEXT("[");
			QueryDesc += ShownQueryData.Options[Idx];
			QueryDesc += (Idx == ShownQueryData.UsedOption) ? TEXT("{white}] ") : TEXT("] ");
		}
		CanvasContext.Print(OptionsDesc);

		DrawLookedAtItem(ShownQueryData, OwnerPC, CanvasContext);

		CanvasContext.MoveToNewLine();
		DrawDetailedItemTable(ShownQueryData, CanvasContext);
	}
#else // USE_EQS_DEBUGGER
	CanvasContext.Print(FColor::Red, TEXT("Unable to gather EQS debug data, use build with USE_EQS_DEBUGGER enabled."));
#endif // USE_EQS_DEBUGGER
}

#if USE_EQS_DEBUGGER
void FGameplayDebuggerCategory_EQS::DrawLookedAtItem(const EQSDebug::FQueryData& QueryData, APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext) const
{
	FVector CameraLocation;
	FVector CameraDirection;
	if (!OwnerPC->GetSpectatorPawn())
	{
		FRotator CameraRotation;
		OwnerPC->GetPlayerViewPoint(CameraLocation, CameraRotation);
		CameraDirection = CameraRotation.Vector();
	}
	else
	{
		CameraDirection = CanvasContext.Canvas->SceneView->GetViewDirection();
		CameraLocation = CanvasContext.Canvas->SceneView->ViewMatrices.ViewOrigin;
	}

	int32 BestItemIndex = INDEX_NONE;
	float BestScore = -FLT_MAX;
	for (int32 Idx = 0; Idx < QueryData.RenderDebugHelpers.Num(); Idx++)
	{
		const EQSDebug::FDebugHelper& ItemInfo = QueryData.RenderDebugHelpers[Idx];
		const FVector DirToItem = ItemInfo.Location - CameraLocation;
		float DistToItem = DirToItem.Size();
		if (FMath::IsNearlyZero(DistToItem))
		{
			DistToItem = 1.0f;
		}

		const float ItemScore = FVector::DotProduct(DirToItem, CameraDirection) / DistToItem;
		if (ItemScore > BestScore)
		{
			BestItemIndex = Idx;
			BestScore = ItemScore;
		}
	}

	if (BestItemIndex != INDEX_NONE)
	{
		DrawDebugSphere(OwnerPC->GetWorld(), QueryData.RenderDebugHelpers[BestItemIndex].Location, QueryData.RenderDebugHelpers[BestItemIndex].Radius, 8, FColor::Red);

		FString ItemDesc;

		const int32 FailedTestIndex = QueryData.RenderDebugHelpers[BestItemIndex].FailedTestIndex;
		if (FailedTestIndex != INDEX_NONE)
		{
			ItemDesc = FString::Printf(TEXT("{red}Selected item:%d failed test[%d]: {yellow}%s {LightBlue}(%s)\n{white}'%s' with score: %3.3f"),
				BestItemIndex, FailedTestIndex,
				*QueryData.Tests[FailedTestIndex].ShortName,
				*QueryData.Tests[FailedTestIndex].Detailed,
				*QueryData.RenderDebugHelpers[BestItemIndex].AdditionalInformation,
				QueryData.RenderDebugHelpers[BestItemIndex].FailedScore);
		}
		else
		{
			ItemDesc = FString::Printf(TEXT("Selected item:{yellow}%d"), BestItemIndex);
		}

		CanvasContext.Print(ItemDesc);
	}
}

namespace FEQSDebugTable
{
	const float RowHeight = 20.0f;
	const float ItemDescriptionWidth = 312.0f;
	const float ItemScoreWidth = 50.0f;
	const float TestScoreWidth = 100.0f;
}

void FGameplayDebuggerCategory_EQS::DrawDetailedItemTable(const EQSDebug::FQueryData& QueryData, FGameplayDebuggerCanvasContext& CanvasContext) const
{
	const int32 EnableTableView = CVarEQSDetailsOnHUD.GetValueOnGameThread();
	CanvasContext.Printf(TEXT("Detailed table view: {%s}%s{white}, use '{green}ai.debug.EQSOnHUD %d{white}' to toggle"),
		EnableTableView ? TEXT("green") : TEXT("red"),
		EnableTableView ? TEXT("active") : TEXT("disabled"),
		EnableTableView ? 0 : 1);

	if (EnableTableView && QueryData.NumValidItems > 0)
	{
		FCanvasTileItem TileItem(FVector2D(0, 0), GWhiteTexture, FVector2D(CanvasContext.Canvas->SizeX, FEQSDebugTable::RowHeight), FLinearColor::Black);
		FLinearColor ColorOdd(0, 0, 0, 0.6f);
		FLinearColor ColorEven(0, 0, 0.4f, 0.4f);
		TileItem.BlendMode = SE_BLEND_Translucent;

		// table header
		CanvasContext.CursorY += FEQSDebugTable::RowHeight;
		const float HeaderY = CanvasContext.CursorY + 3.0f;
		TileItem.SetColor(ColorOdd);
		CanvasContext.DrawItem(TileItem, 0, CanvasContext.CursorY);

		float HeaderX = CanvasContext.CursorX;
		CanvasContext.PrintfAt(HeaderX, HeaderY, FColor::Yellow, TEXT("Num items: %d"), QueryData.NumValidItems);
		HeaderX += FEQSDebugTable::ItemDescriptionWidth;

		CanvasContext.PrintAt(HeaderX, HeaderY, FColor::White, TEXT("Score"));
		HeaderX += FEQSDebugTable::ItemScoreWidth;

		for (int32 Idx = 0; Idx < QueryData.Tests.Num(); Idx++)
		{
			CanvasContext.PrintfAt(HeaderX, HeaderY, FColor::White, TEXT("Test %d"), Idx);
			HeaderX += FEQSDebugTable::TestScoreWidth;
		}

		CanvasContext.CursorY += FEQSDebugTable::RowHeight;

		// item rows
		const int32 MaxShownItems = FMath::Min(MaxItemTableRows, QueryData.Items.Num());
		for (int32 Idx = 0; Idx < MaxShownItems; Idx++)
		{
			TileItem.SetColor((Idx % 2) ? ColorOdd : ColorEven);
			CanvasContext.DrawItem(TileItem, 0, CanvasContext.CursorY);

			DrawDetailedItemRow(QueryData.Items[Idx], CanvasContext);
			CanvasContext.CursorY += FEQSDebugTable::RowHeight;
		}

		// test description
		CanvasContext.Print(TEXT("All tests from used option:"));
		for (int32 Idx = 0; Idx < QueryData.Tests.Num(); Idx++)
		{
			CanvasContext.Printf(TEXT("Test %d = {yellow}%s {LightBlue}(%s)"), Idx, *QueryData.Tests[Idx].ShortName, *QueryData.Tests[Idx].Detailed);
		}
	}
}

void FGameplayDebuggerCategory_EQS::DrawDetailedItemRow(const EQSDebug::FItemData& ItemData, FGameplayDebuggerCanvasContext& CanvasContext) const
{
	const float PosY = CanvasContext.CursorY + 1.0f;
	float PosX = CanvasContext.CursorX;

	CanvasContext.PrintAt(PosX, PosY, FColor::White, ItemData.Desc);
	PosX += FEQSDebugTable::ItemDescriptionWidth;

	CanvasContext.PrintfAt(PosX, PosY, FColor::Yellow, TEXT("%.2f"), ItemData.TotalScore);
	PosX += FEQSDebugTable::ItemScoreWidth;

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

		CanvasContext.DrawItem(ActiveTileItem, ActiveTileItem.Position.X, ActiveTileItem.Position.Y);
		CanvasContext.DrawItem(BackTileItem, BackTileItem.Position.X, BackTileItem.Position.Y);

		CanvasContext.PrintAt(PosX, PosY, FColor::Green, TestDesc);
		PosX += FEQSDebugTable::TestScoreWidth;
	}
}
#endif // USE_EQS_DEBUGGER

void FGameplayDebuggerCategory_EQS::CycleShownQueries()
{
#if USE_EQS_DEBUGGER
	ShownQueryIndex = (DataPack.QueryDebugData.Num() > 0) ? ((ShownQueryIndex + 1) % DataPack.QueryDebugData.Num()) : 0;
#endif // USE_EQS_DEBUGGER
	MarkRenderStateDirty();
}

#endif // ENABLE_GAMEPLAY_DEBUGGER
