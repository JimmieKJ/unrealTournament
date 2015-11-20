// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * One animation sequence of keyframes. Contains a number of tracks of data.
 *
 */

#include "AnimSequenceBase.h"
#include "AnimSequence.generated.h"

typedef TArray<FTransform> FTransformArrayA2;

class USkeletalMesh;
struct FCompactPose;


/**
 * Indicates animation data compression format.
 */
UENUM()
enum AnimationCompressionFormat
{
	ACF_None,
	ACF_Float96NoW,
	ACF_Fixed48NoW,
	ACF_IntervalFixed32NoW,
	ACF_Fixed32NoW,
	ACF_Float32NoW,
	ACF_Identity,
	ACF_MAX UMETA(Hidden),
};

/**
 * Indicates animation data key format.
 */
UENUM()
enum AnimationKeyFormat
{
	AKF_ConstantKeyLerp,
	AKF_VariableKeyLerp,
	AKF_PerTrackCompression,
	AKF_MAX,
};

/** 
 * For an additive animation, indicates what the animation is relative to.
 */
UENUM()
enum EAdditiveBasePoseType
{
	/** Will be deprecated. */
	ABPT_None UMETA(DisplayName="None"),
	/** Use the Skeleton's ref pose as base. */
	ABPT_RefPose UMETA(DisplayName="Reference Pose"),
	/** Use a whole animation as a base pose. BasePoseSeq must be set. */
	ABPT_AnimScaled UMETA(DisplayName="Selected animation scaled"),
	/** Use one frame of an animation as a base pose. BasePoseSeq and RefFrameIndex must be set (RefFrameIndex will be clamped). */
	ABPT_AnimFrame UMETA(DisplayName="Selected animation frame"),
	ABPT_MAX,
};

/**
 * Raw keyframe data for one track.  Each array will contain either NumFrames elements or 1 element.
 * One element is used as a simple compression scheme where if all keys are the same, they'll be
 * reduced to 1 key that is constant over the entire sequence.
 */
USTRUCT()
struct ENGINE_API FRawAnimSequenceTrack
{
	GENERATED_USTRUCT_BODY()

	/** Position keys. */
	UPROPERTY()
	TArray<FVector> PosKeys;

	/** Rotation keys. */
	UPROPERTY()
	TArray<FQuat> RotKeys;

	/** Scale keys. */
	UPROPERTY()
	TArray<FVector> ScaleKeys;

	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FRawAnimSequenceTrack& T )
	{
		T.PosKeys.BulkSerialize(Ar);
		T.RotKeys.BulkSerialize(Ar);

		if (Ar.UE4Ver() >= VER_UE4_ANIM_SUPPORT_NONUNIFORM_SCALE_ANIMATION)
		{
			T.ScaleKeys.BulkSerialize(Ar);
		}

		return Ar;
	}
};

// These two always should go together, but it is not right now. 
// I wonder in the future, we change all compressed to be inside as well, so they all stay together
// When remove tracks, it should be handled together 
USTRUCT()
struct ENGINE_API FAnimSequenceTrackContainer
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<struct FRawAnimSequenceTrack> AnimationTracks;

	UPROPERTY()
	TArray<FName>						TrackNames;

	// @todo expand this struct to work better and assign data better
	void Initialize(int32 NumNode)
	{
		AnimationTracks.Empty(NumNode);
		AnimationTracks.AddZeroed(NumNode);
		TrackNames.Empty(NumNode);
		TrackNames.AddZeroed(NumNode);
	}

	void Initialize(TArray<FName> InTrackNames)
	{
		TrackNames = InTrackNames;
		int32 NumNode = TrackNames.Num();
		AnimationTracks.Empty(NumNode);
		AnimationTracks.AddZeroed(NumNode);
	}

	int32 GetNum() const
	{
		check (TrackNames.Num() == AnimationTracks.Num());
		return (AnimationTracks.Num());
	}
};

// @note We have a plan to support skeletal hierarchy. When that happens, we'd like to keep skeleton indexing.
USTRUCT()
struct ENGINE_API FTrackToSkeletonMap
{
	GENERATED_USTRUCT_BODY()

