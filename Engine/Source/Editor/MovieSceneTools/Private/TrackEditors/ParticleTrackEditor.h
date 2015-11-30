// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Tools for particle tracks
 */
class FParticleTrackEditor
	: public FMovieSceneTrackEditor
{
public:

	/**
	 * Constructor.
	 *
	 * @param InSequencer The sequencer instance to be used by this tool.
	 */
	FParticleTrackEditor( TSharedRef<ISequencer> InSequencer );

	/** Virtual destructor. */
	virtual ~FParticleTrackEditor() { }

	/**
	 * Creates an instance of this class.  Called by a sequencer.
	 *
	 * @param OwningSequencer The sequencer instance to be used by this tool.
	 * @return The new instance of this class.
	 */
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor( TSharedRef<ISequencer> OwningSequencer );

public:

	void AddParticleKey(const FGuid ObjectGuid, bool bTrigger);

public:

	// ISequencerTrackEditor interface

	virtual void BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass) override;
	virtual TSharedRef<ISequencerSection> MakeSectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack& Track ) override;
	virtual bool SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const override;
	virtual void BuildTrackContextMenu( FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track ) override;
	
	void AddParticleKey(const FGuid ObjectGuid);
private:

	/** Delegate for AnimatablePropertyChanged in AddKey. */
	virtual bool AddKeyInternal( float KeyTime, const TArray<UObject*> Objects);
};


/**
 * Class for particle sections.
 */
class FParticleSection
	: public ISequencerSection
	, public TSharedFromThis<FParticleSection>
{
public:

	FParticleSection( UMovieSceneSection& InSection, TSharedRef<ISequencer> InOwningSequencer );
	~FParticleSection();

public:

	// ISequencerSection interface
	virtual UMovieSceneSection* GetSectionObject() override;
	virtual FText GetDisplayName() const override;
	virtual FText GetSectionTitle() const override { return FText::GetEmpty(); }
	virtual float GetSectionHeight() const override;
	virtual void GenerateSectionLayout( class ISectionLayoutBuilder& LayoutBuilder ) const override;
	virtual int32 OnPaintSection( const FGeometry& AllottedGeometry, const FSlateRect& SectionClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, bool bParentEnabled ) const override;
	virtual const FSlateBrush* GetKeyBrush( FKeyHandle KeyHandle ) const override;
	virtual bool SectionIsResizable() const override { return false; }

private:

	/** The section we are visualizing. */
	UMovieSceneSection& Section;

	/** The sequencer that owns this section */
	TSharedRef<ISequencer> OwningSequencer;

	/** The UEnum for the EParticleKey enum */
	const UEnum* ParticleKeyEnum;

	const FSlateBrush* LeftKeyBrush;
	const FSlateBrush* RightKeyBrush;
};
