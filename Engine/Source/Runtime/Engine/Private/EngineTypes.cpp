// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

FAttachmentTransformRules FAttachmentTransformRules::KeepRelativeTransform(EAttachmentRule::KeepRelative, false);
FAttachmentTransformRules FAttachmentTransformRules::KeepWorldTransform(EAttachmentRule::KeepWorld, false);
FAttachmentTransformRules FAttachmentTransformRules::SnapToTargetNotIncludingScale(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, false);
FAttachmentTransformRules FAttachmentTransformRules::SnapToTargetIncludingScale(EAttachmentRule::SnapToTarget, false);

FDetachmentTransformRules FDetachmentTransformRules::KeepRelativeTransform(EDetachmentRule::KeepRelative, true);
FDetachmentTransformRules FDetachmentTransformRules::KeepWorldTransform(EDetachmentRule::KeepWorld, true);

void FMeshProxySettings::PostLoadDeprecated()
{
	FMeshProxySettings DefaultObject;

	if (TextureWidth_DEPRECATED != DefaultObject.TextureWidth_DEPRECATED)
	{
		MaterialSettings.TextureSize.X = TextureWidth_DEPRECATED;
	}
	if (TextureHeight_DEPRECATED != DefaultObject.TextureHeight_DEPRECATED)
	{
		MaterialSettings.TextureSize.Y = TextureHeight_DEPRECATED;
	}
	if (bExportNormalMap_DEPRECATED != DefaultObject.bExportNormalMap_DEPRECATED)
	{
		MaterialSettings.bNormalMap = bExportNormalMap_DEPRECATED;
	}
	if (bExportMetallicMap_DEPRECATED != DefaultObject.bExportMetallicMap_DEPRECATED)
	{
		MaterialSettings.bMetallicMap = bExportMetallicMap_DEPRECATED;
	}
	if (bExportRoughnessMap_DEPRECATED != DefaultObject.bExportRoughnessMap_DEPRECATED)
	{
		MaterialSettings.bRoughnessMap = bExportRoughnessMap_DEPRECATED;
	}
	if (bExportSpecularMap_DEPRECATED != DefaultObject.bExportSpecularMap_DEPRECATED)
	{
		MaterialSettings.bSpecularMap = bExportSpecularMap_DEPRECATED;
	}

	if (!(Material_DEPRECATED == DefaultObject.Material_DEPRECATED))
	{
		MaterialSettings.TextureSize = Material_DEPRECATED.BaseColorMapSize;
		MaterialSettings.bNormalMap = Material_DEPRECATED.bNormalMap;
		MaterialSettings.bMetallicMap = Material_DEPRECATED.bMetallicMap;
		MaterialSettings.bRoughnessMap = Material_DEPRECATED.bRoughnessMap;
		MaterialSettings.bSpecularMap = Material_DEPRECATED.bSpecularMap;
		MaterialSettings.RoughnessConstant = Material_DEPRECATED.RoughnessConstant;
		MaterialSettings.MetallicConstant = Material_DEPRECATED.MetallicConstant;
		MaterialSettings.SpecularConstant = Material_DEPRECATED.SpecularConstant;
	}

	MaterialSettings.MaterialMergeType = EMaterialMergeType::MaterialMergeType_Simplygon;
}


