// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
static const TCHAR* OPACITY_MASK_CHANNEL = TEXT("Opacity Mask");
static const TCHAR* AO_CHANNEL = TEXT("AmbientOcclusion");
static const TCHAR* OUTPUT_LOD = TEXT("outputlod_0");
static const TCHAR* SSF_FILE_TYPE = TEXT("ssf");

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


//static const TCHAR* SPL_TEMPLATE_REMESHING = TEXT("{\"Header\":{\"SPLVersion\":\"7.0\",\"ClientName\":\"UE4\",\"ClientVersion\":\"UE4.9\",\"SimplygonVersion\":\"7.0\"},\"ProcessGraph\":{\"Type\":\"ContainerNode\",\"Name\":\"Node\",\"Children\":[{\"Processor\":{\"RemeshingSettings\":{\"CuttingPlaneSelectionSetName\":0,\"EmptySpaceOverride\":0.0,\"MaxTriangleSize\":32,\"OnScreenSize\":%d,\"ProcessSelectionSetName\":\"\",\"SurfaceTransferMode\":1,\"TransferColors\":false,\"TransferNormals\":false,\"UseCuttingPlanes\":%s,\"UseEmptySpaceOverride\":false,\"Enabled\":true%s},\"MappingImageSettings\":{\"AutomaticTextureSizeMultiplier\":1.0,\"ChartAggregatorMode\":0,\"ChartAggregatorOriginalTexCoordLevel\":0,\"ChartAggregatorUseAreaWeighting\":true,\"ChartAggregatorKeepOriginalChartProportions\":true,\"ChartAggregatorKeepOriginalChartProportionsFromChannel\":\"\",\"ChartAggregatorKeepOriginalChartSizes\":false,\"ChartAggregatorSeparateOverlappingCharts\":true,\"ForcePower2Texture\":true,\"GenerateMappingImage\":true,\"GenerateTangents\":true,\"GenerateTexCoords\":true,\"GutterSpace\":4,\"Height\":%d,\"MaximumLayers\":3,\"MultisamplingLevel\":3,\"ParameterizerMaxStretch\":6.0,\"ParameterizerUseVertexWeights\":false,\"ParameterizerUseVisibilityWeights\":false,\"TexCoordGeneratorType\":0,\"TexCoordLevel\":255,\"UseAutomaticTextureSize\":false,\"UseFullRetexturing\":false,\"Width\":%d,\"Enabled\":true},%s\"Type\":\"RemeshingProcessor\"},\"MaterialCaster\":[%s],\"DefaultTBNType\":2,\"AllowGPUAcceleration\":false,\"Type\":\"ProcessNode\",\"Name\":\"Node\",\"Children\":[{\"Format\":\"ssf\",\"Type\":\"WriteNode\",\"Name\":\"outputlod_0\",\"Children\":[]}]}]}}");
static const TCHAR* CLIPINGSPACEOVERRIDE = TEXT("\"ClippingGeometryEmptySpaceOverride\":{\"PointX\":%f,\"PointY\":%f,\"PointZ\":%f}");
static const TCHAR* SPL_TEMPLATE_REMESHING = TEXT("{\"Header\":{\"SPLVersion\":\"7.0\",\"ClientName\":\"UE4\",\"ClientVersion\":\"UE4.9\",\"SimplygonVersion\":\"7.0\"},\"ProcessGraph\":{\"Type\":\"ContainerNode\",\"Name\":\"Node\",\"Children\":[{\"Processor\":{\"RemeshingSettings\":{\"CuttingPlaneSelectionSetName\":0,\"UseClippingGeometry\":%s,\"ClippingGeometrySelectionSetName\":\"ClippingObjectSet\",\"UseClippingGeometryEmptySpaceOverride\":%s,\"EmptySpaceOverride\":0.0,\"OnScreenSize\":%d,\"ProcessSelectionSetName\":\"RemeshingProcessingSet\",\"SurfaceTransferMode\":1,\"TransferColors\":false,\"TransferNormals\":false,\"UseCuttingPlanes\":%s,\"UseEmptySpaceOverride\":false,\"Enabled\":true%s},\"MappingImageSettings\":{\"AutomaticTextureSizeMultiplier\":1.0,\"ChartAggregatorMode\":0,\"ChartAggregatorOriginalTexCoordLevel\":0,\"ChartAggregatorUseAreaWeighting\":true,\"ChartAggregatorKeepOriginalChartProportions\":true,\"ChartAggregatorKeepOriginalChartProportionsFromChannel\":\"\",\"ChartAggregatorKeepOriginalChartSizes\":false,\"ChartAggregatorSeparateOverlappingCharts\":true,\"ForcePower2Texture\":true,\"GenerateMappingImage\":true,\"GenerateTangents\":true,\"GenerateTexCoords\":true,\"GutterSpace\":%d,\"Height\":%d,\"MaximumLayers\":3,\"MultisamplingLevel\":3,\"ParameterizerMaxStretch\":6.0,\"ParameterizerUseVertexWeights\":false,\"ParameterizerUseVisibilityWeights\":false,\"TexCoordGeneratorType\":0,\"TexCoordLevel\":255,\"UseAutomaticTextureSize\":false,\"UseFullRetexturing\":false,\"Width\":%d,\"Enabled\":true}%s,%s\"Type\":\"RemeshingProcessor\"},\"MaterialCaster\":[%s],\"DefaultTBNType\":2,\"AllowGPUAcceleration\":false,\"Type\":\"ProcessNode\",\"Name\":\"Node\",\"Children\":[{\"Format\":\"ssf\",\"Type\":\"WriteNode\",\"Name\":\"outputlod_0\",\"Children\":[]}]}]}}");
 
static const TCHAR* SPL_TEMPLATE_AGGREGATION = TEXT("{\"Header\":{\"SPLVersion\":\"7.0\",\"ClientName\":\"\",\"ClientVersion\":\"\",\"SimplygonVersion\":\"\"},\"ProcessGraph\":{\"Type\":\"ContainerNode\",\"Name\":\"Node\",\"Children\":[{\"Processor\":{\"AggregationSettings\":{\"BaseAtlasOnOriginalTexCoords\":true,\"ProcessSelectionSetName\":\"\",\"Enabled\":true},\"MappingImageSettings\":{\"AutomaticTextureSizeMultiplier\":1.0,\"ChartAggregatorMode\":%d,\"ChartAggregatorOriginalTexCoordLevel\":0,\"ChartAggregatorSeparateOverlappingCharts\":false,\"ForcePower2Texture\":false,\"GenerateMappingImage\":true,\"GenerateTangents\":false,\"GenerateTexCoords\":true,\"GutterSpace\":%d,\"Height\":%d,\"MaximumLayers\":1,\"MultisamplingLevel\":2,\"TexCoordGeneratorType\":1,\"TexCoordLevel\":255,\"UseAutomaticTextureSize\":false,\"UseFullRetexturing\":false,\"Width\":%d,\"Enabled\":true}%s,\"Type\":\"AggregationProcessor\"},\"MaterialCaster\":[%s],\"DefaultTBNType\":2,\"AllowGPUAcceleration\":false,\"Type\":\"ProcessNode\",\"Name\":\"Node\",\"Children\":[{\"Format\":\"ssf\",\"Type\":\"WriteNode\",\"Name\":\"outputlod_0\",\"Children\":[]}]}]}}");

