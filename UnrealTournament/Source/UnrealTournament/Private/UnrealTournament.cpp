// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"

class FUTModule : public FDefaultGameModuleImpl
{
	virtual void StartupModule() OVERRIDE;
};

IMPLEMENT_PRIMARY_GAME_MODULE(FUTModule, UnrealTournament, "UnrealTournament");
 
DEFINE_LOG_CATEGORY(UT);

// init editor hooks
#if WITH_EDITOR

#include "UTDetailsCustomization.h"

void FUTModule::StartupModule()
{
	FDefaultGameModuleImpl::StartupModule();

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyLayout("UTWeapon", FOnGetDetailCustomizationInstance::CreateStatic(&FUTDetailsCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyLayout("UTWeaponAttachment", FOnGetDetailCustomizationInstance::CreateStatic(&FUTDetailsCustomization::MakeInstance));

	PropertyModule.NotifyCustomizationModuleChanged();
}

#else

void FUTModule::StartupModule()
{
}
#endif