// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "EnvironmentQuery/EnvQueryOption.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Item.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_ActorBase.h"
#include "EnvironmentQuery/EnvQueryTest.h"

#define LOCTEXT_NAMESPACE "EnvQueryGenerator"

UEnvQueryTest::UEnvQueryTest(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	TestPurpose = EEnvTestPurpose::FilterAndScore;
	Cost = EEnvTestCost::Low;
	FilterType = EEnvTestFilterType::Range;
	ScoringEquation = EEnvTestScoreEquation::Linear;
	ClampMinType = EEnvQueryTestClamping::None;
	ClampMaxType = EEnvQueryTestClamping::None;
	BoolValue.DefaultValue = true;
	ScoringFactor.DefaultValue = 1.0f;

	bWorkOnFloatValues = true;

	// keep deprecated properties initialized
	BoolFilter.Value = true;
	Weight.Value = 1.0f;
}

void UEnvQueryTest::NormalizeItemScores(FEnvQueryInstance& QueryInstance)
{
	if (!IsScoring())
	{
		return;
	}

	ScoringFactor.BindData(QueryInstance.Owner.Get(), QueryInstance.QueryID);
	float ScoringFactorValue = ScoringFactor.GetValue();

	float MinScore = 0;
	float MaxScore = -BIG_NUMBER;

	if (ClampMinType == EEnvQueryTestClamping::FilterThreshold)
	{
		FloatValueMin.BindData(QueryInstance.Owner.Get(), QueryInstance.QueryID);
		MinScore = FloatValueMin.GetValue();
	}
	else if (ClampMinType == EEnvQueryTestClamping::SpecifiedValue)
	{
		ScoreClampMin.BindData(QueryInstance.Owner.Get(), QueryInstance.QueryID);
		MinScore = ScoreClampMin.GetValue();
	}

	if (ClampMaxType == EEnvQueryTestClamping::FilterThreshold)
	{
		FloatValueMax.BindData(QueryInstance.Owner.Get(), QueryInstance.QueryID);
		MaxScore = FloatValueMax.GetValue();
	}
	else if (ClampMaxType == EEnvQueryTestClamping::SpecifiedValue)
	{
		ScoreClampMax.BindData(QueryInstance.Owner.Get(), QueryInstance.QueryID);
		MaxScore = ScoreClampMax.GetValue();
	}

	FEnvQueryItemDetails* DetailInfo = QueryInstance.ItemDetails.GetData();
	if ((ClampMinType == EEnvQueryTestClamping::None) ||
		(ClampMaxType == EEnvQueryTestClamping::None)
	   )
	{
		for (int32 ItemIndex = 0; ItemIndex < QueryInstance.Items.Num(); ItemIndex++, DetailInfo++)
		{
			if (!QueryInstance.Items[ItemIndex].IsValid())
			{
				continue;
			}

			float TestValue = DetailInfo->TestResults[QueryInstance.CurrentTest];
			if (TestValue != UEnvQueryTypes::SkippedItemValue)
			{
				if (ClampMinType == EEnvQueryTestClamping::None)
				{
					MinScore = FMath::Min(MinScore, TestValue);
				}

				if (ClampMaxType == EEnvQueryTestClamping::None)
				{
					MaxScore = FMath::Max(MaxScore, TestValue);
				}
			}
		}
	}

	DetailInfo = QueryInstance.ItemDetails.GetData();

	if (MinScore != MaxScore)
	{
		for (int32 ItemIndex = 0; ItemIndex < QueryInstance.ItemDetails.Num(); ItemIndex++, DetailInfo++)
		{
			if (QueryInstance.Items[ItemIndex].IsValid() == false)
			{
				continue;
			}

			float WeightedScore = 0.0f;

			float& TestValue = DetailInfo->TestResults[QueryInstance.CurrentTest];
			if (TestValue != UEnvQueryTypes::SkippedItemValue)
			{
				const float ClampedScore = FMath::Clamp(TestValue, MinScore, MaxScore);
				const float NormalizedScore = (ClampedScore - MinScore) / (MaxScore - MinScore);
				// TODO? Add an option to invert the normalized score before applying an equation.
 				const float NormalizedScoreForEquation = /*bMirrorNormalizedScore ? (1.0f - NormalizedScore) :*/ NormalizedScore;
				switch (ScoringEquation)
				{
					case EEnvTestScoreEquation::Linear:
						WeightedScore = ScoringFactorValue * NormalizedScoreForEquation;
						break;

					case EEnvTestScoreEquation::InverseLinear:
					{
						// For now, we're avoiding having a separate flag for flipping the direction of the curve
						// because we don't have usage cases yet and want to avoid too complex UI.  If we decide
						// to add that flag later, we'll need to remove this option, since it should just be "mirror
						// curve" plus "Linear".
						float InverseNormalizedScore = (1.0f - NormalizedScoreForEquation);
						WeightedScore = ScoringFactorValue * InverseNormalizedScore;
						break;
					}

					case EEnvTestScoreEquation::Square:
						WeightedScore = ScoringFactorValue * (NormalizedScoreForEquation * NormalizedScoreForEquation);
						break;

					case EEnvTestScoreEquation::Constant:
						// I know, it's not "constant".  It's "Constant, or zero".  The tooltip should explain that.
						WeightedScore = (NormalizedScoreForEquation > 0) ? ScoringFactorValue : 0.0f;
						break;
						
					default:
						break;
				}
			}
			else
			{
				TestValue = 0.0f;
				WeightedScore = 0.0f;
			}

#if USE_EQS_DEBUGGER
			DetailInfo->TestWeightedScores[QueryInstance.CurrentTest] = WeightedScore;
#endif
			QueryInstance.Items[ItemIndex].Score += WeightedScore;
		}
	}
}

