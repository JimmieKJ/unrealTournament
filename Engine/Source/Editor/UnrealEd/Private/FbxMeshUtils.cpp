// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "StaticMeshResources.h"
#include "Factories.h"

#include "FbxMeshUtils.h"
#include "FbxExporter.h"

#include "MainFrame.h"
#include "DesktopPlatformModule.h"

#if WITH_APEX_CLOTHING
	#include "ApexClothingUtils.h"
#endif // #if WITH_APEX_CLOTHING

#include "ComponentReregisterContext.h"

#include "FbxErrors.h"
#include "SNotificationList.h"
#include "NotificationManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogExportMeshUtils, Log, All);

#define LOCTEXT_NAMESPACE "FbxMeshUtil"

namespace FbxMeshUtils
{
	/** Helper function used for retrieving data required for importing static mesh LODs */
	void PopulateFBXStaticMeshLODList(UnFbx::FFbxImporter* FFbxImporter, FbxNode* Node, TArray< TArray<FbxNode*>* >& LODNodeList, int32& MaxLODCount, bool bUseLODs)
	{
		// Check for LOD nodes, if one is found, add it to the list
		if (bUseLODs && Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGroup)
		{
			for (int32 ChildIdx = 0; ChildIdx < Node->GetChildCount(); ++ChildIdx)
			{
				if ((LODNodeList.Num() - 1) < ChildIdx)
				{
					TArray<FbxNode*>* NodeList = new TArray<FbxNode*>;
					LODNodeList.Add(NodeList);
				}
				FFbxImporter->FindAllLODGroupNode(*(LODNodeList[ChildIdx]), Node, ChildIdx);
			}

			if (MaxLODCount < (Node->GetChildCount() - 1))
			{
				MaxLODCount = Node->GetChildCount() - 1;
			}
		}
		else
		{
			// If we're just looking for meshes instead of LOD nodes, add those to the list
			if (!bUseLODs && Node->GetMesh())
			{
				if (LODNodeList.Num() == 0)
				{
					TArray<FbxNode*>* NodeList = new TArray<FbxNode*>;
					LODNodeList.Add(NodeList);
				}

				LODNodeList[0]->Add(Node);
			}

			// Recursively examine child nodes
			for (int32 ChildIndex = 0; ChildIndex < Node->GetChildCount(); ++ChildIndex)
			{
				PopulateFBXStaticMeshLODList(FFbxImporter, Node->GetChild(ChildIndex), LODNodeList, MaxLODCount, bUseLODs);
			}
		}
	}

