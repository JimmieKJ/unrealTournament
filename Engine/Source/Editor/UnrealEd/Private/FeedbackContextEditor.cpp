// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "UnrealEd.h"
#include "FeedbackContextEditor.h"
#include "Dialogs/SBuildProgress.h"
#include "Editor/MainFrame/Public/MainFrame.h"

/** Called to cancel the slow task activity */
DECLARE_DELEGATE( FOnCancelClickedDelegate );

/**
 * Simple "slow task" widget
 */
class SSlowTaskWidget : public SBorder
{
	/** The maximum number of secondary bars to show on the widget */
	static const int32 MaxNumSecondaryBars = 3;

	/** The width of the dialog, and horizontal padding */
	static const int32 FixedWidth = 600, FixedPaddingH = 24;

	/** The heights of the progress bars on this widget */
	static const int32 MainBarHeight = 12, SecondaryBarHeight = 3;
public:
	SLATE_BEGIN_ARGS( SSlowTaskWidget )	{ }

		/** Called to when an asset is clicked */
		SLATE_EVENT( FOnCancelClickedDelegate, OnCancelClickedDelegate )

		/** The feedback scope stack that we are presenting to the user */
		SLATE_ARGUMENT( const FScopedSlowTaskStack*, ScopeStack )

	SLATE_END_ARGS()

	/** Construct this widget */
	void Construct( const FArguments& InArgs )
	{
		// Ensure this gets ticked at least once
		LastTickTime = 0;

		OnCancelClickedDelegate = InArgs._OnCancelClickedDelegate;
		ScopeStack = InArgs._ScopeStack;

		TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox)

			// Construct the main progress bar and text
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0, 0, 0, 5.f))
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.HeightOverride(24.f)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						[
							SNew( STextBlock )
							.AutoWrapText(true)
							.Text( this, &SSlowTaskWidget::GetProgressText, 0 )
							// The main font size dynamically changes depending on the content
							.Font( this, &SSlowTaskWidget::GetMainTextFont )
						]

						+ SHorizontalBox::Slot()
						.Padding(FMargin(5.f, 0, 0, 0))
						.AutoWidth()
						[
							SNew( STextBlock )
							.Text( this, &SSlowTaskWidget::GetPercentageText )
							// The main font size dynamically changes depending on the content
							.Font( FSlateFontInfo( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Light.ttf"), 14, EFontHinting::AutoLight ) )
						]
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.HeightOverride(MainBarHeight)
					[
						SNew(SProgressBar)
						.BorderPadding(FVector2D::ZeroVector)
						.Percent( this, &SSlowTaskWidget::GetProgressFraction, 0 )
						.BackgroundImage( FEditorStyle::GetBrush("ProgressBar.ThinBackground") )
						.FillImage( FEditorStyle::GetBrush("ProgressBar.ThinFill") )
					]
				]
			]
			
			// Secondary progress bars
			+ SVerticalBox::Slot()
			.Padding(FMargin(0.f, 8.f, 0.f, 0.f))
			[
				SAssignNew(SecondaryBars, SVerticalBox)
			];
		

		if ( OnCancelClickedDelegate.IsBound() )
		{
			VerticalBox->AddSlot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(10.0f, 7.0f)
				[
					SNew(SButton)
					.Text( NSLOCTEXT("FeedbackContextProgress", "Cancel", "Cancel") )
					.HAlign(EHorizontalAlignment::HAlign_Center)
					.OnClicked(this, &SSlowTaskWidget::OnCancel)
				];
		}

		SBorder::Construct( SBorder::FArguments()
			.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
			.VAlign(VAlign_Center)
			.Padding(FMargin(FixedPaddingH))
			[
				SNew(SBox).WidthOverride(FixedWidth) [ VerticalBox ]
			]
		);

		// Make sure all our bars are set up
		UpdateDynamicProgressBars();
	}

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		UpdateDynamicProgressBars();
	}

