// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LiveEditorPrivatePCH.h"
#include "LiveEditorWizardBase.h"

#define LOCTEXT_NAMESPACE "LiveEditorWizard"

/**
 *
 * FLiveEditorDeviceSetupWizard::FState
 *
 **/
FLiveEditorDeviceSetupWizard::FState::FState( int32 _NextState )
:	NextState(_NextState)
{
}
int32 FLiveEditorDeviceSetupWizard::FState::GetNextState() const
{
	return NextState;
}

TSharedRef<class SWidget> FLiveEditorDeviceSetupWizard::FState::GenerateStateContent()
{
	return SNew( STextBlock )
		.Text( GetStateText() )
		.ColorAndOpacity( FLinearColor::White );
}


/**
 *
 * FLiveEditorWizardBase
 *
 **/

FLiveEditorWizardBase::FLiveEditorWizardBase( int32 _FinalState )
:	bReadyToAdvance(false),
	FinalState(_FinalState),
	CurState(FinalState)
{
}
FLiveEditorWizardBase::~FLiveEditorWizardBase()
{
	ClearStates();
}

FText FLiveEditorWizardBase::GetActiveStateTitle() const
{
	if ( States.Contains(CurState) )
	{
		return States[CurState]->GetStateTitle();
	}
	else
	{
		return LOCTEXT("Complete", "Complete");
	}
}

void FLiveEditorWizardBase::Start( int32 FirstState, PmDeviceID _RestrictedToDeviceID )
{
	RestrictedToDeviceID = _RestrictedToDeviceID;

	bReadyToAdvance = false;
	CurState = FirstState;
	States.Contains(CurState);
	States[CurState]->Init();
}

void FLiveEditorWizardBase::Cancel()
{
	bReadyToAdvance = false;
	CurState = FinalState;
}

bool FLiveEditorWizardBase::IsActive() const
{
	return (CurState != FinalState);
}

bool FLiveEditorWizardBase::IsReadyToAdvance() const
{
	return bReadyToAdvance;
}

bool FLiveEditorWizardBase::IsOnLastStep() const
{
	if ( States.Contains(CurState) )
	{
		return !States.Contains( States[CurState]->GetNextState() );
	}
	else
	{
		return false;
	}
}

bool FLiveEditorWizardBase::AdvanceState()
{
	if ( !bReadyToAdvance )
	{
		if(FPlatformMisc::IsDebuggerPresent())
		{
			FPlatformMisc::DebugBreak();
		}
		return false;
	}
	if ( !States.Contains(CurState) )
	{
		return true;
	}

	bReadyToAdvance = false;

	States[CurState]->OnExit();

	CurState = States[CurState]->GetNextState();
	if ( States.Contains(CurState) )
	{
		States[CurState]->Init();
	}

	return (CurState == FinalState);
}

// arguments need to remain int (instead of int32) since numbers are derived from TPL that uses int
void FLiveEditorWizardBase::ProcessMIDI( int Status, int Data1, int Data2, PmDeviceID DeviceID, struct FLiveEditorDeviceData &Data )
{
	//if we're restriced to a certain device, then discard information if the signal is not sent from that device
	if ( RestrictedToDeviceID != pmNoDevice && RestrictedToDeviceID != DeviceID )
	{
		return;
	}

	//sanity check. Should never == true
	if ( CurState < 0 )
	{
		if(FPlatformMisc::IsDebuggerPresent())
		{
			FPlatformMisc::DebugBreak();
		}
		return;
	}
	if ( !States.Contains(CurState) )
	{
		return;
	}

	if ( States[CurState]->ProcessMIDI( Status, Data1, Data2, Data ) )
	{
		if ( !bReadyToAdvance )
		{
			bReadyToAdvance = true;
			if ( IsOnLastStep() )
			{
				OnWizardFinished(Data);
			}
		}
	}
}

TSharedRef<class SWidget> FLiveEditorWizardBase::GenerateStateContent()
{
	check( States.Contains(CurState) );
	return States[CurState]->GenerateStateContent();
}

void FLiveEditorWizardBase::AddState( int32 StateID, FState *NewState )
{
	States.Add(StateID, NewState);
}

void FLiveEditorWizardBase::ClearStates()
{
	for ( TMap<int32,FState*>::TIterator It(States); It; ++It )
	{
		delete (*It).Value;
	}
	States.Empty();
}

FText FLiveEditorWizardBase::GetAdvanceButtonText() const
{
	return (IsOnLastStep())? LOCTEXT("Finish", "Finish") : LOCTEXT("Next", "Next");
}

#undef LOCTEXT_NAMESPACE