void FMeshMergingSettings::PostLoadDeprecated()
{
	FMeshMergingSettings DefaultObject;
	if (bImportVertexColors_DEPRECATED != DefaultObject.bImportVertexColors_DEPRECATED)
	{
		bBakeVertexDataToMesh = bImportVertexColors_DEPRECATED;
	}

	if (bExportNormalMap_DEPRECATED != DefaultObject.bExportNormalMap_DEPRECATED)
	{
		MaterialSettings.bNormalMap = bExportNormalMap_DEPRECATED;
	}

	if (bExportMetallicMap_DEPRECATED != DefaultObject.bExportMetallicMap_DEPRECATED)
	{
		MaterialSettings.bMetallicMap = bExportMetallicMap_DEPRECATED;
	}
	if (bExportRoughnessMap_DEPRECATED != DefaultObject.bExportRoughnessMap_DEPRECATED)
	{
		MaterialSettings.bRoughnessMap = bExportRoughnessMap_DEPRECATED;
	}
	if (bExportSpecularMap_DEPRECATED != DefaultObject.bExportSpecularMap_DEPRECATED)
	{
		MaterialSettings.bSpecularMap = bExportSpecularMap_DEPRECATED;
	}
	if (MergedMaterialAtlasResolution_DEPRECATED != DefaultObject.MergedMaterialAtlasResolution_DEPRECATED)
	{
		MaterialSettings.TextureSize.X = MergedMaterialAtlasResolution_DEPRECATED;
		MaterialSettings.TextureSize.Y = MergedMaterialAtlasResolution_DEPRECATED;
	}
	if (bCalculateCorrectLODModel_DEPRECATED != DefaultObject.bCalculateCorrectLODModel_DEPRECATED)
	{
		LODSelectionType = EMeshLODSelectionType::CalculateLOD;
	}

	if (ExportSpecificLOD_DEPRECATED != DefaultObject.ExportSpecificLOD_DEPRECATED)
	{
		SpecificLOD = ExportSpecificLOD_DEPRECATED;
		LODSelectionType = EMeshLODSelectionType::SpecificLOD;
	}
}