	// Index of Skeleton.BoneTree this Track belongs to.
	UPROPERTY()
	int32 BoneTreeIndex;


	FTrackToSkeletonMap()
		: BoneTreeIndex(0)
	{
	}

	FTrackToSkeletonMap(int32 InBoneTreeIndex)
		: BoneTreeIndex(InBoneTreeIndex)
	{
	}
};

/**
 * Keyframe position data for one track.  Pos(i) occurs at Time(i).  Pos.Num() always equals Time.Num().
 */
USTRUCT()
struct ENGINE_API FTranslationTrack
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FVector> PosKeys;

	UPROPERTY()
	TArray<float> Times;
};

/**
 * Keyframe rotation data for one track.  Rot(i) occurs at Time(i).  Rot.Num() always equals Time.Num().
 */
USTRUCT()
struct ENGINE_API FRotationTrack
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FQuat> RotKeys;

	UPROPERTY()
	TArray<float> Times;
};

/**
 * Keyframe scale data for one track.  Scale(i) occurs at Time(i).  Rot.Num() always equals Time.Num().
 */
USTRUCT()
struct ENGINE_API FScaleTrack
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FVector> ScaleKeys;

	UPROPERTY()
	TArray<float> Times;
};


/**
 * Key frame curve data for one track
 * CurveName: Morph Target Name
 * CurveWeights: List of weights for each frame
 */
USTRUCT()
struct ENGINE_API FCurveTrack
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName CurveName;

	UPROPERTY()
	TArray<float> CurveWeights;

	/** Returns true if valid curve weight exists in the array*/
	bool IsValidCurveTrack();
	
	/** This is very simple cut to 1 key method if all is same since I see so many redundant same value in every frame 
	 *  Eventually this can get more complicated 
	 *  Will return true if compressed to 1. Return false otherwise **/
	bool CompressCurveWeights();
};

USTRUCT()
struct ENGINE_API FCompressedTrack
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<uint8> ByteStream;

	UPROPERTY()
	TArray<float> Times;

	UPROPERTY()
	float Mins[3];

	UPROPERTY()
	float Ranges[3];


	FCompressedTrack()
	{
		for (int32 ElementIndex = 0; ElementIndex < 3; ElementIndex++)
		{
			Mins[ElementIndex] = 0;
		}
		for (int32 ElementIndex = 0; ElementIndex < 3; ElementIndex++)
		{
			Ranges[ElementIndex] = 0;
		}
	}

};

USTRUCT()
struct ENGINE_API FCompressedOffsetData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<int32> OffsetData;

	UPROPERTY()
	int32 StripSize;

	FCompressedOffsetData( int32 InStripSize=2 )
		: StripSize (InStripSize)
		{}

	void SetStripSize(int32 InStripSize)
	{
		ensure (InStripSize > 0 );
		StripSize = InStripSize;
	}

	const int32 GetOffsetData(int32 StripIndex, int32 Offset) const
	{
		checkSlow(OffsetData.IsValidIndex(StripIndex * StripSize + Offset));

		return OffsetData[StripIndex * StripSize + Offset];
	}

	void SetOffsetData(int32 StripIndex, int32 Offset, int32 Value)
	{
		checkSlow(OffsetData.IsValidIndex(StripIndex * StripSize + Offset));
		OffsetData[StripIndex * StripSize + Offset] = Value;
	}

	void AddUninitialized(int32 NumOfTracks)
	{
		OffsetData.AddUninitialized(NumOfTracks*StripSize);
	}

	void Empty(int32 NumOfTracks=0)
	{
		OffsetData.Empty(NumOfTracks*StripSize);
	}

	int32 GetMemorySize() const
	{
		return sizeof(int32)*OffsetData.Num() + sizeof(int32);
	}

	int32 GetNumTracks() const
	{
		return OffsetData.Num()/StripSize;
	}

	bool IsValid() const
	{
		return (OffsetData.Num() > 0);
	}
};

