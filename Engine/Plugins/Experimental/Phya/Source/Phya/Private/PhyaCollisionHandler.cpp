// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PhyaPluginPrivatePCH.h"
#include "PhysicsPublic.h"
#include "Phya.hpp"
#include "Sound/SoundWaveProcedural.h"

#define N_BODIES 5

static char ModalDataString[] = 
	"1.000000	.4000000	2.000000/n\
	2234.069824	14.590249	1.000000/n\
	2239.453125	14.497436	0.997058/n\
	5211.035156	37.339256	0.300361/n\
	4473.522949	32.487823	0.228933/n\
	5.383301	33.300861	0.097548/n\
	5205.651855	23.603500	0.063464/n\
	5189.501953	26.561996	0.025924/n\
	9124.695313	21.363480	0.016809";

static int SampleRate = 44100;
static int BlockSize = 128;


uint32 GetTypeHash( const FPhyaBodyInstancePair& Pair )
{
	return GetTypeHash(Pair.Body0) ^ GetTypeHash(Pair.Body1);
}

//////////////////////////////////////////////////////////////////////////

UPhyaCollisionHandler::UPhyaCollisionHandler(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}


class FPhyaGlobals
{
	
};

static FPhyaGlobals* GPhyaGlobals;

static void InitPhya()
{
	if(GPhyaGlobals == NULL)
	{
		paBlock::setnMaxFrames(BlockSize);						// The size of each audio block.
		paBlock::pool.allocate(N_BODIES+5);					// Num resonators+5.
		paContact::pool.allocate(100);
		paImpact::pool.allocate(100);
		paFunSurface::contactGenPool.allocate(20);			// Each Contact and Impact takes 2 contactGens.
		paFunSurface::impactGenPool.allocate(20);			// Each Impact takes 2 impactGens.
		paRes::activeResList.allocate(N_BODIES);			// Num resonators.

		paSetnFramesPerSecond(SampleRate);

		//paSetLimiter(0.005f, 0.015f, 0.075f);	// Set limiter parameters. Leave this out if you don't want limiting.

		paInit();

		GPhyaGlobals = new FPhyaGlobals;
	}
}

void UPhyaCollisionHandler::InitCollisionHandler()
{
	// Make sure Phya is init'd
	InitPhya();

	// Create streaming wave object
	ProceduralWave = NewObject<USoundWaveProcedural>(this);
	ProceduralWave->SampleRate = SampleRate;
	ProceduralWave->NumChannels = 1;
	ProceduralWave->Duration = INDEFINITELY_LOOPING_DURATION;
	ProceduralWave->bLooping = false;
	ProceduralWave->OnSoundWaveProceduralUnderflow = FOnSoundWaveProceduralUnderflow::CreateUObject(this, &UPhyaCollisionHandler::ProceduralWaveUnderflow);

	// Create audio component
	AudioComp = NewObject<UAudioComponent>(this);
	AudioComp->SetSound(ProceduralWave);
	AudioComp->Play();

	Whitesurf = new paFunSurface;			// Function surface.. uses surface functions, combined with

	Whitefun = new paWhiteFun;				// White noise
	Whitesurf->setFun(Whitefun);							// White noise surface texture.
	Whitesurf->setContactGain(32000.0f);				// Includes gain to generate audio output level.

	// The following define the velocity to surface-filter map.
	// This rises linearly to a maximum frequency value.
	Whitesurf->setCutoffFreqAtRoll(10.0f);					// Adjust the rel body vel to filter cutoff mapping.
	Whitesurf->setCutoffFreqRate(1000.0f);					// Rate of change of cutoff freq with slip speed.
	Whitesurf->setCutoffFreq2AtRoll(10.0f);					// Beef up rolling with optional extra filter layer.
	Whitesurf->setCutoffFreq2Rate(1000.0f);
	//	whitesurf->setGainAtRoll(10.0f);							// Modify volume towards rolling, simulates increased transfer.
	//	whitesurf->setGainBreakSlipSpeed(0.1f);
	Whitesurf->setContactDirectGain(0.0);					// We can tune the settings for each type of surface.
	Whitesurf->setContactGain(70000.0);
	Whitesurf->setHardness(10000.0f);  // Impulse time = 1/hardness (limited).

	ModalData = new paModalData;
	ModalData->read(ModalDataString);

	for(int32 i=0; i< N_BODIES; i++) 
	{
		paModalRes* Res = new paModalRes;
		Res->setData(ModalData);
		Res->setQuietLevel(1.0f);		// Determines at what rms envelope level a resonator will be 
										// faded out when no longer in contact, to save cpu.
										// Make bigger to save more cpu, but possibly truncate decays notceably.

		//		mr->setnActiveModes(10);		// Can trade detail for speed.
		Res->setAuxAmpScale(.1f);
		//Res->setAuxFreqScale(0.5f + 0.1f*i);
		Res->setAuxFreqScale(1.0f);
		Res->setAuxDampScale(1.0f);


		paBody* Body = new paBody;
		Body->setRes(Res);			// NB Possible to have several bodies using one res for efficiency.
		Body->setSurface(Whitesurf);

		Bodies.Add(Body);
	}

	// Fill the audio buffer
	//GenerateAudio();
}

