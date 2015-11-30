// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "Factories.h"
#include "BusyCursor.h"
#include "SSkeletonWidget.h"
#include "Dialogs/DlgPickPath.h"

#include "AssetSelection.h"

#include "FbxImporter.h"
#include "FbxErrors.h"
#include "AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "Animation/SkeletalMeshActor.h"
#include "PackageTools.h"

#include "FbxSceneOptionWindow.h"
#include "MainFrame.h"

#include "Editor/KismetCompiler/Public/KismetCompilerModule.h"
#include "Kismet2/KismetEditorUtilities.h"

#define LOCTEXT_NAMESPACE "FBXSceneImportFactory"

using namespace UnFbx;

//////////////////////////////////////////////////////////////////////////
// TODO list
// -. When we export the material is set in the actor when we import the mesh,
//    but when we have a link mesh, we want to set the material for all actors.
// -. Set the combineMesh to true when importing a group of staticmesh
// -. Export correctly skeletal mesh, in fbxreview the skeletal mesh is not
//    playing is animation.
// -. Create a UI for the import scene options
// -. Create some tests
// -. Support for camera
// -. Support for light


bool GetFbxSceneImportOptions(UnFbx::FFbxImporter* FbxImporter
	, TSharedPtr<FFbxSceneInfo> SceneInfoPtr
	, UnFbx::FBXImportOptions *GlobalImportSettings
	, UFbxSceneImportOptions *SceneImportOptions
	, UFbxSceneImportOptionsStaticMesh *StaticMeshImportData
	, MeshInfoOverrideOptions &SceneMeshOverrideOptions
	, UFbxSceneImportOptionsSkeletalMesh *SkeletalMeshImportData
	, UFbxSceneImportOptionsAnimation *AnimSequenceImportData
	, UFbxSceneImportOptionsMaterial *TextureImportData
	, FString Path)
{
	//Make sure we don't put the global transform into the vertex position of the mesh
	GlobalImportSettings->bTransformVertexToAbsolute = false;
	//Avoid combining meshes
	GlobalImportSettings->bCombineToSingle = false;
	//Use the full name (avoid creating one) to let us retrieve node transform and attachment later
	GlobalImportSettings->bUsedAsFullName = true;
	//Make sure we import the textures
	GlobalImportSettings->bImportTextures = true;
	//Make sure Material get imported
	GlobalImportSettings->bImportMaterials = true;

	//TODO this options will be set by the fbxscene UI in the material options tab, it also should be save/load from config file
	//Prefix materials package name to put all material under Material folder (this avoid name clash with meshes)
	GlobalImportSettings->MaterialPrefixName = FName("/Materials/");

	TSharedPtr<SWindow> ParentWindow;
	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
		ParentWindow = MainFrame.GetParentWindow();
	}
	TSharedRef<SWindow> Window = SNew(SWindow)
		.ClientSize(FVector2D(800.f, 650.f))
		.Title(NSLOCTEXT("UnrealEd", "FBXSceneImportOpionsTitle", "FBX Scene Import Options"));
	TSharedPtr<SFbxSceneOptionWindow> FbxSceneOptionWindow;
	Window->SetContent
		(
			SAssignNew(FbxSceneOptionWindow, SFbxSceneOptionWindow)
			.SceneInfo(SceneInfoPtr)
			.GlobalImportSettings(GlobalImportSettings)
			.SceneImportOptionsDisplay(SceneImportOptions)
			.SceneImportOptionsStaticMeshDisplay(StaticMeshImportData)
			.SceneMeshOverrideOptions(&SceneMeshOverrideOptions)
			.SceneImportOptionsSkeletalMeshDisplay(SkeletalMeshImportData)
			.SceneImportOptionsAnimationDisplay(AnimSequenceImportData)
			.SceneImportOptionsMaterialDisplay(TextureImportData)
			.OwnerWindow(Window)
			.FullPath(FText::FromString(Path))
			);

	FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);

	if (!FbxSceneOptionWindow->ShouldImport())
	{
		return false;
	}

	//setup all options
	GlobalImportSettings->bImportStaticMeshLODs = SceneImportOptions->bImportStaticMeshLODs;
	GlobalImportSettings->bImportSkeletalMeshLODs = SceneImportOptions->bImportSkeletalMeshLODs;

	//Save the import options in ini config file
	SceneImportOptions->SaveConfig();

	//Save the Default setting copy them in the UObject and save them
	SFbxSceneOptionWindow::CopyFbxOptionsToStaticMeshOptions(GlobalImportSettings, StaticMeshImportData);
	StaticMeshImportData->SaveConfig();

	//TODO save all other options

	return true;
}

bool IsEmptyAttribute(FString AttributeType)
{
	return (AttributeType.Compare("eNull") == 0 || AttributeType.Compare("eUnknown") == 0);
}

