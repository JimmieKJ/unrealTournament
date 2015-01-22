// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "UnrealEd.h"
#include "GlobalEditorNotification.h"
#include "ShaderCompiler.h"
#include "SNotificationList.h"

/** Notification class for asynchronous shader compiling. */
class FShaderCompilingNotificationImpl : public FGlobalEditorNotification
{
protected:
	/** FGlobalEditorNotification interface */
	virtual bool ShouldShowNotification(const bool bIsNotificationAlreadyActive) const override;
	virtual void SetNotificationText(const TSharedPtr<SNotificationItem>& InNotificationItem) const override;
};

/** Global notification object. */
FShaderCompilingNotificationImpl GShaderCompilingNotification;

bool FShaderCompilingNotificationImpl::ShouldShowNotification(const bool bIsNotificationAlreadyActive) const
{
	// ShouldDisplayCompilingNotification is more a hint, and may start returning false while there's still work being done
	// If we're already showing the notification, we should continue to do so until all the jobs have finished
	return GShaderCompilingManager->ShouldDisplayCompilingNotification() || (bIsNotificationAlreadyActive && GShaderCompilingManager->IsCompiling());
}

void FShaderCompilingNotificationImpl::SetNotificationText(const TSharedPtr<SNotificationItem>& InNotificationItem) const
{
	if (GShaderCompilingManager->IsCompiling())
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("ShaderJobs"), FText::AsNumber(GShaderCompilingManager->GetNumRemainingJobs()));
		const FText ProgressMessage = FText::Format(NSLOCTEXT("ShaderCompile", "ShaderCompileInProgressFormat", "Compiling Shaders ({ShaderJobs})"), Args);

		InNotificationItem->SetText(ProgressMessage);
	}
}
