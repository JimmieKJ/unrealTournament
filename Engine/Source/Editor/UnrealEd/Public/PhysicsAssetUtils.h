// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


enum EPhysAssetFitGeomType
{
	EFG_Box,
	EFG_Sphyl,
	EFG_Sphere,
	EFG_SingleConvexHull,
	EFG_MultiConvexHull
};

enum EPhysAssetFitVertWeight
{
	EVW_AnyWeight,
	EVW_DominantWeight
};

/** Parameters for PhysicsAsset creation */
struct FPhysAssetCreateParams
{
	float								MinBoneSize;
	EPhysAssetFitGeomType				GeomType;
	EPhysAssetFitVertWeight				VertWeight;
	bool								bAlignDownBone;
	bool								bCreateJoints;
	bool								bWalkPastSmall;
	bool								bBodyForAll;
	EAngularConstraintMotion			AngularConstraintMode;
	float								HullAccuracy;
	int32								MaxHullVerts;

	UNREALED_API void Initialize();
};

class UPhysicsAsset;
class UPhysicsConstraintTemplate;

/** Collection of functions to create and setup PhysicsAssets */
namespace FPhysicsAssetUtils
{
	/**
	 * Given a USkeletalMesh, construct a new PhysicsAsset automatically, using the vertices weighted to each bone to calculate approximate collision geometry.
	 * Ball-and-socket joints will be created for every joint by default.
	 *
	 * @param	PhysicsAsset		The PhysicsAsset instance to setup
	 * @param	SkelMesh			The Skeletal Mesh to create the physics asset from
	 * @param	Params				Additional creation parameters
	 */
	UNREALED_API bool CreateFromSkeletalMesh(UPhysicsAsset* PhysicsAsset, USkeletalMesh* SkelMesh, FPhysAssetCreateParams& Params, FText& OutErrorMessage);

	/** Replaces any collision already in the BodySetup with an auto-generated one using the parameters provided. 
	 *
	 * @param	bs					BodySetup to create the collision for
	 * @param	skelMesh			The SkeletalMesh we create collision for
	 * @param	BoneIndex			Index of the bone the collision is created for
	 * @param	Params				Additional parameters to control the creation 
	 * @param	Infos				The vertices to create the collirion for
	 * @return  Returns true if successfully created collision from bone
	 */
	UNREALED_API bool CreateCollisionFromBone( UBodySetup* bs, USkeletalMesh* skelMesh, int32 BoneIndex, FPhysAssetCreateParams& Params, const TArray<FBoneVertInfo>& Infos );

	/**
	 * Does a few things:
	 * - add any collision primitives from body2 into body1 (adjusting eaches tm).
	 * - reconnect any constraints between 'add body' to 'base body', destroying any between them.
	 * - update collision disable table for any pairs including 'add body'
	 */
	UNREALED_API void WeldBodies(UPhysicsAsset* PhysAsset, int32 BaseBodyIndex, int32 AddBodyIndex, USkeletalMeshComponent* SkelComp);

	/**
	 * Creates a new constraint
	 *
	 * @param	PhysAsset			The PhysicsAsset to create the constraint for
	 * @param	InConstraintName	Name of the constraint
	 * @param	InConstraintSetup	Optional constraint setup
	 * @return  Returns the index of the newly created constraint.
	 **/
	UNREALED_API int32 CreateNewConstraint(UPhysicsAsset* PhysAsset, FName InConstraintName, UPhysicsConstraintTemplate* InConstraintSetup = NULL);

	/**
	 * Destroys the specified constraint
	 *
	 * @param	PhysAsset			The PhysicsAsset for which the constraint should be destroyed
	 * @param	ConstraintIndex		The index of the constraint to destroy
	 */
	UNREALED_API void DestroyConstraint(UPhysicsAsset* PhysAsset, int32 ConstraintIndex);

	/**
	 * Create a new BodySetup and default BodyInstance if there is not one for this body already.
	 *
	 * @param	PhysAsset			The PhysicsAsset to create the body for
	 * @param	InBodyName			Name of the new body
	 * @return	The Index of the newly created body.
	 */
	UNREALED_API int32 CreateNewBody(UPhysicsAsset* PhysAsset, FName InBodyName);
	
	/** 
	 * Destroys the specified body
	 *
	 * @param	PhysAsset			The PhysicsAsset for which the body should be destroyed
	 * @param	BodyIndex			Index of the body to destroy
	 */
	UNREALED_API void DestroyBody(UPhysicsAsset* PhysAsset, int32 BodyIndex);
};