// TODO we should replace the old UnFbx:: data by the new data that use shared pointer.
// For now we convert the old structure to the new one
TSharedPtr<FFbxSceneInfo> ConvertSceneInfo(UnFbx::FbxSceneInfo &SceneInfo)
{
	TSharedPtr<FFbxSceneInfo> SceneInfoPtr = MakeShareable(new FFbxSceneInfo());
	SceneInfoPtr->NonSkinnedMeshNum = SceneInfo.NonSkinnedMeshNum;
	SceneInfoPtr->SkinnedMeshNum = SceneInfo.SkinnedMeshNum;
	SceneInfoPtr->TotalGeometryNum = SceneInfo.TotalGeometryNum;
	SceneInfoPtr->TotalMaterialNum = SceneInfo.TotalMaterialNum;
	SceneInfoPtr->TotalTextureNum = SceneInfo.TotalTextureNum;
	SceneInfoPtr->bHasAnimation = SceneInfo.bHasAnimation;
	SceneInfoPtr->FrameRate = SceneInfo.FrameRate;
	SceneInfoPtr->TotalTime = SceneInfo.TotalTime;
	for (auto MeshInfoIt = SceneInfo.MeshInfo.CreateConstIterator(); MeshInfoIt; ++MeshInfoIt)
	{
		TSharedPtr<FFbxMeshInfo> MeshInfoPtr = MakeShareable(new FFbxMeshInfo());
		MeshInfoPtr->FaceNum = (*MeshInfoIt).FaceNum;
		MeshInfoPtr->VertexNum = (*MeshInfoIt).VertexNum;
		MeshInfoPtr->bTriangulated = (*MeshInfoIt).bTriangulated;
		MeshInfoPtr->MaterialNum = (*MeshInfoIt).MaterialNum;
		MeshInfoPtr->bIsSkelMesh = (*MeshInfoIt).bIsSkelMesh;
		MeshInfoPtr->SkeletonRoot = (*MeshInfoIt).SkeletonRoot;
		MeshInfoPtr->SkeletonElemNum = (*MeshInfoIt).SkeletonElemNum;
		MeshInfoPtr->LODGroup = (*MeshInfoIt).LODGroup;
		MeshInfoPtr->LODLevel = (*MeshInfoIt).LODLevel;
		MeshInfoPtr->MorphNum = (*MeshInfoIt).MorphNum;
		MeshInfoPtr->Name = (*MeshInfoIt).Name;
		MeshInfoPtr->UniqueId = (*MeshInfoIt).UniqueId;
		SceneInfoPtr->MeshInfo.Add(MeshInfoPtr);
	}
	for (auto NodeInfoIt = SceneInfo.HierarchyInfo.CreateConstIterator(); NodeInfoIt; ++NodeInfoIt)
	{
		const UnFbx::FbxNodeInfo &NodeInfo = (*NodeInfoIt);
		TSharedPtr<FFbxNodeInfo> NodeInfoPtr = MakeShareable(new FFbxNodeInfo());
		NodeInfoPtr->NodeName = NodeInfo.ObjectName;
		NodeInfoPtr->UniqueId = NodeInfo.UniqueId;
		NodeInfoPtr->AttributeType = NodeInfo.AttributeType;

		//Find the parent
		NodeInfoPtr->ParentNodeInfo = NULL;
		for (auto ParentIt = SceneInfoPtr->HierarchyInfo.CreateConstIterator(); ParentIt; ++ParentIt)
		{
			TSharedPtr<FFbxNodeInfo> ParentPtr = (*ParentIt);
			if (ParentPtr->UniqueId == NodeInfo.ParentUniqueId)
			{
				NodeInfoPtr->ParentNodeInfo = ParentPtr;
				ParentPtr->Childrens.Add(NodeInfoPtr);
				break;
			}
		}

		//Find the attribute info
		NodeInfoPtr->AttributeInfo = NULL;
		for (auto AttributeIt = SceneInfoPtr->MeshInfo.CreateConstIterator(); AttributeIt; ++AttributeIt)
		{
			TSharedPtr<FFbxMeshInfo> AttributePtr = (*AttributeIt);
			if (AttributePtr->UniqueId == NodeInfo.AttributeUniqueId)
			{
				NodeInfoPtr->AttributeInfo = AttributePtr;
				break;
			}
		}

		//Set the transform
		NodeInfoPtr->Transform = FTransform::Identity;
		FbxVector4 NewLocalT = NodeInfo.Transform.GetT();
		FbxVector4 NewLocalS = NodeInfo.Transform.GetS();
		FbxQuaternion NewLocalQ = NodeInfo.Transform.GetQ();
		NodeInfoPtr->Transform.SetTranslation(FFbxDataConverter::ConvertPos(NewLocalT));
		NodeInfoPtr->Transform.SetScale3D(FFbxDataConverter::ConvertScale(NewLocalS));
		NodeInfoPtr->Transform.SetRotation(FFbxDataConverter::ConvertRotToQuat(NewLocalQ));

		//by default we import all node
		NodeInfoPtr->bImportNode = true;

		//Add the node to the hierarchy
		SceneInfoPtr->HierarchyInfo.Add(NodeInfoPtr);
	}
	return SceneInfoPtr;
}

UClass *FFbxMeshInfo::GetType()
{
	if (bIsSkelMesh)
		return USkeletalMesh::StaticClass();
	return UStaticMesh::StaticClass();
}

UFbxSceneImportFactory::UFbxSceneImportFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UWorld::StaticClass();
	Formats.Add(TEXT("fbx;Fbx Scene"));

	bCreateNew = false;
	bText = false;
	bEditorImport = true;
	Path = "";
	ImportWasCancel = false;

	SceneImportOptions = CreateDefaultSubobject<UFbxSceneImportOptions>(TEXT("SceneImportOptions"), true);
	SceneImportOptionsStaticMesh = CreateDefaultSubobject<UFbxSceneImportOptionsStaticMesh>(TEXT("SceneImportOptionsStaticMesh"), true);
	SceneImportOptionsSkeletalMesh = CreateDefaultSubobject<UFbxSceneImportOptionsSkeletalMesh>(TEXT("SceneImportOptionsSkeletalMesh"), true);
	SceneImportOptionsAnimation = CreateDefaultSubobject<UFbxSceneImportOptionsAnimation>(TEXT("SceneImportOptionsAnimation"), true);
	SceneImportOptionsMaterial = CreateDefaultSubobject<UFbxSceneImportOptionsMaterial>(TEXT("SceneImportOptionsMaterial"), true);

	StaticMeshImportData = CreateDefaultSubobject<UFbxStaticMeshImportData>(TEXT("StaticMeshImportData"), true);
	SkeletalMeshImportData = CreateDefaultSubobject<UFbxSkeletalMeshImportData>(TEXT("SkeletalMeshImportData"), true);
	AnimSequenceImportData = CreateDefaultSubobject<UFbxAnimSequenceImportData>(TEXT("AnimSequenceImportData"), true);
	TextureImportData = CreateDefaultSubobject<UFbxTextureImportData>(TEXT("TextureImportData"), true);
}

UObject* UFbxSceneImportFactory::FactoryCreateBinary
(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	EObjectFlags		Flags,
	UObject*			Context,
	const TCHAR*		Type,
	const uint8*&		Buffer,
	const uint8*		BufferEnd,
	FFeedbackContext*	Warn,
	bool& bOutOperationCanceled
	)
{
	UObject* ReturnObject = FactoryCreateBinary(Class, InParent, Name, Flags, Context, Type, Buffer, BufferEnd, Warn);
	bOutOperationCanceled = ImportWasCancel;
	ImportWasCancel = false;
	return ReturnObject;
}

