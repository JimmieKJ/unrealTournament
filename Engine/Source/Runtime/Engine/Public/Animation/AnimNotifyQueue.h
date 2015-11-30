// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

struct FAnimNotifyQueue
{
	FAnimNotifyQueue()
		: PredictedLODLevel(-1)
	{
		RandomStream.Initialize(0x05629063);
	}

	/** Should the notifies current filtering mode stop it from triggering */
	bool PassesFiltering(const FAnimNotifyEvent* Notify) const;

	/** Work out whether this notify should be triggered based on its chance of triggering value */
	bool PassesChanceOfTriggering(const FAnimNotifyEvent* Event) const;

	/** Add anim notifier **/
	void AddAnimNotifies(const TArray<const FAnimNotifyEvent*>& NewNotifies, const float InstanceWeight);

	/** Reset queue & update LOD level */
	void Reset(USkeletalMeshComponent* Component);

	/** Append one queue to another */
	void Append(const FAnimNotifyQueue& Queue);

	/** 
	 *	Best LOD that was 'predicted' by UpdateSkelPose. Copied form USkeletalMeshComponent.
	 *	This is what bones were updated based on, so we do not allow rendering at a better LOD than this. 
	 */
	int32 PredictedLODLevel;

	/** Internal random stream */
	FRandomStream RandomStream;

	/** Animation Notifies that has been triggered in the latest tick **/
	TArray<const struct FAnimNotifyEvent *> AnimNotifies;
};
