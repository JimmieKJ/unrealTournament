// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*
* Copyright 2009 - 2010 Autodesk, Inc.  All Rights Reserved.
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

class ALight;
class UInterpGroupInst;
class AMatineeActor;
class AActor;
class UInterpTrackMove;

// Temporarily disable a few warnings due to virtual function abuse in FBX source files
#pragma warning( push )

#pragma warning( disable : 4263 ) // 'function' : member function does not override any base class virtual member function
#pragma warning( disable : 4264 ) // 'virtual_function' : no override available for virtual member function from base 'class'; function is hidden

// Include the fbx sdk header
// temp undef/redef of _O_RDONLY because kfbxcache.h (included by fbxsdk.h) does
// a weird use of these identifiers inside an enum.
#ifdef _O_RDONLY
#define TMP_UNFBX_BACKUP_O_RDONLY _O_RDONLY
#define TMP_UNFBX_BACKUP_O_WRONLY _O_WRONLY
#undef _O_RDONLY
#undef _O_WRONLY
#endif

//Robert G. : Packing was only set for the 64bits platform, but we also need it for 32bits.
//This was found while trying to trace a loop that iterate through all character links.
//The memory didn't match what the debugger displayed, obviously since the packing was not right.
#pragma pack(push,8)

#if PLATFORM_WINDOWS
// _CRT_SECURE_NO_DEPRECATE is defined but is not enough to suppress the deprecation
// warning for vsprintf and stricmp in VS2010.  Since FBX is able to properly handle the non-deprecated
// versions on the appropriate platforms, _CRT_SECURE_NO_DEPRECATE is temporarily undefined before
// including the FBX headers

// The following is a hack to make the FBX header files compile correctly under Visual Studio 2012 and Visual Studio 2013
#if _MSC_VER >= 1700
	#define FBX_DLL_MSC_VER 1600
#endif


#endif // PLATFORM_WINDOWS

#include <fbxsdk.h>

#include "TokenizedMessage.h"


#pragma pack(pop)



#ifdef TMP_UNFBX_BACKUP_O_RDONLY
#define _O_RDONLY TMP_FBX_BACKUP_O_RDONLY
#define _O_WRONLY TMP_FBX_BACKUP_O_WRONLY
#undef TMP_UNFBX_BACKUP_O_RDONLY
#undef TMP_UNFBX_BACKUP_O_WRONLY
#endif

#pragma warning( pop )

class FSkeletalMeshImportData;
class FSkelMeshOptionalImportData;
class ASkeletalMeshActor;
class UInterpTrackMoveAxis;
struct FbxSceneInfo;

DECLARE_LOG_CATEGORY_EXTERN(LogFbx, Log, All);

#define DEBUG_FBX_NODE( Prepend, FbxNode ) FPlatformMisc::LowLevelOutputDebugStringf( TEXT("%s %s\n"), ANSI_TO_TCHAR(Prepend), ANSI_TO_TCHAR( FbxNode->GetName() ) )

namespace UnFbx
{

struct FBXImportOptions
{
	// General options
	bool bImportMaterials;
	bool bInvertNormalMap;
	bool bImportTextures;
	bool bImportLOD;
	bool bUsedAsFullName;
	bool bConvertScene;
	bool bRemoveNameSpace;
	FVector ImportTranslation;
	FRotator ImportRotation;
	float ImportUniformScale;
	EFBXNormalImportMethod NormalImportMethod;
	// Static Mesh options
	bool bCombineToSingle;
	EVertexColorImportOption::Type VertexColorImportOption;
	FColor VertexOverrideColor;
	bool bRemoveDegenerates;
	bool bGenerateLightmapUVs;
	bool bOneConvexHullPerUCX;
	bool bAutoGenerateCollision;
	FName StaticMeshLODGroup;
	// Skeletal Mesh options
	bool bImportMorph;
	bool bImportAnimations;
	bool bUpdateSkeletonReferencePose;
	bool bResample;
	bool bImportRigidMesh;
	bool bUseT0AsRefPose;
	bool bPreserveSmoothingGroups;
	bool bKeepOverlappingVertices;
	bool bImportMeshesInBoneHierarchy;
	bool bCreatePhysicsAsset;
	UPhysicsAsset *PhysicsAsset;
	// Animation option
	USkeleton* SkeletonForAnimation;
	EFBXAnimationLengthImportType AnimationLengthImportType;
	struct FIntPoint AnimationRange;
	FString AnimationName;
	bool	bPreserveLocalTransform;
	bool	bDeleteExistingMorphTargetCurves;
	bool	bImportCustomAttribute;

	bool ShouldImportNormals()
	{
		return NormalImportMethod == FBXNIM_ImportNormals || NormalImportMethod == FBXNIM_ImportNormalsAndTangents;
	}

	bool ShouldImportTangents()
	{
		return NormalImportMethod == FBXNIM_ImportNormalsAndTangents;
	}

	void ResetForReimportAnimation()
	{
		bImportMorph = true;
		AnimationLengthImportType = FBXALIT_ExportedTime;
	}
};

struct FbxMeshInfo
{
	char* Name;
	int32 FaceNum;
	int32 VertexNum;
	bool bTriangulated;
	int32 MaterialNum;
	bool bIsSkelMesh;
	char* SkeletonRoot;
	int32 SkeletonElemNum;
	char* LODGroup;
	int32 LODLevel;
	int32 MorphNum;
};

struct FbxSceneInfo
{
	// data for static mesh
	int32 NonSkinnedMeshNum;
	
	//data for skeletal mesh
	int32 SkinnedMeshNum;

	// common data
	int32 TotalGeometryNum;
	int32 TotalMaterialNum;
	int32 TotalTextureNum;
	
