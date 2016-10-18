// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "StaticMeshResources.h"
#include "Components/SplineMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "MessageLog.h"
#include "UObjectToken.h"
#include "MapErrors.h"
#if WITH_EDITOR
#include "ShowFlags.h"
#include "Collision.h"
#include "ConvexVolume.h"
#include "HierarchicalLODUtilities.h"
#include "HierarchicalLODUtilitiesModule.h"
#endif
#include "ComponentInstanceDataCache.h"
#include "LightMap.h"
#include "ShadowMap.h"
#include "ComponentReregisterContext.h"
#include "Engine/ShadowMapTexture2D.h"
#include "AI/Navigation/NavCollision.h"
#include "Engine/StaticMeshSocket.h"
#include "NavigationSystemHelpers.h"
#include "AI/NavigationOctree.h"
#include "PhysicsEngine/BodySetup.h"

#define LOCTEXT_NAMESPACE "StaticMeshComponent"

DECLARE_MEMORY_STAT( TEXT( "StaticMesh VxColor Inst Mem" ), STAT_InstVertexColorMemory, STATGROUP_MemoryStaticMesh );
DECLARE_MEMORY_STAT( TEXT( "StaticMesh PreCulled Index Memory" ), STAT_StaticMeshPreCulledIndexMemory, STATGROUP_MemoryStaticMesh );

class FStaticMeshComponentInstanceData : public FSceneComponentInstanceData
{
public:
	FStaticMeshComponentInstanceData(const UStaticMeshComponent* SourceComponent)
		: FSceneComponentInstanceData(SourceComponent)
		, StaticMesh(SourceComponent->StaticMesh)
		, bHasCachedStaticLighting(false)
	{
	}

	virtual void ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase) override
	{
		FSceneComponentInstanceData::ApplyToComponent(Component, CacheApplyPhase);
		if (CacheApplyPhase == ECacheApplyPhase::PostUserConstructionScript)
		{
			CastChecked<UStaticMeshComponent>(Component)->ApplyComponentInstanceData(this);
		}
	}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		FSceneComponentInstanceData::AddReferencedObjects(Collector);

		Collector.AddReferencedObject(StaticMesh);
		if (bHasCachedStaticLighting)
		{
			for (FLightMapRef& LightMapRef : CachedStaticLighting.LODDataLightMap)
			{
				if (LightMapRef != nullptr)
				{
					LightMapRef->AddReferencedObjects(Collector);
				}
			}
			for (FShadowMapRef& ShadowMapRef : CachedStaticLighting.LODDataShadowMap)
			{
				if (ShadowMapRef != nullptr)
				{
					ShadowMapRef->AddReferencedObjects(Collector);
				}
			}		
		}
	}

	/** Add vertex color data for a specified LOD before RerunConstructionScripts is called */
	void AddVertexColorData(const struct FStaticMeshComponentLODInfo& LODInfo, uint32 LODIndex)
	{
		if (VertexColorLODs.Num() <= (int32)LODIndex)
		{
			VertexColorLODs.SetNum(LODIndex + 1);
		}
		FVertexColorLODData& VertexColorData = VertexColorLODs[LODIndex];
		VertexColorData.LODIndex = LODIndex;
		VertexColorData.PaintedVertices = LODInfo.PaintedVertices;
		LODInfo.OverrideVertexColors->GetVertexColors(VertexColorData.VertexBufferColors);
	}

	/** Re-apply vertex color data after RerunConstructionScripts is called */
	bool ApplyVertexColorData(UStaticMeshComponent* StaticMeshComponent) const
	{
		bool bAppliedAnyData = false;

		if (StaticMeshComponent != NULL)
		{
			StaticMeshComponent->SetLODDataCount(VertexColorLODs.Num(), StaticMeshComponent->LODData.Num());

			for (int32 LODDataIndex = 0; LODDataIndex < VertexColorLODs.Num(); ++LODDataIndex)
			{
				const FVertexColorLODData& VertexColorLODData = VertexColorLODs[LODDataIndex];
				uint32 LODIndex = VertexColorLODData.LODIndex;

				if (StaticMeshComponent->LODData.IsValidIndex(LODIndex))
				{
					FStaticMeshComponentLODInfo& LODInfo = StaticMeshComponent->LODData[LODIndex];
					// this component could have been constructed from a template
					// that had its own vert color overrides; so before we apply
					// the instance's color data, we need to clear the old
					// vert colors (so we can properly call InitFromColorArray())
					StaticMeshComponent->RemoveInstanceVertexColorsFromLOD(LODIndex);
					// may not be null at the start (could have been initialized 
					// from a  component template with vert coloring), but should
					// be null at this point, after RemoveInstanceVertexColorsFromLOD()
					if (LODInfo.OverrideVertexColors == NULL)
					{
						LODInfo.PaintedVertices = VertexColorLODData.PaintedVertices;

						LODInfo.OverrideVertexColors = new FColorVertexBuffer;
						LODInfo.OverrideVertexColors->InitFromColorArray(VertexColorLODData.VertexBufferColors);

						BeginInitResource(LODInfo.OverrideVertexColors);
						bAppliedAnyData = true;
					}
				}
			}
		}

		return bAppliedAnyData;
	}

	/** Used to store lightmap data during RerunConstructionScripts */
	struct FLightMapInstanceData
	{
		/** Transform of instance */
		FTransform		Transform;
		/** Lightmaps from LODData */
		TArray<FLightMapRef>	LODDataLightMap;
		TArray<FShadowMapRef>	LODDataShadowMap;
		TArray<FGuid> IrrelevantLights;
	};

	/** Vertex data stored per-LOD */
	struct FVertexColorLODData
	{
		/** copy of painted vertex data */
		TArray<FPaintedVertex> PaintedVertices;

		/** Copy of vertex buffer colors */
		TArray<FColor> VertexBufferColors;

		/** Index of the LOD that this data came from */
		uint32 LODIndex;

		/** Check whether this contains valid data */
		bool IsValid() const
		{
			return PaintedVertices.Num() > 0 && VertexBufferColors.Num() > 0;
		}
	};

	/** Mesh being used by component */
	class UStaticMesh* StaticMesh;

	/** Array of cached vertex colors for each LOD */
	TArray<FVertexColorLODData> VertexColorLODs;

	bool bHasCachedStaticLighting;
	FLightMapInstanceData CachedStaticLighting;
};

UStaticMeshComponent::UStaticMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;

	// check BaseEngine.ini for profile setup
	SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);

	WireframeColorOverride = FColor(255, 255, 255, 255);

	MinLOD = 0;
	bOverrideLightMapRes = false;
	OverriddenLightMapRes = 64;
	SubDivisionStepSize = 32;
	bUseSubDivisions = true;
	StreamingDistanceMultiplier = 1.0f;
	bBoundsChangeTriggersStreamingDataRebuild = true;
	bHasCustomNavigableGeometry = EHasCustomNavigableGeometry::Yes;
	bOverrideNavigationExport = false;
	bForceNavigationObstacle = true;
	bDisallowMeshPaintPerInstance = false;

	GetBodyInstance()->bAutoWeld = true;	//static mesh by default has auto welding

#if WITH_EDITORONLY_DATA
	SelectedEditorSection = INDEX_NONE;
	SectionIndexPreview = INDEX_NONE;
#endif
}

UStaticMeshComponent::~UStaticMeshComponent()
{
	// Empty, but required because we don't want to have to include LightMap.h and ShadowMap.h in StaticMeshComponent.h, and they are required to compile FLightMapRef and FShadowMapRef
}

void UStaticMeshComponent::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME( UStaticMeshComponent, StaticMesh );
}

void UStaticMeshComponent::OnRep_StaticMesh(class UStaticMesh *OldStaticMesh)
{
	// Only do stuff if this actually changed from the last local value
	if (OldStaticMesh!= StaticMesh)
	{
		// We have to force a call to SetStaticMesh with a new StaticMesh
		UStaticMesh *NewStaticMesh = StaticMesh;
		StaticMesh = NULL;
		
		SetStaticMesh(NewStaticMesh);
	}
}

bool UStaticMeshComponent::HasAnySockets() const
{
	return (StaticMesh != NULL) && (StaticMesh->Sockets.Num() > 0);
}

void UStaticMeshComponent::QuerySupportedSockets(TArray<FComponentSocketDescription>& OutSockets) const
{
	if (StaticMesh != NULL)
	{
		for (int32 SocketIdx = 0; SocketIdx < StaticMesh->Sockets.Num(); ++SocketIdx)
		{
			if (UStaticMeshSocket* Socket = StaticMesh->Sockets[SocketIdx])
			{
				new (OutSockets) FComponentSocketDescription(Socket->SocketName, EComponentSocketType::Socket);
			}
		}
	}
}

FString UStaticMeshComponent::GetDetailedInfoInternal() const
{
	FString Result;  

	if( StaticMesh != NULL )
	{
		Result = StaticMesh->GetPathName( NULL );
	}
	else
	{
		Result = TEXT("No_StaticMesh");
	}

	return Result;  
}


void UStaticMeshComponent::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{	
	UStaticMeshComponent* This = CastChecked<UStaticMeshComponent>(InThis);
	for (int32 LODIndex = 0; LODIndex < This->LODData.Num(); LODIndex++)
	{
		if (This->LODData[LODIndex].LightMap != NULL)
		{
			This->LODData[LODIndex].LightMap->AddReferencedObjects(Collector);
		}

		if (This->LODData[LODIndex].ShadowMap != NULL)
		{
			This->LODData[LODIndex].ShadowMap->AddReferencedObjects(Collector);
		}
	}

	Super::AddReferencedObjects(This, Collector);
}


void UStaticMeshComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

#if WITH_EDITORONLY_DATA
	if( Ar.IsCooking() )
	{
		// This is needed for data stripping
		for( int32 LODIndex = 0; LODIndex < LODData.Num(); LODIndex++ )
		{
			LODData[ LODIndex ].Owner = this;
		}
	}
#endif // WITH_EDITORONLY_DATA
	Ar << LODData;

	if (Ar.UE4Ver() < VER_UE4_COMBINED_LIGHTMAP_TEXTURES)
	{
		check(AttachmentCounter.GetValue() == 0);
		// Irrelevant lights were incorrect before VER_UE4_TOSS_IRRELEVANT_LIGHTS
		IrrelevantLights.Empty();
	}

	if (Ar.UE4Ver() < VER_UE4_AUTO_WELDING)
	{
		GetBodyInstance()->bAutoWeld = false;	//existing content may rely on no auto welding
	}
}



bool UStaticMeshComponent::AreNativePropertiesIdenticalTo( UObject* Other ) const
{
	bool bNativePropertiesAreIdentical = Super::AreNativePropertiesIdenticalTo( Other );
	UStaticMeshComponent* OtherSMC = CastChecked<UStaticMeshComponent>(Other);

	if( bNativePropertiesAreIdentical )
	{
		// Components are not identical if they have lighting information.
		if( LODData.Num() || OtherSMC->LODData.Num() )
		{
			bNativePropertiesAreIdentical = false;
		}
	}
	
	return bNativePropertiesAreIdentical;
}

void UStaticMeshComponent::PreSave(const class ITargetPlatform* TargetPlatform)
{
	Super::PreSave(TargetPlatform);
#if WITH_EDITORONLY_DATA
	CachePaintedDataIfNecessary();
#endif // WITH_EDITORONLY_DATA
}