UObject* UFbxSceneImportFactory::FactoryCreateBinary
(
UClass*				Class,
UObject*			InParent,
FName				Name,
EObjectFlags		Flags,
UObject*			Context,
const TCHAR*		Type,
const uint8*&		Buffer,
const uint8*		BufferEnd,
FFeedbackContext*	Warn
)
{
	if (InParent == NULL)
	{
		return NULL;
	}

	UWorld* World = GWorld;
	ULevel* CurrentLevel = World->GetCurrentLevel();
	//We will call other factory store the filename value since UFactory::CurrentFilename is static
	FString FbxImportFileName = UFactory::CurrentFilename;
	// Unselect all actors.
	GEditor->SelectNone(false, false);

	FEditorDelegates::OnAssetPreImport.Broadcast(this, Class, InParent, Name, Type);
	
	//TODO verify if we really need this when instancing actor in a level from an import
	//In that case we should change the variable name.
	GEditor->IsImportingT3D = 1;
	GIsImportingT3D = GEditor->IsImportingT3D;

	// logger for all error/warnings
	// this one prints all messages that are stored in FFbxImporter
	UnFbx::FFbxImporter* FbxImporter = UnFbx::FFbxImporter::GetInstance();

	UnFbx::FFbxLoggerSetter Logger(FbxImporter);

	Warn->BeginSlowTask(NSLOCTEXT("FbxSceneFactory", "BeginImportingFbxSceneTask", "Importing FBX scene"), true);
	
	//Read the fbx and store the hierarchy's information so we can reuse it after importing all the model in the fbx file
	if (!FbxImporter->ImportFromFile(*FbxImportFileName, Type))
	{
		// Log the error message and fail the import.
		Warn->Log(ELogVerbosity::Error, FbxImporter->GetErrorMessage());
		FbxImporter->ReleaseScene();
		FbxImporter = NULL;
		// Mark us as no longer importing a T3D.
		GEditor->IsImportingT3D = 0;
		GIsImportingT3D = false;
		Warn->EndSlowTask();
		FEditorDelegates::OnAssetPostImport.Broadcast(this, World);
		return NULL;
	}

	FString PackageName = "";
	InParent->GetName(PackageName);
	Path = FPaths::GetPath(PackageName);

	UnFbx::FbxSceneInfo SceneInfo;
	//Read the scene and found all instance with their scene information.
	FbxImporter->GetSceneInfo(FbxImportFileName, SceneInfo);

	//Convert old structure to the new scene export structure
	TSharedPtr<FFbxSceneInfo> SceneInfoPtr = ConvertSceneInfo(SceneInfo);

	GlobalImportSettings = FbxImporter->GetImportOptions();
	if(!GetFbxSceneImportOptions(FbxImporter
		, SceneInfoPtr
		, GlobalImportSettings
		, SceneImportOptions
		, SceneImportOptionsStaticMesh
		, StaticMeshOverrideOptions
		, SceneImportOptionsSkeletalMesh
		, SceneImportOptionsAnimation
		, SceneImportOptionsMaterial
		, Path))
	{
		//User cancel the scene import
		ImportWasCancel = true;
		FbxImporter->ReleaseScene();
		FbxImporter = NULL;
		GlobalImportSettings = NULL;
		// Mark us as no longer importing a T3D.
		GEditor->IsImportingT3D = 0;
		GIsImportingT3D = false;
		Warn->EndSlowTask();
		FEditorDelegates::OnAssetPostImport.Broadcast(this, World);
		return NULL;
	}

	GlobalImportSettingsReference = new UnFbx::FBXImportOptions();
	SFbxSceneOptionWindow::CopyFbxOptionsToFbxOptions(GlobalImportSettings, GlobalImportSettingsReference);

	//Get the scene root node
	FbxNode* RootNodeToImport = NULL;
	RootNodeToImport = FbxImporter->Scene->GetRootNode();
	
	//Apply the options scene transform to the root node
	ApplySceneTransformOptionsToRootNode(SceneInfoPtr);

	// For animation and static mesh we assume there is at lease one interesting node by default
	int32 InterestingNodeCount = 1;

	AllNewAssets.Empty();

	int32 NodeIndex = 0;

	//////////////////////////////////////////////////////////////////////////
	// IMPORT ALL STATIC MESH
	ImportAllStaticMesh(RootNodeToImport, FbxImporter, Flags, NodeIndex, InterestingNodeCount, SceneInfoPtr);
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	//IMPORT ALL SKELETAL MESH
	ImportAllSkeletalMesh(RootNodeToImport, FbxImporter, Flags, NodeIndex, InterestingNodeCount, SceneInfoPtr);
	//////////////////////////////////////////////////////////////////////////

	UObject *ReturnObject = NULL;
	for (auto ItAsset = AllNewAssets.CreateIterator(); ItAsset; ++ItAsset)
	{
		UObject *AssetObject = ItAsset.Value();
		if (AssetObject && ReturnObject == NULL)
		{
			//Set the first import object as the return object to prevent false error from the caller of this factory
			ReturnObject = AssetObject;
		}
		if (AssetObject->IsA(UStaticMesh::StaticClass()) || AssetObject->IsA(USkeletalMesh::StaticClass()))
		{
			//Mark the mesh as modified so the render will draw the mesh correctly
			AssetObject->Modify();
			AssetObject->PostEditChange();
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// CREATE AND PLACE ACTOR
	// Instantiate all the scene hierarchy in the current level with link to previous created objects
	// go through the hierarchy and instantiate actor in the current level	
	switch (SceneImportOptions->HierarchyType)
	{
		case EFBXSceneOptionsCreateHierarchyType::FBXSOCHT_CreateLevelActors:
		{
			CreateLevelActorHierarchy(SceneInfoPtr);
		}
		break;
		case EFBXSceneOptionsCreateHierarchyType::FBXSOCHT_CreateActorComponents:
		case EFBXSceneOptionsCreateHierarchyType::FBXSOCHT_CreateBlueprint:
		{
			CreateActorComponentsHierarchy(SceneInfoPtr);
		}
		break;
	}

	//Release the FbxImporter 
	FbxImporter->ReleaseScene();
	FbxImporter = NULL;
	GlobalImportSettings = NULL;
	delete GlobalImportSettingsReference;
	GlobalImportSettingsReference = NULL;

	//TODO destroy the memory use by override options
	StaticMeshOverrideOptions.Empty();

	// Mark us as no longer importing a T3D.
	GEditor->IsImportingT3D = 0;
	GIsImportingT3D = false;

	Warn->EndSlowTask();
	FEditorDelegates::OnAssetPostImport.Broadcast(this, World);

	return ReturnObject;
}

void UFbxSceneImportFactory::ApplySceneTransformOptionsToRootNode(TSharedPtr<FFbxSceneInfo> SceneInfoPtr)
{
	for (TSharedPtr<FFbxNodeInfo> NodeInfo : SceneInfoPtr->HierarchyInfo)
	{
		if (NodeInfo->NodeName.Compare("RootNode") == 0)
		{
			FTransform SceneTransformAddition(SceneImportOptions->ImportRotation, SceneImportOptions->ImportTranslation, FVector(SceneImportOptions->ImportUniformScale));
			FTransform OutTransform = FTransform::Identity;
			//We apply the scene pre transform before the existing root node transform and store the result in the rootnode transform
			FTransform::Multiply(&OutTransform, &NodeInfo->Transform, &SceneTransformAddition);
			NodeInfo->Transform = OutTransform;
			return;
		}
	}
}

void UFbxSceneImportFactory::CreateLevelActorHierarchy(TSharedPtr<FFbxSceneInfo> SceneInfoPtr)
{
	FString FbxImportFileName = UFactory::CurrentFilename;
	FString FilenameBase = FPaths::GetBaseFilename(FbxImportFileName);
	EComponentMobility::Type MobilityType = SceneImportOptions->bImportAsDynamic ? EComponentMobility::Movable : EComponentMobility::Static;
	TMap<uint64, AActor *> NewActorNameMap;
	FTransform RootTransform = FTransform::Identity;
	bool bSelectActor = true;
	//////////////////////////////////////////////////////////////////////////
	// iterate the whole hierarchy and create all actors
	for (auto NodeIt = SceneInfoPtr->HierarchyInfo.CreateConstIterator(); NodeIt; ++NodeIt)
	{
		TSharedPtr<FFbxNodeInfo>  NodeInfo = (*NodeIt);
		FString NodeItName(NodeInfo->NodeName);
		if (NodeItName.Compare("RootNode") == 0)
		{
			RootTransform = NodeInfo->Transform;
			continue;
		}

		//Export only the node that are mark for export
		if (!NodeInfo->bImportNode)
		{
			continue;
		}
		//Find the asset that link with this node attribute
		UObject *AssetToPlace = (NodeInfo->AttributeInfo.IsValid() && AllNewAssets.Contains(NodeInfo->AttributeInfo)) ? AllNewAssets[NodeInfo->AttributeInfo] : nullptr;

		//create actor
		AActor *PlacedActor = NULL;
		if (AssetToPlace != NULL)
		{
			//Create an actor from the asset
			//default flag is RF_Transactional;
			PlacedActor = FActorFactoryAssetProxy::AddActorForAsset(AssetToPlace, bSelectActor);
		}
		else if (IsEmptyAttribute(NodeInfo->AttributeType) || NodeInfo->AttributeType.Compare("eMesh") == 0)
		{
			//Create an empty actor if the node is an empty attribute or the attribute is a mesh(static mesh or skeletal mesh) that was not export
			UActorFactory* Factory = GEditor->FindActorFactoryByClass(UActorFactoryEmptyActor::StaticClass());
			FAssetData EmptyActorAssetData = FAssetData(Factory->GetDefaultActorClass(FAssetData()));
			//This is a group create an empty actor that just have a transform
			UObject* EmptyActorAsset = EmptyActorAssetData.GetAsset();
			//Place an empty actor
			PlacedActor = FActorFactoryAssetProxy::AddActorForAsset(EmptyActorAsset, bSelectActor);
			USceneComponent* RootComponent = NewObject<USceneComponent>(PlacedActor, USceneComponent::GetDefaultSceneRootVariableName());
			RootComponent->Mobility = MobilityType;
			RootComponent->bVisualizeComponent = true;
			PlacedActor->SetRootComponent(RootComponent);
			PlacedActor->AddInstanceComponent(RootComponent);
			RootComponent->RegisterComponent();
		}
		else
		{
			//TODO log which fbx attribute we cannot create an actor from
		}

		if (PlacedActor != NULL)
		{
			//Rename the actor correctly
			//When importing a scene we don't want to change the actor name even if there is similar label already existing
			PlacedActor->SetActorLabel(NodeItName);

			USceneComponent* RootComponent = Cast<USceneComponent>(PlacedActor->GetRootComponent());
			if (RootComponent)
			{
				//Map the new actor name with the old name in case the name is changing
				NewActorNameMap.Add(NodeInfo->UniqueId, PlacedActor);
				uint64 ParentUniqueId = NodeInfo->ParentNodeInfo.IsValid() ? NodeInfo->ParentNodeInfo->UniqueId : 0;
				AActor *ParentActor = NULL;
				//If there is a parent we must set the parent actor
				if (NewActorNameMap.Contains(ParentUniqueId))
				{
					ParentActor = *NewActorNameMap.Find(ParentUniqueId);
					if (ParentActor != NULL)
					{
						USceneComponent* ParentRootComponent = Cast<USceneComponent>(ParentActor->GetRootComponent());
						if (ParentRootComponent)
						{
							//Set the mobility when adding the first child
							if (ParentRootComponent->AttachChildren.Num() == 0)
							{
								//Make sure the mobility of the parent is the same of the child
								ParentRootComponent->Mobility = RootComponent->Mobility;
							}

							//Set the parent only if the mobility is matching we cannot mix dynamic and static in the scene hierarchy
							if (ParentRootComponent->Mobility == RootComponent->Mobility)
							{
								if (GEditor->CanParentActors(ParentActor, PlacedActor))
								{
									GEditor->ParentActors(ParentActor, PlacedActor, NAME_None);
								}
							}
						}
					}
				}
				//Apply the hierarchy local transform to the root component
				ApplyTransformToComponent(RootComponent, &(NodeInfo->Transform), ParentActor == nullptr ? &RootTransform : nullptr);
			}
		}
		//We select only the first actor
		bSelectActor = false;
	}
	// End of iteration of the hierarchy
	//////////////////////////////////////////////////////////////////////////
}

void UFbxSceneImportFactory::CreateActorComponentsHierarchy(TSharedPtr<FFbxSceneInfo> SceneInfoPtr)
{
	FString FbxImportFileName = UFactory::CurrentFilename;
	UBlueprint *NewBluePrintActor = NULL;
	AActor *RootActorContainer = NULL;
	FString FilenameBase = FPaths::GetBaseFilename(FbxImportFileName);
	USceneComponent *ActorRootComponent = NULL;
	TMap<uint64, USceneComponent *> NewSceneComponentNameMap;
	EComponentMobility::Type MobilityType = SceneImportOptions->bImportAsDynamic ? EComponentMobility::Movable : EComponentMobility::Static;
	
	//////////////////////////////////////////////////////////////////////////
	//Create the Actor where to put components in
	UActorFactory* Factory = GEditor->FindActorFactoryByClass(UActorFactoryEmptyActor::StaticClass());
	FAssetData EmptyActorAssetData = FAssetData(Factory->GetDefaultActorClass(FAssetData()));
	//This is a group create an empty actor that just have a transform
	UObject* EmptyActorAsset = EmptyActorAssetData.GetAsset();
	//Place an empty actor
	RootActorContainer = FActorFactoryAssetProxy::AddActorForAsset(EmptyActorAsset, true);
	check(RootActorContainer != NULL);
	ActorRootComponent = NewObject<USceneComponent>(RootActorContainer, USceneComponent::GetDefaultSceneRootVariableName());
	check(ActorRootComponent != NULL);
	ActorRootComponent->Mobility = MobilityType;
	ActorRootComponent->bVisualizeComponent = true;
	RootActorContainer->SetRootComponent(ActorRootComponent);
	RootActorContainer->AddInstanceComponent(ActorRootComponent);
	ActorRootComponent->RegisterComponent();
	RootActorContainer->SetActorLabel(FilenameBase);

	//////////////////////////////////////////////////////////////////////////
	// iterate the whole hierarchy and create all component
	FTransform RootTransform = FTransform::Identity;
	for (auto NodeIt = SceneInfoPtr->HierarchyInfo.CreateConstIterator(); NodeIt; ++NodeIt)
	{
		TSharedPtr<FFbxNodeInfo>  NodeInfo = (*NodeIt);
		FString NodeItName(NodeInfo->NodeName);
		//Set the root transform if its the root node and skip the node
		//The root transform will be use for every node under the root node
		if (NodeItName.Compare("RootNode") == 0)
		{
			RootTransform = NodeInfo->Transform;
			continue;
		}
		//Export only the node that are mark for export
		if (!NodeInfo->bImportNode)
		{
			continue;
		}
		//Find the asset that link with this node attribute
		UObject *AssetToPlace = (NodeInfo->AttributeInfo.IsValid() && AllNewAssets.Contains(NodeInfo->AttributeInfo)) ? AllNewAssets[NodeInfo->AttributeInfo] : nullptr;

		//Create the component where the type depend on the asset point by the component
		//In case there is no asset we create a SceneComponent
		USceneComponent* SceneComponent = NULL;
		if (AssetToPlace != NULL)
		{
			if (AssetToPlace->GetClass() == UStaticMesh::StaticClass())
			{
				//Component will be rename later
				UStaticMeshComponent* StaticMeshComponent = NewObject<UStaticMeshComponent>(RootActorContainer, NAME_None);
				StaticMeshComponent->SetStaticMesh(Cast<UStaticMesh>(AssetToPlace));
				StaticMeshComponent->DepthPriorityGroup = SDPG_World;
				SceneComponent = StaticMeshComponent;
				SceneComponent->Mobility = MobilityType;
			}
			else if (AssetToPlace->GetClass() == USkeletalMesh::StaticClass())
			{
				//Component will be rename later
				USkeletalMeshComponent* SkeletalMeshComponent = NewObject<USkeletalMeshComponent>(RootActorContainer, NAME_None);
				SkeletalMeshComponent->SetSkeletalMesh(Cast<USkeletalMesh>(AssetToPlace));
				SkeletalMeshComponent->DepthPriorityGroup = SDPG_World;
				SceneComponent = SkeletalMeshComponent;
				SceneComponent->Mobility = MobilityType;
			}
		}
		else if (IsEmptyAttribute(NodeInfo->AttributeType) || NodeInfo->AttributeType.Compare("eMesh") == 0)
		{
			//Component will be rename later
			SceneComponent = NewObject<USceneComponent>(RootActorContainer, NAME_None);
			SceneComponent->Mobility = MobilityType;
		}
		else
		{
			//TODO log which fbx attribute we cannot create an actor from
		}

		//////////////////////////////////////////////////////////////////////////
		//Make sure scenecomponent name are unique in the hierarchy of the outer
		FString NewUniqueName = NodeItName;
		if (!SceneComponent->Rename(*NewUniqueName, nullptr, REN_Test))
		{
			NewUniqueName = MakeUniqueObjectName(RootActorContainer, USceneComponent::StaticClass(), FName(*NodeItName)).ToString();
		}
		SceneComponent->Rename(*NewUniqueName);

		//Add the component to the owner actor and register it
		RootActorContainer->AddInstanceComponent(SceneComponent);
		SceneComponent->RegisterComponent();

		//Add the component to the temporary map so we can retrieve it later when we search for parent
		NewSceneComponentNameMap.Add(NodeInfo->UniqueId, SceneComponent);

		//Find the parent component by unique ID and attach(as child) the newly created scenecomponent
		//Attach the component to the rootcomponent if we dont find any parent component
		uint64 ParentUniqueId = NodeInfo->ParentNodeInfo.IsValid() ? NodeInfo->ParentNodeInfo->UniqueId : 0;
		USceneComponent *ParentRootComponent = NULL;
		if (NewSceneComponentNameMap.Contains(ParentUniqueId))
		{
			ParentRootComponent = *NewSceneComponentNameMap.Find(ParentUniqueId);
			if (ParentRootComponent != NULL)
			{
				SceneComponent->AttachTo(ParentRootComponent, NAME_None, EAttachLocation::KeepWorldPosition);
			}
		}
		else
		{
			SceneComponent->AttachTo(ActorRootComponent, NAME_None, EAttachLocation::KeepWorldPosition);
		}

		//Apply the local transform to the scene component
		ApplyTransformToComponent(SceneComponent, &(NodeInfo->Transform), ParentRootComponent != nullptr ? nullptr : &RootTransform);
	}
	// End of iteration of the hierarchy
	//////////////////////////////////////////////////////////////////////////

	//If the user want to export to a BP replace the container actor with a BP link
	if (SceneImportOptions->HierarchyType == EFBXSceneOptionsCreateHierarchyType::FBXSOCHT_CreateBlueprint && RootActorContainer != NULL)
	{
		//The location+name of the BP is the user select content path + fbx base filename
		FString FullnameBP = Path + TEXT("/") + FilenameBase;
		//Create the blueprint from the actor and replace the actor with a blueprintactor that point on the blueprint
		FKismetEditorUtilities::CreateBlueprintFromActor(FullnameBP, RootActorContainer, true);
	}
}

void UFbxSceneImportFactory::ApplyTransformToComponent(USceneComponent *SceneComponent, FTransform *LocalTransform, FTransform *PreMultiplyTransform)
{
	//In case there is no parent we must multiply the root transform
	if (PreMultiplyTransform != nullptr)
	{
		FTransform OutTransform = FTransform::Identity;
		FTransform::Multiply(&OutTransform, LocalTransform, PreMultiplyTransform);
		SceneComponent->SetRelativeTransform(OutTransform);
	}
	else
	{
		SceneComponent->SetRelativeTransform(*LocalTransform);
	}
}
void UFbxSceneImportFactory::ImportAllSkeletalMesh(void* VoidRootNodeToImport, void* VoidFbxImporter, EObjectFlags Flags, int32& NodeIndex, int32& InterestingNodeCount, TSharedPtr<FFbxSceneInfo> SceneInfo)
{
	UnFbx::FFbxImporter* FbxImporter = (UnFbx::FFbxImporter*)VoidFbxImporter;
	FbxNode *RootNodeToImport = (FbxNode *)VoidRootNodeToImport;
	InterestingNodeCount = 1;
	TArray< TArray<FbxNode*>* > SkelMeshArray;
	FbxImporter->FillFbxSkelMeshArrayInScene(RootNodeToImport, SkelMeshArray, false);
	InterestingNodeCount = SkelMeshArray.Num();

	int32 TotalNumNodes = 0;

	for (int32 i = 0; i < SkelMeshArray.Num(); i++)
	{
		UObject* NewObject = NULL;
		TArray<FbxNode*> NodeArray = *SkelMeshArray[i];
		UPackage* Pkg = NULL;
		TotalNumNodes += NodeArray.Num();
		if (TotalNumNodes > 0)
		{
			FbxNode* RootNodeArrayNode = NodeArray[0];
			TSharedPtr<FFbxNodeInfo> RootNodeInfo;
			if (!FindSceneNodeInfo(SceneInfo, RootNodeArrayNode->GetUniqueID(), RootNodeInfo))
			{
				continue;
			}
			if (!RootNodeInfo->AttributeInfo.IsValid() || RootNodeInfo->AttributeInfo->GetType() != USkeletalMesh::StaticClass())
			{
				continue;
			}
		}
		// check if there is LODGroup for this skeletal mesh
		int32 MaxLODLevel = 1;
		for (int32 j = 0; j < NodeArray.Num(); j++)
		{
			FbxNode* Node = NodeArray[j];
			if (Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGroup)
			{
				// get max LODgroup level
				if (MaxLODLevel < Node->GetChildCount())
				{
					MaxLODLevel = Node->GetChildCount();
				}
			}
		}
		int32 LODIndex;
		for (LODIndex = 0; LODIndex < MaxLODLevel; LODIndex++)
		{
			TArray<FbxNode*> SkelMeshNodeArray;
			for (int32 j = 0; j < NodeArray.Num(); j++)
			{
				FbxNode* Node = NodeArray[j];
				if (Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGroup)
				{
					if (Node->GetChildCount() > LODIndex)
					{
						SkelMeshNodeArray.Add(Node->GetChild(LODIndex));
					}
					else // in less some LODGroups have less level, use the last level
					{
						SkelMeshNodeArray.Add(Node->GetChild(Node->GetChildCount() - 1));
					}
				}
				else
				{
					SkelMeshNodeArray.Add(Node);
				}
			}

			if (LODIndex == 0 && SkelMeshNodeArray.Num() != 0)
			{
				FName OutputName = FbxImporter->MakeNameForMesh(SkelMeshNodeArray[0]->GetName(), SkelMeshNodeArray[0]);
				FString PackageName = Path + TEXT("/") + OutputName.ToString();
				FString SkeletalMeshName;
				Pkg = CreatePackageForNode(PackageName, SkeletalMeshName);
				if (Pkg == NULL)
					break;
				FName SkeletalMeshFName = FName(*SkeletalMeshName);
				USkeletalMesh* NewMesh = FbxImporter->ImportSkeletalMesh(Pkg, SkelMeshNodeArray, SkeletalMeshFName, Flags, SkeletalMeshImportData);
				NewObject = NewMesh;
				if (NewMesh)
				{
					TSharedPtr<FFbxNodeInfo> SkelMeshNodeInfo;
					if (FindSceneNodeInfo(SceneInfo, SkelMeshNodeArray[0]->GetUniqueID(), SkelMeshNodeInfo) && SkelMeshNodeInfo.IsValid() && SkelMeshNodeInfo->AttributeInfo.IsValid())
					{
						AllNewAssets.Add(SkelMeshNodeInfo->AttributeInfo, NewObject);
					}
					// We need to remove all scaling from the root node before we set up animation data.
					// Othewise some of the global transform calculations will be incorrect.
					FbxImporter->RemoveTransformSettingsFromFbxNode(RootNodeToImport, SkeletalMeshImportData);
					FbxImporter->SetupAnimationDataFromMesh(NewMesh, Pkg, SkelMeshNodeArray, AnimSequenceImportData, OutputName.ToString());

					// Reapply the transforms for the rest of the import
					FbxImporter->ApplyTransformSettingsToFbxNode(RootNodeToImport, SkeletalMeshImportData);
				}
			}
			else if (NewObject) // the base skeletal mesh is imported successfully
			{
				USkeletalMesh* BaseSkeletalMesh = Cast<USkeletalMesh>(NewObject);
				FName LODObjectName = NAME_None;
				USkeletalMesh *LODObject = FbxImporter->ImportSkeletalMesh(GetTransientPackage(), SkelMeshNodeArray, LODObjectName, RF_NoFlags, SkeletalMeshImportData);
				bool bImportSucceeded = FbxImporter->ImportSkeletalMeshLOD(LODObject, BaseSkeletalMesh, LODIndex, false);

				if (bImportSucceeded)
				{
					BaseSkeletalMesh->LODInfo[LODIndex].ScreenSize = 1.0f / (MaxLODLevel * LODIndex);
				}
				else
				{
					FbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, LOCTEXT("FailedToImport_SkeletalMeshLOD", "Failed to import Skeletal mesh LOD.")), FFbxErrors::SkeletalMesh_LOD_FailedToImport);
				}
			}

			// import morph target
			if (NewObject && SkeletalMeshImportData->bImportMorphTargets)
			{
				if (Pkg == NULL)
					continue;
				// TODO: Disable material importing when importing morph targets
				FbxImporter->ImportFbxMorphTarget(SkelMeshNodeArray, Cast<USkeletalMesh>(NewObject), Pkg, LODIndex);
			}
		}

		if (NewObject)
		{
			NodeIndex++;
		}
	}

	for (int32 i = 0; i < SkelMeshArray.Num(); i++)
	{
		delete SkelMeshArray[i];
	}

	// if total nodes we found is 0, we didn't find anything. 
	if (TotalNumNodes == 0)
	{
		FbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, LOCTEXT("FailedToImport_NoMeshFoundOnRoot", "Could not find any valid mesh on the root hierarchy. If you have mesh in the sub hierarchy, please enable option of [Import Meshes In Bone Hierarchy] when import.")),
			FFbxErrors::SkeletalMesh_NoMeshFoundOnRoot);
	}
}

