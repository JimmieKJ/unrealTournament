// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneFwd.h"
#include "MovieSceneCommonHelpers.h"
#include "Evaluation/MovieSceneAnimTypeID.h"
#include "MovieSceneExecutionToken.h"
#include "Evaluation/PersistentEvaluationData.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "MovieScenePropertyTemplate.generated.h"

DECLARE_CYCLE_STAT(TEXT("Property Track Token Execute"), MovieSceneEval_PropertyTrack_TokenExecute, STATGROUP_MovieSceneEval);

namespace PropertyTemplate
{
	/** Persistent section data for a property section */
	struct FSectionData : IPersistentEvaluationData
	{
		MOVIESCENE_API FSectionData();

		/** Initialize track data with the specified property name, path, and optional setter function */
		MOVIESCENE_API void Initialize(FName InPropertyName, FString InPropertyPath, FName InFunctionName = NAME_None);

		/** Property bindings used to get and set the property */
		TSharedPtr<FTrackInstancePropertyBindings> PropertyBindings;

		/** Cached identifier of the property we're editing */
		FMovieSceneAnimTypeID PropertyID;
	};

	/** The value of the object as it existed before this frame's evaluation */
	template<typename PropertyValueType>
	struct TCachedValue
	{
	/** The object */
		TWeakObjectPtr<UObject> WeakObject;

		/** The property value */
		PropertyValueType Value;
	};

	/** Typed persistent section data for a property section that caches object values at the start of the frame. Use in conjunction with TPropertyTrackExecutionToken. */
	template<typename PropertyValueType>
	struct TCachedSectionData : FSectionData
	{
		/** Setup the data for the current frame */
		void SetupFrame(const FMovieSceneEvaluationOperand& Operand, IMovieScenePlayer& Player)
		{
			ObjectsAndValues.Reset();
			
			for (TWeakObjectPtr<> Object : Player.FindBoundObjects(Operand))
			{
				if (UObject* ObjectPtr = Object.Get())
				{
					ObjectsAndValues.Add(TCachedValue<PropertyValueType>{ ObjectPtr, PropertyBindings->GetCurrentValue<PropertyValueType>(*ObjectPtr) });
				}
			}
		}

		TArray<TCachedValue<PropertyValueType>, TInlineAllocator<1>> ObjectsAndValues;
	};

	template<typename PropertyValueType, typename IntermediateType = PropertyValueType>
	struct TTemporarySetterType { typedef PropertyValueType Type; };

	/** Convert from an intermediate type to the type used for setting a property value. Called when resetting pre animated state */
	template<typename PropertyValueType, typename IntermediateType = PropertyValueType>
	typename TTemporarySetterType<PropertyValueType, IntermediateType>::Type
		ConvertFromIntermediateType(const IntermediateType& InIntermediateType, IMovieScenePlayer& Player)
	{
		return InIntermediateType;
	}

	/** Convert from an intermediate type to the type used for setting a property value. Called during token execution. */
	template<typename PropertyValueType, typename IntermediateType = PropertyValueType>
	typename TTemporarySetterType<PropertyValueType, IntermediateType>::Type
		ConvertFromIntermediateType(const IntermediateType& InIntermediateType, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player)
	{
		return InIntermediateType;
	}
	
	template<typename PropertyValueType, typename IntermediateType = PropertyValueType>
	IntermediateType ConvertToIntermediateType(PropertyValueType&& NewValue)
	{
		return MoveTemp(NewValue);
	}

	template<typename T>
	bool IsValueValid(const T& InValue)
	{
		return true;
	}

	/** Cached preanimated state for a given property */
	template<typename PropertyValueType, typename IntermediateType = PropertyValueType>
	struct TCachedState : IMovieScenePreAnimatedToken
	{
		TCachedState(typename TCallTraits<IntermediateType>::ParamType InValue, const FTrackInstancePropertyBindings& InBindings)
			: Value(MoveTemp(InValue))
			, Bindings(InBindings)
		{
		}

		virtual void RestoreState(UObject& Object, IMovieScenePlayer& Player) override
		{
			auto NewValue = PropertyTemplate::ConvertFromIntermediateType<PropertyValueType, IntermediateType>(Value, Player);
			if (IsValueValid(NewValue))
			{
				Bindings.CallFunction<PropertyValueType>(Object, NewValue);
			}
		}

		IntermediateType Value;
		FTrackInstancePropertyBindings Bindings;
	};

