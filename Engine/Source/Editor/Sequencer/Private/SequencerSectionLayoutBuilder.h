// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

class ISequencerSection;
class FSequencerTrackNode;


class FSequencerSectionLayoutBuilder
	: public ISectionLayoutBuilder
{
public:
	FSequencerSectionLayoutBuilder( TSharedRef<FSequencerTrackNode> InRootNode );

public:

	// ISectionLayoutBuilder interface

	virtual void PushCategory( FName CategoryName, const FText& DisplayLabel ) override;
	virtual void SetSectionAsKeyArea( TSharedRef<IKeyArea> KeyArea ) override;
	virtual void AddKeyArea( FName KeyAreaName, const FText& DisplayName, TSharedRef<IKeyArea> KeyArea ) override;
	virtual void PopCategory() override;

private:

	/** Root node of the tree */
	TSharedRef<FSequencerTrackNode> RootNode;

	/** The current node that other nodes are added to */
	TSharedRef<FSequencerDisplayNode> CurrentNode;
};
