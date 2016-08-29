// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneCommonHelpers.h"
#include "MovieSceneCameraShakeTrackInstance.h"
#include "MovieSceneCameraShakeTrack.h"
#include "MovieSceneCameraShakeSection.h"
#include "IMovieScenePlayer.h"


FMovieSceneCameraShakeTrackInstance::FMovieSceneCameraShakeTrackInstance(UMovieSceneCameraShakeTrack& InCameraShakeTrack)
{
	CameraShakeTrack = &InCameraShakeTrack;
}

FMovieSceneCameraShakeTrackInstance::~FMovieSceneCameraShakeTrackInstance()
{
	ACameraActor* const CamActor = TempCameraActor.Get();
	if (CamActor)
	{
		CamActor->Destroy();
	}
}

void FMovieSceneCameraShakeTrackInstance::Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	if (UpdateData.UpdatePass == MSUP_PreUpdate)
	{
		// pre-update pass, clear out the additive offsets (will reapply as necessary in MSUP_Update)
		for (TWeakObjectPtr<UObject> ObjectWP : RuntimeObjects)
		{
			UCameraComponent* const CameraComponent = MovieSceneHelpers::CameraComponentFromRuntimeObject(ObjectWP.Get());
			if (CameraComponent)
			{
				CameraComponent->ClearAdditiveOffset();
				CameraComponent->ClearExtraPostProcessBlends();
			}
		}
	}
	else
	{
		for (TWeakObjectPtr<UObject> ObjectWP : RuntimeObjects)
		{
			UCameraComponent* const CameraComponent = MovieSceneHelpers::CameraComponentFromRuntimeObject(ObjectWP.Get());
			if (CameraComponent)
			{
				// clear it for now, will reset it with correct values later
				TArray<UMovieSceneCameraShakeSection*> ActiveSections;
				CameraShakeTrack->GetCameraShakeSectionsAtTime(UpdateData.Position, ActiveSections);

				// look for section instances that are no longer playing
				TArray<UMovieSceneCameraShakeSection*> DeadSections;
				for (const auto& Iter : SectionInstanceDataMap)
				{
					if (ActiveSections.Contains(Iter.Key) == false)
					{
						// section isn't active anymore, stop playing the anim
						FMovieSceneCameraShakeSectionInstanceData const& SectionInstanceData = Iter.Value;

						if (SectionInstanceData.CameraShakeInst.IsValid())
						{
							SectionInstanceData.CameraShakeInst->StopShake(true);
							SectionInstanceData.CameraShakeInst->RemoveFromRoot();
						}

						DeadSections.Add(Iter.Key);
					}
				}
				for (auto DeadSection : DeadSections)
				{
					SectionInstanceDataMap.Remove(DeadSection);
				}

				// sort by start time to match application order of player camera
				ActiveSections.Sort(
					[](const UMovieSceneCameraShakeSection& One, const UMovieSceneCameraShakeSection& Two)
				{
					return One.GetStartTime() < Two.GetStartTime();
				}
				);

				// look for sections that just started and need instances
				for (auto ActiveSection : ActiveSections)
				{
					FMovieSceneCameraShakeSectionInstanceData& SectionInstanceData = SectionInstanceDataMap.FindOrAdd(ActiveSection);

					// see if we need to init and start playing the shake
					if (SectionInstanceData.CameraShakeInst == nullptr)
					{
						UCameraShake* const ShakeInst = NewObject<UCameraShake>(GetTransientPackage(), ActiveSection->GetCameraShakeClass());
						if (ShakeInst)
						{
							// make it root so GC doesn't take it away
							ShakeInst->AddToRoot();
							ShakeInst->SetTempCameraAnimActor(GetTempCameraActor(Player));
							ShakeInst->PlayShake(nullptr, ActiveSection->PlayScale, ActiveSection->PlaySpace, ActiveSection->UserDefinedPlaySpace);
							if (ShakeInst->AnimInst)
							{
								ShakeInst->AnimInst->SetStopAutomatically(false);
							}
						}

						SectionInstanceData.CameraShakeInst = ShakeInst;
					}

					// advance the shake and apply it
					if (SectionInstanceData.CameraShakeInst.IsValid())
					{
						FMinimalViewInfo POV;
						POV.Location = CameraComponent->GetComponentLocation();
						POV.Rotation = CameraComponent->GetComponentRotation();
						POV.FOV = CameraComponent->FieldOfView;

						// prepare temp camera actor by resetting it
						{
							ACameraActor* const CameraActor = GetTempCameraActor(Player);
							ACameraActor const* const DefaultCamActor = GetDefault<ACameraActor>();
							if (CameraActor && DefaultCamActor)
							{
								CameraActor->SetActorLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);

								CameraActor->GetCameraComponent()->AspectRatio = DefaultCamActor->GetCameraComponent()->AspectRatio;

								if (SectionInstanceData.CameraShakeInst->AnimInst && SectionInstanceData.CameraShakeInst->AnimInst->CamAnim)
								{
									CameraActor->GetCameraComponent()->PostProcessSettings = SectionInstanceData.CameraShakeInst->AnimInst->CamAnim->BasePostProcessSettings;
									CameraActor->GetCameraComponent()->PostProcessBlendWeight = SectionInstanceData.CameraShakeInst->AnimInst->CamAnim->BasePostProcessBlendWeight;
								}
							}
						}

						// set camera anim to the correct time
						float const NewCameraAnimTime = UpdateData.Position - ActiveSection->GetStartTime();
						SectionInstanceData.CameraShakeInst->SetCurrentTimeAndApplyShake(NewCameraAnimTime, POV);

						// work out the offset transforms
						FTransform WorldToBaseCamera = CameraComponent->GetComponentToWorld().Inverse();
						float BaseFOV = CameraComponent->FieldOfView;
						FTransform NewCameraToWorld(POV.Rotation, POV.Location);
						float NewFOV = POV.FOV;

						FTransform NewCameraToBaseCamera = NewCameraToWorld * WorldToBaseCamera;
						float NewFOVToBaseFOV = BaseFOV - NewFOV;

						SectionInstanceData.AdditiveFOVOffset = NewFOVToBaseFOV;
						SectionInstanceData.AdditiveOffset = NewCameraToBaseCamera;

						// harvest PP changes
						{
							UCameraComponent* AnimCamComp = GetTempCameraActor(Player)->GetCameraComponent();
							if (AnimCamComp)
							{
								SectionInstanceData.PostProcessingBlendWeight = AnimCamComp->PostProcessBlendWeight;
								if (SectionInstanceData.PostProcessingBlendWeight > 0.f)
								{
									// avoid big struct copy if not necessary
									SectionInstanceData.PostProcessingSettings = AnimCamComp->PostProcessSettings;
								}
							}
						}
					}
				}

				// now we have all the offsets, collapse them all and add to the camera
				FTransform TotalTransform = FTransform::Identity;
				float TotalFOVOffset = 0.f;
				for (auto ActiveSection : ActiveSections)
				{
					FMovieSceneCameraShakeSectionInstanceData& SectionInstanceData = SectionInstanceDataMap.FindChecked(ActiveSection);
					TotalTransform = TotalTransform * SectionInstanceData.AdditiveOffset;
					TotalFOVOffset += SectionInstanceData.AdditiveFOVOffset;

					if (SectionInstanceData.PostProcessingBlendWeight > 0.f)
					{
						CameraComponent->AddExtraPostProcessBlend(SectionInstanceData.PostProcessingSettings, SectionInstanceData.PostProcessingBlendWeight);
					}
				}

				CameraComponent->AddAdditiveOffset(TotalTransform, TotalFOVOffset);
			}
		}
	}
}


ACameraActor* FMovieSceneCameraShakeTrackInstance::GetTempCameraActor(IMovieScenePlayer& InMovieScenePlayer)
{
	if (TempCameraActor.IsValid() == false)
	{
		// spawn the temp CameraActor used for updating CameraAnims
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnInfo.ObjectFlags |= RF_Transient;	// We never want to save these temp actors into a map
		ACameraActor* const Cam = InMovieScenePlayer.GetPlaybackContext()->GetWorld()->SpawnActor<ACameraActor>(SpawnInfo);
		if (Cam)
		{
#if WITH_EDITOR
			Cam->SetIsTemporarilyHiddenInEditor(true);
#endif
			TempCameraActor = Cam;
		}
	}

	return TempCameraActor.Get();
}