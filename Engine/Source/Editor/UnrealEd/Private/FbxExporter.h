// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*
* Copyright 2010 Autodesk, Inc.  All Rights Reserved.
*
* Permission to use, copy, modify, and distribute this software in object
* code form for any purpose and without fee is hereby granted, provided
* that the above copyright notice appears in all copies and that both
* that copyright notice and the limited warranty and restricted rights
* notice below appear in all supporting documentation.
*
* AUTODESK PROVIDES THIS PROGRAM "AS IS" AND WITH ALL FAULTS.
* AUTODESK SPECIFICALLY DISCLAIMS ANY AND ALL WARRANTIES, WHETHER EXPRESS
* OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTY
* OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR USE OR NON-INFRINGEMENT
* OF THIRD PARTY RIGHTS.  AUTODESK DOES NOT WARRANT THAT THE OPERATION
* OF THE PROGRAM WILL BE UNINTERRUPTED OR ERROR FREE.
*
* In no event shall Autodesk, Inc. be liable for any direct, indirect,
* incidental, special, exemplary, or consequential damages (including,
* but not limited to, procurement of substitute goods or services;
* loss of use, data, or profits; or business interruption) however caused
* and on any theory of liability, whether in contract, strict liability,
* or tort (including negligence or otherwise) arising in any way out
* of such code.
*
* This software is provided to the U.S. Government with the same rights
* and restrictions as described herein.
*/

#pragma once

#include "Factories.h"
#include "MatineeExporter.h"
#include "FbxImporter.h"

class ALandscapeProxy;
struct FAnimControlTrackKey;
class USplineMeshComponent;
class UInstancedStaticMeshComponent;
class ALight;
class UInterpTrackInstMove;
class AMatineeActor;

namespace UnFbx
{

/**
 * Main FBX Exporter class.
 */
class UNREALED_API FFbxExporter  : public MatineeExporter
{
public:
	/**
	 * Returns the exporter singleton. It will be created on the first request.
	 */
	static FFbxExporter* GetInstance();
	static void DeleteInstance();
	~FFbxExporter();
	
	/**
	 * Creates and readies an empty document for export.
	 */
	virtual void CreateDocument();
	
	/**
	 * Closes the FBX document, releasing its memory.
	 */
	virtual void CloseDocument();
	
	/**
	 * Writes the FBX document to disk and releases it by calling the CloseDocument() function.
	 */
	virtual void WriteToFile(const TCHAR* Filename);
	
	/**
	 * Exports the light-specific information for a light actor.
	 */
	virtual void ExportLight( ALight* Actor, AMatineeActor* InMatineeActor );

	/**
	 * Exports the camera-specific information for a camera actor.
	 */
	virtual void ExportCamera( ACameraActor* Actor, AMatineeActor* InMatineeActor, bool bExportComponents );

	/**
	 * Exports the mesh and the actor information for a brush actor.
	 */
	virtual void ExportBrush(ABrush* Actor, UModel* InModel, bool bConvertToStaticMesh );

	/**
	 * Exports the basic scene information to the FBX document.
	 */
	virtual void ExportLevelMesh( ULevel* InLevel, AMatineeActor* InMatineeActor, bool bSelectedOnly );

	/**
	 * Exports the given Matinee sequence information into a FBX document.
	 * 
	 * @return	true, if sucessful
	 */
	virtual bool ExportMatinee(class AMatineeActor* InMatineeActor);

	/**
	 * Exports all the animation sequences part of a single Group in a Matinee sequence
	 * as a single animation in the FBX document.  The animation is created by sampling the
	 * sequence at 30 updates/second and extracting the resulting bone transforms from the given
	 * skeletal mesh
	 * @param MatineeSequence The Matinee Sequence containing the group to export
	 * @param SkeletalMeshComponent The Skeletal mesh that the animations from the Matinee group are applied to
	 */
	virtual void ExportMatineeGroup(class AMatineeActor* MatineeActor, USkeletalMeshComponent* SkeletalMeshComponent);


	/**
	 * Exports the mesh and the actor information for a static mesh actor.
	 */
	virtual void ExportStaticMesh( AActor* Actor, UStaticMeshComponent* StaticMeshComponent, AMatineeActor* InMatineeActor );

	/**
	 * Exports a static mesh
	 * @param StaticMesh	The static mesh to export
	 * @param MaterialOrder	Optional ordering of materials to set up correct material ID's across multiple meshes being export such as BSP surfaces which share common materials. Should be used sparingly
	 */
	virtual void ExportStaticMesh( UStaticMesh* StaticMesh, const TArray<UMaterialInterface*>* MaterialOrder = NULL );

	/**
	 * Exports BSP
	 * @param Model			 The model with BSP to export
	 * @param bSelectedOnly  true to export only selected surfaces (or brushes)
	 */
	virtual void ExportBSP( UModel* Model, bool bSelectedOnly );

	/**
	 * Exports a static mesh light map
	 */
	virtual void ExportStaticMeshLightMap( UStaticMesh* StaticMesh, int32 LODIndex, int32 UVChannel );

