// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveEditorDevice.h"
#include "portmidi.h"

class FLiveEditorWizardBase
{
public:
	FLiveEditorWizardBase( int32 _FinalState );
	virtual ~FLiveEditorWizardBase();

	FText GetActiveStateTitle() const;

	// _DeviceID: device to which this Wizard is restricted from recieving data. If == pmNoDevice, all device input is permitted
	void Start( int32 FirstState, PmDeviceID _RestrictedToDeviceID = pmNoDevice );
	void Cancel();

	bool IsActive() const;

	// check whether or not the Wizard is ready to advance to the next state
	bool IsReadyToAdvance() const;

	// is the wizard on the last step? (i.e. display "Finished" instead of "Next")
	bool IsOnLastStep() const;

	// called to advance to next Wizard Page
	// @return : true if the Wizard is complete (false otherwise)
	bool AdvanceState();

	// arguments need to remain int (instead of int32) since numbers are derived from TPL that uses int
	void ProcessMIDI( int Status, int Data1, int Data2, PmDeviceID DeviceID, struct FLiveEditorDeviceData &Data );

	TSharedRef<class SWidget> GenerateStateContent();

	void ForceReadyToAdvance()
	{
		bReadyToAdvance = true;
	}

	virtual FText GetAdvanceButtonText() const;

	struct FState
	{
		FState( int32 _NextState );
		virtual ~FState() {}
		int32 GetNextState() const;

		virtual FText GetStateTitle() const = 0;
		virtual FText GetStateText() const = 0;
		
		virtual void Init() = 0;
		virtual void OnExit() {}

		//@ return whether or not this state is complete
		// arguments need to remain int (instead of int32) since numbers are derived from TPL that uses int
		virtual bool ProcessMIDI( int Status, int Data1, int Data2, struct FLiveEditorDeviceData &Data ) = 0;

		virtual TSharedRef<class SWidget> GenerateStateContent();

	protected:
		int32 NextState;
	};

	int32 GetCurState() const
	{
		return CurState;
	}

protected:
	void SetFinalState( int32 _FinalState )
	{
		FinalState = _FinalState;
	}

	/**
	 * method for child classes to finish up whatever final steps need to happen on wizard completion
	 **/
	virtual void OnWizardFinished(struct FLiveEditorDeviceData &Data) = 0;

	void AddState( int32 StateID, FState *NewState );
	void ClearStates();

private:
	PmDeviceID RestrictedToDeviceID;
	bool bReadyToAdvance;
	int32 FinalState;
	int32 CurState;
	TMap<int32,FState*> States;
};