	TArray<FbxMeshInfo> MeshInfo;
	
	// only one take supported currently
	char* TakeName;
	double FrameRate;
	double TotalTime;

};

/**
* FBX basic data conversion class.
*/
class FFbxDataConverter
{
public:

	FFbxDataConverter() 
	{}

	FVector ConvertPos(FbxVector4 Vector);
	FVector ConvertDir(FbxVector4 Vector);
	FRotator ConvertEuler( FbxDouble3 Euler );
	FVector ConvertScale(FbxDouble3 Vector);
	FVector ConvertScale(FbxVector4 Vector);
	FRotator ConvertRotation(FbxQuaternion Quaternion);
	FVector ConvertRotationToFVect(FbxQuaternion Quaternion, bool bInvertRot);
	FQuat ConvertRotToQuat(FbxQuaternion Quaternion);
	float ConvertDist(FbxDouble Distance);
	bool ConvertPropertyValue(FbxProperty& FbxProperty, UProperty& UnrealProperty, union UPropertyValue& OutUnrealPropertyValue);
	FTransform ConvertTransform(FbxAMatrix Matrix);
	FMatrix ConvertMatrix(FbxAMatrix Matrix);

	FbxVector4 ConvertToFbxPos(FVector Vector);
	FbxVector4 ConvertToFbxRot(FVector Vector);
	FbxVector4 ConvertToFbxScale(FVector Vector);
	FbxVector4 ConvertToFbxColor(FColor Color);
	FbxString	ConvertToFbxString(FName Name);
	FbxString	ConvertToFbxString(const FString& String);
};

FBXImportOptions* GetImportOptions( class FFbxImporter* FbxImporter, UFbxImportUI* ImportUI, bool bShowOptionDialog, const FString& FullPath, bool& OutOperationCanceled, bool& OutImportAll, bool bIsObjFormat, bool bForceImportType = false, EFBXImportType ImportType = FBXIT_StaticMesh );
void ApplyImportUIToImportOptions(UFbxImportUI* ImportUI, FBXImportOptions& InOutImportOptions);

struct FImportedMaterialData
{
public:
	void AddImportedMaterial( FbxSurfaceMaterial& FbxMaterial, UMaterialInterface& UnrealMaterial );
	bool IsUnique( FbxSurfaceMaterial& FbxMaterial, FName ImportedMaterialName ) const;
	UMaterialInterface* GetUnrealMaterial( const FbxSurfaceMaterial& FbxMaterial ) const;
	void Clear();
private:
	/** Mapping of FBX material to Unreal material.  Some materials in FBX have the same name so we use this map to determine if materials are unique */
	TMap<FbxSurfaceMaterial*, TWeakObjectPtr<UMaterialInterface> > FbxToUnrealMaterialMap;
	TSet<FName> ImportedMaterialNames;
};

/**
 * Main FBX Importer class.
 */
class FFbxImporter
{
public:
	~FFbxImporter();
	/**
	 * Returns the importer singleton. It will be created on the first request.
	 */
	UNREALED_API static FFbxImporter* GetInstance();
	static void DeleteInstance();

	/**
	 * Detect if the FBX file has skeletal mesh model. If there is deformer definition, then there is skeletal mesh.
	 * In this function, we don't need to import the scene. But the open process is time-consume if the file is large.
	 *
	 * @param InFilename	FBX file name. 
	 * @return int32 -1 if parse failed; 0 if geometry ; 1 if there are deformers; 2 otherwise
	 */
	int32 GetImportType(const FString& InFilename);

	/**
	 * Get detail infomation in the Fbx scene
	 *
	 * @param Filename Fbx file name
	 * @param SceneInfo return the scene info
	 * @return bool true if get scene info successfully
	 */
	bool GetSceneInfo(FString Filename, FbxSceneInfo& SceneInfo);
	

	/**
	 * Initialize Fbx file for import.
	 *
	 * @param Filename
	 * @param bParseStatistics
	 * @return bool
	 */
	bool OpenFile( FString Filename, bool bParseStatistics, bool bForSceneInfo = false );
	
	/**
	 * Import Fbx file.
	 *
	 * @param Filename
	 * @return bool
	 */
	bool ImportFile(FString Filename);
	
	/**
	 * Attempt to load an FBX scene from a given filename.
	 *
	 * @param Filename FBX file name to import.
	 * @returns true on success.
	 */
	UNREALED_API bool ImportFromFile(const FString& Filename, const FString& Type);

	/**
	 * Retrieve the FBX loader's error message explaining its failure to read a given FBX file.
	 * Note that the message should be valid even if the parser is successful and may contain warnings.
	 *
	 * @ return TCHAR*	the error message
	 */
	const TCHAR* GetErrorMessage() const
	{
		return *ErrorMessage;
	}

	/**
	 * Retrieve the object inside the FBX scene from the name
	 *
	 * @param ObjectName	Fbx object name
	 * @param Root	Root node, retrieve from it
	 * @return FbxNode*	Fbx object node
	 */
	FbxNode* RetrieveObjectFromName(const TCHAR* ObjectName, FbxNode* Root = NULL);

	/**
	 * Creates a static mesh with the given name and flags, imported from within the FBX scene.
	 * @param InParent
	 * @param Node	Fbx Node to import
	 * @param Name	the Unreal Mesh name after import
	 * @param Flags
	 * @param InStaticMesh	if LODIndex is not 0, this is the base mesh object. otherwise is NULL
	 * @param LODIndex	 LOD level to import to
	 *
	 * @returns UObject*	the UStaticMesh object.
	 */
	UNREALED_API UStaticMesh* ImportStaticMesh(UObject* InParent, FbxNode* Node, const FName& Name, EObjectFlags Flags, UFbxStaticMeshImportData* ImportData, UStaticMesh* InStaticMesh = NULL, int LODIndex = 0);