	/**
	 * Exports a skeletal mesh
	 */
	virtual void ExportSkeletalMesh( USkeletalMesh* SkeletalMesh );

	/**
	 * Exports the mesh and the actor information for a skeletal mesh actor.
	 */
	virtual void ExportSkeletalMesh( AActor* Actor, USkeletalMeshComponent* SkeletalMeshComponent );
	
	/**
	 * Exports the mesh and the actor information for a landscape actor.
	 */
	void ExportLandscape(ALandscapeProxy* Landscape, bool bSelectedOnly);

	/**
	 * Exports a single UAnimSequence, and optionally a skeletal mesh
	 */
	FbxNode* ExportAnimSequence( const UAnimSequence* AnimSeq, const USkeletalMesh* SkelMesh, bool bExportSkelMesh, const TCHAR* MeshNames=NULL, FbxNode* ActorRootNode=NULL);

	/**
	 * Exports the list of UAnimSequences as a single animation based on the settings in the TrackKeys
	 */
	void ExportAnimSequencesAsSingle( USkeletalMesh* SkelMesh, const ASkeletalMeshActor* SkelMeshActor, const FString& ExportName, const TArray<UAnimSequence*>& AnimSeqList, const TArray<FAnimControlTrackKey>& TrackKeys );

	/**
	 * Export Anim Track of the given SkeletalMeshComponent
	 */
	void ExportAnimTrack(class AMatineeActor* MatineeActor, USkeletalMeshComponent* SkeletalMeshComponent);

private:
	FFbxExporter();

	static TSharedPtr<FFbxExporter> StaticInstance;

	FbxManager* SdkManager;
	FbxScene* Scene;
	FbxAnimStack* AnimStack;
	FbxAnimLayer* AnimLayer;
	FbxCamera* DefaultCamera;
	
	FFbxDataConverter Converter;
	
	TMap<FString,int32> FbxNodeNameToIndexMap;
	TMap<const AActor*, FbxNode*> FbxActors;
	TMap<const USkeletalMeshComponent*, FbxNode*> FbxSkeletonRoots;
	TMap<const UMaterial*, FbxSurfaceMaterial*> FbxMaterials;
	TMap<const UStaticMesh*, FbxMesh*> FbxMeshes;

	/** The frames-per-second (FPS) used when baking transforms */
	static const float BakeTransformsFPS;
	
	/** Whether or not to export vertices unwelded */
	static bool bStaticMeshExportUnWeldedVerts;

	void ExportModel(UModel* Model, FbxNode* Node, const char* Name);
	
	/**
	 * Exports the basic information about an actor and buffers it.
	 * This function creates one FBX node for the actor with its placement.
	 */
	FbxNode* ExportActor(AActor* Actor, AMatineeActor* InMatineeActor, bool bExportComponents = false );
	
	/**
	 * Exports a static mesh
	 * @param StaticMesh	The static mesh to export
	 * @param MeshName		The name of the mesh for the FBX file
	 * @param FbxActor		The fbx node representing the mesh
	 * @param ExportLOD		The LOD of the mesh to export
	 * @param LightmapUVChannel Optional UV channel to export
	 * @param ColorBuffer	Vertex color overrides to export
	 * @param MaterialOrderOverride	Optional ordering of materials to set up correct material ID's across multiple meshes being export such as BSP surfaces which share common materials. Should be used sparingly
	 */
	FbxNode* ExportStaticMeshToFbx(const UStaticMesh* StaticMesh, int32 ExportLOD, const TCHAR* MeshName, FbxNode* FbxActor, int32 LightmapUVChannel = -1, const FColorVertexBuffer* ColorBuffer = NULL, const TArray<UMaterialInterface*>* MaterialOrderOverride = NULL);

	/**
	 * Exports a spline mesh
	 * @param SplineMeshComp	The spline mesh component to export
	 * @param MeshName		The name of the mesh for the FBX file
	 * @param FbxActor		The fbx node representing the mesh
	 */
	void ExportSplineMeshToFbx(const USplineMeshComponent* SplineMeshComp, const TCHAR* MeshName, FbxNode* FbxActor);

	/**
	 * Exports an instanced mesh
	 * @param InstancedMeshComp	The instanced mesh component to export
	 * @param MeshName		The name of the mesh for the FBX file
	 * @param FbxActor		The fbx node representing the mesh
	 */
	void ExportInstancedMeshToFbx(const UInstancedStaticMeshComponent* InstancedMeshComp, const TCHAR* MeshName, FbxNode* FbxActor);

	/**
	* Exports a landscape
	* @param Landscape		The landscape to export
	* @param MeshName		The name of the mesh for the FBX file
	* @param FbxActor		The fbx node representing the mesh
	*/
	void ExportLandscapeToFbx(ALandscapeProxy* Landscape, const TCHAR* MeshName, FbxNode* FbxActor, bool bSelectedOnly);

