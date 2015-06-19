// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "DebugRenderSceneProxy.h"
#include "EnvironmentQuery/EQSRenderingComponent.h"
#include "EnvironmentQuery/EQSQueryResultSourceInterface.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"

static const int32 EQSMaxItemsDrawn = 10000;

namespace FEQSRenderingHelper
{
	FVector ExtractLocation(TSubclassOf<UEnvQueryItemType> ItemType, const TArray<uint8>& RawData, const TArray<FEnvQueryItem>& Items, int32 Index)
	{
		if (Items.IsValidIndex(Index) &&
			ItemType->IsChildOf(UEnvQueryItemType_VectorBase::StaticClass()))
		{
			UEnvQueryItemType_VectorBase* DefTypeOb = (UEnvQueryItemType_VectorBase*)ItemType->GetDefaultObject();
			return DefTypeOb->GetItemLocation(RawData.GetData() + Items[Index].DataOffset);
		}
		return FVector::ZeroVector;
	}
}

//----------------------------------------------------------------------//
// FEQSSceneProxy
//----------------------------------------------------------------------//
const FVector FEQSSceneProxy::ItemDrawRadius(30,30,30);

FEQSSceneProxy::FEQSSceneProxy(const UPrimitiveComponent* InComponent, const FString& InViewFlagName) 
	: FDebugRenderSceneProxy(InComponent)
	, ActorOwner(NULL)
	, QueryDataSource(NULL)
{
	DrawType = SolidAndWireMeshes;
	TextWithoutShadowDistance = 1500;
	ViewFlagIndex = uint32(FEngineShowFlags::FindIndexByName(*InViewFlagName));
	ViewFlagName = InViewFlagName;

	if (InComponent == NULL)
	{
		return;
	}

	ActorOwner = InComponent ? InComponent->GetOwner() : NULL;

	const UEQSRenderingComponent* MyRenderComp = Cast<const UEQSRenderingComponent>(InComponent);
	bDrawOnlyWhenSelected = MyRenderComp && MyRenderComp->bDrawOnlyWhenSelected;

	QueryDataSource = Cast<const IEQSQueryResultSourceInterface>(ActorOwner);
	if (QueryDataSource == NULL)
	{
		QueryDataSource = Cast<const IEQSQueryResultSourceInterface>(InComponent);
		if (QueryDataSource == NULL)
		{
			return;
		}
	}

#if  USE_EQS_DEBUGGER 
	TArray<EQSDebug::FDebugHelper> DebugItems;
	FEQSSceneProxy::CollectEQSData(InComponent, QueryDataSource, Spheres, Texts, DebugItems);
#endif
}

FEQSSceneProxy::FEQSSceneProxy(const UPrimitiveComponent* InComponent, const FString& InViewFlagName, const TArray<FSphere>& InSpheres, const TArray<FText3d>& InTexts)
	: FDebugRenderSceneProxy(InComponent)
	, ActorOwner(NULL)
	, QueryDataSource(NULL)
{
	DrawType = SolidAndWireMeshes;
	TextWithoutShadowDistance = 1500;
	ViewFlagIndex = uint32(FEngineShowFlags::FindIndexByName(*InViewFlagName));
	ViewFlagName = InViewFlagName;

	Spheres = InSpheres;
	Texts = InTexts;

	if (InComponent == NULL)
	{
		return;
	}

	const UEQSRenderingComponent* MyRenderComp = Cast<const UEQSRenderingComponent>(InComponent);
	bDrawOnlyWhenSelected = MyRenderComp && MyRenderComp->bDrawOnlyWhenSelected;

	ActorOwner = InComponent ? InComponent->GetOwner() : NULL;
	QueryDataSource = Cast<const IEQSQueryResultSourceInterface>(ActorOwner);
	if (QueryDataSource == NULL)
	{
		QueryDataSource = Cast<const IEQSQueryResultSourceInterface>(InComponent);
		if (QueryDataSource == NULL)
		{
			return;
		}
	}
}