	/**
	* Creates a static mesh from all the meshes in FBX scene with the given name and flags.
	*
	* @param InParent
	* @param MeshNodeArray	Fbx Nodes to import
	* @param Name	the Unreal Mesh name after import
	* @param Flags
	* @param InStaticMesh	if LODIndex is not 0, this is the base mesh object. otherwise is NULL
	* @param LODIndex	 LOD level to import to
	*
	* @returns UObject*	the UStaticMesh object.
	*/
	UNREALED_API UStaticMesh* ImportStaticMeshAsSingle(UObject* InParent, TArray<FbxNode*>& MeshNodeArray, const FName InName, EObjectFlags Flags, UFbxStaticMeshImportData* TemplateImportData, UStaticMesh* InStaticMesh, int LODIndex = 0);

	void ImportStaticMeshSockets( UStaticMesh* StaticMesh );

	/**
	 * re-import Unreal static mesh from updated Fbx file
	 * if the Fbx mesh is in LODGroup, the LOD of mesh will be updated
	 *
	 * @param Mesh the original Unreal static mesh object
	 * @return UObject* the new Unreal mesh object
	 */
	UStaticMesh* ReimportStaticMesh(UStaticMesh* Mesh, UFbxStaticMeshImportData* TemplateImportData);
	
	/**
	* re-import Unreal skeletal mesh from updated Fbx file
	* If the Fbx mesh is in LODGroup, the LOD of mesh will be updated.
	* If the FBX mesh contains morph, the morph is updated.
	* Materials, textures and animation attached in the FBX mesh will not be updated.
	*
	* @param Mesh the original Unreal skeletal mesh object
	* @return UObject* the new Unreal mesh object
	*/
	USkeletalMesh* ReimportSkeletalMesh(USkeletalMesh* Mesh, UFbxSkeletalMeshImportData* TemplateImportData);

	/**
	 * Creates a skeletal mesh from Fbx Nodes with the given name and flags, imported from within the FBX scene.
	 * These Fbx Nodes bind to same skeleton. We need to bind them to one skeletal mesh.
	 *
	 * @param InParent
	 * @param NodeArray	Fbx Nodes to import
	 * @param Name	the Unreal Mesh name after import
	 * @param Flags
	 * @param Filename	Fbx file name
	 * @param FbxShapeArray	Fbx Morph objects.
	 * @param OutData - Optional import data to populate
	 * @param bCreateRenderData - Whether or not skeletal mesh rendering data will be created.
	 *
	 * @return The USkeletalMesh object created
	 */
	USkeletalMesh* ImportSkeletalMesh(UObject* InParent, TArray<FbxNode*>& NodeArray, const FName& Name, EObjectFlags Flags, UFbxSkeletalMeshImportData* TemplateImportData, FString Filename, TArray<FbxShape*> *FbxShapeArray=NULL, FSkeletalMeshImportData* OutData=NULL, bool bCreateRenderData = true );

	/**
	 * Add to the animation set, the animations contained within the FBX scene, for the given skeletal mesh
	 *
	 * @param Skeleton	Skeleton that the animation belong to
	 * @param SortedLinks	skeleton nodes which are sorted
	 * @param Filename	Fbx file name
	 * @param NodeArray node array of FBX meshes
	 */
	UAnimSequence* ImportAnimations(USkeleton* Skeleton, UObject* Outer, TArray<FbxNode*>& SortedLinks, const FString& Name, UFbxAnimSequenceImportData* TemplateImportData, TArray<FbxNode*>& NodeArray);

	/**
	 * Get Animation Time Span - duration of the animation
	 */
	FbxTimeSpan GetAnimationTimeSpan(FbxNode* RootNode, FbxAnimStack* AnimStack);

	/**
	 * Import one animation from CurAnimStack
	 *
	 * @param Skeleton	Skeleton that the animation belong to
	 * @param DestSeq 	Sequence it's overwriting data to
	 * @param Filename	Fbx file name	(not whole path)
	 * @param SortedLinks	skeleton nodes which are sorted
	 * @param NodeArray node array of FBX meshes
	 * @param CurAnimStack 	Animation Data
	 * @param ResampleRate	Resample Rate for data
	 * @param AnimTimeSpan	AnimTimeSpan
	 */
	bool ImportAnimation(USkeleton* Skeleton, UAnimSequence* DestSeq, const FString& FileName, TArray<FbxNode*>& SortedLinks, TArray<FbxNode*>& NodeArray, FbxAnimStack* CurAnimStack, const int32 ResampleRate, const FbxTimeSpan AnimTimeSpan);
	/**
	 * Calculate Max Sample Rate - separate out of the original ImportAnimations
	 *
	 * @param SortedLinks	skeleton nodes which are sorted
	 * @param NodeArray node array of FBX meshes
	 */
	int32 GetMaxSampleRate(TArray<FbxNode*>& SortedLinks, TArray<FbxNode*>& NodeArray);
	/**
	 * Validate Anim Stack - multiple check for validating animstack
	 *
	 * @param SortedLinks	skeleton nodes which are sorted
	 * @param NodeArray node array of FBX meshes
	 * @param CurAnimStack 	Animation Data
	 * @param ResampleRate	Resample Rate for data
	 * @param AnimTimeSpan	AnimTimeSpan	 
	 */	
	bool ValidateAnimStack(TArray<FbxNode*>& SortedLinks, TArray<FbxNode*>& NodeArray, FbxAnimStack* CurAnimStack, int32 ResampleRate, bool bImportMorph, FbxTimeSpan &AnimTimeSpan);