void UFbxSceneImportFactory::ImportAllStaticMesh(void* VoidRootNodeToImport, void* VoidFbxImporter, EObjectFlags Flags, int32& NodeIndex, int32& InterestingNodeCount, TSharedPtr<FFbxSceneInfo> SceneInfo)
{
	UnFbx::FFbxImporter* FbxImporter = (UnFbx::FFbxImporter*)VoidFbxImporter;
	FbxNode *RootNodeToImport = (FbxNode *)VoidRootNodeToImport;
	
	//Copy default options to StaticMeshImportData
	SFbxSceneOptionWindow::CopyFbxOptionsToStaticMeshOptions(GlobalImportSettingsReference, SceneImportOptionsStaticMesh);
	SceneImportOptionsStaticMesh->FillStaticMeshInmportData(StaticMeshImportData, SceneImportOptions);

	FbxImporter->ApplyTransformSettingsToFbxNode(RootNodeToImport, StaticMeshImportData);

	// count meshes in lod groups if we don't care about importing LODs
	int32 NumLODGroups = 0;
	bool bCountLODGroupMeshes = !GlobalImportSettingsReference->bImportStaticMeshLODs;
	InterestingNodeCount = FbxImporter->GetFbxMeshCount(RootNodeToImport, bCountLODGroupMeshes, NumLODGroups);

	int32 ImportedMeshCount = 0;
	UStaticMesh* NewStaticMesh = NULL;
	UObject* Object = RecursiveImportNode(FbxImporter, RootNodeToImport, Flags, NodeIndex, InterestingNodeCount, SceneInfo, Path);

	NewStaticMesh = Cast<UStaticMesh>(Object);

	// Make sure to notify the asset registry of all assets created other than the one returned, which will notify the asset registry automatically.
	for (auto AssetItKvp = AllNewAssets.CreateIterator(); AssetItKvp; ++AssetItKvp)
	{
		UObject* Asset = AssetItKvp.Value();
		if (Asset != NewStaticMesh)
		{
			FAssetRegistryModule::AssetCreated(Asset);
			Asset->MarkPackageDirty();
		}
	}
	ImportedMeshCount = AllNewAssets.Num();
	if (ImportedMeshCount == 1 && NewStaticMesh)
	{
		FbxImporter->ImportStaticMeshSockets(NewStaticMesh);
	}
}