private:

	/** Updates the dynamic progress bars for this widget */
	void UpdateDynamicProgressBars()
	{
		static const double VisibleScopeThreshold = 0.5;

		DynamicProgressIndices.Empty();
		
		// Always show the first one
		DynamicProgressIndices.Add(0);

		for (int32 Index = 1; Index < ScopeStack->Num(); ++Index)
		{
			const auto* Scope = (*ScopeStack)[Index];

			if (Scope->bVisibleOnUI && !Scope->DefaultMessage.IsEmpty())
			{
				const auto TimeOpen = FPlatformTime::Seconds() - Scope->StartTime;
				const auto WorkDone = ScopeStack->GetProgressFraction(Index);

				// We only show visible scopes if they have been opened a while, and have a reasonable amount of work left
				if (WorkDone * TimeOpen > VisibleScopeThreshold)
				{
					DynamicProgressIndices.Add(Index);
				}

				if (DynamicProgressIndices.Num() == MaxNumSecondaryBars)
				{
					break;
				}
			}
		}

		// Create progress bars for anything that we haven't cached yet
		// We don't destroy old widgets, they just remain ghosted until shown again
		for (int32 Index = SecondaryBars->GetChildren()->Num() + 1; Index < DynamicProgressIndices.Num(); ++Index)
		{
			CreateSecondaryBar(Index);
		}
	}

	/** Create a progress bar for the specified index */
	void CreateSecondaryBar(int32 Index) 
	{
		SecondaryBars->AddSlot()
		.Padding( 0.f, 16.f, 0.f, 0.f )
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.Padding( 0.f, 0.f, 0.f, 4.f )
			.AutoHeight()
			[
				SNew( STextBlock )
				.Text( this, &SSlowTaskWidget::GetProgressText, Index )
				.Font( FSlateFontInfo( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 9, EFontHinting::AutoLight ) )
				.ColorAndOpacity( FSlateColor::UseSubduedForeground() )
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.HeightOverride(SecondaryBarHeight)
				[
					SNew(SBorder)
					.Padding(0)
					.BorderImage(FEditorStyle::GetBrush("NoBorder"))
					.ColorAndOpacity( this, &SSlowTaskWidget::GetSecondaryProgressBarTint, Index )
					[
						SNew(SProgressBar)
						.BorderPadding(FVector2D::ZeroVector)
						.Percent( this, &SSlowTaskWidget::GetProgressFraction, Index )
						.BackgroundImage( FEditorStyle::GetBrush("ProgressBar.ThinBackground") )
						.FillImage( FEditorStyle::GetBrush("ProgressBar.ThinFill") )
					]
				]
			]
		];
	}

private:

	/** The main text that we will display in the window */
	FText GetPercentageText() const
	{
		const float ProgressInterp = ScopeStack->GetProgressFraction(0);

		FFormatOrderedArguments Args;
		Args.Add(int(ProgressInterp * 100));

		return FText::Format(NSLOCTEXT("FeedbackContextEditor", "MainProgressDisplayText", "{0}%"), Args);
	}

	/** Calculate the best font to display the main text with */
	FSlateFontInfo GetMainTextFont() const
	{
		TSharedRef<FSlateFontMeasure> MeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

		const int32 MaxFontSize = 14;
		FSlateFontInfo FontInfo( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Light.ttf"), MaxFontSize, EFontHinting::AutoLight );

		const FText MainText = GetProgressText(0);
		const int32 MaxTextWidth = FixedWidth - FixedPaddingH*2;
		while( FontInfo.Size > 9 && MeasureService->Measure(MainText, FontInfo).X > MaxTextWidth )
		{
			FontInfo.Size -= 4;
		}

		return FontInfo;
	}

	/** Get the tint for a secondary progress bar */
	FLinearColor GetSecondaryProgressBarTint(int32 Index) const
	{
		if (!DynamicProgressIndices.IsValidIndex(Index) || !ScopeStack->IsValidIndex(DynamicProgressIndices[Index]))
		{
			return FLinearColor::White.CopyWithNewOpacity(0.25f);
		}
		return FLinearColor::White;
	}

	/** Get the fractional percentage of completion for a progress bar */
	TOptional<float> GetProgressFraction(int32 Index) const
	{
		if (DynamicProgressIndices.IsValidIndex(Index) && ScopeStack->IsValidIndex(DynamicProgressIndices[Index]))
		{
			return ScopeStack->GetProgressFraction(DynamicProgressIndices[Index]);
		}
		return TOptional<float>();
	}

	/** Get the text to display for a progress bar */
	FText GetProgressText(int32 Index) const
	{
		if (DynamicProgressIndices.IsValidIndex(Index) && ScopeStack->IsValidIndex(DynamicProgressIndices[Index]))
		{
			return (*ScopeStack)[DynamicProgressIndices[Index]]->GetCurrentMessage();
		}

		return FText();
	}

	/** Called when the cancel button is clicked */
	FReply OnCancel()
	{
		OnCancelClickedDelegate.ExecuteIfBound();
		return FReply::Handled();
	}

private:

	/** Delegate to invoke if the user clicks cancel */
	FOnCancelClickedDelegate OnCancelClickedDelegate;

	/** The scope stack that we are reflecting */
	const FScopedSlowTaskStack* ScopeStack;

	/** The vertical box containing the secondary progress bars */
	TSharedPtr<SVerticalBox> SecondaryBars;

	/** Array mapping progress bar index -> scope stack index. Updated every tick. */
	TArray<int32> DynamicProgressIndices;

public:
	/** Publicly accessible tick time to throttle slate ticking */
	static double LastTickTime;
};