	/**
	 * Import Fbx Morph object for the Skeletal Mesh.
	 * In Fbx, morph object is a property of the Fbx Node.
	 *
	 * @param SkelMeshNodeArray - Fbx Nodes that the base Skeletal Mesh construct from
	 * @param BaseSkelMesh - base Skeletal Mesh
	 * @param Filename - Fbx file name
	 * @param LODIndex - LOD index
	 */
	void ImportFbxMorphTarget(TArray<FbxNode*> &SkelMeshNodeArray, USkeletalMesh* BaseSkelMesh, UObject* Parent, const FString& Filename, int32 LODIndex);

	/**
	 * Import LOD object for skeletal mesh
	 *
	 * @param InSkeletalMesh - LOD mesh object
	 * @param BaseSkeletalMesh - base mesh object
	 * @param DesiredLOD - LOD level
	 * @param bNeedToReregister - if true, re-register this skeletal mesh to shut down the skeletal mesh component that is previewing this mesh. 
									But you can set this to false when in the first loading before rendering this mesh for a performance issue 
	   @param ReregisterAssociatedComponents - if NULL, just re-registers all SkinnedMeshComponents but if you set the specific components, will only re-registers those components
	 */
	bool ImportSkeletalMeshLOD(USkeletalMesh* InSkeletalMesh, USkeletalMesh* BaseSkeletalMesh, int32 DesiredLOD, bool bNeedToReregister = true, TArray<UActorComponent*>* ReregisterAssociatedComponents = NULL);

	/**
	 * Empties the FBX scene, releasing its memory.
	 * Currently, we can't release KFbxSdkManager because Fbx Sdk2010.2 has a bug that FBX can only has one global sdkmanager.
	 * From Fbx Sdk2011, we can create multiple KFbxSdkManager, then we can release it.
	 */
	UNREALED_API void ReleaseScene();

	/**
	 * If the node model is a collision model, then fill it into collision model list
	 *
	 * @param Node Fbx node
	 * @return true if the node is a collision model
	 */
	bool FillCollisionModelList(FbxNode* Node);

	/**
	 * Import collision models for one static mesh if it has collision models
	 *
	 * @param StaticMesh - mesh object to import collision models
	 * @param NodeName - name of Fbx node that the static mesh constructed from
	 * @return return true if the static mesh has collision model and import successfully
	 */
	bool ImportCollisionModels(UStaticMesh* StaticMesh, const FbxString& NodeName);

	//help
	ANSICHAR* MakeName(const ANSICHAR* name);
	FName MakeNameForMesh(FString InName, FbxObject* FbxObject);

	// meshes
	
	/**
	* Get all Fbx skeletal mesh objects in the scene. these meshes are grouped by skeleton they bind to
	*
	* @param Node Root node to find skeletal meshes
	* @param outSkelMeshArray return Fbx meshes they are grouped by skeleton
	*/
	void FillFbxSkelMeshArrayInScene(FbxNode* Node, TArray< TArray<FbxNode*>* >& outSkelMeshArray, bool ExpandLOD);
	
	/**
	 * Find FBX meshes that match Unreal skeletal mesh according to the bone of mesh
	 *
	 * @param FillInMesh     Unreal skeletal mesh
	 * @param bExpandLOD     flag that if expand FBX LOD group when get the FBX node
	 * @param OutFBXMeshNodeArray  return FBX mesh nodes that match the Unreal skeletal mesh
	 * 
	 * @return the root bone that bind to the FBX skeletal meshes
	 */
	FbxNode* FindFBXMeshesByBone(const FName& RootBoneName, bool bExpandLOD, TArray<FbxNode*>& OutFBXMeshNodeArray);
	
	/**
	* Get mesh count (including static mesh and skeletal mesh, except collision models) and find collision models
	*
	* @param Node			Root node to find meshes
	* @param bCountLODs		Whether or not to count meshes in LOD groups
	* @return int32 mesh count
	*/
	int32 GetFbxMeshCount(FbxNode* Node,bool bCountLODs, int32 &OutNumLODGroups );
	
	/**
	* Get all Fbx mesh objects
	*
	* @param Node Root node to find meshes
	* @param outMeshArray return Fbx meshes
	*/
	UNREALED_API void FillFbxMeshArray(FbxNode* Node, TArray<FbxNode*>& outMeshArray, UnFbx::FFbxImporter* FFbxImporter);
	
	/**
	* Fill FBX skeletons to OutSortedLinks recursively
	*
	* @param Link Fbx node of skeleton root
	* @param OutSortedLinks
	*/
	void RecursiveBuildSkeleton(FbxNode* Link, TArray<FbxNode*>& OutSortedLinks);

	/**
	 * Fill FBX skeletons to OutSortedLinks
	 *
	 * @param ClusterArray Fbx clusters of FBX skeletal meshes
	 * @param OutSortedLinks
	 */
	void BuildSkeletonSystem(TArray<FbxCluster*>& ClusterArray, TArray<FbxNode*>& OutSortedLinks);

	/**
	 * Get Unreal skeleton root from the FBX skeleton node.
	 * Mesh and dummy can be used as skeleton.
	 *
	 * @param Link one FBX skeleton node
	 */
	FbxNode* GetRootSkeleton(FbxNode* Link);
	
	/**
	 * Get the object of import options
	 *
	 * @return FBXImportOptions
	 */
	UNREALED_API FBXImportOptions* GetImportOptions() const;

	/** helper function **/
	UNREALED_API static void DumpFBXNode(FbxNode* Node);

