// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ScriptPluginPrivatePCH.h"
#include "ScriptObjectReferencer.h"

UProperty* FindScriptPropertyHelper(UClass* Class, FName PropertyName)
{
	for (TFieldIterator<UProperty> PropertyIt(Class, EFieldIteratorFlags::ExcludeSuper); PropertyIt; ++PropertyIt)
	{
		UProperty* Property = *PropertyIt;
		if (Property->GetFName() == PropertyName)
		{
			return Property;
		}
	}
	return NULL;
}

#if 0
#include "GeneratedScriptLibraries.inl"
#endif 

DEFINE_LOG_CATEGORY(LogScriptPlugin);

class FScriptPlugin : public IScriptPlugin
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE(FScriptPlugin, ScriptPlugin)


void FScriptPlugin::StartupModule()
{
	FScriptObjectReferencer::Init();

}

void FScriptPlugin::ShutdownModule()
{
	FScriptObjectReferencer::Shutdown();
}