UCLASS(config=Engine, hidecategories=(UObject, Length), BlueprintType)
class ENGINE_API UAnimSequence : public UAnimSequenceBase
{
	GENERATED_UCLASS_BODY()

	/** Number of raw frames in this sequence (not used by engine - just for informational purposes). */
	UPROPERTY(AssetRegistrySearchable)
	int32 NumFrames;

	/**
	 * In the future, maybe keeping RawAnimSequenceTrack + TrackMap as one would be good idea to avoid inconsistent array size
	 * TrackToSkeletonMapTable(i) should contains  track mapping data for RawAnimationData(i). 
	 */
	UPROPERTY()
	TArray<struct FTrackToSkeletonMap> TrackToSkeletonMapTable;

	/**
	 * Raw uncompressed keyframe data. 
	 */
	TArray<struct FRawAnimSequenceTrack> RawAnimationData;

#if WITH_EDITORONLY_DATA
	/**
	 * This is name of tracks for editoronly - if we lose skeleton, we'll need relink them
	 */
	UPROPERTY()
	TArray<FName> AnimationTrackNames;

	/**
	 * Source RawAnimationData. Only can be overridden by when transform curves are added first time OR imported
	 */
	TArray<struct FRawAnimSequenceTrack> SourceRawAnimationData;
#endif // WITH_EDITORONLY_DATA

	/**
	 * Translation data post keyframe reduction.  TranslationData.Num() is zero if keyframe reduction
	 * has not yet been applied.
	 */
	UPROPERTY(transient)
	TArray<struct FTranslationTrack> TranslationData;

	/**
	 * Rotation data post keyframe reduction.  RotationData.Num() is zero if keyframe reduction
	 * has not yet been applied.
	 */
	UPROPERTY(transient)
	TArray<struct FRotationTrack> RotationData;


	/**
	 * Scale data post keyframe reduction.  ScaleData.Num() is zero if keyframe reduction
	 * has not yet been applied.
	 */
	UPROPERTY(transient)
	TArray<struct FScaleTrack> ScaleData;

#if WITH_EDITORONLY_DATA
	/**
	 * The compression scheme that was most recently used to compress this animation.
	 * May be NULL.
	 */
	UPROPERTY(Instanced, Category=Compression, EditAnywhere)
	class UAnimCompress* CompressionScheme;
#endif // WITH_EDITORONLY_DATA

	/** The compression format that was used to compress translation tracks. */
	UPROPERTY()
	TEnumAsByte<enum AnimationCompressionFormat> TranslationCompressionFormat;

	/** The compression format that was used to compress rotation tracks. */
	UPROPERTY()
	TEnumAsByte<enum AnimationCompressionFormat> RotationCompressionFormat;

	/** The compression format that was used to compress rotation tracks. */
	UPROPERTY()
	TEnumAsByte<enum AnimationCompressionFormat> ScaleCompressionFormat;

	/**
	 * An array of 4*NumTrack ints, arranged as follows: - PerTrack is 2*NumTrack, so this isn't true any more
	 *   [0] Trans0.Offset
	 *   [1] Trans0.NumKeys
	 *   [2] Rot0.Offset
	 *   [3] Rot0.NumKeys
	 *   [4] Trans1.Offset
	 *   . . .
	 */
	UPROPERTY()
	TArray<int32> CompressedTrackOffsets;

	/**
	 * An array of 2*NumTrack ints, arranged as follows: 
		if identity, it is offset
		if not, it is num of keys 
	 *   [0] Scale0.Offset or NumKeys
	 *   [1] Scale1.Offset or NumKeys

	 * @TODO NOTE: first implementation is offset is [0], numkeys [1]
	 *   . . .
	 */
	UPROPERTY()
	FCompressedOffsetData CompressedScaleOffsets;

