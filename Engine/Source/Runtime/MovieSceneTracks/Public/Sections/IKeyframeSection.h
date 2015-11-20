#pragma once

template<typename KeyDataType>
class IKeyframeSection
{
public:
	virtual bool NewKeyIsNewData( float Time, const KeyDataType& KeyData ) const = 0;
	virtual bool HasKeys( const KeyDataType& KeyData ) const = 0;
	virtual void AddKey( float Time, const KeyDataType& KeyData, EMovieSceneKeyInterpolation KeyInterpolation ) = 0;
	virtual void SetDefault( const KeyDataType& KeyData ) = 0;
};