	template<typename PropertyValueType, typename IntermediateType = PropertyValueType>
	static IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object, FTrackInstancePropertyBindings& PropertyBindings)
	{
		return TCachedState<PropertyValueType, IntermediateType>(PropertyTemplate::ConvertToIntermediateType(PropertyBindings.GetCurrentValue<PropertyValueType>(Object)), PropertyBindings);
	}


	template<typename PropertyValueType>
	struct FTokenProducer : IMovieScenePreAnimatedTokenProducer
	{
		FTokenProducer(FTrackInstancePropertyBindings& InPropertyBindings) : PropertyBindings(InPropertyBindings) {}

		virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const
		{
			return PropertyTemplate::CacheExistingState<PropertyValueType>(Object, PropertyBindings);
		}

		FTrackInstancePropertyBindings& PropertyBindings;
	};
}

/** Execution token for a given property */
template<typename PropertyValueType>
struct TCachedPropertyTrackExecutionToken : IMovieSceneExecutionToken
{
	/** Execute this token, operating on all objects referenced by 'Operand' */
	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		using namespace PropertyTemplate;

		MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_PropertyTrack_TokenExecute)
		
		TCachedSectionData<PropertyValueType>& PropertyTrackData = PersistentData.GetSectionData<TCachedSectionData<PropertyValueType>>();
		FTrackInstancePropertyBindings* PropertyBindings = PropertyTrackData.PropertyBindings.Get();

		for (TCachedValue<PropertyValueType>& ObjectAndValue : PropertyTrackData.ObjectsAndValues)
		{
			if (UObject* ObjectPtr = ObjectAndValue.WeakObject.Get())
			{
				Player.SavePreAnimatedState(*ObjectPtr, PropertyTrackData.PropertyID, FTokenProducer<PropertyValueType>(*PropertyBindings));
				
				PropertyBindings->CallFunction<PropertyValueType>(*ObjectPtr, ObjectAndValue.Value);
			}
		}
	}
};

/** Execution token that simple stores a value, and sets it when executed */
template<typename PropertyValueType, typename IntermediateType = PropertyValueType>
struct TPropertyTrackExecutionToken : IMovieSceneExecutionToken
{
	TPropertyTrackExecutionToken(IntermediateType InValue)
		: Value(MoveTemp(InValue))
	{}

	/** Execute this token, operating on all objects referenced by 'Operand' */
	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		using namespace PropertyTemplate;

		MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_PropertyTrack_TokenExecute)
		
		FSectionData& PropertyTrackData = PersistentData.GetSectionData<FSectionData>();
		FTrackInstancePropertyBindings* PropertyBindings = PropertyTrackData.PropertyBindings.Get();

		auto NewValue = PropertyTemplate::ConvertFromIntermediateType<PropertyValueType, IntermediateType>(Value, Operand, PersistentData, Player);
		if (!IsValueValid(NewValue))
		{
			return;
		}

		for (TWeakObjectPtr<> WeakObject : Player.FindBoundObjects(Operand))
		{
			if (UObject* ObjectPtr = WeakObject.Get())
			{
				Player.SavePreAnimatedState(*ObjectPtr, PropertyTrackData.PropertyID, FTokenProducer<PropertyValueType>(*PropertyBindings));
				
				PropertyBindings->CallFunction<PropertyValueType>(*ObjectPtr, NewValue);
			}
		}
	}

	IntermediateType Value;
};

USTRUCT()
struct FMovieScenePropertySectionData
{
	GENERATED_BODY()
	
	FMovieScenePropertySectionData()
	{}

	FMovieScenePropertySectionData(FName InPropertyName, FString InPropertyPath, FName InFunctionName = NAME_None)
		: PropertyName(InPropertyName), PropertyPath(MoveTemp(InPropertyPath)), FunctionName(InFunctionName) 
	{
	}

	/** Helper function to create FSectionData for this property section */
	void SetupTrack(FPersistentEvaluationData& PersistentData) const
	{
		PersistentData.AddSectionData<PropertyTemplate::FSectionData>().Initialize(PropertyName, PropertyPath, FunctionName);
	}

	/** Helper function to create TCachedSectionData<T> for this property section */
	template<typename T>
	void SetupCachedTrack(FPersistentEvaluationData& PersistentData) const
	{
		typedef PropertyTemplate::TCachedSectionData<T> FSectionData;
		PersistentData.AddSectionData<FSectionData>().Initialize(PropertyName, PropertyPath, FunctionName);
	}

	/** Helper function to initialize the TCachedSectionData<T> for this property section for the current frame */
	template<typename T>
	void SetupCachedFrame(const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const
	{
		typedef PropertyTemplate::TCachedSectionData<T> FSectionData;
		PersistentData.GetSectionData<FSectionData>().SetupFrame(Operand, Player);
	}

private:

	UPROPERTY()
	FName PropertyName;

	UPROPERTY()
	FString PropertyPath;

	UPROPERTY()
	FName FunctionName;
};