	/**
	 * ByteStream for compressed animation data.
	 * All keys are currently stored at evenly-spaced intervals (ie no explicit key times).
	 *
	 * For a translation track of n keys, data is packed as n uncompressed float[3]:
	 *
	 * For a rotation track of n>1 keys, the first 24 bytes are reserved for compression info
	 * (eg Fixed32 stores float Mins[3]; float Ranges[3]), followed by n elements of the compressed type.
	 * For a rotation track of n=1 keys, the single key is packed as an FQuatFloat96NoW.
	 */
	TArray<uint8> CompressedByteStream;

	UPROPERTY()
	TEnumAsByte<enum AnimationKeyFormat> KeyEncodingFormat;

	/**
	 * The runtime interface to decode and byte swap the compressed animation
	 * May be NULL. Set at runtime - does not exist in editor
	 */
	class AnimEncoding* TranslationCodec;

	class AnimEncoding* RotationCodec;

	class AnimEncoding* ScaleCodec;

	/** Additive animation type. **/
	UPROPERTY(EditAnywhere, Category=AdditiveSettings, AssetRegistrySearchable)
	TEnumAsByte<enum EAdditiveAnimationType> AdditiveAnimType;

	/* Additive refrerence pose type. Refer above enum type */
	UPROPERTY(EditAnywhere, Category=AdditiveSettings, meta=(DisplayName = "Base Pose Type"))
	TEnumAsByte<enum EAdditiveBasePoseType> RefPoseType;

	/* Additive reference animation if it's relevant - i.e. AnimScaled or AnimFrame **/
	UPROPERTY(EditAnywhere, Category=AdditiveSettings, meta=(DisplayName = "Base Pose Animation"))
	class UAnimSequence* RefPoseSeq;

	/* Additve reference frame if RefPoseType == AnimFrame **/
	UPROPERTY(EditAnywhere, Category=AdditiveSettings)
	int32 RefFrameIndex;

	// Versioning Support
	/** The version of the global encoding package used at the time of import */
	UPROPERTY()
	int32 EncodingPkgVersion;

	/** Base pose to use when retargeting */
	UPROPERTY(EditAnywhere, AssetRegistrySearchable, Category=Animation)
	FName RetargetSource;

	/** If this is on, it will allow extracting of root motion **/
	UPROPERTY(EditAnywhere, AssetRegistrySearchable, Category = RootMotion, meta = (DisplayName = "EnableRootMotion"))
	bool bEnableRootMotion;

	/** Root Bone will be locked to that position when extracting root motion.**/
	UPROPERTY(EditAnywhere, Category = RootMotion)
	TEnumAsByte<ERootMotionRootLock::Type> RootMotionRootLock;
	
	/** Force Root Bone Lock even if Root Motion is not enabled */
	UPROPERTY(EditAnywhere, Category = RootMotion)
	bool bForceRootLock;

	/** Have we copied root motion settings from an owning montage */
	UPROPERTY()
	bool bRootMotionSettingsCopiedFromMontage;

#if WITH_EDITORONLY_DATA
	/** Saved version number with CompressAnimations commandlet. To help with doing it in multiple passes. */
	UPROPERTY()
	int32 CompressCommandletVersion;

	/**
	 * Do not attempt to override compression scheme when running CompressAnimations commandlet.
	 * Some high frequency animations are too sensitive and shouldn't be changed.
	 */
	UPROPERTY(EditAnywhere, Category=Compression)
	uint32 bDoNotOverrideCompression:1;

	/**
	 * Used to track whether, or not, this sequence was compressed with it's full translation tracks
	 */
	UPROPERTY()
	uint32 bWasCompressedWithoutTranslations:1;

	/** Importing data and options used for this mesh */
	UPROPERTY(VisibleAnywhere, Instanced, Category=ImportSettings)
	class UAssetImportData* AssetImportData;

	/***  for Reimport **/
	/** Path to the resource used to construct this skeletal mesh */
	UPROPERTY()
	FString SourceFilePath_DEPRECATED;

	/** Date/Time-stamp of the file from the last import */
	UPROPERTY()
	FString SourceFileTimestamp_DEPRECATED;

	UPROPERTY(transient)
	bool bNeedsRebake;

	// Track whether we have updated markers so cached data can be updated
	int32 MarkerDataUpdateCounter;
#endif // WITH_EDITORONLY_DATA

