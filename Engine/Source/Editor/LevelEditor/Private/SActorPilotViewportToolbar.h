
#include "Editor/UnrealEd/Public/SViewportToolBar.h"

#include "SLevelEditor.h"
#include "Editor/UnrealEd/Public/SEditorViewportToolBarMenu.h"
#include "Editor/UnrealEd/Public/SEditorViewportToolBarButton.h"

class SLevelViewport;

#define LOCTEXT_NAMESPACE "SActorPilotViewportToolbar"

class SActorPilotViewportToolbar : public SViewportToolBar
{
public:
	SLATE_BEGIN_ARGS(SActorPilotViewportToolbar){}
		SLATE_ARGUMENT(TSharedPtr<SLevelViewport>, Viewport)
	SLATE_END_ARGS()

	FText GetActiveText() const
	{
		const AActor* Pilot = nullptr;
		auto ViewportPtr = Viewport.Pin();
		if (ViewportPtr.IsValid())
		{
			Pilot = ViewportPtr->GetLevelViewportClient().GetActiveActorLock().Get();
		}
		
		return Pilot ? FText::Format(LOCTEXT("ActiveText", "[ Pilot Active - {0} ]"), FText::FromString(Pilot->GetActorLabel())) : FText();
	}

	void Construct(const FArguments& InArgs)
	{
		SViewportToolBar::Construct(SViewportToolBar::FArguments());

		Viewport = InArgs._Viewport;

		auto& ViewportCommands = FLevelViewportCommands::Get();

		FToolBarBuilder ToolbarBuilder( InArgs._Viewport->GetCommandList(), FMultiBoxCustomization::None, nullptr/*InExtenders*/ );

		// Use a custom style
		FName ToolBarStyle = "ViewportMenu";
		ToolbarBuilder.SetStyle(&FEditorStyle::Get(), ToolBarStyle);
		ToolbarBuilder.SetStyle(&FEditorStyle::Get(), ToolBarStyle);
		ToolbarBuilder.SetLabelVisibility(EVisibility::Collapsed);

		ToolbarBuilder.BeginSection("ActorPilot");
		ToolbarBuilder.BeginBlockGroup();
		{
			static FName EjectActorPilotName = FName(TEXT("EjectActorPilot"));
			ToolbarBuilder.AddToolBarButton( ViewportCommands.EjectActorPilot, NAME_None, TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(), EjectActorPilotName );

			static FName ToggleActorPilotCameraViewName = FName(TEXT("ToggleActorPilotCameraView"));
			ToolbarBuilder.AddToolBarButton( ViewportCommands.ToggleActorPilotCameraView, NAME_None, TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(), ToggleActorPilotCameraViewName );
		}
		ToolbarBuilder.EndBlockGroup();
		ToolbarBuilder.EndSection();

		ToolbarBuilder.BeginSection("ActorPilot_Label");
			ToolbarBuilder.AddWidget(
				SNew(SBox)
				// Nasty hack to make this VAlign_center properly. The parent Box is set to VAlign_Bottom, so we can't fill properly.
				.HeightOverride(24.f)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.TextStyle(FEditorStyle::Get(), "LevelViewport.ActorPilotText")
						.Text(this, &SActorPilotViewportToolbar::GetActiveText)
					]
				]
			);
		ToolbarBuilder.EndSection();

		ChildSlot
		[
			SNew( SBorder )
			.BorderImage( FEditorStyle::GetBrush("NoBorder") )
			.Padding(FMargin(4.f, 0.f))
			// Color and opacity is changed based on whether or not the mouse cursor is hovering over the toolbar area
			.ColorAndOpacity( this, &SViewportToolBar::OnGetColorAndOpacity )
			[
				ToolbarBuilder.MakeWidget()
			]
		];
	}

private:

	/** The viewport that we are in */
	TWeakPtr<SLevelViewport> Viewport;
};

#undef LOCTEXT_NAMESPACE