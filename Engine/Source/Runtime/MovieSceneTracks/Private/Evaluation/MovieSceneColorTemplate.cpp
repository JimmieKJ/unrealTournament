// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneColorTemplate.h"
#include "Sections/MovieSceneColorSection.h"
#include "Tracks/MovieSceneColorTrack.h"
#include "UObject/UnrealType.h"
#include "MovieSceneCommonHelpers.h"
#include "Components/LightComponent.h"
#include "Styling/SlateColor.h"

DECLARE_CYCLE_STAT(TEXT("Color Track Token Execute"), MovieSceneEval_ColorTrack_TokenExecute, STATGROUP_MovieSceneEval);

enum class EColorType : uint8
{
	/** FSlateColor */
	Slate, 
	/** FLinearColor */
	Linear,
	/** FColor */
	Color,
};

struct FColorSectionData : IPersistentEvaluationData
{
	TSharedPtr<FTrackInstancePropertyBindings> PropertyBindings;
	FLinearColor DefaultColor;
};

struct FColorToken
{
	FColorToken() {}
	FColorToken(FLinearColor InColorValue, EColorType InType) : ColorValue(InColorValue), Type(InType) {}

	void Apply(UObject& Object, FTrackInstancePropertyBindings& Bindings)
	{
		switch(Type)
		{
		case EColorType::Slate:		ApplySlateColor(Object, Bindings);		break;
		case EColorType::Linear: 	ApplyLinearColor(Object, Bindings);		break;
		case EColorType::Color: 	ApplyColor(Object, Bindings);			break;
		}
	}

	void ApplyColor(UObject& Object, FTrackInstancePropertyBindings& Bindings)
	{
		const bool bConvertBackToSRgb = true;
		if (ULightComponent* LightComponent = Cast<ULightComponent>(&Object))
		{
			// Light components have to be handled specially here because their set function takes two values, the linear color
			// and whether or not the linear color needs to be converted back to sRGB.  All other other set function cases should
			// follow the sequencer convention of having a single parameter of the correct type, which in this case is an FColor
			// already in sRGB format.
			LightComponent->SetLightColor( ColorValue, bConvertBackToSRgb );
		}
		else
		{
			FColor SRgbColorValue = ColorValue.ToFColor( bConvertBackToSRgb );
			Bindings.CallFunction<FColor>( Object, SRgbColorValue );
		}
	}

	void ApplySlateColor(UObject& Object, FTrackInstancePropertyBindings& Bindings)
	{
		FSlateColor NewColor(ColorValue);
		Bindings.CallFunction<FSlateColor>(Object, NewColor);
	}

	void ApplyLinearColor(UObject& Object, FTrackInstancePropertyBindings& Bindings)
	{
		Bindings.CallFunction<FLinearColor>(Object, ColorValue);
	}

	void DeduceColorType(const UObject& InObject, FTrackInstancePropertyBindings& Bindings)
	{
		const UStructProperty* StructProp = Cast<const UStructProperty>(Bindings.GetProperty(InObject));
		if (StructProp && StructProp->Struct)
		{
			FName StructName = StructProp->Struct->GetFName();

			static const FName SlateColor("SlateColor");
			if( StructName == NAME_Color )
			{
				Type = EColorType::Color;
			}
			else if( StructName == SlateColor )
			{
				Type = EColorType::Slate;
			}
			else
			{
				Type = EColorType::Linear;
			}
		}
	}

	void AssignFromCurrentValue(const UObject& InObject, FTrackInstancePropertyBindings& Bindings)
	{
		switch (Type)
		{
		case EColorType::Color:
			// We assume the color we get back is in sRGB, assigning it to a linear color will implicitly
			// convert it to a linear color instead of using ReinterpretAsLinear which will just change the
			// bytes into floats using divide by 255.
			ColorValue = Bindings.GetCurrentValue<FColor>(InObject);
			break;

		case EColorType::Slate:
			ColorValue = Bindings.GetCurrentValue<FSlateColor>(InObject).GetSpecifiedColor();
			break;

		case EColorType::Linear:
			ColorValue = Bindings.GetCurrentValue<FLinearColor>(InObject);
			break;
		}
	}

	FLinearColor ColorValue;
	EColorType Type;
};

