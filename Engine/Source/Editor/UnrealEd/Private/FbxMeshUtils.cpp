// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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
	/*
	*Writes out all StaticMeshes passed in (and all LoDs) for light map editing
	* @param InAllMeshes - All Static Mesh objects that need their LOD's lightmaps to be exported
	* @param InDirectory - The directory to attempt the export to
	* @return If any errors occured during export
	*/
	bool ExportAllLightmapModels (TArray<UStaticMesh*>& InAllMeshes, const FString& InDirectory, bool IsFBX)
	{
		bool bAnyErrorOccured = false;
		for (int32 MeshIndex = 0; MeshIndex < InAllMeshes.Num(); ++MeshIndex)
		{
			UStaticMesh* CurrentMesh = InAllMeshes[MeshIndex];
			check (CurrentMesh);

			if (FMath::IsWithin(CurrentMesh->LightMapCoordinateIndex, 0, (int32)MAX_TEXCOORDS))
			{
				for (int32 LODIndex = 0; LODIndex < CurrentMesh->GetNumLODs(); ++LODIndex)
				{	
					if (IsFBX)
					{
						FString Filename = InDirectory + TEXT("/") + CurrentMesh->GetName() + FString::Printf( TEXT("_UVs_LOD_%d.FBX"), LODIndex);

						bAnyErrorOccured |= ExportSingleLightmapModelFBX(CurrentMesh, Filename, LODIndex);
					}
					else
					{
						FString Filename = InDirectory + TEXT("/") + CurrentMesh->GetName() + FString::Printf( TEXT("_UVs_LOD_%d.OBJ"), LODIndex);
						bool bDisableBackup = true;
						FOutputDevice* Buffer = new FOutputDeviceFile(*Filename, bDisableBackup);
						if (Buffer)
						{
							Buffer->SetSuppressEventTag(true);
							Buffer->SetAutoEmitLineTerminator(false);

							bAnyErrorOccured |= ExportSingleLightmapModel (CurrentMesh, Buffer, LODIndex);

							Buffer->TearDown();
							//IFileManager::Get().Move(ExportParams.Filename, *TempFile, 1, 1);  //if we were to rename the file

						} 
						else 
						{
							UE_LOG(LogExportMeshUtils, Display, TEXT("Unable to load static mesh from file '%s'"), *Filename);
							bAnyErrorOccured = true;
						}
					}
				}
			}
			else
			{
				bAnyErrorOccured |= true;
				UE_LOG(LogExportMeshUtils, Warning, TEXT("Lightmap Coordinate is out of bounds for '%s' with a value of %d"), *CurrentMesh->GetName(), CurrentMesh->LightMapCoordinateIndex);
			}
		}
		return bAnyErrorOccured;
	}


	/*
	*Reads in all StaticMeshes passed in (and all LoDs) for light map editing
	* @param InAllMeshes - All Static Mesh objects that need their LOD's lightmaps to be imported
	* @param InDirectory - The directory to attempt the import from
	* @return If any errors occured during import
	*/
	bool ImportAllLightmapModels(TArray<UStaticMesh*>& InAllMeshes, const FString& InDirectory, bool IsFBX)
	{

		bool bAnyErrorOccured = false;
		// TODO_STATICMESH: Remove this function.
		return bAnyErrorOccured;
	}


	/*
	*Writes out a single LOD of a StaticMesh(LODIndex) for light map editing
	* @param InStaticMesh - The mesh to be exported
	* @param InOutputDevice - The output device to write the data to.  Current usage is a FOutputDeviceFile
	* @param InLODIndex - The LOD of the model we want exported
	* @return If any errors occured during export
	*/
	bool ExportSingleLightmapModel (UStaticMesh* InStaticMesh, FOutputDevice* InOutputDevice, const int32 InLODIndex)
	{
		bool bAnyErrorOccured = false;

		check(InStaticMesh);
		check(InLODIndex < InStaticMesh->GetNumLODs());
		check(InOutputDevice);

		const int32 LightMapChannelIndex = InStaticMesh->LightMapCoordinateIndex;

		//OBJ File Header
		InOutputDevice->Log( TEXT("# UnrealEd Lightmap OBJ exporter\r\n") );


		// Collect all the data about the mesh
		FStaticMeshLODResources& RenderData = InStaticMesh->GetLODForExport(InLODIndex);

		//inform the user on export
		if (RenderData.GetNumTriangles())
		{
			const int32 MaxUVS = RenderData.VertexBuffer.GetNumTexCoords();

			//if no light map has been authored yet, OR the light map is out of bounds of the current NumUVS
			if ((LightMapChannelIndex == 0) || (LightMapChannelIndex >= MaxUVS)) 
			{
				UE_LOG(LogExportMeshUtils, Warning, TEXT("Lightmap Coordinate is out of bounds for '%s' with a value of %d.  On import, a new set of textures coordinates will be added."),
					*InStaticMesh->GetName(), LightMapChannelIndex);
				bAnyErrorOccured = true;
			}
		} 
		else 
		{
			UE_LOG(LogExportMeshUtils, Warning, TEXT("Source data missing from model '%s'."), *InStaticMesh->GetName());
			bAnyErrorOccured = true;
		}

		TArray<FVector> Verts;				// The verts in the mesh
		TArray<FVector2D> UVs;			// Lightmap UVs from channel 1

		FIndexArrayView Indices = RenderData.IndexBuffer.GetArrayView();

		// Collect all the data about the mesh
		for( int32 tri = 0 ; tri < RenderData.GetNumTriangles() ; tri++ )
		{
			uint32 Index1 = Indices[(tri * 3) + 0];
			uint32 Index2 = Indices[(tri * 3) + 1];
			uint32 Index3 = Indices[(tri * 3) + 2];

			FVector Vertex1 = RenderData.PositionVertexBuffer.VertexPosition(Index1);
			FVector Vertex2 = RenderData.PositionVertexBuffer.VertexPosition(Index2);
			FVector Vertex3 = RenderData.PositionVertexBuffer.VertexPosition(Index3);

			// Vertices
			Verts.Add( RenderData.PositionVertexBuffer.VertexPosition(Index1) );
			Verts.Add( RenderData.PositionVertexBuffer.VertexPosition(Index2) );
			Verts.Add( RenderData.PositionVertexBuffer.VertexPosition(Index3) );

			// Lightmap UVs
			UVs.Add( RenderData.VertexBuffer.GetVertexUV(Index1, LightMapChannelIndex) );
			UVs.Add( RenderData.VertexBuffer.GetVertexUV(Index2, LightMapChannelIndex) );
			UVs.Add( RenderData.VertexBuffer.GetVertexUV(Index3, LightMapChannelIndex) );
		}

		// Write out the vertex data
		InOutputDevice->Log( TEXT("\r\n") );
		for( int32 v = 0 ; v < Verts.Num() ; ++v )
		{
			// Transform to Lightwave's coordinate system
			InOutputDevice->Logf( TEXT("v %f %f %f\r\n"), Verts[v].X, Verts[v].Z, Verts[v].Y );
		}
		InOutputDevice->Logf( TEXT("# %d vertices\r\n"), Verts.Num() );

		// Write out the lightmap UV data 
		InOutputDevice->Log( TEXT("\r\n") );
		for( int32 uv = 0 ; uv < UVs.Num() ; ++uv )
		{
			// Invert the y-coordinate (Lightwave has their bitmaps upside-down from us).
			InOutputDevice->Logf( TEXT("vt %f %f\r\n"), UVs[uv].X, 1.0f - UVs[uv].Y);
		}
		InOutputDevice->Logf( TEXT("# %d uvs\r\n"), UVs.Num() );

		// Write object header
		InOutputDevice->Log( TEXT("\r\n") );
		InOutputDevice->Log( TEXT("g UnrealEdObject\r\n") );
		InOutputDevice->Log( TEXT("\r\n") );

		//NOTE - JB - Do not save out the smoothing groups separately.  If you do, the vertices get swizzled on import.
		//This is because the vertices are exported in RawTriangle Ordering, so to match again on import the face list has to be perfectly sequential.
		InOutputDevice->Logf( TEXT("s %i\r\n"), 0);		//only smoothing group
		for( int32 tri = 0 ; tri < RenderData.GetNumTriangles(); tri++ )
		{
			int32 VertexIndex = 1 + tri*3;
			InOutputDevice->Logf(TEXT("f %d/%d %d/%d %d/%d\r\n"), VertexIndex, VertexIndex, VertexIndex+1, VertexIndex+1, VertexIndex+2, VertexIndex+2 );
		}

		// Write out footer
		InOutputDevice->Log( TEXT("\r\n") );
		InOutputDevice->Log( TEXT("g\r\n") );

		return bAnyErrorOccured;	//no errors occured at this point
	};

	/**
	 * Writes out a single LOD of a StaticMesh(LODIndex) for light map editing
	 * @param InCurrentStaticMesh - The mesh to be exported
	 * @param InFilename - The name of the file to write the data to.
	 * @param InLODIndex - The LOD of the model we want exported
	 * @return If any errors occured during export
	 */
	bool ExportSingleLightmapModelFBX (UStaticMesh* InStaticMesh, FString& InFilename, const int32 InLODIndex)
	{
		bool bAnyErrorOccured = false;

		check(InStaticMesh);
		check(InLODIndex < InStaticMesh->GetNumLODs());

		int32 LightMapChannelIndex = InStaticMesh->LightMapCoordinateIndex;

		UnFbx::FFbxExporter* Exporter = UnFbx::FFbxExporter::GetInstance();
		Exporter->CreateDocument();
		Exporter->ExportStaticMeshLightMap(InStaticMesh, InLODIndex, LightMapChannelIndex);
		Exporter->WriteToFile(*InFilename);


		return bAnyErrorOccured;
	}

	static bool ExtractLightmapUVsFromMesh( UStaticMesh* InDestStaticMesh, UStaticMesh* MeshToExtractFrom, int32 InLODIndex )
	{
		// TODO_STATICMESH: Remove this function.
		return false;
	}


	bool ImportSingleLightmapModelFBX(UStaticMesh* InStaticMesh, FString& Filename, const int32 InLODIndex )
	{
		// TODO_STATICMESH: Remove this function.
		return false;
	}

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

				LODNodeList[ChildIdx]->Add(Node->GetChild(ChildIdx));
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

	void ImportStaticMeshLOD( UStaticMesh* BaseStaticMesh, const FString& Filename, int32 LODLevel )
	{
		UE_LOG(LogExportMeshUtils, Log, TEXT("Fbx LOD loading"));
		// logger for all error/warnings
		// this one prints all messages that are stored in FFbxImporter
		// this function seems to get called outside of FBX factory
		UnFbx::FFbxImporter* FFbxImporter = UnFbx::FFbxImporter::GetInstance();
		UnFbx::FFbxLoggerSetter Logger(FFbxImporter);

		// don't import materials
		UnFbx::FBXImportOptions* ImportOptions = FFbxImporter->GetImportOptions();
		ImportOptions->bImportMaterials = false;
		ImportOptions->bImportTextures = false;

		if ( !FFbxImporter->ImportFromFile( *Filename, FPaths::GetExtension( Filename ) ) )
		{
			// Log the error message and fail the import.
			// @todo verify if the message works
			FFbxImporter->FlushToTokenizedErrorMessage(EMessageSeverity::Error);
		}
		else
		{
			FFbxImporter->FlushToTokenizedErrorMessage(EMessageSeverity::Warning);

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
				TempStaticMesh = (UStaticMesh*)FFbxImporter->ImportStaticMeshAsSingle(GetTransientPackage(), *(LODNodeList[bUseLODs? LODLevel: 0]), NAME_None, RF_NoFlags, NULL, BaseStaticMesh, LODLevel);

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

	void ImportSkeletalMeshLOD( class USkeletalMesh* SelectedSkelMesh, const FString& Filename, int32 LODLevel )
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
			ImportOptions->bImportMaterials = false;
			ImportOptions->bImportTextures = false;
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
				FFbxImporter->FillFbxSkelMeshArrayInScene(FFbxImporter->Scene->GetRootNode(), MeshArray, false);

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
								if (Node->GetChildCount() > SelectedLOD)
								{
									SkelMeshNodeArray.Add(Node->GetChild(SelectedLOD));
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
					}

					// Import mesh
					USkeletalMesh* TempSkelMesh = NULL;
					// @todo AssetImportData does this temp skeletal mesh need import data?
					UFbxSkeletalMeshImportData* TempAssetImportData = NULL;
					TempSkelMesh = (USkeletalMesh*)FFbxImporter->ImportSkeletalMesh(GetTransientPackage(), bUseLODs? SkelMeshNodeArray: *MeshObject, NAME_None, (EObjectFlags)0, TempAssetImportData, FPaths::GetBaseFilename(Filename));

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
						FFbxImporter->ImportFbxMorphTarget(SkelMeshNodeArray, SelectedSkelMesh, SelectedSkelMesh->GetOutermost(), Filename, SelectedLOD);
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

	void ImportMeshLODDialog( class UObject* SelectedMesh, int32 LODLevel )
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

		ExtensionStr += TEXT("FBX files|*.fbx|");

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