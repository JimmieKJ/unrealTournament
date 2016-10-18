// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneCommonHelpers.h"
#include "MovieSceneCameraAnimTrackInstance.h"
#include "MovieSceneCameraAnimTrack.h"
#include "MovieSceneCameraAnimSection.h"
#include "IMovieScenePlayer.h"


FMovieSceneCameraAnimTrackInstance::FMovieSceneCameraAnimTrackInstance(UMovieSceneCameraAnimTrack& InCameraAnimTrack)
{
	CameraAnimTrack = &InCameraAnimTrack;
}

FMovieSceneCameraAnimTrackInstance::~FMovieSceneCameraAnimTrackInstance()
{
	ACameraActor* const CamActor = TempCameraActor.Get();
	if (CamActor)
	{
		CamActor->Destroy();
	}
}

void FMovieSceneCameraAnimTrackInstance::Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
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
				TArray<UMovieSceneCameraAnimSection*> ActiveSections;
				CameraAnimTrack->GetCameraAnimSectionsAtTime(UpdateData.Position, ActiveSections);

				// look for section instances that are no longer playing
				TArray<UMovieSceneCameraAnimSection*> DeadSections;
				for (const auto& Iter : SectionInstanceDataMap)
				{
					if (ActiveSections.Contains(Iter.Key) == false)
					{
						// section isn't active anymore, stop playing the anim
						FMovieSceneCameraAnimSectionInstanceData const& SectionInstanceData = Iter.Value;
						SectionInstanceData.CameraAnimInst->Stop(true);
						SectionInstanceData.CameraAnimInst->RemoveFromRoot();

						DeadSections.Add(Iter.Key);
					}
				}
				for (auto DeadSection : DeadSections)
				{
					SectionInstanceDataMap.Remove(DeadSection);
				}

				// sort by start time to match application order of player camera
				ActiveSections.Sort(
					[](const UMovieSceneCameraAnimSection& One, const UMovieSceneCameraAnimSection& Two)
				{
					return One.GetStartTime() < Two.GetStartTime();
				}
				);

				// look for sections that just started and need instances
				for (auto ActiveSection : ActiveSections)
				{
					FMovieSceneCameraAnimSectionInstanceData& SectionInstanceData = SectionInstanceDataMap.FindOrAdd(ActiveSection);

					if (SectionInstanceData.CameraAnimInst == nullptr)
					{
						// Start playing it
						UCameraAnimInst* const CamAnimInst = NewObject<UCameraAnimInst>(GetTransientPackage());
						if (CamAnimInst)
						{
							// make it root so GC doesn't take it away
							CamAnimInst->AddToRoot();
							CamAnimInst->SetStopAutomatically(false);
							CamAnimInst->Play(ActiveSection->GetCameraAnim(), GetTempCameraActor(Player), ActiveSection->PlayRate, ActiveSection->PlayScale, ActiveSection->BlendInTime, ActiveSection->BlendOutTime, ActiveSection->bLooping, ActiveSection->bRandomStartTime);
						}

						SectionInstanceData.CameraAnimInst = CamAnimInst;
					}

					if (SectionInstanceData.CameraAnimInst.IsValid())
					{
						// prepare temp camera actor by resetting it
						{
							ACameraActor* const CameraActor = GetTempCameraActor(Player);
							CameraActor->SetActorLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);

							ACameraActor const* const DefaultCamActor = GetDefault<ACameraActor>();
							if (DefaultCamActor)
							{
								CameraActor->GetCameraComponent()->AspectRatio = DefaultCamActor->GetCameraComponent()->AspectRatio;
								CameraActor->GetCameraComponent()->PostProcessSettings = SectionInstanceData.CameraAnimInst->CamAnim->BasePostProcessSettings;
								CameraActor->GetCameraComponent()->PostProcessBlendWeight = SectionInstanceData.CameraAnimInst->CamAnim->BasePostProcessBlendWeight;
							}
						}

						// set camera anim to the correct time
						float const NewCameraAnimTime = UpdateData.Position - ActiveSection->GetStartTime();
						SectionInstanceData.CameraAnimInst->SetCurrentTime(NewCameraAnimTime);

						// harvest properties from the actor and apply
						if (SectionInstanceData.CameraAnimInst->CurrentBlendWeight > 0.f)
						{
							FMinimalViewInfo POV;
							POV.Location = CameraComponent->GetComponentLocation();
							POV.Rotation = CameraComponent->GetComponentRotation();
							POV.FOV = CameraComponent->FieldOfView;

							SectionInstanceData.CameraAnimInst->ApplyToView(POV);

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
				}

				// now we have all the offsets, collapse them all and add to the camera
				FTransform TotalTransform = FTransform::Identity;
				float TotalFOVOffset = 0.f;
				for (auto ActiveSection : ActiveSections)
				{
					FMovieSceneCameraAnimSectionInstanceData& SectionInstanceData = SectionInstanceDataMap.FindChecked(ActiveSection);
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

ACameraActor* FMovieSceneCameraAnimTrackInstance::GetTempCameraActor(IMovieScenePlayer& InMovieScenePlayer)
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