	/**
	 * Apply asset import settings for transform to an FBX node
	 *
	 * @param Node Node to apply transform settings too
	 * @param AssetData the asset data object to get transform data from
	 */
	void ApplyTransformSettingsToFbxNode(FbxNode* Node, UFbxAssetImportData* AssetData);

	/**
	 * Remove asset import settings for transform to an FBX node
	 *
	 * @param Node Node to apply transform settings too
	 * @param AssetData the asset data object to get transform data from
	 */
	void RemoveTransformSettingsFromFbxNode(FbxNode* Node, UFbxAssetImportData* AssetData);

	/**
	 * Populate the given matrix with the correct information for the asset data, in
	 * a format that matches FBX internals or without conversion
	 *
	 * @param OutMatrix The matrix to fill
	 * @param AssetData The asset data to extract the transform info from
	 */
	void BuildFbxMatrixForImportTransform(FbxAMatrix& OutMatrix, UFbxAssetImportData* AssetData);

private:
	/**
	 * ActorX plug-in can export mesh and dummy as skeleton.
	 * For the mesh and dummy in the skeleton hierarchy, convert them to FBX skeleton.
	 *
	 * @param Node          root skeleton node
	 * @param SkelMeshes    skeletal meshes that bind to this skeleton
	 * @param bImportNestedMeshes	if true we will import meshes nested in bone hierarchies instead of converting them to bones
	 */
	void RecursiveFixSkeleton(FbxNode* Node, TArray<FbxNode*> &SkelMeshes, bool bImportNestedMeshes );
	
	/**
	* Get all Fbx skeletal mesh objects which are grouped by skeleton they bind to
	*
	* @param Node Root node to find skeletal meshes
	* @param outSkelMeshArray return Fbx meshes they are grouped by skeleton
	* @param SkeletonArray
	* @param ExpandLOD flag of expanding LOD to get each mesh
	*/
	void RecursiveFindFbxSkelMesh(FbxNode* Node, TArray< TArray<FbxNode*>* >& outSkelMeshArray, TArray<FbxNode*>& SkeletonArray, bool ExpandLOD);
	
	/**
	* Get all Fbx rigid mesh objects which are grouped by skeleton hierarchy
	*
	* @param Node Root node to find skeletal meshes
	* @param outSkelMeshArray return Fbx meshes they are grouped by skeleton hierarchy
	* @param SkeletonArray
	* @param ExpandLOD flag of expanding LOD to get each mesh
	*/
	void RecursiveFindRigidMesh(FbxNode* Node, TArray< TArray<FbxNode*>* >& outSkelMeshArray, TArray<FbxNode*>& SkeletonArray, bool ExpandLOD);

	/**
	 * Import Fbx Morph object for the Skeletal Mesh.  Each morph target import processing occurs in a different thread 
	 *
	 * @param SkelMeshNodeArray - Fbx Nodes that the base Skeletal Mesh construct from
	 * @param BaseSkelMesh - base Skeletal Mesh
	 * @param Filename - Fbx file name
	 * @param LODIndex - LOD index of the skeletal mesh
	 */
	void ImportMorphTargetsInternal( TArray<FbxNode*>& SkelMeshNodeArray, USkeletalMesh* BaseSkelMesh, UObject* Parent, const FString& InFilename, int32 LODIndex );

	/**
	* sub-method called from ImportSkeletalMeshLOD method
	*
	* @param InSkeletalMesh - newly created mesh used as LOD
	* @param BaseSkeletalMesh - the destination mesh object 
	* @param DesiredLOD - the LOD index to import into. A new LOD entry is created if one doesn't exist
	*/
	void InsertNewLODToBaseSkeletalMesh(USkeletalMesh* InSkeletalMesh, USkeletalMesh* BaseSkeletalMesh, int32 DesiredLOD);

	/**
	* Method used to verify if the geometry is valid. For example, if the bounding box is tiny we should warn
	* @param StaticMesh - The imported static mesh which we'd like to verify
	*/
	void VerifyGeometry(UStaticMesh* StaticMesh);

public:
	// current Fbx scene we are importing. Make sure to release it after import
	FbxScene* Scene;
	FBXImportOptions* ImportOptions;

protected:
	enum IMPORTPHASE
	{
		NOTSTARTED,
		FILEOPENED,
		IMPORTED
	};
	
	static TSharedPtr<FFbxImporter> StaticInstance;

	struct FFbxMaterial
	{
		FbxSurfaceMaterial* FbxMaterial;
		UMaterialInterface* Material;

		FString GetName() const { return FbxMaterial ? ANSI_TO_TCHAR( FbxMaterial->GetName() ) : TEXT("None"); }
	};
	
	// scene management
	FFbxDataConverter Converter;
	FbxGeometryConverter* GeometryConverter;
	FbxManager* SdkManager;
	FbxImporter* Importer;
	IMPORTPHASE CurPhase;
	FString ErrorMessage;
	// base path of fbx file
	FString FileBasePath;
	TWeakObjectPtr<UObject> Parent;
	// Flag that the mesh is the first mesh to import in current FBX scene
	// FBX scene may contain multiple meshes, importer can import them at one time.
	// Initialized as true when start to import a FBX scene
	bool bFirstMesh;
	
	/**
	 * Collision model list. The key is fbx node name
	 * If there is an collision model with old name format, the key is empty string("").
	 */
	FbxMap<FbxString, TSharedPtr< FbxArray<FbxNode* > > > CollisionModels;
	 
	FFbxImporter();


