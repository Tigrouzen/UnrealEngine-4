// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "InterpTrackEvent.generated.h"

/**
 *	A track containing discrete events that are triggered as its played back. 
 *	Events correspond to Outputs of the SeqAct_Interp in Kismet.
 *	There is no PreviewUpdateTrack function for this type - events are not triggered in editor.
 */


/** Information for one event in the track. */
USTRUCT()
struct FEventTrackKey
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	float Time;

	UPROPERTY(EditAnywhere, Category=EventTrackKey)
	FName EventName;


	FEventTrackKey()
		: Time(0)
	{
	}

};

UCLASS(HeaderGroup=Interpolation, MinimalAPI, meta=( DisplayName = "Event Track" ) )
class UInterpTrackEvent : public UInterpTrack
{
	GENERATED_UCLASS_BODY()

	/** Array of events to fire off. */
	UPROPERTY()
	TArray<struct FEventTrackKey> EventTrack;

	/** If events should be fired when passed playing the sequence forwards. */
	UPROPERTY(EditAnywhere, Category=InterpTrackEvent)
	uint32 bFireEventsWhenForwards:1;

	/** If events should be fired when passed playing the sequence backwards. */
	UPROPERTY(EditAnywhere, Category=InterpTrackEvent)
	uint32 bFireEventsWhenBackwards:1;

	/** If true, events on this track are fired even when jumping forwads through a sequence - for example, skipping a cinematic. */
	UPROPERTY(EditAnywhere, Category=InterpTrackEvent)
	uint32 bFireEventsWhenJumpingForwards:1;


	// Begin UInterpTrack Interface
	virtual int32 GetNumKeyframes() const OVERRIDE;
	virtual void GetTimeRange( float& StartTime, float& EndTime ) const OVERRIDE;
	virtual float GetTrackEndTime() const OVERRIDE;
	virtual float GetKeyframeTime( int32 KeyIndex ) const OVERRIDE;
	virtual int32 GetKeyframeIndex( float KeyTime ) const OVERRIDE;
	virtual int32 AddKeyframe( float Time, UInterpTrackInst* TrackInst, EInterpCurveMode InitInterpMode ) OVERRIDE;
	virtual int32 SetKeyframeTime( int32 KeyIndex, float NewKeyTime, bool bUpdateOrder = true ) OVERRIDE;
	virtual void RemoveKeyframe( int32 KeyIndex ) OVERRIDE;
	virtual int32 DuplicateKeyframe(int32 KeyIndex, float NewKeyTime, UInterpTrack* ToTrack = NULL) OVERRIDE;
	virtual bool GetClosestSnapPosition( float InPosition, TArray<int32>& IgnoreKeys, float& OutPosition ) OVERRIDE;
	virtual void UpdateTrack( float NewPosition, UInterpTrackInst* TrackInst, bool bJump ) OVERRIDE;
	virtual const FString GetEdHelperClassName() const OVERRIDE;
	virtual const FString GetSlateHelperClassName() const OVERRIDE;
	virtual class UTexture2D* GetTrackIcon() const OVERRIDE;
	virtual bool AllowStaticActors() OVERRIDE { return true; }
	virtual void DrawTrack( FCanvas* Canvas, UInterpGroup* Group, const FInterpTrackDrawParams& Params ) OVERRIDE;
	// End UInterpTrack Interface
};



