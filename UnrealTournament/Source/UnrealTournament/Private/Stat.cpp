// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"

UStat::UStat(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbsoluteMaxValue = 0;
	for(int32 i = 0; i < EStatRecordingPeriod::Max; i++)
	{
		StatDataByPeriod.Add(0);
	}
}

/** Modifier function - Adds the Amount to the preexisting InitialValue of the stat
 *	@param InitialValue The original value of this stat
 *	@param Amount The amount to add to the InitialValue
 */
int32 UStat::DeltaModifier(int32 InitialValue, int32 Amount)
{
	//Prevent int wraparound, which can occur in this case
	//Attempted to subtract and the result is greater than what we started at.
	if (Amount < 0 && InitialValue + Amount > InitialValue)
	{
		//Bind to MIN_int32
		return MIN_int32;
	}
	//Attempted to add and the result is less than what we started at.
	else if (Amount > 0 && InitialValue + Amount < InitialValue)
	{
		//Bind to MAX_int32
		return MAX_int32;
	}
	return InitialValue + Amount;
}

/** Modifier function - Use the Amount, ignoring the InitialValue
 *	@param InitialValue The original value of this stat
 *	@param Amount The amount to set the stat to
 */
int32 UStat::SetModifier(int32 InitialValue, int32 Amount)
{
	return Amount;
}

/** Modifier function - Use the max of the existing amount and the new amount
 *	@param InitialValue The original value of this stat
 *	@param Amount The amount to set the stat to
 */
int32 UStat::MaxModifier(int32 InitialValue, int32 Amount)
{
	return FMath::Max(InitialValue, Amount);
}

/** Called by the game code when something has caused this stat to change
 *	@param Amount A value used to change the stat
 *	@param ModType The way in which the Amount will be used to change the stat
 */
void UStat::ModifyStat(int32 Amount, EStatMod::Type ModType)
{
	FStatModifier ModifierFunc;

	//Determine which modifier function to use
	switch (ModType)
	{
	case EStatMod::Delta:
		ModifierFunc = FStatModifier::CreateUObject(this, &UStat::DeltaModifier);
		break;
	case EStatMod::Set:
		ModifierFunc = FStatModifier::CreateUObject(this, &UStat::SetModifier);
		break;
	case EStatMod::Maximum:
		ModifierFunc = FStatModifier::CreateUObject(this, &UStat::MaxModifier);
		break;
	default:
		ModifierFunc = FStatModifier::CreateUObject(this, &UStat::DeltaModifier);
		break;
	}

	//For every period...
	for (int32 PeriodIndex = 0; PeriodIndex < StatDataByPeriod.Num() && PeriodIndex <= HighestPeriodToTrack; PeriodIndex++)
	{
		//Modify that periods data appropriately, then...
		StatDataByPeriod[PeriodIndex] = ModifierFunc.Execute(StatDataByPeriod[PeriodIndex], Amount);

		//Check against max value
		if (AbsoluteMaxValue != 0)
		{
			StatDataByPeriod[PeriodIndex] = FMath::Min(AbsoluteMaxValue, StatDataByPeriod[PeriodIndex]);
		}
	}

	AUTPlayerState* Player = NULL;

	if (Cast<UStatManager>(GetOuter()))
	{
		Player = Cast<UStatManager>(GetOuter())->UTPS;
	}

	OnStatModifiedDelegate.Broadcast(Player, StatName, Amount, ModType);
}
