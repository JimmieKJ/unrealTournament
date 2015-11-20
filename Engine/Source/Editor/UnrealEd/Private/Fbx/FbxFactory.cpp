// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*
* Copyright 2008-2009 Autodesk, Inc.  All Rights Reserved.
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

#include "UnrealEd.h"
#include "Factories.h"
#include "BusyCursor.h"
#include "SSkeletonWidget.h"

#include "FbxImporter.h"

#include "FbxErrors.h"
#include "AssetRegistryModule.h"
#include "Engine/StaticMesh.h"

#define LOCTEXT_NAMESPACE "FBXFactory"

UFbxFactory::UFbxFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = NULL;
	Formats.Add(TEXT("fbx;FBX meshes and animations"));
	Formats.Add(TEXT("obj;OBJ Static meshes"));
	//Formats.Add(TEXT("dae;Collada meshes and animations"));

	bCreateNew = false;
	bText = false;
	bEditorImport = true;
	bOperationCanceled = false;
	bDetectImportTypeOnImport = false;
}


void UFbxFactory::PostInitProperties()
{
	Super::PostInitProperties();
	bEditorImport = true;
	bText = false;

	ImportUI = NewObject<UFbxImportUI>(this, NAME_None, RF_NoFlags);
}


bool UFbxFactory::DoesSupportClass(UClass * Class)
{
	return (Class == UStaticMesh::StaticClass() || Class == USkeletalMesh::StaticClass() || Class == UAnimSequence::StaticClass());
}

UClass* UFbxFactory::ResolveSupportedClass()
{
	UClass* ImportClass = NULL;

	if (ImportUI->MeshTypeToImport == FBXIT_SkeletalMesh)
	{
		ImportClass = USkeletalMesh::StaticClass();
	}
	else if (ImportUI->MeshTypeToImport == FBXIT_Animation)
	{
		ImportClass = UAnimSequence::StaticClass();
	}
	else
	{
		ImportClass = UStaticMesh::StaticClass();
	}

	return ImportClass;
}


bool UFbxFactory::DetectImportType(const FString& InFilename)
{
	UnFbx::FFbxImporter* FFbxImporter = UnFbx::FFbxImporter::GetInstance();
	UnFbx::FFbxLoggerSetter Logger(FFbxImporter);
	int32 ImportType = FFbxImporter->GetImportType(InFilename);
	if ( ImportType == -1)
	{
		return false;
	}
	else
	{
		ImportUI->MeshTypeToImport = EFBXImportType(ImportType);
		ImportUI->OriginalImportType = ImportUI->MeshTypeToImport;
	}
	
	return true;
}


UObject* UFbxFactory::ImportANode(void* VoidFbxImporter, void* VoidNode, UObject* InParent, FName InName, EObjectFlags Flags, int32& NodeIndex, int32 Total, UObject* InMesh, int LODIndex)
{
	UnFbx::FFbxImporter* FFbxImporter = (UnFbx::FFbxImporter*)VoidFbxImporter;
	FbxNode* Node = (FbxNode*)VoidNode;

	UObject* NewObject = NULL;
	FName OutputName = FFbxImporter->MakeNameForMesh(InName.ToString(), Node);
	
	{
		// skip collision models
		FbxString NodeName(Node->GetName());
		if ( NodeName.Find("UCX") != -1 || NodeName.Find("MCDCX") != -1 ||
			 NodeName.Find("UBX") != -1 || NodeName.Find("USP") != -1 )
		{
			return NULL;
		}

		NewObject = FFbxImporter->ImportStaticMesh( InParent, Node, OutputName, Flags, ImportUI->StaticMeshImportData, Cast<UStaticMesh>(InMesh), LODIndex );
	}

	if (NewObject)
	{
		NodeIndex++;
		FFormatNamedArguments Args;
		Args.Add( TEXT("NodeIndex"), NodeIndex );
		Args.Add( TEXT("ArrayLength"), Total );
		GWarn->StatusUpdate( NodeIndex, Total, FText::Format( NSLOCTEXT("UnrealEd", "Importingf", "Importing ({NodeIndex} of {ArrayLength})"), Args ) );
	}

	return NewObject;
}