#if  USE_EQS_DEBUGGER 
void FEQSSceneProxy::CollectEQSData(const UPrimitiveComponent* InComponent, const IEQSQueryResultSourceInterface* InQueryDataSource, TArray<FSphere>& Spheres, TArray<FText3d>& Texts, TArray<EQSDebug::FDebugHelper>& DebugItems)
{
	AActor* ActorOwner = InComponent ? InComponent->GetOwner() : NULL;
	IEQSQueryResultSourceInterface* QueryDataSource = const_cast<IEQSQueryResultSourceInterface*>(InQueryDataSource);
	if (QueryDataSource == NULL)
	{
		QueryDataSource = Cast<IEQSQueryResultSourceInterface>(ActorOwner);
		if (QueryDataSource == NULL)
		{
			QueryDataSource = Cast<IEQSQueryResultSourceInterface>(const_cast<UPrimitiveComponent*>(InComponent));
			if (QueryDataSource == NULL)
			{
				return;
			}
		}
	}

	const FEnvQueryResult* ResultItems = QueryDataSource->GetQueryResult();
	const FEnvQueryInstance* QueryInstance = QueryDataSource->GetQueryInstance();

	FEQSSceneProxy::CollectEQSData(ResultItems, QueryInstance,
		QueryDataSource->GetHighlightRangePct(), QueryDataSource->GetShouldDrawFailedItems(),
		Spheres, Texts, DebugItems);

	if (ActorOwner && Spheres.Num() > EQSMaxItemsDrawn)
	{
		UE_VLOG(ActorOwner, LogEQS, Warning, TEXT("EQS drawing: too much items to draw! Drawing first %d from set of %d")
			, EQSMaxItemsDrawn
			, Spheres.Num()
			);
	}
}

