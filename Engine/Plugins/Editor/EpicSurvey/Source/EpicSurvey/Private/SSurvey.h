// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

class FSurvey;
class FQuestionBlock;

class SSurvey : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS( SSurvey )
	{ }
	SLATE_END_ARGS()


	/** Widget constructor */
	void Construct( const FArguments& Args, const TSharedRef< FEpicSurvey >& InEpicSurvey, const TSharedRef< FSurvey >& InSurvey );
	//virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

private:

	void ConstructLoadingLayout();
	void ConstructSurveyLayout();
	void ConstructFailureLayout();

	ECheckBoxState IsAnswerChecked( TWeakPtr< FQuestionBlock > BlockPtr, int32 QuestionIndex, int32 AnswerIndex ) const;

	void AnswerCheckStateChanged( ECheckBoxState CheckState, TWeakPtr< FQuestionBlock > BlockPtr, int32 QuestionIndex, int32 AnswerIndex );
	EVisibility CanSubmitSurvey() const;
	FReply SubmitSurvey();

	EVisibility CanPageNext() const;
	EVisibility CanPageBack() const;

	FReply PageNext();
	FReply PageBack();

	void DisplayPage( int32 NewPageIndex );

private:
	/** Initializes the survey once its done loading */
	EActiveTimerReturnType MonitorLoadStatePostConstruct( double InCurrentTime, float InDeltaTime );

	TSharedPtr< FEpicSurvey > EpicSurvey;
	TSharedPtr< FSurvey > Survey;
	TSharedPtr< FTextBlockStyle > TitleFont;
	SVerticalBox::FSlot* PageBox;
	TSharedPtr< SScrollBox > ScrollBox;

	TSharedPtr< STileView< TSharedRef< FQuestionBlock > > > ContentView;
	TSharedPtr< SBorder > ContentsContainer;
};

