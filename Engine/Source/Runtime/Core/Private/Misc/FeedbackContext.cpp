// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "FeedbackContext.h"

void FFeedbackContext::RequestUpdateUI(bool bForceUpdate)
{
	// Only update a maximum of 5 times a second
	static double MinUpdateTimeS = 0.2;

	static double LastUIUpdateTime = FPlatformTime::Seconds();
	const double CurrentTime = FPlatformTime::Seconds();

	if (bForceUpdate || CurrentTime - LastUIUpdateTime > MinUpdateTimeS)
	{
		LastUIUpdateTime = CurrentTime;
		UpdateUI();
	}
}

void FFeedbackContext::UpdateUI()
{
	if (ScopeStack->Num() != 0)
	{
		ProgressReported(ScopeStack->GetProgressFraction(0), (*ScopeStack)[0]->GetCurrentMessage());
	}
}

/**** Begin legacy API ****/
void FFeedbackContext::BeginSlowTask( const FText& Task, bool ShowProgressDialog, bool bShowCancelButton )
{
	TUniquePtr<FScopedSlowTask> NewScope(new FScopedSlowTask(0, Task, true, *this));
	if (ShowProgressDialog)
	{
		NewScope->MakeDialog(bShowCancelButton);
	}
	LegacyAPIScopes.Add(MoveTemp(NewScope));
}

void FFeedbackContext::UpdateProgress( int32 Numerator, int32 Denominator )
{
	if (LegacyAPIScopes.Num() != 0)
	{
		LegacyAPIScopes.Last()->TotalAmountOfWork = Denominator;
		LegacyAPIScopes.Last()->CompletedWork = Numerator;
		LegacyAPIScopes.Last()->CurrentFrameScope = Denominator - Numerator;
		RequestUpdateUI();
	}
}

void FFeedbackContext::StatusUpdate( int32 Numerator, int32 Denominator, const FText& StatusText )
{
	if (LegacyAPIScopes.Num() != 0)
	{
		if (Numerator > 0 && Denominator > 0)
		{
			UpdateProgress(Numerator, Denominator);
		}
		LegacyAPIScopes.Last()->FrameMessage = StatusText;
		RequestUpdateUI();
	}
}

void FFeedbackContext::StatusForceUpdate( int32 Numerator, int32 Denominator, const FText& StatusText )
{
	if (LegacyAPIScopes.Num() != 0)
	{
		UpdateProgress(Numerator, Denominator);
		LegacyAPIScopes.Last()->FrameMessage = StatusText;
		UpdateUI();
	}
}

void FFeedbackContext::EndSlowTask()
{
	check(LegacyAPIScopes.Num() != 0);
	LegacyAPIScopes.Pop();
}
/**** End legacy API ****/

void FScopedSlowTask::MakeDialog(bool bShowCancelButton, bool bAllowInPIE)
{
	const bool bIsDisabledByPIE = GIsPlayInEditorWorld && !bAllowInPIE;
	const bool bIsDialogAllowed = bEnabled && !GIsSilent && !bIsDisabledByPIE && !IsRunningCommandlet() && IsInGameThread();
	if (!GIsSlowTask && bIsDialogAllowed)
	{
		Context.StartSlowTask(GetCurrentMessage(), bShowCancelButton);
		if (GIsSlowTask)
		{
			bCreatedDialog = true;
		}
	}
}

float FScopedSlowTaskStack::GetProgressFraction(int32 Index) const
{
	const int32 StartIndex = Num() - 1;
	const int32 EndIndex = Index;

	float Progress = 0.f;
	for (int32 CurrentIndex = StartIndex; CurrentIndex >= EndIndex; --CurrentIndex)
	{
		const FScopedSlowTask* Scope = (*this)[CurrentIndex];
		
		const float ThisScopeCompleted = float(Scope->CompletedWork) / Scope->TotalAmountOfWork;
		const float ThisScopeCurrentFrame = float(Scope->CurrentFrameScope) / Scope->TotalAmountOfWork;

		Progress = ThisScopeCompleted + ThisScopeCurrentFrame*Progress;
	}

	return Progress;
}