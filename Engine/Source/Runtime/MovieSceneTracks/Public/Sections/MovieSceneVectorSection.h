// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Curves/RichCurve.h"
#include "IKeyframeSection.h"
#include "MovieSceneKeyStruct.h"
#include "MovieSceneSection.h"
#include "MovieSceneVectorSection.generated.h"


enum class EKeyVectorChannel
{
	X,
	Y,
	Z,
	W
};


struct MOVIESCENETRACKS_API FVectorKey
{
	FVectorKey(EKeyVectorChannel InChannel, float InValue)
	{
		Channel = InChannel;
		Value = InValue;
	}
	EKeyVectorChannel Channel;
	float Value;
};


/**
 * Proxy structure for vector section key data.
 */
USTRUCT()
struct FMovieSceneVectorKeyStruct
	: public FMovieSceneKeyStruct
{
	GENERATED_BODY()

	/** They key's vector value. */
	UPROPERTY(EditAnywhere, Category=Key)
	FVector4 Vector;

	FRichCurveKey* Keys[4];

	virtual void PropagateChanges(const FPropertyChangedEvent& ChangeEvent) override;
};


/**
 * A vector section.
 */
UCLASS(MinimalAPI)
class UMovieSceneVectorSection
	: public UMovieSceneSection
	, public IKeyframeSection<FVectorKey>
{
	GENERATED_UCLASS_BODY()

public:
	
	/**
	 * Updates this section
	 *
	 * @param Position	The position in time within the movie scene
	 */
	FVector4 Eval(float Position, const FVector4& DefaultVector) const;

	/** Gets one of four curves in this section */
	FRichCurve& GetCurve(const int32& Index) { return Curves[Index]; }

	/** Sets how many channels are to be used */
	void SetChannelsUsed(int32 InChannelsUsed) 
	{
		checkf(InChannelsUsed >= 2 && InChannelsUsed <= 4, TEXT("Only 2-4 channels are supported."));
		ChannelsUsed = InChannelsUsed;
	}
	
	/** Gets the number of channels in use */
	int32 GetChannelsUsed() const {return ChannelsUsed;}

public:

	//~ UMovieSceneSection interface

	virtual void MoveSection(float DeltaPosition, TSet<FKeyHandle>& KeyHandles) override;
	virtual void DilateSection(float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles) override;
	virtual void GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const override;
	virtual TSharedPtr<FStructOnScope> GetKeyStruct(const TArray<FKeyHandle>& KeyHandles) override;
	virtual TOptional<float> GetKeyTime( FKeyHandle KeyHandle ) const override;
	virtual void SetKeyTime( FKeyHandle KeyHandle, float Time ) override;

public:

	//~ IKeyframeSection interface.

	virtual void AddKey(float Time, const FVectorKey& Key, EMovieSceneKeyInterpolation KeyInterpolation) override;
	virtual bool NewKeyIsNewData(float Time, const FVectorKey& Key) const override;
	virtual bool HasKeys(const FVectorKey& Key) const override;
	virtual void SetDefault(const FVectorKey& Key) override;

private:

	/** Vector t */
	UPROPERTY()
	FRichCurve Curves[4];

	/** How many curves are actually used */
	UPROPERTY()
	int32 ChannelsUsed;
};