void FEQSSceneProxy::CollectEQSData(const FEnvQueryResult* ResultItems, const FEnvQueryInstance* QueryInstance, float HighlightRangePct, bool ShouldDrawFailedItems, TArray<FSphere>& Spheres, TArray<FText3d>& Texts, TArray<EQSDebug::FDebugHelper>& DebugItems)
{
	if (ResultItems == NULL)
	{
		if (QueryInstance == NULL)
		{
			return;
		}
		else
		{
			ResultItems = QueryInstance;
		}
	}

	// using "mid-results" requires manual normalization
	const bool bUseMidResults = QueryInstance && (QueryInstance->Items.Num() < QueryInstance->DebugData.DebugItems.Num());
	// no point in checking if QueryInstance != null since bUseMidResults == true guarantees that, PVS-Studio doesn't
	// understand that invariant though, and we need to disable V595:
	const TArray<FEnvQueryItem>& Items = bUseMidResults ? QueryInstance->DebugData.DebugItems : ResultItems->Items; //-V595
	const TArray<uint8>& RawData = bUseMidResults ? QueryInstance->DebugData.RawData : ResultItems->RawData;
	const int32 ItemCountLimit = FMath::Clamp(Items.Num(), 0, EQSMaxItemsDrawn);
	const bool bNoTestsPerformed = QueryInstance != NULL && QueryInstance->CurrentTest <= 0;
	
	const bool bSingleItemResult = QueryInstance != NULL && QueryInstance->DebugData.bSingleItemResult;

	float MinScore = 0.f;
	float MaxScore = -BIG_NUMBER;
	if (bUseMidResults || HighlightRangePct < 1.0f)
	{
		const FEnvQueryItem* ItemInfo = Items.GetData();
		for (int32 ItemIndex = 0; ItemIndex < Items.Num(); ItemIndex++, ItemInfo++)
		{
			if (ItemInfo->IsValid())
			{
				MinScore = FMath::Min(MinScore, ItemInfo->Score);
				MaxScore = FMath::Max(MaxScore, ItemInfo->Score);
			}
		}
	}
	
	const float ScoreNormalizer = bUseMidResults && (MaxScore != MinScore) ? 1.f / (MaxScore - MinScore) : 1.f;
	const float HighlightThreshold = (HighlightRangePct < 1.0f) ? (MaxScore * HighlightRangePct) : FLT_MAX;

	if (bSingleItemResult == false)
	{
		for (int32 ItemIndex = 0; ItemIndex < ItemCountLimit; ++ItemIndex)
		{
			if (Items[ItemIndex].IsValid())
			{
				const float Score = bNoTestsPerformed ? 1 : Items[ItemIndex].Score * ScoreNormalizer;
				const bool bLowRadius = (HighlightThreshold < FLT_MAX) && (bNoTestsPerformed || (Items[ItemIndex].Score < HighlightThreshold));
				const float Radius = ItemDrawRadius.X * (bLowRadius ? 0.2f : 1.0f);
				const FVector Loc = FEQSRenderingHelper::ExtractLocation(ResultItems->ItemType, RawData, Items, ItemIndex);
				Spheres.Add(FSphere(Radius, Loc, bNoTestsPerformed == false
					? FLinearColor(FColor::MakeRedToGreenColorFromScalar(Score)) 
					: FLinearColor(0.2, 1.0, 1.0, 1)));

				DebugItems.Add(EQSDebug::FDebugHelper(Loc, Radius));

				const FString Label = bNoTestsPerformed ? TEXT("") : FString::Printf(TEXT("%.2f"), Score);
				Texts.Add(FText3d(Label, Loc, FLinearColor::White));
			}
		}
	}
	else if (ItemCountLimit > 0)
	{
		if (Items[0].IsValid())
		{
			const float Score = Items[0].Score * ScoreNormalizer;
			const bool bLowRadius = false;
			const float Radius = ItemDrawRadius.X * (bLowRadius ? 0.2f : 1.0f);
			const FVector Loc = FEQSRenderingHelper::ExtractLocation(ResultItems->ItemType, RawData, Items, 0);
			Spheres.Add(FSphere(Radius, Loc, FLinearColor(0.0, 1.0, 0.12, 1)));

			DebugItems.Add(EQSDebug::FDebugHelper(Loc, Radius));

			const FString Label = FString::Printf(TEXT("Winner %.2f"), Score);
			Texts.Add(FText3d(Label, Loc, FLinearColor::White));
		}

		for (int32 ItemIndex = 1; ItemIndex < ItemCountLimit; ++ItemIndex)
		{
			if (Items[ItemIndex].IsValid())
			{
				const float Score = bNoTestsPerformed ? 1 : Items[ItemIndex].Score * ScoreNormalizer;
				const bool bLowRadius = (HighlightThreshold < FLT_MAX) && (bNoTestsPerformed || (Items[ItemIndex].Score < HighlightThreshold));
				const float Radius = ItemDrawRadius.X * (bLowRadius ? 0.2f : 1.0f);
				const FVector Loc = FEQSRenderingHelper::ExtractLocation(ResultItems->ItemType, RawData, Items, ItemIndex);
				Spheres.Add(FSphere(Radius, Loc, FLinearColor(0.0, 0.2, 0.025, 1)));

				DebugItems.Add(EQSDebug::FDebugHelper(Loc, Radius));
				
				const FString Label = bNoTestsPerformed ? TEXT("") : FString::Printf(TEXT("%.2f"), Score);
				Texts.Add(FText3d(Label, Loc, FLinearColor::White));
			}
		}
	}

	if (ShouldDrawFailedItems && QueryInstance)
	{
		const FEQSQueryDebugData& InstanceDebugData = QueryInstance->DebugData;
		const TArray<FEnvQueryItem>& DebugQueryItems = InstanceDebugData.DebugItems;
		const TArray<FEnvQueryItemDetails>& Details = InstanceDebugData.DebugItemDetails;

		const int32 DebugItemCountLimit = DebugQueryItems.Num() == Details.Num() ? FMath::Clamp(DebugQueryItems.Num(), 0, EQSMaxItemsDrawn) : 0;

		for (int32 ItemIndex = 0; ItemIndex < DebugItemCountLimit; ++ItemIndex)
		{
			if (DebugQueryItems[ItemIndex].IsValid())
			{
				continue;
			}

			const float Score = bNoTestsPerformed ? 1 : Items[ItemIndex].Score * ScoreNormalizer;
			const bool bLowRadius = (HighlightThreshold < FLT_MAX) && (bNoTestsPerformed || (Items[ItemIndex].Score < HighlightThreshold));
			const float Radius = ItemDrawRadius.X * (bLowRadius ? 0.2f : 1.0f);
			const FVector Loc = FEQSRenderingHelper::ExtractLocation(QueryInstance->ItemType, InstanceDebugData.RawData, DebugQueryItems, ItemIndex);
			Spheres.Add(FSphere(Radius, Loc, FLinearColor(0.0, 0.0, 0.6, 0.6)));

			auto& DebugHelper = DebugItems[DebugItems.Add(EQSDebug::FDebugHelper(Loc, Radius))];
			DebugHelper.AdditionalInformation = Details[ItemIndex].FailedDescription;
			if (Details[ItemIndex].FailedTestIndex != INDEX_NONE)
			{
				DebugHelper.FailedTestIndex = Details[ItemIndex].FailedTestIndex;
				DebugHelper.FailedScore = Details[ItemIndex].TestResults[DebugHelper.FailedTestIndex];

				const FString Label = InstanceDebugData.PerformedTestNames[DebugHelper.FailedTestIndex] + FString::Printf(TEXT("(%d)"), DebugHelper.FailedTestIndex);
				Texts.Add(FText3d(Label, Loc, FLinearColor::White));
			}
		}
	}
}
#endif //USE_EQS_DEBUGGER 
void FEQSSceneProxy::DrawDebugLabels(UCanvas* Canvas, APlayerController* PC)
{
	if ( !ActorOwner
		 || (ActorOwner->IsSelected() == false && bDrawOnlyWhenSelected == true)
		 || (QueryDataSource && QueryDataSource->GetShouldDebugDrawLabels() == false))
	{
		return;
	}

	FDebugRenderSceneProxy::DrawDebugLabels(Canvas, PC);
}

