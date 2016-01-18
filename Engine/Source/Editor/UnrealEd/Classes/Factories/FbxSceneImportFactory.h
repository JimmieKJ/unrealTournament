// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "FbxSceneImportFactory.generated.h"

class FFbxAttributeInfo : public TSharedFromThis<FFbxAttributeInfo>
{
public:
	FString Name;
	uint64 UniqueId;
	bool bImportAttribute;

	FFbxAttributeInfo()
		: Name(TEXT(""))
		, UniqueId(0)
		, bImportAttribute(true)
	{}

	virtual ~FFbxAttributeInfo() {}

	virtual UClass *GetType() = 0;
};

class FFbxMeshInfo : public FFbxAttributeInfo, public TSharedFromThis<FFbxMeshInfo>
{
public:
	int32 FaceNum;
	int32 VertexNum;
	bool bTriangulated;
	int32 MaterialNum;
	bool bIsSkelMesh;
	FString SkeletonRoot;
	int32 SkeletonElemNum;
	FString LODGroup;
	int32 LODLevel;
	int32 MorphNum;

	FFbxMeshInfo()
		: FaceNum(0)
		, VertexNum(0)
		, bTriangulated(false)
		, MaterialNum(0)
		, bIsSkelMesh(false)
		, SkeletonRoot(TEXT(""))
		, SkeletonElemNum(0)
		, LODGroup(TEXT(""))
		, LODLevel(0)
		, MorphNum(0)
	{}

	virtual ~FFbxMeshInfo() {}

	virtual UClass *GetType();
};

//Node use to store the scene hierarchy transform will be relative to the parent
class FFbxNodeInfo : public TSharedFromThis<FFbxNodeInfo>
{
public:
	FString NodeName;
	uint64 UniqueId;

	TSharedPtr<FFbxNodeInfo> ParentNodeInfo;
	TArray<TSharedPtr<FFbxNodeInfo>> Childrens;
	
	
	TSharedPtr<FFbxAttributeInfo> AttributeInfo;
	FString AttributeType;

	FTransform Transform;
	bool bImportNode;

	FFbxNodeInfo()
		: NodeName(TEXT(""))
		, UniqueId(0)
		, ParentNodeInfo(NULL)
		, AttributeInfo(NULL)
		, AttributeType(TEXT(""))
		, Transform(FTransform::Identity)
		, bImportNode(true)
	{}
};

class FFbxSceneInfo : public TSharedFromThis<FFbxSceneInfo>
{
public:
	// data for static mesh
	int32 NonSkinnedMeshNum;
	//data for skeletal mesh
	int32 SkinnedMeshNum;
	// common data
	int32 TotalGeometryNum;
	int32 TotalMaterialNum;
	int32 TotalTextureNum;
	TArray<TSharedPtr<FFbxMeshInfo>> MeshInfo;
	TArray<TSharedPtr<FFbxNodeInfo>> HierarchyInfo;
	/* true if it has animation */
	bool bHasAnimation;
	double FrameRate;
	double TotalTime;

	FFbxSceneInfo()
		: NonSkinnedMeshNum(0)
		, SkinnedMeshNum(0)
		, TotalGeometryNum(0)
		, TotalMaterialNum(0)
		, TotalTextureNum(0)
		, bHasAnimation(false)
		, FrameRate(0.0)
		, TotalTime(0.0)
	{}
};

namespace UnFbx
{
	struct FBXImportOptions;
}
	
typedef TMap<TSharedPtr<FFbxMeshInfo>, UnFbx::FBXImportOptions*> MeshInfoOverrideOptions;

