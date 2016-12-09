// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieScenePropertyTemplates.h"
#include "Sections/MovieSceneBoolSection.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Sections/MovieSceneByteSection.h"
#include "Sections/MovieSceneEnumSection.h"
#include "Sections/MovieSceneIntegerSection.h"
#include "Sections/MovieSceneVectorSection.h"
#include "Sections/MovieSceneStringSection.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "MovieScene.h"

namespace
{
	FName SanitizeBoolPropertyName(FName InPropertyName)
	{
		FString PropertyVarName = InPropertyName.ToString();
		PropertyVarName.RemoveFromStart("b", ESearchCase::CaseSensitive);
		return FName(*PropertyVarName);
	}
}

FMovieSceneBoolPropertySectionTemplate::FMovieSceneBoolPropertySectionTemplate(const UMovieSceneBoolSection& Section, const UMovieScenePropertyTrack& Track)
	: PropertyData(SanitizeBoolPropertyName(Track.GetPropertyName()), Track.GetPropertyPath())
	, BoolCurve(Section.GetCurve())
{}

void FMovieSceneBoolPropertySectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	using namespace PropertyTemplate;

	for (TCachedValue<bool>& ObjectAndValue : PersistentData.GetSectionData<TCachedSectionData<bool>>().ObjectsAndValues)
	{
		ObjectAndValue.Value = !!BoolCurve.Evaluate(Context.GetTime(), ObjectAndValue.Value ? 1 : 0);
	}
	
	ExecutionTokens.Add(TCachedPropertyTrackExecutionToken<bool>());
}

FMovieSceneFloatPropertySectionTemplate::FMovieSceneFloatPropertySectionTemplate(const UMovieSceneFloatSection& Section, const UMovieScenePropertyTrack& Track)
	: PropertyData(Track.GetPropertyName(), Track.GetPropertyPath())
	, FloatCurve(Section.GetFloatCurve())
{}

void FMovieSceneFloatPropertySectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	using namespace PropertyTemplate;

	for (TCachedValue<float>& ObjectAndValue : PersistentData.GetSectionData<TCachedSectionData<float>>().ObjectsAndValues)
	{
		ObjectAndValue.Value = FloatCurve.Eval(Context.GetTime(), ObjectAndValue.Value);
	}
	
	ExecutionTokens.Add(TCachedPropertyTrackExecutionToken<float>());
}

FMovieSceneBytePropertySectionTemplate::FMovieSceneBytePropertySectionTemplate(const UMovieSceneByteSection& Section, const UMovieScenePropertyTrack& Track)
	: PropertyData(Track.GetPropertyName(), Track.GetPropertyPath())
	, ByteCurve(Section.GetCurve())
{}

void FMovieSceneBytePropertySectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	using namespace PropertyTemplate;

	for (TCachedValue<uint8>& ObjectAndValue : PersistentData.GetSectionData<TCachedSectionData<uint8>>().ObjectsAndValues)
	{
		ObjectAndValue.Value = ByteCurve.Evaluate(Context.GetTime(), ObjectAndValue.Value);
	}
	
	ExecutionTokens.Add(TCachedPropertyTrackExecutionToken<uint8>());
}

FMovieSceneEnumPropertySectionTemplate::FMovieSceneEnumPropertySectionTemplate(const UMovieSceneEnumSection& Section, const UMovieScenePropertyTrack& Track)
	: PropertyData(Track.GetPropertyName(), Track.GetPropertyPath())
	, EnumCurve(Section.GetCurve())
{}

void FMovieSceneEnumPropertySectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	using namespace PropertyTemplate;

	for (TCachedValue<int64>& ObjectAndValue : PersistentData.GetSectionData<TCachedSectionData<int64>>().ObjectsAndValues)
	{
		ObjectAndValue.Value = EnumCurve.Evaluate(Context.GetTime(), ObjectAndValue.Value);
	}
	
	ExecutionTokens.Add(TCachedPropertyTrackExecutionToken<int64>());
}

FMovieSceneIntegerPropertySectionTemplate::FMovieSceneIntegerPropertySectionTemplate(const UMovieSceneIntegerSection& Section, const UMovieScenePropertyTrack& Track)
	: PropertyData(Track.GetPropertyName(), Track.GetPropertyPath())
	, IntegerCurve(Section.GetCurve())
{}

void FMovieSceneIntegerPropertySectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	using namespace PropertyTemplate;

	for (TCachedValue<int32>& ObjectAndValue : PersistentData.GetSectionData<TCachedSectionData<int32>>().ObjectsAndValues)
	{
		ObjectAndValue.Value = IntegerCurve.Evaluate(Context.GetTime(), ObjectAndValue.Value);
	}
	
	ExecutionTokens.Add(TCachedPropertyTrackExecutionToken<int32>());
}