/** Static integer definitions required on some builds where the linker needs access to these */
const int32 SSlowTaskWidget::MaxNumSecondaryBars;
const int32 SSlowTaskWidget::FixedWidth;
const int32 SSlowTaskWidget::FixedPaddingH;;
const int32 SSlowTaskWidget::MainBarHeight;
const int32 SSlowTaskWidget::SecondaryBarHeight;
double SSlowTaskWidget::LastTickTime = 0;

static void TickSlate()
{
	static double MinFrameTime = 0.05;		// Only update at 20fps so as not to slow down the actual task
	if (FPlatformTime::Seconds() - SSlowTaskWidget::LastTickTime > MinFrameTime)
	{
		SSlowTaskWidget::LastTickTime = FPlatformTime::Seconds();

		// Tick Slate application
		FSlateApplication::Get().Tick();

		// Sync the game thread and the render thread. This is needed if many StatusUpdate are called
		FSlateApplication::Get().GetRenderer()->Sync();
	}
}

FFeedbackContextEditor::FFeedbackContextEditor()
	: HasTaskBeenCancelled(false)
{
	
}

void FFeedbackContextEditor::Serialize( const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category )
{
	if( !GLog->IsRedirectingTo( this ) )
	{
		GLog->Serialize( V, Verbosity, Category );
	}
}

void FFeedbackContextEditor::StartSlowTask( const FText& Task, bool bShowCancelButton )
{
	FFeedbackContext::StartSlowTask( Task, bShowCancelButton );

	// Attempt to parent the slow task window to the slate main frame
	TSharedPtr<SWindow> ParentWindow;
	if( FModuleManager::Get().IsModuleLoaded( "MainFrame" ) )
	{
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>( "MainFrame" );
		ParentWindow = MainFrame.GetParentWindow();
	}

	if( GIsEditor && ParentWindow.IsValid())
	{
		GSlowTaskOccurred = GIsSlowTask;

		// Don't show the progress dialog if the Build Progress dialog is already visible
		bool bProgressWindowShown = BuildProgressWidget.IsValid();

		// Don't show the progress dialog if a Slate menu is currently open
		const bool bHaveOpenMenu = FSlateApplication::Get().AnyMenusVisible();

		if( bHaveOpenMenu )
		{
			UE_LOG(LogSlate, Warning, TEXT("Prevented a slow task dialog from being summoned while a context menu was open") );
		}

		// reset the cancellation flag
		HasTaskBeenCancelled = false;

		if (!bProgressWindowShown && !bHaveOpenMenu && FSlateApplication::Get().CanDisplayWindows())
		{
			FOnCancelClickedDelegate OnCancelClicked;
			if ( bShowCancelButton )
			{
				// The cancel button is only displayed if a delegate is bound to it.
				OnCancelClicked = FOnCancelClickedDelegate::CreateRaw(this, &FFeedbackContextEditor::OnUserCancel);
			}

			TSharedRef<SWindow> SlowTaskWindowRef = SNew(SWindow)
				.SizingRule(ESizingRule::Autosized)
				.AutoCenter(EAutoCenter::PreferredWorkArea)
				.IsPopupWindow(true)
				.CreateTitleBar(true)
				.ActivateWhenFirstShown(true);

			SlowTaskWidget = SNew(SSlowTaskWidget)
				.ScopeStack(&ScopeStack)
				.OnCancelClickedDelegate( OnCancelClicked );
			SlowTaskWindowRef->SetContent( SlowTaskWidget.ToSharedRef() );

			SlowTaskWindow = SlowTaskWindowRef;

			const bool bSlowTask = true;
			FSlateApplication::Get().AddModalWindow( SlowTaskWindowRef, ParentWindow, bSlowTask );

			SlowTaskWindowRef->ShowWindow();

			TickSlate();
		}

		FPlatformSplash::SetSplashText( SplashTextType::StartupProgress, *Task.ToString() );
	}
}

