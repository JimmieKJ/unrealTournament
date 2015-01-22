// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LiveEditorPrivatePCH.h"
#include "LiveEditorUserData.h"
#include "LiveEditorBlueprintBindingWizard.h"

#define LOCTEXT_NAMESPACE "LiveEditorWizard"

class SLiveEditorBlueprintBindingWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLiveEditorBlueprintBindingWindow){}
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, struct FBindState *_Owner);
	FText GetStateText() const;
	void OnTypeChanged(TSharedPtr<ELiveEditControllerType::Type> InType, ESelectInfo::Type);
	TSharedRef<SWidget> GenerateControlTypeItem(TSharedPtr<ELiveEditControllerType::Type> InType);
	FText GetControlTypeText(ELiveEditControllerType::Type Type) const;
	FText GetSelectedControlTypeText() const;

private:
	struct FBindState *Owner;
	TArray< TSharedPtr<ELiveEditControllerType::Type> > ControlTypes;
};



/**
 * 
 * Wizard States
 *
 **/

//const class UK2Node_MakeStruct*
struct FBindState : FLiveEditorWizardBase::FState
{
	FBindState( const FString _BlueprintName, UK2Node &_ScriptNode, int32 _NextState )
	:	FLiveEditorWizardBase::FState(_NextState),
		BlueprintName(_BlueprintName),
		ScriptNode(&_ScriptNode)
	{
	}
	
	virtual FText GetStateTitle() const
	{
		return LOCTEXT("MIDIBinding", "MIDI Binding");
	}
	virtual FText GetStateText() const
	{
		return FText::Format( LOCTEXT("MIDIBindingDescFmt", "Use the MIDI Control you would like bound to:\n\n{0}\n\nChannel: {1}\nControlID: {2}"),
			FText::FromString(Description),
			FText::AsNumber(BindingData.Channel),
			FText::AsNumber(BindingData.ControlID) );
	}

	virtual void Init()
	{
		PermittedControlTypes.Empty();

		check( ScriptNode != NULL );
		for ( TArray<UEdGraphPin*>::TConstIterator Jt(ScriptNode->Pins); Jt; Jt++ )
		{
			UEdGraphPin *Pin = *Jt;
			if ( Pin->PinName == FString(TEXT("EventName")) )
			{
				EventName = *Pin->DefaultValue;
			}
			else if ( Pin->PinName == FString(TEXT("Description")) )
			{
				if ( !Pin->DefaultTextValue.IsEmpty() )
				{
					Description = Pin->DefaultTextValue.ToString();
				}
				else
				{
					Description = *Pin->DefaultValue;
				}
			}
			else if ( Pin->PinName == FString(TEXT("PermittedBindings")) )
			{
				if ( Pin->LinkedTo.Num() > 0 )
				{
					UEdGraphPin *ArrayPin = Pin->LinkedTo[0];
					check( ArrayPin != NULL );
					UK2Node_MakeArray *ArrayNode = Cast<UK2Node_MakeArray>(ArrayPin->GetOuter());
					if ( ArrayNode != NULL )
					{
						UEdGraphPin *OutputPin = ArrayNode->GetOutputPin();
						for ( int32 i = 0; i < ArrayNode->Pins.Num(); ++i )
						{
							UEdGraphPin *ArrayNodePin = ArrayNode->Pins[i];
							if ( ArrayNodePin == NULL || ArrayNodePin == OutputPin )
								continue;

							ELiveEditControllerType::Type type = StringToControllerType(ArrayNodePin->DefaultValue);
							if ( type != ELiveEditControllerType::Count )
							{
								PermittedControlTypes.AddUnique( type );
							}
						}
					}
				}
			}
		}

		UK2Node_LiveEditObject *LiveEditObjectNode = Cast<UK2Node_LiveEditObject>(ScriptNode);
		if ( LiveEditObjectNode != NULL )
		{
			EventName = LiveEditObjectNode->GetEventName();
		}
	}