static const TCHAR* SPL_TEMPLATE_COLORCASTER = TEXT("{\"BakeOpacityInAlpha\":false,\"ColorType\":\"%s\",\"Dilation\":8,\"FillMode\":0,\"IsSRGB\":true,\"OutputChannelBitDepth\":8,\"OutputChannels\":4,\"Type\":\"ColorCaster\",\"Name\":\"%s\",\"Channel\":\"%s\",\"DitherType\":0}");

static const TCHAR* SPL_TEMPLATE_OPACITYCASTER = TEXT("{\"BakeOpacityInAlpha\":false,\"ColorType\":\"%s\",\"Dilation\":8,\"FillMode\":0,\"IsSRGB\":true,\"OutputChannelBitDepth\":8,\"OutputChannels\":4,\"Type\":\"OpacityCaster\",\"Name\":\"%s\",\"Channel\":\"%s\",\"DitherType\":0}");

static const TCHAR* SPL_NORMAL_RECALC = TEXT("\"HardEdgeAngleInRadians\":%f");

static const TCHAR* SPL_MERGE_DIST = TEXT("\"MergeDistance\":%f");

static const TCHAR* SPL_VISIBILITY = TEXT(",\"VisibilitySettings\":{\"UseCustomVisibilitySphere\": true,\"CustomVisibilitySphereFidelity\": 5,\"CustomVisibilitySphereYaw\": 0.0,\"CustomVisibilitySpherePitch\": 0.0,\"CustomVisibilitySphereCoverage\": 180.0,\"CameraSelectionSetName\": \"\",\"CullOccludedGeometry\": true,\"ForceVisibilityCalculation\": false,\"OccluderSelectionSetName\": \"\",\"UseBackfaceCulling\": false,\"UseVisibilityWeightsInReducer\": true,\"UseVisibilityWeightsInTexcoordGenerator\": true,\"VisibilityWeightsPower\": 1.0,\"Enabled\": true}");

static const TCHAR* CUTTING_PLANE_SETTINGS = TEXT("\"CuttingPlaneSettings\": [%s],");
 
static const TCHAR* CUTTING_PLANE = TEXT("{\"PointX\": %f,\"PointY\": %f,\"PointZ\": %f,\"NormalX\": %f,\"NormalY\": %f,\"NormalZ\": %f,\"Enabled\": true}");

static const TCHAR* SPL_TEMPLATE_NORMALCASTER = TEXT("{\"Dilation\":8,\"FillMode\":1,\"FlipBackfacingNormals\":false,\"FlipGreen\":false,\"GenerateTangentSpaceNormals\":true,\"NormalMapTextureLevel\":0,\"OutputChannelBitDepth\":8,\"OutputChannels\":3,\"Type\":\"NormalCaster\",\"Name\":\"%s\",\"Channel\":\"%s\",\"DitherType\":0}");

//static const TCHAR* SHADING_NETWORK_TEMPLATE = TEXT("<SimplygonShadingNetwork version=\"1.0\">\n\t<ShadingTextureNode ref=\"node_0\" name=\"ShadingTextureNode\">\n\t\t<DefaultColor0>\n\t\t\t<DefaultValue>1 1 1 1</DefaultValue>\n\t\t</DefaultColor0>\n\t\t<TextureName>%s</TextureName>\n\t\t<TextureLevelName>%s</TextureLevelName>\n\t\t<UseSRGB>%d</UseSRGB>\n\t\t<TileU>1.000000</TileU>\n\t\t<TileV>1.000000</TileV>\n\t</ShadingTextureNode>\n</SimplygonShadingNetwork>");

#if SPL_CURRENT_VERSION > 7
static const TCHAR* SHADING_NETWORK_TEMPLATE = TEXT("<SimplygonShadingNetwork version=\"1.0\">\n\t<ShadingTextureNode ref=\"node_0\" name=\"ShadingTextureNode\">\n\t\t<DefaultColor0>\n\t\t\t<DefaultValue>1 1 1 1</DefaultValue>\n\t\t</DefaultColor0>\n\t\t<TextureName>%s</TextureName>\n\t\t<TextureLevelName>%s</TextureLevelName>\n\t\t<UseSRGB>%d</UseSRGB>\n\t\t<TileU>1.000000</TileU>\n\t\t<TileV>1.000000</TileV>\n\t</ShadingTextureNode>\n</SimplygonShadingNetwork>");
#else
static const TCHAR* SHADING_NETWORK_TEMPLATE = TEXT("<SimplygonShadingNetwork version=\"1.0\">\n\t<ShadingTextureNode ref=\"node_0\" name=\"ShadingTextureNode\">\n\t\t<DefaultColor0>\n\t\t\t<DefaultValue>1 1 1 1</DefaultValue>\n\t\t</DefaultColor0>\n\t\t<TextureName>%s</TextureName>\n\t\t<TexCoordSet>%s</TexCoordSet>\n\t\t<UseSRGB>%d</UseSRGB>\n\t\t<TileU>1.000000</TileU>\n\t\t<TileV>1.000000</TileV>\n\t</ShadingTextureNode>\n</SimplygonShadingNetwork>");
#endif

static const TCHAR* PATH_TO_7ZIP = TEXT("Binaries/ThirdParty/NotForLicensees/7-Zip/7z.exe");

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
		return GetMutableDefault<UEditorPerProjectUserSettings>()->SwarmIntermediateFolder;
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
		FString SPLSettingsText;


		EBlendMode OutputMaterialBlendMode = BLEND_Opaque;
		bool bHasMaked = false;
		bool bHasOpacity = false;

		for (int MaterialIndex = 0; MaterialIndex < InputMaterials.Num(); MaterialIndex++)
		{
			if (InputMaterials[MaterialIndex].BlendMode == BLEND_Translucent)
			{
				bHasOpacity = true;
			}

			if (InputMaterials[MaterialIndex].BlendMode == BLEND_Masked)
			{
				bHasMaked = true;
			}
		}

		if ( (bHasMaked && bHasOpacity) || bHasOpacity)
		{
			OutputMaterialBlendMode = BLEND_Translucent;
		}
		else if (bHasMaked && !bHasOpacity)
		{
			OutputMaterialBlendMode = BLEND_Masked;
		}

		//scan for clipping geometry 
		bool bHasClippingGeometry = false;
		if (InData.FindByPredicate([](const FMeshMergeData InMeshData) {return InMeshData.bIsClippingMesh == true; }))
			bHasClippingGeometry = true;

