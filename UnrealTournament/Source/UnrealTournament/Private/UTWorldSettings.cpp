// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWorldSettings.h"
#include "UTDmgType_KillZ.h"
#include "Particles/ParticleSystemComponent.h"
#include "UTGameEngine.h"
#include "UTLevelSummary.h"

AUTWorldSettings::AUTWorldSettings(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	KillZDamageType = UUTDmgType_KillZ::StaticClass();

	MaxImpactEffectVisibleLifetime = 30.0f;
	MaxImpactEffectInvisibleLifetime = 15.0f;
	ImpactEffectFadeSpeed = 0.5f;
	ImpactEffectFadeTime=1.0f;

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

AUTWorldSettings* AUTWorldSettings::GetWorldSettings(UObject* WorldContextObject)
{
	UWorld* World = (WorldContextObject != NULL) ? WorldContextObject->GetWorld() : NULL;
	return (World != NULL) ? Cast<AUTWorldSettings>(World->GetWorldSettings()) : NULL;
}

void AUTWorldSettings::PostLoad()
{
	Super::PostLoad();
	CreateLevelSummary();
}
void AUTWorldSettings::PostInitProperties()
{
	Super::PostInitProperties();
	if (!HasAnyFlags(RF_NeedLoad))
	{
		CreateLevelSummary();
	}
}

void AUTWorldSettings::CreateLevelSummary()
{
	if (!IsTemplate())
	{
		// we need the object name to be reliable so we can pull it out by itself in the menus without loading the whole map
		static FName NAME_LevelSummary(TEXT("LevelSummary"));
		if (LevelSummary == NULL)
		{
			LevelSummary = FindObject<UUTLevelSummary>(UUTLevelSummary::StaticClass(), *NAME_LevelSummary.ToString());
			if (LevelSummary == NULL)
			{
				LevelSummary = NewObject<UUTLevelSummary>(GetOutermost(), NAME_LevelSummary, RF_Standalone);
			}
		}
		else if (LevelSummary->GetFName() != NAME_LevelSummary)
		{
			// we have to duplicate instead of rename because we may be in PostLoad() and it's not safe to rename from there
			LevelSummary = DuplicateObject<UUTLevelSummary>(LevelSummary, GetOutermost(), *NAME_LevelSummary.ToString());
		}
	}
}

void AUTWorldSettings::NotifyBeginPlay()
{
	UWorld* World = GetWorld();
	if (!World->bBegunPlay)
	{
		World->bBegunPlay = true;
		// in order to avoid double calls to newly added actors we need to track what we started with
		TArray<AActor*> FullActorList;
		FullActorList.Reserve(World->PersistentLevel->Actors.Num());
		for (FActorIterator It(World); It; ++It)
		{
			FullActorList.Add(*It);
		}
		for (AActor* Actor : FullActorList)
		{
			Actor->BeginPlay();
		}
	}
}

void AUTWorldSettings::BeginPlay()
{
	if (Music != NULL && GetNetMode() != NM_DedicatedServer)
	{
		MusicComp = NewObject<UAudioComponent>(this);
		MusicComp->bAllowSpatialization = false;
		MusicComp->SetSound(Music);
		MusicComp->Play();
	}

	Super::BeginPlay();

	if (!bPendingKillPending)
	{
		FTimerHandle TempHandle;
		GetWorldTimerManager().SetTimer(TempHandle, this, &AUTWorldSettings::ExpireImpactEffects, 0.5f, true);
	}
}

void AUTWorldSettings::AddImpactEffect(USceneComponent* NewEffect, float LifeScaling)
{
	bool bNeedsTiming = true;

	// do some checks for components we can assume will go away on their own
	UParticleSystemComponent* PSC = Cast <UParticleSystemComponent>(NewEffect);
	if (PSC != NULL)
	{
		if (PSC->bAutoDestroy && PSC->Template != NULL && !IsLoopingParticleSystem(PSC->Template))
		{
			bNeedsTiming = false;
		}
	}
	else
	{
		UAudioComponent* AC = Cast<UAudioComponent>(NewEffect);
		if (AC != NULL)
		{
			if (AC->bAutoDestroy && AC->Sound != NULL && AC->Sound->GetDuration() < INDEFINITELY_LOOPING_DURATION)
			{
				bNeedsTiming = false;
			}
		}
	}

	if (bNeedsTiming)
	{
		// spawning is usually the hotspot over removal so add to end here and deal with slightly more cost on the other end
		new(TimedEffects)FTimedImpactEffect(NewEffect, GetWorld()->TimeSeconds, LifeScaling);
	}
}

void AUTWorldSettings::FadeImpactEffects(float DeltaTime)
{
	float WorldTime = GetWorld()->TimeSeconds;

	for (int32 i = 0; i < FadingEffects.Num(); i++)
	{
		float LastRenderTime = WorldTime;
		UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(FadingEffects[i].EffectComp);
		if (Prim != NULL)
		{
			LastRenderTime = Prim->LastRenderTime;
		}

		float DesiredTimeout = (WorldTime - LastRenderTime < 1.0f) ? MaxImpactEffectVisibleLifetime : MaxImpactEffectInvisibleLifetime;
		DesiredTimeout *= FadingEffects[i].LifetimeScaling;

		float TimeLived = WorldTime - FadingEffects[i].CreationTime;

		UDecalComponent* Decal = Cast<UDecalComponent>(FadingEffects[i].EffectComp);
		if (Decal)
		{
			Decal->FadeScreenSize += ImpactEffectFadeSpeed * DeltaTime * FadingEffects[i].FadeMultipllier;
		}

		if (TimeLived > DesiredTimeout)
		{
			FadingEffects[i].EffectComp->DetachFromParent();
			FadingEffects[i].EffectComp->DestroyComponent();
			FadingEffects.RemoveAt(i--);
		}
	}
}

void AUTWorldSettings::ExpireImpactEffects()
{
	float WorldTime = GetWorld()->TimeSeconds;
	// clamp to a maximum total in addition to a time limit so a burst of visible effects doesn't kill framerate until lifetime expires
	// note that we're doing this here so we don't have detach times stacking on top of spawn times
	if (MaxImpactEffectInvisibleLifetime > 0.0f)
	{
		int32 NumToKill = TimedEffects.Num() - FMath::TruncToInt(MaxImpactEffectInvisibleLifetime * 3.0f);
		if (NumToKill > 0)
		{
			for (int32 i = NumToKill - 1; i >= 0; i--)
			{
				if (TimedEffects[i].EffectComp != NULL && TimedEffects[i].EffectComp->IsRegistered())
				{
					TimedEffects[i].CreationTime = WorldTime - MaxImpactEffectVisibleLifetime * TimedEffects[i].LifetimeScaling + ImpactEffectFadeTime;
					UDecalComponent* Decal = Cast<UDecalComponent>(TimedEffects[i].EffectComp);
					if (Decal)
					{
						TimedEffects[i].FadeMultipllier = Decal->FadeScreenSize / 0.01f;
					}
					FadingEffects.Add(TimedEffects[i]);
				}
			}
			TimedEffects.RemoveAt(0, NumToKill);
		}
	}
	for (int32 i = 0; i < TimedEffects.Num(); i++)
	{
		if (TimedEffects[i].EffectComp == NULL || !TimedEffects[i].EffectComp->IsRegistered())
		{
			TimedEffects.RemoveAt(i--);
		}
		else
		{
			// try to get LastRenderTime of component
			// if it's not available, assume it's visible
			float LastRenderTime = WorldTime;
			UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(TimedEffects[i].EffectComp);
			if (Prim != NULL)
			{
				LastRenderTime = Prim->LastRenderTime;
			}
			float DesiredTimeout = (WorldTime - LastRenderTime < 1.0f) ? MaxImpactEffectVisibleLifetime : MaxImpactEffectInvisibleLifetime;
			DesiredTimeout *= TimedEffects[i].LifetimeScaling;
			if (DesiredTimeout > 0.0f)
			{
				float TimeLived = WorldTime - TimedEffects[i].CreationTime;
				if (TimeLived > DesiredTimeout)
				{
					TimedEffects[i].EffectComp->DetachFromParent();
					TimedEffects[i].EffectComp->DestroyComponent();
					TimedEffects.RemoveAt(i--);
				}
				else if (TimeLived > DesiredTimeout - ImpactEffectFadeTime)
				{
					UDecalComponent* Decal = Cast<UDecalComponent>(TimedEffects[i].EffectComp);
					if (Decal)
					{
						TimedEffects[i].FadeMultipllier = Decal->FadeScreenSize / 0.01f;
					}

					FadingEffects.Add(TimedEffects[i]);
					TimedEffects.RemoveAt(i--);
				}
			}
		}
	}
}

void AUTWorldSettings::AddTimedMaterialParameter(UMaterialInstanceDynamic* InMI, FName InParamName, UCurveBase* InCurve, bool bInClearOnComplete)
{
	for (int32 i = 0; i < MaterialParamCurves.Num(); i++)
	{
		if (MaterialParamCurves[i].MI == InMI && MaterialParamCurves[i].ParamName == InParamName)
		{
			MaterialParamCurves[i] = FTimedMaterialParameter(InMI, InParamName, InCurve, bInClearOnComplete);
			return;
		}
	}

	new(MaterialParamCurves) FTimedMaterialParameter(InMI, InParamName, InCurve, bInClearOnComplete);
}

void AUTWorldSettings::AddTimedLightParameter(ULightComponent* InLight, ETimedLightParameter InParam, UCurveBase* InCurve)
{
	for (int32 i = 0; i < LightParamCurves.Num(); i++)
	{
		if (LightParamCurves[i].Light == InLight && LightParamCurves[i].Param == InParam)
		{
			LightParamCurves[i] = FTimedLightParameter(InLight, InParam, InCurve);
			return;
		}
	}

	new(LightParamCurves) FTimedLightParameter(InLight, InParam, InCurve);
}

void AUTWorldSettings::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	FadeImpactEffects(DeltaTime);

	for (int32 i = 0; i < MaterialParamCurves.Num(); i++)
	{
		if (!MaterialParamCurves[i].MI.IsValid() || MaterialParamCurves[i].ParamCurve == NULL)
		{
			MaterialParamCurves.RemoveAt(i--, 1);
		}
		else
		{
			MaterialParamCurves[i].ElapsedTime += DeltaTime;
			bool bAtEnd = false;
			{
				float MinTime, MaxTime;
				MaterialParamCurves[i].ParamCurve->GetTimeRange(MinTime, MaxTime);
				bAtEnd = MaterialParamCurves[i].ElapsedTime > MaxTime;
			}
			UCurveFloat* FloatCurve = Cast<UCurveFloat>(MaterialParamCurves[i].ParamCurve);
			if (FloatCurve != NULL)
			{
				if (bAtEnd && MaterialParamCurves[i].bClearOnComplete && MaterialParamCurves[i].MI->Parent != NULL)
				{
					// there's no clear single parameter in UMaterialInstance...
					float ParentValue = 0.0f;
					MaterialParamCurves[i].MI->Parent->GetScalarParameterValue(MaterialParamCurves[i].ParamName, ParentValue);
					MaterialParamCurves[i].MI->SetScalarParameterValue(MaterialParamCurves[i].ParamName, ParentValue);
				}
				else
				{
					MaterialParamCurves[i].MI->SetScalarParameterValue(MaterialParamCurves[i].ParamName, FloatCurve->GetFloatValue(MaterialParamCurves[i].ElapsedTime));
				}
			}
			else
			{
				UCurveLinearColor* ColorCurve = Cast<UCurveLinearColor>(MaterialParamCurves[i].ParamCurve);
				if (ColorCurve != NULL)
				{
					if (bAtEnd && MaterialParamCurves[i].bClearOnComplete && MaterialParamCurves[i].MI->Parent != NULL)
					{
						// there's no clear single parameter in UMaterialInstance...
						FLinearColor ParentValue = FLinearColor::Black;
						MaterialParamCurves[i].MI->Parent->GetVectorParameterValue(MaterialParamCurves[i].ParamName, ParentValue);
						MaterialParamCurves[i].MI->SetVectorParameterValue(MaterialParamCurves[i].ParamName, ParentValue);
					}
					else
					{
						MaterialParamCurves[i].MI->SetVectorParameterValue(MaterialParamCurves[i].ParamName, ColorCurve->GetLinearColorValue(MaterialParamCurves[i].ElapsedTime));
					}
				}
			}
			if (bAtEnd)
			{
				MaterialParamCurves.RemoveAt(i--, 1);
			}
		}
	}
	for (int32 i = 0; i < LightParamCurves.Num(); i++)
	{
		if (!LightParamCurves[i].Light.IsValid() || LightParamCurves[i].ParamCurve == NULL)
		{
			LightParamCurves.RemoveAt(i--, 1);
		}
		else
		{
			LightParamCurves[i].ElapsedTime += DeltaTime;
			bool bAtEnd = false;
			{
				float MinTime, MaxTime;
				LightParamCurves[i].ParamCurve->GetTimeRange(MinTime, MaxTime);
				bAtEnd = LightParamCurves[i].ElapsedTime > MaxTime;
			}
			switch (LightParamCurves[i].Param)
			{
				case TLP_Color:
				{
					UCurveLinearColor* ColorCurve = Cast<UCurveLinearColor>(LightParamCurves[i].ParamCurve);
					if (ColorCurve != NULL)
					{
						LightParamCurves[i].Light->SetLightColor(ColorCurve->GetLinearColorValue(LightParamCurves[i].ElapsedTime));
					}
					break;
				}
				case TLP_Intensity:
				{
					UCurveFloat* FloatCurve = Cast<UCurveFloat>(LightParamCurves[i].ParamCurve);
					if (FloatCurve != NULL)
					{
						LightParamCurves[i].Light->SetIntensity(FloatCurve->GetFloatValue(LightParamCurves[i].ElapsedTime));
					}
					break;
				}
				default:
					break;
			}
			if (bAtEnd)
			{
				// if the light is part of a timed effect, terminate it when the animation completes if it is at zero intensity
				if (LightParamCurves[i].Light->Intensity <= 0.0f)
				{
					for (int32 j = 0; j < TimedEffects.Num(); j++)
					{
						if (TimedEffects[j].EffectComp == LightParamCurves[i].Light)
						{
							TimedEffects[j].EffectComp->DetachFromParent();
							TimedEffects[j].EffectComp->DestroyComponent();
							TimedEffects.RemoveAt(j--);
							break;
						}
					}
				}
				
				LightParamCurves.RemoveAt(i--, 1);
			}
		}
	}
}