	/**
	 * Set up the static mesh data from Fbx Mesh.
	 *
	 * @param FbxMesh  Fbx Mesh object
	 * @param StaticMesh Unreal static mesh object to fill data into
	 * @param LODIndex	LOD level to set up for StaticMesh
	 * @return bool true if set up successfully
	 */
	bool BuildStaticMeshFromGeometry(FbxMesh* Mesh, UStaticMesh* StaticMesh, TArray<FFbxMaterial>& MeshMaterials, int LODIndex,FRawMesh& RawMesh,
									 EVertexColorImportOption::Type VertexColorImportOption, const TMap<FVector, FColor>& ExistingVertexColorData, const FColor& VertexOverrideColor);
	
	/**
	 * Clean up for destroy the Importer.
	 */
	void CleanUp();

	/**
	* Compute the global matrix for Fbx Node
	*
	* @param Node	Fbx Node
	* @return KFbxXMatrix*	The global transform matrix
	*/
	FbxAMatrix ComputeTotalMatrix(FbxNode* Node);

	/**
	 * Check if there are negative scale in the transform matrix and its number is odd.
	 * @return bool True if there are negative scale and its number is 1 or 3. 
	 */
	bool IsOddNegativeScale(FbxAMatrix& TotalMatrix);

	// various actors, current the Fbx importer don't importe them
	/**
	 * Import Fbx light
	 *
	 * @param FbxLight fbx light object 
	 * @param World in which to create the light
	 * @return ALight*
	 */
	ALight* CreateLight(FbxLight* InLight, UWorld* InWorld );	
	/**
	* Import Light detail info
	*
	* @param FbxLight
	* @param UnrealLight
	* @return  bool
	*/
	bool FillLightComponent(FbxLight* Light, ULightComponent* UnrealLight);
	/**
	* Import Fbx Camera object
	*
	* @param FbxCamera Fbx camera object
	* @param World in which to create the camera
	* @return ACameraActor*
	*/
	ACameraActor* CreateCamera(FbxCamera* InCamera, UWorld* InWorld);

	// meshes
	/**
	* Fill skeletal mesh data from Fbx Nodes.  If this function needs to triangulate the mesh, then it could invalidate the
	* original FbxMesh pointer.  Hence FbxMesh is a reference so this function can set the new pointer if need be.  
	*
	* @param ImportData object to store skeletal mesh data
	* @param FbxMesh	Fbx mesh object belonging to Node
	* @param FbxSkin	Fbx Skin object belonging to FbxMesh
	* @param FbxShape	Fbx Morph object, if not NULL, we are importing a morph object.
	* @param SortedLinks    Fbx Links(bones) of this skeletal mesh
	* @param FbxMatList  All material names of the skeletal mesh
	*
	* @returns bool*	true if import successfully.
	*/
    bool FillSkelMeshImporterFromFbx(FSkeletalMeshImportData& ImportData, FbxMesh*& Mesh, FbxSkin* Skin, 
										FbxShape* Shape, TArray<FbxNode*> &SortedLinks, const TArray<FbxSurfaceMaterial*>& FbxMaterials );

	/**
	 * Import bones from skeletons that NodeArray bind to.
	 *
	 * @param NodeArray Fbx Nodes to import, they are bound to the same skeleton system
	 * @param ImportData object to store skeletal mesh data
	 * @param OutSortedLinks return all skeletons sorted by depth traversal
	 * @param bOutDiffPose
	 * @param bDisableMissingBindPoseWarning
	 * @param bUseTime0AsRefPose	in/out - Use Time 0 as Ref Pose 
	 */
	bool ImportBone(TArray<FbxNode*>& NodeArray, FSkeletalMeshImportData &ImportData, UFbxSkeletalMeshImportData* TemplateData, TArray<FbxNode*> &OutSortedLinks, bool& bOutDiffPose, bool bDisableMissingBindPoseWarning, bool & bUseTime0AsRefPose);
	
	/**
	 * Skins the control points of the given mesh or shape using either the default pose for skinning or the first frame of the
	 * default animation.  The results are saved as the last X verts in the given FSkeletalMeshBinaryImport
	 *
	 * @param SkelMeshImporter object to store skeletal mesh data
	 * @param FbxMesh	The Fbx mesh object with the control points to skin
	 * @param FbxShape	If a shape (aka morph) is provided, its control points will be used instead of the given meshes
	 * @param bUseT0	If true, then the pose at time=0 will be used instead of the ref pose
	 */
	void SkinControlPointsToPose(FSkeletalMeshImportData &ImportData, FbxMesh* Mesh, FbxShape* Shape, bool bUseT0 );
	
	// anims
	/**
	 * Check if the Fbx node contains animation
	 *
	 * @param Node Fbx node
	 * @return bool true if the Fbx node contains animation.
	 */
	//bool IsAnimated(FbxNode* Node);

	/**
	* Fill each Trace for AnimSequence with Fbx skeleton animation by key
	*
	* @param Node   Fbx skeleton node
	* @param AnimSequence
	* @param TakeName
	* @param bIsRoot if the Fbx skeleton node is root skeleton
	* @param Scale scale factor for this skeleton node
	*/
	bool FillAnimSequenceByKey(FbxNode* Node, UAnimSequence* AnimSequence, const char* TakeName, FbxTime& Start, FbxTime& End, bool bIsRoot, FbxVector4 Scale);
	/*bool CreateMatineeSkeletalAnimation(ASkeletalMeshActor* Actor, UAnimSet* AnimSet);
	bool CreateMatineeAnimation(FbxNode* Node, AActor* Actor, bool bInvertOrient, bool bAddDirectorTrack);*/