	/** Authored Sync markers */
	UPROPERTY()
	TArray<FAnimSyncMarker>		AuthoredSyncMarkers;

	/** List of Unique marker names in this animation sequence */
	TArray<FName>				UniqueMarkerNames;

public:
	//~ Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	virtual void PreSave() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
#endif // WITH_EDITOR
	virtual void BeginDestroy() override;
	virtual SIZE_T GetResourceSize(EResourceSizeMode::Type Mode) override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	//~ End UObject Interface

	//~ Begin UAnimationAsset Interface
	virtual bool IsValidAdditive() const override;
	virtual TArray<FName>* GetUniqueMarkerNames() { return &UniqueMarkerNames; }
#if WITH_EDITOR
	virtual bool GetAllAnimationSequencesReferred(TArray<UAnimSequence*>& AnimationSequences) override;
	virtual void ReplaceReferredAnimations(const TMap<UAnimSequence*, UAnimSequence*>& ReplacementMap) override;
	virtual int32 GetNumberOfFrames() const override { return NumFrames; }
#endif
	//~ End UAnimationAsset Interface

	//~ Begin UAnimSequenceBase Interface
	virtual void OnAssetPlayerTickedInternal(FAnimAssetTickContext &Context, const float PreviousTime, const float MoveDelta, const FAnimTickRecord &Instance, class UAnimInstance* InstanceOwner) const override;
	virtual bool HasRootMotion() const override { return bEnableRootMotion; }
	virtual void RefreshCacheData() override;
	//~ End UAnimSequenceBase Interface

	// Extract Root Motion transform from the animation
	FTransform ExtractRootMotion(float StartTime, float DeltaTime, bool bAllowLooping) const;

	// Extract Root Motion transform from a contiguous position range (no looping)
	FTransform ExtractRootMotionFromRange(float StartTrackPosition, float EndTrackPosition) const;

	// Extract the transform from the root track for the given animation position
	FTransform ExtractRootTrackTransform(float Pos, const FBoneContainer * RequiredBones) const;

	// Begin Transform related functions 

	/**
	* Get Bone Transform of the Time given, relative to Parent for all RequiredBones
	* This returns different transform based on additive or not. Or what kind of additive.
	*
	* @param	OutPose				[out] Pose object to fill
	* @param	OutCurve			[out] Curves to fill
	* @param	ExtractionContext	Extraction Context (position, looping, root motion, etc.)
	*/
	virtual void GetAnimationPose(FCompactPose& OutPose, FBlendedCurve& OutCurve, const FAnimExtractContext& ExtractionContext) const override;

	/**
	* Get Bone Transform of the animation for the Time given, relative to Parent for all RequiredBones
	*
	* @param	OutPose				[out] Array of output bone transforms
	* @param	OutCurve			[out] Curves to fill	
	* @param	RequiredBones		Array of Desired Tracks
	* @param	ExtractionContext	Extraction Context (position, looping, root motion, etc.)
	*/
	void GetBonePose(FCompactPose& OutPose, FBlendedCurve& OutCurve, const FAnimExtractContext& ExtractionContext) const;

private:
	/** 
	 * Utility function.
	 * When extracting root motion, this function resets the root bone to the first frame of the animation.
	 * 
	 * @param	BoneTransform		Root Bone Transform
	 * @param	RequiredBones		BoneContainer
	 * @param	ExtractionContext	Extraction Context to access root motion extraction information.
	 */
	void ResetRootBoneForRootMotion(FTransform& BoneTransform, const FBoneContainer & RequiredBones, ERootMotionRootLock::Type InRootMotionRootLock) const;