// @todo document
UObject* UFbxSceneImportFactory::RecursiveImportNode(void* VoidFbxImporter, void* VoidNode, EObjectFlags Flags, int32& NodeIndex, int32 Total, TSharedPtr<FFbxSceneInfo>  SceneInfo, FString PackagePath)
{
	UObject* NewObject = NULL;
	TSharedPtr<FFbxNodeInfo> OutNodeInfo;
	UnFbx::FFbxImporter* FFbxImporter = (UnFbx::FFbxImporter*)VoidFbxImporter;

	FbxNode* Node = (FbxNode*)VoidNode;

	if (Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGroup && Node->GetChildCount() > 0)
	{
		// import base mesh
		NewObject = ImportANode(VoidFbxImporter, Node->GetChild(0), Flags, NodeIndex, SceneInfo, OutNodeInfo, PackagePath, Total);

		if (NewObject)
		{
			//We should always have a valid attribute if we just create a new asset
			check(OutNodeInfo.IsValid() && OutNodeInfo->AttributeInfo.IsValid());

			AllNewAssets.Add(OutNodeInfo->AttributeInfo, NewObject);
			if (GlobalImportSettingsReference->bImportStaticMeshLODs)
			{
				// import LOD meshes
				for (int32 LODIndex = 1; LODIndex < Node->GetChildCount(); LODIndex++)
				{
					FbxNode* ChildNode = Node->GetChild(LODIndex);
					ImportANode(VoidFbxImporter, ChildNode, Flags, NodeIndex, SceneInfo, OutNodeInfo, PackagePath, Total, NewObject, LODIndex);
				}
			}
		}
	}
	else
	{
		if (Node->GetMesh())
		{
			NewObject = ImportANode(VoidFbxImporter, Node, Flags, NodeIndex, SceneInfo, OutNodeInfo, PackagePath, Total);

			if (NewObject)
			{
				//We should always have a valid attribute if we just create a new asset
				check(OutNodeInfo.IsValid() && OutNodeInfo->AttributeInfo.IsValid());

				AllNewAssets.Add(OutNodeInfo->AttributeInfo, NewObject);
			}
		}
		
		if (SceneImportOptions->bCreateContentFolderHierarchy)
		{
			PackagePath += TEXT("/") + FString(FFbxImporter->MakeName(Node->GetName()));
		}

		for (int32 ChildIndex = 0; ChildIndex < Node->GetChildCount(); ++ChildIndex)
		{
			UObject* SubObject = RecursiveImportNode(VoidFbxImporter, Node->GetChild(ChildIndex), Flags, NodeIndex, Total, SceneInfo, PackagePath);
			if (NewObject == NULL)
			{
				NewObject = SubObject;
			}
		}
	}

	return NewObject;
}

