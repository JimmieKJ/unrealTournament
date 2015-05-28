// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#define LAUNCHERSERVICES_ADDEDINCREMENTALDEPLOYVERSION 11
#define LAUNCHERSERVICES_ADDEDPATCHSOURCECONTENTPATH 12
#define LAUNCHERSERVICES_ADDEDRELEASEVERSION 13
#define LAUNCHERSERVICES_REMOVEDPATCHSOURCECONTENTPATH 14
#define LAUNCHERSERVICES_ADDEDDLCINCLUDEENGINECONTENT 15
#define LAUNCHERSERVICES_ADDEDGENERATECHUNKS 16

/**
* Implements a simple profile which controls the desired output of the Launcher for simple
*/
class FLauncherSimpleProfile
	: public ILauncherSimpleProfile
{
public:

	FLauncherSimpleProfile(const FString& InDeviceName)
		: DeviceName(InDeviceName)
	{
		SetDefaults();
	}

	// Begin ILauncherSimpleProfile interface

	virtual const FString& GetDeviceName() const override
	{
		return DeviceName;
	}

	virtual FName GetDeviceVariant() const  override
	{
		return Variant;
	}

	virtual EBuildConfigurations::Type GetBuildConfiguration() const override
	{
		return BuildConfiguration;
	}

	virtual ELauncherProfileCookModes::Type GetCookMode() const override
	{
		return CookMode;
	}

	virtual void SetDeviceName(const FString& InDeviceName) override
	{
		if (DeviceName != InDeviceName)
		{
			DeviceName = InDeviceName;
		}
	}

	virtual void SetDeviceVariant(FName InVariant) override
	{
		Variant = InVariant;
	}

	virtual void SetBuildConfiguration(EBuildConfigurations::Type InConfiguration) override
	{
		BuildConfiguration = InConfiguration;
	}

	virtual void SetCookMode(ELauncherProfileCookModes::Type InMode) override
	{
		CookMode = InMode;
	}

	virtual bool Serialize(FArchive& Archive) override
	{
		int32 Version = LAUNCHERSERVICES_SIMPLEPROFILEVERSION;

		Archive << Version;

		if (Version != LAUNCHERSERVICES_SIMPLEPROFILEVERSION)
		{
			return false;
		}

		// IMPORTANT: make sure to bump LAUNCHERSERVICES_SIMPLEPROFILEVERSION when modifying this!
		Archive << DeviceName
			<< Variant
			<< BuildConfiguration
			<< CookMode;

		return true;
	}

	virtual void SetDefaults() override
	{
		// None will mean the preferred Variant for the device is used.
		Variant = NAME_None;
		
		// I don't use FApp::GetBuildConfiguration() because i don't want the act of running in debug the first time to cause the simple
		// profiles created for your persistent devices to be in debug. The user might not see this if they don't expand the Advanced options.
		BuildConfiguration = EBuildConfigurations::Development;
		
		CookMode = ELauncherProfileCookModes::OnTheFly;
	}

private:

	// Holds the name of the device this simple profile is for
	FString DeviceName;

	// Holds the name of the device variant.
	FName Variant;

	// Holds the desired build configuration (only used if creating new builds).
	TEnumAsByte<EBuildConfigurations::Type> BuildConfiguration;

	// Holds the cooking mode.
	TEnumAsByte<ELauncherProfileCookModes::Type> CookMode;
};


/**
 * Implements a profile which controls the desired output of the Launcher
 */