bool UEnvQueryTest::IsContextPerItem(TSubclassOf<UEnvQueryContext> CheckContext) const
{
	return CheckContext == UEnvQueryContext_Item::StaticClass();
}

FVector UEnvQueryTest::GetItemLocation(FEnvQueryInstance& QueryInstance, int32 ItemIndex) const
{
	return QueryInstance.ItemTypeVectorCDO ?
		QueryInstance.ItemTypeVectorCDO->GetItemLocation(QueryInstance.RawData.GetData() + QueryInstance.Items[ItemIndex].DataOffset) :
		FVector::ZeroVector;
}

FRotator UEnvQueryTest::GetItemRotation(FEnvQueryInstance& QueryInstance, int32 ItemIndex) const
{
	return QueryInstance.ItemTypeVectorCDO ?
		QueryInstance.ItemTypeVectorCDO->GetItemRotation(QueryInstance.RawData.GetData() + QueryInstance.Items[ItemIndex].DataOffset) :
		FRotator::ZeroRotator;
}

AActor* UEnvQueryTest::GetItemActor(FEnvQueryInstance& QueryInstance, int32 ItemIndex) const
{
	return QueryInstance.ItemTypeActorCDO ?
		QueryInstance.ItemTypeActorCDO->GetActor(QueryInstance.RawData.GetData() + QueryInstance.Items[ItemIndex].DataOffset) :
		NULL;
}

FString UEnvQueryTest::GetDescriptionTitle() const
{
	return UEnvQueryTypes::GetShortTypeName(this).ToString();
}

FText UEnvQueryTest::GetDescriptionDetails() const
{
	return FText::GetEmpty();
}

void UEnvQueryTest::PostLoad()
{
	Super::PostLoad();

	if (VerNum < EnvQueryTestVersion::DataProviders)
	{
		BoolFilter.Convert(this, BoolValue);
		FloatFilterMin.Convert(this, FloatValueMin);
		FloatFilterMax.Convert(this, FloatValueMax);
		ScoreClampingMin.Convert(this, ScoreClampMin);
		ScoreClampingMax.Convert(this, ScoreClampMax);
		Weight.Convert(this, ScoringFactor);
	}

	UpdateTestVersion();
}

void UEnvQueryTest::UpdateTestVersion()
{
	VerNum = EnvQueryTestVersion::Latest;
}

#if WITH_EDITOR && USE_EQS_DEBUGGER
void UEnvQueryTest::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) 
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
#if USE_EQS_DEBUGGER
	UEnvQueryManager::NotifyAssetUpdate(NULL);
#endif
}
#endif //WITH_EDITOR && USE_EQS_DEBUGGER