// @todo document
UObject* UFbxSceneImportFactory::ImportANode(void* VoidFbxImporter, void* VoidNode, EObjectFlags Flags, int32& NodeIndex, TSharedPtr<FFbxSceneInfo> SceneInfo, TSharedPtr<FFbxNodeInfo> &OutNodeInfo, FString PackagePath, int32 Total, UObject* InMesh, int LODIndex)
{
	UnFbx::FFbxImporter* FFbxImporter = (UnFbx::FFbxImporter*)VoidFbxImporter;
	FbxNode* Node = (FbxNode*)VoidNode;
	FString ParentName;
	if (Node->GetParent() != NULL)
	{
		ParentName = FFbxImporter->MakeName(Node->GetParent()->GetName());
	}
	else
	{
		ParentName.Empty();
	}
	
	FbxString NodeName(FFbxImporter->MakeName(Node->GetName()));
	//Find the scene node info in the hierarchy
	if (!FindSceneNodeInfo(SceneInfo, Node->GetUniqueID(), OutNodeInfo) || !OutNodeInfo->AttributeInfo.IsValid())
	{
		//We cannot instantiate this asset if its not part of the hierarchy
		return NULL;
	}

	if(OutNodeInfo->AttributeInfo->GetType() != UStaticMesh::StaticClass() || !OutNodeInfo->AttributeInfo->bImportAttribute)
	{
		//export only static mesh or the user specify to not import this mesh
		return NULL;
	}

	//Check if the Mesh was already import
	if (AllNewAssets.Contains(OutNodeInfo->AttributeInfo))
	{
		return AllNewAssets[OutNodeInfo->AttributeInfo];
	}

	UObject* NewObject = NULL;
	FName OutputName = FFbxImporter->MakeNameForMesh(OutNodeInfo->AttributeInfo->Name, NULL);
	{
		// skip collision models
		if (NodeName.Find("UCX") != -1 || NodeName.Find("MCDCX") != -1 ||
			NodeName.Find("UBX") != -1 || NodeName.Find("USP") != -1)
		{
			return NULL;
		}

		//Create a package for this node
		//Get Parent hierarchy name to create new package path

		FString PackageName = PackagePath + TEXT("/") + OutputName.ToString();
		FString StaticMeshName;
		UPackage* Pkg = CreatePackageForNode(PackageName, StaticMeshName);
		if (Pkg == NULL)
			return NULL;

		//Apply the correct fbx options
		TSharedPtr<FFbxMeshInfo> MeshInfo = StaticCastSharedPtr<FFbxMeshInfo>(OutNodeInfo->AttributeInfo);
		if (StaticMeshOverrideOptions.Contains(MeshInfo))
		{
			UnFbx::FBXImportOptions* OverrideImportSettings = *StaticMeshOverrideOptions.Find(MeshInfo);
			SFbxSceneOptionWindow::CopyFbxOptionsToFbxOptions(OverrideImportSettings, GlobalImportSettings);
			SFbxSceneOptionWindow::CopyFbxOptionsToStaticMeshOptions(OverrideImportSettings, SceneImportOptionsStaticMesh);
			SceneImportOptionsStaticMesh->FillStaticMeshInmportData(StaticMeshImportData, SceneImportOptions);
		}
		else
		{
			SFbxSceneOptionWindow::CopyFbxOptionsToFbxOptions(GlobalImportSettingsReference, GlobalImportSettings);
			SFbxSceneOptionWindow::CopyFbxOptionsToStaticMeshOptions(GlobalImportSettingsReference, SceneImportOptionsStaticMesh);
			SceneImportOptionsStaticMesh->FillStaticMeshInmportData(StaticMeshImportData, SceneImportOptions);
		}
		FName StaticMeshFName = FName(*StaticMeshName);
		NewObject = FFbxImporter->ImportStaticMesh(Pkg, Node, StaticMeshFName, Flags, StaticMeshImportData, Cast<UStaticMesh>(InMesh), LODIndex);
	}

	if (NewObject)
	{
		NodeIndex++;
		FFormatNamedArguments Args;
		Args.Add(TEXT("NodeIndex"), NodeIndex);
		Args.Add(TEXT("ArrayLength"), Total);
		GWarn->StatusUpdate(NodeIndex, Total, FText::Format(NSLOCTEXT("UnrealEd", "Importingf", "Importing ({NodeIndex} of {ArrayLength})"), Args));
	}

	return NewObject;
}