bool UFbxFactory::ConfigureProperties()
{
	bDetectImportTypeOnImport = true;
	EnableShowOption();

	return true;
}

UObject* UFbxFactory::FactoryCreateBinary
(
 UClass*			Class,
 UObject*			InParent,
 FName				Name,
 EObjectFlags		Flags,
 UObject*			Context,
 const TCHAR*		Type,
 const uint8*&		Buffer,
 const uint8*		BufferEnd,
 FFeedbackContext*	Warn,
 bool&				bOutOperationCanceled
 )
{
	if( bOperationCanceled )
	{
		bOutOperationCanceled = true;
		FEditorDelegates::OnAssetPostImport.Broadcast(this, NULL);
		return NULL;
	}

	FEditorDelegates::OnAssetPreImport.Broadcast(this, Class, InParent, Name, Type);

	UObject* NewObject = NULL;

	if ( bDetectImportTypeOnImport )
	{
		if ( !DetectImportType(UFactory::CurrentFilename) )
		{
			// Failed to read the file info, fail the import
			FEditorDelegates::OnAssetPostImport.Broadcast(this, NULL);
			return NULL;
		}
	}
	// logger for all error/warnings
	// this one prints all messages that are stored in FFbxImporter
	UnFbx::FFbxImporter* FbxImporter = UnFbx::FFbxImporter::GetInstance();
	
	UnFbx::FFbxLoggerSetter Logger(FbxImporter);

	EFBXImportType ForcedImportType = FBXIT_StaticMesh;

	bool bIsObjFormat = false;
	if( FString(Type).Equals(TEXT("obj"), ESearchCase::IgnoreCase ) )
	{
		bIsObjFormat = true;
	}


	bool bShowImportDialog = bShowOption && !GIsAutomationTesting;
	bool bImportAll = false;
	UnFbx::FBXImportOptions* ImportOptions = GetImportOptions(FbxImporter, ImportUI, bShowImportDialog, InParent->GetPathName(), bOperationCanceled, bImportAll, bIsObjFormat, bIsObjFormat, ForcedImportType );
	bOutOperationCanceled = bOperationCanceled;
	
	if( bImportAll )
	{
		// If the user chose to import all, we don't show the dialog again and use the same settings for each object until importing another set of files
		bShowOption = false;
	}

	// For multiple files, use the same settings
	bDetectImportTypeOnImport = false;

	if (ImportOptions)
	{
		Warn->BeginSlowTask( NSLOCTEXT("FbxFactory", "BeginImportingFbxMeshTask", "Importing FBX mesh"), true );
		if ( !FbxImporter->ImportFromFile( *UFactory::CurrentFilename, Type ) )
		{
			// Log the error message and fail the import.
			Warn->Log(ELogVerbosity::Error, FbxImporter->GetErrorMessage() );
		}
		else
		{
			// Log the import message and import the mesh.
			const TCHAR* errorMessage = FbxImporter->GetErrorMessage();
			if (errorMessage[0] != '\0')
			{
				Warn->Log( errorMessage );
			}

			FbxNode* RootNodeToImport = NULL;
			RootNodeToImport = FbxImporter->Scene->GetRootNode();

			// For animation and static mesh we assume there is at lease one interesting node by default
			int32 InterestingNodeCount = 1;
			TArray< TArray<FbxNode*>* > SkelMeshArray;

			bool bImportStaticMeshLODs = ImportUI->StaticMeshImportData->bImportMeshLODs;
			bool bCombineMeshes = ImportUI->bCombineMeshes;

			if ( ImportUI->MeshTypeToImport == FBXIT_SkeletalMesh )
			{
				FbxImporter->FillFbxSkelMeshArrayInScene(RootNodeToImport, SkelMeshArray, false);
				InterestingNodeCount = SkelMeshArray.Num();
			}
			else if( ImportUI->MeshTypeToImport == FBXIT_StaticMesh )
			{
				FbxImporter->ApplyTransformSettingsToFbxNode(RootNodeToImport, ImportUI->StaticMeshImportData);

				if( bCombineMeshes && !bImportStaticMeshLODs )
				{
					// If Combine meshes and dont import mesh LODs, the interesting node count should be 1 so all the meshes are grouped together into one static mesh
					InterestingNodeCount = 1;
				}
				else
				{
					// count meshes in lod groups if we dont care about importing LODs
					bool bCountLODGroupMeshes = !bImportStaticMeshLODs;
					int32 NumLODGroups = 0;
					InterestingNodeCount = FbxImporter->GetFbxMeshCount(RootNodeToImport,bCountLODGroupMeshes,NumLODGroups);

					// if there were LODs in the file, do not combine meshes even if requested
					if( bImportStaticMeshLODs && bCombineMeshes )
					{
						bCombineMeshes = NumLODGroups == 0;
					}
				}
			}

		
			if (InterestingNodeCount > 1)
			{
				// the option only works when there are only one asset
				ImportOptions->bUsedAsFullName = false;
			}

			const FString Filename( UFactory::CurrentFilename );
			if (RootNodeToImport && InterestingNodeCount > 0)
			{  
				int32 NodeIndex = 0;

				int32 ImportedMeshCount = 0;
				UStaticMesh* NewStaticMesh = NULL;
				if ( ImportUI->MeshTypeToImport == FBXIT_StaticMesh )  // static mesh
				{
					if (bCombineMeshes)
					{
						TArray<FbxNode*> FbxMeshArray;
						FbxImporter->FillFbxMeshArray(RootNodeToImport, FbxMeshArray, FbxImporter);
						if (FbxMeshArray.Num() > 0)
						{
							NewStaticMesh = FbxImporter->ImportStaticMeshAsSingle(InParent, FbxMeshArray, Name, Flags, ImportUI->StaticMeshImportData, NULL, 0);
						}

						ImportedMeshCount = NewStaticMesh ? 1 : 0;
					}
					else
					{
						TArray<UObject*> AllNewAssets;
						UObject* Object = RecursiveImportNode(FbxImporter,RootNodeToImport,InParent,Name,Flags,NodeIndex,InterestingNodeCount, AllNewAssets);

						NewStaticMesh = Cast<UStaticMesh>( Object );

						// Make sure to notify the asset registry of all assets created other than the one returned, which will notify the asset registry automatically.
						for ( auto AssetIt = AllNewAssets.CreateConstIterator(); AssetIt; ++AssetIt )
						{
							UObject* Asset = *AssetIt;
							if ( Asset != NewStaticMesh )
							{
								FAssetRegistryModule::AssetCreated(Asset);
								Asset->MarkPackageDirty();
							}
						}

						ImportedMeshCount = AllNewAssets.Num();
					}

					// Importing static mesh sockets only works if one mesh is being imported
					if( ImportedMeshCount == 1 && NewStaticMesh )
					{
						FbxImporter->ImportStaticMeshSockets( NewStaticMesh );
					}

					NewObject = NewStaticMesh;

				}
				else if ( ImportUI->MeshTypeToImport == FBXIT_SkeletalMesh )// skeletal mesh
				{
					int32 TotalNumNodes = 0;

					for (int32 i = 0; i < SkelMeshArray.Num(); i++)
					{
						TArray<FbxNode*> NodeArray = *SkelMeshArray[i];
					
						TotalNumNodes += NodeArray.Num();
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
						bool bImportSkeletalMeshLODs = ImportUI->SkeletalMeshImportData->bImportMeshLODs;
						for (LODIndex = 0; LODIndex < MaxLODLevel; LODIndex++)
						{
							if ( !bImportSkeletalMeshLODs && LODIndex > 0) // not import LOD if UI option is OFF
							{
								break;
							}
						
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
								FName OutputName = FbxImporter->MakeNameForMesh(Name.ToString(), SkelMeshNodeArray[0]);

								USkeletalMesh* NewMesh = FbxImporter->ImportSkeletalMesh( InParent, SkelMeshNodeArray, OutputName, Flags, ImportUI->SkeletalMeshImportData, &bOperationCanceled );
								NewObject = NewMesh;

								if(bOperationCanceled)
								{
									// User cancelled, clean up and return
									FbxImporter->ReleaseScene();
									Warn->EndSlowTask();
									bOperationCanceled = true;
									return nullptr;
								}

								if ( NewMesh && ImportUI->bImportAnimations )
								{
									// We need to remove all scaling from the root node before we set up animation data.
									// Othewise some of the global transform calculations will be incorrect.
									FbxImporter->RemoveTransformSettingsFromFbxNode(RootNodeToImport, ImportUI->SkeletalMeshImportData);
									FbxImporter->SetupAnimationDataFromMesh(NewMesh, InParent, SkelMeshNodeArray, ImportUI->AnimSequenceImportData, OutputName.ToString());

									// Reapply the transforms for the rest of the import
									FbxImporter->ApplyTransformSettingsToFbxNode(RootNodeToImport, ImportUI->SkeletalMeshImportData);
								}
							}
							else if (NewObject) // the base skeletal mesh is imported successfully
							{
								USkeletalMesh* BaseSkeletalMesh = Cast<USkeletalMesh>(NewObject);
								FName LODObjectName = NAME_None;
								USkeletalMesh *LODObject = FbxImporter->ImportSkeletalMesh( GetTransientPackage(), SkelMeshNodeArray, LODObjectName, RF_NoFlags, ImportUI->SkeletalMeshImportData, &bOperationCanceled );
								bool bImportSucceeded = !bOperationCanceled && FbxImporter->ImportSkeletalMeshLOD(LODObject, BaseSkeletalMesh, LODIndex, false);

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
							if ( NewObject && ImportUI->SkeletalMeshImportData->bImportMorphTargets)
							{
								// Disable material importing when importing morph targets
								uint32 bImportMaterials = ImportOptions->bImportMaterials;
								ImportOptions->bImportMaterials = 0;

								FbxImporter->ImportFbxMorphTarget(SkelMeshNodeArray, Cast<USkeletalMesh>(NewObject), InParent, LODIndex);
							
								ImportOptions->bImportMaterials = !!bImportMaterials;
							}
						}
					
						if (NewObject)
						{
							NodeIndex++;
							FFormatNamedArguments Args;
							Args.Add( TEXT("NodeIndex"), NodeIndex );
							Args.Add( TEXT("ArrayLength"), SkelMeshArray.Num() );
							GWarn->StatusUpdate( NodeIndex, SkelMeshArray.Num(), FText::Format( NSLOCTEXT("UnrealEd", "Importingf", "Importing ({NodeIndex} of {ArrayLength})"), Args ) );
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
				else if ( ImportUI->MeshTypeToImport == FBXIT_Animation )// animation
				{
					if (ImportOptions->SkeletonForAnimation)
					{
						// will return the last animation sequence that were added
						NewObject = UEditorEngine::ImportFbxAnimation( ImportOptions->SkeletonForAnimation, InParent, ImportUI->AnimSequenceImportData, *Filename, *Name.ToString(), true );
					}
				}
			}
			else
			{
				if (RootNodeToImport == NULL)
				{
					FbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, LOCTEXT("FailedToImport_InvalidRoot", "Could not find root node.")), FFbxErrors::SkeletalMesh_InvalidRoot);
				}
				else if (ImportUI->MeshTypeToImport == FBXIT_SkeletalMesh)
				{
					FbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, LOCTEXT("FailedToImport_InvalidBone", "Failed to find any bone hierarchy. Try disabling the \"Import As Skeletal\" option to import as a rigid mesh. ")), FFbxErrors::SkeletalMesh_InvalidBone);
				}
				else
				{
					FbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, LOCTEXT("FailedToImport_InvalidNode", "Could not find any node.")), FFbxErrors::SkeletalMesh_InvalidNode);
				}
			}
		}

		if (NewObject == NULL)
		{
			FbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, LOCTEXT("FailedToImport_NoObject", "Import failed.")), FFbxErrors::Generic_ImportingNewObjectFailed);
		}

		FbxImporter->ReleaseScene();
		Warn->EndSlowTask();
	}

	FEditorDelegates::OnAssetPostImport.Broadcast(this, NewObject);

	return NewObject;
}