	// material
	/**
	 * Import each material Input from Fbx Material
	 *
	 * @param FbxMaterial	Fbx material object
	 * @param UnrealMaterial
	 * @param MaterialProperty The material component to import
	 * @param MaterialInput
	 * @param bSetupAsNormalMap
	 * @param UVSet
	 * @return bool	
	 */
	bool CreateAndLinkExpressionForMaterialProperty(	FbxSurfaceMaterial& FbxMaterial,
														UMaterial* UnrealMaterial,
														const char* MaterialProperty ,
														FExpressionInput& MaterialInput, 
														bool bSetupAsNormalMap,
														TArray<FString>& UVSet,
														const FVector2D& Location );
	/**
	 * Add a basic white diffuse color if no expression is linked to diffuse input.
	 *
	 * @param unMaterial Unreal material object.
	 */
	void FixupMaterial( FbxSurfaceMaterial& FbxMaterial, UMaterial* unMaterial);
	
	/**
	 * Get material mapping array according "Skinxx" flag in material name
	 *
	 * @param FSkeletalMeshBinaryImport& The unreal skeletal mesh.
	 */
	void SetMaterialSkinXXOrder(FSkeletalMeshImportData& ImportData);
	
	/**
	 * Create materials from Fbx node.
	 * Only setup channels that connect to texture, and setup the UV coordinate of texture.
	 * If diffuse channel has no texture, one default node will be created with constant.
	 *
	 * @param FbxNode  Fbx node
	 * @param outMaterials Unreal Materials we created
	 * @param UVSets UV set name list
	 * @return int32 material count that created from the Fbx node
	 */
	int32 CreateNodeMaterials(FbxNode* FbxNode, TArray<UMaterialInterface*>& outMaterials, TArray<FString>& UVSets);

	/**
	 * Make material Unreal asset name from the Fbx material
	 *
	 * @param FbxMaterial Material from the Fbx node
	 * @return Sanitized asset name
	 */
	FString GetMaterialFullName(FbxSurfaceMaterial& FbxMaterial);

	/**
	 * Create Unreal material from Fbx material.
	 * Only setup channels that connect to texture, and setup the UV coordinate of texture.
	 * If diffuse channel has no texture, one default node will be created with constant.
	 *
	 * @param KFbxSurfaceMaterial*  Fbx material
	 * @param outMaterials Unreal Materials we created
	 * @param outUVSets
	 */
	void CreateUnrealMaterial(FbxSurfaceMaterial& FbxMaterial, TArray<UMaterialInterface*>& OutMaterials, TArray<FString>& UVSets);
	
	/**
	 * Visit all materials of one node, import textures from materials.
	 *
	 * @param Node FBX node.
	 */
	void ImportTexturesFromNode(FbxNode* Node);
	
	/**
	 * Generate Unreal texture object from FBX texture.
	 *
	 * @param FbxTexture FBX texture object to import.
	 * @param bSetupAsNormalMap Flag to import this texture as normal map.
	 * @return UTexture* Unreal texture object generated.
	 */
	UTexture* ImportTexture(FbxFileTexture* FbxTexture, bool bSetupAsNormalMap);
	
	/**
	 *
	 *
	 * @param
	 * @return UMaterial*
	 */
	//UMaterial* GetImportedMaterial(KFbxSurfaceMaterial* pMaterial);

	/**
	* Check if the meshes in FBX scene contain smoothing group info.
	* It's enough to only check one of mesh in the scene because "Export smoothing group" option affects all meshes when export from DCC.
	* To ensure only check one time, use flag bFirstMesh to record if this is the first mesh to check.
	*
	* @param FbxMesh Fbx mesh to import
	*/
	void CheckSmoothingInfo(FbxMesh* FbxMesh);

	/**
	 * check if two faces belongs to same smoothing group
	 *
	 * @param ImportData
	 * @param Face1 one face of the skeletal mesh
	 * @param Face2 another face
	 * @return bool true if two faces belongs to same group
	 */
	bool FacesAreSmoothlyConnected( FSkeletalMeshImportData &ImportData, int32 Face1, int32 Face2 );

	/**
	 * Make un-smooth faces work.
	 *
	 * @param ImportData
	 * @return int32 number of points that added when process unsmooth faces
	*/
	int32 DoUnSmoothVerts(FSkeletalMeshImportData &ImportData);
	
	/**
	 * Merge all layers of one AnimStack to one layer.
	 *
	 * @param AnimStack     AnimStack which layers will be merged
	 * @param ResampleRate  resample rate for the animation
	 */
	void MergeAllLayerAnimation(FbxAnimStack* AnimStack, int32 ResampleRate);
	
	//
	// for matinee export
	//
public:
	/**
	 * Retrieves whether there are any unknown camera instances within the FBX document.
	 */
	UNREALED_API bool HasUnknownCameras( AMatineeActor* InMatineeActor ) const;
	
	/**
	 * Sets the camera creation flag. Call this function before the import in order to enforce
	 * the creation of FBX camera instances that do not exist in the current scene.
	 */
	inline void SetProcessUnknownCameras(bool bCreateMissingCameras)
	{
		bCreateUnknownCameras = bCreateMissingCameras;
	}
	
	/**
	 * Modifies the Matinee actor with the animations found in the FBX document.
	 * 
	 * @return	true, if sucessful
	 */
	UNREALED_API bool ImportMatineeSequence(AMatineeActor* InMatineeActor);


	/** Create a new asset from the package and objectname and class */
	static UObject* CreateAssetOfClass(UClass* AssetClass, FString ParentPackageName, FString ObjectName, bool bAllowReplace = false);

	/* Templated function to create an asset with given package and name */
	template< class T> 
	static T * CreateAsset( FString ParentPackageName, FString ObjectName, bool bAllowReplace = false )
	{
		return (T*)CreateAssetOfClass(T::StaticClass(), ParentPackageName, ObjectName, bAllowReplace);
	}

protected:
	bool bCreateUnknownCameras;
	