#if SPL_CURRENT_VERSION > 7
		SPL::SPL* spl = new SPL::SPL();
		spl->Header.ClientName = TCHAR_TO_ANSI(TEXT("UE4"));
		spl->Header.ClientVersion = TCHAR_TO_ANSI(TEXT("4.11"));
		spl->Header.SimplygonVersion = TCHAR_TO_ANSI(TEXT("8.0"));
		SPL::ProcessNode* splProcessNode = new SPL::ProcessNode();
		spl->ProcessGraph = splProcessNode;
#endif

		 
#if SPL_CURRENT_VERSION > 7
		CreateRemeshingProcess(InProxySettings, *splProcessNode, OutputMaterialBlendMode, bHasClippingGeometry);
#else
		CreateRemeshingSPL(InProxySettings, SPLSettingsText, OutputMaterialBlendMode, bHasClippingGeometry);
#endif 

		//output spl to file. Since REST Interface
		ssf::pssfScene SsfScene;
		
		TArray<FRawMesh*> InputMeshes;

		for (auto Data : InData)
		{
			InputMeshes.Push(Data.RawMesh);
		}

		//converts UE entities to ssf, Textures will be exported to file
		ConvertToSsfScene(InData, InputMaterials, InProxySettings, InputFolderPath, SsfScene);
		
		SsfScene->CoordinateSystem->Value = 1;
		SsfScene->WorldOrientation->Value = 3;
		
		FString SsfOuputPath = FString::Printf(TEXT("%s/input.ssf"), *InputFolderPath);

		//save out ssf file.
		WriteSsfFile(SsfScene, SsfOuputPath);

#if SPL_CURRENT_VERSION > 7
		spl->Save(TCHAR_TO_ANSI(*SPLFileOutputFullPath));
#else
		SaveSPL(SPLSettingsText, SPLFileOutputFullPath);
