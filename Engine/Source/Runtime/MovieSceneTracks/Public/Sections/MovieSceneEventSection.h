// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "Curves/KeyHandle.h"
#include "MovieSceneSection.h"
#include "Curves/NameCurve.h"
#include "Curves/CurveInterface.h"
#include "UObject/StructOnScope.h"
#include "Serialization/MemoryReader.h"
#include "Engine/Engine.h"
#include "MovieSceneEventSection.generated.h"

struct EventData;

USTRUCT()
struct FMovieSceneEventParameters
{
	GENERATED_BODY()

	FMovieSceneEventParameters() {}

	/** Construction from a struct type */
	FMovieSceneEventParameters(UStruct& InStruct)
		: StructType(&InStruct)
	{
	}

	FMovieSceneEventParameters(const FMovieSceneEventParameters& RHS) = default;
	FMovieSceneEventParameters& operator=(const FMovieSceneEventParameters& RHS) = default;

#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS
	FMovieSceneEventParameters(FMovieSceneEventParameters&&) = default;
	FMovieSceneEventParameters& operator=(FMovieSceneEventParameters&&) = default;
#else
	FMovieSceneEventParameters(FMovieSceneEventParameters&& RHS)
	{
		*this = MoveTemp(RHS);
	}
	FMovieSceneEventParameters& operator=(FMovieSceneEventParameters&& RHS)
	{
		StructType = MoveTemp(RHS.StructType);
		StructBytes = MoveTemp(RHS.StructBytes);
		return *this;
	}
#endif

	void OverwriteWith(const TArray<uint8>& Bytes)
	{
		StructBytes = Bytes;
	}

	void GetInstance(FStructOnScope& OutStruct) const
	{
		UStruct* StructPtr = StructType.Get();
		OutStruct.Initialize(StructPtr);
		uint8* Memory = OutStruct.GetStructMemory();
		if (StructPtr && StructPtr->GetStructureSize() > 0 && StructBytes.Num())
		{
			FMemoryReader Reader(StructBytes);
			StructPtr->SerializeTaggedProperties(Reader, Memory, StructPtr, nullptr);
		}
	}

	UStruct* GetStructType() const
	{
		return StructType.Get();
	}

	void Reassign(UStruct* NewStruct)
	{
		StructType = NewStruct;

		if (!NewStruct)
		{
			StructBytes.Reset();
		}
	}

	bool Serialize(FArchive& Ar)
	{
		UStruct* StructTypePtr = StructType.Get();
		Ar << StructTypePtr;
		StructType = StructTypePtr;
		
		Ar << StructBytes;

		return true;
	}

	friend FArchive& operator<<(FArchive& Ar, FMovieSceneEventParameters& Payload)
	{
		Payload.Serialize(Ar);
		return Ar;
	}

private:

	TWeakObjectPtr<UStruct> StructType;
	TArray<uint8> StructBytes;
};

template<>
struct TStructOpsTypeTraits<FMovieSceneEventParameters> : public TStructOpsTypeTraitsBase
{
	enum 
	{
		WithCopy = true,
		WithSerializer = true
	};
};

USTRUCT()
struct FEventPayload
{
	GENERATED_BODY()

	FEventPayload() {}
	FEventPayload(FName InEventName) : EventName(InEventName) {}

	FEventPayload(const FEventPayload&) = default;
	FEventPayload& operator=(const FEventPayload&) = default;

#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS
	FEventPayload(FEventPayload&&) = default;
	FEventPayload& operator=(FEventPayload&&) = default;
#else
	FEventPayload(FEventPayload&& RHS)
		: EventName(MoveTemp(RHS.EventName))
		, Parameters(MoveTemp(RHS.Parameters))
	{
	}
	FEventPayload& operator=(FEventPayload&& RHS)
	{
		EventName = MoveTemp(RHS.EventName);
		Parameters = MoveTemp(RHS.Parameters);
		return *this;
	}
#endif

	/** The name of the event to trigger */
	UPROPERTY(EditAnywhere, Category=Event)
	FName EventName;

	/** The event parameters */
	UPROPERTY(EditAnywhere, Category=Event, meta=(ShowOnlyInnerProperties))
	FMovieSceneEventParameters Parameters;
};

/** A curve of events */
USTRUCT()
struct FMovieSceneEventSectionData
{
	GENERATED_BODY()
	
	FMovieSceneEventSectionData() = default;

	FMovieSceneEventSectionData(const FMovieSceneEventSectionData& RHS)
		: KeyTimes(RHS.KeyTimes)
		, KeyValues(RHS.KeyValues)
	{
	}

	FMovieSceneEventSectionData& operator=(const FMovieSceneEventSectionData& RHS)
	{
		KeyTimes = RHS.KeyTimes;
		KeyValues = RHS.KeyValues;
#if WITH_EDITORONLY_DATA
		KeyHandles.Reset();
#endif
		return *this;
	}

	/** Sorted array of key times */
	UPROPERTY()
	TArray<float> KeyTimes;

	/** Array of values that correspond to each key time */
	UPROPERTY()
	TArray<FEventPayload> KeyValues;

#if WITH_EDITORONLY_DATA
	/** Transient key handles */
	FKeyHandleLookupTable KeyHandles;
#endif
};

/**
 * Implements a section in movie scene event tracks.
 */
UCLASS(MinimalAPI)
class UMovieSceneEventSection
	: public UMovieSceneSection
{
	GENERATED_BODY()

	/** Default constructor. */
	UMovieSceneEventSection();

public:
	
	// ~UObject interface
	virtual void PostLoad() override;

	/**
	 * Get the section's event data.
	 *
	 * @return Event data.
	 */
	const FMovieSceneEventSectionData& GetEventData() const { return EventData; }
	
	TCurveInterface<FEventPayload, float> GetCurveInterface() { return CurveInterface.GetValue(); }

public:

	//~ UMovieSceneSection interface

	virtual void DilateSection(float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles) override;
	virtual void GetKeyHandles(TSet<FKeyHandle>& KeyHandles, TRange<float> TimeRange) const override;
	virtual void MoveSection(float DeltaPosition, TSet<FKeyHandle>& KeyHandles) override;
	virtual TOptional<float> GetKeyTime(FKeyHandle KeyHandle) const override;
	virtual void SetKeyTime(FKeyHandle KeyHandle, float Time) override;

private:

	UPROPERTY()
	FNameCurve Events_DEPRECATED;

	UPROPERTY()
	FMovieSceneEventSectionData EventData;

	TOptional<TCurveInterface<FEventPayload, float>> CurveInterface;
};