UCLASS(hidecategories=Object)
class UNREALED_API UFbxSceneImportFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn) override;
	virtual UObject* FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;
	//~ Begin UFactory Interface

	/** Import options UI detail when importing fbx scene */
	UPROPERTY(Transient)
	class UFbxSceneImportOptions* SceneImportOptions;
	
	/** Import options UI detail when importing fbx scene static mesh*/
	UPROPERTY(Transient)
	class UFbxSceneImportOptionsStaticMesh* SceneImportOptionsStaticMesh;
	
	/** Import options UI detail when importing fbx scene skeletal mesh*/
	UPROPERTY(Transient)
	class UFbxSceneImportOptionsSkeletalMesh* SceneImportOptionsSkeletalMesh;
	
	/** Import options UI detail when importing fbx scene animation*/
	UPROPERTY(Transient)
	class UFbxSceneImportOptionsAnimation* SceneImportOptionsAnimation;
	
	/** Import options UI detail when importing fbx scene material*/
	UPROPERTY(Transient)
	class UFbxSceneImportOptionsMaterial* SceneImportOptionsMaterial;


	/** Import data used when importing static meshes */
	UPROPERTY(Transient)
	class UFbxStaticMeshImportData* StaticMeshImportData;

	/** Import data used when importing skeletal meshes */
	UPROPERTY(Transient)
	class UFbxSkeletalMeshImportData* SkeletalMeshImportData;

	/** Import data used when importing animations */
	UPROPERTY(Transient)
	class UFbxAnimSequenceImportData* AnimSequenceImportData;

	/** Import data used when importing textures */
	UPROPERTY(Transient)
	class UFbxTextureImportData* TextureImportData;
	
protected:
	/** Create a hierarchy of actor in the current level */
	void CreateLevelActorHierarchy(TSharedPtr<FFbxSceneInfo> SceneInfoPtr);

	/** Create a hierarchy of actor in the current level */
	void CreateActorComponentsHierarchy(TSharedPtr<FFbxSceneInfo> SceneInfoPtr);

	/** Apply the LocalTransform to the SceneComponent and if PreMultiplyTransform is not null do a pre multiplication
	* SceneComponent: Must be a valid pointer
	* LocalTransform: Must be a valid pointer
	* PreMultiplyTransform: Can be nullptr
	*/
	void ApplyTransformToComponent(USceneComponent *SceneComponent, FTransform *LocalTransform, FTransform *PreMultiplyTransform);

	/** This will add the scene transform options to the root node */
	void ApplySceneTransformOptionsToRootNode(TSharedPtr<FFbxSceneInfo> SceneInfoPtr);

	/** Import all skeletal mesh from the fbx scene */
	void ImportAllSkeletalMesh(void* VoidRootNodeToImport, void* VoidFbxImporter, EObjectFlags Flags, int32& NodeIndex, int32& InterestingNodeCount , TSharedPtr<FFbxSceneInfo> SceneInfo);

	/** Import all static mesh from the fbx scene */
	void ImportAllStaticMesh(void* VoidRootNodeToImport, void* VoidFbxImporter, EObjectFlags Flags, int32& NodeIndex, int32& InterestingNodeCount, TSharedPtr<FFbxSceneInfo> SceneInfo);

	// @todo document
	UObject* RecursiveImportNode(void* FFbxImporter, void* VoidNode, EObjectFlags Flags, int32& Index, int32 Total, TSharedPtr<FFbxSceneInfo> SceneInfo, FString PackagePath);

	// @todo document
	UObject* ImportANode(void* VoidFbxImporter, void* VoidNode, EObjectFlags Flags, int32& NodeIndex, TSharedPtr<FFbxSceneInfo> SceneInfo, TSharedPtr<FFbxNodeInfo> &OutNodeInfo, FString PackagePath, int32 Total = 0, UObject* InMesh = NULL, int LODIndex = 0);

	/** Find the FFbxNodeInfo in the hierarchy. */
	bool FindSceneNodeInfo(TSharedPtr<FFbxSceneInfo> SceneInfo, uint64 NodeInfoUniqueId, TSharedPtr<FFbxNodeInfo> &OutNodeInfo);

	/** Create a package for the specified node. Package will be the concatenation of UFbxSceneImportFactory::Path and Node->GetName(). */
	UPackage *CreatePackageForNode(FString PackageName, FString &StaticMeshName);

	/** The path of the asset to import */
	FString Path;
	
	/** Assets created by the factory*/
	TMap<TSharedPtr<FFbxAttributeInfo>, UObject*> AllNewAssets;

	/*Global Setting for non overriden Node*/
	UnFbx::FBXImportOptions* GlobalImportSettings;

	/*The Global Settings Reference*/
	UnFbx::FBXImportOptions* GlobalImportSettingsReference;
	
	/*Import UI override options*/
	MeshInfoOverrideOptions StaticMeshOverrideOptions;

	/** Is the import was cancel*/
	bool ImportWasCancel;
};