	void ImportStaticMeshLOD( UStaticMesh* BaseStaticMesh, const FString& Filename, int32 LODLevel)
	{
		UE_LOG(LogExportMeshUtils, Log, TEXT("Fbx LOD loading"));
		// logger for all error/warnings
		// this one prints all messages that are stored in FFbxImporter
		// this function seems to get called outside of FBX factory
		UnFbx::FFbxImporter* FFbxImporter = UnFbx::FFbxImporter::GetInstance();
		UnFbx::FFbxLoggerSetter Logger(FFbxImporter);

		UnFbx::FBXImportOptions* ImportOptions = FFbxImporter->GetImportOptions();
		
		UFbxStaticMeshImportData* ImportData = Cast<UFbxStaticMeshImportData>(BaseStaticMesh->AssetImportData);
		if (ImportData != nullptr)
		{
			
			UFbxImportUI* ReimportUI = NewObject<UFbxImportUI>();
			ReimportUI->MeshTypeToImport = FBXIT_StaticMesh;
			UnFbx::FBXImportOptions::ResetOptions(ImportOptions);
			// Import data already exists, apply it to the fbx import options
			ReimportUI->StaticMeshImportData = ImportData;
			ApplyImportUIToImportOptions(ReimportUI, *ImportOptions);

			ImportOptions->bImportMaterials = ImportData->bImportMaterials;
			ImportOptions->bImportTextures = ImportData->bImportMaterials;
		}

		if ( !FFbxImporter->ImportFromFile( *Filename, FPaths::GetExtension( Filename ) ) )
		{
			// Log the error message and fail the import.
			// @todo verify if the message works
			FFbxImporter->FlushToTokenizedErrorMessage(EMessageSeverity::Error);
		}
		else
		{
			FFbxImporter->FlushToTokenizedErrorMessage(EMessageSeverity::Warning);
			if (ImportData)
			{
				FFbxImporter->ApplyTransformSettingsToFbxNode(FFbxImporter->Scene->GetRootNode(), ImportData);
			}

			bool bUseLODs = true;
			int32 MaxLODLevel = 0;
			TArray< TArray<FbxNode*>* > LODNodeList;
			TArray<FString> LODStrings;

			// Create a list of LOD nodes
			PopulateFBXStaticMeshLODList(FFbxImporter, FFbxImporter->Scene->GetRootNode(), LODNodeList, MaxLODLevel, bUseLODs);

			// No LODs, so just grab all of the meshes in the file
			if (MaxLODLevel == 0)
			{
				bUseLODs = false;
				MaxLODLevel = BaseStaticMesh->GetNumLODs();

				// Create a list of meshes
				PopulateFBXStaticMeshLODList(FFbxImporter, FFbxImporter->Scene->GetRootNode(), LODNodeList, MaxLODLevel, bUseLODs);

				// Nothing found, error out
				if (LODNodeList.Num() == 0)
				{
					FFbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText(LOCTEXT("Prompt_NoMeshFound", "No meshes were found in file."))), FFbxErrors::Generic_Mesh_MeshNotFound);

					FFbxImporter->ReleaseScene();
					return;
				}
			}