	/**
	* Retarget a single bone transform, to apply right after extraction.
	*
	* @param	BoneTransform		BoneTransform to read/write from.
	* @param	SkeletonBoneIndex	Bone Index in USkeleton.
	* @param	BoneIndex			Bone Index in Bone Transform array.
	* @param	RequiredBones		BoneContainer
	*/
	void RetargetBoneTransform(FTransform& BoneTransform, const int32& SkeletonBoneIndex, const FCompactPoseBoneIndex& BoneIndex, const FBoneContainer& RequiredBones) const;

public:
	/**
	* Get Bone Transform of the additive animation for the Time given, relative to Parent for all RequiredBones
	*
	* @param	OutPose				[out] Output bone transforms
	* @param	OutCurve			[out] Curves to fill	
	* @param	ExtractionContext	Extraction Context (position, looping, root motion, etc.)
	*/
	void GetBonePose_Additive(FCompactPose& OutPose, FBlendedCurve& OutCurve, const FAnimExtractContext& ExtractionContext) const;

	/**
	* Get Bone Transform of the base (reference) pose of the additive animation for the Time given, relative to Parent for all RequiredBones
	*
	* @param	OutPose				[out] Output bone transforms
	* @param	OutCurve			[out] Curves to fill	
	* @param	ExtractionContext	Extraction Context (position, looping, root motion, etc.)
	*/
	void GetAdditiveBasePose(FCompactPose& OutPose, FBlendedCurve& OutCurve, const FAnimExtractContext& ExtractionContext) const;

	/**
	 * Get Bone Transform of the Time given, relative to Parent for the Track Given
	 *
	 * @param	OutAtom			[out] Output bone transform.
	 * @param	TrackIndex		Index of track to interpolate.
	 * @param	Time			Time on track to interpolate to.
	 * @param	bUseRawData		If true, use raw animation data instead of compressed data.
	 */
	void GetBoneTransform(FTransform& OutAtom, int32 TrackIndex, float Time, bool bUseRawData) const;

	/**
	 * Extract Bone Transform of the Time given, from InRawAnimationData
	 *
	 * @param	InRawAnimationData	RawAnimationData it extracts bone transform from
	 * @param	OutAtom				[out] Output bone transform.
	 * @param	TrackIndex			Index of track to interpolate.
	 * @param	Time				Time on track to interpolate to.
	 */
	void ExtractBoneTransform(const TArray<struct FRawAnimSequenceTrack> & InRawAnimationData, FTransform& OutAtom, int32 TrackIndex, float Time) const;

	// End Transform related functions 

	// Begin Memory related functions
	/**
	 * @return		The approximate size of raw animation data.
	 */
	int32 GetApproxRawSize() const;

	/**
	 * @return		The approximate size of key-reduced animation data.
	 */
	int32 GetApproxReducedSize() const;

	/**
	 * @return		The approximate size of compressed animation data.
	 */
	int32 GetApproxCompressedSize() const;

	/** 
	 *  Utility function for lossless compression of a FRawAnimSequenceTrack 
	 *  @return true if keys were removed.
	 **/
	bool CompressRawAnimSequenceTrack(FRawAnimSequenceTrack& RawTrack, float MaxPosDiff, float MaxAngleDiff);

	/**
	 * Removes trivial frames -- frames of tracks when position or orientation is constant
	 * over the entire animation -- from the raw animation data.  If both position and rotation
	 * go down to a single frame, the time is stripped out as well.
	 * @return true if keys were removed.
	 */
	bool CompressRawAnimData(float MaxPosDiff, float MaxAngleDiff);

	/**
	 * Removes trivial frames -- frames of tracks when position or orientation is constant
	 * over the entire animation -- from the raw animation data.  If both position and rotation
	 * go down to a single frame, the time is stripped out as well.
	 * @return true if keys were removed.
	 */
	bool CompressRawAnimData();
	// End Memory related functions

	// Begin Utility functions
	/**
	 * Get Skeleton Bone Index from Track Index
	 *
	 * @param	TrackIndex		Track Index
	 */
	int32 GetSkeletonIndexFromTrackIndex(const int32& TrackIndex) const 
	{ 
		return TrackToSkeletonMapTable[TrackIndex].BoneTreeIndex; 
	}

	/** Clears any data in the AnimSequence, so it can be recycled when importing a new animation with same name over it. */
	void RecycleAnimSequence();

