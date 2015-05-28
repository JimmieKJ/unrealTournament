// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/** A curve owner interface for displaying animation curves in sequencer. */
class FSequencerCurveOwner : public FCurveOwnerInterface
{
public:
	FSequencerCurveOwner(TSharedPtr<FSequencerNodeTree> InSequencerNodeTree);

	/** FCurveOwnerInterface */
	virtual TArray<FRichCurveEditInfoConst> GetCurves() const override;
	virtual TArray<FRichCurveEditInfo> GetCurves() override;
	virtual void ModifyOwner() override;
	virtual void MakeTransactional() override;
	virtual void OnCurveChanged( const TArray<FRichCurveEditInfo>& ChangedCurveEditInfos ) override;
	virtual bool IsValidCurve( FRichCurveEditInfo CurveInfo ) override;

private:
	/** The FSequencerNodeTree used to build the curve owner. */
	TSharedPtr<FSequencerNodeTree> SequencerNodeTree;

	/** The ordered array of const curves used to implement the curve owner interface. */
	TArray<FRichCurveEditInfoConst> ConstCurves;

	/** The ordered array or curves used to implement the curve owner interface. */
	TArray<FRichCurveEditInfo> Curves;

	/** A map of curve edit infos to their corresponding sections. */
	TMap<FRichCurveEditInfo, UMovieSceneSection*> EditInfoToSectionMap;
};
