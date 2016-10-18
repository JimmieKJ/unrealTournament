// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Sound/SoundBase.h"
#include "SoundCue.generated.h"

struct FActiveSound;
struct FSoundParseParameters;

USTRUCT()
struct FSoundNodeEditorData
{
	GENERATED_USTRUCT_BODY()

	int32 NodePosX;

	int32 NodePosY;


	FSoundNodeEditorData()
		: NodePosX(0)
		, NodePosY(0)
	{
	}


	friend FArchive& operator<<(FArchive& Ar,FSoundNodeEditorData& MySoundNodeEditorData)
	{
		return Ar << MySoundNodeEditorData.NodePosX << MySoundNodeEditorData.NodePosY;
	}
};

/**
 * The behavior of audio playback is defined within Sound Cues.
 */
UCLASS(hidecategories=object, MinimalAPI, BlueprintType)
class USoundCue : public USoundBase
{
	GENERATED_UCLASS_BODY()

	/* Indicates whether attenuation should use the Attenuation Overrides or the Attenuation Settings asset */
	UPROPERTY(EditAnywhere, Category=Attenuation)
	uint32 bOverrideAttenuation:1;

	UPROPERTY()
	class USoundNode* FirstNode;

	/* Volume multiplier for the Sound Cue */
	UPROPERTY(EditAnywhere, Category=Sound, AssetRegistrySearchable)
	float VolumeMultiplier;

	/* Pitch multiplier for the Sound Cue */
	UPROPERTY(EditAnywhere, Category=Sound, AssetRegistrySearchable)
	float PitchMultiplier;

	/* Attenuation settings to use if Override Attenuation is set to true */
	UPROPERTY(EditAnywhere, Category=Attenuation, meta=(EditCondition="bOverrideAttenuation"))
	FAttenuationSettings AttenuationOverrides;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TArray<USoundNode*> AllNodes;

	UPROPERTY()
	class UEdGraph* SoundCueGraph;

#endif

private:
	float MaxAudibleDistance;


public:

	//~ Begin UObject Interface.
	virtual SIZE_T GetResourceSize(EResourceSizeMode::Type Mode) override;
	virtual FString GetDesc() override;
#if WITH_EDITOR
	virtual void PostInitProperties() override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
#endif
	virtual void PostLoad() override;
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface.

	//~ Begin USoundBase Interface.
	virtual bool IsPlayable() const override;
	virtual bool ShouldApplyInteriorVolumes() const override;
	virtual void Parse( class FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances ) override;
	virtual float GetVolumeMultiplier() override;
	virtual float GetPitchMultiplier() override;
	virtual float GetMaxAudibleDistance() override;
	virtual float GetDuration() override;
	virtual const FAttenuationSettings* GetAttenuationSettingsToApply() const override;
	//~ End USoundBase Interface.

	/** Construct and initialize a node within this Cue */
	template<class T>
	T* ConstructSoundNode(TSubclassOf<USoundNode> SoundNodeClass = T::StaticClass(), bool bSelectNewNode = true)
	{
		T* SoundNode = NewObject<T>(this, SoundNodeClass);
#if WITH_EDITOR
		AllNodes.Add(SoundNode);
		SetupSoundNode(SoundNode, bSelectNewNode);
#endif // WITH_EDITORONLY_DATA
		return SoundNode;
	}

	/**
	*	@param		Format		Format to check
	 *
	 *	@return		Sum of the size of waves referenced by this cue for the given platform.
	 */
	virtual int32 GetResourceSizeForFormat(FName Format);

	/**
	 * Recursively finds all Nodes in the Tree
	 */
	ENGINE_API void RecursiveFindAllNodes( USoundNode* Node, TArray<class USoundNode*>& OutNodes );

	/**
	 * Recursively finds sound nodes of type T
	 */
	template<typename T> ENGINE_API void RecursiveFindNode( USoundNode* Node, TArray<T*>& OutNodes );

	/** Find the path through the sound cue to a node identified by its hash */
	ENGINE_API bool FindPathToNode(const UPTRINT NodeHashToFind, TArray<USoundNode*>& OutPath) const;

protected:
	bool RecursiveFindPathToNode(USoundNode* CurrentNode, const UPTRINT CurrentHash, const UPTRINT NodeHashToFind, TArray<USoundNode*>& OutPath) const;

private:
	void OnPostEngineInit();
	void EvaluateNodes(bool bAddToRoot);

	FDelegateHandle OnPostEngineInitHandle;

public:

	/**
	 * Instantiate certain functions to work around a linker issue
	 */
	ENGINE_API void RecursiveFindAttenuation( USoundNode* Node, TArray<class USoundNodeAttenuation*> &OutNodes );

#if WITH_EDITOR
	/** Create the basic sound graph */
	ENGINE_API void CreateGraph();

	/** Clears all nodes from the graph (for old editor's buffer soundcue) */
	ENGINE_API void ClearGraph();

	/** Set up EdGraph parts of a SoundNode */
	ENGINE_API void SetupSoundNode(class USoundNode* InSoundNode, bool bSelectNewNode = true);

	/** Use the SoundCue's children to link EdGraph Nodes together */
	ENGINE_API void LinkGraphNodesFromSoundNodes();

	/** Use the EdGraph representation to compile the SoundCue */
	ENGINE_API void CompileSoundNodesFromGraphNodes();

	/** Get the EdGraph of SoundNodes */
	ENGINE_API class USoundCueGraph* GetGraph();
#endif
};