	/** 
	 * Utility function to copy all UAnimSequence properties from Source to Destination.
	 * Does not copy however RawAnimData, CompressedAnimData and AdditiveBasePose.
	 */
	static bool CopyAnimSequenceProperties(UAnimSequence* SourceAnimSeq, UAnimSequence* DestAnimSeq, bool bSkipCopyingNotifies=false);

	/** 
	 * Copy AnimNotifies from one UAnimSequence to another.
	 */
	static bool CopyNotifies(UAnimSequence* SourceAnimSeq, UAnimSequence* DestAnimSeq);

	/**
	 * Flip Rotation's W For NonRoot items, and compress it again if SkelMesh exists
	 */
	void FlipRotationWForNonRoot(USkeletalMesh * SkelMesh);

	/** 
	 * Return number of tracks in this animation
	 */
	int32 GetNumberOfTracks() const;

	// End Utility functions
#if WITH_EDITOR
	/**
	 * After imported or any other change is made, call this to apply post process
	 */
	void PostProcessSequence();
	/** 
	 * Insert extra frame of the first frame at the end of the frame so that it improves the interpolation when it loops
	 * This increases framecount + time, so that it requires recompression
	 */
	bool AddLoopingInterpolation();

	/** 
	 * Bake Transform Curves.TransformCurves to RawAnimation after making a back up of current RawAnimation
	 */
	void BakeTrackCurvesToRawAnimation();

	/**
	 * Add Key to Transform Curves
	 */
	void AddKeyToSequence(float Time, const FName& BoneName, const FTransform& AdditiveTransform);
	/**
	 * Return true if it needs to re-bake
	 */
	bool DoesNeedRebake() const;
	/**
	 * Return true if it contains transform curves
	 */
	bool DoesContainTransformCurves() const;

	/**
	 * Create Animation Sequence from Reference Pose of the Mesh
	 */
	bool CreateAnimation(class USkeletalMesh * Mesh);
	/**
	 * Create Animation Sequence from the Mesh Component's current bone trasnform
	 */
	bool CreateAnimation(class USkeletalMeshComponent * MeshComponent);
	/**
	 * Create Animation Sequence from the given animation
	 */
	bool CreateAnimation(class UAnimSequence * Sequence);

	/**
	 * Crops the raw anim data either from Start to CurrentTime or CurrentTime to End depending on
	 * value of bFromStart.  Can't be called against cooked data.
	 *
	 * @param	CurrentTime		marker for cropping (either beginning or end)
	 * @param	bFromStart		whether marker is begin or end marker
	 * @return					true if the operation was successful.
	 */
	bool CropRawAnimData( float CurrentTime, bool bFromStart );

		
	/**
	 * Crops the raw anim data either from Start to CurrentTime or CurrentTime to End depending on
	 * value of bFromStart.  Can't be called against cooked data.
	 *
	 * @param	StartFrame		StartFrame to insert (0-based)
	 * @param	EndFrame		EndFrame to insert (0-based
	 * @param	CopyFrame		A frame that we copy from (0-based)
	 * @return					true if the operation was successful.
	 */
	bool InsertFramesToRawAnimData( int32 StartFrame, int32 EndFrame, int32 CopyFrame);

	/** 
	 * Add validation check to see if it's being ready to play or not
	 */
	virtual bool IsValidToPlay() const override;

	// Get a pointer to the data for a given Anim Notify
	uint8* FindSyncMarkerPropertyData(int32 SyncMarkerIndex, UArrayProperty*& ArrayProperty);

	virtual int32 GetMarkerUpdateCounter() const { return MarkerDataUpdateCounter; }
#endif

	/** Sort the sync markers array by time, earliest first. */
	void SortSyncMarkers();

