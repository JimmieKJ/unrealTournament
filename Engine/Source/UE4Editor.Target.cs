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
	}

	public override void SetupGlobalEnvironment(
		TargetInfo Target,
		ref LinkEnvironmentConfiguration OutLinkEnvironmentConfiguration,
		ref CPPEnvironmentConfiguration OutCPPEnvironmentConfiguration
		)
	{
	}

	public override bool ShouldUseSharedBuildEnvironment(TargetInfo Target)
	{
		return true;
	}

	public override void GetModulesToPrecompile(TargetInfo Target, List<string> ModuleNames)
	{
		ModuleNames.Add("Launch");
		ModuleNames.Add("GameMenuBuilder");
		if ((Target.Platform == UnrealTargetPlatform.Win32) || (Target.Platform == UnrealTargetPlatform.Win64))
		{
			ModuleNames.Add("OnlineSubsystemNull");
			ModuleNames.Add("OnlineSubsystemAmazon");
			if (UEBuildConfiguration.bCompileSteamOSS == true)
			{
				ModuleNames.Add("OnlineSubsystemSteam");
			}
			ModuleNames.Add("OnlineSubsystemFacebook");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.Linux)
		{
			ModuleNames.Add("OnlineSubsystemNull");
			if (UEBuildConfiguration.bCompileSteamOSS == true)
			{
				ModuleNames.Add("OnlineSubsystemSteam");
			}
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

        #region Common Configurations

        //DESKTOP PLATFORMS
        List<UnrealTargetPlatform> DesktopPlats = null;
        if (HostPlatform == UnrealTargetPlatform.Win64)
        {
            DesktopPlats = new List<UnrealTargetPlatform> { HostPlatform, UnrealTargetPlatform.Linux, UnrealTargetPlatform.Win32 };
        }
        else
        {
            DesktopPlats = new List<UnrealTargetPlatform> { HostPlatform };
        }
        //MOBILE PLATFORMS
		//List<UnrealTargetPlatform> MobilePlats = null;
		//if (HostPlatform == UnrealTargetPlatform.Mac)
		//{
		//	MobilePlats = new List<UnrealTargetPlatform> { HostPlatform, UnrealTargetPlatform.IOS };
		//}
		//else
		//{
		//	MobilePlats = new List<UnrealTargetPlatform> { HostPlatform, UnrealTargetPlatform.Android };
		//}

        //DESKTOP AND MOBILE PLATFORMS
        List<UnrealTargetPlatform> DesktopAndMobilePlats = null;
        if (HostPlatform == UnrealTargetPlatform.Mac)
        {
            DesktopAndMobilePlats = new List<UnrealTargetPlatform> { HostPlatform, UnrealTargetPlatform.IOS };
        }
        else
        {
            DesktopAndMobilePlats = new List<UnrealTargetPlatform> { HostPlatform, UnrealTargetPlatform.Android, UnrealTargetPlatform.Win32 };
        }

        //ALL SUPPORTED PLATFORMS
        List<UnrealTargetPlatform> AllSupportedPlats = null;
        if (HostPlatform == UnrealTargetPlatform.Mac)
        {
            AllSupportedPlats = new List<UnrealTargetPlatform> { HostPlatform, UnrealTargetPlatform.IOS };
        }
        else
        {
            AllSupportedPlats = new List<UnrealTargetPlatform> { HostPlatform, UnrealTargetPlatform.Android, UnrealTargetPlatform.HTML5, UnrealTargetPlatform.Win32, UnrealTargetPlatform.XboxOne, UnrealTargetPlatform.PS4 };
        }
        #endregion

        #region Sample Specific Platform Configurations

        //ELEMENTAL DEMO
        List<UnrealTargetPlatform> ElementalDemoSupportedPlats = null;
        if (HostPlatform == UnrealTargetPlatform.Mac)
        {
            ElementalDemoSupportedPlats = new List<UnrealTargetPlatform> { HostPlatform };
        }
        else
        {
            ElementalDemoSupportedPlats = new List<UnrealTargetPlatform> { HostPlatform, UnrealTargetPlatform.Win32, UnrealTargetPlatform.XboxOne, UnrealTargetPlatform.PS4 };
        }

        //TAPPY CHICKEN
        List<UnrealTargetPlatform> TappyChickenPlats = null;
        if (HostPlatform == UnrealTargetPlatform.Mac)
        {
            TappyChickenPlats = new List<UnrealTargetPlatform> {};
        }
        else
        {
            TappyChickenPlats = new List<UnrealTargetPlatform> { HostPlatform, UnrealTargetPlatform.Android, UnrealTargetPlatform.HTML5 };
        }

		List<UnrealTargetPlatform> TP_2DSideScrollerPlats = null;
		if (HostPlatform == UnrealTargetPlatform.Mac)
        {
			TP_2DSideScrollerPlats = new List<UnrealTargetPlatform> { HostPlatform };
        }
        else
        {
			TP_2DSideScrollerPlats = new List<UnrealTargetPlatform> { HostPlatform, UnrealTargetPlatform.Android, UnrealTargetPlatform.HTML5, UnrealTargetPlatform.Win32, UnrealTargetPlatform.XboxOne, UnrealTargetPlatform.PS4 };
        }
        //TP_PUZZLE
        List<UnrealTargetPlatform> TPPuzzlePlats = null;
        if (HostPlatform == UnrealTargetPlatform.Mac)
        {
            TPPuzzlePlats = new List<UnrealTargetPlatform> { HostPlatform, UnrealTargetPlatform.IOS };
        }
        else
        {
            TPPuzzlePlats = new List<UnrealTargetPlatform> { HostPlatform, UnrealTargetPlatform.Android, UnrealTargetPlatform.HTML5, UnrealTargetPlatform.Win32 };
        }
        //TP_TOPDOWN
        List<UnrealTargetPlatform> TPTopDownPlats = null;
        if (HostPlatform == UnrealTargetPlatform.Mac)
        {
            TPTopDownPlats = new List<UnrealTargetPlatform> { HostPlatform, UnrealTargetPlatform.IOS };
        }
        else
        {
            TPTopDownPlats = new List<UnrealTargetPlatform> { HostPlatform, UnrealTargetPlatform.Android, UnrealTargetPlatform.HTML5, UnrealTargetPlatform.Win32 };
        }
        //TP_TWINSTICK
        List<UnrealTargetPlatform> TPTwinStickPlats = null;
        if (HostPlatform == UnrealTargetPlatform.Mac)
        {
            TPTwinStickPlats = new List<UnrealTargetPlatform> { HostPlatform, UnrealTargetPlatform.IOS };
        }
        else
        {
            TPTwinStickPlats = new List<UnrealTargetPlatform> { HostPlatform, UnrealTargetPlatform.Android, UnrealTargetPlatform.Win32, UnrealTargetPlatform.XboxOne, UnrealTargetPlatform.PS4 };
        }
        #endregion

        NonCodeProjectNames.Add("ElementalDemo", ElementalDemoSupportedPlats);
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

        NonCodeProjectNames.Add("BlackJack", DesktopAndMobilePlats);
        NonCodeProjectNames.Add("MemoryGame", DesktopAndMobilePlats);
        NonCodeProjectNames.Add("TappyChicken", TappyChickenPlats);
        NonCodeProjectNames.Add("SwingNinja", DesktopAndMobilePlats);
        NonCodeProjectNames.Add("SunTemple", DesktopAndMobilePlats);

        NonCodeProjectNames.Add("FP_FirstPersonBP", AllSupportedPlats);
		NonCodeProjectNames.Add("TP_2DSideScrollerBP", TP_2DSideScrollerPlats);
        NonCodeProjectNames.Add("TP_FirstPersonBP", AllSupportedPlats);
        NonCodeProjectNames.Add("TP_FlyingBP", AllSupportedPlats);
        NonCodeProjectNames.Add("TP_PuzzleBP", TPPuzzlePlats);
        NonCodeProjectNames.Add("TP_RollingBP", AllSupportedPlats);
        NonCodeProjectNames.Add("TP_SideScrollerBP", AllSupportedPlats);
        NonCodeProjectNames.Add("TP_ThirdPersonBP", AllSupportedPlats);
        NonCodeProjectNames.Add("TP_TopDownBP", TPTopDownPlats);
        NonCodeProjectNames.Add("TP_TwinStickBP", TPTwinStickPlats);
        NonCodeProjectNames.Add("TP_VehicleBP", AllSupportedPlats);
        NonCodeProjectNames.Add("TP_VehicleAdvBP", AllSupportedPlats);

        return NonCodeProjectNames;
    }
    public override Dictionary<string, List<GUBPFormalBuild>> GUBP_GetNonCodeFormalBuilds_BaseEditorTypeOnly()
    {
        #region Sample Configurations
        //Generic Configurations
        var PCBuildSettings = new List<GUBPFormalBuild>
            {
                    new GUBPFormalBuild(UnrealTargetPlatform.Win32, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.Win64, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.Mac, UnrealTargetConfiguration.Test, false),
            };

		//var MobileBuildSettings = new List<GUBPFormalBuild>
		//	{
		//			new GUBPFormalBuild(UnrealTargetPlatform.Android, UnrealTargetConfiguration.Test, false),
		//			new GUBPFormalBuild(UnrealTargetPlatform.IOS, UnrealTargetConfiguration.Test, false),
		//	};
        // Non-Code Sample Configurations
        var BlackJackBuildSettings = new List<GUBPFormalBuild>
            {
                    new GUBPFormalBuild(UnrealTargetPlatform.Win32, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.Win64, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.Mac, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.Android, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.IOS, UnrealTargetConfiguration.Test, false),
            };
        
        var MemoryGameBuildSettings = new List<GUBPFormalBuild>
            {
                    new GUBPFormalBuild(UnrealTargetPlatform.Win32, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.Win64, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.Mac, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.Android, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.IOS, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.HTML5, UnrealTargetConfiguration.Test, false),
            };
        
        var SwingNinjaBuildSettings = new List<GUBPFormalBuild>
            {
                    new GUBPFormalBuild(UnrealTargetPlatform.Win32, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.Win64, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.Mac, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.Android, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.IOS, UnrealTargetConfiguration.Test, false),
            };
        
        var SunTempleBuildSettings = new List<GUBPFormalBuild>
            {
                    new GUBPFormalBuild(UnrealTargetPlatform.Win32, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.Win64, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.Mac, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.Android, UnrealTargetConfiguration.Test, false, true),
                    new GUBPFormalBuild(UnrealTargetPlatform.IOS, UnrealTargetConfiguration.Test, false, true),
                    new GUBPFormalBuild(UnrealTargetPlatform.HTML5, UnrealTargetConfiguration.Test, false),
            };

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

        var TappyChickenBuildSettings = new List<GUBPFormalBuild>
            {
                    new GUBPFormalBuild(UnrealTargetPlatform.Android, UnrealTargetConfiguration.Shipping, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.Android, UnrealTargetConfiguration.Test, false, true),                    
                    new GUBPFormalBuild(UnrealTargetPlatform.Android, UnrealTargetConfiguration.Development, false),        			
					new GUBPFormalBuild(UnrealTargetPlatform.HTML5, UnrealTargetConfiguration.Shipping, false),
					new GUBPFormalBuild(UnrealTargetPlatform.HTML5, UnrealTargetConfiguration.Test, false),
            };
        #endregion

        #region Template Configurations
        // Template Build Configurations
        // DEFAULT 
        var AllBuildSettings = new List<GUBPFormalBuild>
            {                                     
                    new GUBPFormalBuild(UnrealTargetPlatform.IOS, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.Android, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.HTML5, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.Win32, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.Win64, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.Mac, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.XboxOne, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.PS4, UnrealTargetConfiguration.Test, false),
            };

        //TP_PUZZLE
        var TP_PuzzleBuildSettings = new List<GUBPFormalBuild>
            {
                    new GUBPFormalBuild(UnrealTargetPlatform.IOS, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.Android, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.HTML5, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.Win32, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.Win64, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.Mac, UnrealTargetConfiguration.Test, false),
            };

        //TP_TOPDOWN
        var TP_TopDownBuildSettings = new List<GUBPFormalBuild>
            {
                    new GUBPFormalBuild(UnrealTargetPlatform.IOS, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.Android, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.HTML5, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.Win32, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.Win64, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.Mac, UnrealTargetConfiguration.Test, false),
            };

        //TP_TWINSTICK
        var TP_TwinStickBuildSettings = new List<GUBPFormalBuild>
            {
                    new GUBPFormalBuild(UnrealTargetPlatform.IOS, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.Android, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.Win32, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.Win64, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.Mac, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.XboxOne, UnrealTargetConfiguration.Test, false),
                    new GUBPFormalBuild(UnrealTargetPlatform.PS4, UnrealTargetConfiguration.Test, false),
            };
        #endregion

        var NonCodeProjectNames = new Dictionary<string, List<GUBPFormalBuild>>();

        //Add Samples to the list with its corresponding settings
        NonCodeProjectNames.Add("TappyChicken", TappyChickenBuildSettings);    
        NonCodeProjectNames.Add("BlackJack", BlackJackBuildSettings);
        NonCodeProjectNames.Add("MemoryGame", MemoryGameBuildSettings);
        NonCodeProjectNames.Add("SwingNinja", SwingNinjaBuildSettings);
        NonCodeProjectNames.Add("SunTemple", SunTempleBuildSettings);
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
        NonCodeProjectNames.Add("ElementalDemo", ElementalBuildSettings);

        //Add Templates to the list with its corresponding settings
        NonCodeProjectNames.Add("FP_FirstPersonBP", AllBuildSettings);
		NonCodeProjectNames.Add("TP_2DSideScrollerBP", AllBuildSettings);
        NonCodeProjectNames.Add("TP_FirstPersonBP", AllBuildSettings);
        NonCodeProjectNames.Add("TP_FlyingBP", AllBuildSettings);
        NonCodeProjectNames.Add("TP_PuzzleBP", TP_PuzzleBuildSettings);
        NonCodeProjectNames.Add("TP_RollingBP", AllBuildSettings);
        NonCodeProjectNames.Add("TP_SideScrollerBP", AllBuildSettings);
        NonCodeProjectNames.Add("TP_ThirdPersonBP", AllBuildSettings);
        NonCodeProjectNames.Add("TP_TopDownBP", TP_TopDownBuildSettings);
        NonCodeProjectNames.Add("TP_TwinStickBP", TP_TwinStickBuildSettings);
        NonCodeProjectNames.Add("TP_VehicleBP", AllBuildSettings);
        NonCodeProjectNames.Add("TP_VehicleAdvBP", AllBuildSettings);


        return NonCodeProjectNames;
    }
}
