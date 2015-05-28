// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

namespace FEQSHelpers
{
	enum class ETraceMode : uint8
	{
		Keep,
		Discard,
	};

	struct FBatchTrace
	{
		UWorld* World;
		const FVector Extent;
		const FCollisionQueryParams Params;
		enum ECollisionChannel Channel;
		ETraceMode TraceMode;

		FBatchTrace(UWorld* InWorld, enum ECollisionChannel InChannel, const FCollisionQueryParams& InParams,
			const FVector& InExtent, ETraceMode InTraceMode)
			: World(InWorld), Extent(InExtent), Params(InParams), Channel(InChannel), TraceMode(InTraceMode)
		{

		}

		FORCEINLINE_DEBUGGABLE bool RunLineTrace(const FVector& StartPos, const FVector& EndPos, FVector& HitPos)
		{
			FHitResult OutHit;
			const bool bHit = World->LineTraceSingleByChannel(OutHit, StartPos, EndPos, Channel, Params);
			HitPos = OutHit.ImpactPoint;
			return bHit;
		}

		FORCEINLINE_DEBUGGABLE bool RunSphereTrace(const FVector& StartPos, const FVector& EndPos, FVector& HitPos)
		{
			FHitResult OutHit;
			const bool bHit = World->SweepSingleByChannel(OutHit, StartPos, EndPos, FQuat::Identity, Channel, FCollisionShape::MakeSphere(Extent.X), Params);
			HitPos = OutHit.ImpactPoint;
			return bHit;
		}

		FORCEINLINE_DEBUGGABLE bool RunCapsuleTrace(const FVector& StartPos, const FVector& EndPos, FVector& HitPos)
		{
			FHitResult OutHit;
			const bool bHit = World->SweepSingleByChannel(OutHit, StartPos, EndPos, FQuat::Identity, Channel, FCollisionShape::MakeCapsule(Extent.X, Extent.Z), Params);
			HitPos = OutHit.ImpactPoint;
			return bHit;
		}

		FORCEINLINE_DEBUGGABLE bool RunBoxTrace(const FVector& StartPos, const FVector& EndPos, FVector& HitPos)
		{
			FHitResult OutHit;
			const bool bHit = World->SweepSingleByChannel(OutHit, StartPos, EndPos, FQuat((EndPos - StartPos).Rotation()), Channel, FCollisionShape::MakeBox(Extent), Params);
			HitPos = OutHit.ImpactPoint;
			return bHit;
		}

		template<EEnvTraceShape::Type TraceType>
		void DoSingleSourceMultiDestinations(const FVector& Source, TArray<FNavLocation>& Points)
		{
			UE_LOG(LogEQS, Error, TEXT("FBatchTrace::DoSingleSourceMultiDestinations called with unhandled trace type: %d"), int32(TraceType));
		}

		template<EEnvTraceShape::Type TraceType>
		void DoProject(TArray<FNavLocation>& Points, float StartOffsetZ, float EndOffsetZ, float HitOffsetZ)
		{
			UE_LOG(LogEQS, Error, TEXT("FBatchTrace::DoSingleSourceMultiDestinations called with unhandled trace type: %d"), int32(TraceType));
		}
	};

	template<>
	void FBatchTrace::DoSingleSourceMultiDestinations<EEnvTraceShape::Line>(const FVector& Source, TArray<FNavLocation>& Points);

	template<>
	void FBatchTrace::DoSingleSourceMultiDestinations<EEnvTraceShape::Box>(const FVector& Source, TArray<FNavLocation>& Points);

	template<>
	void FBatchTrace::DoSingleSourceMultiDestinations<EEnvTraceShape::Sphere>(const FVector& Source, TArray<FNavLocation>& Points);

	template<>
	void FBatchTrace::DoSingleSourceMultiDestinations<EEnvTraceShape::Capsule>(const FVector& Source, TArray<FNavLocation>& Points);

	template<>
	void FBatchTrace::DoProject<EEnvTraceShape::Line>(TArray<FNavLocation>& Points, float StartOffsetZ, float EndOffsetZ, float HitOffsetZ);

	template<>
	void FBatchTrace::DoProject<EEnvTraceShape::Box>(TArray<FNavLocation>& Points, float StartOffsetZ, float EndOffsetZ, float HitOffsetZ);

	template<>
	void FBatchTrace::DoProject<EEnvTraceShape::Sphere>(TArray<FNavLocation>& Points, float StartOffsetZ, float EndOffsetZ, float HitOffsetZ);

	template<>
	void FBatchTrace::DoProject<EEnvTraceShape::Capsule>(TArray<FNavLocation>& Points, float StartOffsetZ, float EndOffsetZ, float HitOffsetZ);

	void RunNavRaycasts(const ANavigationData& NavData, const FEnvTraceData& TraceData, const FVector& SourcePt, TArray<FNavLocation>& Points, ETraceMode TraceMode = ETraceMode::Keep);
	void RunNavProjection(const ANavigationData& NavData, const FEnvTraceData& TraceData, TArray<FNavLocation>& Points, ETraceMode TraceMode = ETraceMode::Discard);
	void RunPhysRaycasts(UWorld* World, const FEnvTraceData& TraceData, const FVector& SourcePt, TArray<FNavLocation>& Points, ETraceMode TraceMode = ETraceMode::Keep);
	void RunPhysProjection(UWorld* World, const FEnvTraceData& TraceData, TArray<FNavLocation>& Points, ETraceMode TraceMode = ETraceMode::Discard);

	// deprecated

	DEPRECATED(4.8, "This version of RunNavRaycasts is deprecated. Please use ANavigationData reference rather than a pointer version")
	FORCEINLINE void RunNavRaycasts(const ANavigationData* NavData, const FEnvTraceData& TraceData, const FVector& SourcePt, TArray<FNavLocation>& Points, ETraceMode TraceMode = ETraceMode::Keep)
	{
		if (NavData)
		{
			RunNavRaycasts(*NavData, TraceData, SourcePt, Points, TraceMode);
		}
	}

	DEPRECATED(4.8, "This version of RunNavProjection is deprecated. Please use ANavigationData reference rather than a pointer version")
	FORCEINLINE void RunNavProjection(const ANavigationData* NavData, const FEnvTraceData& TraceData, TArray<FNavLocation>& Points, ETraceMode TraceMode = ETraceMode::Discard)
	{
		if (NavData)
		{
			RunNavProjection(*NavData, TraceData, Points, TraceMode);
		}
	}
}
