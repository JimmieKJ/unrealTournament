// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Factories.h"
#include "Engine.h"
#include "TextureLayout.h"
#include "FbxImporter.h"
#include "ObjectTools.h"
#include "PackageTools.h"
#include "AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "ComponentReregisterContext.h"

DEFINE_LOG_CATEGORY_STATIC(LogFbxMaterialImport, Log, All);

using namespace UnFbx;

UTexture* UnFbx::FFbxImporter::ImportTexture( FbxFileTexture* FbxTexture, bool bSetupAsNormalMap )
{
	if( !FbxTexture )
	{
		return nullptr;
	}
	
	// create an unreal texture asset
	UTexture* UnrealTexture = NULL;
	FString Filename1 = UTF8_TO_TCHAR(FbxTexture->GetFileName());
	FString Extension = FPaths::GetExtension(Filename1).ToLower();
	// name the texture with file name
	FString TextureName = FPaths::GetBaseFilename(Filename1);

	TextureName = ObjectTools::SanitizeObjectName(TextureName);

	// set where to place the textures
	FString BasePackageName = FPackageName::GetLongPackagePath(Parent->GetOutermost()->GetName()) / TextureName;
	BasePackageName = PackageTools::SanitizePackageName(BasePackageName);

	UTexture* ExistingTexture = NULL;
	UPackage* TexturePackage = NULL;
	// First check if the asset already exists.
	{
		FString ObjectPath = BasePackageName + TEXT(".") + TextureName;
		ExistingTexture = LoadObject<UTexture>(NULL, *ObjectPath, nullptr, LOAD_Quiet | LOAD_NoWarn);
	}


	if( !ExistingTexture )
	{
		const FString Suffix(TEXT(""));

		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
		FString FinalPackageName;
		AssetToolsModule.Get().CreateUniqueAssetName(BasePackageName, Suffix, FinalPackageName, TextureName);

		TexturePackage = CreatePackage(NULL, *FinalPackageName);
	}
	else
	{
		TexturePackage = ExistingTexture->GetOutermost();
	}


	// try opening from absolute path
	FString Filename = Filename1;
	TArray<uint8> DataBinary;
	if ( ! FFileHelper::LoadFileToArray( DataBinary, *Filename ))
	{
		// try fbx file base path + relative path
		FString Filename2 = FileBasePath / UTF8_TO_TCHAR(FbxTexture->GetRelativeFileName());
		Filename = Filename2;
		if ( ! FFileHelper::LoadFileToArray( DataBinary, *Filename ))
		{
			// try fbx file base path + texture file name (no path)
			FString Filename3 = UTF8_TO_TCHAR(FbxTexture->GetRelativeFileName());
			FString FileOnly = FPaths::GetCleanFilename(Filename3);
			Filename3 = FileBasePath / FileOnly;
			Filename = Filename3;
			if ( ! FFileHelper::LoadFileToArray( DataBinary, *Filename ))
			{
				UE_LOG(LogFbxMaterialImport, Warning,TEXT("Unable to find TEXTure file %s. Tried:\n - %s\n - %s\n - %s"),*FileOnly,*Filename1,*Filename2,*Filename3);
			}
		}
	}
	if (DataBinary.Num()>0)
	{
		UE_LOG(LogFbxMaterialImport, Verbose, TEXT("Loading texture file %s"),*Filename);
		const uint8* PtrTexture = DataBinary.GetData();
		auto TextureFact = NewObject<UTextureFactory>();
		TextureFact->AddToRoot();

		// save texture settings if texture exist
		TextureFact->SuppressImportOverwriteDialog();
		const TCHAR* TextureType = *Extension;

		// Unless the normal map setting is used during import, 
		//	the user has to manually hit "reimport" then "recompress now" button
		if ( bSetupAsNormalMap )
		{
			if (!ExistingTexture)
			{
				TextureFact->LODGroup = TEXTUREGROUP_WorldNormalMap;
				TextureFact->CompressionSettings = TC_Normalmap;
				TextureFact->bFlipNormalMapGreenChannel = ImportOptions->bInvertNormalMap;
			}
			else
			{
				UE_LOG(LogFbxMaterialImport, Warning, TEXT("Manual texture reimport and recompression may be needed for %s"), *TextureName);
			}
		}

		UnrealTexture = (UTexture*)TextureFact->FactoryCreateBinary(
			UTexture2D::StaticClass(), TexturePackage, *TextureName, 
			RF_Standalone|RF_Public, NULL, TextureType, 
			PtrTexture, PtrTexture+DataBinary.Num(), GWarn );

		if ( UnrealTexture != NULL )
		{
			//Make sure the AssetImportData point on the texture file and not on the fbx files since the factory point on the fbx file
			UnrealTexture->AssetImportData->Update(IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*Filename));

			// Notify the asset registry
			FAssetRegistryModule::AssetCreated(UnrealTexture);

			// Set the dirty flag so this package will get saved later
			TexturePackage->SetDirtyFlag(true);
		}
		TextureFact->RemoveFromRoot();
	}

	return UnrealTexture;
}

