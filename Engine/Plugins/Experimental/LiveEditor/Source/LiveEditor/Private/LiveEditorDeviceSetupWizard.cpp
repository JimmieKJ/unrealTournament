// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LiveEditorPrivatePCH.h"
#include "LiveEditorDeviceSetupWizard.h"

#define LOCTEXT_NAMESPACE "LiveEditorWizard"

namespace nLiveEditorDeviceSetupWizard
{
	enum States
	{
		S_CONFIGURATION = 0,
		S_BUTTON,
		S_CONTINUOUSKNOB_RIGHT,
		S_CONTINUOUSKNOB_LEFT,
		S_CONFIGURED
	};
}



/**
 * 
 * Wizard States
 *
 **/

class SDeviceQuestionnaireSubWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDeviceQuestionnaireSubWindow){}
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, struct FConfigurationState *_Owner);

	void OnHasButtonsChanged(ECheckBoxState NewCheckedState);
	ECheckBoxState HasButtons() const;
	void OnHasEndlessEncodersChanged(ECheckBoxState NewCheckedState);
	ECheckBoxState HasEndlessEncoders() const;

private:
	struct FConfigurationState *Owner;
};

struct FConfigurationState : public FLiveEditorDeviceSetupWizard::FState
{
	FConfigurationState( FLiveEditorDeviceSetupWizard *_Owner, int32 _NextState )
	:	FLiveEditorDeviceSetupWizard::FState(_NextState),
		bHasButtons(false),
		bHasEndlessEncoders(false),
		Owner(_Owner)
	{
	}

	virtual FText GetStateTitle() const
	{
		return LOCTEXT("DeviceQuestionnaire", "Device Questionnaire");
	}
	virtual FText GetStateText() const
	{
		return LOCTEXT("DeviceQuestionnaireDesc", "Select all properties that apply to your device");
	}
	virtual void Init()
	{
		check(Owner != NULL);
		Owner->ForceReadyToAdvance();
	}
	virtual void OnExit()
	{
		if ( bHasButtons )
		{
			NextState = nLiveEditorDeviceSetupWizard::S_BUTTON;
		}
		else if ( bHasEndlessEncoders )
		{
			NextState = nLiveEditorDeviceSetupWizard::S_CONTINUOUSKNOB_RIGHT;
		}

		check( Owner != NULL );
		Owner->Configure( bHasButtons, bHasEndlessEncoders );
	}
	virtual bool ProcessMIDI( int Status, int Data1, int Data2, struct FLiveEditorDeviceData &Data )
	{
		return false;
	}

	virtual TSharedRef<class SWidget> GenerateStateContent()
	{
		return SNew( SDeviceQuestionnaireSubWindow, this );
	}

public:
	bool bHasButtons;
	bool bHasEndlessEncoders;

private:
	FLiveEditorDeviceSetupWizard *Owner;
};

