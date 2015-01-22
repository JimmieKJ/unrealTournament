// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "ParticleDefinitions.h"

#include "Net/UnrealNetwork.h"
#include "Camera/CameraModifier.h"

//////////////////////////////////////////////////////////////////////////

DEFINE_LOG_CATEGORY_STATIC(LogCamera, Log, All);

//////////////////////////////////////////////////////////////////////////
// UCameraModifier

UCameraModifier::UCameraModifier(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Priority = 127;
}

bool UCameraModifier::ModifyCamera(class APlayerCameraManager* Camera, float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	// If pending disable and fully alpha'd out, truly disable this modifier
	if (bPendingDisable && (Alpha <= 0.0f))
	{
		DisableModifier(true);
	}

	return false;
}

float UCameraModifier::GetTargetAlpha(class APlayerCameraManager* Camera)
{
	if( bPendingDisable )
	{
		return 0.0f;
	}
	return 1.0f;		
}

void UCameraModifier::UpdateAlpha(class APlayerCameraManager* Camera, float DeltaTime)
{
	float Time;

	TargetAlpha = GetTargetAlpha( Camera );

	// Alpha out
	if( TargetAlpha == 0.f )
	{
		Time = AlphaOutTime;
	}
	else
	{
		// Otherwise, alpha in
		Time = AlphaInTime;
	}

	// interpolate!
	if( Time <= 0.f )
	{
		Alpha = TargetAlpha;
	}
	else if( Alpha > TargetAlpha )
	{
		Alpha = FMath::Max<float>( Alpha - DeltaTime/Time, TargetAlpha );
	}
	else
	{
		Alpha = FMath::Min<float>( Alpha + DeltaTime/Time, TargetAlpha );
	}
}


bool UCameraModifier::IsDisabled() const
{
	return bDisabled;
}

void UCameraModifier::Init( APlayerCameraManager* Camera ) 
{
	AddCameraModifier( Camera );
}

UWorld* UCameraModifier::GetWorld() const
{
	UWorld* World = NULL;
	if( CameraOwner )
	{
		World = CameraOwner->GetWorld();
	}
	return World;
}

bool UCameraModifier::RemoveCameraModifier( APlayerCameraManager* Camera )
{
	// Loop through each modifier in camera
	for( int32 ModifierIdx = 0; ModifierIdx < Camera->ModifierList.Num(); ModifierIdx++ )
	{
		// If we found ourselves, remove ourselves from the list and return
		if( Camera->ModifierList[ModifierIdx] == this )
		{
			Camera->ModifierList.RemoveAt(ModifierIdx, 1);
			return true;
		}
	}

	// Didn't find ourselves in the list
	return false;
}


void UCameraModifier::DisableModifier(bool bImmediate)
{
	if (bImmediate)
	{
		bDisabled = true;
		bPendingDisable = false;
	}
	else if (!bDisabled)
	{
		bPendingDisable = true;
	}

}

void UCameraModifier::EnableModifier()
{
	bDisabled = false;
	bPendingDisable = false;
}

void UCameraModifier::ToggleModifier()
{
	if( bDisabled )
	{
		EnableModifier();
	}
	else
	{
		DisableModifier();
	}
}

bool UCameraModifier::ProcessViewRotation( AActor* ViewTarget, float DeltaTime, FRotator& OutViewRotation, FRotator& OutDeltaRot)
{
	return false;
}

bool UCameraModifier::AddCameraModifier( APlayerCameraManager* Camera )
{
	// Make sure we don't already have this modifier in the list
	for( int32 ModifierIdx = 0; ModifierIdx < Camera->ModifierList.Num(); ModifierIdx++ )
	{
		if ( Camera->ModifierList[ModifierIdx] == this )
		{
			return false;
		}
	}

	// Make sure we don't already have a modifier of this type
	for( int32 ModifierIdx = 0; ModifierIdx < Camera->ModifierList.Num(); ModifierIdx++ )
	{
		if ( Camera->ModifierList[ModifierIdx]->GetClass() == GetClass() )
		{
			UE_LOG(LogCamera, Log, TEXT("AddCameraModifier found existing Modifier in list, replacing with new one %s"), *GetName());

			// hack replace old by new (delete??)
			Camera->ModifierList[ModifierIdx] = this;

			// Save camera
			CameraOwner = Camera;

			return true;
		}
	}

	// Look through current modifier list and find slot for this priority
	int32 BestIdx = 0;

	for( int32 ModifierIdx = 0; ModifierIdx < Camera->ModifierList.Num(); ModifierIdx++ )
	{
		UCameraModifier* Modifier = Camera->ModifierList[ModifierIdx];
		if( Modifier == NULL ) {
			continue;
		}

		// If priority of current index has passed or equaled ours - we have the insert location
		if( Priority <= Modifier->Priority )
		{
			// Disallow addition of exclusive modifier if priority is already occupied
			if( bExclusive && Priority == Modifier->Priority )
			{
				return false;
			}

			break;
		}

		// Update best index
		BestIdx++;
	}

	// Insert self into best index
	Camera->ModifierList.InsertUninitialized( BestIdx, 1 );
	Camera->ModifierList[BestIdx] = this;

	// Save camera
	CameraOwner = Camera;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	//debug
	if( bDebug )
	{
		UE_LOG(LogCamera, Log,  TEXT("AddModifier %i %s"), BestIdx, *GetName() );
		for( int32 ModifierIdx = 0; ModifierIdx < Camera->ModifierList.Num(); ModifierIdx++ )
		{
			UE_LOG(LogCamera, Log,  TEXT("%s Idx %i Pri %i"), *Camera->ModifierList[ModifierIdx]->GetName(), ModifierIdx, Camera->ModifierList[ModifierIdx]->Priority);
		}
		UE_LOG(LogCamera, Log,  TEXT("****************") );
	}
#endif

	return true;
}
