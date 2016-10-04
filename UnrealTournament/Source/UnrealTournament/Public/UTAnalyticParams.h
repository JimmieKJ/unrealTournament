namespace EGenericAnalyticParam
{
	enum Type
	{
		MatchTime,
		MapName,
		GameModeName,

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

		WeaponName,
		NumKills,
		UTServerWeaponKills,

		UTFPSCharts,
		UTServerFPSCharts,

		Team,
		MaxRequiredTextureSize,
		QuickMatch,

		FlagRunRoundEnd,
		OffenseKills,
		DefenseKills,
		DefenseLivesRemaining,
		DefensePlayersEliminated,
		PointsScored,
		DefenseWin,
		TimeRemaining,
		RoundNumber,
		FinalRound,
		EndedInTieBreaker,
		RedTeamBonusTime,
		BlueTeamBonusTime,

		UTEnterMatch,
		EnterMethod,

		UTTutorialPickupToken,
		TokenID,
		AnnouncementID,
		UTTutorialPlayInstruction,

		NUM_GENERIC_PARAMS
	};
}