#if WITH_EDITOR
void UStaticMeshComponent::CheckForErrors()
{
	Super::CheckForErrors();

	// Get the mesh owner's name.
	AActor* Owner = GetOwner();
	FString OwnerName(*(GNone.ToString()));
	if ( Owner )
	{
		OwnerName = Owner->GetName();
	}

	// Make sure any simplified meshes can still find their high res source mesh
	if( StaticMesh != NULL && StaticMesh->RenderData )
	{
		int32 ZeroTriangleElements = 0;

		// Check for element material index/material mismatches
		for (int32 LODIndex = 0; LODIndex < StaticMesh->RenderData->LODResources.Num(); ++LODIndex)
		{
			FStaticMeshLODResources& MeshLODData = StaticMesh->RenderData->LODResources[LODIndex];
			for (int32 SectionIndex = 0; SectionIndex < MeshLODData.Sections.Num(); SectionIndex++)
			{
				FStaticMeshSection& Element = MeshLODData.Sections[SectionIndex];
				if (Element.NumTriangles == 0)
				{
					ZeroTriangleElements++;
				}
			}
		}

		if (OverrideMaterials.Num() > StaticMesh->Materials.Num())
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("OverridenCount"), OverrideMaterials.Num());
			Arguments.Add(TEXT("ReferencedCount"), StaticMesh->Materials.Num());
			Arguments.Add(TEXT("MeshName"), FText::FromString(StaticMesh->GetName()));
			FMessageLog("MapCheck").Warning()
				->AddToken(FUObjectToken::Create(Owner))
				->AddToken(FTextToken::Create(FText::Format(LOCTEXT( "MapCheck_Message_MoreMaterialsThanReferenced", "More overridden materials ({OverridenCount}) on static mesh component than are referenced ({ReferencedCount}) in source mesh '{MeshName}'" ), Arguments ) ))
				->AddToken(FMapErrorToken::Create(FMapErrors::MoreMaterialsThanReferenced));
		}
		if (ZeroTriangleElements > 0)
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("ElementCount"), ZeroTriangleElements);
			Arguments.Add(TEXT("MeshName"), FText::FromString(StaticMesh->GetName()));
			FMessageLog("MapCheck").Warning()
				->AddToken(FUObjectToken::Create(Owner))
				->AddToken(FTextToken::Create(FText::Format(LOCTEXT( "MapCheck_Message_ElementsWithZeroTriangles", "{ElementCount} element(s) with zero triangles in static mesh '{MeshName}'" ), Arguments ) ))
				->AddToken(FMapErrorToken::Create(FMapErrors::ElementsWithZeroTriangles));
		}
	}

	if (!StaticMesh && (!Owner || !Owner->IsA(AWorldSettings::StaticClass())))	// Ignore worldsettings
	{
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(Owner))
			->AddToken(FTextToken::Create(LOCTEXT("MapCheck_Message_StaticMeshNull", "Static mesh actor has NULL StaticMesh property")))
			->AddToken(FMapErrorToken::Create(FMapErrors::StaticMeshNull));
	}

	if ( BodyInstance.bSimulatePhysics && StaticMesh != NULL && StaticMesh->BodySetup != NULL && StaticMesh->BodySetup->AggGeom.GetElementCount() == 0) 
	{
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(FText::Format(LOCTEXT( "MapCheck_Message_SimulatePhyNoSimpleCollision", "{0} : Using bSimulatePhysics but StaticMesh has not simple collision."), FText::FromString(GetName()) ) ));
	}

	// Warn if component with collision enabled, but no collision data
	if (StaticMesh != NULL && GetCollisionEnabled() != ECollisionEnabled::NoCollision)
	{
		int32 NumSectionsWithCollision = StaticMesh->GetNumSectionsWithCollision();
		int32 NumCollisionPrims = (StaticMesh->BodySetup != nullptr) ? StaticMesh->BodySetup->AggGeom.GetElementCount() : 0;

		if (NumSectionsWithCollision == 0 && NumCollisionPrims == 0)
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("ActorName"), FText::FromString(GetName()));
			Arguments.Add(TEXT("StaticMeshName"), FText::FromString(StaticMesh->GetName()));

			FMessageLog("MapCheck").Warning()
				->AddToken(FUObjectToken::Create(Owner))
				->AddToken(FTextToken::Create(FText::Format(LOCTEXT("MapCheck_Message_CollisionEnabledNoCollisionGeom", "Collision enabled but StaticMesh ({StaticMeshName}) has no simple or complex collision."), Arguments)))
				->AddToken(FMapErrorToken::Create(FMapErrors::CollisionEnabledNoCollisionGeom));
		}
	}

	if( Mobility == EComponentMobility::Movable &&
		CastShadow && 
		bCastDynamicShadow && 
		IsRegistered() && 
		Bounds.SphereRadius > 2000.0f )
	{
		// Large shadow casting objects that create preshadows will cause a massive performance hit, since preshadows are meant for small shadow casters.
		FMessageLog("MapCheck").PerformanceWarning()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(LOCTEXT( "MapCheck_Message_ActorLargeShadowCaster", "Large actor receives a pre-shadow and will cause an extreme performance hit unless bCastDynamicShadow is set to false.") ))
			->AddToken(FMapErrorToken::Create(FMapErrors::ActorLargeShadowCaster));
	}
}
#endif

FBoxSphereBounds UStaticMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if(StaticMesh)
	{
		// Graphics bounds.
		FBoxSphereBounds NewBounds = StaticMesh->GetBounds().TransformBy(LocalToWorld);
		NewBounds.BoxExtent *= BoundsScale;
		NewBounds.SphereRadius *= BoundsScale;

		return NewBounds;
	}
	else
	{
		return FBoxSphereBounds(LocalToWorld.GetLocation(), FVector::ZeroVector, 0.f);
	}
}

void UStaticMeshComponent::AddSpeedTreeWind()
{
	if (StaticMesh && StaticMesh->RenderData && StaticMesh->SpeedTreeWind.IsValid() && GetScene())
	{
		for (int32 LODIndex = 0; LODIndex < StaticMesh->RenderData->LODResources.Num(); ++LODIndex)
		{
			GetScene()->AddSpeedTreeWind(&StaticMesh->RenderData->LODResources[LODIndex].VertexFactory, StaticMesh);
		}
	}
}

void UStaticMeshComponent::RemoveSpeedTreeWind()
{
	if (StaticMesh && StaticMesh->RenderData && StaticMesh->SpeedTreeWind.IsValid() && GetScene())
	{
		for (int32 LODIndex = 0; LODIndex < StaticMesh->RenderData->LODResources.Num(); ++LODIndex)
		{
			GetScene()->RemoveSpeedTreeWind(&StaticMesh->RenderData->LODResources[LODIndex].VertexFactory, StaticMesh);
		}
	}
}

void UStaticMeshComponent::OnRegister()
{
	UpdateCollisionFromStaticMesh();
	
	if(StaticMesh != NULL && StaticMesh->RenderData)
	{
		// Check that the static-mesh hasn't been changed to be incompatible with the cached light-map.
		int32 NumLODs = StaticMesh->RenderData->LODResources.Num();
		bool bLODsShareStaticLighting = StaticMesh->RenderData->bLODsShareStaticLighting;
		if (!bLODsShareStaticLighting && NumLODs != LODData.Num())
		{
			for(int32 i=0;i<LODData.Num();i++)
			{
				LODData[i].LightMap = NULL;
				LODData[i].ShadowMap = NULL;
			}
		}
		AddSpeedTreeWind();
	}
	Super::OnRegister();
}

void UStaticMeshComponent::OnUnregister()
{
	RemoveSpeedTreeWind();

	Super::OnUnregister();
}

#if WITH_EDITORONLY_DATA

/** Return the total number of LOD sections in the LOD resources */
static int32 GetNumberOfElements(const TIndirectArray<FStaticMeshLODResources>& LODResources)
{
	int32 Count = 0;
	for (int32 LODIndex = 0; LODIndex < LODResources.Num(); ++LODIndex)
	{
		Count += LODResources[LODIndex].Sections.Num();
	}
	return Count;
}

/**
 *	Set this struct to match the unpacked params.
 *
 *	@param	LevelTextures			[in,out]	The list of textures referred by all component of a level. The array index maps to UTexture2D::LevelIndex.
 *	@param	UnpackedData			[in,out]	The unpacked data, emptied after the function executes.
 *	@param	StreamingTextureData	[out]		The resulting packed data.
 *	@param	RefBounds				[in]		The reference bounds used to packed the relative bounds.
 */
static void PackStreamingTextureData(TArray<UTexture2D*>& LevelTextures, TArray<FStreamingTexturePrimitiveInfo>& UnpackedData, TArray<FStreamingTextureBuildInfo>& StreamingTextureData, const FBoxSphereBounds& RefBounds)
{
	StreamingTextureData.Empty();

	while (UnpackedData.Num())
	{
		FStreamingTexturePrimitiveInfo Info = UnpackedData[0];
		UnpackedData.RemoveAtSwap(0);

		// Merge with any other lod section using the same texture.
		for (int32 Index = 0; Index < UnpackedData.Num(); ++Index)
		{
			const FStreamingTexturePrimitiveInfo& CurrInfo = UnpackedData[Index];

			if (CurrInfo.Texture == Info.Texture)
			{
				Info.Bounds = Union(Info.Bounds, CurrInfo.Bounds);
				// Take the max scale since it relates to higher texture resolution.
				Info.TexelFactor = FMath::Max<float>(Info.TexelFactor, CurrInfo.TexelFactor);

				UnpackedData.RemoveAtSwap(Index);
				--Index;
			}
		}

		FStreamingTextureBuildInfo PackedInfo;
		PackedInfo.PackFrom(LevelTextures, RefBounds, Info);
		StreamingTextureData.Push(PackedInfo);
	}
}

#endif

void UStaticMeshComponent::UpdateStreamingTextureData(TArray<UTexture2D*>& LevelTextures, const FTexCoordScaleMap& TexCoordScales, EMaterialQualityLevel::Type QualityLevel, ERHIFeatureLevel::Type FeatureLevel)
{
#if WITH_EDITORONLY_DATA // Only rebuild the data in editor 
	if (RequiresStreamingTextureData())
	{
		UpdateStreamingSectionData(TexCoordScales);

		TArray<FStreamingTexturePrimitiveInfo> UnpackedData;

		TArray<UTexture*> Textures;
		TArray< TArray<int32> > Indices;

		for (const FStreamingSectionBuildInfo& SectionData : *StreamingSectionData)
		{
			UMaterialInterface* Material = GetMaterial(SectionData.MaterialIndex);

			Textures.Empty();
			Indices.Empty();

			// Get the texture register indices used by each textures. 
			// Must be using the same quality and feature level as the one used to compute scales
			// in order to remap to the correct texture register index.
			Material->GetUsedTexturesAndIndices(Textures, Indices, QualityLevel, FeatureLevel);

			for (int32 TextureIndex = 0; TextureIndex < Textures.Num(); ++TextureIndex)
			{
				UTexture2D* Texture = Cast<UTexture2D>(Textures[TextureIndex]);
				if (!Texture) continue;

				bool bMaxValid = false;
				float MaxTexelFactor = 0;

				if (Indices.IsValidIndex(TextureIndex))
				{
					for (int32 TextureRegisterIndex : Indices[TextureIndex])
					{
						if (SectionData.TexCoordData.IsValidIndex(TextureRegisterIndex))
						{
							const FMaterialTexCoordBuildInfo& TexCoordInfo = SectionData.TexCoordData[TextureRegisterIndex];
							 // 0 would indicate that this register index is irrelevant (not used). Also could cause divide by 0
							if (TexCoordInfo.Scale > 0)
							{
								float TexelFactor = SectionData.TexelFactors[0];
								if (TexCoordInfo.Index >= 0 && TexCoordInfo.Index < FMaterialTexCoordBuildInfo::MAX_NUM_TEX_COORD && SectionData.TexelFactors[TexCoordInfo.Index] > 0)
								{
									TexelFactor = SectionData.TexelFactors[TexCoordInfo.Index];
								}
								MaxTexelFactor = FMath::Max<float>(MaxTexelFactor, TexelFactor / TexCoordInfo.Scale);
								bMaxValid = true;
							}
						}
					}
				}

				FStreamingTexturePrimitiveInfo& Info = *new(UnpackedData) FStreamingTexturePrimitiveInfo();
				Info.Texture = Texture;
				Info.TexelFactor = bMaxValid ? MaxTexelFactor : SectionData.TexelFactors[0];
				Info.Bounds.Origin = SectionData.BoxOrigin;
				Info.Bounds.BoxExtent = SectionData.BoxExtent;
				Info.Bounds.SphereRadius = SectionData.BoxExtent.Size();
			}
		}

		PackStreamingTextureData(LevelTextures, UnpackedData, StreamingTextureData, Bounds);
		bStreamingTextureDataValid = true;
		MarkPackageDirty();
	}
	else if (StreamingTextureData.Num() > 0)
	{
		StreamingTextureData.Empty();
		StreamingSectionData.Reset();
		bStreamingTextureDataValid = false; // Only meaningful if required (prevents having to set it for dynamic primitives)
		MarkPackageDirty();
	}
#endif
}

