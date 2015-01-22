// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class UE4EditorTarget : TargetRules
{
	public UE4EditorTarget( TargetInfo Target )
	{
		Type = TargetType.Editor;
	}

	public override void SetupBinaries(
		TargetInfo Target,
		ref List<UEBuildBinaryConfiguration> OutBuildBinaryConfigurations,
		ref List<string> OutExtraModuleNames
		)
	{
		OutExtraModuleNames.Add("UE4Game");
		OutExtraModuleNames.Add("GameMenuBuilder");
		if ((Target.Platform == UnrealTargetPlatform.Win32) || (Target.Platform == UnrealTargetPlatform.Win64))
		{
			OutExtraModuleNames.Add("OnlineSubsystemNull");
			OutExtraModuleNames.Add("OnlineSubsystemAmazon");
			if (UEBuildConfiguration.bCompileSteamOSS == true)
			{
				OutExtraModuleNames.Add("OnlineSubsystemSteam");
			}
			OutExtraModuleNames.Add("OnlineSubsystemFacebook");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.Linux)
		{
			OutExtraModuleNames.Add("OnlineSubsystemNull");
			if (UEBuildConfiguration.bCompileSteamOSS == true)
			{
				OutExtraModuleNames.Add("OnlineSubsystemSteam");
			}
		}
	}

	public override void SetupGlobalEnvironment(
		TargetInfo Target,
		ref LinkEnvironmentConfiguration OutLinkEnvironmentConfiguration,
		ref CPPEnvironmentConfiguration OutCPPEnvironmentConfiguration
		)
	{
		if( UnrealBuildTool.UnrealBuildTool.BuildingRocket() )
		{ 
			// no exports, so no need to verify that a .lib and .exp file was emitted by the linker.
			OutLinkEnvironmentConfiguration.bHasExports = false;
		}
	}
    public override GUBPProjectOptions GUBP_IncludeProjectInPromotedBuild_EditorTypeOnly(UnrealTargetPlatform HostPlatform)
    {
        var Result = new GUBPProjectOptions();
        Result.bIsPromotable = true;
        return Result;
    }
    public override Dictionary<string, List<UnrealTargetPlatform>> GUBP_NonCodeProjects_BaseEditorTypeOnly(UnrealTargetPlatform HostPlatform)
    {
        var NonCodeProjectNames = new Dictionary<string, List<UnrealTargetPlatform>>();

        List<UnrealTargetPlatform> DesktopPlats = null;
        if (HostPlatform == UnrealTargetPlatform.Win64)
        {
            DesktopPlats = new List<UnrealTargetPlatform> { HostPlatform, UnrealTargetPlatform.Linux };
        }
        else
        {
            DesktopPlats = new List<UnrealTargetPlatform> { HostPlatform };
        }

        NonCodeProjectNames.Add("ElementalDemo", DesktopPlats);
        //NonCodeProjectNames.Add("InfiltratorDemo", DesktopPlats);
        NonCodeProjectNames.Add("HoverShip", DesktopPlats);
        NonCodeProjectNames.Add("BlueprintOffice", DesktopPlats);
        NonCodeProjectNames.Add("ReflectionsSubway", DesktopPlats);
        NonCodeProjectNames.Add("ElementalVR", DesktopPlats);
        NonCodeProjectNames.Add("CouchKnights", DesktopPlats);
        NonCodeProjectNames.Add("Stylized", DesktopPlats);
        //NonCodeProjectNames.Add("Landscape", new List<UnrealTargetPlatform> { HostPlatform });
        NonCodeProjectNames.Add("MatineeFightScene", DesktopPlats);
        NonCodeProjectNames.Add("RealisticRendering", DesktopPlats);
        NonCodeProjectNames.Add("EffectsCave", DesktopPlats);
        NonCodeProjectNames.Add("GDC2014", DesktopPlats);
        NonCodeProjectNames.Add("ContentExamples", DesktopPlats);
        // NonCodeProjectNames.Add("PhysicsPirateShip", DesktopPlats);
        NonCodeProjectNames.Add("TowerDefenseGame", DesktopPlats);
        NonCodeProjectNames.Add("LandscapeMountains", DesktopPlats);
        NonCodeProjectNames.Add("MorphTargets", DesktopPlats);
        NonCodeProjectNames.Add("PostProcessMatinee", DesktopPlats);
        NonCodeProjectNames.Add("SciFiHallway", DesktopPlats);

        List<UnrealTargetPlatform> MobilePlats = null;
        if (HostPlatform == UnrealTargetPlatform.Mac)
        {
            MobilePlats = new List<UnrealTargetPlatform> { HostPlatform, UnrealTargetPlatform.IOS };
        }
        else
        {
            MobilePlats = new List<UnrealTargetPlatform> { HostPlatform, UnrealTargetPlatform.Android, UnrealTargetPlatform.IOS };
        }

		List<UnrealTargetPlatform> TappyChickenPlats = null;
		if (HostPlatform == UnrealTargetPlatform.Mac)
		{
			TappyChickenPlats = new List<UnrealTargetPlatform> { HostPlatform, UnrealTargetPlatform.IOS };
		}
		else
		{
			TappyChickenPlats = new List<UnrealTargetPlatform> { HostPlatform, UnrealTargetPlatform.Android, UnrealTargetPlatform.IOS, UnrealTargetPlatform.HTML5 };
		}

        NonCodeProjectNames.Add("BlackJack", MobilePlats);
        NonCodeProjectNames.Add("MemoryGame", MobilePlats);
		NonCodeProjectNames.Add("TappyChicken", TappyChickenPlats);
        NonCodeProjectNames.Add("SwingNinja", MobilePlats);
        NonCodeProjectNames.Add("SunTemple", MobilePlats);

        NonCodeProjectNames.Add("StarterContent", MobilePlats);
		NonCodeProjectNames.Add("TP_2DSideScrollerBP", MobilePlats);
        NonCodeProjectNames.Add("TP_FirstPersonBP", MobilePlats);
        NonCodeProjectNames.Add("TP_FlyingBP", MobilePlats);
        NonCodeProjectNames.Add("TP_RollingBP", MobilePlats);
        NonCodeProjectNames.Add("TP_SideScrollerBP", MobilePlats);
        NonCodeProjectNames.Add("TP_ThirdPersonBP", MobilePlats);
        NonCodeProjectNames.Add("TP_TopDownBP", MobilePlats);
        NonCodeProjectNames.Add("TP_VehicleBP", MobilePlats);

        return NonCodeProjectNames;
    }
    public override Dictionary<string, List<GUBPFormalBuild>> GUBP_GetNonCodeFormalBuilds_BaseEditorTypeOnly()
    {
        var TappyChickenBuildSettings = new List<GUBPFormalBuild>
            {
                    new GUBPFormalBuild(UnrealTargetPlatform.Android, UnrealTargetConfiguration.Shipping, true),
                    new GUBPFormalBuild(UnrealTargetPlatform.Android, UnrealTargetConfiguration.Test, true),                    
                    new GUBPFormalBuild(UnrealTargetPlatform.Android, UnrealTargetConfiguration.Development, true),          
                    new GUBPFormalBuild(UnrealTargetPlatform.IOS, UnrealTargetConfiguration.Shipping, true),
                    new GUBPFormalBuild(UnrealTargetPlatform.IOS, UnrealTargetConfiguration.Test, true),
                    new GUBPFormalBuild(UnrealTargetPlatform.IOS, UnrealTargetConfiguration.Development, true),          			
					new GUBPFormalBuild(UnrealTargetPlatform.HTML5, UnrealTargetConfiguration.Shipping, false),
					new GUBPFormalBuild(UnrealTargetPlatform.HTML5, UnrealTargetConfiguration.Test, false),
            };

        var NonCodeProjectNames = new Dictionary<string, List<GUBPFormalBuild>>();
        NonCodeProjectNames.Add("TappyChicken", TappyChickenBuildSettings);



        var MobileBuildSettings = new List<GUBPFormalBuild>
            {
                    new GUBPFormalBuild(UnrealTargetPlatform.Android, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.IOS, UnrealTargetConfiguration.Test, false),
            };

        NonCodeProjectNames.Add("BlackJack", MobileBuildSettings);
        NonCodeProjectNames.Add("MemoryGame", MobileBuildSettings);
        NonCodeProjectNames.Add("SwingNinja", MobileBuildSettings);
        NonCodeProjectNames.Add("SunTemple", MobileBuildSettings);

        var PCBuildSettings = new List<GUBPFormalBuild>
            {
                    new GUBPFormalBuild(UnrealTargetPlatform.Win32, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.Win64, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.Mac, UnrealTargetConfiguration.Test, false),
            };

        NonCodeProjectNames.Add("BlueprintOffice", PCBuildSettings);
        NonCodeProjectNames.Add("ContentExamples", PCBuildSettings);
        NonCodeProjectNames.Add("CouchKnights", PCBuildSettings);
        NonCodeProjectNames.Add("EffectsCave", PCBuildSettings);
        NonCodeProjectNames.Add("LandscapeMountains", PCBuildSettings);
        NonCodeProjectNames.Add("GDC2014", PCBuildSettings);
        NonCodeProjectNames.Add("MatineeFightScene", PCBuildSettings);
        NonCodeProjectNames.Add("RealisticRendering", PCBuildSettings);
        NonCodeProjectNames.Add("ReflectionsSubway", PCBuildSettings);
        NonCodeProjectNames.Add("Stylized", PCBuildSettings);


        var ElementalBuildSettings = new List<GUBPFormalBuild>
            {
                    new GUBPFormalBuild(UnrealTargetPlatform.Win32, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.Win64, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.Mac, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.XboxOne, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.PS4, UnrealTargetConfiguration.Test, false),
                    
                    new GUBPFormalBuild(UnrealTargetPlatform.XboxOne, UnrealTargetConfiguration.Shipping, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.PS4, UnrealTargetConfiguration.Shipping, false),
                    
                    new GUBPFormalBuild(UnrealTargetPlatform.XboxOne, UnrealTargetConfiguration.Development, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.PS4, UnrealTargetConfiguration.Development, false),
            };


        NonCodeProjectNames.Add("ElementalDemo", ElementalBuildSettings);



        return NonCodeProjectNames;
    }
}