void SDeviceQuestionnaireSubWindow::Construct(const FArguments& InArgs, FConfigurationState *_Owner)
{
	check( _Owner != NULL );
	Owner = _Owner;

	Owner->bHasButtons = false;
	Owner->bHasEndlessEncoders = false;

	ChildSlot
	[
		SNew( SVerticalBox )
		+SVerticalBox::Slot()
		[
			SNew( STextBlock )
			.Text( _Owner->GetStateText() )
			.ColorAndOpacity( FLinearColor::White )
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f)
		[
			SNew(SCheckBox)
			.OnCheckStateChanged(this, &SDeviceQuestionnaireSubWindow::OnHasButtonsChanged)
			.IsChecked( this, &SDeviceQuestionnaireSubWindow::HasButtons )
			.Content()
			[
				SNew(STextBlock)
				.Text( LOCTEXT("Buttons", "Buttons") )
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f)
		[
			SNew(SCheckBox)
			.OnCheckStateChanged(this, &SDeviceQuestionnaireSubWindow::OnHasEndlessEncodersChanged)
			.IsChecked( this, &SDeviceQuestionnaireSubWindow::HasEndlessEncoders )
			.Content()
			[
				SNew(STextBlock)
				.Text( LOCTEXT("EndlessEncoders", "Endless Encoders") )
			]
		]
	];
}
void SDeviceQuestionnaireSubWindow::OnHasButtonsChanged(ECheckBoxState NewCheckedState)
{
	check( Owner != NULL );
	Owner->bHasButtons = (NewCheckedState == ECheckBoxState::Checked);
}
ECheckBoxState SDeviceQuestionnaireSubWindow::HasButtons() const
{
	check( Owner != NULL );
	return (Owner->bHasButtons)? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}
void SDeviceQuestionnaireSubWindow::OnHasEndlessEncodersChanged(ECheckBoxState NewCheckedState)
{
	check( Owner != NULL );
	Owner->bHasEndlessEncoders = (NewCheckedState == ECheckBoxState::Checked);
}
ECheckBoxState SDeviceQuestionnaireSubWindow::HasEndlessEncoders() const
{
	check( Owner != NULL );
	return (Owner->bHasEndlessEncoders)? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}


struct FButtonState : public FLiveEditorDeviceSetupWizard::FState
{
	FButtonState( int32 _NextState )
	:	FLiveEditorDeviceSetupWizard::FState(_NextState),
		HandleType(HT_DOWN)
	{
	}
	virtual FText GetStateTitle() const
	{
		return LOCTEXT("ButtonConfiguration", "Button Configuration");
	}
	virtual FText GetStateText() const
	{
		return LOCTEXT("ButtonConfigurationDesc", "Push a button");
	}
	virtual void Init()
	{
		HandleType = HT_DOWN;
	}
	//needs to remain int (instead of int32) since numbers are derived from TPL that uses int
	virtual bool ProcessMIDI( int Status, int Data1, int Data2, struct FLiveEditorDeviceData &Data )
	{
		switch ( HandleType )
		{
			case HT_DOWN:
				Data.ButtonSignalDown = Data2;
				HandleType = HT_UP;
				return false;

			case HT_UP:
				Data.ButtonSignalUp = Data2;
				HandleType = HT_COMPLETE;
				return true;

			case HT_COMPLETE:
			default:
				return true;
		}
	}

private:
	enum EHandleType
	{
		HT_DOWN,
		HT_UP,
		HT_COMPLETE
	};
	EHandleType HandleType;
};

struct FContinuousKnobBaseState : public FLiveEditorDeviceSetupWizard::FState
{
	FContinuousKnobBaseState( int32 _MinSampleCount, int32 _NextState )
	:	FLiveEditorDeviceSetupWizard::FState(_NextState),
		MinSampleCount(_MinSampleCount),
		SamplesCollected(0)
	{
	}

	virtual void Init()
	{
		Samples.Empty();
		SamplesCollected = 0;
	}

	//needs to remain int (instead of int32) since numbers are derived from TPL that uses int
	virtual bool ProcessMIDI( int Status, int Data1, int Data2, struct FLiveEditorDeviceData &Data )
	{
		if ( SamplesCollected > MinSampleCount )
		{
			return true;
		}

		//needs to remain int (instead of int32) since numbers are derived from TPL that uses int
		int SampleCount = (Samples.Contains(Data2))? Samples[Data2]+1 : 1;
		Samples.Add(Data2, SampleCount);

		++SamplesCollected;

		if ( SamplesCollected == MinSampleCount )
		{
			int SampleWinner = -1; ////needs to remain int (instead of int32) since numbers are derived from TPL that uses int
			int32 MaxCount = 0;
			for ( TMap<int,int32>::TConstIterator It(Samples); It; ++It )
			{
				int32 Count = (*It).Value;
				if ( Count > MaxCount )
				{
					SampleWinner = (*It).Key;
					MaxCount = Count;
				}
			}

			check( SampleWinner != -1 );
			SetDeviceData( SampleWinner, Data );

			return true;
		}
		else
		{
			return false;
		}
	}

protected:
	//needs to remain int (instead of int32) since numbers are derived from TPL that uses int
	virtual void SetDeviceData( int Data2, FLiveEditorDeviceData &Data ) = 0;

private:
	TMap<int,int32> Samples;
	int32 MinSampleCount;
	int32 SamplesCollected;
};
struct FContinuousKnobRightState : public FContinuousKnobBaseState
{
	FContinuousKnobRightState( int32 _NextState ) : FContinuousKnobBaseState( 15, _NextState ) {}
	virtual FText GetStateTitle() const
	{
		return LOCTEXT("ContinuousKnobConfiguration", "Continuous Knob Configuration");
	}
	virtual FText GetStateText() const
	{
		return LOCTEXT("ContinuousKnobConfigurationRightDesc", "Twist a knob to the RIGHT");
	}

protected:
	//needs to remain int (instead of int32) since numbers are derived from TPL that uses int
	virtual void SetDeviceData( int Data2, FLiveEditorDeviceData &Data )
	{
		Data.ContinuousIncrement = Data2;
	}
};
struct FContinuousKnobLeftState : public FContinuousKnobBaseState
{
	FContinuousKnobLeftState( int32 _NextState ) : FContinuousKnobBaseState( 15, _NextState ) {}
	virtual FText GetStateTitle() const
	{
		return LOCTEXT("ContinuousKnobConfiguration", "Continuous Knob Configuration");
	}
	virtual FText GetStateText() const
	{
		return LOCTEXT("ContinuousKnobConfigurationLeftDesc", "Twist a knob to the LEFT");
	}

protected:
	//needs to remain int (instead of int32) since numbers are derived from TPL that uses int
	virtual void SetDeviceData( int Data2, FLiveEditorDeviceData &Data )
	{
		Data.ContinuousDecrement = Data2;
	}
};

/**
 *
 * FLiveEditorDeviceSetupWizard
 *
 **/

FLiveEditorDeviceSetupWizard::FLiveEditorDeviceSetupWizard()
:	FLiveEditorWizardBase(nLiveEditorDeviceSetupWizard::S_CONFIGURED),
	ConfigState(NULL)
{
}
FLiveEditorDeviceSetupWizard::~FLiveEditorDeviceSetupWizard()
{
}

FText FLiveEditorDeviceSetupWizard::GetAdvanceButtonText() const
{
	if ( GetCurState() == nLiveEditorDeviceSetupWizard::S_CONFIGURATION )
	{
		check( ConfigState != NULL );
		return ( ConfigState->bHasButtons || ConfigState->bHasEndlessEncoders )? LOCTEXT("Next", "Next") : LOCTEXT("Finish", "Finish");
	}
	else
	{
		return FLiveEditorWizardBase::GetAdvanceButtonText();
	}
}

void FLiveEditorDeviceSetupWizard::Start( PmDeviceID _DeviceID, struct FLiveEditorDeviceData &Data )
{
	ClearStates();
	
	ConfigState = new FConfigurationState(this, nLiveEditorDeviceSetupWizard::S_CONFIGURED);
	AddState( nLiveEditorDeviceSetupWizard::S_CONFIGURATION, ConfigState );

	Data.ConfigState = FLiveEditorDeviceData::UNCONFIGURED;
	FLiveEditorWizardBase::Start(nLiveEditorDeviceSetupWizard::S_CONFIGURATION, _DeviceID);
}

void FLiveEditorDeviceSetupWizard::Configure( bool bHasButtons, bool bHasEndlessEncoders )
{
	if ( bHasButtons )
	{
		int32 NextState = (bHasEndlessEncoders)? nLiveEditorDeviceSetupWizard::S_CONTINUOUSKNOB_RIGHT : nLiveEditorDeviceSetupWizard::S_CONFIGURED;
		AddState( nLiveEditorDeviceSetupWizard::S_BUTTON, new FButtonState(NextState) );
	}

	if ( bHasEndlessEncoders )
	{
		AddState( nLiveEditorDeviceSetupWizard::S_CONTINUOUSKNOB_RIGHT, new FContinuousKnobRightState(nLiveEditorDeviceSetupWizard::S_CONTINUOUSKNOB_LEFT) );
		AddState( nLiveEditorDeviceSetupWizard::S_CONTINUOUSKNOB_LEFT, new FContinuousKnobLeftState(nLiveEditorDeviceSetupWizard::S_CONFIGURED) );
	}
}

void FLiveEditorDeviceSetupWizard::OnWizardFinished( struct FLiveEditorDeviceData &Data )
{
	Data.ConfigState = FLiveEditorDeviceData::CONFIGURED;

	FLiveEditorManager::SaveDeviceData( Data );
}

#undef LOCTEXT_NAMESPACE