UObject* UFbxFactory::RecursiveImportNode(void* VoidFbxImporter, void* VoidNode, UObject* InParent, FName InName, EObjectFlags Flags, int32& NodeIndex, int32 Total, TArray<UObject*>& OutNewAssets)
{
	UObject* NewObject = NULL;

	FbxNode* Node = (FbxNode*)VoidNode;
	if (Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGroup && Node->GetChildCount() > 0 )
	{
		// import base mesh
		NewObject = ImportANode(VoidFbxImporter, Node->GetChild(0), InParent, InName, Flags, NodeIndex, Total);

		if ( NewObject )
		{
			OutNewAssets.Add(NewObject);
		}

		bool bImportMeshLODs;
		if ( ImportUI->MeshTypeToImport == FBXIT_StaticMesh )
		{
			bImportMeshLODs = ImportUI->StaticMeshImportData->bImportMeshLODs;
		}
		else if ( ImportUI->MeshTypeToImport == FBXIT_SkeletalMesh )
		{
			bImportMeshLODs = ImportUI->SkeletalMeshImportData->bImportMeshLODs;
		}
		else
		{
			bImportMeshLODs = false;
		}

		if (NewObject && bImportMeshLODs)
		{
			// import LOD meshes
			for (int32 LODIndex = 1; LODIndex < Node->GetChildCount(); LODIndex++)
			{
				FbxNode* ChildNode = Node->GetChild(LODIndex);
				ImportANode(VoidFbxImporter, ChildNode, InParent, InName, Flags, NodeIndex, Total, NewObject, LODIndex);
			}
		}
	}
	else
	{
		if (Node->GetMesh())
		{
			NewObject = ImportANode(VoidFbxImporter, Node, InParent, InName, Flags, NodeIndex, Total);

			if ( NewObject )
			{
				OutNewAssets.Add(NewObject);
			}
		}
		
		for (int32 ChildIndex=0; ChildIndex<Node->GetChildCount(); ++ChildIndex)
		{
			UObject* SubObject = RecursiveImportNode(VoidFbxImporter,Node->GetChild(ChildIndex),InParent,InName,Flags,NodeIndex,Total,OutNewAssets);

			if ( SubObject )
			{
				OutNewAssets.Add(SubObject);
			}

			if (NewObject==NULL)
			{
				NewObject = SubObject;
			}
		}
	}

	return NewObject;
}