#endif
		//zip contents and spawn a task 
		if (ZipContentsForUpload(InputFolderPath, ZipFileName))
		{

			//validate if patch exist
			if (!FPaths::FileExists(*FPaths::ConvertRelativePathToFull(ZipFileName)))
			{
				UE_LOG(LogSimplygonSwarm, Error , TEXT("Could not find zip file for uploading %s"), *ZipFileName);
				FailedDelegate.ExecuteIfBound(InJobGUID, TEXT("Could not find zip file for uploading"));
				return;
			}

			//NOTE : Currently SgRESTInterface is not cleaned up and Task ref counted . The FSimplygonSwarmTask & FSimplygonRESTClient pari can be stored in a TArray or TMap to make aync calls
			// The pair can be cleanup after the import process

			FSwarmTaskkData TaskData;
			TaskData.ZipFilePath = ZipFileName;
			TaskData.SplFilePath = SPLFileOutputFullPath;
			TaskData.OutputZipFilePath = OutputZipFileName;
			TaskData.JobDirectory = JobDirectory;
			TaskData.StateLock = &InStateLock;
			TaskData.ProcessorJobID = InJobGUID;
			TaskData.bDitheredTransition = (InputMaterials.Num() > 0) ? InputMaterials[0].bDitheredLODTransition : false;
						 
			SwarmTask = MakeShareable(new FSimplygonSwarmTask(TaskData));
			SwarmTask->OnAssetDownloaded().BindRaw(this, &FSimplygonSwarm::ImportFile);
			SwarmTask->OnAssetUploaded().BindRaw(this, &FSimplygonSwarm::Cleanup);
			FSimplygonRESTClient::Get()->AddSwarmTask(SwarmTask);			
		}
	}


	/*
	The Following method will cleanup temp files and folders created to upload the job to the simplygon job manager
	*/
	void Cleanup(const FSimplygonSwarmTask& InSwarmTask)
	{

		bool bDebuggingEnabled = GetDefault<UEditorPerProjectUserSettings>()->bEnableSwarmDebugging;
		
		if(!bDebuggingEnabled)
		{
			FString InputFolderPath = FPaths::ConvertRelativePathToFull(FString::Printf(TEXT("%s/Input"), *InSwarmTask.TaskData.JobDirectory));
			//remove folder folder
			if (FPaths::DirectoryExists(InputFolderPath))
			{
				if (!IFileManager::Get().DeleteDirectory(*InputFolderPath, true, true))
				{
					UE_LOG(LogSimplygonSwarm, Log, TEXT("Failed to remove simplygon swarm task temp directory %s"), *InputFolderPath);
				}
			}
			FString FullZipPath = FPaths::ConvertRelativePathToFull(*InSwarmTask.TaskData.ZipFilePath);
			//remove uploaded zip file
			if (FPaths::FileExists(FullZipPath))
			{
				if (!IFileManager::Get().Delete(*FullZipPath))
				{
					UE_LOG(LogSimplygonSwarm, Log, TEXT("Failed to remove Simplygon Swarm Task temp file %s"), *InSwarmTask.TaskData.ZipFilePath);
				}
			}
		}
	}


	/*
	This mehtod would be fired when the job has been download on the machine. A good plane to hook contents to the LODActor.
	Note this would need to run on the Main Thread
	*/
	void ImportFile(const FSimplygonSwarmTask& InSwarmTask)
	{
		FRawMesh OutProxyMesh;
		FFlattenMaterial OutMaterial;

		FString OutputFolderPath = FString::Printf(TEXT("%s/Output"), *InSwarmTask.TaskData.JobDirectory);
#if SPL_CURRENT_VERSION > 7
		FString ParentDirForOutputSsf = FString::Printf(TEXT("%s/outputlod_0"), *OutputFolderPath);
#else
		FString ParentDirForOutputSsf = FString::Printf(TEXT("%s/Node/Node/outputlod_0"), *OutputFolderPath);
#endif

		//for import the file back in uncomment
		if (UnzipDownloadedContent(FPaths::ConvertRelativePathToFull(InSwarmTask.TaskData.OutputZipFilePath), FPaths::ConvertRelativePathToFull(OutputFolderPath)))
		{
			//FString InOuputSsfPath = FString::Printf(TEXT("%s/Node/Node/outputlod_0/output.ssf"), *OutputFolderPath);
			FString InOuputSsfPath = FString::Printf(TEXT("%s/output.ssf"), *ParentDirForOutputSsf);
			ssf::pssfScene OutSsfScene = new ssf::ssfScene();

			FString SsfFullPath = FPaths::ConvertRelativePathToFull(InOuputSsfPath);

			if (!FPaths::FileExists(SsfFullPath))
			{
				UE_LOG(LogSimplygonSwarm, Log, TEXT("Ssf file not found %s"), *SsfFullPath);
				FailedDelegate.ExecuteIfBound(InSwarmTask.TaskData.ProcessorJobID, TEXT("Ssf file not found"));
				return;
			}

			ReadSsfFile(SsfFullPath, OutSsfScene);

			ConvertFromSsfScene(OutSsfScene, OutProxyMesh, OutMaterial, ParentDirForOutputSsf);

			OutMaterial.bDitheredLODTransition = InSwarmTask.TaskData.bDitheredTransition;

			if (!OutProxyMesh.IsValid())
			{
				UE_LOG(LogSimplygonSwarm, Log, TEXT("RawMesh is invalid."));
				FailedDelegate.ExecuteIfBound(InSwarmTask.TaskData.ProcessorJobID, TEXT("Invalid FRawMesh data"));
			}

			 
			bool bDebuggingEnabled = GetDefault<UEditorPerProjectUserSettings>()->bEnableSwarmDebugging;
			//do cleanup work
			if (!bDebuggingEnabled)
			{
				FString FullOutputFolderPath = FPaths::ConvertRelativePathToFull(*OutputFolderPath);
				if (!IFileManager::Get().DeleteDirectory(*FullOutputFolderPath, true, true))
				{
					UE_LOG(LogSimplygonSwarm, Log, TEXT("Failed to remove simplygon swarm task temp directory %s"), *FullOutputFolderPath);
				}

				FString FullOutputFileName = FPaths::ConvertRelativePathToFull(*InSwarmTask.TaskData.OutputZipFilePath);
				//remove uploaded zip file
				if (!IFileManager::Get().Delete(*FullOutputFileName, true, true, false))
				{
					UE_LOG(LogSimplygonSwarm, Log, TEXT("Failed to remove Simplygon Swarm Task temp file %s"), *FullOutputFileName);
				}
			}

			//if is bound then execute
			if (CompleteDelegate.IsBound())
			{
				CompleteDelegate.Execute(OutProxyMesh, OutMaterial, InSwarmTask.TaskData.ProcessorJobID);
			}
			 
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

	void SetupSplMappingImage(const struct FMaterialProxySettings& InMaterialProxySettings, SPL::MappingImageSettings& InMappingImageSettings)
	{
		FIntPoint ImageSizes = ComputeMappingImageSize(InMaterialProxySettings);

		InMappingImageSettings.GenerateMappingImage = true;
		InMappingImageSettings.GutterSpace = InMaterialProxySettings.GutterSpace;
		InMappingImageSettings.Height = ImageSizes.X;
		InMappingImageSettings.Width = ImageSizes.Y;
		InMappingImageSettings.UseFullRetexturing = true;
		InMappingImageSettings.GenerateTangents = true;
		InMappingImageSettings.GenerateTexCoords = true;
		InMappingImageSettings.TexCoordLevel = 255;
		InMappingImageSettings.MultisamplingLevel = 3;
		InMappingImageSettings.TexCoordGeneratorType = SPL::TexCoordGeneratorType::SG_TEXCOORDGENERATORTYPE_PARAMETERIZER;
		InMappingImageSettings.Enabled = true;

	}

	void CreateAggregateProcess(const struct FMeshProxySettings& InProxySettings, SPL::ProcessNode& InProcessNodeSpl, EBlendMode InOutputMaterialBlendMode = BLEND_Opaque)
	{
		SPL::AggregationProcessor* processor = new SPL::AggregationProcessor();
		processor->AggregationSettings = new SPL::AggregationSettings();
		processor->AggregationSettings->Enabled = true;

		processor->MappingImageSettings = new SPL::MappingImageSettings();
		SetupSplMaterialCasters(InProxySettings.MaterialSettings, InProcessNodeSpl, InOutputMaterialBlendMode);

		InProcessNodeSpl.Processor = processor;
		InProcessNodeSpl.DefaultTBNType = SPL::SG_TANGENTSPACEMETHOD_ORTHONORMAL_LEFTHANDED;

		SPL::WriteNode* splWriteNode = new SPL::WriteNode();
		splWriteNode->Format = TCHAR_TO_ANSI(SSF_FILE_TYPE);
		splWriteNode->Name = TCHAR_TO_ANSI(OUTPUT_LOD);


		InProcessNodeSpl.Children.push_back(splWriteNode);
		 
	}

	void CreateRemeshingProcess(const struct FMeshProxySettings& InProxySettings, SPL::ProcessNode& InProcessNodeSpl, EBlendMode InOutputMaterialBlendMode = BLEND_Opaque, bool InHasClippingGeometry = false)
	{
		SPL::RemeshingProcessor* processor = new SPL::RemeshingProcessor();
		processor->RemeshingSettings = new SPL::RemeshingSettings();
		 
		processor->RemeshingSettings->OnScreenSize = InProxySettings.ScreenSize;

		if (InProxySettings.bRecalculateNormals)
			processor->RemeshingSettings->HardEdgeAngleInRadians = FMath::DegreesToRadians(InProxySettings.HardAngleThreshold);

		processor->RemeshingSettings->MergeDistance = InProxySettings.MergeDistance;
		processor->RemeshingSettings->Enabled = true;

		processor->RemeshingSettings->UseEmptySpaceOverride = InHasClippingGeometry;

		FIntPoint ImageSizes = ComputeMappingImageSize(InProxySettings.MaterialSettings);

		//mapping image settings
		processor->MappingImageSettings = new SPL::MappingImageSettings();
		SetupSplMappingImage(InProxySettings.MaterialSettings, *processor->MappingImageSettings);
		 
		// Clipping planes removed for now
		/*if (InProxySettings.bUseClippingPlanes)
		{
			
			// Note : An arbitary plane can be define using a point (position) and a normal (direction)
			// Since UE  currently only has axis aligned plane. We need to convert values for SPL
			FVector OutPoint, OutNormal;

			GetAxisAlignedVectorsForCuttingPlanes(InProxySettings, OutPoint, OutNormal);
			processor->CuttingPlaneSettings = new SPL::CuttingPlaneSettings();
			processor->CuttingPlaneSettings->NormalX = OutNormal.X;
			processor->CuttingPlaneSettings->NormalY = OutNormal.Y;
			processor->CuttingPlaneSettings->NormalZ = OutNormal.Z;

			processor->CuttingPlaneSettings->PointX = OutPoint.X;
			processor->CuttingPlaneSettings->PointY = OutPoint.Y;
			processor->CuttingPlaneSettings->PointZ = OutPoint.Z;
			processor->CuttingPlaneSettings->Enabled = true;
		}*/

		SetupSplMaterialCasters(InProxySettings.MaterialSettings, InProcessNodeSpl, InOutputMaterialBlendMode);
		 
		InProcessNodeSpl.Processor = processor;
		InProcessNodeSpl.DefaultTBNType = SPL::SG_TANGENTSPACEMETHOD_ORTHONORMAL_LEFTHANDED;
		 
		//setup caster	 


		SPL::WriteNode* splWriteNode = new SPL::WriteNode();
		splWriteNode->Format = TCHAR_TO_ANSI(SSF_FILE_TYPE);
		splWriteNode->Name = TCHAR_TO_ANSI(OUTPUT_LOD);
		 

		InProcessNodeSpl.Children.push_back(splWriteNode);
	}

	void CreateRemeshingSPL(const struct FMeshProxySettings& InProxySettings,
		FString& OutSplText, EBlendMode InOutputMaterialBlendMode = BLEND_Opaque, bool InHasClippingGeometry = false)
	{

		////Compute max mapping image size
		FIntPoint ImageSizes = ComputeMappingImageSize(InProxySettings.MaterialSettings);

		////setup casters
		FString CasterSPLTempalte = CreateSPLTemplateChannels(InProxySettings.MaterialSettings, InOutputMaterialBlendMode);
		FString MergeDist = FString::Printf(SPL_MERGE_DIST, InProxySettings.MergeDistance);
		FString AdditionalSettings = FString::Printf(TEXT(",%s"), *MergeDist);
		FString RecalNormals = FString::Printf(SPL_NORMAL_RECALC, FMath::DegreesToRadians(InProxySettings.HardAngleThreshold));

		if (InProxySettings.bRecalculateNormals)
			AdditionalSettings = FString::Printf(TEXT("%s,%s"), *AdditionalSettings, *RecalNormals);

		//Note : The Simplygon API now supports multiple clipping planes. SPL has recently added support for this.
		// 
		FString CuttingPlaneSetting;

		// Clipping planes removed for now
		/*
		if (InProxySettings.bUseClippingPlanes)
		{
			// Note : An arbitary plane can be define using a point (position) and a normal (direction)
			// Since UE  currently only has axis aligned plane. We need to convert values for SPL

			FVector OutPoint, OutNormal;

			GetAxisAlignedVectorsForCuttingPlanes(InProxySettings, OutPoint, OutNormal);

			FString CuttingPlane = FString::Printf(CUTTING_PLANE, OutPoint.X, OutPoint.Y, OutPoint.Z, OutNormal.X, OutNormal.Y, OutNormal.Z);
			CuttingPlaneSetting = FString::Printf(CUTTING_PLANE_SETTINGS, *CuttingPlane);


			//OutSplText = FString::Printf(SPL_TEMPLATE_REMESHING, InProxySettings.ScreenSize, InProxySettings.bUseClippingPlane ? TEXT("true") : TEXT("false"), *AdditionalSettings, ImageSizes.X, ImageSizes.Y, *CuttingPlaneSetting, *CasterSPLTempalte);
		}*/
		 
		OutSplText = FString::Printf(SPL_TEMPLATE_REMESHING, InHasClippingGeometry == true ? TEXT("true") : TEXT("false"), TEXT("false"), InProxySettings.ScreenSize, /*InProxySettings.bUseClippingPlanes ? TEXT("true") :*/ TEXT("false"), *AdditionalSettings, (int32)InProxySettings.MaterialSettings.GutterSpace,ImageSizes.X, ImageSizes.Y, TEXT(""), *CuttingPlaneSetting, *CasterSPLTempalte);
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

		//set processing and clipping geometry sets

		//processing set
		ssf::ssfNamedIdList<ssf::ssfString> ProcessingObjectsSet;
		ssf::ssfNamedIdList<ssf::ssfString> ClippingGeometrySet;
		 
		ProcessingObjectsSet.Name = FSimplygonSSFHelper::TCHARToSSFString(TEXT("RemeshingProcessingSet"));
		ProcessingObjectsSet.ID = FSimplygonSSFHelper::SSFNewGuid();
		ClippingGeometrySet.Name = FSimplygonSSFHelper::TCHARToSSFString(TEXT("ClippingObjectSet"));
		ClippingGeometrySet.ID = FSimplygonSSFHelper::SSFNewGuid();
		 
		 
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
			ssf::ssfMatrix4x4 IdenMatrix;
			IdenMatrix.M[0][0] = IdenMatrix.M[1][1] = IdenMatrix.M[2][2] = IdenMatrix.M[3][3] = 1;
			SsfNode->LocalTransform.Set(IdenMatrix);

			//create the mesh object
			ssf::pssfMesh SsfMesh = new ssf::ssfMesh();
			SsfMesh->Id.Set(FSimplygonSSFHelper::SSFNewGuid());
			FString MeshName = FString::Printf(TEXT("Mesh%i"), Count);
			SsfMesh->Name.Set(FSimplygonSSFHelper::TCHARToSSFString(*MeshName));

			Count++;

			//setup mesh data
			ssf::pssfMeshData SsfMeshData = CreateSSFMeshDataFromRawMesh(*MergeData.RawMesh, MergeData.TexCoordBounds, MergeData.NewUVs);
			SsfMesh->MeshDataList.push_back(SsfMeshData);

			//setup mesh material information
			SsfMesh->MaterialIds.Create();
			TArray<int32> UniqueMaterialIds;
			UniqueMaterialIds.Reserve(InputMaterials.Num());

			//get unqiue material ids
			GetUniqueMaterialIndices(MergeData.RawMesh->FaceMaterialIndices, UniqueMaterialIds);

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

			if (MergeData.bIsClippingMesh)
			{
				ClippingGeometrySet.Items.push_back(SsfNode->Id->ToCharString());
			}
			else
			{
				ProcessingObjectsSet.Items.push_back(SsfNode->Id->ToCharString());
			}
		}

		if(ClippingGeometrySet.Items.size() > 0)
			OutSsfScene->SelectionGroupSetsList.push_back(ClippingGeometrySet);
		 
		if (ProcessingObjectsSet.Items.size() > 0)
		OutSsfScene->SelectionGroupSetsList.push_back(ProcessingObjectsSet);
		 
	}

	 
	void ConvertFromSsfScene(ssf::pssfScene SceneGraph, FRawMesh& OutProxyMesh, FFlattenMaterial& OutMaterial, const FString OutputFolderPath)
	{
		bool bReverseWinding = true;
		 
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
					OutProxyMesh.VertexPositions[VertexIndex] = GetConversionMatrixYUP().InverseTransformPosition(FVector(VertexCoord.V[0], VertexCoord.V[1], VertexCoord.V[2]));
					VertexIndex++;
				}
				 
				 

				OutProxyMesh.WedgeIndices.SetNumUninitialized(ToatalCorners);
				for (int32 TriIndex = 0; TriIndex < TotalTriangles; ++TriIndex)
				{
					for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
					{
						int32 DestCornerIndex = bReverseWinding ? 2 - CornerIndex : CornerIndex;
						OutProxyMesh.WedgeIndices[TriIndex * 3 + DestCornerIndex] = MeshData->TriangleIndices.Get().Items[TriIndex].V[CornerIndex];
					}					 
				}

				int32 TexCoordIndex = 0;
				for (ssf::ssfNamedList<ssf::ssfVector2> TexCoorChannel : MeshData->TextureCoordinatesList)
				{
					OutProxyMesh.WedgeTexCoords[TexCoordIndex].SetNumUninitialized(ToatalCorners);
					for (int32 TriIndex = 0; TriIndex < TotalTriangles; ++TriIndex)
					{						 
						for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
						{
							int32 DestCornerIndex =  bReverseWinding ? 2 - CornerIndex : CornerIndex;
							OutProxyMesh.WedgeTexCoords[TexCoordIndex][TriIndex * 3 + DestCornerIndex].X = TexCoorChannel.Items[TriIndex * 3 + CornerIndex].V[0];
							OutProxyMesh.WedgeTexCoords[TexCoordIndex][TriIndex * 3 + DestCornerIndex].Y = TexCoorChannel.Items[TriIndex * 3 + CornerIndex].V[1];

						}

					}
					//TexCoordIndex++;
				}

				//SSF Can store multiple color channels. However UE only supports one color channel
				int32 ColorChannelIndex = 0;
				for (ssf::ssfNamedList<ssf::ssfVector4> TexCoorChannel : MeshData->ColorsList)
				{
					OutProxyMesh.WedgeColors.SetNumUninitialized(ToatalCorners);
					for (int TriIndex = 0; TriIndex < TotalTriangles; ++TriIndex)
					{
						for (int32 CornerIndex = 0; CornerIndex < 2; ++CornerIndex)
						{
							int32 DestCornerIndex = bReverseWinding ? 2 - CornerIndex : CornerIndex;
							OutProxyMesh.WedgeColors[TriIndex * 3 +DestCornerIndex].R = TexCoorChannel.Items[TriIndex * 3 +CornerIndex].V[0];
							OutProxyMesh.WedgeColors[TriIndex * 3 +DestCornerIndex].G = TexCoorChannel.Items[TriIndex * 3 +CornerIndex].V[1];
							OutProxyMesh.WedgeColors[TriIndex * 3 +DestCornerIndex].B = TexCoorChannel.Items[TriIndex * 3 +CornerIndex].V[2];
							OutProxyMesh.WedgeColors[TriIndex * 3 +DestCornerIndex].A = TexCoorChannel.Items[TriIndex * 3 +CornerIndex].V[3];
						}
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
						OutProxyMesh.WedgeTangentY.SetNumUninitialized(ToatalCorners);

						for (int32 TriIndex = 0; TriIndex < TotalTriangles; ++TriIndex)
						{
							for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
							{
								int32 DestCornerIndex = bReverseWinding ? 2 - CornerIndex : CornerIndex;
								OutProxyMesh.WedgeTangentX[TriIndex * 3 + DestCornerIndex].X = MeshData->Tangents.Get().Items[TriIndex * 3 + CornerIndex].V[0];
								OutProxyMesh.WedgeTangentX[TriIndex * 3 + DestCornerIndex].Y = MeshData->Tangents.Get().Items[TriIndex * 3 + CornerIndex].V[1];
								OutProxyMesh.WedgeTangentX[TriIndex * 3 + DestCornerIndex].Z = MeshData->Tangents.Get().Items[TriIndex * 3 + CornerIndex].V[2];
								OutProxyMesh.WedgeTangentX[TriIndex * 3 + DestCornerIndex] = GetConversionMatrixYUP().InverseTransformPosition(OutProxyMesh.WedgeTangentX[TriIndex * 3 + DestCornerIndex]);
							}
						}

						for (int32 TriIndex = 0; TriIndex < TotalTriangles; ++TriIndex)
						{
							for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
							{
								int32 DestCornerIndex = bReverseWinding ? 2 - CornerIndex : CornerIndex;
								OutProxyMesh.WedgeTangentY[TriIndex * 3 + DestCornerIndex].X = MeshData->Bitangents.Get().Items[TriIndex * 3 + CornerIndex].V[0];
								OutProxyMesh.WedgeTangentY[TriIndex * 3 + DestCornerIndex].Y = MeshData->Bitangents.Get().Items[TriIndex * 3 + CornerIndex].V[1];
								OutProxyMesh.WedgeTangentY[TriIndex * 3 + DestCornerIndex].Z = MeshData->Bitangents.Get().Items[TriIndex * 3 + CornerIndex].V[2];
								OutProxyMesh.WedgeTangentY[TriIndex * 3 + DestCornerIndex] = GetConversionMatrixYUP().InverseTransformPosition(OutProxyMesh.WedgeTangentY[TriIndex * 3 + DestCornerIndex]);
							}
						}

						 
						 
					}

					OutProxyMesh.WedgeTangentZ.SetNumUninitialized(ToatalCorners);
					for (int32 TriIndex = 0; TriIndex < TotalTriangles; ++TriIndex)
					{
						for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
						{
							int32 DestCornerIndex = bReverseWinding ? 2 - CornerIndex : CornerIndex;
							OutProxyMesh.WedgeTangentZ[TriIndex * 3 + DestCornerIndex].X = MeshData->Normals.Get().Items[TriIndex * 3 + CornerIndex].V[0];
							OutProxyMesh.WedgeTangentZ[TriIndex * 3 + DestCornerIndex].Y = MeshData->Normals.Get().Items[TriIndex * 3 + CornerIndex].V[1];
							OutProxyMesh.WedgeTangentZ[TriIndex * 3 + DestCornerIndex].Z = MeshData->Normals.Get().Items[TriIndex * 3 + CornerIndex].V[2];
							OutProxyMesh.WedgeTangentZ[TriIndex * 3 + DestCornerIndex] = GetConversionMatrixYUP().InverseTransformPosition(OutProxyMesh.WedgeTangentZ[TriIndex * 3 + DestCornerIndex]);
						}
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
				SetupMaterial(SceneGraph, ProxyMaterial, OutMaterial, OutputFolderPath);
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

	void SetupMaterial(ssf::pssfScene SceneGraph, ssf::pssfMaterial Material, FFlattenMaterial &OutMaterial, FString OutputFolderPath)
	{

		bool  bHasOpacityMask = false;
		bool  bHasOpacity = false;
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
				bHasOpacity = true;
			}
			else if (ChannelName.Compare(OPACITY_MASK_CHANNEL) == 0)
			{
				ExtractTextureDescriptors(SceneGraph, Channel, OutputFolderPath, ChannelName, OutMaterial.OpacitySamples, OutMaterial.OpacitySize);
				bHasOpacityMask = true;
			}
		}

		if ( (bHasOpacity && bHasOpacityMask) || bHasOpacity)
		{
			OutMaterial.BlendMode = BLEND_Translucent;
		}
		else if (bHasOpacityMask)
		{
			OutMaterial.BlendMode = BLEND_Masked;
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
		FString ZipToolPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / PATH_TO_7ZIP);

		bool bEnableDebugging = GetDefault<UEditorPerProjectUserSettings>()->bEnableSwarmDebugging;
	
		if (!FPaths::FileExists(ZipToolPath))
		{
			UE_LOG(LogSimplygonSwarm, Log, TEXT("External Zip Utility not found at location %s"), *ZipToolPath);
			return false;
		}

		FString FullCommandLine = FString::Printf(TEXT("/c \"\"%s\" %s\""), *ZipToolPath, *CommandLine);
		TSharedPtr<FMonitoredProcess> UatProcess = MakeShareable(new FMonitoredProcess(CmdExe, FullCommandLine, true));
		UatProcess->OnOutput().BindLambda([&](FString Message) {

			if(bEnableDebugging)
			UE_LOG(LogSimplygonSwarm, Display, TEXT("Simplygon Swarm UnzipFiles Output %s"), *Message); 
		});

			 
		UatProcess->Launch();

		//ugly spin lock
		while (UatProcess->IsRunning() && UatProcess->GetDuration().GetSeconds() < 300)
		{
			FPlatformProcess::Sleep(0.1f);
		}
		

		return true;
	}

bool ZipContentsForUpload(FString InputDirectoryPath, FString OutputFileName)
{
		 
	FString CommandLine = FString::Printf(TEXT("a %s %s/* -mx0"), *FPaths::ConvertRelativePathToFull(OutputFileName), *FPaths::ConvertRelativePathToFull(InputDirectoryPath));

	FString CmdExe = TEXT("cmd.exe");
	FString ZipToolPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / PATH_TO_7ZIP);

	const bool bEnableDebugging = GetDefault<UEditorPerProjectUserSettings>()->bEnableSwarmDebugging;		 
	if (!FPaths::FileExists(ZipToolPath))
	{
		//FFormatNamedArguments Arguments;
		//Arguments.Add(TEXT("File"), FText::FromString(ZipToolPath));
		//FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("RequiredFileNotFoundMessage", "A required file could not be found:\n{File}"), Arguments));
		UE_LOG(LogSimplygonSwarm, Error, TEXT("External Zip Tool not found at location %s"), *ZipToolPath)
		return false;
	}

	FString FullCommandLine = FString::Printf(TEXT("/c \"\"%s\" %s\""), *ZipToolPath, *CommandLine);
	TSharedPtr<FMonitoredProcess> UatProcess = MakeShareable(new FMonitoredProcess(CmdExe, FullCommandLine, true));
			 
	UatProcess->OnOutput().BindLambda([&](FString Message) 
	{
		if(bEnableDebugging)
			UE_LOG(LogSimplygonSwarm, Display, TEXT("Simplygon Swarm ZipFile Process OuputMessage %s"), *Message); 
	});

	UatProcess->Launch();
			 
	//ugly spin lock
	while (UatProcess->IsRunning())
	{
		FPlatformProcess::Sleep(0.1f);
	}
		
		 

	return true;
}


	/*
	Takes in a UAT Command and executes it. Is based on MainFrameAction CreateUatTask ( super striped down version).
	No need to redist 7-zip and would in the future work out for XPlatorm implementation.
	TODO: Optionally add notification.
	*/

	bool UatTask(FString CommandLine)
	{
#if PLATFORM_WINDOWS
		FString RunUATScriptName = TEXT("RunUAT.bat");
		FString CmdExe = TEXT("cmd.exe");
#elif PLATFORM_LINUX
		FString RunUATScriptName = TEXT("RunUAT.sh");
		FString CmdExe = TEXT("/bin/bash");
#else
		FString RunUATScriptName = TEXT("RunUAT.command");
		FString CmdExe = TEXT("/bin/sh");
#endif

		FString UatPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Build/BatchFiles") / RunUATScriptName);

		if (!FPaths::FileExists(UatPath))
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("File"), FText::FromString(UatPath));
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("RequiredFileNotFoundMessage", "A required file could not be found:\n{File}"), Arguments));

			return false;
		}

