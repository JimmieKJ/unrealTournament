// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "FbxMeshUtils.h"
#include "EngineDefines.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "Engine/SkeletalMesh.h"
#include "Factories/FbxAssetImportData.h"
#include "Factories/FbxSkeletalMeshImportData.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "Factories/FbxImportUI.h"
#include "Engine/StaticMesh.h"
#include "EditorDirectories.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/MessageDialog.h"

#include "Logging/TokenizedMessage.h"
#include "FbxImporter.h"

#include "DesktopPlatformModule.h"

#if WITH_APEX_CLOTHING
	#include "ApexClothingUtils.h"
#endif // #if WITH_APEX_CLOTHING


#include "Misc/FbxErrors.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

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

	bool ImportStaticMeshLOD( UStaticMesh* BaseStaticMesh, const FString& Filename, int32 LODLevel)
	{
		bool bSuccess = false;

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

			ImportOptions->bImportMaterials = false;
			ImportOptions->bImportTextures = false;
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
					return bSuccess;
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
				UStaticMesh* TempStaticMesh = NULL;
				TempStaticMesh = (UStaticMesh*)FFbxImporter->ImportStaticMeshAsSingle(BaseStaticMesh->GetOutermost(), *(LODNodeList[bUseLODs? LODLevel: 0]), NAME_None, RF_NoFlags, ImportData, BaseStaticMesh, LODLevel, nullptr);

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

					bSuccess = true;
				}
				else
				{
					// Import failed
					FNotificationInfo NotificationInfo(FText::GetEmpty());
					NotificationInfo.Text = FText::Format(LOCTEXT("LODImportFail", "Failed to import mesh for LOD {0}!"), FText::AsNumber( LODLevel ));
					NotificationInfo.ExpireDuration = 5.0f;
					FSlateNotificationManager::Get().AddNotification(NotificationInfo);

					bSuccess = false;
				}
			}

			// Cleanup
			for (int32 i = 0; i < LODNodeList.Num(); ++i)
			{
				delete LODNodeList[i];
			}
		}
		FFbxImporter->ReleaseScene();

		return bSuccess;
	}

	bool ImportSkeletalMeshLOD( class USkeletalMesh* SelectedSkelMesh, const FString& Filename, int32 LODLevel)
	{
		bool bSuccess = false;

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
			
			//Set the skeletal mesh import data from the base mesh, this make sure the import rotation transform is use when importing a LOD
			UFbxSkeletalMeshImportData* TempAssetImportData = NULL;

			UFbxAssetImportData *FbxAssetImportData = Cast<UFbxAssetImportData>(SelectedSkelMesh->AssetImportData);
			if (FbxAssetImportData != nullptr)
			{
				UFbxSkeletalMeshImportData* ImportData = Cast<UFbxSkeletalMeshImportData>(FbxAssetImportData);
				if (ImportData)
				{
					TempAssetImportData = ImportData;
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
				ImportOptions->bImportMaterials = false;
				ImportOptions->bImportTextures = false;
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
					return false;
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
					TArray<FName> OrderedMaterialNames;
					{
						int32 NoneNameCount = 0;
						for (const FSkeletalMaterial &Material : SelectedSkelMesh->Materials)
						{
							if (Material.ImportedMaterialSlotName == NAME_None)
								NoneNameCount++;

							OrderedMaterialNames.Add(Material.ImportedMaterialSlotName);
						}
						if (NoneNameCount >= OrderedMaterialNames.Num())
						{
							OrderedMaterialNames.Empty();
						}
					}
					
					TempSkelMesh = (USkeletalMesh*)FFbxImporter->ImportSkeletalMesh(SelectedSkelMesh->GetOutermost(), bUseLODs? SkelMeshNodeArray: *MeshObject, NAME_None, RF_Transient, TempAssetImportData, LODLevel, nullptr, nullptr, nullptr, true, OrderedMaterialNames.Num() > 0 ? &OrderedMaterialNames : nullptr);

					// Add imported mesh to existing model
					bool bMeshImportSuccess = false;
					if( TempSkelMesh )
					{
						bMeshImportSuccess = FFbxImporter->ImportSkeletalMeshLOD(TempSkelMesh, SelectedSkelMesh, SelectedLOD);

						// Mark package containing skeletal mesh as dirty.
						SelectedSkelMesh->MarkPackageDirty();
					}

					if(ImportOptions->bImportMorph)
					{
						FFbxImporter->ImportFbxMorphTarget(SkelMeshNodeArray, SelectedSkelMesh, SelectedSkelMesh->GetOutermost(), SelectedLOD);
					}

					if (bMeshImportSuccess)
					{
						bSuccess = true;
						// Set LOD source filename
						SelectedSkelMesh->LODInfo[SelectedLOD].SourceImportFilename = Filename;

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

		return bSuccess;
	}

	FString PromptForLODImportFile(const FText& PromptTitle)
	{
		FString ChosenFilname("");

		FString ExtensionStr;
		ExtensionStr += TEXT("All model files|*.fbx;*.obj|");
		ExtensionStr += TEXT("FBX files|*.fbx|");
		ExtensionStr += TEXT("Object files|*.obj|");
		ExtensionStr += TEXT("All files|*.*");

		// First, display the file open dialog for selecting the file.
		TArray<FString> OpenFilenames;
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
		bool bOpen = false;
		if(DesktopPlatform)
		{
			bOpen = DesktopPlatform->OpenFileDialog(
				FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
				PromptTitle.ToString(),
				*FEditorDirectories::Get().GetLastDirectory(ELastDirectory::FBX),
				TEXT(""),
				*ExtensionStr,
				EFileDialogFlags::None,
				OpenFilenames
				);
		}

		// Only continue if we pressed OK and have only one file selected.
		if(bOpen)
		{
			if(OpenFilenames.Num() == 0)
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
				ChosenFilname = OpenFilenames[0];
				FEditorDirectories::Get().SetLastDirectory(ELastDirectory::FBX, FPaths::GetPath(ChosenFilname)); // Save path as default for next time.
			}
		}
		
		return ChosenFilname;
	}

	bool ImportMeshLODDialog(class UObject* SelectedMesh, int32 LODLevel)
	{
		if(!SelectedMesh)
		{
			return false;
		}

		USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(SelectedMesh);
		UStaticMesh* StaticMesh = Cast<UStaticMesh>(SelectedMesh);

		if( !SkeletalMesh && !StaticMesh )
		{
			return false;
		}

		FString FilenameToImport("");

		if(SkeletalMesh)
		{
			if(SkeletalMesh->LODInfo.IsValidIndex(LODLevel))
			{
				FSkeletalMeshLODInfo& SkelLodInfo = SkeletalMesh->LODInfo[LODLevel];
				FilenameToImport = SkelLodInfo.SourceImportFilename;
			}
		}

		// Check the file exists first
		const bool bSourceFileExists = FPaths::FileExists(FilenameToImport);
		// We'll give the user a chance to choose a new file if a previously set file fails to import
		const bool bPromptOnFail = bSourceFileExists;
		
		if(!bSourceFileExists || FilenameToImport.IsEmpty())
		{
			FText PromptTitle;

			if(FilenameToImport.IsEmpty())
			{
				PromptTitle = FText::Format(LOCTEXT("LODImportPrompt_NoSource", "Choose a file to import for LOD {0}"), FText::AsNumber(LODLevel));
			}
			else if(!bSourceFileExists)
			{
				PromptTitle = FText::Format(LOCTEXT("LODImportPrompt_SourceNotFound", "LOD {0} Source file not found. Choose new file."), FText::AsNumber(LODLevel));
			}

			FilenameToImport = PromptForLODImportFile(PromptTitle);
		}
		
		bool bImportSuccess = false;

		if(!FilenameToImport.IsEmpty())
		{
			if(SkeletalMesh)
			{
				bImportSuccess = ImportSkeletalMeshLOD(SkeletalMesh, FilenameToImport, LODLevel);
			}
			else if(StaticMesh)
			{
				bImportSuccess = ImportStaticMeshLOD(StaticMesh, FilenameToImport, LODLevel);
			}
		}

		if(!bImportSuccess && bPromptOnFail)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("LODImport_SourceMissingDialog", "Failed to import LOD{0} as the source file failed to import, please select a new source file."), FText::AsNumber(LODLevel)));

			FText PromptTitle = FText::Format(LOCTEXT("LODImportPrompt_SourceFailed", "Failed to import source file for LOD {0}, choose a new file"), FText::AsNumber(LODLevel));
			FilenameToImport = PromptForLODImportFile(PromptTitle);

			if(FilenameToImport.Len() > 0 && FPaths::FileExists(FilenameToImport))
			{
				if(SkeletalMesh)
				{
					bImportSuccess = ImportSkeletalMeshLOD(SkeletalMesh, FilenameToImport, LODLevel);
				}
				else if(StaticMesh)
				{
					bImportSuccess = ImportStaticMeshLOD(StaticMesh, FilenameToImport, LODLevel);
				}
			}
		}

		if(!bImportSuccess)
		{
			// Failed to import a LOD, even after retries (if applicable)
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("LODImport_Failure", "Failed to import LOD{0}"), FText::AsNumber(LODLevel)));
		}

		return bImportSuccess;
	}

	void SetImportOption(UFbxImportUI* ImportUI)
	{
		UnFbx::FFbxImporter* FFbxImporter = UnFbx::FFbxImporter::GetInstance();
		UnFbx::FBXImportOptions* ImportOptions = FFbxImporter->GetImportOptions();
		ApplyImportUIToImportOptions(ImportUI, *ImportOptions);
	}
}  //end namespace MeshUtils

#undef LOCTEXT_NAMESPACE
