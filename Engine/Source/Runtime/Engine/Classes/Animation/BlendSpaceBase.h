// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 * Blend Space Base. Contains base functionality shared across all blend space objects
 *
 */

#pragma once

#include "AnimSequence.h"
#include "BlendSpaceBase.generated.h"

// Interpolation data types.
UENUM()
enum EBlendSpaceAxis
{
	BSA_None, 
	BSA_X,
	BSA_Y, 
	BSA_Max
};

USTRUCT()
struct FInterpolationParameter
{
	GENERATED_USTRUCT_BODY()

	/** Interpolation Time for input, when it gets input, it will use this time to interpolate to target, used for smoother interpolation **/
	UPROPERTY(EditAnywhere, Category=Parameter)
	float InterpolationTime;

	/** Interpolation Type for input, when it gets input, it will use this filter to decide how to get to target **/
	UPROPERTY(EditAnywhere, Category=Parameter)
	TEnumAsByte<EFilterInterpolationType> InterpolationType;
};

USTRUCT()
struct FBlendParameter
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=BlendParameter)
	FString DisplayName;

	// min value for this parameter
	UPROPERTY(EditAnywhere, Category=BlendParameter)
	float Min;

	// max value for this parameter
	UPROPERTY(EditAnywhere, Category=BlendParameter)
	float Max;

	// how many grid for this parameter
	UPROPERTY(EditAnywhere, Category=BlendParameter)
	int32 GridNum;

	FBlendParameter()
		: DisplayName(TEXT("None"))
		, Min(0.f)
		, Max(100.f)
		, GridNum(4) // TODO when changing GridNum's default value, it breaks all grid samples ATM - provide way to rebuild grid samples during loading
	{
	}

	float GetRange() const
	{
		return Max-Min;
	}
	/** return size of each grid **/
	float GetGridSize() const
	{
		return GetRange()/(float)GridNum;
	}
	
};

/** Sample data **/
USTRUCT()
struct FBlendSample
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=BlendSample)
	class UAnimSequence* Animation;

	// blend 0->x, blend 1->y, blend 2->z
	UPROPERTY(EditAnywhere, Category=BlendSample)
	FVector SampleValue;

	bool bIsValid;

	FBlendSample()
		: Animation(NULL)
		, SampleValue(0.f)
		, bIsValid(false)
	{		
	}
	
	FBlendSample(class UAnimSequence* InAnim, FVector InValue, bool bInIsValid) 
		: Animation(InAnim)
		, SampleValue(InValue)
		, bIsValid(bInIsValid)
	{		
	}
	
	bool operator==( const FBlendSample& Other ) const 
	{
		return (Other.Animation == Animation && (Other.SampleValue-SampleValue).IsNearlyZero());
	}
};

/**
 * Each elements in the grid
 */
USTRUCT()
struct FEditorElement
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=EditorElement)
	int32 Indices[3];    /*MAX_VERTICES @fixmeconst*/

	UPROPERTY(EditAnywhere, Category=EditorElement)
	float Weights[3];    /*MAX_VERTICES @fixmeconst*/

	// for now we only support triangle
	static const int32 MAX_VERTICES = 3;

	FEditorElement()
	{
		for (int32 ElementIndex = 0; ElementIndex < MAX_VERTICES; ElementIndex++)
		{
			Indices[ElementIndex] = INDEX_NONE;
		}
		for (int32 ElementIndex = 0; ElementIndex < MAX_VERTICES; ElementIndex++)
		{
			Weights[ElementIndex] = 0;
		}
	}
	
};

/** result of how much weight of the grid element **/
USTRUCT()
struct FGridBlendSample
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	struct FEditorElement GridElement;

	UPROPERTY()
	float BlendWeight;


	FGridBlendSample()
		: BlendWeight(0)
	{
	}

};

USTRUCT()
struct FPerBoneInterpolation
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=FPerBoneInterpolation)
	FBoneReference BoneReference;

	UPROPERTY(EditAnywhere, Category=FPerBoneInterpolation)
	float InterpolationSpeedPerSec;

	FPerBoneInterpolation()
		: InterpolationSpeedPerSec(6.f)
	{}

	void Initialize(const USkeleton* Skeleton)
	{
		BoneReference.Initialize(Skeleton);
	}
};

UENUM()
namespace ENotifyTriggerMode
{
	enum Type
	{
		AllAnimations UMETA(DisplayName="All Animations"),
		HighestWeightedAnimation UMETA(DisplayName="Highest Weighted Animation"),
		None,
	};
}

/**
 * Allows multiple animations to be blended between based on input parameters
 */
UCLASS(config=Engine, hidecategories=Object, MinimalAPI, BlueprintType)
class UBlendSpaceBase : public UAnimationAsset
{
	GENERATED_UCLASS_BODY()

protected:
	/** Blend Parameters for each axis. **/
	UPROPERTY()
	struct FBlendParameter BlendParameters[3];