#if PLATFORM_WINDOWS
		FString FullCommandLine = FString::Printf(TEXT("/c \"\"%s\" %s\""), *UatPath, *CommandLine);
#else
		FString FullCommandLine = FString::Printf(TEXT("\"%s\" %s"), *UatPath, *CommandLine);
#endif

		TSharedPtr<FMonitoredProcess> UatProcess = MakeShareable(new FMonitoredProcess(CmdExe, FullCommandLine, true));

		// create notification item

		bool sucess = UatProcess->Launch();

		UatProcess->OnOutput().BindLambda([&](FString Message) {UE_LOG(LogSimplygonSwarm, Log, TEXT("Simplygon Swarm Uat Task Output %s"), *Message); });

		while (UatProcess->IsRunning())
		{
			FPlatformProcess::Sleep(0.1f);
		}

		return sucess;

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


	void SetupColorCaster(SPL::ProcessNode& InSplProcessNode, FString Channel)
	{
		SPL::ColorCaster* colorCaster = new SPL::ColorCaster();
		colorCaster->Dilation = 10;
		colorCaster->OutputChannels = 4;
		colorCaster->OutputSRGB = false;
		colorCaster->FillMode = SPL::FillMode::SG_ATLASFILLMODE_INTERPOLATE;
		colorCaster->ColorType = TCHAR_TO_ANSI(*Channel);
		colorCaster->Name = TCHAR_TO_ANSI(*Channel);
		colorCaster->Channel = TCHAR_TO_ANSI(*Channel);
		colorCaster->DitherType = SPL::DitherType::SG_DITHERPATTERNS_FLOYDSTEINBERG;
		//for spl we need to expliclity set the enabled flag.
		colorCaster->Enabled = true;

		InSplProcessNode.MaterialCaster.push_back(colorCaster);
	}

	void SetupNormalCaster(SPL::ProcessNode& InSplProcessNode, FString Channel)
	{
		SPL::NormalCaster* normalCaster = new SPL::NormalCaster();
		normalCaster->Name = TCHAR_TO_ANSI(*Channel);
		normalCaster->Channel = TCHAR_TO_ANSI(*Channel);
		normalCaster->GenerateTangentSpaceNormals = true;
		normalCaster->OutputChannels = 3;
		normalCaster->Dilation = 10;
		normalCaster->FlipGreen = false;
		normalCaster->FillMode = SPL::FillMode::SG_ATLASFILLMODE_NEARESTNEIGHBOR;
		normalCaster->DitherType = SPL::DitherType::SG_DITHERPATTERNS_NO_DITHER;
		normalCaster->Enabled = true;

		InSplProcessNode.MaterialCaster.push_back(normalCaster);
	}

	void SetupOpacityCaster(SPL::ProcessNode& InSplProcessNode, FString Channel)
	{
		SPL::OpacityCaster* opacityCaster = new SPL::OpacityCaster();
		opacityCaster->Dilation = 10;
		opacityCaster->OutputChannels = 4;
		opacityCaster->FillMode = SPL::FillMode::SG_ATLASFILLMODE_INTERPOLATE;
		opacityCaster->ColorType = TCHAR_TO_ANSI(*Channel);
		opacityCaster->Name = TCHAR_TO_ANSI(*Channel);
		opacityCaster->Channel = TCHAR_TO_ANSI(*Channel);
		opacityCaster->DitherType = SPL::DitherType::SG_DITHERPATTERNS_FLOYDSTEINBERG;

		//for spl we need to expliclity set the enabled flag.
		opacityCaster->Enabled = true;

		InSplProcessNode.MaterialCaster.push_back(opacityCaster);
	}


	void SetupSplMaterialCasters(const FMaterialProxySettings& InMaterialProxySettings, SPL::ProcessNode& InSplProcessNode, EBlendMode InOutputMaterialBlendMode = BLEND_Opaque)
	{
		SetupColorCaster(InSplProcessNode, BASECOLOR_CHANNEL);

		if (InMaterialProxySettings.bRoughnessMap)
		{
			SetupColorCaster(InSplProcessNode, ROUGHNESS_CHANNEL);
		}
		if (InMaterialProxySettings.bSpecularMap)
		{
			SetupColorCaster(InSplProcessNode, SPECULAR_CHANNEL);
		}
		if (InMaterialProxySettings.bMetallicMap)
		{
			SetupColorCaster(InSplProcessNode, METALLIC_CHANNEL);
		}

		if (InMaterialProxySettings.bNormalMap)
		{
			SetupNormalCaster(InSplProcessNode, NORMAL_CHANNEL);
		}

		if (InMaterialProxySettings.bOpacityMap)
		{
			SetupOpacityCaster(InSplProcessNode, InOutputMaterialBlendMode == BLEND_Translucent ? OPACITY_CHANNEL : OPACITY_MASK_CHANNEL);
		}
	}

	FString CreateSPLTemplateChannels(const FMaterialProxySettings& Settings,EBlendMode InOutputMaterialBlendMode = BLEND_Opaque)
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

	if (Settings.bOpacityMap)
	{
		const TCHAR* ChannelTypeName = InOutputMaterialBlendMode == BLEND_Translucent ? OPACITY_CHANNEL : OPACITY_MASK_CHANNEL;
		FinalTemplate += FString::Printf(TEXT(",%s"), *FString::Printf(SPL_TEMPLATE_OPACITYCASTER, ChannelTypeName, ChannelTypeName, ChannelTypeName));
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

	 
	/*
	*	(1,0,0)
	*	(0,0,1)
	*	(0,1,0)
	*/
	//const FMatrix& GetConversionMatrix()
	//{
	// 
	// static FMatrix m;
	// static bool bInitialized = false;
	// if (!bInitialized)
	// {
	//	 m.SetIdentity();
	//	 m.SetAxis(0, FVector(1, 0, 0));	//forward
	//	 m.SetAxis(1, FVector(0, 0, 1));	//right
	//	 m.SetAxis(2, FVector(0, 1, 0));	//up
	//	 bInitialized = true;

	// }
	// 
	// return m;		 
	//}

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
			//m.SetAxis(0, FVector(1, 0, 0));	//forward
			//m.SetAxis(1, FVector(0, 0, 1));	//right
			//m.SetAxis(2, FVector(0, 1, 0));	//up
			 
			 
			bInitialized = true;
		}
		//FMatrix InvM = m.Inverse();
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


		//Reverse winding switches
		bool bReverseWinding = true;

		for (int32 TriIndex = 0; TriIndex < NumTris; ++TriIndex)
		{
			for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
			{
				int32 DestCornerIndex = bReverseWinding ? 2 - CornerIndex : CornerIndex;
				SgTriangleIds.Items[TriIndex].V[DestCornerIndex] = RawMesh.WedgeIndices[TriIndex * 3 + CornerIndex];
			}
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

					for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
					{
						const FVector2D& TexCoord = SrcTexCoords[TriIndex * 3 + CornerIndex];
						ssf::ssfVector2 temp;
						temp.V[0] = (TexCoord.X - MinU) * ScaleU;
						temp.V[1] = (TexCoord.Y - MinV) * ScaleV;
						int32 DestCornerIndex = bReverseWinding ? 2 - CornerIndex : CornerIndex;
						SgTexCoord.Items[TriIndex * 3 + DestCornerIndex] = temp;
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
			for (int32 TriIndex = 0; TriIndex < NumTris; ++TriIndex)
			{
				for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
				{
					int32 DestCornerIndex = bReverseWinding ? 2 - CornerIndex : CornerIndex;
					FLinearColor LinearColor(RawMesh.WedgeColors[TriIndex*3+CornerIndex]);
					SgColors.Items[TriIndex*3+DestCornerIndex].V[0] = LinearColor.R;
					SgColors.Items[TriIndex*3+DestCornerIndex].V[1] = LinearColor.G;
					SgColors.Items[TriIndex*3+DestCornerIndex].V[2] = LinearColor.B;
					SgColors.Items[TriIndex*3+DestCornerIndex].V[3] = LinearColor.A;
				}
				 
			}
			SgMeshData->ColorsList.push_back(SgColors);
		}
		 
		 
		if (RawMesh.WedgeTangentZ.Num() == NumWedges)
		{
			if (RawMesh.WedgeTangentX.Num() == NumWedges && RawMesh.WedgeTangentY.Num() == NumWedges)
			{
				ssf::ssfList<ssf::ssfVector3> & SgTangents = SgMeshData->Tangents.Create();
				SgTangents.Items.resize(NumWedges);

				for (int32 TriIndex = 0; TriIndex < NumTris; ++TriIndex)
				{
					for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
					{
						int32 DestCornerIndex = bReverseWinding ? 2 - CornerIndex : CornerIndex;
						ssf::ssfVector3 SsfTangent;
						FVector4 Tangent = GetConversionMatrixYUP().TransformPosition(RawMesh.WedgeTangentX[TriIndex * 3 + CornerIndex]);
						SsfTangent.V[0] = double(Tangent.X);
						SsfTangent.V[1] = double(Tangent.Y);
						SsfTangent.V[2] = double(Tangent.Z);
						SgTangents.Items[TriIndex * 3 + DestCornerIndex] = SsfTangent;
					}

				}
				 

				ssf::ssfList<ssf::ssfVector3> & SgBitangents = SgMeshData->Bitangents.Create();
				SgBitangents.Items.resize(NumWedges);
				for (int32 TriIndex = 0; TriIndex < NumTris; ++TriIndex)
				{
					for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
					{
						int32 DestCornerIndex = bReverseWinding ? 2 - CornerIndex : CornerIndex;
						ssf::ssfVector3 SsfBitangent;
						FVector4 Bitangent = GetConversionMatrixYUP().TransformPosition(RawMesh.WedgeTangentY[TriIndex * 3 + CornerIndex]);
						SsfBitangent.V[0] = double(Bitangent.X);
						SsfBitangent.V[1] = double(Bitangent.Y);
						SsfBitangent.V[2] = double(Bitangent.Z);
						SgBitangents.Items[TriIndex * 3 + DestCornerIndex] = SsfBitangent;
					}
				}
				 
			}

			ssf::ssfList<ssf::ssfVector3> & SgNormals = SgMeshData->Normals.Create();
			SgNormals.Items.resize(NumWedges);

			for (int32 TriIndex = 0; TriIndex < NumTris; ++TriIndex)
			{
				for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
				{
					int32 DestCornerIndex = bReverseWinding ? 2 - CornerIndex : CornerIndex;
					ssf::ssfVector3 SsfNormal;
					FVector4 Normal = GetConversionMatrixYUP().TransformPosition(RawMesh.WedgeTangentZ[TriIndex * 3 + CornerIndex]);
					SsfNormal.V[0] = double(Normal.X);
					SsfNormal.V[1] = double(Normal.Y);
					SsfNormal.V[2] = double(Normal.Z);
					SgNormals.Items[TriIndex * 3 + DestCornerIndex] = SsfNormal;
				}
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
					FString ShadingNetwork = FString::Printf(SHADING_NETWORK_TEMPLATE, *TextureName, TEXT("TexCoord0"), IsSRGB);
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
	FSimplygonRESTClient::Shutdown();
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