FMovieSceneStringPropertySectionTemplate::FMovieSceneStringPropertySectionTemplate(const UMovieSceneStringSection& Section, const UMovieScenePropertyTrack& Track)
	: PropertyData(Track.GetPropertyName(), Track.GetPropertyPath())
	, StringCurve(Section.GetStringCurve())
{}

void FMovieSceneStringPropertySectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	using namespace PropertyTemplate;

	for (TCachedValue<FString>& ObjectAndValue : PersistentData.GetSectionData<TCachedSectionData<FString>>().ObjectsAndValues)
	{
		ObjectAndValue.Value = StringCurve.Eval(Context.GetTime(), ObjectAndValue.Value);
	}
	
	ExecutionTokens.Add(TCachedPropertyTrackExecutionToken<FString>());
}

FMovieSceneVectorPropertySectionTemplate::FMovieSceneVectorPropertySectionTemplate(const UMovieSceneVectorSection& Section, const UMovieScenePropertyTrack& Track)
	: PropertyData(Track.GetPropertyName(), Track.GetPropertyPath())
	, NumChannelsUsed(Section.GetChannelsUsed())
{
	for (int32 Index = 0; Index < NumChannelsUsed; ++Index)
	{
		ComponentCurves[Index] = Section.GetCurve(Index);
	}
}

void FMovieSceneVectorPropertySectionTemplate::Setup(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const
{
	switch (NumChannelsUsed)
	{
	case 2:	PropertyData.SetupCachedTrack<FVector2D>(PersistentData);	break;
	case 3:	PropertyData.SetupCachedTrack<FVector>(PersistentData);	break;
	case 4:	PropertyData.SetupCachedTrack<FVector4>(PersistentData);	break;

	default:
		UE_LOG(LogMovieScene, Warning, TEXT("Invalid number of channels(%d) for vector track"), NumChannelsUsed );
		break;
	}
}

void FMovieSceneVectorPropertySectionTemplate::Initialize(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const
{
	switch (NumChannelsUsed)
	{
	case 2:	PropertyData.SetupCachedFrame<FVector2D>(Operand, PersistentData, Player);	break;
	case 3:	PropertyData.SetupCachedFrame<FVector>(Operand, PersistentData, Player);		break;
	case 4:	PropertyData.SetupCachedFrame<FVector4>(Operand, PersistentData, Player);		break;

	default:
		UE_LOG(LogMovieScene, Warning, TEXT("Invalid number of channels(%d) for vector track"), NumChannelsUsed );
		break;
	}
}

void FMovieSceneVectorPropertySectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	using namespace PropertyTemplate;

	float Time = Context.GetTime();

	switch (NumChannelsUsed)
	{
	case 2:
		for (auto& ObjectAndValue : PersistentData.GetSectionData<TCachedSectionData<FVector2D>>().ObjectsAndValues)
		{
			ObjectAndValue.Value = FVector2D(ComponentCurves[0].Eval(Time, ObjectAndValue.Value.X), ComponentCurves[1].Eval(Time, ObjectAndValue.Value.Y));
		}
		ExecutionTokens.Add(TCachedPropertyTrackExecutionToken<FVector2D>());
		break;

	case 3:
		for (auto& ObjectAndValue : PersistentData.GetSectionData<TCachedSectionData<FVector>>().ObjectsAndValues)
		{
			ObjectAndValue.Value = FVector(ComponentCurves[0].Eval(Time, ObjectAndValue.Value.X), ComponentCurves[1].Eval(Time, ObjectAndValue.Value.Y), ComponentCurves[2].Eval(Time, ObjectAndValue.Value.Z));
		}
		ExecutionTokens.Add(TCachedPropertyTrackExecutionToken<FVector>());
		break;

	case 4:
		for (auto& ObjectAndValue : PersistentData.GetSectionData<TCachedSectionData<FVector4>>().ObjectsAndValues)
		{
			ObjectAndValue.Value = FVector4(ComponentCurves[0].Eval(Time, ObjectAndValue.Value.X), ComponentCurves[1].Eval(Time, ObjectAndValue.Value.Y), ComponentCurves[2].Eval(Time, ObjectAndValue.Value.Z), ComponentCurves[3].Eval(Time, ObjectAndValue.Value.W));
		}
		ExecutionTokens.Add(TCachedPropertyTrackExecutionToken<FVector4>());
		break;

	default:
		UE_LOG(LogMovieScene, Warning, TEXT("Invalid number of channels(%d) for vector track"), NumChannelsUsed );
		break;
	}
}