	// Advancing based on markers
	float GetCurrentTimeFromMarkers(FMarkerPair& PrevMarker, FMarkerPair& NextMarker, float PositionBetweenMarkers) const;
	virtual void AdvanceMarkerPhaseAsLeader(bool bLooping, float MoveDelta, const TArray<FName>& ValidMarkerNames, float& CurrentTime, FMarkerPair& PrevMarker, FMarkerPair& NextMarker, TArray<FPassedMarker>& MarkersPassed) const;
	virtual void AdvanceMarkerPhaseAsFollower(const FMarkerTickContext& Context, float DeltaRemaining, bool bLooping, float& CurrentTime, FMarkerPair& PreviousMarker, FMarkerPair& NextMarker) const;
	virtual void GetMarkerIndicesForTime(float CurrentTime, bool bLooping, const TArray<FName>& ValidMarkerNames, FMarkerPair& OutPrevMarker, FMarkerPair& OutNextMarker) const;
	virtual FMarkerSyncAnimPosition GetMarkerSyncPositionfromMarkerIndicies(int32 PrevMarker, int32 NextMarker, float CurrentTime) const;
	virtual void GetMarkerIndicesForPosition(const FMarkerSyncAnimPosition& SyncPosition, bool bLooping, FMarkerPair& OutPrevMarker, FMarkerPair& OutNextMarker, float& CurrentTime) const;
	
	virtual float GetFirstMatchingPosFromMarkerSyncPos(const FMarkerSyncAnimPosition& InMarkerSyncGroupPosition) const override;
	virtual float GetNextMatchingPosFromMarkerSyncPos(const FMarkerSyncAnimPosition& InMarkerSyncGroupPosition, const float& StartingPosition) const override;

private:
	/**
	* Get Bone Transform of the animation for the Time given, relative to Parent for all RequiredBones
	* This return mesh rotation only additive pose
	*
	* @param	OutPose				[out] Output bone transforms
	* @param	OutCurve			[out] Curves to fill	
	* @param	ExtractionContext	Extraction Context (position, looping, root motion, etc.)
	*/
	void GetBonePose_AdditiveMeshRotationOnly(FCompactPose& OutPose, FBlendedCurve& OutCurve, const FAnimExtractContext& ExtractionContext) const;

#if WITH_EDITOR
	/**
	 * Remap Tracks to New Skeleton
	 */
	void RemapTracksToNewSkeleton( USkeleton* NewSkeleton, bool bConvertSpaces );
	/**
	 * Remap NaN tracks from the RawAnimation data and recompress
	 */	
	void RemoveNaNTracks();

	/** Retargeting functions */
	bool ConvertAnimationDataToRiggingData(FAnimSequenceTrackContainer & RiggingAnimationData);
	bool ConvertRiggingDataToAnimationData(FAnimSequenceTrackContainer & RiggingAnimationData);
	int32 GetSpaceBasedAnimationData(TArray< TArray<FTransform> > & AnimationDataInComponentSpace, FAnimSequenceTrackContainer * RiggingAnimationData) const;

	/** Verify Track Map is valid, if not, fix up */
	void VerifyTrackMap(USkeleton* MySkeleton=NULL);
	/** Reset Animation Data. Called before Creating new Animation data **/
	void ResetAnimation();
	/** Refresh Track Map from Animation Track Names **/
	void RefreshTrackMapFromAnimTrackNames();

	/**
	 * Utility function that helps to remove track, you can't just remove RawAnimationData
	 */
	void RemoveTrack(int32 TrackIndex);
	/**
	 * Utility function that finds the correct spot to insert track to 
	 */
	int32 InsertTrack(const FName& BoneName);

	/**
	 * Utility function to resize the sequence
	 * It rearranges curve data + notifies
	 */
	void ResizeSequence(float NewLength, int32 NewNumFrames, bool bInsert, int32 StartFrame/*inclusive */, int32 EndFrame/*inclusive*/);

#endif

	/** Refresh sync marker data*/
	void RefreshSyncMarkerDataFromAuthored();

	/** Take a set of marker positions and validates them against a requested start position, updating them as desired */
	void ValidateCurrentPosition(const FMarkerSyncAnimPosition& Position, bool bPlayingForwards, bool bLooping, float&CurrentTime, FMarkerPair& PreviousMarker, FMarkerPair& NextMarker) const;

	friend class UAnimationAsset;
};