void UFbxFactory::CleanUp() 
{
	UnFbx::FFbxImporter* FFbxImporter = UnFbx::FFbxImporter::GetInstance();
	bDetectImportTypeOnImport = true;
	bShowOption = true;
	// load options
	if (FFbxImporter)
	{
		struct UnFbx::FBXImportOptions* ImportOptions = FFbxImporter->GetImportOptions();
		if ( ImportOptions )
		{
			ImportOptions->SkeletonForAnimation = NULL;
			ImportOptions->PhysicsAsset = NULL;
		}
	}
}

bool UFbxFactory::FactoryCanImport(const FString& Filename)
{
	const FString Extension = FPaths::GetExtension(Filename);

	if( Extension == TEXT("fbx") || Extension == TEXT("obj") )
	{
		return true;
	}
	return false;
}

UFbxImportUI::UFbxImportUI(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCombineMeshes = true;

	StaticMeshImportData = CreateDefaultSubobject<UFbxStaticMeshImportData>(TEXT("StaticMeshImportData"));
	SkeletalMeshImportData = CreateDefaultSubobject<UFbxSkeletalMeshImportData>(TEXT("SkeletalMeshImportData"));
	AnimSequenceImportData = CreateDefaultSubobject<UFbxAnimSequenceImportData>(TEXT("AnimSequenceImportData"));
	TextureImportData = CreateDefaultSubobject<UFbxTextureImportData>(TEXT("TextureImportData"));
}


bool UFbxImportUI::CanEditChange( const UProperty* InProperty ) const
{
	bool bIsMutable = Super::CanEditChange( InProperty );
	if( bIsMutable && InProperty != NULL )
	{
		FName PropName = InProperty->GetFName();

		if(PropName == TEXT("StartFrame") || PropName == TEXT("EndFrame"))
		{
			bIsMutable = AnimSequenceImportData->AnimationLength == FBXALIT_SetRange && bImportAnimations;
		}
		else if(PropName == TEXT("bImportCustomAttribute") || PropName == TEXT("AnimationLength"))
		{
			bIsMutable = bImportAnimations;
		}

		if(bIsObjImport && InProperty->GetBoolMetaData(TEXT("OBJRestrict")))
		{
			bIsMutable = false;
		}
	}

	return bIsMutable;
}

#undef LOCTEXT_NAMESPACE