	/**
	 * Adds FBX skeleton nodes to the FbxScene based on the skeleton in the given USkeletalMesh, and fills
	 * the given array with the nodes created
	 */
	FbxNode* CreateSkeleton(const USkeletalMesh* SkelMesh, TArray<FbxNode*>& BoneNodes);

	/**
	 * Adds an Fbx Mesh to the FBX scene based on the data in the given FStaticLODModel
	 */
	FbxNode* CreateMesh(const USkeletalMesh* SkelMesh, const TCHAR* MeshName);

	/**
	 * Adds Fbx Clusters necessary to skin a skeletal mesh to the bones in the BoneNodes list
	 */
	void BindMeshToSkeleton(const USkeletalMesh* SkelMesh, FbxNode* MeshRootNode, TArray<FbxNode*>& BoneNodes);

	/**
	 * Add a bind pose to the scene based on the FbxMesh and skinning settings of the given node
	 */
	void CreateBindPose(FbxNode* MeshRootNode);

	/**
	 * Add the given skeletal mesh to the Fbx scene in preparation for exporting.  Makes all new nodes a child of the given node
	 */
	FbxNode* ExportSkeletalMeshToFbx(const USkeletalMesh* SkelMesh, const UAnimSequence* AnimSeq, const TCHAR* MeshName, FbxNode* ActorRootNode);

	/** Export SkeletalMeshComponent */
	void ExportSkeletalMeshComponent(USkeletalMeshComponent* SkelMeshComp, const TCHAR* MeshName, FbxNode* ActorRootNode);

	/**
	 * Add the given animation sequence as rotation and translation tracks to the given list of bone nodes
	 */
	void ExportAnimSequenceToFbx(const UAnimSequence* AnimSeq, const USkeletalMesh* SkelMesh, TArray<FbxNode*>& BoneNodes, FbxAnimLayer* AnimLayer,
		float AnimStartOffset, float AnimEndOffset, float AnimPlayRate, float StartTime);

	/** 
	 * The curve code doesn't differentiate between angles and other data, so an interpolation from 179 to -179
	 * will cause the bone to rotate all the way around through 0 degrees.  So here we make a second pass over the 
	 * rotation tracks to convert the angles into a more interpolation-friendly format.  
	 */
	void CorrectAnimTrackInterpolation( TArray<FbxNode*>& BoneNodes, FbxAnimLayer* AnimLayer );

	/**
	 * Exports the Matinee movement track into the FBX animation stack.
	 */
	void ExportMatineeTrackMove(FbxNode* FbxActor, UInterpTrackInstMove* MoveTrackInst, UInterpTrackMove* MoveTrack, float InterpLength);

	/**
	 * Exports the Matinee float property track into the FBX animation stack.
	 */
	void ExportMatineeTrackFloatProp(FbxNode* FbxActor, UInterpTrackFloatProp* PropTrack);

	/**
	 * Exports a given interpolation curve into the FBX animation curve.
	 */
	void ExportAnimatedVector(FbxAnimCurve* FbxCurve, const ANSICHAR* ChannelName, UInterpTrackMove* MoveTrack, UInterpTrackInstMove* MoveTrackInst, bool bPosCurve, int32 CurveIndex, bool bNegative, float InterpLength);
	
	/**
	 * Exports a movement subtrack to an FBX curve
	 */
	void ExportMoveSubTrack(FbxAnimCurve* FbxCurve, const ANSICHAR* ChannelName, UInterpTrackMoveAxis* SubTrack, UInterpTrackInstMove* MoveTrackInst, bool bPosCurve, int32 CurveIndex, bool bNegative, float InterpLength);
	
	void ExportAnimatedFloat(FbxProperty* FbxProperty, FInterpCurveFloat* Curve, bool IsCameraFoV);

	/**
	 * Finds the given actor in the already-exported list of structures
	 * @return FbxNode* the FBX node created from the UE4 actor
	 */
	FbxNode* FindActor(AActor* Actor);

	/**
	 * Find bone array of FbxNOdes of the given skeletalmeshcomponent  
	 */
	bool FindSkeleton(const USkeletalMeshComponent* SkelComp, TArray<FbxNode*>& BoneNodes);

	/** recursively get skeleton */
	void GetSkeleton(FbxNode* RootNode, TArray<FbxNode*>& BoneNodes);

	/**
	 * Exports the profile_COMMON information for a material.
	 */
	FbxSurfaceMaterial* ExportMaterial(UMaterial* Material);
	
	FbxSurfaceMaterial* CreateDefaultMaterial();
	
	/**
	 * Create user property in Fbx Node.
	 * Some Unreal animatable property can't be animated in FBX property. So create user property to record the animation of property.
	 *
	 * @param Node  FBX Node the property append to.
	 * @param Value Property value.
	 * @param Name  Property name.
	 * @param Label Property label.
	 */
	void CreateAnimatableUserProperty(FbxNode* Node, float Value, const char* Name, const char* Label);
};



} // namespace UnFbx