	//@ return whether or not this state is complete
	//arguments need to remain int (instead of int32) since numbers are derived from TPL that uses int
	virtual bool ProcessMIDI( int Status, int Data1, int Data2, struct FLiveEditorDeviceData &Data )
	{
		int Channel = (Status % 16) + 1;
		BindingData.Channel = Channel;

		int ControlID = Data1;
		BindingData.ControlID = ControlID;

		if ( BindingData.ControlType != ELiveEditControllerType::NotSet )
		{
			check( FLiveEditorManager::Get().GetLiveEditorUserData() != NULL );
			FLiveEditorManager::Get().GetLiveEditorUserData()->AddBlueprintBinding( BlueprintName, EventName, BindingData, false );
			return true;
		}
		else
		{
			return false;
		}
	}

	FLiveEditBinding &GetBindingData()
	{
		return BindingData;
	}
	const FLiveEditBinding &GetBindingData() const
	{
		return BindingData;
	}

	const TArray< ELiveEditControllerType::Type > &GetPermittedControlTypes()
	{
		return PermittedControlTypes;
	}

protected:
	virtual TSharedRef<class SWidget> GenerateStateContent()
	{
		return SNew( SLiveEditorBlueprintBindingWindow, this );
	}

private:
	ELiveEditControllerType::Type StringToControllerType( const FString &String )
	{
		if ( String == FString(TEXT("NotSet")) )
		{
			return ELiveEditControllerType::NotSet;
		}
		else if ( String == FString(TEXT("NoteOnOff")) )
		{
			return ELiveEditControllerType::NoteOnOff;
		}
		else if ( String == FString(TEXT("ControlChangeContinuous")) )
		{
			return ELiveEditControllerType::ControlChangeContinuous;
		}
		else if ( String == FString(TEXT("ControlChangeFixed")) )
		{
			return ELiveEditControllerType::ControlChangeFixed;
		}
		else
		{
			return ELiveEditControllerType::Count;
		}
	}

	FString BlueprintName;
	UK2Node *ScriptNode;
	FLiveEditBinding BindingData;
	FString EventName;
	FString Description;
	TArray< ELiveEditControllerType::Type > PermittedControlTypes;
};



/**
 *
 * SLiveEditorBlueprintBindingWindow
 *
 **/

void SLiveEditorBlueprintBindingWindow::Construct(const FArguments& InArgs, FBindState *_Owner)
{
	check( _Owner != NULL );
	Owner = _Owner;

	ControlTypes.Empty();
	const TArray< ELiveEditControllerType::Type > &PermittedControlTypes = Owner->GetPermittedControlTypes();
	for( int32 i = 0; i < PermittedControlTypes.Num(); ++i )
	{
		ControlTypes.Add( MakeShareable( new ELiveEditControllerType::Type(PermittedControlTypes[i]) ) );
	}
	if ( PermittedControlTypes.Num() > 0 )
	{
		Owner->GetBindingData().ControlType = PermittedControlTypes[0];
	}

	ChildSlot
	[
		SNew( SVerticalBox )
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f)
		[
			SNew( STextBlock )
			.Text( this, &SLiveEditorBlueprintBindingWindow::GetStateText )
			.ColorAndOpacity( FLinearColor::White )
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f)
		[
			SNew( SComboBox< TSharedPtr<ELiveEditControllerType::Type> > )
			.OptionsSource( &ControlTypes )
			.OnGenerateWidget( this, &SLiveEditorBlueprintBindingWindow::GenerateControlTypeItem )
			.OnSelectionChanged( this, &SLiveEditorBlueprintBindingWindow::OnTypeChanged )
			[
				SNew(STextBlock)
				.Text( this, &SLiveEditorBlueprintBindingWindow::GetSelectedControlTypeText )
			]
		]
	];
}

FText SLiveEditorBlueprintBindingWindow::GetStateText() const
{
	return Owner->GetStateText();
}

