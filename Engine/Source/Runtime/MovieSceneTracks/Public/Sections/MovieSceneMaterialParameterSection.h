// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneFloatSection.h"
#include "MovieSceneMaterialParameterSection.generated.h"

/**
 * Structure representing an animated scalar parameter and it's associated animation curve.
 */
USTRUCT()
struct FScalarParameterNameAndCurve
{
	GENERATED_USTRUCT_BODY()

	FScalarParameterNameAndCurve()
	{
		Index = 0;
	}

	/** Creates a new FScalarParameterNameAndCurve for a specific scalar parameter. */
	FScalarParameterNameAndCurve( FName InParameterName );

	/** The name of the scalar parameter which is being animated. */
	UPROPERTY()
	FName ParameterName;

	UPROPERTY()
	int32 Index;

	/** The curve which contains the animation data for the scalar parameter. */
	UPROPERTY()
	FRichCurve ParameterCurve;
};

/**
* Structure representing an animated vector parameter and it's associated animation curve.
*/
USTRUCT()
struct FVectorParameterNameAndCurves
{
	GENERATED_USTRUCT_BODY()

	FVectorParameterNameAndCurves() 
	{
		Index = 0;
	}

	/** Creates a new FVectorParameterNameAndCurve for a specific vector parameter. */
	FVectorParameterNameAndCurves( FName InParameterName );

	/** The name of the vector parameter which is being animated. */
	UPROPERTY()
	FName ParameterName;

	UPROPERTY()
	int32 Index;

	/** The curve which contains the animation data for the red component of the vector parameter. */
	UPROPERTY()
	FRichCurve RedCurve;

	/** The curve which contains the animation data for the green component of the vector parameter. */
	UPROPERTY()
	FRichCurve GreenCurve;

	/** The curve which contains the animation data for the blue component of the vector parameter. */
	UPROPERTY()
	FRichCurve BlueCurve;

	/** The curve which contains the animation data for the alpha component of the vector parameter. */
	UPROPERTY()
	FRichCurve AlphaCurve;
};

/**
 * A movie scene section representing animation data for material parameters. 
 */
UCLASS( MinimalAPI )
class UMovieSceneMaterialParameterSection : public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()

public:
	/** Adds a a key for a specific scalar parameter at the specified time with the specified value. */
	void AddScalarParameterKey(FName InParameterName, float InTime, float InValue);

	/** Adds a a key for a specific vector parameter at the specified time with the specified value. */
	void AddVectorParameterKey( FName InParameterName, float InTime, FLinearColor InValue );

	/** 
	 * Removes a scalar parameter from this section. 
	 * 
	 * @param InParameterName The name of the scalar parameter to remove.
	 * @returns True if a parameter with that name was found and removed, otherwise false.
	 */
	MOVIESCENETRACKS_API bool RemoveScalarParameter( FName InParameterName );

	/**
	* Removes a vector parameter from this section.
	*
	* @param InParameterName The name of the vector parameter to remove.
	* @returns True if a parameter with that name was found and removed, otherwise false.
	*/
	MOVIESCENETRACKS_API bool RemoveVectorParameter( FName InParameterName );

	/** Gets the animated scalar parameters and their associated curves. */
	MOVIESCENETRACKS_API TArray<FScalarParameterNameAndCurve>* GetScalarParameterNamesAndCurves();

	/** Gets the animated scalar parameters and their associated curves. */
	MOVIESCENETRACKS_API TArray<FVectorParameterNameAndCurves>* GetVectorParameterNamesAndCurves();

private:
	void UpdateParameterIndicesFromRemoval(int32 RemovedIndex);

private:
	/**
	 * The scalar parameter names and their associated curves.
	 */
	UPROPERTY()
	TArray<FScalarParameterNameAndCurve> ScalarParameterNamesAndCurves;

	/**
	* The vector parameter names and their associated curves.
	*/
	UPROPERTY()
	TArray<FVectorParameterNameAndCurves> VectorParameterNamesAndCurves;
};