bool UStaticMeshComponent::RequiresStreamingTextureData() const
{
	const bool bUseMetrics = CVarStreamingUseNewMetrics.GetValueOnGameThread() != 0;
	const bool bNeedsPrecomputedData = !bIgnoreInstanceForTextureStreaming && Mobility == EComponentMobility::Static && StaticMesh && StaticMesh->RenderData;
	return bUseMetrics && bNeedsPrecomputedData;
}

bool UStaticMeshComponent::HasStreamingSectionData(bool bCheckTexCoordScales) const
{
#if WITH_EDITORONLY_DATA
	if (RequiresStreamingTextureData())
	{
		if (!StreamingSectionData.IsValid())
		{
			return false;
		}
		else if (bCheckTexCoordScales)
		{
			for (const FStreamingSectionBuildInfo& SectionData : *StreamingSectionData)
			{
				// If at least one section has data, then it's enough to say it was built with TexCoordScales.
				if (SectionData.TexCoordData.Num())
				{
					return true;
				}
			}
			return false;
		}
		else
		{
			return StreamingSectionData->Num() > 0;
		}
	}
#endif
	return false;
}

bool UStaticMeshComponent::HasStreamingTextureData() const
{
#if WITH_EDITORONLY_DATA
	return RequiresStreamingTextureData() && bStreamingTextureDataValid;
#else
	return RequiresStreamingTextureData() && StreamingTextureData.Num() > 0;
#endif
}

void UStaticMeshComponent::UpdateStreamingSectionData(const FTexCoordScaleMap& TexCoordScales)
{
#if WITH_EDITORONLY_DATA
	StreamingSectionData.Reset();

	if (RequiresStreamingTextureData())
	{
		StreamingSectionData = TSharedPtr<TArray<FStreamingSectionBuildInfo>, ESPMode::NotThreadSafe>(new TArray<FStreamingSectionBuildInfo>());

		const TIndirectArray<FStaticMeshLODResources>& LODResources = StaticMesh->RenderData->LODResources;
		StreamingSectionData->Reserve(GetNumberOfElements(LODResources) + 1);

		for (int32 LODIndex = 0; LODIndex < LODResources.Num(); ++LODIndex)
		{
			const FStaticMeshLODResources& LOD = LODResources[LODIndex];
			for (int32 ElementIndex = 0; ElementIndex < LOD.Sections.Num(); ++ElementIndex)
			{
				// If the streaming factors are not valid, this section can be ignored. Could be related to 0 area size.
				float LODElementTexelFactor;
				FBoxSphereBounds LODElementBounds;
				if (!StaticMesh->GetStreamingTextureFactor(LODElementTexelFactor, LODElementBounds, 0, LODIndex, ElementIndex, ComponentToWorld))
					continue;

				const FStaticMeshSection& Element = LOD.Sections[ElementIndex];
				UMaterialInterface* Material = GetMaterial(Element.MaterialIndex);
				if (!Material) continue;

				FStreamingSectionBuildInfo& SectionData = *new(*StreamingSectionData) FStreamingSectionBuildInfo();
				SectionData.LODIndex = LODIndex;
				SectionData.ElementIndex = ElementIndex;
				SectionData.MaterialIndex = Element.MaterialIndex;
				SectionData.BoxOrigin = LODElementBounds.Origin;
				SectionData.BoxExtent = LODElementBounds.BoxExtent;
				SectionData.TexelFactors[0] = LODElementTexelFactor;

				const TArray<FMaterialTexCoordBuildInfo>* TexCoordData = TexCoordScales.Find(Material);
				int32 MaxTexCoordIndex = MaxTexCoordIndex = FMaterialTexCoordBuildInfo::MAX_NUM_TEX_COORD - 1;
				if (TexCoordData)
				{
					SectionData.TexCoordData = *TexCoordData;
					MaxTexCoordIndex = 0;
					for (const FMaterialTexCoordBuildInfo& TexCoordInfo : *TexCoordData)
					{
						MaxTexCoordIndex = FMath::Max<int32>(TexCoordInfo.Index, MaxTexCoordIndex);
					}
					MaxTexCoordIndex = FMath::Min<int32>(MaxTexCoordIndex, FMaterialTexCoordBuildInfo::MAX_NUM_TEX_COORD - 1);
				}

				for (int32 TexCoordIndex = 1; TexCoordIndex <= MaxTexCoordIndex; ++TexCoordIndex)
				{
					if (StaticMesh->GetStreamingTextureFactor(LODElementTexelFactor, LODElementBounds, TexCoordIndex, LODIndex, ElementIndex, ComponentToWorld))
					{
						SectionData.TexelFactors[TexCoordIndex] = LODElementTexelFactor;
					}
				}

			}
		}
	}
#endif
}

bool UStaticMeshComponent::GetStreamingTextureFactors(float& OutWorldTexelFactor, float& OutWorldLightmapFactor) const
{
	if (StaticMesh && StaticMesh->RenderData && StaticMesh->RenderData->LODResources.Num() > 0)
	{
		OutWorldTexelFactor = OutWorldLightmapFactor = ComponentToWorld.GetMaximumAxisScale();
		OutWorldTexelFactor *= StaticMesh->GetStreamingTextureFactor(0);

		TIndirectArray<FStaticMeshLODResources>& LODResources = StaticMesh->RenderData->LODResources;
		const bool bHasValidLightmapCoordinates = StaticMesh->LightMapCoordinateIndex >= 0 && (uint32)StaticMesh->LightMapCoordinateIndex < LODResources[0].VertexBuffer.GetNumTexCoords();
		if (bHasValidLightmapCoordinates)
		{
			OutWorldLightmapFactor *= StaticMesh->GetStreamingTextureFactor(StaticMesh->LightMapCoordinateIndex);
		}
		else
		{
			OutWorldLightmapFactor = 0;
		}
		return true;
	}
	else
	{
		return false;
	}
}


void UStaticMeshComponent::GetStreamingTextureInfo(FStreamingTextureLevelContext& LevelContext, TArray<FStreamingTexturePrimitiveInfo>& OutStreamingTextures) const
{
	float WorldTexelFactor = 1.f;
	float WorldLightmapFactor = 1.f;

	if (!bIgnoreInstanceForTextureStreaming && GetStreamingTextureFactors(WorldTexelFactor, WorldLightmapFactor))
	{
		const ERHIFeatureLevel::Type FeatureLevel = GetWorld() ? GetWorld()->FeatureLevel : GMaxRHIFeatureLevel;
		const bool bUseNewMetrics = CVarStreamingUseNewMetrics.GetValueOnGameThread() != 0;

		const float MeshExtraScale = FMath::Max(0.0f, StaticMesh->StreamingDistanceMultiplier);
		const float PrimitiveExtraScale = FMath::Max(0.0f, StreamingDistanceMultiplier);
		LevelContext.BindComponent(bUseNewMetrics ? &StreamingTextureData : nullptr, Bounds, MeshExtraScale * PrimitiveExtraScale, WorldTexelFactor * PrimitiveExtraScale);

		for (int32 LODIndex = 0; LODIndex < StaticMesh->RenderData->LODResources.Num(); ++LODIndex)
		{
			FStaticMeshLODResources& LOD = StaticMesh->RenderData->LODResources[LODIndex];

			// Process each material applied to the top LOD of the mesh.
			for( int32 ElementIndex = 0; ElementIndex < LOD.Sections.Num(); ElementIndex++ )
			{
				const FStaticMeshSection& Element = LOD.Sections[ElementIndex];
				UMaterialInterface* Material = GetMaterial(Element.MaterialIndex);
				if(!Material)
				{
					Material = UMaterial::GetDefaultMaterial(MD_Surface);
				}

				// Enumerate the textures used by the material.
				TArray<UTexture*> Textures;
				Material->GetUsedTextures(Textures, EMaterialQualityLevel::Num, false, FeatureLevel, false);

				LevelContext.Process(Textures, OutStreamingTextures);
			}

			// Add in the lightmaps and shadowmaps.
			if ( LODData.IsValidIndex(LODIndex) && WorldLightmapFactor != 0)
			{
				const FStaticMeshComponentLODInfo& LODInfo = LODData[LODIndex];
				FLightMap2D* Lightmap = LODInfo.LightMap ? LODInfo.LightMap->GetLightMap2D() : NULL;
				uint32 LightmapIndex = AllowHighQualityLightmaps(FeatureLevel) ? 0 : 1;
				if ( Lightmap != NULL && Lightmap->IsValid(LightmapIndex) )
				{
					const FVector2D& Scale = Lightmap->GetCoordinateScale();
					if ( Scale.X > SMALL_NUMBER && Scale.Y > SMALL_NUMBER )
					{
						float LightmapFactorX		 = WorldLightmapFactor / Scale.X;
						float LightmapFactorY		 = WorldLightmapFactor / Scale.Y;
						FStreamingTexturePrimitiveInfo& StreamingTexture = *new(OutStreamingTextures) FStreamingTexturePrimitiveInfo;
						StreamingTexture.Bounds		 = Bounds;
						StreamingTexture.TexelFactor = FMath::Max(LightmapFactorX, LightmapFactorY);
						StreamingTexture.Texture	 = Lightmap->GetTexture(LightmapIndex);
					}
				}

				FShadowMap2D* Shadowmap = LODInfo.ShadowMap ? LODInfo.ShadowMap->GetShadowMap2D() : NULL;
				if ( Shadowmap != NULL && Shadowmap->IsValid() )
				{
					const FVector2D& Scale = Shadowmap->GetCoordinateScale();
					if ( Scale.X > SMALL_NUMBER && Scale.Y > SMALL_NUMBER )
					{
						float ShadowmapFactorX		 = WorldLightmapFactor / Scale.X;
						float ShadowmapFactorY		 = WorldLightmapFactor / Scale.Y;
						FStreamingTexturePrimitiveInfo& StreamingTexture = *new(OutStreamingTextures) FStreamingTexturePrimitiveInfo;
						StreamingTexture.Bounds		 = Bounds;
						StreamingTexture.TexelFactor = FMath::Max(ShadowmapFactorX, ShadowmapFactorY);
						StreamingTexture.Texture	 = Shadowmap->GetTexture();
					}
				}
			}
		}
	}
}

UBodySetup* UStaticMeshComponent::GetBodySetup()
{
	if(StaticMesh)
	{
		return StaticMesh->BodySetup;
	}

	return NULL;
}

bool UStaticMeshComponent::CanEditSimulatePhysics()
{
	if (UBodySetup* BodySetup = GetBodySetup())
	{
		return (BodySetup->AggGeom.GetElementCount() > 0) || (BodySetup->GetCollisionTraceFlag() == CTF_UseComplexAsSimple);
	}
	else
	{
		return false;
	}
}

FColor UStaticMeshComponent::GetWireframeColor() const
{
	if(bOverrideWireframeColor)
	{
		return WireframeColorOverride;
	}
	else
	{
		if(Mobility == EComponentMobility::Static)
		{
			return FColor(0, 255, 255, 255);
		}
		else if(Mobility == EComponentMobility::Stationary)
		{
			return FColor(128, 128, 255, 255);
		}
		else // Movable
		{
			if(BodyInstance.bSimulatePhysics)
			{
				return FColor(0, 255, 128, 255);
			}
			else
			{
				return FColor(255, 0, 255, 255);
			}
		}
	}
}


bool UStaticMeshComponent::DoesSocketExist(FName InSocketName) const 
{
	return (GetSocketByName(InSocketName)  != NULL);
}

