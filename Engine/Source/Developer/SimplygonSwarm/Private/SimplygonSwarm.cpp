// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SimplygonSwarmPrivatePCH.h"
#include <algorithm>
#define LOCTEXT_NAMESPACE "SimplygonSwarm"

#include "MeshMergeData.h"

 // Standard Simplygon channels have some issues with extracting color data back from simplification, 
 // so we use this workaround with user channels
 static const char* USER_MATERIAL_CHANNEL_METALLIC = "UserMetallic";
 static const char* USER_MATERIAL_CHANNEL_ROUGHNESS = "UserRoughness";
 static const char* USER_MATERIAL_CHANNEL_SPECULAR = "UserSpecular";

 static const TCHAR* BASECOLOR_CHANNEL = TEXT("Basecolor");
 static const TCHAR* METALLIC_CHANNEL = TEXT("Metallic");
 static const TCHAR* SPECULAR_CHANNEL = TEXT("Specular");
 static const TCHAR* ROUGHNESS_CHANNEL = TEXT("Roughness");
 static const TCHAR* NORMAL_CHANNEL = TEXT("Normals");
 static const TCHAR* OPACITY_CHANNEL = TEXT("Opacity");
 static const TCHAR* EMISSIVE_CHANNEL = TEXT("Emissive");

#define SIMPLYGON_COLOR_CHANNEL "VertexColors"