bool FEQSSceneProxy::SafeIsActorSelected() const
{
	if(ActorOwner)
	{
		return ActorOwner->IsSelected();
	}

	return false;
}

FPrimitiveViewRelevance FEQSSceneProxy::GetViewRelevance(const FSceneView* View)
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = View->Family->EngineShowFlags.GetSingleFlag(ViewFlagIndex) && IsShown(View) 
		&& (!bDrawOnlyWhenSelected || SafeIsActorSelected());
	Result.bDynamicRelevance = true;
	Result.bNormalTranslucencyRelevance = IsShown(View);
	return Result;
}

//----------------------------------------------------------------------//
// UEQSRenderingComponent
//----------------------------------------------------------------------//
UEQSRenderingComponent::UEQSRenderingComponent(const FObjectInitializer& ObjectInitializer) 
	: Super(ObjectInitializer)
	, DrawFlagName("GameplayDebug")
	, bDrawOnlyWhenSelected(true)
{
}

FPrimitiveSceneProxy* UEQSRenderingComponent::CreateSceneProxy()
{
#if  USE_EQS_DEBUGGER || ENABLE_VISUAL_LOG
	if (DebugData.SolidSpheres.Num() > 0 || DebugData.Texts.Num() > 0)
	{
		return new FEQSSceneProxy(this, DrawFlagName, DebugData.SolidSpheres, DebugData.Texts);
	}
#endif
	return new FEQSSceneProxy(this, DrawFlagName);
}

FBoxSphereBounds UEQSRenderingComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	static FBox BoundingBox(FVector(-HALF_WORLD_MAX), FVector(HALF_WORLD_MAX));
	return FBoxSphereBounds(BoundingBox);
}

void UEQSRenderingComponent::CreateRenderState_Concurrent()
{
	Super::CreateRenderState_Concurrent();

#if USE_EQS_DEBUGGER
	if (SceneProxy)
	{
		static_cast<FEQSSceneProxy*>(SceneProxy)->RegisterDebugDrawDelgate();
	}
#endif
}

void UEQSRenderingComponent::DestroyRenderState_Concurrent()
{
#if USE_EQS_DEBUGGER
	if (SceneProxy)
	{
		static_cast<FEQSSceneProxy*>(SceneProxy)->UnregisterDebugDrawDelgate();
	}
#endif

	Super::DestroyRenderState_Concurrent();
}

//----------------------------------------------------------------------//
// UEQSQueryResultSourceInterface
//----------------------------------------------------------------------//
UEQSQueryResultSourceInterface::UEQSQueryResultSourceInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}