	/** Input interpolation parameter for all 3 axis, for each axis input, decide how you'd like to interpolate input to*/
	UPROPERTY(EditAnywhere, Category=InputInterpolation)
	FInterpolationParameter	InterpolationParam[3];

	/** 
	 * Target weight interpolation. When target samples are set, how fast you'd like to get to target. Improve target blending. 
	 * i.e. for locomotion, if you interpolate input, when you move from left to right rapidly, you'll interpolate through forward, but if you use target weight interpolation, 
	 * you'll skip forward, but interpolate between left to right 
	 */
	UPROPERTY(EditAnywhere, Category=SampleInterpolation)
	float TargetWeightInterpolationSpeedPerSec;

	/** The current mode used by the blendspace to decide which animation notifies to fire. Valid options are:
		- AllAnimations - All notify events will fire
		- HighestWeightedAnimation - Notify events will only fire from the highest weighted animation
		- None - No notify events will fire from any animations
	*/
	UPROPERTY(EditAnywhere, Category=AnimationNotifies)
	TEnumAsByte<ENotifyTriggerMode::Type> NotifyTriggerMode;
	
public:

	/** 
	 * When you use blend per bone, allows rotation to blend in mesh space. This only works if this does not contain additive animation samples
	 * This is more performance intensive
	 */
	UPROPERTY(EditAnywhere, Category=SampleInterpolation)
	bool bRotationBlendInMeshSpace;

	/** Number of dimensions for this blend space (1 or 2) **/
	UPROPERTY()
	int32 NumOfDimension;

#if WITH_EDITORONLY_DATA
	/** Preview Base pose for additive BlendSpace **/
	UPROPERTY(EditAnywhere, Category=AdditiveSettings)
	UAnimSequence* PreviewBasePose;
#endif // WITH_EDITORONLY_DATA

	/** This animation length changes based on current input (resulting in different blend time)**/
	UPROPERTY(transient)
	float AnimLength;

	/** 
	 * Define target weight interpolation per bone. This will blend in different speed per each bone setting 
	 */
	UPROPERTY(EditAnywhere, Category=SampleInterpolation)
	TArray<FPerBoneInterpolation> PerBoneBlend;

	// Begin UObject interface
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	// End UObject interface

	// Begin UAnimationAsset interface
	virtual void TickAssetPlayerInstance(const FAnimTickRecord& Instance, class UAnimInstance* InstanceOwner, FAnimAssetTickContext& Context) const override;
	// this is used in editor only when used for transition getter
	// this doesn't mean max time. In Sequence, this is SequenceLength,
	// but for BlendSpace CurrentTime is normalized [0,1], so this is 1
	virtual float GetMaxCurrentTime() override { return 1.f; }	
#if WITH_EDITOR
	virtual bool GetAllAnimationSequencesReferred(TArray<UAnimSequence*>& AnimationSequences) override;
	virtual void ReplaceReferredAnimations(const TMap<UAnimSequence*, UAnimSequence*>& ReplacementMap) override;
#endif
	// End of UAnimationAsset interface

	/** Accessor for blend parameter **/
	ENGINE_API const FBlendParameter& GetBlendParameter(int32 Index)
	{
		return BlendParameters[Index];
	}

	/** New Parameter set up for BlendParameters**/
	ENGINE_API bool UpdateParameter(int32 Index, const FBlendParameter& Parameter);

	/** Add samples */
	ENGINE_API bool	AddSample(const FBlendSample& BlendSample);

	/** edit samples */
	ENGINE_API bool	EditSample(const FBlendSample& BlendSample, FVector& NewValue);

	/** replace sample animation */
	ENGINE_API bool	EditSampleAnimation(const FBlendSample& BlendSample, class UAnimSequence* AnimSequence);

	/** delete samples */
	ENGINE_API bool	DeleteSample(const FBlendSample& BlendSample);

	/** whenever sample is modified, grid data isn't valid anymore, clear grid */
	ENGINE_API void ClearAllSamples();

	/** Get the number of sample points for this blend space */
	ENGINE_API int32 GetBlendSamplePointNum()  const { return SampleData.Num(); }

	/** Get this blend spaces sample data */
	const TArray<struct FBlendSample>& GetBlendSamples() const { return SampleData; } 

	/**
	 * return GridSamples from this BlendSpace
	 *
	 * @param	OutGridElements
	 *
	 * @return	Number of OutGridElements
	 */
	ENGINE_API int32 GetGridSamples(TArray<FEditorElement> & OutGridElements) const;

	/** 
	 * return SamplePoints from this BlendSpace
	 * 
	 * @param	OutPointList	
	 *
	 * @return	Number of OutPointList
	 */
	ENGINE_API int32 GetBlendSamplePoints(TArray<FBlendSample> & OutPointList) const;