			// Display the LOD selection dialog
			if (LODLevel > BaseStaticMesh->GetNumLODs())
			{
				// Make sure they don't manage to select a bad LOD index
				FFbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("Prompt_InvalidLODIndex", "Invalid mesh LOD index {0}, as no prior LOD index exists!"), FText::AsNumber(LODLevel))), FFbxErrors::Generic_Mesh_LOD_InvalidIndex);
			}
			else
			{
				// Import mesh
				UStaticMesh* TempStaticMesh = NULL;
				TempStaticMesh = (UStaticMesh*)FFbxImporter->ImportStaticMeshAsSingle(BaseStaticMesh->GetOutermost(), *(LODNodeList[bUseLODs? LODLevel: 0]), NAME_None, RF_NoFlags, ImportData, BaseStaticMesh, LODLevel);

				// Add imported mesh to existing model
				if( TempStaticMesh )
				{
					// Update mesh component
					BaseStaticMesh->MarkPackageDirty();

					// Import worked
					FNotificationInfo NotificationInfo(FText::GetEmpty());
					NotificationInfo.Text = FText::Format(LOCTEXT("LODImportSuccessful", "Mesh for LOD {0} imported successfully!"), FText::AsNumber(LODLevel));
					NotificationInfo.ExpireDuration = 5.0f;
					FSlateNotificationManager::Get().AddNotification(NotificationInfo);
				}
				else
				{
					// Import failed
					FNotificationInfo NotificationInfo(FText::GetEmpty());
					NotificationInfo.Text = FText::Format(LOCTEXT("LODImportFail", "Failed to import mesh for LOD {0}!"), FText::AsNumber( LODLevel ));
					NotificationInfo.ExpireDuration = 5.0f;
					FSlateNotificationManager::Get().AddNotification(NotificationInfo);
				}
			}

			// Cleanup
			for (int32 i = 0; i < LODNodeList.Num(); ++i)
			{
				delete LODNodeList[i];
			}
		}
		FFbxImporter->ReleaseScene();
	}

	void ImportSkeletalMeshLOD( class USkeletalMesh* SelectedSkelMesh, const FString& Filename, int32 LODLevel)
	{
		// Check the file extension for FBX. Anything that isn't .FBX is rejected
		const FString FileExtension = FPaths::GetExtension(Filename);
		const bool bIsFBX = FCString::Stricmp(*FileExtension, TEXT("FBX")) == 0;

		if (bIsFBX)
		{
#if WITH_APEX_CLOTHING
			FClothingBackup ClothingBackup;

			if(LODLevel == 0)
			{
				ApexClothingUtils::BackupClothingDataFromSkeletalMesh(SelectedSkelMesh, ClothingBackup);
			}
#endif// #if WITH_APEX_CLOTHING

			UnFbx::FFbxImporter* FFbxImporter = UnFbx::FFbxImporter::GetInstance();
			// don't import material and animation
			UnFbx::FBXImportOptions* ImportOptions = FFbxImporter->GetImportOptions();

			UFbxAssetImportData *FbxAssetImportData = Cast<UFbxAssetImportData>(SelectedSkelMesh->AssetImportData);
			if (FbxAssetImportData != nullptr)
			{
				UFbxSkeletalMeshImportData* ImportData = Cast<UFbxSkeletalMeshImportData>(FbxAssetImportData);
				if (ImportData)
				{
					UnFbx::FBXImportOptions::ResetOptions(ImportOptions);
					// Prepare the import options
					UFbxImportUI* ReimportUI = NewObject<UFbxImportUI>();
					ReimportUI->MeshTypeToImport = FBXIT_SkeletalMesh;
					ReimportUI->Skeleton = SelectedSkelMesh->Skeleton;
					ReimportUI->PhysicsAsset = SelectedSkelMesh->PhysicsAsset;
					// Import data already exists, apply it to the fbx import options
					ReimportUI->SkeletalMeshImportData = ImportData;
					//Some options not supported with skeletal mesh
					ReimportUI->SkeletalMeshImportData->bBakePivotInVertex = false;
					ReimportUI->SkeletalMeshImportData->bTransformVertexToAbsolute = true;
					ApplyImportUIToImportOptions(ReimportUI, *ImportOptions);
				}
				ImportOptions->bImportMaterials = FbxAssetImportData->bImportMaterials;
				ImportOptions->bImportTextures = FbxAssetImportData->bImportMaterials;
			}
			ImportOptions->bImportAnimations = false;

			if ( !FFbxImporter->ImportFromFile( *Filename, FPaths::GetExtension( Filename ) ) )
			{
				// Log the error message and fail the import.
				FFbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, LOCTEXT("FBXImport_ParseFailed", "FBX file parsing failed.")), FFbxErrors::Generic_FBXFileParseFailed);
			}
			else
			{
				bool bUseLODs = true;
				int32 MaxLODLevel = 0;
				TArray< TArray<FbxNode*>* > MeshArray;
				TArray<FString> LODStrings;
				TArray<FbxNode*>* MeshObject = NULL;;

				// Populate the mesh array
				FFbxImporter->FillFbxSkelMeshArrayInScene(FFbxImporter->Scene->GetRootNode(), MeshArray, false, ImportOptions->bImportScene);

				// Nothing found, error out
				if (MeshArray.Num() == 0)
				{
					FFbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, LOCTEXT("FBXImport_NoMesh", "No meshes were found in file.")), FFbxErrors::Generic_MeshNotFound);
					FFbxImporter->ReleaseScene();
					return;
				}

				MeshObject = MeshArray[0];

				// check if there is LODGroup for this skeletal mesh
				for (int32 j = 0; j < MeshObject->Num(); j++)
				{
					FbxNode* Node = (*MeshObject)[j];
					if (Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGroup)
					{
						// get max LODgroup level
						if (MaxLODLevel < (Node->GetChildCount() - 1))
						{
							MaxLODLevel = Node->GetChildCount() - 1;
						}
					}
				}

				// No LODs found, switch to supporting a mesh array containing meshes instead of LODs
				if (MaxLODLevel == 0)
				{
					bUseLODs = false;
					MaxLODLevel = SelectedSkelMesh->LODInfo.Num();
				}

				// Create LOD dropdown strings
				LODStrings.AddZeroed(MaxLODLevel + 1);
				LODStrings[0] = FString::Printf( TEXT("Base") );
				for(int32 i = 1; i < MaxLODLevel + 1; i++)
				{
					LODStrings[i] = FString::Printf(TEXT("%d"), i);
				}


				int32 SelectedLOD = LODLevel;
				if (SelectedLOD > SelectedSkelMesh->LODInfo.Num())
				{
					// Make sure they don't manage to select a bad LOD index
					FFbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("FBXImport_InvalidLODIdx", "Invalid mesh LOD index {0}, no prior LOD index exists"), FText::AsNumber(SelectedLOD))), FFbxErrors::Generic_Mesh_LOD_InvalidIndex);
				}
				else
				{
					TArray<FbxNode*> SkelMeshNodeArray;

					if (bUseLODs || ImportOptions->bImportMorph)
					{
						for (int32 j = 0; j < MeshObject->Num(); j++)
						{
							FbxNode* Node = (*MeshObject)[j];
							if (Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGroup)
							{
								TArray<FbxNode*> NodeInLod;
								if (Node->GetChildCount() > SelectedLOD)
								{
									FFbxImporter->FindAllLODGroupNode(NodeInLod, Node, SelectedLOD);
								}
								else // in less some LODGroups have less level, use the last level
								{
									FFbxImporter->FindAllLODGroupNode(NodeInLod, Node, Node->GetChildCount() - 1);
								}

								for (FbxNode *MeshNode : NodeInLod)
								{
									SkelMeshNodeArray.Add(MeshNode);
								}
							}
							else
							{
								SkelMeshNodeArray.Add(Node);
							}
						}
					}

					// Import mesh
					USkeletalMesh* TempSkelMesh = NULL;
					// @todo AssetImportData does this temp skeletal mesh need import data?
					UFbxSkeletalMeshImportData* TempAssetImportData = NULL;
					TempSkelMesh = (USkeletalMesh*)FFbxImporter->ImportSkeletalMesh(SelectedSkelMesh->GetOutermost(), bUseLODs? SkelMeshNodeArray: *MeshObject, NAME_None, RF_Transient, TempAssetImportData, LODLevel);

					// Add imported mesh to existing model
					bool bImportSucceeded = false;
					if( TempSkelMesh )
					{
						bImportSucceeded = FFbxImporter->ImportSkeletalMeshLOD(TempSkelMesh, SelectedSkelMesh, SelectedLOD);

						// Mark package containing skeletal mesh as dirty.
						SelectedSkelMesh->MarkPackageDirty();
					}

					if(ImportOptions->bImportMorph)
					{
						FFbxImporter->ImportFbxMorphTarget(SkelMeshNodeArray, SelectedSkelMesh, SelectedSkelMesh->GetOutermost(), SelectedLOD);
					}

					if (bImportSucceeded)
					{
						// Notification of success
						FNotificationInfo NotificationInfo(FText::GetEmpty());
						NotificationInfo.Text = FText::Format(NSLOCTEXT("UnrealEd", "LODImportSuccessful", "Mesh for LOD {0} imported successfully!"), FText::AsNumber(SelectedLOD));
						NotificationInfo.ExpireDuration = 5.0f;
						FSlateNotificationManager::Get().AddNotification(NotificationInfo);
					}
					else
					{
						// Notification of failure
						FNotificationInfo NotificationInfo(FText::GetEmpty());
						NotificationInfo.Text = FText::Format(NSLOCTEXT("UnrealEd", "LODImportFail", "Failed to import mesh for LOD {0}!"), FText::AsNumber(SelectedLOD));
						NotificationInfo.ExpireDuration = 5.0f;
						FSlateNotificationManager::Get().AddNotification(NotificationInfo);
					}
				}

				// Cleanup
				for (int32 i=0; i<MeshArray.Num(); i++)
				{
					delete MeshArray[i];
				}					
			}
			FFbxImporter->ReleaseScene();