void SLiveEditorBlueprintBindingWindow::OnTypeChanged(TSharedPtr<ELiveEditControllerType::Type> InType, ESelectInfo::Type)
{
	Owner->GetBindingData().ControlType = *InType;
}

TSharedRef<SWidget> SLiveEditorBlueprintBindingWindow::GenerateControlTypeItem(TSharedPtr<ELiveEditControllerType::Type> InType)
{
	return SNew(STextBlock) 
		.Text( GetControlTypeText(*InType) );
}

FText SLiveEditorBlueprintBindingWindow::GetSelectedControlTypeText() const
{
	return GetControlTypeText(Owner->GetBindingData().ControlType);
}

FText SLiveEditorBlueprintBindingWindow::GetControlTypeText(ELiveEditControllerType::Type Type) const
{
	switch( Type )
	{
	case ELiveEditControllerType::NotSet:
		return LOCTEXT("NotSet", "Not Set");
	case ELiveEditControllerType::NoteOnOff:
		return LOCTEXT("NoteOnOrOff", "Note On/Off");
	case ELiveEditControllerType::ControlChangeContinuous:
		return LOCTEXT("ControlChangeContinuous", "Continuous Knob");
	case ELiveEditControllerType::ControlChangeFixed:
		return LOCTEXT("ControlChangeFixed", "Fixed Knob/Slider");
	}

	return FText::GetEmpty();
}


/**
 *
 * FLiveEditorDeviceSetupWizard
 *
 **/

FLiveEditorBlueprintBindingWizard::FLiveEditorBlueprintBindingWizard()
:	FLiveEditorWizardBase(-1),
	Blueprint(NULL)
{
}
FLiveEditorBlueprintBindingWizard::~FLiveEditorBlueprintBindingWizard()
{
}

void FLiveEditorBlueprintBindingWizard::Start( UBlueprint &_Blueprint )
{
	Blueprint = &_Blueprint;
	ClearStates();

	if ( GenerateStates() )
	{
		FLiveEditorWizardBase::Start(0);
	}
}

void FLiveEditorBlueprintBindingWizard::OnWizardFinished( struct FLiveEditorDeviceData &Data )
{
	check( FLiveEditorManager::Get().GetLiveEditorUserData() != NULL );
	FLiveEditorManager::Get().GetLiveEditorUserData()->Save();
	FLiveEditorManager::Get().FinishBlueprintBinding( *Blueprint );

	Blueprint = NULL;
}

bool FLiveEditorBlueprintBindingWizard::GenerateStates()
{
	if ( Blueprint == NULL )
	{
		return false;
	}

	int32 FoundCount = 0;

	for ( TArray<UEdGraph*>::TIterator GraphIt(Blueprint->UbergraphPages); GraphIt; ++GraphIt )
	{
		for ( TArray<UEdGraphNode*>::TIterator NodeIt( (*GraphIt)->Nodes ); NodeIt; ++NodeIt )
		{
			UEdGraphNode *GraphNode = *NodeIt;

			//it could be a standard RegisterForLiveEditEvent
			UK2Node_CallFunction *RegisterFunctionNode = Cast<UK2Node_CallFunction>( GraphNode );
			if ( RegisterFunctionNode && RegisterFunctionNode->FunctionReference.GetMemberName().ToString() == FString(TEXT("RegisterForLiveEditEvent")) )
			{
				AddState( FoundCount, new FBindState(Blueprint->GetName(), *RegisterFunctionNode, FoundCount+1) );
				++FoundCount;
				continue;
			}

			//or it could be the uber object: UK2Node_LiveEditObject
			UK2Node_LiveEditObject *LiveEditObjectNode = Cast<UK2Node_LiveEditObject>( GraphNode );
			if ( LiveEditObjectNode )
			{
				AddState( FoundCount, new FBindState(Blueprint->GetName(), *LiveEditObjectNode, FoundCount+1) );
				++FoundCount;
				continue;
			}
		}
	}

	SetFinalState(FoundCount);
	return (FoundCount > 0);
}

#undef LOCTEXT_NAMESPACE