	/**
	 * Creates a Matinee group for a given actor within a given Matinee actor.
	 */
	UInterpGroupInst* CreateMatineeGroup(AMatineeActor* InMatineeActor, AActor* Actor, FString GroupName);
	/**
	 * Imports a FBX scene node into a Matinee actor group.
	 */
	float ImportMatineeActor(FbxNode* FbxNode, UInterpGroupInst* MatineeGroup);

	/**
	 * Imports an FBX transform curve into a movement subtrack
	 */
	void ImportMoveSubTrack( FbxAnimCurve* FbxCurve, int32 FbxDimension, UInterpTrackMoveAxis* SubTrack, int32 CurveIndex, bool bNegative, FbxAnimCurve* RealCurve, float DefaultVal );

	/**
	 * Imports a FBX animated element into a Matinee track.
	 */
	void ImportMatineeAnimated(FbxAnimCurve* FbxCurve, FInterpCurveVector& Curve, int32 CurveIndex, bool bNegative, FbxAnimCurve* RealCurve, float DefaultVal);
	/**
	 * Imports a FBX camera into properties tracks of a Matinee group for a camera actor.
	 */
	void ImportCamera(ACameraActor* Actor, UInterpGroupInst* MatineeGroup, FbxCamera* Camera);
	/**
	 * Imports a FBX animated value into a property track of a Matinee group.
	 */
	void ImportAnimatedProperty(float* Value, const TCHAR* ValueName, UInterpGroupInst* MatineeGroup, const float FbxValue, FbxProperty Property, bool bImportFOV = false, FbxCamera* Camera = NULL );
	/**
	 * Check if FBX node has transform animation (translation and rotation, not check scale animation)
	 */
	bool IsNodeAnimated(FbxNode* FbxNode, FbxAnimLayer* AnimLayer = NULL);

	/** 
	 * As movement tracks in Unreal cannot have differing interpolation modes for position & rotation,
	 * we consolidate the two modes here.
	 */
	void ConsolidateMovementTrackInterpModes(UInterpTrackMove* MovementTrack);

	/**
	 * Get Unreal Interpolation mode from FBX interpolation mode
	 */
	EInterpCurveMode GetUnrealInterpMode(FbxAnimCurveKey FbxKey);

	/**
	 * Fill up and verify bone names for animation 
	 */
	void FillAndVerifyBoneNames(USkeleton* Skeleton, TArray<FbxNode*>& SortedLinks, TArray<FName> & OutRawBoneNames, FString Filename);
	/**
	 * Is valid animation data
	 */
	bool IsValidAnimationData(TArray<FbxNode*>& SortedLinks, TArray<FbxNode*>& NodeArray, int32& ValidTakeCount);

	/**
	 * Retrieve pose array from bind pose
	 *
	 * Iterate through Scene:Poses, and find valid bind pose for NodeArray, and return those Pose if valid
	 *
	 */
	bool RetrievePoseFromBindPose(const TArray<FbxNode*>& NodeArray, FbxArray<FbxPose*> & PoseArray) const;

public:
	/** Import and set up animation related data from mesh **/
	void SetupAnimationDataFromMesh(USkeletalMesh * SkeletalMesh, UObject* InParent, TArray<FbxNode*>& NodeArray, UFbxAnimSequenceImportData* ImportData, const FString& Filename);

	/** error message handler */
	void AddTokenizedErrorMessage(TSharedRef<FTokenizedMessage> Error, FName FbxErrorName );
	void ClearTokenizedErrorMessages();
	void FlushToTokenizedErrorMessage(enum EMessageSeverity::Type Severity);

private:
	friend class FFbxLoggerSetter;

	// logger set/clear function
	class FFbxLogger * Logger;
	void SetLogger(class FFbxLogger * InLogger);
	void ClearLogger();

	FImportedMaterialData ImportedMaterialData;

private:
	/**
	 * Import FbxCurve to Curve
	 */
	bool ImportCurve(const FbxAnimCurve* FbxCurve, FFloatCurve * Curve, const FbxTimeSpan &AnimTimeSpan, const float ValueScale = 1.f) const;


	/**
	 * Import FbxCurve to anim sequence
	 */
	bool ImportCurveToAnimSequence(class UAnimSequence * TargetSequence, const FString& CurveName, const FbxAnimCurve* FbxCurve, int32 CurveFlags,const FbxTimeSpan AnimTimeSpan, const float ValueScale = 1.f) const;
};


/** message Logger for FBX. Saves all the messages and prints when it's destroyed */
class FFbxLogger
{
	FFbxLogger();
	~FFbxLogger();

	/** Error messages **/
	TArray<TSharedRef<FTokenizedMessage>> TokenizedErrorMessages;

	friend class FFbxImporter;
	friend class FFbxLoggerSetter;
};

/**
* This class is to make sure Logger isn't used by outside of purpose.
* We add this only top level of functions where it needs to be handled
* if the importer already has logger set, it won't set anymore
*/
class FFbxLoggerSetter
{
	class FFbxLogger Logger;
	FFbxImporter * Importer;

public:
	FFbxLoggerSetter(FFbxImporter * InImpoter)
		: Importer(InImpoter)
	{
		// if impoter doesn't have logger, sets it
		if(Importer->Logger == NULL)
		{
			Importer->SetLogger(&Logger);
		}
		else
		{
			// if impoter already has logger set
			// invalidated Importer to make sure it doesn't clear
			Importer = NULL;
		}
	}

	~FFbxLoggerSetter()
	{
		if(Importer)
		{
			Importer->ClearLogger();
		}
	}
};
} // namespace UnFbx