void UnFbx::FFbxImporter::ImportTexturesFromNode(FbxNode* Node)
{
	FbxProperty Property;
	int32 NbMat = Node->GetMaterialCount();

	// visit all materials
	int32 MaterialIndex;
	for (MaterialIndex = 0; MaterialIndex < NbMat; MaterialIndex++)
	{
		FbxSurfaceMaterial *Material = Node->GetMaterial(MaterialIndex);

		//go through all the possible textures
		if(Material)
		{
			int32 TextureIndex;
			FBXSDK_FOR_EACH_TEXTURE(TextureIndex)
			{
				Property = Material->FindProperty(FbxLayerElement::sTextureChannelNames[TextureIndex]);

				if( Property.IsValid() )
				{
					FbxTexture * lTexture= NULL;

					//Here we have to check if it's layered textures, or just textures:
					int32 LayeredTextureCount = Property.GetSrcObjectCount<FbxLayeredTexture>();
					FbxString PropertyName = Property.GetName();
					if(LayeredTextureCount > 0)
					{
						for(int32 LayerIndex=0; LayerIndex<LayeredTextureCount; ++LayerIndex)
						{
							FbxLayeredTexture *lLayeredTexture = Property.GetSrcObject<FbxLayeredTexture>(LayerIndex);
							int32 NbTextures = lLayeredTexture->GetSrcObjectCount<FbxTexture>();
							for(int32 TexIndex =0; TexIndex<NbTextures; ++TexIndex)
							{
								FbxFileTexture* Texture = lLayeredTexture->GetSrcObject<FbxFileTexture>(TexIndex);
								if(Texture)
								{
									ImportTexture(Texture, PropertyName == FbxSurfaceMaterial::sNormalMap || PropertyName == FbxSurfaceMaterial::sBump);
								}
							}
						}
					}
					else
					{
						//no layered texture simply get on the property
						int32 NbTextures = Property.GetSrcObjectCount<FbxTexture>();
						for(int32 TexIndex =0; TexIndex<NbTextures; ++TexIndex)
						{

							FbxFileTexture* Texture = Property.GetSrcObject<FbxFileTexture>(TexIndex);
							if(Texture)
							{
								ImportTexture(Texture, PropertyName == FbxSurfaceMaterial::sNormalMap || PropertyName == FbxSurfaceMaterial::sBump);
							}
						}
					}
				}
			}

		}//end if(Material)

	}// end for MaterialIndex
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------

//Enable debug log of fbx material properties, this will log all material properties that are in the FBX file
#define DEBUG_LOG_FBX_MATERIAL_PROPERTIES 0
#if DEBUG_LOG_FBX_MATERIAL_PROPERTIES
void LogPropertyAndChild(FbxSurfaceMaterial& FbxMaterial, const FbxProperty &Property)
{
	FbxString PropertyName = Property.GetHierarchicalName();
	UE_LOG(LogFbxMaterialImport, Display, TEXT("Property Name [%s]"), UTF8_TO_TCHAR(PropertyName.Buffer()));
	int32 TextureCount = Property.GetSrcObjectCount<FbxTexture>();
	if (TextureCount > 0)
	{
		for (int32 TextureIndex = 0; TextureIndex < TextureCount; ++TextureIndex)
		{
			FbxFileTexture* TextureObj = Property.GetSrcObject<FbxFileTexture>(TextureIndex);
			if (TextureObj != nullptr)
			{
				UE_LOG(LogFbxMaterialImport, Display, TEXT("Texture Path [%s]"), UTF8_TO_TCHAR(TextureObj->GetFileName()));
			}
		}
	}
	const FbxProperty &NextProperty = FbxMaterial.GetNextProperty(Property);
	if (NextProperty.IsValid())
	{
		LogPropertyAndChild(FbxMaterial, NextProperty);
	}
}
#endif

bool UnFbx::FFbxImporter::CreateAndLinkExpressionForMaterialProperty(
							FbxSurfaceMaterial& FbxMaterial,
							UMaterial* UnrealMaterial,
							const char* MaterialProperty ,
							FExpressionInput& MaterialInput, 
							bool bSetupAsNormalMap,
							TArray<FString>& UVSet,
							const FVector2D& Location)
{
	bool bCreated = false;
	FbxProperty FbxProperty = FbxMaterial.FindProperty( MaterialProperty );
	if( FbxProperty.IsValid() )
	{
		int32 UnsupportedTextureCount = FbxProperty.GetSrcObjectCount<FbxLayeredTexture>();
		UnsupportedTextureCount += FbxProperty.GetSrcObjectCount<FbxProceduralTexture>();
		if (UnsupportedTextureCount>0)
		{
			UE_LOG(LogFbxMaterialImport, Warning,TEXT("Layered or procedural Textures are not supported (material %s)"),UTF8_TO_TCHAR(FbxMaterial.GetName()));
		}
		else
		{
			int32 TextureCount = FbxProperty.GetSrcObjectCount<FbxTexture>();
			if (TextureCount>0)
			{
				for(int32 TextureIndex =0; TextureIndex<TextureCount; ++TextureIndex)
				{
					FbxFileTexture* FbxTexture = FbxProperty.GetSrcObject<FbxFileTexture>(TextureIndex);

					// create an unreal texture asset
					UTexture* UnrealTexture = ImportTexture(FbxTexture, bSetupAsNormalMap);
				
					if (UnrealTexture)
					{
						float ScaleU = FbxTexture->GetScaleU();
						float ScaleV = FbxTexture->GetScaleV();

						// and link it to the material 
						UMaterialExpressionTextureSample* UnrealTextureExpression = NewObject<UMaterialExpressionTextureSample>(UnrealMaterial);
						UnrealMaterial->Expressions.Add( UnrealTextureExpression );
						MaterialInput.Expression = UnrealTextureExpression;
						UnrealTextureExpression->Texture = UnrealTexture;
						UnrealTextureExpression->SamplerType = bSetupAsNormalMap ? SAMPLERTYPE_Normal : SAMPLERTYPE_Color;
						UnrealTextureExpression->MaterialExpressionEditorX = FMath::TruncToInt(Location.X);
						UnrealTextureExpression->MaterialExpressionEditorY = FMath::TruncToInt(Location.Y);

						// add/find UVSet and set it to the texture
						FbxString UVSetName = FbxTexture->UVSet.Get();
						FString LocalUVSetName = UTF8_TO_TCHAR(UVSetName.Buffer());
						if (LocalUVSetName.IsEmpty())
						{
							LocalUVSetName = TEXT("UVmap_0");
						}
						int32 SetIndex = UVSet.Find(LocalUVSetName);
						if( (SetIndex != 0 && SetIndex != INDEX_NONE) || ScaleU != 1.0f || ScaleV != 1.0f )
						{
							// Create a texture coord node for the texture sample
							UMaterialExpressionTextureCoordinate* MyCoordExpression = NewObject<UMaterialExpressionTextureCoordinate>(UnrealMaterial);
							UnrealMaterial->Expressions.Add(MyCoordExpression);
							MyCoordExpression->CoordinateIndex = (SetIndex >= 0) ? SetIndex : 0;
							MyCoordExpression->UTiling = ScaleU;
							MyCoordExpression->VTiling = ScaleV;
							UnrealTextureExpression->Coordinates.Expression = MyCoordExpression;
							MyCoordExpression->MaterialExpressionEditorX = FMath::TruncToInt(Location.X-175);
							MyCoordExpression->MaterialExpressionEditorY = FMath::TruncToInt(Location.Y);

						}

						bCreated = true;
					}		
				}
			}

			if (MaterialInput.Expression)
			{
				TArray<FExpressionOutput> Outputs = MaterialInput.Expression->GetOutputs();
				FExpressionOutput* Output = Outputs.GetData();
				MaterialInput.Mask = Output->Mask;
				MaterialInput.MaskR = Output->MaskR;
				MaterialInput.MaskG = Output->MaskG;
				MaterialInput.MaskB = Output->MaskB;
				MaterialInput.MaskA = Output->MaskA;
			}
		}
	}

	return bCreated;
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
void UnFbx::FFbxImporter::FixupMaterial( FbxSurfaceMaterial& FbxMaterial, UMaterial* UnrealMaterial )
{
	// add a basic diffuse color if no texture is linked to diffuse
	if (UnrealMaterial->BaseColor.Expression == NULL)
	{
		FbxDouble3 DiffuseColor;
		
		UMaterialExpressionVectorParameter* MyColorExpression = NewObject<UMaterialExpressionVectorParameter>(UnrealMaterial);
		UnrealMaterial->Expressions.Add( MyColorExpression );
		UnrealMaterial->BaseColor.Expression = MyColorExpression;

		bool bFoundDiffuseColor = true;
		if( FbxMaterial.GetClassId().Is(FbxSurfacePhong::ClassId) )
		{
			DiffuseColor = ((FbxSurfacePhong&)(FbxMaterial)).Diffuse.Get();
		}
		else if( FbxMaterial.GetClassId().Is(FbxSurfaceLambert::ClassId) )
		{
			DiffuseColor = ((FbxSurfaceLambert&)(FbxMaterial)).Diffuse.Get();
		}
		else
		{
			bFoundDiffuseColor = false;
		}

		if( bFoundDiffuseColor )
		{
			MyColorExpression->DefaultValue.R = (float)(DiffuseColor[0]);
			MyColorExpression->DefaultValue.G = (float)(DiffuseColor[1]);
			MyColorExpression->DefaultValue.B = (float)(DiffuseColor[2]);
		}
		else
		{
			// use random color because there may be multiple materials, then they can be different 
			MyColorExpression->DefaultValue.R = 0.5f+(0.5f*FMath::Rand())/RAND_MAX;
			MyColorExpression->DefaultValue.G = 0.5f+(0.5f*FMath::Rand())/RAND_MAX;
			MyColorExpression->DefaultValue.B = 0.5f+(0.5f*FMath::Rand())/RAND_MAX;
		}

		TArray<FExpressionOutput> Outputs = UnrealMaterial->BaseColor.Expression->GetOutputs();
		FExpressionOutput* Output = Outputs.GetData();
		UnrealMaterial->BaseColor.Mask = Output->Mask;
		UnrealMaterial->BaseColor.MaskR = Output->MaskR;
		UnrealMaterial->BaseColor.MaskG = Output->MaskG;
		UnrealMaterial->BaseColor.MaskB = Output->MaskB;
		UnrealMaterial->BaseColor.MaskA = Output->MaskA;
	}
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------

FString UnFbx::FFbxImporter::GetMaterialFullName(FbxSurfaceMaterial& FbxMaterial)
{
	FString MaterialFullName = UTF8_TO_TCHAR(MakeName(FbxMaterial.GetName()));

	if (MaterialFullName.Len() > 6)
	{
		int32 Offset = MaterialFullName.Find(TEXT("_SKIN"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		if (Offset != INDEX_NONE)
		{
			// Chop off the material name so we are left with the number in _SKINXX
			FString SkinXXNumber = MaterialFullName.Right(MaterialFullName.Len() - (Offset + 1)).RightChop(4);

			if (SkinXXNumber.IsNumeric())
			{
				// remove the '_skinXX' suffix from the material name					
				MaterialFullName = MaterialFullName.LeftChop(Offset + 1);
			}
		}
	}

	MaterialFullName = ObjectTools::SanitizeObjectName(MaterialFullName);

	return MaterialFullName;
}

void UnFbx::FFbxImporter::CreateUnrealMaterial(FbxSurfaceMaterial& FbxMaterial, TArray<UMaterialInterface*>& OutMaterials, TArray<FString>& UVSets)
{
	// Make sure we have a parent
	if ( !ensure(Parent.IsValid()) )
	{
		return;
	}
	if (ImportOptions->OverrideMaterials.Contains(FbxMaterial.GetUniqueID()))
	{
		UMaterialInterface* FoundMaterial = *(ImportOptions->OverrideMaterials.Find(FbxMaterial.GetUniqueID()));
		if (ImportedMaterialData.IsUnique(FbxMaterial, FName(*FoundMaterial->GetPathName())) == false)
		{
			ImportedMaterialData.AddImportedMaterial(FbxMaterial, *FoundMaterial);
		}
		// The material is override add the existing one
		OutMaterials.Add(FoundMaterial);
		return;
	}
	FString MaterialFullName = GetMaterialFullName(FbxMaterial);
	FString BasePackageName = FPackageName::GetLongPackagePath(Parent->GetOutermost()->GetName());
	if (ImportOptions->MaterialBasePath != NAME_None)
	{
		BasePackageName = ImportOptions->MaterialBasePath.ToString();
	}
	else
	{
		BasePackageName += TEXT("/");
	}
	BasePackageName += MaterialFullName;
	
	BasePackageName = PackageTools::SanitizePackageName(BasePackageName);

	// The material could already exist in the project
	FName ObjectPath = *(BasePackageName + TEXT(".") + MaterialFullName);

	if( ImportedMaterialData.IsUnique( FbxMaterial, ObjectPath ) )
	{
		UMaterialInterface* FoundMaterial = ImportedMaterialData.GetUnrealMaterial( FbxMaterial );
		if (FoundMaterial)
		{
			// The material was imported from this FBX.  Reuse it
			OutMaterials.Add(FoundMaterial);
			return;
		}
	}
	else
	{
		UMaterialInterface* FoundMaterial = LoadObject<UMaterialInterface>(NULL, *ObjectPath.ToString(), NULL, LOAD_Quiet | LOAD_NoWarn);
		// do not override existing materials
		if (FoundMaterial)
		{
			ImportedMaterialData.AddImportedMaterial( FbxMaterial, *FoundMaterial );
			OutMaterials.Add(FoundMaterial);
			return;
		}
	}
	
	const FString Suffix(TEXT(""));
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	FString FinalPackageName;
	AssetToolsModule.Get().CreateUniqueAssetName(BasePackageName, Suffix, FinalPackageName, MaterialFullName);

	UPackage* Package = CreatePackage(NULL, *FinalPackageName);
	

	// create an unreal material asset
	auto MaterialFactory = NewObject<UMaterialFactoryNew>();
	
	UMaterial* UnrealMaterial = (UMaterial*)MaterialFactory->FactoryCreateNew(
		UMaterial::StaticClass(), Package, *MaterialFullName, RF_Standalone|RF_Public, NULL, GWarn );

	if ( UnrealMaterial != NULL )
	{
		// Notify the asset registry
		FAssetRegistryModule::AssetCreated(UnrealMaterial);

		// Set the dirty flag so this package will get saved later
		Package->SetDirtyFlag(true);
	}

	// textures and properties
#if DEBUG_LOG_FBX_MATERIAL_PROPERTIES
	const FbxProperty &FirstProperty = FbxMaterial.GetFirstProperty();
	if (FirstProperty.IsValid())
	{
		UE_LOG(LogFbxMaterialImport, Display, TEXT("Creating Material [%s]"), UTF8_TO_TCHAR(FbxMaterial.GetName()));
		LogPropertyAndChild(FbxMaterial, FirstProperty);
		UE_LOG(LogFbxMaterialImport, Display, TEXT("-------------------------------"));
	}
#endif
	CreateAndLinkExpressionForMaterialProperty(FbxMaterial, UnrealMaterial, FbxSurfaceMaterial::sDiffuse, UnrealMaterial->BaseColor, false, UVSets, FVector2D(240, -320));
	CreateAndLinkExpressionForMaterialProperty( FbxMaterial, UnrealMaterial, FbxSurfaceMaterial::sEmissive, UnrealMaterial->EmissiveColor, false, UVSets, FVector2D(240,-64) );
	CreateAndLinkExpressionForMaterialProperty( FbxMaterial, UnrealMaterial, FbxSurfaceMaterial::sSpecular, UnrealMaterial->Specular, false, UVSets, FVector2D(240, -128) );
	if (!CreateAndLinkExpressionForMaterialProperty( FbxMaterial, UnrealMaterial, FbxSurfaceMaterial::sNormalMap, UnrealMaterial->Normal, true, UVSets, FVector2D(240,256) ) )
	{
		CreateAndLinkExpressionForMaterialProperty( FbxMaterial, UnrealMaterial, FbxSurfaceMaterial::sBump, UnrealMaterial->Normal, true, UVSets, FVector2D(240,256) ); // no bump in unreal, use as normal map
	}
	if (CreateAndLinkExpressionForMaterialProperty(FbxMaterial, UnrealMaterial, FbxSurfaceMaterial::sTransparentColor, UnrealMaterial->Opacity, false, UVSets, FVector2D(200, 256)))
	{
		UnrealMaterial->BlendMode = BLEND_Translucent;
		CreateAndLinkExpressionForMaterialProperty(FbxMaterial, UnrealMaterial, FbxSurfaceMaterial::sTransparencyFactor, UnrealMaterial->OpacityMask, false, UVSets, FVector2D(150, 256));

	}
	FixupMaterial( FbxMaterial, UnrealMaterial); // add random diffuse if none exists

	// compile shaders for PC (from UPrecompileShadersCommandlet::ProcessMaterial
	// and FMaterialEditor::UpdateOriginalMaterial)

	// let the material update itself if necessary
	UnrealMaterial->PreEditChange(NULL);
	UnrealMaterial->PostEditChange();
	
	ImportedMaterialData.AddImportedMaterial( FbxMaterial, *UnrealMaterial );

	OutMaterials.Add(UnrealMaterial);
}

int32 UnFbx::FFbxImporter::CreateNodeMaterials(FbxNode* FbxNode, TArray<UMaterialInterface*>& OutMaterials, TArray<FString>& UVSets)
{
	int32 MaterialCount = FbxNode->GetMaterialCount();
	for(int32 MaterialIndex=0; MaterialIndex < MaterialCount; ++MaterialIndex)
	{
		FbxSurfaceMaterial *FbxMaterial = FbxNode->GetMaterial(MaterialIndex);

		if( FbxMaterial )
		{
			CreateUnrealMaterial(*FbxMaterial, OutMaterials, UVSets);
		}
	}
	return MaterialCount;
}