class UStaticMeshSocket const* UStaticMeshComponent::GetSocketByName(FName InSocketName) const
{
	UStaticMeshSocket const* Socket = NULL;

	if( StaticMesh )
	{
		Socket = StaticMesh->FindSocket( InSocketName );
	}

	return Socket;
}

FTransform UStaticMeshComponent::GetSocketTransform(FName InSocketName, ERelativeTransformSpace TransformSpace) const
{
	if (InSocketName != NAME_None)
	{
		UStaticMeshSocket const* const Socket = GetSocketByName(InSocketName);
		if (Socket)
		{
			FTransform SocketWorldTransform;
			if ( Socket->GetSocketTransform(SocketWorldTransform, this) )
			{
				switch(TransformSpace)
				{
					case RTS_World:
					{
						return SocketWorldTransform;
					}
					case RTS_Actor:
					{
						if( const AActor* Actor = GetOwner() )
						{
							return SocketWorldTransform.GetRelativeTransform( GetOwner()->GetTransform() );
						}
						break;
					}
					case RTS_Component:
					{
						return SocketWorldTransform.GetRelativeTransform(ComponentToWorld);
					}
				}
			}
		}
	}

	return Super::GetSocketTransform(InSocketName, TransformSpace);
}


bool UStaticMeshComponent::RequiresOverrideVertexColorsFixup()
{
	bool bFixupRequired = false;

#if WITH_EDITORONLY_DATA
	if ( StaticMesh && StaticMesh->RenderData
		&& StaticMesh->RenderData->DerivedDataKey != StaticMeshDerivedDataKey
		&& LODData.Num() > 0
		&& LODData[0].OverrideVertexColors
		&& LODData[0].OverrideVertexColors->GetNumVertices() > 0
		&& LODData[0].PaintedVertices.Num() > 0 )
	{
		bFixupRequired = true;
	}
#endif // WITH_EDITORONLY_DATA

	return bFixupRequired;
}

void UStaticMeshComponent::SetSectionPreview(int32 InSectionIndexPreview)
{
#if WITH_EDITORONLY_DATA
	if (SectionIndexPreview != InSectionIndexPreview)
	{
		SectionIndexPreview = InSectionIndexPreview;
		MarkRenderStateDirty();
	}
#endif
}


void UStaticMeshComponent::RemoveInstanceVertexColorsFromLOD( int32 LODToRemoveColorsFrom )
{
#if WITH_EDITORONLY_DATA
	if (StaticMesh && LODToRemoveColorsFrom < StaticMesh->GetNumLODs() && LODToRemoveColorsFrom < LODData.Num())
	{
		FStaticMeshComponentLODInfo& CurrentLODInfo = LODData[LODToRemoveColorsFrom];

		CurrentLODInfo.ReleaseOverrideVertexColorsAndBlock();
		CurrentLODInfo.PaintedVertices.Empty();
		StaticMeshDerivedDataKey = StaticMesh->RenderData->DerivedDataKey;
	}
#endif
}

void UStaticMeshComponent::RemoveInstanceVertexColors()
{
#if WITH_EDITORONLY_DATA
	for ( int32 i=0; i < StaticMesh->GetNumLODs(); i++ )
	{
		RemoveInstanceVertexColorsFromLOD( i );
	}
#endif
}

void UStaticMeshComponent::CopyInstanceVertexColorsIfCompatible( UStaticMeshComponent* SourceComponent )
{
#if WITH_EDITORONLY_DATA
	// The static mesh assets have to match, currently.
	if (( StaticMesh->GetPathName() == SourceComponent->StaticMesh->GetPathName() ) &&
		( SourceComponent->LODData.Num() != 0 ))
	{
		Modify();

		bool bIsRegistered = IsRegistered();
		FComponentReregisterContext ReregisterContext(this);
		if (bIsRegistered)
		{
			FlushRenderingCommands(); // don't sync threads unless we have to
		}
		// Remove any and all vertex colors from the target static mesh, if they exist.
		RemoveInstanceVertexColors();

		int32 NumSourceLODs = SourceComponent->StaticMesh->GetNumLODs();

		// This this will set up the LODData for all the LODs
		SetLODDataCount( NumSourceLODs, NumSourceLODs );

		// Copy vertex colors from Source to Target (this)
		for ( int32 CurrentLOD = 0; CurrentLOD != NumSourceLODs; CurrentLOD++ )
		{
			FStaticMeshLODResources& SourceLODModel = SourceComponent->StaticMesh->RenderData->LODResources[CurrentLOD];
			FStaticMeshComponentLODInfo& SourceLODInfo = SourceComponent->LODData[CurrentLOD];

			FStaticMeshLODResources& TargetLODModel = StaticMesh->RenderData->LODResources[CurrentLOD];
			FStaticMeshComponentLODInfo& TargetLODInfo = LODData[CurrentLOD];

			if ( SourceLODInfo.OverrideVertexColors != NULL )
			{
				// Copy vertex colors from source to target.
				FColorVertexBuffer* SourceColorBuffer = SourceLODInfo.OverrideVertexColors;

				TArray< FColor > CopiedColors;
				for ( uint32 ColorVertexIndex = 0; ColorVertexIndex < SourceColorBuffer->GetNumVertices(); ColorVertexIndex++ )
				{
					CopiedColors.Add( SourceColorBuffer->VertexColor( ColorVertexIndex ) );
				}

				FColorVertexBuffer* TargetColorBuffer = &TargetLODModel.ColorVertexBuffer;

				if ( TargetLODInfo.OverrideVertexColors != NULL )
				{
					TargetLODInfo.BeginReleaseOverrideVertexColors();
					FlushRenderingCommands();
				}
				else
				{
					TargetLODInfo.OverrideVertexColors = new FColorVertexBuffer;
					TargetLODInfo.OverrideVertexColors->InitFromColorArray( CopiedColors );
				}
				BeginInitResource( TargetLODInfo.OverrideVertexColors );
			}
		}

		CachePaintedDataIfNecessary();
		StaticMeshDerivedDataKey = StaticMesh->RenderData->DerivedDataKey;

		MarkRenderStateDirty();
	}
#endif
}

void UStaticMeshComponent::CachePaintedDataIfNecessary()
{
#if WITH_EDITORONLY_DATA
	// Only cache the vertex positions if we're in the editor
	if ( GIsEditor && StaticMesh )
	{
		// Iterate over each component LOD info checking for the existence of override colors
		int32 NumLODs = StaticMesh->GetNumLODs();
		for ( TArray<FStaticMeshComponentLODInfo>::TIterator LODIter( LODData ); LODIter; ++LODIter )
		{
			FStaticMeshComponentLODInfo& CurCompLODInfo = *LODIter;

			// Workaround for a copy-paste bug. If the number of painted vertices is <= 1 we know the data is garbage.
			if ( CurCompLODInfo.PaintedVertices.Num() <= 1 )
			{
				CurCompLODInfo.PaintedVertices.Empty();
			}

			// If the mesh has override colors but no cached vertex positions, then the current vertex positions should be cached to help preserve instanced vertex colors during mesh tweaks
			// NOTE: We purposefully do *not* cache the positions if cached positions already exist, as this would result in the loss of the ability to fixup the component if the source mesh
			// were changed multiple times before a fix-up operation was attempted
			if ( CurCompLODInfo.OverrideVertexColors && 
				 CurCompLODInfo.OverrideVertexColors->GetNumVertices() > 0 &&
				 CurCompLODInfo.PaintedVertices.Num() == 0 &&
				 LODIter.GetIndex() < NumLODs ) 
			{
				FStaticMeshLODResources* CurRenderData = &(StaticMesh->RenderData->LODResources[ LODIter.GetIndex() ]);
				if ( CurRenderData->GetNumVertices() == CurCompLODInfo.OverrideVertexColors->GetNumVertices() )
				{
					// Cache the data.
					CurCompLODInfo.PaintedVertices.Reserve( CurRenderData->GetNumVertices() );
					for ( int32 VertIndex = 0; VertIndex < CurRenderData->GetNumVertices(); ++VertIndex )
					{
						FPaintedVertex* Vertex = new( CurCompLODInfo.PaintedVertices ) FPaintedVertex;
						Vertex->Position = CurRenderData->PositionVertexBuffer.VertexPosition( VertIndex );
						Vertex->Normal = CurRenderData->VertexBuffer.VertexTangentZ( VertIndex );
						Vertex->Color = CurCompLODInfo.OverrideVertexColors->VertexColor( VertIndex );
					}
				}
				else
				{					
					// At this point we can't resolve the colors, so just discard any isolated data we still have
					if (CurCompLODInfo.OverrideVertexColors && CurCompLODInfo.OverrideVertexColors->GetNumVertices() > 0)
					{
						UE_LOG(LogStaticMesh, Warning, TEXT("Level requires re-saving! Outdated vertex color overrides have been discarded for %s %s LOD%d. "), *GetFullName(), *StaticMesh->GetFullName(), LODIter.GetIndex());
						CurCompLODInfo.ReleaseOverrideVertexColorsAndBlock();
					}
					else
					{
						UE_LOG(LogStaticMesh, Warning, TEXT("Unable to cache painted data for mesh component. Vertex color overrides will be lost if the mesh is modified. %s %s LOD%d."), *GetFullName(), *StaticMesh->GetFullName(), LODIter.GetIndex() );
					}
				}
			}
		}
	}
#endif // WITH_EDITORONLY_DATA
}

bool UStaticMeshComponent::FixupOverrideColorsIfNecessary( bool bRebuildingStaticMesh )
{
#if WITH_EDITORONLY_DATA

	// Detect if there is a version mismatch between the source mesh and the component. If so, the component's LODs potentially
	// need to have their override colors updated to match changes in the source mesh.

	if ( RequiresOverrideVertexColorsFixup() )
	{
		// Check if we are building the static mesh.  If so we dont need to reregister this component as its already unregistered and will be reregistered
		// when the static mesh is done building.  Having nested reregister contexts is not supported.
		if( bRebuildingStaticMesh )
		{
			PrivateFixupOverrideColors();
		}
		else
		{
			// Detach this component because rendering changes are about to be applied
			FComponentReregisterContext ReregisterContext( this );
			PrivateFixupOverrideColors();
		}

		return true;
	}
#endif // WITH_EDITORONLY_DATA

	return false;
}

void UStaticMeshComponent::InitResources()
{
	for(int32 LODIndex = 0; LODIndex < LODData.Num(); LODIndex++)
	{
		FStaticMeshComponentLODInfo &LODInfo = LODData[LODIndex];
		if(LODInfo.OverrideVertexColors)
		{
			BeginInitResource(LODInfo.OverrideVertexColors);
			INC_DWORD_STAT_BY( STAT_InstVertexColorMemory, LODInfo.OverrideVertexColors->GetAllocatedSize() );
		}
	}
}

void UStaticMeshComponent::PrivateFixupOverrideColors()
{
#if WITH_EDITOR
	if (!StaticMesh || !StaticMesh->RenderData)
	{
		return;
	}

	const uint32 NumLODs = StaticMesh->RenderData->LODResources.Num();

	// Initialize override vertex colors on any new LODs which have just been created
	SetLODDataCount(NumLODs, LODData.Num());

	FStaticMeshComponentLODInfo& LOD0Info = LODData[0];
	if (LOD0Info.OverrideVertexColors)
	{
		for (uint32 LODIndex = 0; LODIndex < NumLODs; ++LODIndex)
		{
			FStaticMeshComponentLODInfo& LODInfo = LODData[LODIndex];

			if (LODInfo.OverrideVertexColors == nullptr)
			{
				LODInfo.OverrideVertexColors = new FColorVertexBuffer;
			}
			else
			{
				LODInfo.BeginReleaseOverrideVertexColors();
				FlushRenderingCommands();
			}

			FStaticMeshLODResources& CurRenderData = StaticMesh->RenderData->LODResources[LODIndex];

			TArray<FColor> NewOverrideColors;

			if (LOD0Info.PaintedVertices.Num() > 0)
			{
				// Build override colors for LOD, based on LOD0
				RemapPaintedVertexColors(
					LOD0Info.PaintedVertices,
					*LOD0Info.OverrideVertexColors,
					CurRenderData.PositionVertexBuffer,
					&CurRenderData.VertexBuffer,
					NewOverrideColors
					);
			}

			if (NewOverrideColors.Num())
			{
				LODInfo.OverrideVertexColors->InitFromColorArray(NewOverrideColors);

				// Update the PaintedVertices array
				const int32 NumVerts = CurRenderData.GetNumVertices();
				check(NumVerts == NewOverrideColors.Num());

				LODInfo.PaintedVertices.Reserve(NumVerts);
				for (int32 VertIndex = 0; VertIndex < NumVerts; ++VertIndex)
				{
					FPaintedVertex* Vertex = new(LODInfo.PaintedVertices) FPaintedVertex;
					Vertex->Position = CurRenderData.PositionVertexBuffer.VertexPosition(VertIndex);
					Vertex->Normal = CurRenderData.VertexBuffer.VertexTangentZ(VertIndex);
					Vertex->Color = LODInfo.OverrideVertexColors->VertexColor(VertIndex);
				}
			}

			BeginInitResource(LODInfo.OverrideVertexColors);
		}

		StaticMeshDerivedDataKey = StaticMesh->RenderData->DerivedDataKey;
	}

#endif // WITH_EDITOR
}