struct FColorTrackPreAnimatedState : IMovieScenePreAnimatedToken
{
	FColorTrackPreAnimatedState(FColorToken InToken, const FTrackInstancePropertyBindings& InBindings)
		: Token(InToken)
		, Bindings(InBindings)
	{
	}

	virtual void RestoreState(UObject& Object, IMovieScenePlayer& Player) override
	{
		Token.Apply(Object, Bindings);
	}

	FColorToken Token;
	FTrackInstancePropertyBindings Bindings;
};

struct FColorTokenProducer : IMovieScenePreAnimatedTokenProducer
{
	FColorTokenProducer(const FColorToken& InSourceToken, FTrackInstancePropertyBindings& InPropertyBindings)
		: SourceToken(InSourceToken), PropertyBindings(InPropertyBindings)
	{}

	virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const
	{
		FColorToken TokenCopy = SourceToken;
		TokenCopy.AssignFromCurrentValue(Object, PropertyBindings);
		return FColorTrackPreAnimatedState(TokenCopy, PropertyBindings);
	}

	const FColorToken& SourceToken;
	FTrackInstancePropertyBindings& PropertyBindings;
};

struct FColorTrackExecutionToken : IMovieSceneExecutionToken
{
	FColorTrackExecutionToken(FLinearColor InColor)
		: Token(InColor, EColorType::Linear)	// Just initialize to linear color for now, we'll deduce the type later
	{
	}

	/** Execute this token, operating on all objects referenced by 'Operand' */
	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& SectionData, IMovieScenePlayer& Player) override
	{
		MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_ColorTrack_TokenExecute)
		
		FTrackInstancePropertyBindings* PropertyBindings = SectionData.GetSectionData<FColorSectionData>().PropertyBindings.Get();

		for (TWeakObjectPtr<> Object : Player.FindBoundObjects(Operand))
		{
			if (UObject* ObjectPtr = Object.Get())
			{
				// Deduce the color type before we save pre animated state (so we store the right type)
				Token.DeduceColorType(*ObjectPtr, *PropertyBindings);

				FMovieSceneAnimTypeID AnimType = TMovieSceneAnimTypeID<FColorTrackExecutionToken>();
				Player.SavePreAnimatedState(*ObjectPtr, AnimType, FColorTokenProducer(Token, *PropertyBindings));

				Token.Apply(*ObjectPtr, *PropertyBindings);
			}
		}
	}

	FColorToken Token;
};

FMovieSceneColorSectionTemplate::FMovieSceneColorSectionTemplate(const UMovieSceneColorSection& Section, const UMovieSceneColorTrack& Track)
	: PropertyName(Track.GetPropertyName())
	, PropertyPath(Track.GetPropertyPath())
{
	Curves[0] = Section.GetRedCurve();
	Curves[1] = Section.GetGreenCurve();
	Curves[2] = Section.GetBlueCurve();
	Curves[3] = Section.GetAlphaCurve();
}

void FMovieSceneColorSectionTemplate::Setup(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const
{
	PersistentData.AddSectionData<FColorSectionData>().PropertyBindings = MakeShareable(new FTrackInstancePropertyBindings(PropertyName, PropertyPath));
}

void FMovieSceneColorSectionTemplate::Initialize(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const
{
	FColorSectionData& ColorSectionData = PersistentData.GetSectionData<FColorSectionData>();

	// Cache off the default color from the first object.
	// To support multiple relative animations, we'd need to animate each color relatively
	for (TWeakObjectPtr<> Object : Player.FindBoundObjects(Operand))
	{
		if (UObject* ObjectPtr = Object.Get())
		{
			FColorToken TempToken;
			TempToken.DeduceColorType(*ObjectPtr, *ColorSectionData.PropertyBindings);
			TempToken.AssignFromCurrentValue(*ObjectPtr, *ColorSectionData.PropertyBindings);
			ColorSectionData.DefaultColor = TempToken.ColorValue;
			break;
		}
	}

	
}

void FMovieSceneColorSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	float Time = Context.GetTime();
	FLinearColor DefaultColor = PersistentData.GetSectionData<FColorSectionData>().DefaultColor;

	ExecutionTokens.Add(
		FColorTrackExecutionToken(
			FLinearColor(
				Curves[0].Eval(Time, DefaultColor.R),
				Curves[1].Eval(Time, DefaultColor.G),
				Curves[2].Eval(Time, DefaultColor.B),
				Curves[3].Eval(Time, DefaultColor.A)
			)
		)
	);
}