	/** Fill up local GridElements from the grid elements that are created using the sorted points
	 *	This will map back to original index for result
	 * 
	 *  @param	SortedPointList		This is the pointlist that are used to create the given GridElements
	 *								This list contains subsets of the points it originally requested for visualization and sorted
	 *
	 */
	ENGINE_API void FillupGridElements(const TArray<FVector> & SortedPointList, const TArray<FEditorElement> & GridElements);

	/**
	 * Get Grid Samples from BlendInput
	 * It will return all samples that has weight > KINDA_SMALL_NUMBER
	 *
	 * @param	BlendInput	BlendInput X, Y, Z corresponds to BlendParameters[0], [1], [2]
	 * 
	 * @return	true if it has valid OutSampleDataList, false otherwise
	 */
	ENGINE_API bool GetSamplesFromBlendInput(const FVector &BlendInput, TArray<FBlendSampleData> & OutSampleDataList) const;
	
	/** Check if given sample value isn't too close to existing sample point **/
	ENGINE_API bool IsTooCloseToExistingSamplePoint(const FVector& SampleValue, int32 OriginalIndex) const;

	/** Initialize BlendSpace for runtime. It needs certain data to be reinitialized per instsance **/
	void InitializeFilter(FBlendFilter* Filter) const;

	/** 
	 * Get PerBoneInterpolationIndex for the input BoneIndex
	 * If nothing found, return INDEX_NONE
	 */
	int32 GetPerBoneInterpolationIndex(int32 BoneIndex, const FBoneContainer& RequiredBones) const;

	/** return true if all sample data is additive **/
	virtual bool IsValidAdditive() const {check(false); return false;}

protected:
	/** Initialize Per Bone Blend **/
	void InitializePerBoneBlend();

	/** Interpolate BlendInput based on Filter data **/
	FVector FilterInput(FBlendFilter* Filter, const FVector& BlendInput, float DeltaTime) const;

	/** Let derived blend space decided how to handle scaling */
	virtual EBlendSpaceAxis GetAxisToScale() const PURE_VIRTUAL(UBlendSpaceBase::GetAxisToScale, return BSA_None;);

	/** Validates supplied blend sample against current contents of blendspace */
	virtual bool ValidateSampleInput(FBlendSample & BlendSample, int32 OriginalIndex=INDEX_NONE) const;

	/** 1) Remove data if redundant 2) Remove data if animation isn't found **/
	void ValidateSampleData();

	/** Returns where blend space sample animations are valid for the supplied AdditiveType */
	bool IsValidAdditiveInternal(EAdditiveAnimationType AdditiveType) const;

	/** Utility function to normalize sample data weights **/
	void NormalizeSampleDataWeight(TArray<FBlendSampleData> & SampleDataList) const;

	/** Utility function to calculate animation length from sample data list **/
	float GetAnimationLengthFromSampleData(const TArray<FBlendSampleData> & SampleDataList) const;

	/** Returns the grid element at Index or NULL if Index is not valid */
	const FEditorElement* GetGridSampleInternal(int32 Index) const { return GridSamples.IsValidIndex(Index) ? &GridSamples[Index] : NULL; }

	virtual bool IsSameSamplePoint(const FVector& SamplePointA, const FVector& SamplePointB) const PURE_VIRTUAL(UBlendSpaceBase::IsSameSamplePoint, return false;);

	/** If around border, snap to the border to avoid empty hole of data that is not valid **/
	virtual void SnapToBorder(FBlendSample& Sample) const PURE_VIRTUAL(UBlendSpaceBase::SnapToBorder, return;);

	/** Clamp blend input to valid point **/
	FVector ClampBlendInput(const FVector& BlendInput) const;

	/** Translates BlendInput to grid space */
	FVector GetNormalizedBlendInput(const FVector& BlendInput) const;

	/** Utility function to interpolate weight of samples from OldSampleDataList to NewSampleDataList and copy back the interpolated result to FinalSampleDataList **/
	bool InterpolateWeightOfSampleData(float DeltaTime, const TArray<FBlendSampleData> & OldSampleDataList, const TArray<FBlendSampleData> & NewSampleDataList, TArray<FBlendSampleData> & FinalSampleDataList) const;

	/** 
	 * Get Grid Samples from BlendInput, From Input, it will populate OutGridSamples with the closest grid points. 
	 * 
	 * @param	BlendInput			BlendInput X, Y, Z corresponds to BlendParameters[0], [1], [2]
	 * @param	OutBlendSamples		Populated with the samples nearest the BlendInput 
	 *
	 */
	virtual void GetRawSamplesFromBlendInput(const FVector &BlendInput, TArray<FGridBlendSample> & OutBlendSamples) const {}

public:
	
	/** Sample animation data **/
	UPROPERTY()
	TArray<struct FBlendSample> SampleData;

	/** Grid samples, indexing scheme imposed by subclass **/
	UPROPERTY()
	TArray<struct FEditorElement> GridSamples;
};