float GKeepPreCulledIndicesThreshold = .95f;

FAutoConsoleVariableRef CKeepPreCulledIndicesThreshold(
	TEXT("r.KeepPreCulledIndicesThreshold"),
	GKeepPreCulledIndicesThreshold,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

void UStaticMeshComponent::UpdatePreCulledData(int32 LODIndex, const TArray<uint32>& PreCulledData, const TArray<int32>& NumTrianglesPerSection)
{
	const FStaticMeshLODResources& StaticMeshLODResources = StaticMesh->RenderData->LODResources[LODIndex];

	int32 NumOriginalTriangles = 0;
	int32 NumVisibleTriangles = 0;

	for (int32 SectionIndex = 0; SectionIndex < StaticMeshLODResources.Sections.Num(); SectionIndex++)
	{
		const FStaticMeshSection& Section = StaticMeshLODResources.Sections[SectionIndex];
		NumOriginalTriangles += Section.NumTriangles;
		NumVisibleTriangles += NumTrianglesPerSection[SectionIndex];
	}

	if (NumVisibleTriangles / (float)NumOriginalTriangles < GKeepPreCulledIndicesThreshold)
	{
		SetLODDataCount(LODIndex + 1, LODData.Num());

		DEC_DWORD_STAT_BY(STAT_StaticMeshPreCulledIndexMemory, LODData[LODIndex].PreCulledIndexBuffer.GetAllocatedSize());
		//@todo - game thread
		check(IsInRenderingThread());
		LODData[LODIndex].PreCulledIndexBuffer.ReleaseResource();
		LODData[LODIndex].PreCulledIndexBuffer.SetIndices(PreCulledData, EIndexBufferStride::AutoDetect);
		LODData[LODIndex].PreCulledIndexBuffer.InitResource();

		INC_DWORD_STAT_BY(STAT_StaticMeshPreCulledIndexMemory, LODData[LODIndex].PreCulledIndexBuffer.GetAllocatedSize());
		LODData[LODIndex].PreCulledSections.Empty(StaticMeshLODResources.Sections.Num());

		int32 FirstIndex = 0;

		for (int32 SectionIndex = 0; SectionIndex < StaticMeshLODResources.Sections.Num(); SectionIndex++)
		{
			const FStaticMeshSection& Section = StaticMeshLODResources.Sections[SectionIndex];
			FPreCulledStaticMeshSection PreCulledSection;
			PreCulledSection.FirstIndex = FirstIndex;
			PreCulledSection.NumTriangles = NumTrianglesPerSection[SectionIndex];
			FirstIndex += PreCulledSection.NumTriangles * 3;
			LODData[LODIndex].PreCulledSections.Add(PreCulledSection);
		}
	}
	else if (LODIndex < LODData.Num())
	{
		LODData[LODIndex].PreCulledIndexBuffer.ReleaseResource();
		TArray<uint32> EmptyIndices;
		LODData[LODIndex].PreCulledIndexBuffer.SetIndices(EmptyIndices, EIndexBufferStride::AutoDetect);
		LODData[LODIndex].PreCulledSections.Empty(StaticMeshLODResources.Sections.Num());
	}
}

void UStaticMeshComponent::ReleaseResources()
{
	for(int32 LODIndex = 0;LODIndex < LODData.Num();LODIndex++)
	{
		LODData[LODIndex].BeginReleaseOverrideVertexColors();
		DEC_DWORD_STAT_BY(STAT_StaticMeshPreCulledIndexMemory, LODData[LODIndex].PreCulledIndexBuffer.GetAllocatedSize());
		BeginReleaseResource(&LODData[LODIndex].PreCulledIndexBuffer);
	}

	DetachFence.BeginFence();
}

void UStaticMeshComponent::BeginDestroy()
{
	Super::BeginDestroy();
	ReleaseResources();
}

void UStaticMeshComponent::ExportCustomProperties(FOutputDevice& Out, uint32 Indent)
{
	for (int32 LODIdx = 0; LODIdx < LODData.Num(); ++LODIdx)
	{
		Out.Logf(TEXT("%sCustomProperties "), FCString::Spc(Indent));

		FStaticMeshComponentLODInfo& LODInfo = LODData[LODIdx];

		if ((LODInfo.PaintedVertices.Num() > 0) || LODInfo.OverrideVertexColors)
		{
			Out.Logf( TEXT("CustomLODData LOD=%d "), LODIdx );
		}

		// Export the PaintedVertices array
		if (LODInfo.PaintedVertices.Num() > 0)
		{
			FString	ValueStr;
			LODInfo.ExportText(ValueStr);
			Out.Log(ValueStr);
		}
		
		// Export the OverrideVertexColors buffer
		if(LODInfo.OverrideVertexColors && LODInfo.OverrideVertexColors->GetNumVertices() > 0)
		{
			FString	Value;
			LODInfo.OverrideVertexColors->ExportText(Value);

			Out.Log(Value);
		}
		Out.Logf(TEXT("\r\n"));
	}
}

void UStaticMeshComponent::ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn)
{
	check(SourceText);
	check(Warn);

	if (FParse::Command(&SourceText,TEXT("CustomLODData")))
	{
		int32 MaxLODIndex = -1;
		int32 LODIndex;
		FString TmpStr;

		static const TCHAR LODString[] = TEXT("LOD=");
		if (FParse::Value(SourceText, LODString, LODIndex))
		{
			TmpStr = FString::Printf(TEXT("%d"), LODIndex);
			SourceText += TmpStr.Len() + (ARRAY_COUNT(LODString) - 1); // without the zero terminator

			// See if we need to add a new element to the LODData array
			if (LODIndex > MaxLODIndex)
			{
				SetLODDataCount(LODIndex + 1, LODIndex + 1);
				MaxLODIndex = LODIndex;
			}
		}

		FStaticMeshComponentLODInfo& LODInfo = LODData[LODIndex];

		// Populate the PaintedVertices array
		LODInfo.ImportText(&SourceText);

		// Populate the OverrideVertexColors buffer
		if (const TCHAR* VertColorStr = FCString::Stristr(SourceText, TEXT("ColorVertexData")))
		{
			SourceText = VertColorStr;

			// this component could have been constructed from a template that
			// had its own vert color overrides; so before we apply the
			// custom color data, we need to clear the old vert colors (so
			// we can properly call ImportText())
			RemoveInstanceVertexColorsFromLOD(LODIndex);

			// may not be null at the start (could have been initialized 
			// from a blueprint component template with vert coloring), but 
			// should be null by this point, after RemoveInstanceVertexColorsFromLOD()
			check(LODInfo.OverrideVertexColors == nullptr);

			LODInfo.OverrideVertexColors = new FColorVertexBuffer;
			LODInfo.OverrideVertexColors->ImportText(SourceText);
		}
	}
}

#if WITH_EDITOR

void UStaticMeshComponent::PreEditUndo()
{
	Super::PreEditUndo();

	// Undo can result in a resize of LODData which can calls ~FStaticMeshComponentLODInfo.
	// To safely delete FStaticMeshComponentLODInfo::OverrideVertexColors we
	// need to make sure the RT thread has no access to it any more.
	check(!IsRegistered());
	ReleaseResources();
	DetachFence.Wait();
}

void UStaticMeshComponent::PostEditUndo()
{
	// If the StaticMesh was also involved in this transaction, it may need reinitialization first
	if (StaticMesh)
	{
		StaticMesh->InitResources();
	}

	// The component's light-maps are loaded from the transaction, so their resources need to be reinitialized.
	InitResources();

	// Debug check command trying to track down undo related uninitialized resource
	if (StaticMesh != NULL && StaticMesh->RenderData.IsValid() && StaticMesh->RenderData->LODResources.Num() > 0)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			ResourceCheckCommand,
			FRenderResource*, Resource, &StaticMesh->RenderData->LODResources[0].IndexBuffer,
			{
				check( Resource->IsInitialized() );
			}
		);
	}
	Super::PostEditUndo();
}

void UStaticMeshComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Ensure that OverriddenLightMapRes is a factor of 4
	OverriddenLightMapRes = FMath::Max(OverriddenLightMapRes + 3 & ~3,4);

	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged)
	{
		if (((PropertyThatChanged->GetName().Contains(TEXT("OverriddenLightMapRes")) ) && (bOverrideLightMapRes == true)) ||
			(PropertyThatChanged->GetName().Contains(TEXT("bOverrideLightMapRes")) ))
		{
			InvalidateLightingCache();
		}

		if ( PropertyThatChanged->GetName().Contains(TEXT("bIgnoreInstanceForTextureStreaming")) ||
			 PropertyThatChanged->GetName().Contains(TEXT("StreamingDistanceMultiplier")) )
		{
			GEngine->TriggerStreamingDataRebuild();
		}

		if ( PropertyThatChanged->GetName() == TEXT("StaticMesh") )
		{
			InvalidateLightingCache();

			// If the owning actor is part of a cluster flag it as dirty
			IHierarchicalLODUtilitiesModule& Module = FModuleManager::LoadModuleChecked<IHierarchicalLODUtilitiesModule>("HierarchicalLODUtilities");
			IHierarchicalLODUtilities* Utilities = Module.GetUtilities();
			Utilities->HandleActorModified(GetOwner());

			// Broadcast that the static mesh has changed
			OnStaticMeshChangedEvent.Broadcast(this);
		}

		if (PropertyThatChanged->GetName() == TEXT("overridematerials"))
		{
			// If the owning actor is part of a cluster flag it as dirty
			IHierarchicalLODUtilitiesModule& Module = FModuleManager::LoadModuleChecked<IHierarchicalLODUtilitiesModule>("HierarchicalLODUtilities");
			IHierarchicalLODUtilities* Utilities = Module.GetUtilities();
			Utilities->HandleActorModified(GetOwner());
		}

#if WITH_EDITORONLY_DATA
		bStreamingTextureDataValid = false;
#endif
	}

	FBodyInstanceEditorHelpers::EnsureConsistentMobilitySimulationSettingsOnPostEditChange(this, PropertyChangedEvent);

	LightmassSettings.EmissiveBoost = FMath::Max(LightmassSettings.EmissiveBoost, 0.0f);
	LightmassSettings.DiffuseBoost = FMath::Max(LightmassSettings.DiffuseBoost, 0.0f);

	// Ensure properties are in sane range.
	SubDivisionStepSize = FMath::Clamp( SubDivisionStepSize, 1, 128 );

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

bool UStaticMeshComponent::SupportsDefaultCollision()
{
	return StaticMesh && GetBodySetup() == StaticMesh->BodySetup;
}

void UStaticMeshComponent::UpdateCollisionFromStaticMesh()
{
	if(bUseDefaultCollision && SupportsDefaultCollision())
	{
		if (UBodySetup* BodySetup = GetBodySetup())
		{
			BodyInstance.UseExternalCollisionProfile(BodySetup);	//static mesh component by default uses the same collision profile as its static mesh
		}
	}
}

