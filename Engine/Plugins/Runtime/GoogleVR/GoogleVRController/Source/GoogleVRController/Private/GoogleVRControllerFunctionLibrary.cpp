/* Copyright 2016 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Classes/GoogleVRControllerFunctionLibrary.h"
#include "GoogleVRController.h"
#include "GoogleVRControllerPrivate.h"
#include "InputCoreTypes.h"

UGoogleVRControllerEventManager* UGoogleVRControllerFunctionLibrary::ControllerEventManager = nullptr;

UGoogleVRControllerFunctionLibrary::UGoogleVRControllerFunctionLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (ControllerEventManager == nullptr)
	{
		ControllerEventManager = NewObject<UGoogleVRControllerEventManager>();
	}
}

FGoogleVRController* GetGoogleVRController()
{
	FGoogleVRController* GVRController;
	TArray<IMotionController*> MotionControllers = IModularFeatures::Get().GetModularFeatureImplementations<IMotionController>( IMotionController::GetModularFeatureName() );
	for( auto MotionController : MotionControllers )
	{
		if (MotionController != nullptr)
		{
			GVRController = static_cast<FGoogleVRController*>(MotionController);
			if(GVRController != nullptr)
				return GVRController;
		}
	}

	return nullptr;
}

EGoogleVRControllerHandedness UGoogleVRControllerFunctionLibrary::GetGoogleVRControllerHandedness()
{
	EGoogleVRControllerHandedness HandednessPrefsEnum = EGoogleVRControllerHandedness::Unknown;
	FGoogleVRController* GVRController = GetGoogleVRController();
	if(GVRController != nullptr)
	{
		int HandednessPrefsInt = GVRController->GetGVRControllerHandedness();
		if (HandednessPrefsInt == 0)
		{
			HandednessPrefsEnum = EGoogleVRControllerHandedness::RightHanded;
		}
		else if (HandednessPrefsInt == 1)
		{
			HandednessPrefsEnum = EGoogleVRControllerHandedness::LeftHanded;
		}
	}
	return HandednessPrefsEnum;
}

FVector UGoogleVRControllerFunctionLibrary::GetGoogleVRControllerRawAccel()
{
	FGoogleVRController* GVRController = GetGoogleVRController();
	if(GVRController != nullptr)
	{
#if GOOGLEVRCONTROLLER_SUPPORTED_PLATFORMS
		gvr_vec3f ControllerAccel = GVRController->CachedControllerState.GetAccel();
		return FVector(ControllerAccel.x, ControllerAccel.y, ControllerAccel.z);
#endif
	}
	return FVector::ZeroVector;
}

FVector UGoogleVRControllerFunctionLibrary::GetGoogleVRControllerRawGyro()
{
	FGoogleVRController* GVRController = GetGoogleVRController();
	if(GVRController != nullptr)
	{
#if GOOGLEVRCONTROLLER_SUPPORTED_PLATFORMS
		gvr_vec3f ControllerGyro = GVRController->CachedControllerState.GetGyro();
		return FVector(ControllerGyro.x, ControllerGyro.y, ControllerGyro.z);
#endif
	}
	return FVector::ZeroVector;
}

FRotator UGoogleVRControllerFunctionLibrary::GetGoogleVRControllerOrientation()
{
	FGoogleVRController* GVRController = GetGoogleVRController();
	if(GVRController != nullptr)
	{
		FRotator orientation;
		FVector position;
		GVRController->GetControllerOrientationAndPosition(0, EControllerHand::Right, orientation, position);
		return orientation;
	}
	return FRotator::ZeroRotator;
}


UGoogleVRControllerEventManager* UGoogleVRControllerFunctionLibrary::GetGoogleVRControllerEventManager()
{
	return ControllerEventManager;
}