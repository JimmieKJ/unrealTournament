namespace EGenericAnalyticParam
{
	enum Type
	{
		MatchTime,
		MapName,

		Hostname,
		SystemId,
		InstanceId,
		BuildVersion,
		MachinePhysicalRAMInGB,
		NumLogicalCoresAvailable,
		NumPhysicalCoresAvailable,
		MachineCPUSignature,
		MachineCPUBrandName,
		NumClients,

		Platform,
		Location,
		SocialPartyCount,
		IsIdle,
		RegionId,
		PlayerContextLocationPerMinute,

		HitchThresholdInMs,
		NumHitchesAboveThreshold,
		TotalUnplayableTimeInMs,
		ServerUnplayableCondition,

		UTFPSCharts,
		UTServerFPSCharts,

		Team,
		MaxRequiredTextureSize,

		NUM_GENERIC_PARAMS
	};
}