#define KEEP_SIMPLYGON_SWARM_TEMPFILES

 //@third party code BEGIN SIMPLYGON
 #define USE_USER_OPACITY_CHANNEL 1
 #if USE_USER_OPACITY_CHANNEL
 static const char* USER_MATERIAL_CHANNEL_OPACITY = "UserOpacity";
 #endif
 //@third party code END SIMPLYGON
 static const TCHAR* SG_UE_INTEGRATION_REV = TEXT("#SG_UE_INTEGRATION_REV");

 #ifdef __clang__
	 // SimplygonSDK.h uses 'deprecated' pragma which Clang does not recognize
	 #pragma clang diagnostic push
	 #pragma clang diagnostic ignored "-Wunknown-pragmas"	// warning : unknown pragma ignored [-Wunknown-pragmas]
 #endif

 #ifdef __clang__
	 #pragma clang diagnostic pop
 #endif


 static const TCHAR* SPL_TEMPLATE_REMESHING = TEXT("{\"Header\":{\"SPLVersion\":\"7.0\",\"ClientName\":\"UE4\",\"ClientVersion\":\"UE4.9\",\"SimplygonVersion\":\"7.0\"},\"ProcessGraph\":{\"Type\":\"ContainerNode\",\"Name\":\"Node\",\"Children\":[{\"Processor\":{\"RemeshingSettings\":{\"CuttingPlaneSelectionSetName\":0,\"EmptySpaceOverride\":0.0,\"MaxTriangleSize\":32,\"OnScreenSize\":%d,\"ProcessSelectionSetName\":\"\",\"SurfaceTransferMode\":1,\"TransferColors\":false,\"TransferNormals\":false,\"UseCuttingPlanes\":%s,\"UseEmptySpaceOverride\":false,\"Enabled\":true%s},\"MappingImageSettings\":{\"AutomaticTextureSizeMultiplier\":1.0,\"ChartAggregatorMode\":0,\"ChartAggregatorOriginalTexCoordLevel\":0,\"ChartAggregatorUseAreaWeighting\":true,\"ChartAggregatorKeepOriginalChartProportions\":true,\"ChartAggregatorKeepOriginalChartProportionsFromChannel\":\"\",\"ChartAggregatorKeepOriginalChartSizes\":false,\"ChartAggregatorSeparateOverlappingCharts\":true,\"ForcePower2Texture\":true,\"GenerateMappingImage\":true,\"GenerateTangents\":true,\"GenerateTexCoords\":true,\"GutterSpace\":4,\"Height\":%d,\"MaximumLayers\":3,\"MultisamplingLevel\":3,\"ParameterizerMaxStretch\":6.0,\"ParameterizerUseVertexWeights\":false,\"ParameterizerUseVisibilityWeights\":false,\"TexCoordGeneratorType\":0,\"TexCoordLevel\":255,\"UseAutomaticTextureSize\":false,\"UseFullRetexturing\":false,\"Width\":%d,\"Enabled\":true},%s\"Type\":\"RemeshingProcessor\"},\"MaterialCaster\":[%s],\"DefaultTBNType\":2,\"AllowGPUAcceleration\":false,\"Type\":\"ProcessNode\",\"Name\":\"Node\",\"Children\":[{\"Format\":\"ssf\",\"Type\":\"WriteNode\",\"Name\":\"outputlod_0\",\"Children\":[]}]}]}}");


 static const TCHAR* SPL_TEMPLATE_COLORCASTER = TEXT("{\"BakeOpacityInAlpha\":false,\"ColorType\":\"%s\",\"Dilation\":8,\"FillMode\":2,\"IsSRGB\":true,\"OutputChannelBitDepth\":8,\"OutputChannels\":4,\"Type\":\"ColorCaster\",\"Name\":\"%s\",\"Channel\":\"%s\",\"DitherType\":0}");

 static const TCHAR* SPL_NORMAL_RECALC = TEXT("\"HardEdgeAngleInRadians\":%f");

 static const TCHAR* SPL_MERGE_DIST = TEXT("\"MergeDistance\":%f");

 static const TCHAR* CUTTING_PLANE_SETTINGS = TEXT("\"CuttingPlaneSettings\": [%s],");
 
 static const TCHAR* CUTTING_PLANE = TEXT("{\"PointX\": %f,\"PointY\": %f,\"PointZ\": %f,\"NormalX\": %f,\"NormalY\": %f,\"NormalZ\": %f,\"Enabled\": true}");

 static const TCHAR* SPL_TEMPLATE_NORMALCASTER = TEXT("{\"Dilation\":8,\"FillMode\":2,\"FlipBackfacingNormals\":false,\"FlipGreen\":false,\"GenerateTangentSpaceNormals\":true,\"NormalMapTextureLevel\":0,\"OutputChannelBitDepth\":8,\"OutputChannels\":3,\"Type\":\"NormalCaster\",\"Name\":\"%s\",\"Channel\":\"%s\",\"DitherType\":0}");

 static const TCHAR* SHADING_NETWORK_TEMPLATE = TEXT("<SimplygonShadingNetwork version=\"1.0\">\n\t<ShadingTextureNode ref=\"node_0\" name=\"ShadingTextureNode\">\n\t\t<DefaultColor0>\n\t\t\t<DefaultValue>1 1 1 1</DefaultValue>\n\t\t</DefaultColor0>\n\t\t<TextureName>%s</TextureName>\n\t\t<TexCoordSet>%s</TexCoordSet>\n\t\t<UseSRGB>%d</UseSRGB>\n\t\t<TileU>1.000000</TileU>\n\t\t<TileV>1.000000</TileV>\n\t</ShadingTextureNode>\n</SimplygonShadingNetwork>");

 class FSimplygonSwarmModule : public IMeshReductionModule
 {
 public:
	 // IModuleInterface interface.
	 virtual void StartupModule() override;
	 virtual void ShutdownModule() override;

	 // IMeshReductionModule interface.
	 virtual class IMeshReduction* GetMeshReductionInterface() override;
	 virtual class IMeshMerging* GetMeshMergingInterface() override;
 private:
 };


 DEFINE_LOG_CATEGORY_STATIC(LogSimplygonSwarm, Log, All);
 IMPLEMENT_MODULE(FSimplygonSwarmModule, SimplygonSwarm);



 struct FSimplygonSSFHelper
 {
 public:
	 static ssf::ssfString SSFNewGuid()
	 {
		 return TCHARToSSFString(*FGuid::NewGuid().ToString());
	 }

	 static ssf::ssfString SFFEmptyGuid()
	 {
		 return TCHARToSSFString(*FGuid::FGuid().ToString());
	 }

	 static ssf::ssfString TCHARToSSFString(const TCHAR* str)
	 {
		 return ssf::ssfString(std::basic_string<TCHAR>(str));
	 }

	 
	 static bool CompareSSFStr(ssf::ssfString lhs, ssf::ssfString rhs)
	 {
		 if (lhs.Value == rhs.Value)
			return true;

		 return false;
	 }

	 static FString SimplygonDirectory()
	 {
		 return FPaths::GameIntermediateDir() + TEXT("Simplygon/");
	 }
	 
 };

 class FSimplygonSwarm
	 : public IMeshMerging
 {
 public:
	 virtual ~FSimplygonSwarm()
	 {
	 }

	 static FSimplygonSwarm* Create()
	 {
		 return new FSimplygonSwarm();
	 }

	 struct FMaterialCastingProperties
	 {
		 bool bCastMaterials;
		 bool bCastNormals;
		 bool bCastMetallic;
		 bool bCastRoughness;
		 bool bCastSpecular;

		 FMaterialCastingProperties()
			 : bCastMaterials(false)
			 , bCastNormals(false)
			 , bCastMetallic(false)
			 , bCastRoughness(false)
			 , bCastSpecular(false)
		 {
		 }
	 };

	 void BuildProxy(
		 const TArray<UStaticMeshComponent*> InputStaticMeshComponents,
		 const struct FMeshProxySettings& InProxySettings,
		 FRawMesh& OutProxyMesh,
		 FFlattenMaterial& OutMaterial,
		 FBox& OutProxyBox)
	 {

	 }

	 virtual void ProxyLOD(const TArray<FMeshMergeData>& InData,
		 const struct FMeshProxySettings& InProxySettings,
		 const TArray<struct FFlattenMaterial>& InputMaterials,
		 const FGuid InJobGUID)
	 {
		 FRawMesh OutProxyMesh;
		 FFlattenMaterial OutMaterial;

		 //setup path variables
		 FString JobPath = FGuid::NewGuid().ToString();
		 FString JobDirectory = FString::Printf(TEXT("%s%s"), *FSimplygonSSFHelper::SimplygonDirectory(), *JobPath);
		 FString InputFolderPath = FString::Printf(TEXT("%s/Input"), *JobDirectory);

		 FString ZipFileName = FString::Printf(TEXT("%s/%s.zip"), *JobDirectory, *JobPath);
		 FString OutputZipFileName = FString::Printf(TEXT("%s/%s_output.zip"), *JobDirectory, *JobPath);
		 FString SPLFileOutputFullPath = FString::Printf(TEXT("%s/input.spl"), *InputFolderPath);
		 FString RemeshingSPLText;


		 //Create an spl template
		 CreateRemeshingSPL(InProxySettings, RemeshingSPLText);

		 //output spl to file. Since REST Interface
		 SaveSPL(RemeshingSPLText, SPLFileOutputFullPath);

		 ssf::pssfScene SsfScene;

		 TArray<FRawMesh> InputMeshes;

		 for (auto Data : InData)
		 {
			 InputMeshes.Push(Data.RawMesh);
		 }

		 //converts UE entities to ssf, Textures will be exported to file
		 ConvertToSsfScene(InData, InputMaterials, InProxySettings, InputFolderPath, SsfScene);

		 FString SsfOuputPath = FString::Printf(TEXT("%s/input.ssf"), *InputFolderPath);

		 //save out ssf file.
		 WriteSsfFile(SsfScene, SsfOuputPath);

		 //zip contents and spawn a task 
		 if (ZipContentsForUpload(InputFolderPath, ZipFileName))
		 {
			 //NOTE : Currently SgRESTInterface is not cleaned up and Task ref counted . The FSimplygonSwarmTask & FSimplygonRESTClient pari can be stored in a TArray or TMap to make aync calls
			 // The pair can be cleanup after the import process
			 SwarmTask = MakeShareable(new FSimplygonSwarmTask(ZipFileName, SPLFileOutputFullPath, OutputZipFileName, JobDirectory, &InStateLock, InJobGUID, CompleteDelegate));
			 SwarmTask->OnAssetDownloaded().BindRaw(this, &FSimplygonSwarm::ImportFile);
			 SwarmTask->OnAssetUploaded().BindRaw(this, &FSimplygonSwarm::Cleanup);
			 SgRESTInterface = new FSimplygonRESTClient(SwarmTask);
			 SgRESTInterface->StartJobAsync();
		 }
	 }


	 //@third party code BEGIN SIMPLYGON
	 //virtual void ProxyLOD(
		// const TArray<FMeshMaterialReductionData*>& InData,
		// const struct FSimplygonRemeshingSettings& InProxySettings,
		// FRawMesh& OutProxyMesh,
		// FFlattenMaterial& OutMaterial)
	 //{
		// //TODO : Implment only if Epic are not using Build Proxy 
	 //}

	 // IMeshMerging interface
	 virtual void BuildProxy(
		 const TArray<FRawMesh>& InputMeshes,
		 const TArray<FFlattenMaterial>& InputMaterials,
		 const struct FMeshProxySettings& InProxySettings,
		 FRawMesh& OutProxyMesh,
		 FFlattenMaterial& OutMaterial)
	 {
		 //setup path variables
		 FString JobPath = FGuid::NewGuid().ToString();
		 FString JobDirectory = FString::Printf(TEXT("%s%s"), *FSimplygonSSFHelper::SimplygonDirectory(), *JobPath);
		 FString InputFolderPath = FString::Printf(TEXT("%s/Input"), *JobDirectory);
		 
		 FString ZipFileName = FString::Printf(TEXT("%s/%s.zip"), *JobDirectory,*JobPath);
		 FString OutputZipFileName = FString::Printf(TEXT("%s/%s_output.zip"), *JobDirectory, *JobPath);
		 FString SPLFileOutputFullPath = FString::Printf(TEXT("%s/input.spl"), *InputFolderPath);
		 FString RemeshingSPLText;


		 //Create an spl template
		 CreateRemeshingSPL(InProxySettings, RemeshingSPLText);

		 //output spl to file. Since REST Interface
		 SaveSPL(RemeshingSPLText, SPLFileOutputFullPath);

		 ssf::pssfScene SsfScene;

		 //converts UE entities to ssf, Textures will be exported to file
		 ConvertToSsfScene(InputMeshes, InputMaterials, InProxySettings, InputFolderPath, SsfScene);
		 
		 FString SsfOuputPath = FString::Printf(TEXT("%s/input.ssf"), *InputFolderPath);

		 //save out ssf file.
		 WriteSsfFile(SsfScene, SsfOuputPath);

		 //zip contents and spawn a task 
		 if (ZipContentsForUpload(InputFolderPath, ZipFileName))
		 {
			 //NOTE : Currently SgRESTInterface is not cleaned up and Task ref counted . The FSimplygonSwarmTask & FSimplygonRESTClient pari can be stored in a TArray or TMap to make aync calls
			 // The pair can be cleanup after the import process
			 SwarmTask = MakeShareable(new FSimplygonSwarmTask(ZipFileName, SPLFileOutputFullPath, OutputZipFileName, JobDirectory, &InStateLock, FGuid::NewGuid(), CompleteDelegate));
			 SwarmTask->OnAssetDownloaded().BindRaw(this, &FSimplygonSwarm::ImportFile);
			 SwarmTask->OnAssetUploaded().BindRaw(this, &FSimplygonSwarm::Cleanup);
			 SgRESTInterface = new FSimplygonRESTClient(SwarmTask);
			 SgRESTInterface->StartJobAsync();
		 } 
	 }

	 /*
	 The Following method will cleanup temp files and folders created to upload the job to the simplygon job manager
	 */
	 void Cleanup(const FSimplygonSwarmTask& InSwarmTask)
	 {
#ifndef KEEP_SIMPLYGON_SWARM_TEMPFILES
		 FString InputFolderPath = FString::Printf(TEXT("%s/Input"), *InSwarmTask.JobDirectory);
		 //remove folder folder
		 if (FPaths::DirectoryExists(InputFolderPath))
		 {
			 if (!IFileManager::Get().DeleteDirectory(*InputFolderPath, true, true))
			 {
				 UE_LOG(LogSimplygonSwarm, Log, TEXT("Failed to remove simplygon swarm task temp directory %s"), *InputFolderPath);
			 }
		 }

		 //remove uploaded zip file
		 if (FPaths::FileExists(InSwarmTask.ZipFilePath))
		 {
			 if (!IFileManager::Get().Delete(*InSwarmTask.ZipFilePath))
			 {
				 UE_LOG(LogSimplygonSwarm, Log, TEXT("Failed to remove Simplygon Swarm Task temp file %s"), *InSwarmTask.ZipFilePath);
		 }
		 }
#endif
	 }


	 /*
	 This mehtod would be fired when the job has been download on the machine. A good plane to hook contents to the LODActor.
	 Note this would need to run on the Main Thread
	 */
	 void ImportFile(const FSimplygonSwarmTask& InSwarmTask)
	 {
		 FRawMesh OutProxyMesh;
		 FFlattenMaterial OutMaterial;

		 FString OutputFolderPath = FString::Printf(TEXT("%s/Output"), *InSwarmTask.JobDirectory);
		 
		 FString ParentDirForOutputSsf = FString::Printf(TEXT("%s/Node/Node/outputlod_0"), *OutputFolderPath);

		 //for import the file back in uncomment
		 if (UnzipDownloadedContent(InSwarmTask.OutputFilename, OutputFolderPath))
		 {
			 //FString InOuputSsfPath = FString::Printf(TEXT("%s/Node/Node/outputlod_0/output.ssf"), *OutputFolderPath);
			 FString InOuputSsfPath = FString::Printf(TEXT("%s/output.ssf"), *ParentDirForOutputSsf);
			 ssf::pssfScene OutSsfScene = new ssf::ssfScene();

			 FString SsfFullPath = FPaths::ConvertRelativePathToFull(InOuputSsfPath);
			 		 
			 if (!FPaths::FileExists(SsfFullPath))
			 {
				 UE_LOG(LogSimplygonSwarm, Log, TEXT("Ssf file not found %s"), *SsfFullPath);
				 return;
		 }

			 ReadSsfFile(SsfFullPath, OutSsfScene);

			 ConvertFromSsfScene(OutSsfScene, OutProxyMesh, OutMaterial, ParentDirForOutputSsf);

		 if (!OutProxyMesh.IsValid())
			 UE_LOG(LogSimplygonSwarm, Log, TEXT("RawMesh is invalid."));

		 CompleteDelegate.ExecuteIfBound(OutProxyMesh, OutMaterial, InSwarmTask.TestJobID);


		 //do cleanup work
#ifndef KEEP_SIMPLYGON_SWARM_TEMPFILES
			 if (FPaths::DirectoryExists(OutputFolderPath))
			 {
				 if (!IFileManager::Get().DeleteDirectory(*OutputFolderPath, true, true))
				 {
					 UE_LOG(LogSimplygonSwarm, Log, TEXT("Failed to remove simplygon swarm task temp directory %s"), *OutputFolderPath);
		 }
			 }

			 //remove uploaded zip file
			 if (FPaths::FileExists(InSwarmTask.OutputFilename))
			 {
				 if (!IFileManager::Get().Delete(*InSwarmTask.OutputFilename))
				 {
					 UE_LOG(LogSimplygonSwarm, Log, TEXT("Failed to remove Simplygon Swarm Task temp file %s"), *InSwarmTask.OutputFilename);
	 }
			 }
#endif
		 }
	 }

 private:
	 FString VersionString;
	 FSimplygonRESTClient* SgRESTInterface;
	 FCriticalSection InStateLock;
	 //FRunnableThread *Thread;
	 TSharedPtr<class FSimplygonSwarmTask> SwarmTask;
	 uint8 ToolMajorVersion;
	 uint8 ToolMinorVersion;
	 uint16 ToolBuildVersion;

	 explicit FSimplygonSwarm()
	 {
		 VersionString = FString::Printf(TEXT("%s"), SG_UE_INTEGRATION_REV);
		 ToolMajorVersion =  FEngineVersion::Current().GetMajor();
		 ToolMinorVersion = FEngineVersion::Current().GetMinor();
		 ToolBuildVersion = FEngineVersion::Current().GetPatch();
		 
	 }
	 


	 void ReadSsfFile(FString InSsfFilePath, ssf::pssfScene& SsfScene)
	 {
		 ssf::ssfString ToolName = FSimplygonSSFHelper::TCHARToSSFString(TEXT("UE4"));

		 ssf::ssfBinaryInputStream InputStream;
		 InputStream.OpenFile(FSimplygonSSFHelper::TCHARToSSFString(*InSsfFilePath));
		 SsfScene->ReadFile(&InputStream, ToolName, ToolMajorVersion, ToolMinorVersion, ToolBuildVersion);
	 }

	 void WriteSsfFile(ssf::pssfScene SsfScene, FString InSsfFilePath)
	 {
		 ssf::ssfString ToolName = FSimplygonSSFHelper::TCHARToSSFString(TEXT("UE4"));
		 ssf::ssfBinaryOutputStream theOutputStream;
		 theOutputStream.OpenFile(FSimplygonSSFHelper::TCHARToSSFString(*InSsfFilePath));
		 SsfScene->WriteFile(&theOutputStream, ToolName, ToolMajorVersion, ToolMinorVersion, ToolBuildVersion);
		 theOutputStream.CloseFile();
	 }

	 void CreateRemeshingSPL(const struct FMeshProxySettings& InProxySettings,
		 FString& OutSplText)
	 {

		 ////Compute max mapping image size
		 FIntPoint ImageSizes = ComputeMappingImageSize(InProxySettings.MaterialSettings);

		 ////setup casters
		 FString CasterSPLTempalte = CreateSPLTemplateChannels(InProxySettings.MaterialSettings);
		 FString MergeDist = FString::Printf(SPL_MERGE_DIST, InProxySettings.MergeDistance);
		 FString AdditionalSettings = FString::Printf(TEXT(",%s"), *MergeDist);
		 FString RecalNormals = FString::Printf(SPL_NORMAL_RECALC, FMath::DegreesToRadians(InProxySettings.HardAngleThreshold));

		 if (InProxySettings.bRecalculateNormals)
			 AdditionalSettings = FString::Printf(TEXT("%s,%s"), *AdditionalSettings, *RecalNormals);

		 //Note : The Simplygon API now supports multiple clipping planes. SPL has recently added support for this.
		 // 
		 FString CuttingPlaneSetting;

		 if (InProxySettings.bUseClippingPlane)
		 {
			 // Note : An arbitary plane can be define using a point (position) and a normal (direction)
			 // Since UE  currently only has axis aligned plane. We need to convert values for SPL

			 FVector OutPoint, OutNormal;

			 GetAxisAlignedVectorsForCuttingPlanes(InProxySettings, OutPoint, OutNormal);

			 FString CuttingPlane = FString::Printf(CUTTING_PLANE, OutPoint.X, OutPoint.Y, OutPoint.Z, OutNormal.X, OutNormal.Y, OutNormal.Z);
			 CuttingPlaneSetting = FString::Printf(CUTTING_PLANE_SETTINGS, *CuttingPlane);
		 }
		 
		 OutSplText = FString::Printf(SPL_TEMPLATE_REMESHING, InProxySettings.ScreenSize, InProxySettings.bUseClippingPlane ? TEXT("true") :TEXT("false"), *AdditionalSettings, ImageSizes.X, ImageSizes.Y, *CuttingPlaneSetting, *CasterSPLTempalte);

	 }
	 

	 void GetAxisAlignedVectorsForCuttingPlanes(const struct FMeshProxySettings& InProxySettings, 
		 FVector& OutPoint, 
		 FVector& OutNormal)
	 {
		 
		 // 0 -> X , 1 -> Y, 2 -> Z
		 if (InProxySettings.AxisIndex == 0)
		 {
			 OutNormal.X = InProxySettings.bPlaneNegativeHalfspace ? -1 : 1 ;
			 OutPoint.X = InProxySettings.ClippingLevel;
			 
		 }
		 else if (InProxySettings.AxisIndex == 1)
		 {
			 OutNormal.Y = InProxySettings.bPlaneNegativeHalfspace ? -1 : 1;
			 OutPoint.Y = InProxySettings.ClippingLevel;
		 }
		 else
		 {
			 OutNormal.Z = InProxySettings.bPlaneNegativeHalfspace ? -1 : 1;
			 OutPoint.Z = InProxySettings.ClippingLevel;
			 //default to z up 
		 }
	 }
	 /*
	 Write the SPL string to file
	 */
	 void SaveSPL(FString InSplText, FString InOutputFilePath)
	 {
		 FArchive* SPLFile = IFileManager::Get().CreateFileWriter(*InOutputFilePath);
		 SPLFile->Logf(*InSplText);
		 SPLFile->Close();
	 }

	 void ConvertToSsfScene(const TArray<FMeshMergeData>& InputData,
		 const TArray<FFlattenMaterial>& InputMaterials,
		 const struct FMeshProxySettings& InProxySettings, FString InputFolderPath, ssf::pssfScene& OutSsfScene)
	 {
		 //creeate the ssf scene
		 OutSsfScene = new ssf::ssfScene();

		 OutSsfScene->CoordinateSystem.Set(1);
		 OutSsfScene->WorldOrientation.Set(2);
		 OutSsfScene->TextureTable->TexturesDirectory.Set(FSimplygonSSFHelper::TCHARToSSFString(TEXT("/Textures")));


		 TMap<int, FString> MaterialMap;

		 CreateSSFMaterialFromFlattenMaterial(InputMaterials, InProxySettings.MaterialSettings, OutSsfScene->MaterialTable, OutSsfScene->TextureTable, InputFolderPath, true, MaterialMap);

		 //create the root node
		 ssf::pssfNode SsfRootNode = new ssf::ssfNode();
		 SsfRootNode->Id.Set(FSimplygonSSFHelper::SSFNewGuid());
		 SsfRootNode->ParentId.Set(FSimplygonSSFHelper::SFFEmptyGuid());

		 //add root node to scene
		 OutSsfScene->NodeTable->NodeList.push_back(SsfRootNode);

		 int32 Count = 0;
		 for (FMeshMergeData MergeData : InputData)
		 {
			 //create a the node that will contain the mesh
			 ssf::pssfNode SsfNode = new ssf::ssfNode();
			 SsfNode->Id.Set(FSimplygonSSFHelper::SSFNewGuid());
			 SsfNode->ParentId.Set(SsfRootNode->Id.Get());
			 FString NodeName = FString::Printf(TEXT("Node%i"), Count);

			 SsfNode->Name.Set(FSimplygonSSFHelper::TCHARToSSFString(*NodeName));

			 //create the mesh object
			 ssf::pssfMesh SsfMesh = new ssf::ssfMesh();
			 SsfMesh->Id.Set(FSimplygonSSFHelper::SSFNewGuid());
			 FString MeshName = FString::Printf(TEXT("Mesh%i"), Count);
			 SsfMesh->Name.Set(FSimplygonSSFHelper::TCHARToSSFString(*MeshName));

			 Count++;

			 //setup mesh data
			 ssf::pssfMeshData SsfMeshData = CreateSSFMeshDataFromRawMesh(MergeData.RawMesh, MergeData.TexCoordBounds, MergeData.NewUVs);
			 SsfMesh->MeshDataList.push_back(SsfMeshData);

			 //setup mesh material information
			 SsfMesh->MaterialIds.Create();
			 TArray<int32> UniqueMaterialIds;
			 UniqueMaterialIds.Reserve(InputMaterials.Num());

			 //get unqiue material ids
			 GetUniqueMaterialIndices(MergeData.RawMesh.FaceMaterialIndices, UniqueMaterialIds);

			 SsfMesh->MaterialIds->Items.reserve(UniqueMaterialIds.Num());

			 TMap<int, int> GlobalToLocal;
			 //map ssfmesh local materials
			 for (int32 GlobalMaterialId : UniqueMaterialIds)
			 {
				 SsfMesh->MaterialIds->Items.push_back(FSimplygonSSFHelper::TCHARToSSFString(*MaterialMap[GlobalMaterialId]));
				 int32 localIndex = SsfMesh->MaterialIds->Items.size() - 1;
				 //replace 
				 GlobalToLocal.Add(GlobalMaterialId, localIndex);
			 }

			 for (ssf::pssfMeshData MeshData : SsfMesh->MeshDataList)
			 {
				 for (int Index = 0; Index < MeshData->MaterialIndices.Get().Items.size(); Index++)
				 {
					 MeshData->MaterialIndices.Get().Items[Index] = GlobalToLocal[MeshData->MaterialIndices.Get().Items[Index]];
				 }
			 }

			 //link mesh to node
			 SsfNode->MeshId.Set(SsfMesh->Id.Get().Value);

			 //add mesh and node to their respective tables
			 OutSsfScene->NodeTable->NodeList.push_back(SsfNode);
			 OutSsfScene->MeshTable->MeshList.push_back(SsfMesh);
		 }
	 }

	 void ConvertToSsfScene(const TArray<FRawMesh>& InputMeshes,
		 const TArray<FFlattenMaterial>& InputMaterials,
		 const struct FMeshProxySettings& InProxySettings, FString InputFolderPath, ssf::pssfScene& OutSsfScene)
	 {
		 //creeate the ssf scene
		 OutSsfScene = new ssf::ssfScene();

		 OutSsfScene->CoordinateSystem.Set(1);
		 OutSsfScene->WorldOrientation.Set(2);
		 OutSsfScene->TextureTable->TexturesDirectory.Set(FSimplygonSSFHelper::TCHARToSSFString(TEXT("/Textures")));


		 TMap<int, FString> MaterialMap;

		 CreateSSFMaterialFromFlattenMaterial(InputMaterials, InProxySettings.MaterialSettings, OutSsfScene->MaterialTable, OutSsfScene->TextureTable, InputFolderPath, true, MaterialMap);

		 //create the root node
		 ssf::pssfNode SsfRootNode = new ssf::ssfNode();
		 SsfRootNode->Id.Set(FSimplygonSSFHelper::SSFNewGuid());
		 SsfRootNode->ParentId.Set(FSimplygonSSFHelper::SFFEmptyGuid());

		 //add root node to scene
		 OutSsfScene->NodeTable->NodeList.push_back(SsfRootNode);

		 for (FRawMesh RawMesh : InputMeshes)
		 {
			 //create a the node that will contain the mesh
			 ssf::pssfNode SsfNode = new ssf::ssfNode();
			 SsfNode->Id.Set(FSimplygonSSFHelper::SSFNewGuid());
			 SsfNode->ParentId.Set(SsfRootNode->Id.Get());
			 SsfNode->Name.Set(FSimplygonSSFHelper::TCHARToSSFString(TEXT("Node")));

			 //create the mesh object
			 ssf::pssfMesh SsfMesh = new ssf::ssfMesh();
			 SsfMesh->Id.Set(FSimplygonSSFHelper::SSFNewGuid());
			 SsfMesh->Name.Set(FSimplygonSSFHelper::TCHARToSSFString(TEXT("Mesh")));

			 //setup mesh data
			 ssf::pssfMeshData SsfMeshData = CreateSSFMeshDataFromRawMesh(RawMesh);
			 SsfMesh->MeshDataList.push_back(SsfMeshData);

			 //setup mesh material information
			 SsfMesh->MaterialIds.Create();
			 TArray<int32> UniqueMaterialIds;
			 UniqueMaterialIds.Reserve(InputMaterials.Num());

			 //get unqiue material ids
			 GetUniqueMaterialIndices(RawMesh.FaceMaterialIndices, UniqueMaterialIds);


			 SsfMesh->MaterialIds->Items.reserve(UniqueMaterialIds.Num());

			 TMap<int, int> GlobalToLocal;
			 //map ssfmesh local materials
			 for (int32 GlobalMaterialId : UniqueMaterialIds)
			 {
				 SsfMesh->MaterialIds->Items.push_back(FSimplygonSSFHelper::TCHARToSSFString(*MaterialMap[GlobalMaterialId]));
				 int32 localIndex = SsfMesh->MaterialIds->Items.size() - 1;
				 //replace 
				 GlobalToLocal.Add(GlobalMaterialId, localIndex);
			 }

			 for (ssf::pssfMeshData MeshData : SsfMesh->MeshDataList)
			 {
				 for (int Index = 0; Index < MeshData->MaterialIndices.Get().Items.size(); Index++)
				 {
					 MeshData->MaterialIndices.Get().Items[Index] = GlobalToLocal[MeshData->MaterialIndices.Get().Items[Index]];
				 }
			 }

			 //link mesh to node
			 SsfNode->MeshId.Set(SsfMesh->Id.Get().Value);

			 //add mesh and node to their respective tables
			 OutSsfScene->NodeTable->NodeList.push_back(SsfNode);
			 OutSsfScene->MeshTable->MeshList.push_back(SsfMesh);
		 }
	 }

	 void ConvertFromSsfScene(ssf::pssfScene SceneGraph, FRawMesh& OutProxyMesh, FFlattenMaterial& OutMaterial, const FString OutputFolderPath)
	 {

		 for (ssf::pssfMesh Mesh : SceneGraph->MeshTable->MeshList)
		 {
			 //extract geometry data
			 for (ssf::pssfMeshData MeshData : Mesh->MeshDataList)
			 {
				 int32 TotalVertices = MeshData->GetVerticesCount();
				 int32 ToatalCorners = MeshData->GetCornersCount();
				 int32 TotalTriangles = MeshData->GetTrianglesCount();

				 OutProxyMesh.VertexPositions.SetNumUninitialized(TotalVertices);
				 int VertexIndex = 0;
				 for (ssf::ssfVector3 VertexCoord : MeshData->Coordinates.Get().Items)
				 {
					 OutProxyMesh.VertexPositions[VertexIndex] = GetConversionMatrix().TransformPosition(FVector(VertexCoord.V[0], VertexCoord.V[1], VertexCoord.V[2]));
					 VertexIndex++;
				 }
				 
				 OutProxyMesh.WedgeIndices.SetNumUninitialized(ToatalCorners);
				 for (int32 TriIndex = 0; TriIndex < TotalTriangles; ++TriIndex)
				 {
					 OutProxyMesh.WedgeIndices[TriIndex * 3 + 0] = MeshData->TriangleIndices.Get().Items[TriIndex].V[0];
					 OutProxyMesh.WedgeIndices[TriIndex * 3 + 1] = MeshData->TriangleIndices.Get().Items[TriIndex].V[1];
					 OutProxyMesh.WedgeIndices[TriIndex * 3 + 2] = MeshData->TriangleIndices.Get().Items[TriIndex].V[2];
				 }

				 int32 TexCoordIndex = 0;
				 for (ssf::ssfNamedList<ssf::ssfVector2> TexCoorChannel : MeshData->TextureCoordinatesList)
				 {
					 OutProxyMesh.WedgeTexCoords[TexCoordIndex].SetNumUninitialized(ToatalCorners);
					 for (int32 CornerIndex = 0; CornerIndex < ToatalCorners; ++CornerIndex)
					 {
						 OutProxyMesh.WedgeTexCoords[TexCoordIndex][CornerIndex].X = TexCoorChannel.Items[CornerIndex].V[0];
						 OutProxyMesh.WedgeTexCoords[TexCoordIndex][CornerIndex].Y = TexCoorChannel.Items[CornerIndex].V[1];
					 }
					 TexCoordIndex++;
				 }

				 //SSF Can store multiple color channels. However UE only supports one color channel
				 int32 ColorChannelIndex = 0;
				 for (ssf::ssfNamedList<ssf::ssfVector4> TexCoorChannel : MeshData->ColorsList)
				 {
					 OutProxyMesh.WedgeColors.SetNumUninitialized(ToatalCorners);
					 for (int32 CornerIndex = 0; CornerIndex < ToatalCorners; ++CornerIndex)
					 {
						 OutProxyMesh.WedgeColors[CornerIndex].R = TexCoorChannel.Items[CornerIndex].V[0];
						 OutProxyMesh.WedgeColors[CornerIndex].G = TexCoorChannel.Items[CornerIndex].V[1];
						 OutProxyMesh.WedgeColors[CornerIndex].B = TexCoorChannel.Items[CornerIndex].V[2];
						 OutProxyMesh.WedgeColors[CornerIndex].A = TexCoorChannel.Items[CornerIndex].V[3];
					 }
				 }
				
				 
				 bool Normals = !MeshData->Normals.IsEmpty() && MeshData->Normals.Get().Items.size() > 0;
				 bool Tangents = !MeshData->Tangents.IsEmpty() && MeshData->Tangents.Get().Items.size() > 0;
				 bool Bitangents = !MeshData->Bitangents.IsEmpty() && MeshData->Bitangents.Get().Items.size() > 0;
				 bool MaterialIndices = !MeshData->MaterialIndices.IsEmpty() && MeshData->MaterialIndices.Get().Items.size() > 0;
				 bool GroupIds = !MeshData->SmoothingGroup.IsEmpty() && MeshData->SmoothingGroup.Get().Items.size() > 0;

				 if (Normals)
				 {
					 if (Tangents && Bitangents)
					 {
						 OutProxyMesh.WedgeTangentX.SetNumUninitialized(ToatalCorners);
						 for (int32 WedgeIndex = 0; WedgeIndex < ToatalCorners; ++WedgeIndex)
						 {
							 OutProxyMesh.WedgeTangentX[WedgeIndex].X = MeshData->Tangents.Get().Items[WedgeIndex].V[0];
							 OutProxyMesh.WedgeTangentX[WedgeIndex].Y = MeshData->Tangents.Get().Items[WedgeIndex].V[1];
							 OutProxyMesh.WedgeTangentX[WedgeIndex].Z = MeshData->Tangents.Get().Items[WedgeIndex].V[2];
							 OutProxyMesh.WedgeTangentX[WedgeIndex] = GetConversionMatrix().TransformVector(OutProxyMesh.WedgeTangentX[WedgeIndex]);
						 }

						 OutProxyMesh.WedgeTangentY.SetNumUninitialized(ToatalCorners);
						 for (int32 WedgeIndex = 0; WedgeIndex < ToatalCorners; ++WedgeIndex)
						 {
							 OutProxyMesh.WedgeTangentY[WedgeIndex].X = MeshData->Bitangents.Get().Items[WedgeIndex].V[0];
							 OutProxyMesh.WedgeTangentY[WedgeIndex].Y = MeshData->Bitangents.Get().Items[WedgeIndex].V[1];
							 OutProxyMesh.WedgeTangentY[WedgeIndex].Z = MeshData->Bitangents.Get().Items[WedgeIndex].V[2];
							 OutProxyMesh.WedgeTangentY[WedgeIndex] = GetConversionMatrix().TransformVector(OutProxyMesh.WedgeTangentY[WedgeIndex]);
						 }
					 }

					 OutProxyMesh.WedgeTangentZ.SetNumUninitialized(ToatalCorners);
					 for (int32 WedgeIndex = 0; WedgeIndex < ToatalCorners; ++WedgeIndex)
					 {
						 //Normals->GetTuple(WedgeIndex, (float*)&OutProxyMesh.WedgeTangentZ[WedgeIndex]);
						 OutProxyMesh.WedgeTangentZ[WedgeIndex].X = MeshData->Normals.Get().Items[WedgeIndex].V[0];
						 OutProxyMesh.WedgeTangentZ[WedgeIndex].Y = MeshData->Normals.Get().Items[WedgeIndex].V[1];
						 OutProxyMesh.WedgeTangentZ[WedgeIndex].Z = MeshData->Normals.Get().Items[WedgeIndex].V[2];
						 OutProxyMesh.WedgeTangentZ[WedgeIndex] = GetConversionMatrix().TransformVector(OutProxyMesh.WedgeTangentZ[WedgeIndex]);
					 }
				 }

				 OutProxyMesh.FaceMaterialIndices.SetNumUninitialized(TotalTriangles);
				 if (MaterialIndices)
				 {
					 for (int32 TriIndex = 0; TriIndex < TotalTriangles; ++TriIndex)
					 {
						 OutProxyMesh.FaceMaterialIndices[TriIndex] = MeshData->MaterialIndices.Get().Items[TriIndex].Value;
					 }
				 }

				 OutProxyMesh.FaceSmoothingMasks.SetNumUninitialized(TotalTriangles);
				 if (GroupIds)
				 {
					 for (int32 TriIndex = 0; TriIndex < TotalTriangles; ++TriIndex)
					 {
						 OutProxyMesh.FaceSmoothingMasks[TriIndex] = MeshData->SmoothingGroup.Get().Items[TriIndex].Value;
					 }
				 }

			 }

			 //since its a proxy will only contain one material on it
			 ssf::ssfString ProxyMaterialGuid = Mesh->MaterialIds.Get().Items[0].Value;
			 
			 ssf::pssfMaterial ProxyMaterial = FindMaterialById(SceneGraph, ProxyMaterialGuid);

			 if (ProxyMaterial != nullptr)
			 {
				 SetupMaterail(SceneGraph, ProxyMaterial, OutMaterial, OutputFolderPath);
			 }

		 }
		 
	 }

	 void ExtractTextureDescriptors(ssf::pssfScene SceneGraph, 
		 ssf::pssfMaterialChannel Channel, 
		 FString OutputFolderPath,
		 FString ChannelName,
		 TArray<FColor>& OutSamples,
		 FIntPoint& OutTextureSize)
				 {
		 for (ssf::pssfMaterialChannelTextureDescriptor TextureDescriptor : Channel->MaterialChannelTextureDescriptorList)
		 {

			 //SceneGraph->TextureTable->TextureList.
			 ssf::pssfTexture Texture = FindTextureById(SceneGraph, TextureDescriptor->TextureID.Get().Value);
			 
			 if (Texture != nullptr)
			 {
				 FString TextureFilePath = FString::Printf(TEXT("%s/%s"), *OutputFolderPath, ANSI_TO_TCHAR(Texture->Path.Get().Value.c_str()));
				 CopyTextureData(OutSamples, OutTextureSize, ChannelName, TextureFilePath);
				 }
			 
			 }
	 }

	 void SetupMaterail(ssf::pssfScene SceneGraph, ssf::pssfMaterial Material, FFlattenMaterial &OutMaterial, FString OutputFolderPath)
			 {

		 for (ssf::pssfMaterialChannel Channel : Material->MaterialChannelList)
				 {
			 const FString ChannelName(ANSI_TO_TCHAR(Channel->ChannelName.Get().Value.c_str()));

			 if (ChannelName.Compare(BASECOLOR_CHANNEL) == 0)
			 {
				 ExtractTextureDescriptors(SceneGraph, Channel, OutputFolderPath, ChannelName, OutMaterial.DiffuseSamples, OutMaterial.DiffuseSize);
				 }
			 else if (ChannelName.Compare(NORMAL_CHANNEL) == 0)
				 {
				 ExtractTextureDescriptors(SceneGraph, Channel, OutputFolderPath, ChannelName, OutMaterial.NormalSamples, OutMaterial.NormalSize);
			 }
			 else if (ChannelName.Compare(SPECULAR_CHANNEL) == 0)
			 {
				 ExtractTextureDescriptors(SceneGraph, Channel, OutputFolderPath, ChannelName, OutMaterial.SpecularSamples, OutMaterial.SpecularSize);
			 }
			 else if (ChannelName.Compare(ROUGHNESS_CHANNEL) == 0)
			 {
				 ExtractTextureDescriptors(SceneGraph, Channel, OutputFolderPath, ChannelName, OutMaterial.RoughnessSamples, OutMaterial.RoughnessSize);
			 }
			 else if (ChannelName.Compare(METALLIC_CHANNEL) == 0)
			 {
				 ExtractTextureDescriptors(SceneGraph, Channel, OutputFolderPath, ChannelName, OutMaterial.MetallicSamples, OutMaterial.MetallicSize);
			 }
			 else if (ChannelName.Compare(OPACITY_CHANNEL) == 0)
			 {
				 ExtractTextureDescriptors(SceneGraph, Channel, OutputFolderPath, ChannelName, OutMaterial.OpacitySamples, OutMaterial.OpacitySize);
			 }
		 }
	 }

	 ssf::pssfTexture FindTextureById(ssf::pssfScene SceneGraph, ssf::ssfString TextureId)
	 {
		 auto Texture = std::find_if(SceneGraph->TextureTable->TextureList.begin(), SceneGraph->TextureTable->TextureList.end(),
			 [TextureId](const ssf::pssfTexture tex)
		 {
			 if (FSimplygonSSFHelper::CompareSSFStr(tex->Id.Get().Value, TextureId))
			 {
				 return true;
				 }
			 return false;
		 });


		 if (Texture != std::end(SceneGraph->TextureTable->TextureList))
		 {
			 if (!Texture->IsNull())
			 {
				 return *Texture;
			 }
		 }

		 return nullptr;
	 }

	 ssf::pssfMaterial FindMaterialById(ssf::pssfScene SceneGraph, ssf::ssfString MaterailId)
	 {
		 auto ProxyMaterial = std::find_if(SceneGraph->MaterialTable->MaterialList.begin(), SceneGraph->MaterialTable->MaterialList.end(),
			 [MaterailId](const ssf::pssfMaterial mat)
		 {
			 if (FSimplygonSSFHelper::CompareSSFStr(mat->Id.Get().Value, MaterailId))
			 {
				 return true;
			 }
			 return false;
		 });

		 if (ProxyMaterial != std::end(SceneGraph->MaterialTable->MaterialList))
		 {
			 if (!ProxyMaterial->IsNull())
			 {
				 return *ProxyMaterial;
			 }
		 }

		 return nullptr;
	 }

	 bool UnzipDownloadedContent(FString ZipFileName, FString OutputFolderPath)
	 {
		 if (!FPaths::FileExists(FPaths::ConvertRelativePathToFull(ZipFileName)))
			 return false;

		 FString CommandLine = FString::Printf(TEXT("x %s -o%s * -y"), *FPaths::ConvertRelativePathToFull(ZipFileName), *FPaths::ConvertRelativePathToFull(OutputFolderPath));

		 FString CmdExe = TEXT("cmd.exe");
		 FString ZipToolPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Extras/7-Zip/7z.exe"));

		 if (!FPaths::FileExists(ZipToolPath))
		 {
			 FFormatNamedArguments Arguments;
			 Arguments.Add(TEXT("File"), FText::FromString(ZipToolPath));
			 FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("RequiredFileNotFoundMessage", "A required file could not be found:\n{File}"), Arguments));
			 return false;
		 }

		 FString FullCommandLine = FString::Printf(TEXT("/c \"\"%s\" %s\""), *ZipToolPath, *CommandLine);
		 TSharedPtr<FMonitoredProcess> UatProcess = MakeShareable(new FMonitoredProcess(CmdExe, FullCommandLine, true));
		 UatProcess->Launch();

		 //ugly spin lock
		 while (UatProcess->IsRunning())
		 {
			 FPlatformProcess::Sleep(0.01f);
		 }

		 return true;
	 }

	 bool ZipContentsForUpload(FString InputDirectoryPath, FString OutputFileName)
	 {
		 FString CommandLine = FString::Printf(TEXT("a %s %s/*"), *FPaths::ConvertRelativePathToFull(OutputFileName), *FPaths::ConvertRelativePathToFull(InputDirectoryPath));

		 FString CmdExe = TEXT("cmd.exe");
		 FString ZipToolPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Extras/7-Zip/7z.exe"));

		 if (!FPaths::FileExists(ZipToolPath))
		 {
			 FFormatNamedArguments Arguments;
			 Arguments.Add(TEXT("File"), FText::FromString(ZipToolPath));
			 FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("RequiredFileNotFoundMessage", "A required file could not be found:\n{File}"), Arguments));
			 return false;
		 }

		 FString FullCommandLine = FString::Printf(TEXT("/c \"\"%s\" %s\""), *ZipToolPath, *CommandLine);
		 TSharedPtr<FMonitoredProcess> UatProcess = MakeShareable(new FMonitoredProcess(CmdExe, FullCommandLine, true));
		 UatProcess->Launch();
		 
		 //ugly spin lock
		 while (UatProcess->IsRunning())
		 {
			 FPlatformProcess::Sleep(0.01f);
		 }

		 return true;
	 }

	 void GetUniqueMaterialIndices(const TArray<int32>& OriginalMaterialIds, TArray<int32>& UniqueMaterialIds)
	 {
		 for (int32 index : OriginalMaterialIds)
		 {
			 UniqueMaterialIds.AddUnique(index);
		 }
	 }
	
	 struct FSkeletalMeshData
	 {
		 TArray<FVertInfluence> Influences;
		 TArray<FMeshWedge> Wedges;
		 TArray<FMeshFace> Faces;
		 TArray<FVector> Points;
		 uint32 TexCoordCount;
	 };

	

	 FString CreateSPLTemplateChannels(const FMaterialProxySettings& Settings)
	 {
		FString FinalTemplate;

		FinalTemplate += FString::Printf(TEXT("%s"), *FString::Printf(SPL_TEMPLATE_COLORCASTER, BASECOLOR_CHANNEL, BASECOLOR_CHANNEL, BASECOLOR_CHANNEL));
		
		if (Settings.bMetallicMap)
		{
			FinalTemplate += FString::Printf(TEXT(",%s"), *FString::Printf(SPL_TEMPLATE_COLORCASTER, METALLIC_CHANNEL, METALLIC_CHANNEL, METALLIC_CHANNEL));
		}

		if (Settings.bRoughnessMap)
		{
			FinalTemplate += FString::Printf(TEXT(",%s"), *FString::Printf(SPL_TEMPLATE_COLORCASTER, ROUGHNESS_CHANNEL, ROUGHNESS_CHANNEL, ROUGHNESS_CHANNEL));
		}

		if (Settings.bSpecularMap)
		{
			FinalTemplate += FString::Printf(TEXT(",%s"), *FString::Printf(SPL_TEMPLATE_COLORCASTER, SPECULAR_CHANNEL, SPECULAR_CHANNEL, SPECULAR_CHANNEL));
		}

		if (Settings.bNormalMap)
		{
			FinalTemplate += FString::Printf(TEXT(",%s"), *FString::Printf(SPL_TEMPLATE_NORMALCASTER, NORMAL_CHANNEL, NORMAL_CHANNEL));
		}

		return FinalTemplate;
	 }

	 /**
	  * Calculates the view distance that a mesh should be displayed at.
	  * @param MaxDeviation - The maximum surface-deviation between the reduced geometry and the original. This value should be acquired from Simplygon
	  * @returns The calculated view distance	 
	  */
	 float CalculateViewDistance( float MaxDeviation )
	 {
		 // We want to solve for the depth in world space given the screen space distance between two pixels
		 //
		 // Assumptions:
		 //   1. There is no scaling in the view matrix.
		 //   2. The horizontal FOV is 90 degrees.
		 //   3. The backbuffer is 1920x1080.
		 //
		 // If we project two points at (X,Y,Z) and (X',Y,Z) from view space, we get their screen
		 // space positions: (X/Z, Y'/Z) and (X'/Z, Y'/Z) where Y' = Y * AspectRatio.
		 //
		 // The distance in screen space is then sqrt( (X'-X)^2/Z^2 + (Y'-Y')^2/Z^2 )
		 // or (X'-X)/Z. This is in clip space, so PixelDist = 1280 * 0.5 * (X'-X)/Z.
		 //
		 // Solving for Z: ViewDist = (X'-X * 640) / PixelDist

		 const float ViewDistance = (MaxDeviation * 960.0f);
		 return ViewDistance;
	 }

	 static FIntPoint ComputeMappingImageSize(const FMaterialProxySettings& Settings)
	 {
		 FIntPoint ImageSize = Settings.TextureSize;/* Settings.BaseColorMapSize;
		 ImageSize = ImageSize.ComponentMax(Settings.NormalMapSize);
		 ImageSize = ImageSize.ComponentMax(Settings.MetallicMapSize);
		 ImageSize = ImageSize.ComponentMax(Settings.RoughnessMapSize);
		 ImageSize = ImageSize.ComponentMax(Settings.SpecularMapSize);*/
		 return ImageSize;
	 }

	 
	 /**
	  * The method converts ESimplygonTextureSamplingQuality
	  * @param InSamplingQuality - The Caster Settings used to setup the Simplygon Caster.
	  * @param InMappingImage - Simplygon MappingImage.
	  * @result 
	  */
	 uint8 GetSamples(ESimplygonTextureSamplingQuality::Type InSamplingQuality)
	 {
		 switch (InSamplingQuality)
		 {
		 case ESimplygonTextureSamplingQuality::Poor:
			 return 1;
		 case ESimplygonTextureSamplingQuality::Low:
			 return 2;
		 case ESimplygonTextureSamplingQuality::Medium:
			 return 6;
		 case ESimplygonTextureSamplingQuality::High:
			 return 8;
		 }
		 return 1;
	 }

	 uint8 ConvertColorChannelToInt(ESimplygonColorChannels::Type InSamplingQuality)
	 {
		 switch (InSamplingQuality)
		 {
		 case ESimplygonColorChannels::RGBA:
			 return 4;
		 case ESimplygonColorChannels::RGB:
			 return 3;
		 case ESimplygonColorChannels::L:
			 return 1;
		 }

		 return 3;
	 }

	
	 /*
	 *  (-1, 0, 0)
	 *  (0, 1, 0)
	 *  (0, 0, 1)
	 */
	 const FMatrix& GetConversionMatrix()
	 {
		 
		 static FMatrix m;
		 static bool bInitialized = false;
		 if (!bInitialized)
		 {
			 m.SetIdentity();
			 m.SetAxis(0, FVector(1, 0, 0));
			 m.SetAxis(1, FVector(0, 0, 1));
			 m.SetAxis(2, FVector(0, 1, 0));
			 bInitialized = true;
		 }

		 return m;		 
	 }

	 /*
	 *	(1,0,0)
	 *	(0,0,1)
	 *	(0,1,0)
	 */
	 const FMatrix& GetConversionMatrixYUP()
	 {

		 static FMatrix m;
		 static bool bInitialized = false;
		 if (!bInitialized)
		 {
			 m.SetIdentity();
			 m.SetAxis(0, FVector(1, 0, 0));
			 m.SetAxis(1, FVector(0, 0, 1));
			 m.SetAxis(2, FVector(0, 1, 0));
			 bInitialized = true;
		 }

		 return m;
	 }


	 ssf::pssfMeshData CreateSSFMeshDataFromRawMesh(const FRawMesh& RawMesh,TArray<FBox2D> TextureBounds,
		 TArray<FVector2D> InTexCoords)
	 {
		 int32 NumVertices = RawMesh.VertexPositions.Num();
		 int32 NumWedges = RawMesh.WedgeIndices.Num();
		 int32 NumTris = NumWedges / 3;

		 if (NumWedges == 0)
		 {
			 return NULL;
		 }

		 //assuming everything is left-handed so no need to change winding order and handedness. SSF supports both

		 ssf::pssfMeshData SgMeshData = new ssf::ssfMeshData();

		 //setup vertex coordinates
		 ssf::ssfList<ssf::ssfVector3> & SgCoords = SgMeshData->Coordinates.Create();
		 SgCoords.Items.resize(NumVertices);
		 for (int32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
		 {
			 ssf::ssfVector3 SgCurrentVertex;
			 FVector4 Position = GetConversionMatrixYUP().TransformPosition(RawMesh.VertexPositions[VertexIndex]);
			 SgCurrentVertex.V[0] = double(Position.X);
			 SgCurrentVertex.V[1] = double(Position.Y);
			 SgCurrentVertex.V[2] = double(Position.Z);
			 SgCoords.Items[VertexIndex] = SgCurrentVertex;
		 }

		 //setup triangle data
		 ssf::ssfList<ssf::ssfIndex3>& SgTriangleIds = SgMeshData->TriangleIndices.Create();
		 ssf::ssfList<ssf::ssfUInt32>& SgMaterialIds = SgMeshData->MaterialIndices.Create();
		 ssf::ssfList<ssf::ssfInt32>& SgSmoothingGroupIds = SgMeshData->SmoothingGroup.Create();

		 SgTriangleIds.Items.resize(NumTris);
		 SgMaterialIds.Items.resize(NumTris);
		 SgSmoothingGroupIds.Items.resize(NumTris);

		 for (int32 TriIndex = 0; TriIndex < NumTris; ++TriIndex)
		 {
			 SgTriangleIds.Items[TriIndex].V[0] = RawMesh.WedgeIndices[TriIndex * 3 + 0];
			 SgTriangleIds.Items[TriIndex].V[1] = RawMesh.WedgeIndices[TriIndex * 3 + 1];
			 SgTriangleIds.Items[TriIndex].V[2] = RawMesh.WedgeIndices[TriIndex * 3 + 2];
		 }

		 for (int32 TriIndex = 0; TriIndex < NumTris; ++TriIndex)
		 {
			 SgMaterialIds.Items[TriIndex] = RawMesh.FaceMaterialIndices[TriIndex];
			 SgSmoothingGroupIds.Items[TriIndex] = RawMesh.FaceSmoothingMasks[TriIndex];
		 }

		 SgMeshData->MaterialIndices.Create();

		 //setup texcoords
		 for (int32 TexCoordIndex = 0; TexCoordIndex < MAX_MESH_TEXTURE_COORDS; ++TexCoordIndex)
		 {
			 const TArray<FVector2D>& SrcTexCoords = (TexCoordIndex == 0 && InTexCoords.Num() == NumWedges) ? InTexCoords : RawMesh.WedgeTexCoords[TexCoordIndex];

			 if (SrcTexCoords.Num() == NumWedges)
			 {
				 ssf::ssfNamedList<ssf::ssfVector2> SgTexCoord;

				 //Since SSF uses Named Channels
				 SgTexCoord.Name = FSimplygonSSFHelper::TCHARToSSFString(*FString::Printf(TEXT("TexCoord%d"), TexCoordIndex));
				 SgTexCoord.Items.resize(NumWedges);

				 int32 WedgeIndex = 0;
				 for (int32 TriIndex = 0; TriIndex < NumTris; ++TriIndex)
				 {
					 int32 MaterialIndex = RawMesh.FaceMaterialIndices[TriIndex];
					 // Compute texture bounds for current material.
					 float MinU = 0, ScaleU = 1;
					 float MinV = 0, ScaleV = 1;
					 if (TextureBounds.IsValidIndex(MaterialIndex) && TexCoordIndex == 0 && InTexCoords.Num() == 0)
					 {
						 const FBox2D& Bounds = TextureBounds[MaterialIndex];
						 if (Bounds.GetArea() > 0)
						 {
							 MinU = Bounds.Min.X;
							 MinV = Bounds.Min.Y;
							 ScaleU = 1.0f / (Bounds.Max.X - Bounds.Min.X);
							 ScaleV = 1.0f / (Bounds.Max.Y - Bounds.Min.Y);
						 }
					 }
					 for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex, ++WedgeIndex)
					 {
						 const FVector2D& TexCoord = SrcTexCoords[WedgeIndex];
						  ssf::ssfVector2 temp;
						 temp.V[0] = (TexCoord.X - MinU) * ScaleU;
						 temp.V[1] = (TexCoord.Y - MinV) * ScaleV;
						 SgTexCoord.Items[WedgeIndex] = temp;
					 }
				 }

				 SgMeshData->TextureCoordinatesList.push_back(SgTexCoord);
			 }
		 }

		 //setup colors
		 if (RawMesh.WedgeColors.Num() == NumWedges)
		 {
			 //setup the color named channel . Currently its se to index zero. If multiple colors channel are need then use an index instead of 0
			 ssf::ssfNamedList<ssf::ssfVector4> SgColors;
			 SgColors.Name = FSimplygonSSFHelper::TCHARToSSFString(*FString::Printf(TEXT("Colors%d"), 0));
			 SgColors.Items.resize(NumWedges);
			 for (int32 WedgeIndex = 0; WedgeIndex < NumWedges; ++WedgeIndex)
			 {
				 FLinearColor LinearColor(RawMesh.WedgeColors[WedgeIndex]);
				 SgColors.Items[WedgeIndex].V[0] = LinearColor.R;
				 SgColors.Items[WedgeIndex].V[1] = LinearColor.G;
				 SgColors.Items[WedgeIndex].V[2] = LinearColor.B;
				 SgColors.Items[WedgeIndex].V[3] = LinearColor.A;
			 }
			 SgMeshData->ColorsList.push_back(SgColors);
		 }
		 
		 if (RawMesh.WedgeTangentZ.Num() == NumWedges)
		 {
			 if (RawMesh.WedgeTangentX.Num() == NumWedges && RawMesh.WedgeTangentY.Num() == NumWedges)
			 {
				 ssf::ssfList<ssf::ssfVector3> & SgTangents = SgMeshData->Tangents.Create();
				 SgTangents.Items.resize(NumWedges);
				 for (int32 WedgeIndex = 0; WedgeIndex < NumWedges; ++WedgeIndex)
				 {
					 ssf::ssfVector3 SsfTangent;
					 FVector4 Tangent = GetConversionMatrixYUP().TransformPosition(RawMesh.WedgeTangentX[WedgeIndex]);
					 SsfTangent.V[0] = double(Tangent.X);
					 SsfTangent.V[1] = double(Tangent.Y);
					 SsfTangent.V[2] = double(Tangent.Z);
					 SgTangents.Items[WedgeIndex] = SsfTangent;
				 }

				 ssf::ssfList<ssf::ssfVector3> & SgBitangents = SgMeshData->Bitangents.Create();
				 SgBitangents.Items.resize(NumWedges);
				 for (int32 WedgeIndex = 0; WedgeIndex < NumWedges; ++WedgeIndex)
				 {
					 ssf::ssfVector3 SsfBitangent;
					 FVector4 Bitangent = GetConversionMatrixYUP().TransformPosition(RawMesh.WedgeTangentY[WedgeIndex]);
					 SsfBitangent.V[0] = double(RawMesh.WedgeTangentY[WedgeIndex].X);
					 SsfBitangent.V[1] = double(RawMesh.WedgeTangentY[WedgeIndex].Y);
					 SsfBitangent.V[2] = double(RawMesh.WedgeTangentY[WedgeIndex].Z);
					 SgBitangents.Items[WedgeIndex] = SsfBitangent;
				 }
			 }

			 ssf::ssfList<ssf::ssfVector3> & SgNormals = SgMeshData->Normals.Create();
			 SgNormals.Items.resize(NumWedges);
			 for (int32 WedgeIndex = 0; WedgeIndex < NumWedges; ++WedgeIndex)
			 {
				 ssf::ssfVector3 SsfNormal;
				 FVector4 Normal = GetConversionMatrixYUP().TransformPosition(RawMesh.WedgeTangentZ[WedgeIndex]);
				 SsfNormal.V[0] = double(Normal.X);
				 SsfNormal.V[1] = double(Normal.Y);
				 SsfNormal.V[2] = double(Normal.Z);
				 SgNormals.Items[WedgeIndex] = SsfNormal;
			 }
		 }

		 return SgMeshData;

	 }

	 ssf::pssfMeshData CreateSSFMeshDataFromRawMesh(const FRawMesh& RawMesh)
	 {
		 int32 NumVertices = RawMesh.VertexPositions.Num();
		 int32 NumWedges = RawMesh.WedgeIndices.Num();
		 int32 NumTris = NumWedges / 3;

		 if (NumWedges == 0)
		 {
			 return NULL;
		 }

		 //assuming everything is left-handed so no need to change winding order and handedness. SSF supports both

		 ssf::pssfMeshData SgMeshData = new ssf::ssfMeshData();

		 //setup vertex coordinates
		 ssf::ssfList<ssf::ssfVector3> & SgCoords = SgMeshData->Coordinates.Create();
		 SgCoords.Items.resize(NumVertices);
		 for (int32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
		 {
			 ssf::ssfVector3 SgCurrentVertex;
			 FVector4 Position = GetConversionMatrixYUP().TransformPosition(RawMesh.VertexPositions[VertexIndex]);
			 SgCurrentVertex.V[0] = double(Position.X);
			 SgCurrentVertex.V[1] = double(Position.Y);
			 SgCurrentVertex.V[2] = double(Position.Z);
			 SgCoords.Items[VertexIndex] = SgCurrentVertex;
		 }

		 //setup triangle data
		 ssf::ssfList<ssf::ssfIndex3>& SgTriangleIds = SgMeshData->TriangleIndices.Create();
		 ssf::ssfList<ssf::ssfUInt32>& SgMaterialIds = SgMeshData->MaterialIndices.Create();
		 ssf::ssfList<ssf::ssfInt32>& SgSmoothingGroupIds = SgMeshData->SmoothingGroup.Create();

		 SgTriangleIds.Items.resize(NumTris);
		 SgMaterialIds.Items.resize(NumTris);
		 SgSmoothingGroupIds.Items.resize(NumTris);

		 for (int32 TriIndex = 0; TriIndex < NumTris; ++TriIndex)
		 {
			 SgTriangleIds.Items[TriIndex].V[0] = RawMesh.WedgeIndices[TriIndex * 3 + 0];
			 SgTriangleIds.Items[TriIndex].V[1] = RawMesh.WedgeIndices[TriIndex * 3 + 1];
			 SgTriangleIds.Items[TriIndex].V[2] = RawMesh.WedgeIndices[TriIndex * 3 + 2];
		 }

		 for (int32 TriIndex = 0; TriIndex < NumTris; ++TriIndex)
		 {
			 SgMaterialIds.Items[TriIndex] = RawMesh.FaceMaterialIndices[TriIndex];
			 SgSmoothingGroupIds.Items[TriIndex] = RawMesh.FaceSmoothingMasks[TriIndex];
		 }
		 
		 SgMeshData->MaterialIndices.Create();

		 //setup texcoords
		 for (int32 TexCoordIndex = 0; TexCoordIndex < MAX_MESH_TEXTURE_COORDS; ++TexCoordIndex)
		 {
			 if (RawMesh.WedgeTexCoords[TexCoordIndex].Num() == NumWedges)
			 {
				 ssf::ssfNamedList<ssf::ssfVector2> SgTexCoord;

				 //Since SSF uses Named Channels
				 SgTexCoord.Name = FSimplygonSSFHelper::TCHARToSSFString(*FString::Printf(TEXT("TexCoord%d"), TexCoordIndex));
				 SgTexCoord.Items.resize(NumWedges);

				 for (int WedgeIndex = 0; WedgeIndex < NumWedges; WedgeIndex++)
				 {
					 ssf::ssfVector2 temp;
					 temp.V[0] = RawMesh.WedgeTexCoords[TexCoordIndex][WedgeIndex].X;
					 temp.V[1] = RawMesh.WedgeTexCoords[TexCoordIndex][WedgeIndex].Y;
					 SgTexCoord.Items[WedgeIndex] = temp;
				 }
				 SgMeshData->TextureCoordinatesList.push_back(SgTexCoord);
			 }
		 }

		 //setup colors
		 if (RawMesh.WedgeColors.Num() == NumWedges)
		 {
			 //setup the color named channel . Currently its se to index zero. If multiple colors channel are need then use an index instead of 0
			 ssf::ssfNamedList<ssf::ssfVector4> SgColors;
			 SgColors.Name = FSimplygonSSFHelper::TCHARToSSFString(*FString::Printf(TEXT("Colors%d"), 0));
			 SgColors.Items.resize(NumWedges);
			 for (int32 WedgeIndex = 0; WedgeIndex < NumWedges; ++WedgeIndex)
			 {
				 FLinearColor LinearColor(RawMesh.WedgeColors[WedgeIndex]);
				 SgColors.Items[WedgeIndex].V[0] = LinearColor.R;
				 SgColors.Items[WedgeIndex].V[1] = LinearColor.G;
				 SgColors.Items[WedgeIndex].V[2] = LinearColor.B;
				 SgColors.Items[WedgeIndex].V[3] = LinearColor.A;
			 }
			 SgMeshData->ColorsList.push_back(SgColors);
		 }

		 if (RawMesh.WedgeTangentZ.Num() == NumWedges)
		 {
			 if (RawMesh.WedgeTangentX.Num() == NumWedges && RawMesh.WedgeTangentY.Num() == NumWedges)
			 {
				 ssf::ssfList<ssf::ssfVector3> & SgTangents = SgMeshData->Tangents.Create();
				 SgTangents.Items.resize(NumWedges);
				 for (int32 WedgeIndex = 0; WedgeIndex < NumWedges; ++WedgeIndex)
				 {
					 ssf::ssfVector3 SsfTangent;
					 FVector4 Tangent = GetConversionMatrixYUP().TransformPosition(RawMesh.WedgeTangentX[WedgeIndex]);
					 SsfTangent.V[0] = double(Tangent.X);
					 SsfTangent.V[1] = double(Tangent.Y);
					 SsfTangent.V[2] = double(Tangent.Z);
					 SgTangents.Items[WedgeIndex] = SsfTangent;
				 }

				 ssf::ssfList<ssf::ssfVector3> & SgBitangents = SgMeshData->Bitangents.Create();
				 SgBitangents.Items.resize(NumWedges);
				 for (int32 WedgeIndex = 0; WedgeIndex < NumWedges; ++WedgeIndex)
				 {
					 ssf::ssfVector3 SsfBitangent;
					 FVector4 Bitangent = GetConversionMatrixYUP().TransformPosition(RawMesh.WedgeTangentY[WedgeIndex]);
					 SsfBitangent.V[0] = double(RawMesh.WedgeTangentY[WedgeIndex].X);
					 SsfBitangent.V[1] = double(RawMesh.WedgeTangentY[WedgeIndex].Y);
					 SsfBitangent.V[2] = double(RawMesh.WedgeTangentY[WedgeIndex].Z);
					 SgBitangents.Items[WedgeIndex] = SsfBitangent;
				 }
			 }

			 ssf::ssfList<ssf::ssfVector3> & SgNormals = SgMeshData->Normals.Create();
			 SgNormals.Items.resize(NumWedges);
			 for (int32 WedgeIndex = 0; WedgeIndex < NumWedges; ++WedgeIndex)
			 {
				 ssf::ssfVector3 SsfNormal;
				 FVector4 Normal = GetConversionMatrixYUP().TransformPosition(RawMesh.WedgeTangentZ[WedgeIndex]);
				 SsfNormal.V[0] = double(Normal.X);
				 SsfNormal.V[1] = double(Normal.Y);
				 SsfNormal.V[2] = double(Normal.Z);
				 SgNormals.Items[WedgeIndex] = SsfNormal;
			 }
		 }

		 return SgMeshData;
	 }

	 void CopyTextureData(
		 TArray<FColor>& OutSamples,
		 FIntPoint& OutTextureSize,
		 FString ChannelName,
		 FString InputPath, 
		 bool IsNormalMap = false
		 )
	 {
		 IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
		 IImageWrapperPtr ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
		 
		 //if (ImageWrapper.IsValid() && ImageWrapper->GetRaw()
		 TArray<uint8> TextureData;
		 if (!FFileHelper::LoadFileToArray(TextureData, *FPaths::ConvertRelativePathToFull(InputPath)) && TextureData.Num() > 0)
		 {
			 UE_LOG(LogSimplygonSwarm, Warning, TEXT("Unable to find Texture file %s"), *InputPath);
		 }
		 else
		 {
			 const TArray<uint8>* RawData = NULL;
			 
			 if (ImageWrapper->SetCompressed(TextureData.GetData(), TextureData.Num()) && ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData))
			 {
				 OutTextureSize.X = ImageWrapper->GetHeight();
				 OutTextureSize.Y = ImageWrapper->GetWidth();
				 int32 TexelsCount = ImageWrapper->GetHeight() * ImageWrapper->GetWidth();
				 OutSamples.Empty(TexelsCount);
				 OutSamples.AddUninitialized(TexelsCount);

				 for (int32 X = 0; X < ImageWrapper->GetHeight(); ++X)
				 {
					 for (int32 Y = 0; Y < ImageWrapper->GetWidth(); ++Y)
					 {
						 int32 PixelIndex = ImageWrapper->GetHeight() * X + Y;

						 OutSamples[PixelIndex].B = (*RawData)[PixelIndex*sizeof(FColor) + 0];
						 OutSamples[PixelIndex].G = (*RawData)[PixelIndex*sizeof(FColor) + 1];
						 OutSamples[PixelIndex].R = (*RawData)[PixelIndex*sizeof(FColor) + 2];
						 OutSamples[PixelIndex].A = (*RawData)[PixelIndex*sizeof(FColor) + 3];
					 }
				}				 
			 }

			 //just for the fun of it write it out
			
		 }
	 }

	 ssf::pssfMaterialChannel SetMaterialChannelData(
		 const TArray<FColor>& InSamples,
		 FIntPoint InTextureSize,
		 ssf::pssfTextureTable SgTextureTable,
		 FString ChannelName, FString TextureName,FString OutputPath, bool IsSRGB = true)
	 {

		 ssf::pssfMaterialChannel SgMaterialChannel = new ssf::ssfMaterialChannel();
		 SgMaterialChannel->ChannelName.Set(FSimplygonSSFHelper::TCHARToSSFString(*ChannelName));

		 if (InSamples.Num() >= 1)
		 {

			 IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
			 IImageWrapperPtr ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
			
			 FString TextureOutputRelative = FString::Printf(TEXT("%s/%s.png"), ANSI_TO_TCHAR(SgTextureTable->TexturesDirectory->Value.c_str()), *TextureName);
			 FString TextureOutputPath = FString::Printf(TEXT("%s%s"), *OutputPath, *TextureOutputRelative);
			 
			 if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(&InSamples[0], InSamples.Num() * sizeof(FColor), InTextureSize.X, InTextureSize.Y, ERGBFormat::BGRA, 8))
			 {
				 if (FFileHelper::SaveArrayToFile(ImageWrapper->GetCompressed(), *TextureOutputPath))
				 {
					 ssf::pssfTexture SgTexture = new ssf::ssfTexture();
					 ssf::pssfMaterialChannelTextureDescriptor SgTextureDescriptor = new ssf::ssfMaterialChannelTextureDescriptor();
					 SgTexture->Id.Set(FSimplygonSSFHelper::SSFNewGuid());
					 SgTexture->Name.Set(FSimplygonSSFHelper::TCHARToSSFString(*TextureName));
					 SgTexture->Path.Set(FSimplygonSSFHelper::TCHARToSSFString(*TextureOutputRelative));
					 SgTextureDescriptor->TextureID.Set(SgTexture->Id.Get());

					 //TODO: Some how get the TexCoord Channel information down here. This is a hack that will only work for a very simple mesh
					 SgTextureDescriptor->TexCoordSet.Set(FSimplygonSSFHelper::TCHARToSSFString(TEXT("TexCoord0")));

					 SgMaterialChannel->MaterialChannelTextureDescriptorList.push_back(SgTextureDescriptor);
					 FString ShadingNetwork = FString::Printf(SHADING_NETWORK_TEMPLATE, *TextureName, TEXT("TexCoord0"), (int)IsSRGB);
					 SgMaterialChannel->ShadingNetwork.Set(FSimplygonSSFHelper::TCHARToSSFString(*ShadingNetwork));
					 SgTextureTable->TextureList.push_back(SgTexture);
				 }
				 else
				 {
					 //UE_LOG(LogSimplygonSwarm, Log, TEXT("%s"), TEXT("Could not save textrue to file");
				 }
				 
			 }

		 }
		 else
		 {
			// InSGMaterial->SetColorRGB(SGMaterialChannelName, 1.0f, 1.0f, 1.0f);
			 SgMaterialChannel->Color.Create();
			 SgMaterialChannel->Color->V[0] = 1.0f;
			 SgMaterialChannel->Color->V[1] = 1.0f;
			 SgMaterialChannel->Color->V[2] = 1.0f;
			 SgMaterialChannel->Color->V[3] = 1.0f;
		 }

		 return SgMaterialChannel;
	 }

	 bool CreateSSFMaterialFromFlattenMaterial(
		 const TArray<FFlattenMaterial>& InputMaterials,
		 const FMaterialProxySettings& InMaterialLODSettings,
		 ssf::pssfMaterialTable SgMaterialTable,
		 ssf::pssfTextureTable SgTextureTable,
		 FString OutputFolderPath,
		 bool bReleaseInputMaterials, TMap<int,FString>& MaterialMapping)
	 {
		 if (InputMaterials.Num() == 0)
		 {
			 //If there are no materials, feed Simplygon with a default material instead.
			 UE_LOG(LogSimplygonSwarm, Log, TEXT("Input meshes do not contain any materials. A proxy without material will be generated."));
			 return false;
		 }
		 
		 for (int32 MaterialIndex = 0; MaterialIndex < InputMaterials.Num(); MaterialIndex++)
		 {
			FString MaterialGuidString = FGuid::NewGuid().ToString();
			const FFlattenMaterial& FlattenMaterial = InputMaterials[MaterialIndex];
			FString MaterialName = FString::Printf(TEXT("Material%d"), MaterialIndex);

			ssf::pssfMaterial SgMaterial = new ssf::ssfMaterial();
			SgMaterial->Id.Set(FSimplygonSSFHelper::TCHARToSSFString(*MaterialGuidString));
			SgMaterial->Name.Set(FSimplygonSSFHelper::TCHARToSSFString(*MaterialName));

			MaterialMapping.Add(MaterialIndex, MaterialGuidString);


			// Does current material have BaseColor?
			if (FlattenMaterial.DiffuseSamples.Num())
			{
				FString ChannelName(BASECOLOR_CHANNEL);
				ssf::pssfMaterialChannel BaseColorChannel = SetMaterialChannelData(FlattenMaterial.DiffuseSamples, FlattenMaterial.DiffuseSize, SgTextureTable, ChannelName, FString::Printf(TEXT("%s%s"), *MaterialName, *ChannelName), OutputFolderPath);
				 
			// if (InMaterialLODSettings.ChannelsToCast[0].bBakeVertexColors)
				//{
					//This shit does not work with ssf unless a vertedc color  node with a channel is set which is multiplied with the base color
					//SgMaterial->MaterialChannelList.push_back()
					//SGMaterial->SetVertexColorChannel(SimplygonSDK::SG_MATERIAL_CHANNEL_BASECOLOR, 0);
				//}
				SgMaterial->MaterialChannelList.push_back(BaseColorChannel);
			}

			// Does current material have Metallic?
			if (FlattenMaterial.MetallicSamples.Num())
			{
				FString ChannelName(METALLIC_CHANNEL);
				ssf::pssfMaterialChannel MetallicChannel = SetMaterialChannelData(FlattenMaterial.MetallicSamples, FlattenMaterial.MetallicSize, SgTextureTable, ChannelName, FString::Printf(TEXT("%s%s"), *MaterialName, *ChannelName), OutputFolderPath);
				SgMaterial->MaterialChannelList.push_back(MetallicChannel);
			}

			// Does current material have Specular?
			if (FlattenMaterial.SpecularSamples.Num())
			{
				FString ChannelName(SPECULAR_CHANNEL);
				ssf::pssfMaterialChannel SpecularChannel = SetMaterialChannelData(FlattenMaterial.SpecularSamples, FlattenMaterial.SpecularSize, SgTextureTable, ChannelName, FString::Printf(TEXT("%s%s"), *MaterialName, *ChannelName), OutputFolderPath);
				SgMaterial->MaterialChannelList.push_back(SpecularChannel);
			}

			// Does current material have Roughness?
			if (FlattenMaterial.RoughnessSamples.Num())
			{
				FString ChannelName(ROUGHNESS_CHANNEL);
				ssf::pssfMaterialChannel RoughnessChannel = SetMaterialChannelData(FlattenMaterial.RoughnessSamples, FlattenMaterial.RoughnessSize, SgTextureTable, ChannelName, FString::Printf(TEXT("%s%s"), *MaterialName, *ChannelName), OutputFolderPath);
				SgMaterial->MaterialChannelList.push_back(RoughnessChannel);
			}

			//Does current material have a normalmap?
			if (FlattenMaterial.NormalSamples.Num())
			{
				FString ChannelName(NORMAL_CHANNEL);
				SgMaterial->TangentSpaceNormals.Create();
				SgMaterial->TangentSpaceNormals.Set(true);
				ssf::pssfMaterialChannel NormalChannel = SetMaterialChannelData(FlattenMaterial.NormalSamples, FlattenMaterial.NormalSize, SgTextureTable, ChannelName, FString::Printf(TEXT("%s%s"), *MaterialName, *ChannelName), OutputFolderPath, false);
				SgMaterial->MaterialChannelList.push_back(NormalChannel);
			}

			// Does current material have Opacity?
			if (FlattenMaterial.OpacitySamples.Num())
			{
				FString ChannelName(OPACITY_CHANNEL);
				ssf::pssfMaterialChannel OpacityChannel = SetMaterialChannelData(FlattenMaterial.OpacitySamples, FlattenMaterial.OpacitySize, SgTextureTable, ChannelName, FString::Printf(TEXT("%s%s"), *MaterialName, *ChannelName), OutputFolderPath);
				SgMaterial->MaterialChannelList.push_back(OpacityChannel);
			}

			if (FlattenMaterial.EmissiveSamples.Num())
			{
				FString ChannelName(EMISSIVE_CHANNEL);
				ssf::pssfMaterialChannel EmissiveChannel = SetMaterialChannelData(FlattenMaterial.EmissiveSamples, FlattenMaterial.EmissiveSize, SgTextureTable, ChannelName, FString::Printf(TEXT("%s%s"), *MaterialName, *ChannelName), OutputFolderPath);
				SgMaterial->MaterialChannelList.push_back(EmissiveChannel);
			}
			else
			{
				FString ChannelName(EMISSIVE_CHANNEL);
				TArray<FColor> BlackEmissive;
				BlackEmissive.AddZeroed(1);
				ssf::pssfMaterialChannel EmissiveChannel = SetMaterialChannelData(FlattenMaterial.EmissiveSamples, FlattenMaterial.EmissiveSize, SgTextureTable, ChannelName, FString::Printf(TEXT("%s%s"), *MaterialName, *ChannelName), OutputFolderPath);
				SgMaterial->MaterialChannelList.push_back(EmissiveChannel);
			}

			SgMaterialTable->MaterialList.push_back(SgMaterial);

			if (bReleaseInputMaterials)
			{
				// Release FlattenMaterial. Using const_cast here to avoid removal of "const" from input data here
				// and above the call chain.
				const_cast<FFlattenMaterial*>(&FlattenMaterial)->ReleaseData();
			}
		}

		return true;
	 }


 };

 TScopedPointer<FSimplygonSwarm> GSimplygonMeshReduction;


 void FSimplygonSwarmModule::StartupModule()
 {
	 GSimplygonMeshReduction = FSimplygonSwarm::Create();
 }

 void FSimplygonSwarmModule::ShutdownModule()
 {

 }

 IMeshReduction* FSimplygonSwarmModule::GetMeshReductionInterface()
 {
	 return nullptr;
 }

 IMeshMerging* FSimplygonSwarmModule::GetMeshMergingInterface()
 {
	 return GSimplygonMeshReduction;
 }


#undef LOCTEXT_NAMESPACE