#if WITH_APEX_CLOTHING
			if(LODLevel == 0)
			{
				ApexClothingUtils::ReapplyClothingDataToSkeletalMesh(SelectedSkelMesh, ClothingBackup);
			}
			ApexClothingUtils::ReImportClothingSectionsFromClothingAsset(SelectedSkelMesh);
#endif// #if WITH_APEX_CLOTHING
		}
	}

	void ImportMeshLODDialog( class UObject* SelectedMesh, int32 LODLevel)
	{
		if(!SelectedMesh)
		{
			return;
		}

		USkeletalMesh* SkeletonMesh = Cast<USkeletalMesh>(SelectedMesh);
		UStaticMesh* StaticMesh = Cast<UStaticMesh>(SelectedMesh);

		if( !SkeletonMesh && !StaticMesh )
		{
			return;
		}

		FString ExtensionStr;

		ExtensionStr += TEXT("All model files|*.fbx;*.obj|");

		ExtensionStr += TEXT("FBX files|*.fbx|");

		ExtensionStr += TEXT("Object files|*.obj|");

		ExtensionStr += TEXT("All files|*.*");

		// First, display the file open dialog for selecting the file.
		TArray<FString> OpenFilenames;
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
		bool bOpen = false;
		if ( DesktopPlatform )
		{
			void* ParentWindowWindowHandle = NULL;

			IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
			const TSharedPtr<SWindow>& MainFrameParentWindow = MainFrameModule.GetParentWindow();
			if ( MainFrameParentWindow.IsValid() && MainFrameParentWindow->GetNativeWindow().IsValid() )
			{
				ParentWindowWindowHandle = MainFrameParentWindow->GetNativeWindow()->GetOSWindowHandle();
			}

			bOpen = DesktopPlatform->OpenFileDialog(
				ParentWindowWindowHandle,
				FText::Format( NSLOCTEXT("UnrealEd", "ImportMeshLOD", "Failed to import mesh for LOD {0}!"), FText::AsNumber( LODLevel ) ).ToString(),
				*FEditorDirectories::Get().GetLastDirectory(ELastDirectory::FBX),
				TEXT(""),
				*ExtensionStr,
				EFileDialogFlags::None,
				OpenFilenames
				);
		}
			
		// Only continue if we pressed OK and have only one file selected.
		if( bOpen )
		{
			if( OpenFilenames.Num() == 0)
			{
				UnFbx::FFbxImporter* FFbxImporter = UnFbx::FFbxImporter::GetInstance();
				FFbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, LOCTEXT("NoFileSelectedForLOD", "No file was selected for the LOD.")), FFbxErrors::Generic_Mesh_LOD_NoFileSelected);
			}
			else if(OpenFilenames.Num() > 1)
			{
				UnFbx::FFbxImporter* FFbxImporter = UnFbx::FFbxImporter::GetInstance();
				FFbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, LOCTEXT("MultipleFilesSelectedForLOD", "You may only select one file for the LOD.")), FFbxErrors::Generic_Mesh_LOD_MultipleFilesSelected);
			}
			else
			{
				FString Filename = OpenFilenames[0];
				FEditorDirectories::Get().SetLastDirectory(ELastDirectory::FBX, FPaths::GetPath(Filename)); // Save path as default for next time.

				if( SkeletonMesh )
				{
					ImportSkeletalMeshLOD(SkeletonMesh, Filename, LODLevel);
				}
				else if( StaticMesh )
				{
					ImportStaticMeshLOD(StaticMesh, Filename, LODLevel);
				}
			}
		}
	}

	void SetImportOption(UFbxImportUI* ImportUI)
	{
		UnFbx::FFbxImporter* FFbxImporter = UnFbx::FFbxImporter::GetInstance();
		UnFbx::FBXImportOptions* ImportOptions = FFbxImporter->GetImportOptions();
		ApplyImportUIToImportOptions(ImportUI, *ImportOptions);
	}
}  //end namespace MeshUtils

#undef LOCTEXT_NAMESPACE