void FFeedbackContextEditor::FinalizeSlowTask()
{
	TSharedPtr<SWindow> ParentWindow;
	if( FModuleManager::Get().IsModuleLoaded( "MainFrame" ) )
	{
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>( "MainFrame" );
		ParentWindow = MainFrame.GetParentWindow();
	}

	if( GIsEditor && ParentWindow.IsValid())
	{
		if( SlowTaskWindow.IsValid() )
		{
			SlowTaskWindow.Pin()->RequestDestroyWindow();
			SlowTaskWindow.Reset();
			SlowTaskWidget.Reset();
		}
	}

	FFeedbackContext::FinalizeSlowTask( );
}

void FFeedbackContextEditor::ProgressReported( const float TotalProgressInterp, FText DisplayMessage )
{
	// Clean up deferred cleanup objects from rendering thread every once in a while.
	static double LastTimePendingCleanupObjectsWhereDeleted;
	if( FPlatformTime::Seconds() - LastTimePendingCleanupObjectsWhereDeleted > 1 )
	{
		// Get list of objects that are pending cleanup.
		FPendingCleanupObjects* PendingCleanupObjects = GetPendingCleanupObjects();
		// Flush rendering commands in the queue.
		FlushRenderingCommands();
		// It is now safe to delete the pending clean objects.
		delete PendingCleanupObjects;
		// Keep track of time this operation was performed so we don't do it too often.
		LastTimePendingCleanupObjectsWhereDeleted = FPlatformTime::Seconds();
	}

	if (FSlateApplication::Get().CanDisplayWindows())
	{
		if (BuildProgressWidget.IsValid())
		{
			if (!DisplayMessage.IsEmpty())
			{
				BuildProgressWidget->SetBuildStatusText(DisplayMessage);
			}

			BuildProgressWidget->SetBuildProgressPercent(TotalProgressInterp * 100, 100);
			TickSlate();
		}
		else if (SlowTaskWidget.IsValid())
		{
			TickSlate();
		}
	}
	else
	{
		// Always show the top-most message
		for (auto& Scope : ScopeStack)
		{
			const FText ThisMessage = Scope->GetCurrentMessage();
			if (!ThisMessage.IsEmpty())
			{
				DisplayMessage = ThisMessage;
				break;
			}
		}

		if (!DisplayMessage.IsEmpty() && TotalProgressInterp > 0)
		{
            FFormatOrderedArguments Args;
            Args.Add(DisplayMessage);
            Args.Add(int(TotalProgressInterp * 100.f));

            DisplayMessage = FText::Format(NSLOCTEXT("FeedbackContextEditor", "ProgressDisplayText", "{0} ({1}%)"), Args);
		}
		FPlatformSplash::SetSplashText( SplashTextType::StartupProgress, *DisplayMessage.ToString() );
	}
}

/** Whether or not the user has canceled out of this dialog */
bool FFeedbackContextEditor::ReceivedUserCancel( void )
{
	const bool res = HasTaskBeenCancelled;
	HasTaskBeenCancelled = false;
	return res;
}

void FFeedbackContextEditor::OnUserCancel()
{
	HasTaskBeenCancelled = true;
}

/** 
 * Show the Build Progress Window 
 * @return Handle to the Build Progress Widget created
 */
TWeakPtr<class SBuildProgressWidget> FFeedbackContextEditor::ShowBuildProgressWindow()
{
	TSharedRef<SWindow> BuildProgressWindowRef = SNew(SWindow)
		.ClientSize(FVector2D(500,200))
		.IsPopupWindow(true);

	BuildProgressWidget = SNew(SBuildProgressWidget);
		
	BuildProgressWindowRef->SetContent( BuildProgressWidget.ToSharedRef() );

	BuildProgressWindow = BuildProgressWindowRef;
				
	// Attempt to parent the slow task window to the slate main frame
	TSharedPtr<SWindow> ParentWindow;

	if( FModuleManager::Get().IsModuleLoaded( "MainFrame" ) )
	{
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>( "MainFrame" );
		ParentWindow = MainFrame.GetParentWindow();
	}

	FSlateApplication::Get().AddModalWindow( BuildProgressWindowRef, ParentWindow, true );
	BuildProgressWindowRef->ShowWindow();	

	BuildProgressWidget->MarkBuildStartTime();
	
	if (FSlateApplication::Get().CanDisplayWindows())
	{
		TickSlate();
	}

	return BuildProgressWidget;
}

/** Close the Build Progress Window */
void FFeedbackContextEditor::CloseBuildProgressWindow()
{
	if( BuildProgressWindow.IsValid() && BuildProgressWidget.IsValid())
	{
		BuildProgressWindow.Pin()->RequestDestroyWindow();
		BuildProgressWindow.Reset();
		BuildProgressWidget.Reset();	
	}
}
