// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


/* Private dependencies
 *****************************************************************************/

#include "Core.h"
#include "CoreUObject.h"
#include "LevelSequence.h"
#include "AssetToolsModule.h"
#include "EditorStyle.h"
#include "Engine.h"


/* Private includes
 *****************************************************************************/

#include "Matinee/InterpData.h"
#include "Matinee/InterpTrackFloatBase.h"
#include "Matinee/InterpTrackBoolProp.h"
#include "Matinee/InterpTrackLinearColorProp.h"
#include "Matinee/InterpTrackColorScale.h"
#include "Matinee/InterpTrackColorProp.h"
#include "Matinee/InterpTrackFloatProp.h"
#include "Matinee/InterpTrackMove.h"
#include "Matinee/InterpTrackAnimControl.h"
#include "Matinee/InterpTrackSound.h"
#include "Matinee/InterpTrackFade.h"
#include "Matinee/InterpTrackSlomo.h"
#include "Matinee/InterpTrackDirector.h"
#include "Matinee/InterpTrackEvent.h"
#include "Matinee/InterpTrackAudioMaster.h"
#include "Matinee/InterpTrackVisibility.h"
#include "Matinee/InterpGroup.h"
#include "Matinee/InterpGroupInst.h"
#include "Matinee/InterpGroupDirector.h"
#include "Matinee/MatineeActor.h"
#include "Matinee/MatineeActorCameraAnim.h"
#include "Matinee/InterpTrackMoveAxis.h"
#include "MatineeUtils.h"

#include "LevelEditor.h"
#include "LevelSequenceActor.h"

#include "MatineeImportTools.h"
#include "MovieScene.h"
#include "MovieSceneFolder.h"
#include "MovieSceneBoolTrack.h"
#include "MovieSceneColorTrack.h"
#include "MovieSceneFloatTrack.h"
#include "MovieScene3DTransformTrack.h"
#include "MovieSceneParticleTrack.h"
#include "MovieSceneSkeletalAnimationTrack.h"
#include "MovieSceneAudioTrack.h"
#include "MovieSceneFadeTrack.h"
#include "MovieSceneSlomoTrack.h"
#include "MovieSceneCameraCutTrack.h"
#include "MovieSceneEventTrack.h"
#include "MovieSceneAudioTrack.h"
#include "MovieSceneVisibilityTrack.h"

#include "AssetToolsModule.h"
#include "AssetRegistryModule.h"
#include "Camera/CameraActor.h"
#include "Animation/SkeletalMeshActor.h"
#include "Particles/ParticleSystemComponent.h"
#include "Components/LightComponent.h"

#include "UnrealEd.h"


DECLARE_LOG_CATEGORY_EXTERN(LogMatineeToLevelSequence, Verbose, All);
