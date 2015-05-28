// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SizeMapModule.h"
#include "ISizeMapModule.h"
#include "SSizeMap.h"
#include "SDockTab.h"

#define LOCTEXT_NAMESPACE "SizeMap"

class FSizeMapModule : public ISizeMapModule
{
public:
	FSizeMapModule()
		: SizeMapTabId("SizeMap")
	{
	}

	virtual void StartupModule() override
	{
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(SizeMapTabId, FOnSpawnTab::CreateRaw( this, &FSizeMapModule::SpawnSizeMapTab ) )
			.SetDisplayName( LOCTEXT("SizeMapTitle", "Size Map") )
			.SetMenuType( ETabSpawnerMenuType::Hidden );
	}

	virtual void ShutdownModule() override
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(SizeMapTabId);
	}

	virtual void InvokeSizeMapTab(const TArray<FName>& AssetPackageNames) override
	{
		TSharedRef<SDockTab> NewTab = FGlobalTabmanager::Get()->InvokeTab( SizeMapTabId );
		TSharedRef<SSizeMap> SizeMap = StaticCastSharedRef<SSizeMap>( NewTab->GetContent() );
		SizeMap->SetRootAssetPackageNames( AssetPackageNames );
	}

private:
	TSharedRef<SDockTab> SpawnSizeMapTab( const FSpawnTabArgs& SpawnTabArgs )
	{
		TSharedRef<SDockTab> NewTab = SNew(SDockTab)
			.TabRole(ETabRole::NomadTab);

		NewTab->SetContent( SNew(SSizeMap) );

		return NewTab;
	}

private:
	FName SizeMapTabId;
};


IMPLEMENT_MODULE( FSizeMapModule, SizeMap )


#undef LOCTEXT_NAMESPACE