void UStaticMeshComponent::PostLoad()
{
	// need to postload the StaticMesh because super initializes variables based on GetStaticLightingType() which we override and use from the StaticMesh
	if (StaticMesh)
	{
		StaticMesh->ConditionalPostLoad();
	}

	Super::PostLoad();

	if ( StaticMesh )
	{
		CachePaintedDataIfNecessary();

		double StartFixupTime = FPlatformTime::Seconds();

		if (FixupOverrideColorsIfNecessary())
		{
#if WITH_EDITORONLY_DATA

			AActor* Owner = GetOwner();

			if (Owner)
			{
				ULevel* Level = Owner->GetLevel();

				if (Level)
				{
					// Accumulate stats about the fixup so we don't spam log messages
					Level->FixupOverrideVertexColorsTime += (float)(FPlatformTime::Seconds() - StartFixupTime);
					Level->FixupOverrideVertexColorsCount++;
				}
			}
#endif
		}
	}

	// Empty after potential editor fix-up when we don't care about re-saving, e.g. game or client
	if (!GIsEditor && !IsRunningCommandlet())
	{
		for (FStaticMeshComponentLODInfo& LOD : LODData)
		{
			LOD.PaintedVertices.Empty();
		}
	}

#if WITH_EDITORONLY_DATA
	// Remap the materials array if the static mesh materials may have been remapped to remove zero triangle sections.
	if (StaticMesh && GetLinkerUE4Version() < VER_UE4_REMOVE_ZERO_TRIANGLE_SECTIONS && OverrideMaterials.Num())
	{
		StaticMesh->ConditionalPostLoad();
		if (StaticMesh->HasValidRenderData()
			&& StaticMesh->RenderData->MaterialIndexToImportIndex.Num())
		{
			TArray<UMaterialInterface*> OldMaterials;
			const TArray<int32>& MaterialIndexToImportIndex = StaticMesh->RenderData->MaterialIndexToImportIndex;

			Exchange(OverrideMaterials,OldMaterials);
			OverrideMaterials.Empty(MaterialIndexToImportIndex.Num());
			for (int32 MaterialIndex = 0; MaterialIndex < MaterialIndexToImportIndex.Num(); ++MaterialIndex)
			{
				UMaterialInterface* Material = NULL;
				int32 OldMaterialIndex = MaterialIndexToImportIndex[MaterialIndex];
				if (OldMaterials.IsValidIndex(OldMaterialIndex))
				{
					Material = OldMaterials[OldMaterialIndex];
				}
				OverrideMaterials.Add(Material);
			}
		}

		if (OverrideMaterials.Num() > StaticMesh->Materials.Num())
		{
			OverrideMaterials.RemoveAt(StaticMesh->Materials.Num(), OverrideMaterials.Num() - StaticMesh->Materials.Num());
		}
	}
#endif // #if WITH_EDITORONLY_DATA

	// Legacy content may contain a lightmap resolution of 0, which was valid when vertex lightmaps were supported, but not anymore with only texture lightmaps
	OverriddenLightMapRes = FMath::Max(OverriddenLightMapRes, 4);

	// Initialize the resources for the freshly loaded component.
	InitResources();
}

bool UStaticMeshComponent::SetStaticMesh(UStaticMesh* NewMesh)
{
	// Do nothing if we are already using the supplied static mesh
	if(NewMesh == StaticMesh)
	{
		return false;
	}

	// Don't allow changing static meshes if "static" and registered
	AActor* Owner = GetOwner();
	if(!AreDynamicDataChangesAllowed() && Owner != NULL)
	{
		FMessageLog("PIE").Warning(FText::Format(LOCTEXT("SetMeshOnStatic", "Calling SetStaticMesh on '{0}' but Mobility is Static."), 
			FText::FromString(GetPathName())));
		return false;
	}

	// Remove speed tree wind for this staticmesh from scene
	RemoveSpeedTreeWind();

	StaticMesh = NewMesh;

	// Add speed tree wind if required
	AddSpeedTreeWind();

	// Need to send this to render thread at some point
	MarkRenderStateDirty();

	// Update physics representation right away
	RecreatePhysicsState();

	// update navigation relevancy
	bNavigationRelevant = IsNavigationRelevant();

	// Notify the streaming system. Don't use Update(), because this may be the first time the mesh has been set
	// and the component may have to be added to the streaming system for the first time.
	IStreamingManager::Get().NotifyPrimitiveAttached( this, DPT_Spawned );

	// Since we have new mesh, we need to update bounds
	UpdateBounds();

	// Mark cached material parameter names dirty
	MarkCachedMaterialParameterNameIndicesDirty();

#if WITH_EDITOR
	// Broadcast that the static mesh has changed
	OnStaticMeshChangedEvent.Broadcast(this);
#endif

	return true;
}

void UStaticMeshComponent::SetForcedLodModel(int32 NewForcedLodModel)
{
	if (ForcedLodModel != NewForcedLodModel)
	{
		ForcedLodModel = NewForcedLodModel;
		MarkRenderStateDirty();
	}
}

void UStaticMeshComponent::GetLocalBounds(FVector& Min, FVector& Max) const
{
	if (StaticMesh)
	{
		FBoxSphereBounds MeshBounds = StaticMesh->GetBounds();
		Min = MeshBounds.Origin - MeshBounds.BoxExtent;
		Max = MeshBounds.Origin + MeshBounds.BoxExtent;
	}
}

void UStaticMeshComponent::SetCollisionProfileName(FName InCollisionProfileName)
{
	Super::SetCollisionProfileName(InCollisionProfileName);
	bUseDefaultCollision = false;
}

bool UStaticMeshComponent::UsesOnlyUnlitMaterials() const
{
	if (StaticMesh && StaticMesh->RenderData)
	{
		// Figure out whether any of the sections has a lit material assigned.
		bool bUsesOnlyUnlitMaterials = true;
		for (int32 LODIndex = 0; bUsesOnlyUnlitMaterials && LODIndex < StaticMesh->RenderData->LODResources.Num(); ++LODIndex)
		{
			FStaticMeshLODResources& LOD = StaticMesh->RenderData->LODResources[LODIndex];
			for (int32 ElementIndex=0; bUsesOnlyUnlitMaterials && ElementIndex<LOD.Sections.Num(); ElementIndex++)
			{
				UMaterialInterface*	MaterialInterface	= GetMaterial(LOD.Sections[ElementIndex].MaterialIndex);
				UMaterial*			Material			= MaterialInterface ? MaterialInterface->GetMaterial() : NULL;

				bUsesOnlyUnlitMaterials = Material && Material->GetShadingModel() == MSM_Unlit;
			}
		}
		return bUsesOnlyUnlitMaterials;
	}
	else
	{
		return false;
	}
}


bool UStaticMeshComponent::GetLightMapResolution( int32& Width, int32& Height ) const
{
	bool bPadded = false;
	if( StaticMesh )
	{
		// Use overridden per component lightmap resolution.
		if( bOverrideLightMapRes )
		{
			Width	= OverriddenLightMapRes;
			Height	= OverriddenLightMapRes;
		}
		// Use the lightmap resolution defined in the static mesh.
		else
		{
			Width	= StaticMesh->LightMapResolution;
			Height	= StaticMesh->LightMapResolution;
		}
		bPadded = true;
	}
	// No associated static mesh!
	else
	{
		Width	= 0;
		Height	= 0;
	}

	return bPadded;
}


void UStaticMeshComponent::GetEstimatedLightMapResolution(int32& Width, int32& Height) const
{
	if (StaticMesh)
	{
		ELightMapInteractionType LMIType = GetStaticLightingType();

		bool bUseSourceMesh = false;

		// Use overridden per component lightmap resolution.
		// If the overridden LM res is > 0, then this is what would be used...
		if (bOverrideLightMapRes == true)
		{
			if (OverriddenLightMapRes != 0)
			{
				Width	= OverriddenLightMapRes;
				Height	= OverriddenLightMapRes;
			}
		}
		else
		{
			bUseSourceMesh = true;
		}

		// Use the lightmap resolution defined in the static mesh.
		if (bUseSourceMesh == true)
		{
			Width	= StaticMesh->LightMapResolution;
			Height	= StaticMesh->LightMapResolution;
		}

		// If it was not set by anything, give it a default value...
		if (Width == 0)
		{
			int32 TempInt = 0;
			verify(GConfig->GetInt(TEXT("DevOptions.StaticLighting"), TEXT("DefaultStaticMeshLightingRes"), TempInt, GLightmassIni));

			Width	= TempInt;
			Height	= TempInt;
		}
	}
	else
	{
		Width	= 0;
		Height	= 0;
	}
}


int32 UStaticMeshComponent::GetStaticLightMapResolution() const
{
	int32 Width, Height;
	GetLightMapResolution(Width, Height);
	return FMath::Max<int32>(Width, Height);
}

bool UStaticMeshComponent::HasValidSettingsForStaticLighting(bool bOverlookInvalidComponents) const
{
	if (bOverlookInvalidComponents && !StaticMesh)
	{
		// Return true for invalid components, this is used during the map check where those invalid components will be warned about separately
		return true;
	}
	else
	{
		int32 LightMapWidth = 0;
		int32 LightMapHeight = 0;
		GetLightMapResolution(LightMapWidth, LightMapHeight);

		return Super::HasValidSettingsForStaticLighting(bOverlookInvalidComponents) 
			&& StaticMesh
			&& UsesTextureLightmaps(LightMapWidth, LightMapHeight);
	}
}

bool UStaticMeshComponent::UsesTextureLightmaps(int32 InWidth, int32 InHeight) const
{
	return (
		(HasLightmapTextureCoordinates()) &&
		(InWidth > 0) && 
		(InHeight > 0)
		);
}


bool UStaticMeshComponent::HasLightmapTextureCoordinates() const
{
	if ((StaticMesh != NULL) &&
		(StaticMesh->LightMapCoordinateIndex >= 0) &&
		(StaticMesh->RenderData != NULL) &&
		(StaticMesh->RenderData->LODResources.Num() > 0) &&
		(StaticMesh->LightMapCoordinateIndex >= 0) &&	
		((uint32)StaticMesh->LightMapCoordinateIndex < StaticMesh->RenderData->LODResources[0].VertexBuffer.GetNumTexCoords()))
	{
		return true;
	}
	return false;
}


void UStaticMeshComponent::GetTextureLightAndShadowMapMemoryUsage(int32 InWidth, int32 InHeight, int32& OutLightMapMemoryUsage, int32& OutShadowMapMemoryUsage) const
{
	// Stored in texture.
	const float MIP_FACTOR = 1.33f;
	OutShadowMapMemoryUsage = FMath::TruncToInt(MIP_FACTOR * InWidth * InHeight); // G8

	auto FeatureLevel = GetWorld() ? GetWorld()->FeatureLevel : GMaxRHIFeatureLevel;

	if (AllowHighQualityLightmaps(FeatureLevel))
	{
		OutLightMapMemoryUsage = FMath::TruncToInt(NUM_HQ_LIGHTMAP_COEF * MIP_FACTOR * InWidth * InHeight); // DXT5
	}
	else
	{
		OutLightMapMemoryUsage = FMath::TruncToInt(NUM_LQ_LIGHTMAP_COEF * MIP_FACTOR * InWidth * InHeight / 2); // DXT1
	}
}


void UStaticMeshComponent::GetLightAndShadowMapMemoryUsage( int32& LightMapMemoryUsage, int32& ShadowMapMemoryUsage ) const
{
	// Zero initialize.
	ShadowMapMemoryUsage = 0;
	LightMapMemoryUsage = 0;

	// Cache light/ shadow map resolution.
	int32 LightMapWidth = 0;
	int32	LightMapHeight = 0;
	GetLightMapResolution(LightMapWidth, LightMapHeight);

	// Determine whether static mesh/ static mesh component has static shadowing.
	if (HasStaticLighting() && StaticMesh)
	{
		// Determine whether we are using a texture or vertex buffer to store precomputed data.
		if (UsesTextureLightmaps(LightMapWidth, LightMapHeight) == true)
		{
			GetTextureLightAndShadowMapMemoryUsage(LightMapWidth, LightMapHeight, LightMapMemoryUsage, ShadowMapMemoryUsage);
		}
	}
}