class FLauncherProfile
	: public ILauncherProfile
{
public:

	/**
	 * Default constructor.
	 */
	FLauncherProfile(ILauncherProfileManagerRef ProfileManager)
		: LauncherProfileManager(ProfileManager)
		, DefaultLaunchRole(MakeShareable(new FLauncherProfileLaunchRole()))
	{ }

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InProfileName - The name of the profile.
	 */
	FLauncherProfile(ILauncherProfileManagerRef ProfileManager, FGuid InId, const FString& InProfileName)
		: LauncherProfileManager(ProfileManager)
		, DefaultLaunchRole(MakeShareable(new FLauncherProfileLaunchRole()))
		, Id(InId)
		, Name(InProfileName)
	{
		SetDefaults();
	}

	/**
	 * Destructor.
	 */
	~FLauncherProfile( ) { }

public:

	/**
	 * Gets the identifier of the device group to deploy to.
	 *
	 * This method is used internally by the profile manager to read the device group identifier after
	 * loading this profile from a file. The profile manager will use this identifier to locate the
	 * actual device group to deploy to.
	 *
	 * @return The device group identifier, or an invalid GUID if no group was set or deployment is disabled.
	 */
	const FGuid& GetDeployedDeviceGroupId( ) const
	{
		return DeployedDeviceGroupId;
	}

public:

	// Begin ILauncherProfile interface

	virtual void AddCookedCulture( const FString& CultureName ) override
	{
		CookedCultures.AddUnique(CultureName);

		Validate();
	}

	virtual void AddCookedMap( const FString& MapName ) override
	{
		CookedMaps.AddUnique(MapName);

		Validate();
	}

	virtual void AddCookedPlatform( const FString& PlatformName ) override
	{
		CookedPlatforms.AddUnique(PlatformName);

		Validate();
	}

	virtual void ClearCookedCultures( ) override
	{
		if (CookedCultures.Num() > 0)
		{
			CookedCultures.Reset();

			Validate();
		}
	}

	virtual void ClearCookedMaps( ) override
	{
		if (CookedMaps.Num() > 0)
		{
			CookedMaps.Reset();

			Validate();
		}
	}

	virtual void ClearCookedPlatforms( ) override
	{
		if (CookedPlatforms.Num() > 0)
		{
			CookedPlatforms.Reset();

			Validate();
		}
	}

	virtual ILauncherProfileLaunchRolePtr CreateLaunchRole( ) override
	{
		ILauncherProfileLaunchRolePtr Role = MakeShareable(new FLauncherProfileLaunchRole());
			
		LaunchRoles.Add(Role);

		Validate();

		return Role;
	}

	virtual EBuildConfigurations::Type GetBuildConfiguration( ) const override
	{
		return BuildConfiguration;
	}

	virtual EBuildConfigurations::Type GetCookConfiguration( ) const override
	{
		return CookConfiguration;
	}

	virtual ELauncherProfileCookModes::Type GetCookMode( ) const override
	{
		return CookMode;
	}

	virtual const FString& GetCookOptions( ) const override
	{
		return CookOptions;
	}

	virtual const TArray<FString>& GetCookedCultures( ) const override
	{
		return CookedCultures;
	}

	virtual const TArray<FString>& GetCookedMaps( ) const override
	{
		return CookedMaps;
	}

	virtual const TArray<FString>& GetCookedPlatforms( ) const override
	{
		return CookedPlatforms;
	}

	virtual const ILauncherProfileLaunchRoleRef& GetDefaultLaunchRole( ) const override
	{
		return DefaultLaunchRole;
	}

	virtual ILauncherDeviceGroupPtr GetDeployedDeviceGroup( ) const override
	{
		return DeployedDeviceGroup;
	}

	virtual bool IsGeneratingPatch() const override
	{
		return GeneratePatch;
	}


	virtual bool IsCreatingDLC() const override
	{
		return CreateDLC;
	}
	virtual void SetCreateDLC(bool InBuildDLC) override
	{
		CreateDLC = InBuildDLC;
	}

	virtual FString GetDLCName() const override
	{
		return DLCName;
	}
	virtual void SetDLCName(const FString& InDLCName) override
	{
		DLCName = InDLCName;
	}

	virtual bool IsDLCIncludingEngineContent() const
	{
		return DLCIncludeEngineContent;
	}
	virtual void SetDLCIncludeEngineContent(bool InDLCIncludeEngineContent)
	{
		DLCIncludeEngineContent = InDLCIncludeEngineContent;
	}



	virtual bool IsCreatingReleaseVersion() const override
	{
		return CreateReleaseVersion;
	}

	virtual void SetCreateReleaseVersion(bool InCreateReleaseVersion) override
	{
		CreateReleaseVersion = InCreateReleaseVersion;
	}

	virtual FString GetCreateReleaseVersionName() const override
	{
		return CreateReleaseVersionName;
	}

	virtual void SetCreateReleaseVersionName(const FString& InCreateReleaseVersionName) override
	{
		CreateReleaseVersionName = InCreateReleaseVersionName;
	}


	virtual FString GetBasedOnReleaseVersionName() const override
	{
		return BasedOnReleaseVersionName;
	}

	virtual void SetBasedOnReleaseVersionName(const FString& InBasedOnReleaseVersionName) override
	{
		BasedOnReleaseVersionName = InBasedOnReleaseVersionName;
	}



	virtual ELauncherProfileDeploymentModes::Type GetDeploymentMode( ) const override
	{
		return DeploymentMode;
	}

    virtual bool GetForceClose() const override
    {
        return ForceClose;
    }
    
	virtual FGuid GetId( ) const override
	{
		return Id;
	}

	virtual ELauncherProfileLaunchModes::Type GetLaunchMode( ) const override
	{
		return LaunchMode;
	}

	virtual const TArray<ILauncherProfileLaunchRolePtr>& GetLaunchRoles( ) const override
	{
		return LaunchRoles;
	}

	virtual const int32 GetLaunchRolesFor( const FString& DeviceId, TArray<ILauncherProfileLaunchRolePtr>& OutRoles ) override
	{
		OutRoles.Empty();

		if (LaunchMode == ELauncherProfileLaunchModes::CustomRoles)
		{
			for (TArray<ILauncherProfileLaunchRolePtr>::TConstIterator It(LaunchRoles); It; ++It)
			{
				ILauncherProfileLaunchRolePtr Role = *It;

				if (Role->GetAssignedDevice() == DeviceId)
				{
					OutRoles.Add(Role);
				}
			}
		}
		else if (LaunchMode == ELauncherProfileLaunchModes::DefaultRole)
		{
			OutRoles.Add(DefaultLaunchRole);
		}

		return OutRoles.Num();
	}

	virtual FString GetName( ) const override
	{
		return Name;
	}

	virtual FString GetDescription() const override
	{
		return Description;
	}

	virtual ELauncherProfilePackagingModes::Type GetPackagingMode( ) const override
	{
		return PackagingMode;
	}

	virtual FString GetPackageDirectory( ) const override
	{
		return PackageDir;
	}

	virtual bool HasProjectSpecified() const override
	{
		return ProjectSpecified;
	}

	virtual FString GetProjectName() const override
	{
		if (ProjectSpecified)
		{
			FString Path = FLauncherProjectPath::GetProjectName(ProjectPath);
			return Path;
		}
		return LauncherProfileManager->GetProjectName();
	}

	virtual FString GetProjectBasePath() const override
	{
		if (ProjectSpecified)
		{
			return FLauncherProjectPath::GetProjectBasePath(ProjectPath);
		}
		return LauncherProfileManager->GetProjectBasePath();
	}

	virtual FString GetProjectPath() const override
	{
		if (ProjectSpecified)
		{
			return ProjectPath;
		}
		return LauncherProfileManager->GetProjectPath();
	}

    virtual uint32 GetTimeout() const override
    {
        return Timeout;
    }

	virtual bool HasValidationError() const override
	{
		return ValidationErrors.Num() > 0;
	}
    
	virtual bool HasValidationError( ELauncherProfileValidationErrors::Type Error ) const override
	{
		return ValidationErrors.Contains(Error);
	}

	virtual FString GetInvalidPlatform() const override
	{
		return InvalidPlatform;
	}

	virtual bool IsBuilding() const override
	{
		return BuildGame;
	}

	virtual bool IsBuildingUAT() const override
	{
		return BuildUAT;
	}

	virtual bool IsCookingIncrementally( ) const override
	{
		if ( CookMode != ELauncherProfileCookModes::DoNotCook )
		{
			return CookIncremental;
		}
		return false;
	}

	virtual bool IsCompressed() const override
	{
		return Compressed;
	}

	virtual bool IsCookingUnversioned( ) const override
	{
		return CookUnversioned;
	}

	virtual bool IsDeployablePlatform( const FString& PlatformName ) override
	{
		if (CookMode == ELauncherProfileCookModes::ByTheBook || CookMode == ELauncherProfileCookModes::ByTheBookInEditor)
		{
			return CookedPlatforms.Contains(PlatformName);
		}

		return true;
	}

	virtual bool IsDeployingIncrementally( ) const override
	{
		return DeployIncremental;
	}

	virtual bool IsFileServerHidden( ) const override
	{
		return HideFileServerWindow;
	}

	virtual bool IsFileServerStreaming( ) const override
	{
		return DeployStreamingServer;
	}

	virtual bool IsPackingWithUnrealPak( ) const  override
	{
		return DeployWithUnrealPak;
	}

	virtual bool IsGeneratingChunks() const override
	{
		return bGenerateChunks;
	}

	virtual bool IsGenerateHttpChunkData() const override
	{
		return bGenerateHttpChunkData;
	}

	virtual FString GetHttpChunkDataDirectory() const override
	{
		return HttpChunkDataDirectory;
	}

	virtual FString GetHttpChunkDataReleaseName() const override
	{
		return HttpChunkDataReleaseName;
	}

	virtual bool IsValidForLaunch( ) override
	{
		return (ValidationErrors.Num() == 0);
	}

	virtual void RemoveCookedCulture( const FString& CultureName ) override
	{
		CookedCultures.Remove(CultureName);

		Validate();
	}

	virtual void RemoveCookedMap( const FString& MapName ) override
	{
		CookedMaps.Remove(MapName);

		Validate();
	}

	virtual void RemoveCookedPlatform( const FString& PlatformName ) override
	{
		CookedPlatforms.Remove(PlatformName);

		Validate();
	}

	virtual void RemoveLaunchRole( const ILauncherProfileLaunchRoleRef& Role ) override
	{
		LaunchRoles.Remove(Role);

		Validate();
	}

	virtual bool Serialize( FArchive& Archive ) override
	{
		int32 Version = LAUNCHERSERVICES_PROFILEVERSION;

		Archive	<< Version;

		if (Version < LAUNCHERSERVICES_MINPROFILEVERSION)
		{
			return false;
		}

		if (Archive.IsSaving())
		{
			if (DeployedDeviceGroup.IsValid())
			{
				DeployedDeviceGroupId = DeployedDeviceGroup->GetId();
			}
			else
			{
				DeployedDeviceGroupId = FGuid();
			}
		}

		// IMPORTANT: make sure to bump LAUNCHERSERVICES_PROFILEVERSION when modifying this!
		Archive << Id
				<< Name
				<< Description
				<< BuildConfiguration
				<< ProjectSpecified
				<< ProjectPath
				<< CookConfiguration
				<< CookIncremental
				<< CookOptions
				<< CookMode
				<< CookUnversioned
				<< CookedCultures
				<< CookedMaps
				<< CookedPlatforms
				<< DeployStreamingServer
				<< DeployWithUnrealPak
				<< DeployedDeviceGroupId
				<< DeploymentMode
				<< HideFileServerWindow
				<< LaunchMode
				<< PackagingMode
				<< PackageDir
				<< BuildGame
                << ForceClose
                << Timeout;

		if (Version >= LAUNCHERSERVICES_ADDEDINCREMENTALDEPLOYVERSION)
		{
			Archive << DeployIncremental;
		}

		if ( Version >= LAUNCHERSERVICES_REMOVEDPATCHSOURCECONTENTPATH )
		{
			Archive << GeneratePatch;
		}
		else if ( Version >= LAUNCHERSERVICES_ADDEDPATCHSOURCECONTENTPATH)
		{
			FString Temp;
			Archive << Temp;
			Archive << GeneratePatch;
		}
		else if ( Archive.IsLoading() )
		{
			GeneratePatch = false;
		}

		if (Version >= LAUNCHERSERVICES_ADDEDDLCINCLUDEENGINECONTENT)
		{
			Archive << DLCIncludeEngineContent;
		}
		else if (Archive.IsLoading())
		{
			DLCIncludeEngineContent = false;
		}
		

		if ( Version >= LAUNCHERSERVICES_ADDEDRELEASEVERSION )
		{
			Archive << CreateReleaseVersion;
			Archive << CreateReleaseVersionName;
			Archive << BasedOnReleaseVersionName;
			
			Archive << CreateDLC;
			Archive << DLCName;
		}
		else if ( Archive.IsLoading() )
		{
			CreateReleaseVersion = false;
			CreateDLC = false;
		}

		if (Version >= LAUNCHERSERVICES_ADDEDGENERATECHUNKS)
		{
			Archive << bGenerateChunks;
			Archive << bGenerateHttpChunkData;
			Archive << HttpChunkDataDirectory;
			Archive << HttpChunkDataReleaseName;
		}
		else if (Archive.IsLoading())
		{
			bGenerateChunks = false;
			bGenerateHttpChunkData = false;
			HttpChunkDataDirectory = TEXT("");
			HttpChunkDataReleaseName = TEXT("");
		}

		DefaultLaunchRole->Serialize(Archive);

		// serialize launch roles
		if (Archive.IsLoading())
		{
			DeployedDeviceGroup.Reset();
			LaunchRoles.Reset();
		}

		int32 NumLaunchRoles = LaunchRoles.Num();

		Archive << NumLaunchRoles;

		for (int32 RoleIndex = 0; RoleIndex < NumLaunchRoles; ++RoleIndex)
		{
			if (Archive.IsLoading())
			{
				LaunchRoles.Add(MakeShareable(new FLauncherProfileLaunchRole(Archive)));				
			}
			else
			{
				LaunchRoles[RoleIndex]->Serialize(Archive);
			}
		}

		Validate();

		return true;
	}

	virtual void SetDefaults( ) override
	{
		ProjectSpecified = false;

		// default project settings
		if (FPaths::IsProjectFilePathSet())
		{
			ProjectPath = FPaths::GetProjectFilePath();
		}
		else if (FGameProjectHelper::IsGameAvailable(FApp::GetGameName()))
		{
			ProjectPath = FPaths::RootDir() / FApp::GetGameName() / FApp::GetGameName() + TEXT(".uproject");
		}
		else
		{
			ProjectPath = FString();
		}

		// Use the locally specified project path is resolving through the root isn't working
		ProjectSpecified = GetProjectPath().IsEmpty();
		
		// I don't use FApp::GetBuildConfiguration() because i don't want the act of running in debug the first time to cause 
		// profiles the user creates to be in debug. This will keep consistency.
		BuildConfiguration = EBuildConfigurations::Development;

		FInternationalization& I18N = FInternationalization::Get();

		// default build settings
		BuildGame = false;
		BuildUAT = false;

		// default cook settings
		CookConfiguration = FApp::GetBuildConfiguration();
		CookMode = ELauncherProfileCookModes::OnTheFly;
		CookOptions = FString();
		CookIncremental = false;
		CookUnversioned = true;
		Compressed = false;
		CookedCultures.Reset();
		CookedCultures.Add(I18N.GetCurrentCulture()->GetName());
		CookedMaps.Reset();
		CookedPlatforms.Reset();
        ForceClose = true;
        Timeout = 60;

/*		if (GetTargetPlatformManager()->GetRunningTargetPlatform() != NULL)
		{
			CookedPlatforms.Add(GetTargetPlatformManager()->GetRunningTargetPlatform()->PlatformName());
		}*/	

		// default deploy settings
		DeployedDeviceGroup.Reset();
		DeploymentMode = ELauncherProfileDeploymentModes::CopyToDevice;
		DeployStreamingServer = false;
		DeployWithUnrealPak = false;
		DeployedDeviceGroupId = FGuid();
		HideFileServerWindow = false;
		DeployIncremental = false;

		CreateReleaseVersion = false;
		GeneratePatch = false;
		CreateDLC = false;
		DLCIncludeEngineContent = false;

		bGenerateChunks = false;
		bGenerateHttpChunkData = false;
		HttpChunkDataDirectory = TEXT("");
		HttpChunkDataReleaseName = TEXT("");

		// default launch settings
		LaunchMode = ELauncherProfileLaunchModes::DefaultRole;
		DefaultLaunchRole->SetCommandLine(FString());
		DefaultLaunchRole->SetInitialCulture(I18N.GetCurrentCulture()->GetName());
		DefaultLaunchRole->SetInitialMap(FString());
		DefaultLaunchRole->SetName(TEXT("Default Role"));
		DefaultLaunchRole->SetInstanceType(ELauncherProfileRoleInstanceTypes::StandaloneClient);
		DefaultLaunchRole->SetVsyncEnabled(false);
		LaunchRoles.Reset();

		// default packaging settings
		PackagingMode = ELauncherProfilePackagingModes::DoNotPackage;

		// default UAT settings
		EditorExe.Empty();

		Validate();
	}

	virtual void SetBuildGame(bool Build) override
	{
		if (BuildGame != Build)
		{
			BuildGame = Build;

			Validate();
		}
	}

	virtual void SetBuildUAT(bool Build) override
	{
		if (BuildUAT != Build)
		{
			BuildUAT = Build;

			Validate();
		}
	}

	virtual void SetBuildConfiguration( EBuildConfigurations::Type Configuration ) override
	{
		if (BuildConfiguration != Configuration)
		{
			BuildConfiguration = Configuration;

			Validate();
		}
	}

	virtual void SetCookConfiguration( EBuildConfigurations::Type Configuration ) override
	{
		if (CookConfiguration != Configuration)
		{
			CookConfiguration = Configuration;

			Validate();
		}
	}

	virtual void SetCookMode( ELauncherProfileCookModes::Type Mode ) override
	{
		if (CookMode != Mode)
		{
			CookMode = Mode;

			Validate();
		}
	}

	virtual void SetCookOptions(const FString& Options) override
	{
		if (CookOptions != Options)
		{
			CookOptions = Options;

			Validate();
		}
	}

	virtual void SetDeployWithUnrealPak( bool UseUnrealPak ) override
	{
		if (DeployWithUnrealPak != UseUnrealPak)
		{
			DeployWithUnrealPak = UseUnrealPak;

			Validate();
		}
	}

	virtual void SetGenerateChunks(bool bInGenerateChunks) override
	{
		if (bGenerateChunks != bInGenerateChunks)
		{
			bGenerateChunks = bInGenerateChunks;
			Validate();
		}
	}

	virtual void SetGenerateHttpChunkData(bool bInGenerateHttpChunkData) override
	{
		if (bGenerateHttpChunkData != bInGenerateHttpChunkData)
		{
			bGenerateHttpChunkData = bInGenerateHttpChunkData;
			Validate();
		}
	}

	virtual void SetHttpChunkDataDirectory(const FString& InHttpChunkDataDirectory) override
	{
		if (HttpChunkDataDirectory != InHttpChunkDataDirectory)
		{
			HttpChunkDataDirectory = InHttpChunkDataDirectory;
			Validate();
		}
	}

	virtual void SetHttpChunkDataReleaseName(const FString& InHttpChunkDataReleaseName) override
	{
		if (HttpChunkDataReleaseName != InHttpChunkDataReleaseName)
		{
			HttpChunkDataReleaseName = InHttpChunkDataReleaseName;
			Validate();
		}
	}

	virtual void SetDeployedDeviceGroup( const ILauncherDeviceGroupPtr& DeviceGroup ) override
	{
		if(DeployedDeviceGroup.IsValid())
		{
			DeployedDeviceGroup->OnDeviceAdded().Remove(OnLauncherDeviceGroupDeviceAddedDelegateHandle);
			DeployedDeviceGroup->OnDeviceRemoved().Remove(OnLauncherDeviceGroupDeviceRemoveDelegateHandle);
		}
		DeployedDeviceGroup = DeviceGroup;
		if (DeployedDeviceGroup.IsValid())
		{
			OnLauncherDeviceGroupDeviceAddedDelegateHandle   = DeployedDeviceGroup->OnDeviceAdded().AddRaw(this, &FLauncherProfile::OnLauncherDeviceGroupDeviceAdded);
			OnLauncherDeviceGroupDeviceRemoveDelegateHandle  = DeployedDeviceGroup->OnDeviceRemoved().AddRaw(this, &FLauncherProfile::OnLauncherDeviceGroupDeviceRemove);
			DeployedDeviceGroupId = DeployedDeviceGroup->GetId();
		}
		else
		{
			DeployedDeviceGroupId = FGuid();
		}

		Validate();
	}

	virtual FIsCookFinishedDelegate& OnIsCookFinished() override
	{
		return IsCookFinishedDelegate;
	}

	virtual FCookCanceledDelegate& OnCookCanceled() override
	{
		return CookCanceledDelegate;
	}

	virtual void SetDeploymentMode( ELauncherProfileDeploymentModes::Type Mode ) override
	{
		if (DeploymentMode != Mode)
		{
			DeploymentMode = Mode;

			Validate();
		}
	}

    virtual void SetForceClose( bool Close ) override
    {
        if (ForceClose != Close)
        {
            ForceClose = Close;
            Validate();
        }
    }
    
	virtual void SetHideFileServerWindow( bool Hide ) override
	{
		HideFileServerWindow = Hide;
	}

	virtual void SetIncrementalCooking( bool Incremental ) override
	{
		if (CookIncremental != Incremental)
		{
			CookIncremental = Incremental;

			Validate();
		}
	}

	virtual void SetCompressed( bool Enabled ) override
	{
		if (Compressed != Enabled)
		{
			Compressed = Enabled;

			Validate();
		}
	}

	virtual void SetIncrementalDeploying( bool Incremental ) override
	{
		if (DeployIncremental != Incremental)
		{
			DeployIncremental = Incremental;

			Validate();
		}
	}

	virtual void SetLaunchMode( ELauncherProfileLaunchModes::Type Mode ) override
	{
		if (LaunchMode != Mode)
		{
			LaunchMode = Mode;

			Validate();
		}
	}

	virtual void SetName( const FString& NewName ) override
	{
		if (Name != NewName)
		{
			Name = NewName;

			Validate();
		}
	}

	virtual void SetDescription(const FString& NewDescription) override
	{
		if (Description != NewDescription)
		{
			Description = NewDescription;

			Validate();
		}
	}

	virtual void SetPackagingMode( ELauncherProfilePackagingModes::Type Mode ) override
	{
		if (PackagingMode != Mode)
		{
			PackagingMode = Mode;

			Validate();
		}
	}

	virtual void SetPackageDirectory( const FString& Dir ) override
	{
		if (PackageDir != Dir)
		{
			PackageDir = Dir;

			Validate();
		}
	}

	virtual void SetProjectSpecified(bool Specified) override
	{
		if (ProjectSpecified != Specified)
		{
			ProjectSpecified = Specified;

			Validate();

			ProjectChangedDelegate.Broadcast();
		}
	}

	virtual void FallbackProjectUpdated() override
	{
		if (!HasProjectSpecified())
		{
			Validate();

			ProjectChangedDelegate.Broadcast();
		}
	}

	virtual void SetProjectPath( const FString& Path ) override
	{
		if (ProjectPath != Path)
		{
			if(Path.IsEmpty())
			{
				ProjectPath = Path;
			}
			else
			{
				ProjectPath = FPaths::ConvertRelativePathToFull(Path);
			}
			CookedMaps.Reset();

			Validate();

			ProjectChangedDelegate.Broadcast();
		}
	}

	virtual void SetStreamingFileServer( bool Streaming ) override
	{
		if (DeployStreamingServer != Streaming)
		{
			DeployStreamingServer = Streaming;

			Validate();
		}
	}

    virtual void SetTimeout( uint32 InTime ) override
    {
        if (Timeout != InTime)
        {
            Timeout = InTime;
            
            Validate();
        }
    }
    
	virtual void SetUnversionedCooking( bool Unversioned ) override
	{
		if (CookUnversioned != Unversioned)
		{
			CookUnversioned = Unversioned;

			Validate();
		}
	}

	virtual void SetGeneratePatch( bool InGeneratePatch ) override
	{
		GeneratePatch = InGeneratePatch;
	}

	virtual bool SupportsEngineMaps( ) const override
	{
		return false;
	}

	virtual FOnProfileProjectChanged& OnProjectChanged() override
	{
		return ProjectChangedDelegate;
	}

	virtual void SetEditorExe( const FString& InEditorExe ) override
	{
		EditorExe = InEditorExe;
	}

	virtual FString GetEditorExe( ) const override
	{
		return EditorExe;
	}

	// End ILauncherProfile interface

protected:

	/**
	 * Validates the profile's current settings.
	 *
	 * Possible validation errors and warnings can be retrieved using the HasValidationError() method.
	 *
	 * @return true if the profile passed validation, false otherwise.
	 */
	void Validate( )
	{
		ValidationErrors.Reset();

		// Build: a build configuration must be selected
		if (BuildConfiguration == EBuildConfigurations::Unknown)
		{
			ValidationErrors.Add(ELauncherProfileValidationErrors::NoBuildConfigurationSelected);
		}

		// Build: a project must be selected
		if (GetProjectPath().IsEmpty())
		{
			ValidationErrors.Add(ELauncherProfileValidationErrors::NoProjectSelected);
		}

		// Cook: at least one platform must be selected when cooking by the book
		if ((CookMode == ELauncherProfileCookModes::ByTheBook) && (CookedPlatforms.Num() == 0))
		{
			ValidationErrors.Add(ELauncherProfileValidationErrors::NoPlatformSelected);
		}

		// Cook: at least one culture must be selected when cooking by the book
		if ((CookMode == ELauncherProfileCookModes::ByTheBook) && (CookedCultures.Num() == 0))
		{
			ValidationErrors.Add(ELauncherProfileValidationErrors::NoCookedCulturesSelected);
		}

		// Deploy: a device group must be selected when deploying builds
		if ((DeploymentMode == ELauncherProfileDeploymentModes::CopyToDevice) && !DeployedDeviceGroupId.IsValid())
		{
			ValidationErrors.Add(ELauncherProfileValidationErrors::DeployedDeviceGroupRequired);
		}

		// Deploy: deployment by copying to devices requires cooking by the book
		if ((DeploymentMode == ELauncherProfileDeploymentModes::CopyToDevice) && ((CookMode != ELauncherProfileCookModes::ByTheBook)&&(CookMode!=ELauncherProfileCookModes::ByTheBookInEditor)))
		{
			ValidationErrors.Add(ELauncherProfileValidationErrors::CopyToDeviceRequiresCookByTheBook);
		}

		// Deploy: deployment by copying a packaged build to devices requires a package dir
		if ((DeploymentMode == ELauncherProfileDeploymentModes::CopyRepository) && (PackageDir == TEXT("")))
		{
			ValidationErrors.Add(ELauncherProfileValidationErrors::NoPackageDirectorySpecified);
		}

		// Launch: custom launch roles are not supported yet
		if (LaunchMode == ELauncherProfileLaunchModes::CustomRoles)
		{
			ValidationErrors.Add(ELauncherProfileValidationErrors::CustomRolesNotSupportedYet);
		}

		// Launch: when using custom launch roles, all roles must have a device assigned
		if (LaunchMode == ELauncherProfileLaunchModes::CustomRoles)
		{
			for (int32 RoleIndex = 0; RoleIndex < LaunchRoles.Num(); ++RoleIndex)
			{
				if (LaunchRoles[RoleIndex]->GetAssignedDevice().IsEmpty())
				{
					ValidationErrors.Add(ELauncherProfileValidationErrors::NoLaunchRoleDeviceAssigned);
				
					break;
				}
			}
		}

		if ( CookUnversioned && CookIncremental )
		{
			ValidationErrors.Add(ELauncherProfileValidationErrors::UnversionedAndIncrimental);
		}


		if ( IsGeneratingPatch() && (CookMode != ELauncherProfileCookModes::ByTheBook) )
		{
			ValidationErrors.Add(ELauncherProfileValidationErrors::GeneratingPatchesCanOnlyRunFromByTheBookCookMode);
		}

		if ( IsGeneratingChunks() && (CookMode != ELauncherProfileCookModes::ByTheBook) )
		{
			ValidationErrors.Add(ELauncherProfileValidationErrors::GeneratingChunksRequiresCookByTheBook);
		}

		if (IsGeneratingChunks() && !IsPackingWithUnrealPak())
		{
			ValidationErrors.Add(ELauncherProfileValidationErrors::GeneratingChunksRequiresUnrealPak);
		}

		if (IsGenerateHttpChunkData() && !IsGeneratingChunks())
		{
			ValidationErrors.Add(ELauncherProfileValidationErrors::GeneratingHttpChunkDataRequiresGeneratingChunks);
		}

		if (IsGenerateHttpChunkData() && (GetHttpChunkDataReleaseName().IsEmpty() || !FPaths::DirectoryExists(*GetHttpChunkDataDirectory())))
		{
			ValidationErrors.Add(ELauncherProfileValidationErrors::GeneratingHttpChunkDataRequiresValidDirectoryAndName);
		}

		// Launch: when launching, all devices that the build is launched on must have content cooked for their platform
		if ((LaunchMode != ELauncherProfileLaunchModes::DoNotLaunch) && (CookMode != ELauncherProfileCookModes::OnTheFly || CookMode != ELauncherProfileCookModes::OnTheFlyInEditor))
		{
			// @todo ensure that launched devices have cooked content
		}
		
		ValidatePlatformSDKs();
	}
	
	void ValidatePlatformSDKs(void)
	{
		ValidationErrors.Remove(ELauncherProfileValidationErrors::NoPlatformSDKInstalled);
		
		// Cook: ensure that all platform SDKs are installed
		if (CookedPlatforms.Num() > 0)
		{
			bool bProjectHasCode = false; // @todo: Does the project have any code?
			FString NotInstalledDocLink;
			for (auto PlatformName : CookedPlatforms)
			{
				const ITargetPlatform* Platform = GetTargetPlatformManager()->FindTargetPlatform(PlatformName);
				if(!Platform || !Platform->IsSdkInstalled(bProjectHasCode, NotInstalledDocLink))
				{
					ValidationErrors.Add(ELauncherProfileValidationErrors::NoPlatformSDKInstalled);
					ILauncherServicesModule& LauncherServicesModule = FModuleManager::GetModuleChecked<ILauncherServicesModule>(TEXT("LauncherServices"));
					LauncherServicesModule.BroadcastLauncherServicesSDKNotInstalled(PlatformName, NotInstalledDocLink);
					if (!Platform)
					{
						CookedPlatforms.Remove(PlatformName);
					}
					else
					{
						InvalidPlatform = PlatformName;
					}
					return;
				}
			}
		}
		
		// Deploy: ensure that all the target device SDKs are installed
		if ((DeploymentMode != ELauncherProfileDeploymentModes::DoNotDeploy) && DeployedDeviceGroup.IsValid())
		{
			const TArray<FString>& Devices = DeployedDeviceGroup->GetDeviceIDs();
			for(auto DeviceId : Devices)
			{
				TSharedPtr<ITargetDeviceServicesModule> TargetDeviceServicesModule = StaticCastSharedPtr<ITargetDeviceServicesModule>(FModuleManager::Get().LoadModule(TEXT("TargetDeviceServices")));
				
				if (TargetDeviceServicesModule.IsValid())
				{
					ITargetDeviceProxyPtr DeviceProxy = TargetDeviceServicesModule->GetDeviceProxyManager()->FindProxy(DeviceId);
					
					if(DeviceProxy.IsValid())
					{
						FString const& PlatformName = DeviceProxy->GetTargetPlatformName(DeviceProxy->GetTargetDeviceVariant(DeviceId));
						bool bProjectHasCode = false; // @todo: Does the project have any code?
						FString NotInstalledDocLink;
						const ITargetPlatform* Platform = GetTargetPlatformManager()->FindTargetPlatform(PlatformName);
						if(!Platform || !Platform->IsSdkInstalled(bProjectHasCode, NotInstalledDocLink))
						{
							ValidationErrors.Add(ELauncherProfileValidationErrors::NoPlatformSDKInstalled);
							ILauncherServicesModule& LauncherServicesModule = FModuleManager::GetModuleChecked<ILauncherServicesModule>(TEXT("LauncherServices"));
							LauncherServicesModule.BroadcastLauncherServicesSDKNotInstalled(PlatformName, NotInstalledDocLink);
							DeployedDeviceGroup->RemoveDevice(DeviceId);
							return;
						}
					}
				}
			}
		}
	}
	
	void OnLauncherDeviceGroupDeviceAdded(const ILauncherDeviceGroupRef& DeviceGroup, const FString& DeviceId)
	{
		if( DeviceGroup == DeployedDeviceGroup )
		{
			ValidatePlatformSDKs();
		}
	}
	
	void OnLauncherDeviceGroupDeviceRemove(const ILauncherDeviceGroupRef& DeviceGroup, const FString& DeviceId)
	{
		if( DeviceGroup == DeployedDeviceGroup )
		{
			ValidatePlatformSDKs();
		}
	}

private:

	//  Holds a reference to the launcher profile manager.
	ILauncherProfileManagerRef LauncherProfileManager;

	// Holds the desired build configuration (only used if creating new builds).
	TEnumAsByte<EBuildConfigurations::Type> BuildConfiguration;

	// Holds the build mode.
	// Holds the build configuration name of the cooker.
	TEnumAsByte<EBuildConfigurations::Type> CookConfiguration;

	// Holds additional cooker command line options.
	FString CookOptions;

	// Holds the cooking mode.
	TEnumAsByte<ELauncherProfileCookModes::Type> CookMode;

	// Holds a flag indicating whether the game should be built
	bool BuildGame;

	// Holds a flag indicating whether UAT should be built
	bool BuildUAT;

	// Generate compressed content
	bool Compressed;

	// Holds a flag indicating whether only modified content should be cooked.
	bool CookIncremental;

	// Holds a flag indicating whether packages should be saved without a version.
	bool CookUnversioned;

	// This setting is used only if cooking by the book (only used if cooking by the book).
	TArray<FString> CookedCultures;

	// Holds the collection of maps to cook (only used if cooking by the book).
	TArray<FString> CookedMaps;

	// Holds the platforms to include in the build (only used if creating new builds).
	TArray<FString> CookedPlatforms;

	// Holds the default role (only used if launch mode is DefaultRole).
	ILauncherProfileLaunchRoleRef DefaultLaunchRole;

	// Holds a flag indicating whether a streaming file server should be used.
	bool DeployStreamingServer;

	// Holds a flag indicating whether content should be packaged with UnrealPak.
	bool DeployWithUnrealPak;

	// Flag indicating if content should be split into chunks
	bool bGenerateChunks;
	
	// Flag indicating if chunked content should be used to generate HTTPChunkInstall data
	bool bGenerateHttpChunkData;
	
	// Where to store HTTPChunkInstall data
	FString HttpChunkDataDirectory;
	
	// Version name of the HTTPChunkInstall data
	FString HttpChunkDataReleaseName;

	// create a release version of the content (this can be used to base dlc / patches from)
	bool CreateReleaseVersion;

	// name of the release version
	FString CreateReleaseVersionName;

	// name of the release version to base this dlc / patch on
	FString BasedOnReleaseVersionName;

	// This build generate a patch based on some source content seealso PatchSourceContentPath
	bool GeneratePatch;

	// This build will cook content for dlc See also DLCName
	bool CreateDLC;

	// name of the dlc we are going to build (the name of the dlc plugin)
	FString DLCName;

	// should the dlc include engine content in the current dlc 
	//  engine content which was not referenced by original release
	//  otherwise error on any access of engine content during dlc cook
	bool DLCIncludeEngineContent;

	// Holds a flag indicating whether to use incremental deployment
	bool DeployIncremental;

	// Holds the device group to deploy to.
	ILauncherDeviceGroupPtr DeployedDeviceGroup;

	// Delegate handles for registered DeployedDeviceGroup event handlers.
	FDelegateHandle OnLauncherDeviceGroupDeviceAddedDelegateHandle;
	FDelegateHandle OnLauncherDeviceGroupDeviceRemoveDelegateHandle;

	// Holds the identifier of the deployed device group.
	FGuid DeployedDeviceGroupId;

	// Holds the deployment mode.
	TEnumAsByte<ELauncherProfileDeploymentModes::Type> DeploymentMode;

	// Holds a flag indicating whether the file server's console window should be hidden.
	bool HideFileServerWindow;

	// Holds the profile's unique identifier.
	FGuid Id;

	// Holds the launch mode.
	TEnumAsByte<ELauncherProfileLaunchModes::Type> LaunchMode;

	// Holds the launch roles (only used if launch mode is UsingRoles).
	TArray<ILauncherProfileLaunchRolePtr> LaunchRoles;

	// Holds the profile's name.
	FString Name;

	// Holds the profile's description.
	FString Description;

	// Holds the packaging mode.
	TEnumAsByte<ELauncherProfilePackagingModes::Type> PackagingMode;

	// Holds the package directory
	FString PackageDir;

	// Holds a flag indicating whether the project is specified by this profile.
	bool ProjectSpecified;

	// Holds the path to the Unreal project used by this profile.
	FString ProjectPath;

	// Holds the collection of validation errors.
	TArray<ELauncherProfileValidationErrors::Type> ValidationErrors;
	FString InvalidPlatform;

    // Holds the time out time for the cook on the fly server
    uint32 Timeout;

    // Holds the close value for the cook on the fly server
    bool ForceClose;

	// Path to the editor executable to pass to UAT, for cooking, etc... May be empty.
	FString EditorExe;

private:

	// cook in the editor callbacks (not valid for any other cook mode)
	FIsCookFinishedDelegate IsCookFinishedDelegate;
	FCookCanceledDelegate CookCanceledDelegate;

	// Holds a delegate to be invoked when changing the device group to deploy to.
	FOnLauncherProfileDeployedDeviceGroupChanged DeployedDeviceGroupChangedDelegate;

	// Holds a delegate to be invoked when the project has changed
	FOnProfileProjectChanged ProjectChangedDelegate;
};