FText UEnvQueryTest::DescribeFloatTestParams() const
{
	FText FilterDesc;
	if (IsFiltering())
	{
		switch (FilterType)
		{
		case EEnvTestFilterType::Minimum:
			FilterDesc = FText::Format(LOCTEXT("FilterAtLeast", "at least {0}"), FText::FromString(FloatValueMin.ToString()));
			break;

		case EEnvTestFilterType::Maximum:
			FilterDesc = FText::Format(LOCTEXT("FilterUpTo", "up to {0}"), FText::FromString(FloatValueMax.ToString()));
			break;

		case EEnvTestFilterType::Range:
			FilterDesc = FText::Format(LOCTEXT("FilterBetween", "between {0} and {1}"),
				FText::FromString(FloatValueMin.ToString()), FText::FromString(FloatValueMax.ToString()));
			break;

		default:
			break;
		}
	}

	FNumberFormattingOptions NumberFormattingOptions;
	NumberFormattingOptions.MaximumFractionalDigits = 2;

	FText ScoreDesc;
	if (!IsScoring())
	{
		ScoreDesc = LOCTEXT("DontScore", "don't score");
	}
	else if (ScoringEquation == EEnvTestScoreEquation::Constant)
	{
		FText FactorDesc = ScoringFactor.IsDynamic() ?
			FText::FromString(ScoringFactor.ToString()) :
			FText::Format(FText::FromString("x{0}"), FText::AsNumber(FMath::Abs(ScoringFactor.DefaultValue), &NumberFormattingOptions));

		ScoreDesc = FText::Format(FText::FromString("{0} [{1}]"), LOCTEXT("ScoreConstant", "constant score"), FactorDesc);
	}
	else if (ScoringFactor.IsDynamic())
	{
		ScoreDesc = FText::Format(FText::FromString("{0}: {1}"), LOCTEXT("ScoreFactor", "score factor"), FText::FromString(ScoringFactor.ToString()));
	}
	else
	{
		FText ScoreSignDesc = (ScoringFactor.DefaultValue > 0) ? LOCTEXT("Greater", "greater") : LOCTEXT("Lesser", "lesser");
		FText ScoreValueDesc = FText::AsNumber(FMath::Abs(ScoringFactor.DefaultValue), &NumberFormattingOptions);
		ScoreDesc = FText::Format(FText::FromString("{0} {1} [x{2}]"), LOCTEXT("ScorePrefer", "prefer"), ScoreSignDesc, ScoreValueDesc);
	}

	return FilterDesc.IsEmpty() ? ScoreDesc : FText::Format(FText::FromString("{0}, {1}"), FilterDesc, ScoreDesc);
}

FText UEnvQueryTest::DescribeBoolTestParams(const FString& ConditionDesc) const
{
	FText FilterDesc;
	if (IsFiltering() && FilterType == EEnvTestFilterType::Match)
	{
		FilterDesc = BoolValue.IsDynamic() ?
			FText::Format(FText::FromString("{0} {1}: {2}"), LOCTEXT("FilterRequire", "require"), FText::FromString(ConditionDesc), FText::FromString(BoolValue.ToString())) :
			FText::Format(FText::FromString("{0} {1}{2}"), LOCTEXT("FilterRequire", "require"), BoolValue.DefaultValue ? FText::GetEmpty() : LOCTEXT("NotWithSpace", "not "), FText::FromString(ConditionDesc));
	}

	FNumberFormattingOptions NumberFormattingOptions;
	NumberFormattingOptions.MaximumFractionalDigits = 2;

	FText ScoreDesc;
	if (!IsScoring())
	{
		ScoreDesc = LOCTEXT("DontScore", "don't score");
	}
	else if (ScoringEquation == EEnvTestScoreEquation::Constant)
	{
		FText FactorDesc = ScoringFactor.IsDynamic() ?
			FText::FromString(ScoringFactor.ToString()) :
			FText::Format(FText::FromString("x{0}"), FText::AsNumber(FMath::Abs(ScoringFactor.DefaultValue), &NumberFormattingOptions));

		ScoreDesc = FText::Format(FText::FromString("{0} [{1}]"), LOCTEXT("ScoreConstant", "constant score"), FactorDesc);
	}
	else if (ScoringFactor.IsDynamic())
	{
		ScoreDesc = FText::Format(FText::FromString("{0}: {1}"), LOCTEXT("ScoreFactor", "score factor"), FText::FromString(ScoringFactor.ToString()));
	}
	else
	{
		FText ScoreSignDesc = (ScoringFactor.DefaultValue > 0) ? FText::GetEmpty() : LOCTEXT("NotWithSpace", "not ");
		FText ScoreValueDesc = FText::AsNumber(FMath::Abs(ScoringFactor.DefaultValue), &NumberFormattingOptions);
		ScoreDesc = FText::Format(FText::FromString("{0} {1}{2} [x{3}]"), LOCTEXT("ScorePrefer", "prefer"), ScoreSignDesc, FText::FromString(ConditionDesc), ScoreValueDesc);
	}

	return FilterDesc.IsEmpty() ? ScoreDesc : FText::Format(FText::FromString("{0}, {1}"), FilterDesc, ScoreDesc);
}

void UEnvQueryTest::SetWorkOnFloatValues(bool bWorkOnFloats)
{
	bWorkOnFloatValues = bWorkOnFloats;
	// Make sure FilterType is set to a valid value.
	if (bWorkOnFloats)
	{
		if (FilterType == EEnvTestFilterType::Match)
		{
			FilterType = EEnvTestFilterType::Range;
		}
	}
	else
	{
		if (FilterType != EEnvTestFilterType::Match)
		{
			FilterType = EEnvTestFilterType::Match;
		}

		// Scoring MUST be Constant for boolean tests.
		ScoringEquation = EEnvTestScoreEquation::Constant;
	}
}

#undef LOCTEXT_NAMESPACE