bool AUTWorldSettings::EffectIsRelevant(AActor* RelevantActor, const FVector& SpawnLocation, bool bSpawnNearSelf, bool bIsLocallyOwnedEffect, float CullDistance, float AlwaysSpawnDist, bool bForceDedicated)
{
	// dedicated server entirely controlled by bForceDedicated flag, as most effects shouldn't be spawned on server
	if (GetNetMode() == NM_DedicatedServer)
	{
		return bForceDedicated;
	}

	// Check if beyond cull distance for all local viewers
	// @TODO FIXMESTEVE take zoom into account
	bool bWithinCullDistance = false;
	bool bBehindAllPlayers = true;
	// @TODO FIXMESTEVE - add setting for effect cull distance scaling, and a bNeverCull option
	float CullDistSq = bIsLocallyOwnedEffect ? 2.f*FMath::Square(CullDistance) : FMath::Square(CullDistance);
	for (ULocalPlayer* LocalPlayer : GEngine->GetGamePlayers(GetWorld()))
	{
		if (LocalPlayer->PlayerController != NULL)
		{
			FVector ViewLoc;
			FRotator ViewRot;
			LocalPlayer->PlayerController->GetPlayerViewPoint(ViewLoc, ViewRot);
			float DistSq = (ViewLoc - SpawnLocation).SizeSquared();
			//UE_LOG(UT, Warning, TEXT("Effect dist %f"), (ViewLoc - SpawnLocation).Size());
			if (DistSq < CullDistSq)
			{
				bWithinCullDistance = true;
				if (DistSq < FMath::Square(AlwaysSpawnDist))
				{
					// Always spawn effect when this close
					// @TODO FIXMESTEVE - do we want to differentiate between dist when in front of viewer versus behind?
					return true;
				}
				if ((ViewRot.Vector() | (SpawnLocation - ViewLoc).GetSafeNormal()) > 0.1f)
				{
					bBehindAllPlayers = false;
				}
			}
		}
	}
	if (bIsLocallyOwnedEffect && bWithinCullDistance)
	{
		return true;
	}
	if (!bWithinCullDistance || bBehindAllPlayers)
	{
		return false;
	}

	// if effect is spawning near me, always spawn if being rendered
	if (bSpawnNearSelf && RelevantActor != NULL)
	{
		return (GetWorld()->GetTimeSeconds() - RelevantActor->GetLastRenderTime() < 0.3f);
	}

	return true;
}