bool UStaticMeshComponent::GetEstimatedLightAndShadowMapMemoryUsage( 
	int32& TextureLightMapMemoryUsage, int32& TextureShadowMapMemoryUsage,
	int32& VertexLightMapMemoryUsage, int32& VertexShadowMapMemoryUsage,
	int32& StaticLightingResolution, bool& bIsUsingTextureMapping, bool& bHasLightmapTexCoords) const
{
	TextureLightMapMemoryUsage	= 0;
	TextureShadowMapMemoryUsage	= 0;
	VertexLightMapMemoryUsage	= 0;
	VertexShadowMapMemoryUsage	= 0;
	bIsUsingTextureMapping		= false;
	bHasLightmapTexCoords		= false;

	// Cache light/ shadow map resolution.
	int32 LightMapWidth			= 0;
	int32 LightMapHeight		= 0;
	GetEstimatedLightMapResolution(LightMapWidth, LightMapHeight);
	StaticLightingResolution = LightMapWidth;

	int32 TrueLightMapWidth		= 0;
	int32 TrueLightMapHeight	= 0;
	GetLightMapResolution(TrueLightMapWidth, TrueLightMapHeight);

	// Determine whether static mesh/ static mesh component has static shadowing.
	if (HasStaticLighting() && StaticMesh)
	{
		// Determine whether we are using a texture or vertex buffer to store precomputed data.
		bHasLightmapTexCoords = HasLightmapTextureCoordinates();
		// Determine whether we are using a texture or vertex buffer to store precomputed data.
		bIsUsingTextureMapping = UsesTextureLightmaps(TrueLightMapWidth, TrueLightMapHeight);
		// Stored in texture.
		GetTextureLightAndShadowMapMemoryUsage(LightMapWidth, LightMapHeight, TextureLightMapMemoryUsage, TextureShadowMapMemoryUsage);

		return true;
	}

	return false;
}

int32 UStaticMeshComponent::GetNumMaterials() const
{
	// @note : you don't have to consider Materials.Num()
	// that only counts if overridden and it can't be more than StaticMesh->Materials. 
	if(StaticMesh)
	{
		return StaticMesh->Materials.Num();
	}
	else
	{
		return 0;
	}
}

UMaterialInterface* UStaticMeshComponent::GetMaterial(int32 MaterialIndex) const
{
	// If we have a base materials array, use that
	if(MaterialIndex < OverrideMaterials.Num() && OverrideMaterials[MaterialIndex])
	{
		return OverrideMaterials[MaterialIndex];
	}
	// Otherwise get from static mesh
	else
	{
		return StaticMesh ? StaticMesh->GetMaterial(MaterialIndex) : nullptr;
	}
}

void UStaticMeshComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials) const
{
	if( StaticMesh && StaticMesh->RenderData )
	{
		for (int32 LODIndex = 0; LODIndex < StaticMesh->RenderData->LODResources.Num(); LODIndex++)
		{
			FStaticMeshLODResources& LODResources = StaticMesh->RenderData->LODResources[LODIndex];
			for (int32 SectionIndex = 0; SectionIndex < LODResources.Sections.Num(); SectionIndex++)
			{
				// Get the material for each element at the current lod index
				OutMaterials.AddUnique(GetMaterial(LODResources.Sections[SectionIndex].MaterialIndex));
			}
		}
	}
}

int32 UStaticMeshComponent::GetBlueprintCreatedComponentIndex() const
{
	int32 ComponentIndex = 0;
	for(const auto& Component : GetOwner()->BlueprintCreatedComponents)
	{
		if(Component == this)
		{
			return ComponentIndex;
		}

		ComponentIndex++;
	}

	return INDEX_NONE;
}

FActorComponentInstanceData* UStaticMeshComponent::GetComponentInstanceData() const
{
	FStaticMeshComponentInstanceData* StaticMeshInstanceData = nullptr;

	// Don't back up static lighting if there isn't any
	if(bHasCachedStaticLighting)
	{
		StaticMeshInstanceData = new FStaticMeshComponentInstanceData(this);

		// Fill in info
		StaticMeshInstanceData->bHasCachedStaticLighting = true;
		StaticMeshInstanceData->CachedStaticLighting.Transform = ComponentToWorld;
		StaticMeshInstanceData->CachedStaticLighting.IrrelevantLights = IrrelevantLights;
		StaticMeshInstanceData->CachedStaticLighting.LODDataLightMap.Empty(LODData.Num());
		for (const FStaticMeshComponentLODInfo& LODDataEntry : LODData)
		{
			StaticMeshInstanceData->CachedStaticLighting.LODDataLightMap.Add(LODDataEntry.LightMap);
			StaticMeshInstanceData->CachedStaticLighting.LODDataShadowMap.Add(LODDataEntry.ShadowMap);
		}
	}


	// Cache instance vertex colors
	for( int32 LODIndex = 0; LODIndex < LODData.Num(); ++LODIndex )
	{
		const FStaticMeshComponentLODInfo& LODInfo = LODData[LODIndex];

		if ( LODInfo.OverrideVertexColors && LODInfo.OverrideVertexColors->GetNumVertices() > 0 && LODInfo.PaintedVertices.Num() > 0 )
		{
			if (!StaticMeshInstanceData)
			{
				StaticMeshInstanceData = new FStaticMeshComponentInstanceData(this);
			}

			StaticMeshInstanceData->AddVertexColorData(LODInfo, LODIndex);
		}
	}

	return (StaticMeshInstanceData ? StaticMeshInstanceData : Super::GetComponentInstanceData());
}

void UStaticMeshComponent::ApplyComponentInstanceData(FStaticMeshComponentInstanceData* StaticMeshInstanceData)
{
	check(StaticMeshInstanceData);

	// Note: ApplyComponentInstanceData is called while the component is registered so the rendering thread is already using this component
	// That means all component state that is modified here must be mirrored on the scene proxy, which will be recreated to receive the changes later due to MarkRenderStateDirty.
	// Changing refcounted pointers like LightMap works because those are deferred cleanup resources (deleted next frame)

	if (StaticMesh != StaticMeshInstanceData->StaticMesh)
	{
		return;
	}

	if (StaticMeshInstanceData->bHasCachedStaticLighting)
	{
		// See if data matches current state
		if (StaticMeshInstanceData->CachedStaticLighting.Transform.Equals(ComponentToWorld, 1.e-3f))
		{
			const int32 NumLODLightMaps = StaticMeshInstanceData->CachedStaticLighting.LODDataLightMap.Num();
			SetLODDataCount(NumLODLightMaps, NumLODLightMaps);

			for (int32 i = 0; i < NumLODLightMaps; ++i)
			{
				LODData[i].LightMap = StaticMeshInstanceData->CachedStaticLighting.LODDataLightMap[i];
				LODData[i].ShadowMap = StaticMeshInstanceData->CachedStaticLighting.LODDataShadowMap[i];

				// this code is to try to track down a mystery GC crash from crash reporter...if this code crashes, and then we know it is component reinstancing
				if (LODData[i].LightMap.GetReference())
				{
					FLightMap2D* LightMap = LODData[i].LightMap->GetLightMap2D();
					if (LightMap)
					{
						if (LightMap->IsValid(0))
						{
							UTexture2D* Tex = LightMap->GetTexture(0);
							Tex->GetResourceSize(EResourceSizeMode::Exclusive);
						}
						if (LightMap->IsValid(1))
						{
							UTexture2D* Tex = LightMap->GetTexture(1);
							Tex->GetResourceSize(EResourceSizeMode::Exclusive);
						}
						if (LightMap->GetSkyOcclusionTexture())
						{
							UTexture2D* Tex = LightMap->GetSkyOcclusionTexture();
							Tex->GetResourceSize(EResourceSizeMode::Exclusive);
						}
					}
				}
			}

			IrrelevantLights = StaticMeshInstanceData->CachedStaticLighting.IrrelevantLights;
			bHasCachedStaticLighting = true;
		}
		else
		{
			UE_LOG(LogStaticMesh, Warning, TEXT("Cached component instance data transform did not match!  Discarding cached lighting data which will cause lighting to be unbuilt.\n%s\nCurrent: %s Cached: %s"), 
				*GetPathName(),
				*ComponentToWorld.ToString(),
				*StaticMeshInstanceData->CachedStaticLighting.Transform.ToString());
		}
	}

	if (!bDisallowMeshPaintPerInstance)
	{
		FComponentReregisterContext ReregisterStaticMesh(this);
		StaticMeshInstanceData->ApplyVertexColorData(this);
	}
}

#include "AI/Navigation/RecastHelpers.h"
bool UStaticMeshComponent::DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const
{
	if (StaticMesh != NULL && StaticMesh->NavCollision != NULL)
	{
		UNavCollision* NavCollision = StaticMesh->NavCollision;
		const bool bExportAsObstacle = bOverrideNavigationExport ? bForceNavigationObstacle : NavCollision->bIsDynamicObstacle;

		if (bExportAsObstacle)
		{
			return false;
		}
		
		if (NavCollision->bHasConvexGeometry)
		{
			const FVector Scale3D = ComponentToWorld.GetScale3D();
			// if any of scales is 0 there's no point in exporting it
			if (!Scale3D.IsZero())
			{
				GeomExport.ExportCustomMesh(NavCollision->ConvexCollision.VertexBuffer.GetData(), NavCollision->ConvexCollision.VertexBuffer.Num(),
					NavCollision->ConvexCollision.IndexBuffer.GetData(), NavCollision->ConvexCollision.IndexBuffer.Num(), ComponentToWorld);

				GeomExport.ExportCustomMesh(NavCollision->TriMeshCollision.VertexBuffer.GetData(), NavCollision->TriMeshCollision.VertexBuffer.Num(),
					NavCollision->TriMeshCollision.IndexBuffer.GetData(), NavCollision->TriMeshCollision.IndexBuffer.Num(), ComponentToWorld);
			}

			// regardless of above we don't want "regular" collision export for this mesh instance
			return false;
		}
	}

	return true;
}

bool UStaticMeshComponent::IsNavigationRelevant() const
{
	return StaticMesh != nullptr && Super::IsNavigationRelevant();
}

void UStaticMeshComponent::GetNavigationData(FNavigationRelevantData& Data) const
{
	Super::GetNavigationData(Data);

	if (StaticMesh && StaticMesh->NavCollision)	
	{
		UNavCollision* NavCollision = StaticMesh->NavCollision;
		const bool bExportAsObstacle = bOverrideNavigationExport ? bForceNavigationObstacle : NavCollision->bIsDynamicObstacle;

		if (bExportAsObstacle)
		{
			NavCollision->GetNavigationModifier(Data.Modifiers, ComponentToWorld);
		}
	}
}

