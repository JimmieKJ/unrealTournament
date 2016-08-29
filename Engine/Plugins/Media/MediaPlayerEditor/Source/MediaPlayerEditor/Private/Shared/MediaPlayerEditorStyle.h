// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


#define IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define BOX_BRUSH(RelativePath, ...) FSlateBoxBrush(RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define BORDER_BRUSH(RelativePath, ...) FSlateBorderBrush(RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define TTF_FONT(RelativePath, ...) FSlateFontInfo(RootToContentDir(RelativePath, TEXT(".ttf")), __VA_ARGS__)
#define OTF_FONT(RelativePath, ...) FSlateFontInfo(RootToContentDir(RelativePath, TEXT(".otf")), __VA_ARGS__)


/**
 * Implements the visual style of the media player editor UI.
 */
class FMediaPlayerEditorStyle
	: public FSlateStyleSet
{
public:

	/** Default constructor. */
	FMediaPlayerEditorStyle()
		: FSlateStyleSet("MediaPlayerEditorStyle")
	{
		const FVector2D Icon8x8(8.0f, 8.0f);
		const FVector2D Icon12x12(12.0f, 12.0f);
		const FVector2D Icon16x16(16.0f, 16.0f);
		const FVector2D Icon20x20(20.0f, 20.0f);
		const FVector2D Icon40x40(40.0f, 40.0f);

		SetContentRoot(FPaths::EnginePluginsDir() / TEXT("Media/MediaPlayerEditor/Content"));

		// buttons
		Set("MediaPlayerEditor.GoButton", new IMAGE_BRUSH("btn_go_12x", Icon12x12));
		Set("MediaPlayerEditor.ReloadButton", new IMAGE_BRUSH("btn_reload_12x", Icon12x12));
		Set("MediaPlayerEditor.SettingsButton", new IMAGE_BRUSH("btn_settings_16x", Icon12x12));

		// misc
		Set("MediaPlayerEditor.FileMediaSourcePrecached", new IMAGE_BRUSH("filemediasource_precached", Icon12x12));
		Set("MediaPlayerEditor.MediaSourceOpened", new IMAGE_BRUSH("mediasource_opened", Icon8x8));

		// tabs
		Set("MediaPlayerEditor.Tabs.Info", new IMAGE_BRUSH("tab_info_16x", Icon16x16));
		Set("MediaPlayerEditor.Tabs.Media", new IMAGE_BRUSH("tab_media_16x", Icon16x16));
		Set("MediaPlayerEditor.Tabs.Player", new IMAGE_BRUSH("tab_player_16x", Icon16x16));
		Set("MediaPlayerEditor.Tabs.Playlist", new IMAGE_BRUSH("tab_playlist_16x", Icon16x16));
		Set("MediaPlayerEditor.Tabs.Stats", new IMAGE_BRUSH("tab_stats_16x", Icon16x16));

		// toolbar icons
		Set("MediaPlayerEditor.CloseMedia", new IMAGE_BRUSH("icon_eject_40x", Icon40x40));
		Set("MediaPlayerEditor.ForwardMedia", new IMAGE_BRUSH("icon_forward_40x", Icon40x40));
		Set("MediaPlayerEditor.ForwardMedia.Small", new IMAGE_BRUSH("icon_forward_40x", Icon20x20));
		Set("MediaPlayerEditor.NextMedia", new IMAGE_BRUSH("icon_step_40x", Icon40x40));
		Set("MediaPlayerEditor.NextMedia.Small", new IMAGE_BRUSH("icon_step_40x", Icon20x20));
		Set("MediaPlayerEditor.PauseMedia", new IMAGE_BRUSH("icon_pause_40x", Icon40x40));
		Set("MediaPlayerEditor.PauseMedia.Small", new IMAGE_BRUSH("icon_pause_40x", Icon20x20));
		Set("MediaPlayerEditor.PlayMedia", new IMAGE_BRUSH("icon_play_40x", Icon40x40));
		Set("MediaPlayerEditor.PlayMedia.Small", new IMAGE_BRUSH("icon_play_40x", Icon20x20));
		Set("MediaPlayerEditor.PreviousMedia", new IMAGE_BRUSH("icon_step_back_40x", Icon40x40));
		Set("MediaPlayerEditor.PreviousMedia.Small", new IMAGE_BRUSH("icon_step_back_40x", Icon20x20));
		Set("MediaPlayerEditor.ReverseMedia", new IMAGE_BRUSH("icon_reverse_40x", Icon40x40));
		Set("MediaPlayerEditor.ReverseMedia.Small", new IMAGE_BRUSH("icon_reverse_40x", Icon20x20));
		Set("MediaPlayerEditor.RewindMedia", new IMAGE_BRUSH("icon_rewind_40x", Icon40x40));
		Set("MediaPlayerEditor.RewindMedia.Small", new IMAGE_BRUSH("icon_rewind_40x", Icon20x20));
		Set("MediaPlayerEditor.StopMedia", new IMAGE_BRUSH("icon_stop_40x", Icon40x40));
		Set("MediaPlayerEditor.StopMedia.Small", new IMAGE_BRUSH("icon_stop_40x", Icon20x20));
		
		FSlateStyleRegistry::RegisterSlateStyle(*this);
	 }

	 /** Destructor. */
	 ~FMediaPlayerEditorStyle()
	 {
		FSlateStyleRegistry::UnRegisterSlateStyle(*this);
	 }
};


#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH
#undef TTF_FONT
#undef OTF_FONT