bool UFbxSceneImportFactory::FindSceneNodeInfo(TSharedPtr<FFbxSceneInfo> SceneInfo, uint64 NodeInfoUniqueId, TSharedPtr<FFbxNodeInfo> &OutNodeInfo)
{
	for (auto NodeIt = SceneInfo->HierarchyInfo.CreateIterator(); NodeIt; ++NodeIt)
	{
		if (NodeInfoUniqueId == (*NodeIt)->UniqueId)
		{
			OutNodeInfo = (*NodeIt);
			return true;
		}
	}
	return false;
}

UPackage *UFbxSceneImportFactory::CreatePackageForNode(FString PackageName, FString &StaticMeshName)
{
	FString PackageNameOfficial = PackageTools::SanitizePackageName(PackageName);
	// We can not create assets that share the name of a map file in the same location
	if (FEditorFileUtils::IsMapPackageAsset(PackageNameOfficial))
	{
		return NULL;
	}
	
	UPackage* PkgExist = FindPackage(NULL, *PackageNameOfficial);
	int32 tryCount = 1;
	while (PkgExist != NULL)
	{
		PackageNameOfficial = PackageName;
		PackageNameOfficial += TEXT("_");
		PackageNameOfficial += FString::FromInt(tryCount++);
		PkgExist = FindPackage(NULL, *PackageNameOfficial);
	}
	UPackage* Pkg = CreatePackage(NULL, *PackageNameOfficial);
	if (!ensure(Pkg))
	{
		return NULL;
	}
	Pkg->FullyLoad();

	StaticMeshName = FPackageName::GetLongPackageAssetName(Pkg->GetOutermost()->GetName());
	return Pkg;
}

#undef LOCTEXT_NAMESPACE