void UPhyaCollisionHandler::ProceduralWaveUnderflow(USoundWaveProcedural* InProceduralWave, int32 SamplesRequired)
{
	check(ProceduralWave == InProceduralWave);

	const int32 QueuedSamples = ProceduralWave->GetAvailableAudioByteCount()/sizeof(uint16);
	const int32 SamplesNeeded = SamplesRequired - QueuedSamples;
	const int32 BlocksNeeded = FMath::CeilToInt(SamplesNeeded/BlockSize);

	UE_LOG(LogTemp, Log, TEXT("Creating %d blocks for %s"), BlocksNeeded, *GetWorld()->GetPathName());

	TArray<int16> SampleData;
	SampleData.AddUninitialized(BlocksNeeded * BlockSize);
	int32 CurrSample = 0;

	for(int32 BlockIdx=0; BlockIdx<BlocksNeeded; BlockIdx++)
	{
		paBlock* AudioBlock = paTick();
		float* AudioData = AudioBlock->getStart();

		for(int32 SampleIdx=0; SampleIdx<BlockSize; SampleIdx++)
		{
			SampleData[CurrSample] = (int16)AudioData[SampleIdx];
			CurrSample++;
			//UE_LOG(LogTemp, Log, TEXT("%d:%f"), CurrSample, SampleData[CurrSample]);
		}
	}

	ProceduralWave->QueueAudio((uint8*)SampleData.GetData(), SampleData.Num() * sizeof(int16));
}


void UPhyaCollisionHandler::TestImpact()
{
	UE_LOG(LogTemp, Log, TEXT("Creating Test Impact for %s"), *GetWorld()->GetPathName());

	paImpact* Impact = paImpact::newImpact();
	if(Impact != NULL)
	{
		Impact->setBody1(Bodies[0]);

		paImpactDynamicData ImpactData;
		ImpactData.relTangentSpeedAtImpact = 0; // No skid.
		ImpactData.impactImpulse = 1.0;

		Impact->setDynamicData(&ImpactData);
	}
}



void UPhyaCollisionHandler::HandlePhysicsCollisions_AssumesLocked(const TArray<FCollisionNotifyInfo>& PendingCollisionNotifies)
{
	if(GetWorld()->HasBegunPlay())
	{
		TMap< FPhyaBodyInstancePair, TSharedPtr<FPhyaPairInfo> > ExpiredPairHash = PairHash;

		for(int32 InfoIdx=0; InfoIdx<PendingCollisionNotifies.Num(); InfoIdx++)
		{
			const FCollisionNotifyInfo& Info = PendingCollisionNotifies[InfoIdx];
			FPhyaBodyInstancePair Pair(Info.Info0.GetBodyInstance(), Info.Info1.GetBodyInstance());
			// Find pair in hash
			TSharedPtr<FPhyaPairInfo> PairInfo = PairHash.FindRef(Pair);

			// Existing pair
			if(PairInfo.IsValid())
			{
				UE_LOG(LogTemp, Log, TEXT("EXISTING"));

				ExpiredPairHash.Remove(Pair); // Not expired
			}
			// New pair
			else
			{
				UE_LOG(LogTemp, Log, TEXT("NEW"));

				PairInfo = MakeShareable( new FPhyaPairInfo );
				PairHash.Add(Pair, PairInfo);

				paImpact* Impact = paImpact::newImpact();
				if(Impact != NULL)
				{
					Impact->setBody1(Bodies[0]);

					paImpactDynamicData ImpactData;
					ImpactData.relTangentSpeedAtImpact = 0; // No skid.
					ImpactData.impactImpulse = 1.0;

					Impact->setDynamicData(&ImpactData);
				}
			}

		}

		// Expire pairs from PairHash still in ExpiredPairHash
		for( auto It = ExpiredPairHash.CreateConstIterator(); It; ++It )
		{
			UE_LOG(LogTemp, Log, TEXT("EXPIRE"));
			FPhyaBodyInstancePair Pair = It.Key();
			PairHash.Remove(Pair);
		}

		/*
		float WorldTime = GetWorld()->GetTimeSeconds();
		float TimeSinceLastTestImpact = WorldTime - LastTestImpactTime;
		if(TimeSinceLastTestImpact > 1.f)
		{
			TestImpact();
			LastTestImpactTime = WorldTime;
		}
		*/
	}
}