#if WITH_EDITOR
bool UStaticMeshComponent::ComponentIsTouchingSelectionBox(const FBox& InSelBBox, const FEngineShowFlags& ShowFlags, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const
{
	if (!bConsiderOnlyBSP && ShowFlags.StaticMeshes && StaticMesh != nullptr && StaticMesh->HasValidRenderData())
	{
		// Check if we are even inside it's bounding box, if we are not, there is no way we colliding via the more advanced checks we will do.
		if (Super::ComponentIsTouchingSelectionBox(InSelBBox, ShowFlags, bConsiderOnlyBSP, false))
		{
			TArray<FVector> Vertex;

			FStaticMeshLODResources& LODModel = StaticMesh->RenderData->LODResources[0];
			FIndexArrayView Indices = LODModel.IndexBuffer.GetArrayView();

			for (const auto& Section : LODModel.Sections)
			{
				// Iterate over each triangle.
				for (int32 TriangleIndex = 0; TriangleIndex < (int32)Section.NumTriangles; TriangleIndex++)
				{
					Vertex.Empty(3);

					int32 FirstIndex = TriangleIndex * 3 + Section.FirstIndex;
					for (int32 i = 0; i < 3; i++)
					{
						int32 VertexIndex = Indices[FirstIndex + i];
						FVector LocalPosition = LODModel.PositionVertexBuffer.VertexPosition(VertexIndex);
						Vertex.Emplace(ComponentToWorld.TransformPosition(LocalPosition));
					}

					// Check if the triangle is colliding with the bounding box.
					FSeparatingAxisPointCheck ThePointCheck(Vertex, InSelBBox.GetCenter(), InSelBBox.GetExtent(), false);
					if (!bMustEncompassEntireComponent && ThePointCheck.bHit)
					{
						// Needn't encompass entire component: any intersection, we consider as touching
						return true;
					}
					else if (bMustEncompassEntireComponent && !ThePointCheck.bHit)
					{
						// Must encompass entire component: any non intersection, we consider as not touching
						return false;
					}
				}
			}

			// Either:
			// a) It must encompass the entire component and all points were intersected (return true), or;
			// b) It needn't encompass the entire component but no points were intersected (return false)
			return bMustEncompassEntireComponent;
		}
	}

	return false;
}


bool UStaticMeshComponent::ComponentIsTouchingSelectionFrustum(const FConvexVolume& InFrustum, const FEngineShowFlags& ShowFlags, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const
{
	if (!bConsiderOnlyBSP && ShowFlags.StaticMeshes && StaticMesh != nullptr && StaticMesh->HasValidRenderData())
	{
		// Check if we are even inside it's bounding box, if we are not, there is no way we colliding via the more advanced checks we will do.
		if (Super::ComponentIsTouchingSelectionFrustum(InFrustum, ShowFlags, bConsiderOnlyBSP, false))
		{
			TArray<FVector> Vertex;

			FStaticMeshLODResources& LODModel = StaticMesh->RenderData->LODResources[0];

			uint32 NumVertices = LODModel.VertexBuffer.GetNumVertices();
			for (uint32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
			{
				const FVector& LocalPosition = LODModel.PositionVertexBuffer.VertexPosition(VertexIndex);
				const FVector WorldPosition = ComponentToWorld.TransformPosition(LocalPosition);
				bool bLocationIntersected = InFrustum.IntersectSphere(WorldPosition, 0.0f);
				if (bLocationIntersected && !bMustEncompassEntireComponent)
				{
					return true;
				}
				else if (!bLocationIntersected && bMustEncompassEntireComponent)
				{
					return false;
				}
			}

			// Either:
			// a) It must encompass the entire component and all points were intersected (return true), or;
			// b) It needn't encompass the entire component but no points were intersected (return false)
			return bMustEncompassEntireComponent;
		}
	}

	return false;
}
#endif


//////////////////////////////////////////////////////////////////////////
// StaticMeshComponentLODInfo



/** Default constructor */
FStaticMeshComponentLODInfo::FStaticMeshComponentLODInfo()
	: OverrideVertexColors(NULL)
#if WITH_EDITORONLY_DATA
	, Owner(NULL)
#endif
{
}

/** Copy constructor */
FStaticMeshComponentLODInfo::FStaticMeshComponentLODInfo(const FStaticMeshComponentLODInfo &rhs)
	: OverrideVertexColors(NULL)
#if WITH_EDITORONLY_DATA
	, Owner(NULL)
#endif
{
	if(rhs.OverrideVertexColors)
	{
		OverrideVertexColors = new FColorVertexBuffer;
		OverrideVertexColors->InitFromColorArray(&rhs.OverrideVertexColors->VertexColor(0), rhs.OverrideVertexColors->GetNumVertices());
	}
}

/** Destructor */
FStaticMeshComponentLODInfo::~FStaticMeshComponentLODInfo()
{
	// Note: OverrideVertexColors had BeginReleaseResource called in UStaticMeshComponent::BeginDestroy, 
	// And waits on a fence for that command to complete in UStaticMeshComponent::IsReadyForFinishDestroy,
	// So we know it is safe to delete OverrideVertexColors here (RT can't be referencing it anymore)
	CleanUp();
}

void FStaticMeshComponentLODInfo::CleanUp()
{
	if(OverrideVertexColors)
	{
		DEC_DWORD_STAT_BY( STAT_InstVertexColorMemory, OverrideVertexColors->GetAllocatedSize() );
	}

	delete OverrideVertexColors;
	OverrideVertexColors = NULL;

	PaintedVertices.Empty();
}


void FStaticMeshComponentLODInfo::BeginReleaseOverrideVertexColors()
{
	if(OverrideVertexColors)
	{
		// enqueue a rendering command to release
		BeginReleaseResource(OverrideVertexColors);
	}
}

void FStaticMeshComponentLODInfo::ReleaseOverrideVertexColorsAndBlock()
{
	if(OverrideVertexColors)
	{
		// enqueue a rendering command to release
		BeginReleaseResource(OverrideVertexColors);
		// Ensure the RT no longer accessed the data, might slow down
		FlushRenderingCommands();
		// The RT thread has no access to it any more so it's safe to delete it.
		CleanUp();
	}
}

void FStaticMeshComponentLODInfo::ExportText(FString& ValueStr)
{
	ValueStr += FString::Printf(TEXT("PaintedVertices(%d)="), PaintedVertices.Num());

	// Rough approximation
	ValueStr.Reserve(ValueStr.Len() + PaintedVertices.Num() * 125);

	// Export the Position, Normal and Color info for each vertex
	for(int32 i = 0; i < PaintedVertices.Num(); ++i)
	{
		FPaintedVertex& Vert = PaintedVertices[i];

		ValueStr += FString::Printf(TEXT("((Position=(X=%.6f,Y=%.6f,Z=%.6f),"), Vert.Position.X, Vert.Position.Y, Vert.Position.Z);
		ValueStr += FString::Printf(TEXT("(Normal=(X=%d,Y=%d,Z=%d,W=%d),"), Vert.Normal.Vector.X, Vert.Normal.Vector.Y, Vert.Normal.Vector.Z, Vert.Normal.Vector.W);
		ValueStr += FString::Printf(TEXT("(Color=(B=%d,G=%d,R=%d,A=%d))"), Vert.Color.B, Vert.Color.G, Vert.Color.R, Vert.Color.A);

		// Seperate each vertex entry with a comma
		if ((i + 1) != PaintedVertices.Num())
		{
			ValueStr += TEXT(",");
		}
	}

	ValueStr += TEXT(" ");
}

void FStaticMeshComponentLODInfo::ImportText(const TCHAR** SourceText)
{
	FString TmpStr;
	int32 VertCount;
	if (FParse::Value(*SourceText, TEXT("PaintedVertices("), VertCount))
	{
		TmpStr = FString::Printf(TEXT("%d"), VertCount);
		*SourceText += TmpStr.Len() + 18;

		FString SourceTextStr = *SourceText;
		TArray<FString> Tokens;
		int32 TokenIdx = 0;
		bool bValidInput = true;

		// Tokenize the text
		SourceTextStr.ParseIntoArray(Tokens, TEXT(","), false);

		// There should be 11 tokens per vertex
		check(Tokens.Num() * 11 >= VertCount);

		PaintedVertices.AddUninitialized(VertCount);

		for (int32 Idx = 0; Idx < VertCount; ++Idx)
		{
			// Position
			bValidInput &= FParse::Value(*Tokens[TokenIdx++], TEXT("X="), PaintedVertices[Idx].Position.X);
			bValidInput &= FParse::Value(*Tokens[TokenIdx++], TEXT("Y="), PaintedVertices[Idx].Position.Y);
			bValidInput &= FParse::Value(*Tokens[TokenIdx++], TEXT("Z="), PaintedVertices[Idx].Position.Z);
			// Normal
			bValidInput &= FParse::Value(*Tokens[TokenIdx++], TEXT("X="), PaintedVertices[Idx].Normal.Vector.X);
			bValidInput &= FParse::Value(*Tokens[TokenIdx++], TEXT("Y="), PaintedVertices[Idx].Normal.Vector.Y);
			bValidInput &= FParse::Value(*Tokens[TokenIdx++], TEXT("Z="), PaintedVertices[Idx].Normal.Vector.Z);
			bValidInput &= FParse::Value(*Tokens[TokenIdx++], TEXT("W="), PaintedVertices[Idx].Normal.Vector.W);
			// Color
			bValidInput &= FParse::Value(*Tokens[TokenIdx++], TEXT("B="), PaintedVertices[Idx].Color.B);
			bValidInput &= FParse::Value(*Tokens[TokenIdx++], TEXT("G="), PaintedVertices[Idx].Color.G);
			bValidInput &= FParse::Value(*Tokens[TokenIdx++], TEXT("R="), PaintedVertices[Idx].Color.R);
			bValidInput &= FParse::Value(*Tokens[TokenIdx++], TEXT("A="), PaintedVertices[Idx].Color.A);

			// Verify that the info for this vertex was read correctly
			check(bValidInput);
		}

		// Advance the text pointer past all of the data we just read
		int32 LODDataStrLen = 0;
		for (int32 Idx = 0; Idx < TokenIdx - 1; ++Idx)
		{
			LODDataStrLen += Tokens[Idx].Len() + 1;
		}
		*SourceText += LODDataStrLen;
	}
}

int32 GKeepKeepOverrideVertexColorsOnCPU = 1;
FAutoConsoleVariableRef CKeepOverrideVertexColorsOnCPU(
	TEXT("r.KeepOverrideVertexColorsOnCPU"),
	GKeepKeepOverrideVertexColorsOnCPU,
	TEXT("Keeps a CPU copy of override vertex colors.  May be required for some blueprints / object spawning."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

FArchive& operator<<(FArchive& Ar,FStaticMeshComponentLODInfo& I)
{
	const uint8 OverrideColorsStripFlag = 1;
	bool bStrippedOverrideColors = false;
#if WITH_EDITORONLY_DATA
	if( Ar.IsCooking() )
	{
		// Check if override color should be stripped too	
		int32 LODIndex = 0;
		for( ; LODIndex < I.Owner->LODData.Num() && &I != &I.Owner->LODData[ LODIndex ]; LODIndex++ )
		{}
		check( LODIndex < I.Owner->LODData.Num() );

		bStrippedOverrideColors = !I.OverrideVertexColors || 
			( I.Owner->StaticMesh == NULL || 
			I.Owner->StaticMesh->RenderData == NULL ||
			LODIndex >= I.Owner->StaticMesh->RenderData->LODResources.Num() ||
			I.OverrideVertexColors->GetNumVertices() != I.Owner->StaticMesh->RenderData->LODResources[LODIndex].VertexBuffer.GetNumVertices() );
	}
#endif // WITH_EDITORONLY_DATA
	FStripDataFlags StripFlags( Ar, bStrippedOverrideColors ? OverrideColorsStripFlag : 0 );

	if( !StripFlags.IsDataStrippedForServer() )
	{
		Ar << I.LightMap;
		Ar << I.ShadowMap;
	}

	if( !StripFlags.IsClassDataStripped( OverrideColorsStripFlag ) )
	{
		// Bulk serialization (new method)
		uint8 bLoadVertexColorData = (I.OverrideVertexColors != NULL);
		Ar << bLoadVertexColorData;

		if(bLoadVertexColorData)
		{
			if(Ar.IsLoading())
			{
				check(!I.OverrideVertexColors);
				I.OverrideVertexColors = new FColorVertexBuffer;
			}

			//we want to discard the vertex colors after rhi init when in cooked/client builds.
			const bool bNeedsCPUAccess = !Ar.IsLoading() || GIsEditor || IsRunningCommandlet() || (GKeepKeepOverrideVertexColorsOnCPU != 0);
			I.OverrideVertexColors->Serialize(Ar, bNeedsCPUAccess);
		}
	}

	// Serialize out cached vertex information if necessary.
	if ( !StripFlags.IsEditorDataStripped() )
	{
		Ar << I.PaintedVertices;
	}

	return Ar;
}

#undef LOCTEXT_NAMESPACE
