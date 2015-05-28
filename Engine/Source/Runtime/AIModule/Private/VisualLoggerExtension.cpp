// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "DrawDebugHelpers.h"
#include "EnvironmentQuery/EnvQueryDebugHelpers.h"
#include "EnvironmentQuery/EQSRenderingComponent.h"
#include "VisualLoggerExtension.h"

#if ENABLE_VISUAL_LOG
FVisualLoggerExtension::FVisualLoggerExtension()
	: CachedEQSId(INDEX_NONE)
	, SelectedEQSId(INDEX_NONE)
	, CurrentTimestamp(FLT_MIN)
{

}

void FVisualLoggerExtension::OnTimestampChange(float Timestamp, UWorld* InWorld, AActor* HelperActor )
{
#if USE_EQS_DEBUGGER
	if (CurrentTimestamp != Timestamp)
	{
		CurrentTimestamp = Timestamp;
		CachedEQSId = INDEX_NONE;
		DisableEQSRendering(HelperActor);
	}
#endif
}

void FVisualLoggerExtension::DisableDrawingForData(UWorld* InWorld, UCanvas* Canvas, AActor* HelperActor, const FName& TagName, const FVisualLogDataBlock& DataBlock, float Timestamp)
{
#if USE_EQS_DEBUGGER
	if (TagName == *EVisLogTags::TAG_EQS && CurrentTimestamp == Timestamp)
	{
		DisableEQSRendering(HelperActor);
	}
#endif
}

void FVisualLoggerExtension::DisableEQSRendering(AActor* HelperActor)
{
#if USE_EQS_DEBUGGER
	if (HelperActor)
	{
		SelectedEQSId = INDEX_NONE;
		UEQSRenderingComponent* EQSRenderComp = HelperActor->FindComponentByClass<UEQSRenderingComponent>();
		if (EQSRenderComp)
		{
			EQSRenderComp->SetHiddenInGame(true);
			EQSRenderComp->Deactivate();
			EQSRenderComp->DebugData.Reset();
			EQSRenderComp->MarkRenderStateDirty();
		}
	}
#endif
}

void FVisualLoggerExtension::LogEntryLineSelectionChanged(TSharedPtr<FLogEntryItem> SelectedItem, int64 UserData, FName TagName)
{
	if (TagName == *EVisLogTags::TAG_EQS)
	{
		SelectedEQSId = (int32)UserData;
	}
}

extern RENDERCORE_API FTexture* GWhiteTexture;

void FVisualLoggerExtension::DrawData(UWorld* InWorld, UCanvas* Canvas, AActor* HelperActor, const FName& TagName, const FVisualLogDataBlock& DataBlock, float Timestamp)
{
#if USE_EQS_DEBUGGER
	if (TagName == *EVisLogTags::TAG_EQS)
	{
		EQSDebug::FQueryData DebugData;
		UEnvQueryDebugHelpers::BlobArrayToDebugData(DataBlock.Data, DebugData, false);
		if (HelperActor)
		{
			UEQSRenderingComponent* EQSRenderComp = HelperActor->FindComponentByClass<UEQSRenderingComponent>();
			if (!EQSRenderComp)
			{
				EQSRenderComp = NewObject<UEQSRenderingComponent>(HelperActor);
				EQSRenderComp->bDrawOnlyWhenSelected = false;
				EQSRenderComp->RegisterComponent();
				EQSRenderComp->SetHiddenInGame(true);
			}

			if (SelectedEQSId == INDEX_NONE || SelectedEQSId != CachedEQSId /*|| (EQSRenderComp && EQSRenderComp->bHiddenInGame)*/)
			{
				if (SelectedEQSId == INDEX_NONE)
				{
					SelectedEQSId = DebugData.Id;
				}
				CachedEQSId = DebugData.Id;
				EQSRenderComp->DebugData = DebugData;
				EQSRenderComp->Activate();
				EQSRenderComp->SetHiddenInGame(false);
				EQSRenderComp->MarkRenderStateDirty();
			}
		}

		/** find and draw item selection */
		int32 BestItemIndex = INDEX_NONE;
		if (SelectedEQSId != INDEX_NONE && DebugData.Id == SelectedEQSId && Canvas)
		{
			FVector FireDir = Canvas->SceneView->GetViewDirection();
			FVector CamLocation = Canvas->SceneView->ViewMatrices.ViewOrigin;

			float bestAim = 0;
			for (int32 Index = 0; Index < DebugData.RenderDebugHelpers.Num(); ++Index)
			{
				auto& CurrentItem = DebugData.RenderDebugHelpers[Index];

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
				DrawDebugSphere(InWorld, DebugData.RenderDebugHelpers[BestItemIndex].Location, DebugData.RenderDebugHelpers[BestItemIndex].Radius, 8, FColor::Red, false);
				int32 FailedTestIndex = DebugData.RenderDebugHelpers[BestItemIndex].FailedTestIndex;
				if (FailedTestIndex != INDEX_NONE)
				{
					FString FailInfo = FString::Printf(TEXT("Selected item failed with test %d: %s (%s)\n'%s' with score %3.3f")
						, FailedTestIndex
						, *DebugData.Tests[FailedTestIndex].ShortName
						, *DebugData.Tests[FailedTestIndex].Detailed
						, *DebugData.RenderDebugHelpers[BestItemIndex].AdditionalInformation, DebugData.RenderDebugHelpers[BestItemIndex].FailedScore
						);
					float OutX, OutY;
					Canvas->StrLen(GEngine->GetSmallFont(), FailInfo, OutX, OutY);

					FCanvasTileItem TileItem(FVector2D(10, 10), GWhiteTexture, FVector2D(Canvas->SizeX, 2*OutY), FColor(0, 0, 0, 200));
					TileItem.BlendMode = SE_BLEND_Translucent;
					Canvas->DrawItem(TileItem, 0, Canvas->SizeY - 2*OutY);

					FCanvasTextItem TextItem(FVector2D::ZeroVector, FText::FromString(FailInfo), GEngine->GetSmallFont(), FLinearColor::White);
					TextItem.Depth = 1.1;
					TextItem.EnableShadow(FColor::Black, FVector2D(1, 1));
					Canvas->DrawItem(TextItem, 5, Canvas->SizeY - 2*OutY);

				}
			}
		}


	}
#endif
}
#endif //ENABLE_VISUAL_LOG