UEngineBaseTypes::UEngineBaseTypes(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UEngineTypes::UEngineTypes(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

ECollisionChannel UEngineTypes::ConvertToCollisionChannel(ETraceTypeQuery TraceType)
{
	return UCollisionProfile::Get()->ConvertToCollisionChannel(true, (int32)TraceType);
}

ECollisionChannel UEngineTypes::ConvertToCollisionChannel(EObjectTypeQuery ObjectType)
{
	return UCollisionProfile::Get()->ConvertToCollisionChannel(false, (int32)ObjectType);
}

EObjectTypeQuery UEngineTypes::ConvertToObjectType(ECollisionChannel CollisionChannel)
{
	return UCollisionProfile::Get()->ConvertToObjectType(CollisionChannel);
}

ETraceTypeQuery UEngineTypes::ConvertToTraceType(ECollisionChannel CollisionChannel)
{
	return UCollisionProfile::Get()->ConvertToTraceType(CollisionChannel);
}

void FDamageEvent::GetBestHitInfo(AActor const* HitActor, AActor const* HitInstigator, FHitResult& OutHitInfo, FVector& OutImpulseDir) const
{
	ensure(HitActor);
	if (HitActor)
	{
		// fill out the hitinfo as best we can
		OutHitInfo.Actor = const_cast<AActor*>(HitActor);
		OutHitInfo.bBlockingHit = true;
		OutHitInfo.BoneName = NAME_None;
		OutHitInfo.Component = Cast<UPrimitiveComponent>(HitActor->GetRootComponent());
		
		// assume the actor got hit in the center of his root component
		OutHitInfo.ImpactPoint = HitActor->GetActorLocation();
		OutHitInfo.Location = OutHitInfo.ImpactPoint;
		
		// assume hit came from instigator's location
		OutImpulseDir = HitInstigator ? 
			( OutHitInfo.ImpactPoint - HitInstigator->GetActorLocation() ).GetSafeNormal()
			: FVector::ZeroVector;

		// assume normal points back toward instigator
		OutHitInfo.ImpactNormal = -OutImpulseDir;
		OutHitInfo.Normal = OutHitInfo.ImpactNormal;
	}
}

void FPointDamageEvent::GetBestHitInfo(AActor const* HitActor, AActor const* HitInstigator, FHitResult& OutHitInfo, FVector& OutImpulseDir) const
{
	// assume the actor got hit in the center of his root component
	OutHitInfo = HitInfo;
	OutImpulseDir = ShotDirection;
}


void FRadialDamageEvent::GetBestHitInfo(AActor const* HitActor, AActor const* HitInstigator, FHitResult& OutHitInfo, FVector& OutImpulseDir) const
{
	ensure(ComponentHits.Num() > 0);

	// for now, just return the first one
	OutHitInfo = ComponentHits[0];
	OutImpulseDir = (OutHitInfo.ImpactPoint - Origin).GetSafeNormal();
}


float FRadialDamageParams::GetDamageScale(float DistanceFromEpicenter) const
{
	float const ValidatedInnerRadius = FMath::Max(0.f, InnerRadius);
	float const ValidatedOuterRadius = FMath::Max(OuterRadius, ValidatedInnerRadius);
	float const ValidatedDist = FMath::Max(0.f, DistanceFromEpicenter);

	if (ValidatedDist >= ValidatedOuterRadius)
	{
		// outside the radius, no effect
		return 0.f;
	}

	if ( (DamageFalloff == 0.f)	|| (ValidatedDist <= ValidatedInnerRadius) )
	{
		// no falloff or inside inner radius means full effect
		return 1.f;
	}

	// calculate the interpolated scale
	float DamageScale = 1.f - ( (ValidatedDist - ValidatedInnerRadius) / (ValidatedOuterRadius - ValidatedInnerRadius) );
	DamageScale = FMath::Pow(DamageScale, DamageFalloff);

	return DamageScale;
}

FLightmassDebugOptions::FLightmassDebugOptions()
	: bDebugMode(false)
	, bStatsEnabled(false)
	, bGatherBSPSurfacesAcrossComponents(true)
	, CoplanarTolerance(0.001f)
	, bUseImmediateImport(true)
	, bImmediateProcessMappings(true)
	, bSortMappings(true)
	, bDumpBinaryFiles(false)
	, bDebugMaterials(false)
	, bPadMappings(true)
	, bDebugPaddings(false)
	, bOnlyCalcDebugTexelMappings(false)
	, bUseRandomColors(false)
	, bColorBordersGreen(false)
	, bColorByExecutionTime(false)
	, ExecutionTimeDivisor(15.0f)
{}

USceneComponent* FComponentReference::GetComponent(AActor* OwningActor) const
{
	USceneComponent* Result = NULL;
	// Component is specified directly, use that
	if(OverrideComponent.IsValid())
	{
		Result = OverrideComponent.Get();
	}
	else
	{
		// Look in Actor if specified, OwningActor if not
		AActor* SearchActor = (OtherActor != NULL) ? OtherActor : OwningActor;
		if(SearchActor)
		{
			if(ComponentProperty != NAME_None)
			{
				UObjectPropertyBase* ObjProp = FindField<UObjectPropertyBase>(SearchActor->GetClass(), ComponentProperty);
				if(ObjProp != NULL)
				{
					// .. and return the component that is there
					Result = Cast<USceneComponent>(ObjProp->GetObjectPropertyValue_InContainer(SearchActor));
				}
			}
			else
			{
				Result = Cast<USceneComponent>(SearchActor->GetRootComponent());
			}
		}
	}

	return Result;
}

FString FHitResult::ToString() const
{
	return FString::Printf(TEXT("bBlockingHit:%s bStartPenetrating:%s Time:%f Location:%s ImpactPoint:%s Normal:%s ImpactNormal:%s TraceStart:%s TraceEnd:%s PenetrationDepth:%f Item:%d PhysMaterial:%s Actor:%s Component:%s BoneName:%s FaceIndex:%d"),
		bBlockingHit == true ? TEXT("True") : TEXT("False"),
		bStartPenetrating == true ? TEXT("True") : TEXT("False"),
		Time,
		*Location.ToString(),
		*ImpactPoint.ToString(),
		*Normal.ToString(),
		*ImpactNormal.ToString(),
		*TraceStart.ToString(),
		*TraceEnd.ToString(),
		PenetrationDepth,
		Item,
		PhysMaterial.IsValid() ? *PhysMaterial->GetName() : TEXT("None"),
		Actor.IsValid() ? *Actor->GetName() : TEXT("None"),
		Component.IsValid() ? *Component->GetName() : TEXT("None"),
		BoneName.IsValid() ? *BoneName.ToString() : TEXT("None"),
		FaceIndex);
}

FRepMovement::FRepMovement()
	: LinearVelocity(ForceInit)
	, AngularVelocity(ForceInit)
	, Location(ForceInit)
	, Rotation(ForceInit)
	, bSimulatedPhysicSleep(false)
	, bRepPhysics(false)
	, LocationQuantizationLevel(EVectorQuantization::RoundWholeNumber)
	, VelocityQuantizationLevel(EVectorQuantization::RoundWholeNumber)
	, RotationQuantizationLevel(ERotatorQuantization::ByteComponents)
{
}
