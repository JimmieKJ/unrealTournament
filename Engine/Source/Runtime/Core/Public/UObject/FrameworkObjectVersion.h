// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

// Custom serialization version for changes made in Dev-Framework stream
struct CORE_API FFrameworkObjectVersion
{
	enum Type
	{
		// Before any version changes were made
		BeforeCustomVersionWasAdded = 0,

		// BodySetup's default instance collision profile is used by default when creating a new instance.
		UseBodySetupCollisionProfile,

		// Regenerate subgraph arrays correctly in animation blueprints to remove duplicates and add
		// missing graphs that appear read only when edited
		AnimBlueprintSubgraphFix,

		// Static and skeletal mesh sockets now use the specified scale
		MeshSocketScaleUtilization,

		// Attachment rules are now explicit in how they affect location, rotation and scale
		ExplicitAttachmentRules,

		// Moved compressed anim data from uasset to the DDC
		MoveCompressedAnimDataToTheDDC,

		// Some graph pins created using legacy code seem to have lost the RF_Transactional flag,
		// which causes issues with undo. Restore the flag at this version
		FixNonTransactionalPins,

		// Create new struct for SmartName, and use that for CurveName
		SmartNameRefactor,
		
		// Add Reference Skeleton to Rig
		AddSourceReferenceSkeletonToRig, 

		// Refactor ConstraintInstance so that we have an easy way to swap behavior paramters
		ConstraintInstanceBehaviorParameters,

		// Pose Asset support mask per bone
		PoseAssetSupportPerBoneMask,

		// Physics Assets now use SkeletalBodySetup instead of BodySetup
		PhysAssetUseSkeletalBodySetup,

		// Remove SoundWave CompressionName
		RemoveSoundWaveCompressionName,

		// Switched render data for clothing over to unreal data, reskinned to the simulation mesh
		AddInternalClothingGraphicalSkinning,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;

private:
	FFrameworkObjectVersion() {}
};
