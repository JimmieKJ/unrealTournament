// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PersonaPrivatePCH.h"

#include "AnimationEditorPreviewScene.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SphereReflectionCaptureComponent.h"

/////////////////////////////////////////////////////////////////////////
// FAnimationEditorPreviewScene

FAnimationEditorPreviewScene::FAnimationEditorPreviewScene(ConstructionValues CVS) : FPreviewScene(CVS)
{
	// set light options 
	DirectionalLight->SetRelativeLocation(FVector(-1024.f, 1024.f, 2048.f));
	DirectionalLight->SetRelativeScale3D(FVector(15.f));
	DirectionalLight->Mobility = EComponentMobility::Movable;
	DirectionalLight->DynamicShadowDistanceStationaryLight = 3000.f;
	DirectionalLight->bPrecomputedLightingIsValid = false;
	SetLightBrightness(4.f);
	//InSetSkyBrightness(0.5f);
	DirectionalLight->InvalidateLightingCache();
	DirectionalLight->RecreateRenderState_Concurrent();

	// A background sky sphere
	EditorSkyComp = NewObject<UStaticMeshComponent>(GetTransientPackage());
	UStaticMesh * StaticMesh = LoadObject<UStaticMesh>(NULL, TEXT("/Engine/MapTemplates/Sky/SM_SkySphere.SM_SkySphere"), NULL, LOAD_None, NULL);
	check (StaticMesh);
	EditorSkyComp->SetStaticMesh( StaticMesh );
	UMaterial* SkyMaterial= LoadObject<UMaterial>(NULL, TEXT("/Engine/EditorMaterials/PersonaSky.PersonaSky"), NULL, LOAD_None, NULL);
	check (SkyMaterial);
	EditorSkyComp->SetMaterial(0, SkyMaterial);
	const float SkySphereScale = 1000.f;
	const FTransform SkyTransform(FRotator(0,0,0), FVector(0,0,0), FVector(SkySphereScale));
	AddComponent(EditorSkyComp, SkyTransform);

	// now add height fog component

	EditorHeightFogComponent = NewObject<UExponentialHeightFogComponent>(GetTransientPackage());
	
	EditorHeightFogComponent->FogDensity=0.00075f;
	EditorHeightFogComponent->FogInscatteringColor=FLinearColor(3.f,4.f,6.f,0.f)*0.3f;
	EditorHeightFogComponent->DirectionalInscatteringExponent=16.f;
	EditorHeightFogComponent->DirectionalInscatteringColor=FLinearColor(1.1f,0.9f,0.538427f,0.f);
	EditorHeightFogComponent->FogHeightFalloff=0.01f;
	EditorHeightFogComponent->StartDistance=15000.f;
	const FTransform FogTransform(FRotator(0,0,0), FVector(3824.f,34248.f,50000.f), FVector(80.f));
	AddComponent(EditorHeightFogComponent, FogTransform);

	// add capture component for reflection
	USphereReflectionCaptureComponent* CaptureComponent = NewObject<USphereReflectionCaptureComponent>(GetTransientPackage());
 
 	const FTransform CaptureTransform(FRotator(0,0,0), FVector(0.f,0.f,100.f), FVector(1.f));
 	AddComponent(CaptureComponent, CaptureTransform);
	CaptureComponent->SetCaptureIsDirty();
	CaptureComponent->UpdateReflectionCaptureContents(GetWorld());

	// now add floor
 	UStaticMesh* FloorMesh = LoadObject<UStaticMesh>(NULL, TEXT("/Engine/EditorMeshes/PhAT_FloorBox.PhAT_FloorBox"), NULL, LOAD_None, NULL);
 	check(FloorMesh);
	EditorFloorComp = NewObject<UStaticMeshComponent>(GetTransientPackage());
 	EditorFloorComp->SetStaticMesh( FloorMesh );
 	AddComponent(EditorFloorComp, FTransform::Identity);
 	EditorFloorComp->SetRelativeScale3D(FVector(3.f, 3.f, 1.f));
	UMaterial* Material= LoadObject<UMaterial>(NULL, TEXT("/Engine/EditorMaterials/PersonaFloorMat.PersonaFloorMat"), NULL, LOAD_None, NULL);
	check(Material);
	EditorFloorComp->SetMaterial( 0, Material );
}

