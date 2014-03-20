// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Tools for director tracks
 */
class FDirectorTrackEditor : public FMovieSceneTrackEditor
{
public:
	/**
	 * Constructor
	 *
	 * @param InSequencer	The sequencer instance to be used by this tool
	 */
	FDirectorTrackEditor( TSharedRef<ISequencer> InSequencer );
	~FDirectorTrackEditor();

	/**
	 * Creates an instance of this class.  Called by a sequencer 
	 *
	 * @param OwningSequencer The sequencer instance to be used by this tool
	 * @return The new instance of this class
	 */
	static TSharedRef<FMovieSceneTrackEditor> CreateTrackEditor( TSharedRef<ISequencer> OwningSequencer );

	/** FMovieSceneTrackEditor Interface */
	virtual bool SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const OVERRIDE;
	virtual TSharedRef<ISequencerSection> MakeSectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack* Track ) OVERRIDE;
	virtual void AddKey(const FGuid& ObjectGuid, UObject* AdditionalAsset = NULL) OVERRIDE;
	virtual void Tick(float DeltaTime) OVERRIDE;
	virtual void BuildObjectBindingContextMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass) OVERRIDE;

private:
	/** Delegate for AnimatablePropertyChanged in AddKey */
	void AddKeyInternal(float AutoKeyTime, const FGuid ObjectGuid);

private:
	/** The Thumbnail pool which draws all the viewport thumbnails for the director track */
	TSharedPtr<class FShotThumbnailPool> ThumbnailPool;
};


/**
 * Shot Thumbnail pool, which keeps a list of thumbnails that need to be drawn
 * and draws them incrementally
 */
class FShotThumbnailPool
{
public:
	FShotThumbnailPool(TSharedPtr<ISequencer> InSequencer, uint32 InMaxThumbnailsToDrawAtATime = 1);

	/** Requests that the passed in thumbnails need to be drawn */
	void AddThumbnailsNeedingRedraw(const TArray< TSharedPtr<class FShotThumbnail> >& InThumbnails);

	/** Informs the pool that the thumbnails passed in no longer need to be drawn */
	void RemoveThumbnailsNeedingRedraw(const TArray< TSharedPtr<class FShotThumbnail> >& InThumbnails);

	/** Draws a small number of thumbnails that are enqueued for drawing */
	void DrawThumbnails();

private:
	/** Parent sequencer we're drawing thumbnails for */
	TWeakPtr<ISequencer> Sequencer;

	/** Thumbnails enqueued for drawing */
	TArray< TSharedPtr<class FShotThumbnail> > ThumbnailsNeedingDraw;

	/** How many thumbnails we are allowed to draw in a single DrawThumbnails call */
	uint32 MaxThumbnailsToDrawAtATime;
};


/**
 * Shot Thumbnail, which keeps a Texture to be displayed by a viewport
 */
class FShotThumbnail : public ISlateViewport, public TSharedFromThis<FShotThumbnail>
{
public:
	FShotThumbnail(TSharedPtr<class FShotSection> InSection, TRange<float> InTimeRange);
	~FShotThumbnail();

	/* ISlateViewport interface */
	virtual FIntPoint GetSize() const OVERRIDE;
	virtual class FSlateShaderResource* GetViewportRenderTargetTexture() const OVERRIDE;
	virtual bool RequiresVsync() const OVERRIDE;

	/** Gets the time that this thumbnail is a rendering of */
	float GetTime() const;

	/** Renders the thumbnail to the texture */
	void DrawThumbnail();

	/** Copies the incoming render target to this thumbnails texture */
	void CopyTextureIn(class FSlateRenderTargetRHI* InTexture);

	/** Gets the curve for fading in the thumbnail */
	float GetFadeInCurve() const;

	/** Returns whether this thumbnail is visible based on the shot section geometry visibility */
	bool IsVisible() const;

	bool IsValid() const;

private:
	/** Parent shot section we are a thumbnail of */
	TWeakPtr<class FShotSection> OwningSection;
	
	/** The Texture RHI that holds the thumbnail */
	class FSlateTexture2DRHIRef* Texture;

	/** Where in time this thumbnail is a rendering of */
	TRange<float> TimeRange;

	/** The fade in curve, so the thumbnails don't just pop in */
	FCurveSequence ThumbnailFadeAnimation;
	FCurveHandle ThumbnailFadeCurve;
};


/**
 * Shot section, which paints and ticks the appropriate section
 */
class FShotSection : public ISequencerSection, public TSharedFromThis<FShotSection>
{
public:
	FShotSection( TSharedPtr<ISequencer> InSequencer, TSharedPtr<FShotThumbnailPool> InThumbnailPool, UMovieSceneSection& InSection, UObject* InTargetObject );
	~FShotSection();

	/** ISequencerSection interface */
	virtual UMovieSceneSection* GetSectionObject() OVERRIDE;
	virtual int32 OnPaintSection( const FGeometry& AllottedGeometry, const FSlateRect& SectionClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, bool bParentEnabled ) const OVERRIDE;
	virtual void Tick( const FGeometry& AllottedGeometry, const FGeometry& ParentGeometry, const double InCurrentTime, const float InDeltaTime ) OVERRIDE;
	virtual FString GetDisplayName() const OVERRIDE { return TEXT("Director Track"); }
	virtual FString GetSectionTitle() const OVERRIDE;
	virtual float GetSectionHeight() const OVERRIDE;
	virtual void GenerateSectionLayout( class ISectionLayoutBuilder& LayoutBuilder ) const OVERRIDE {}
	virtual FReply OnSectionDoubleClicked( const FGeometry& SectionGeometry, const FPointerEvent& MouseEvent ) OVERRIDE;

	/** Gets the thumbnail width */
	uint32 GetThumbnailWidth() const;

	/** Regenerates all viewports and thumbnails at the new size */
	void RegenerateViewportThumbnails(const FIntPoint& Size);

	/** Draws the passed in viewport thumbnail and copies it to the thumbnail's texture */
	void DrawViewportThumbnail(TSharedPtr<FShotThumbnail> ShotThumbnail);

	/** Calculates and sets the thumbnail width, and resizes if it is different than before */
	void CalculateThumbnailWidthAndResize();

	/** Gets the time range of what in the sequencer is visible */
	TRange<float> GetVisibleTimeRange() const {return VisibleTimeRange;}

private:
	/** The section we are visualizing */
	UMovieSceneSection* Section;
	/** The parent sequencer we are a part of */
	TWeakPtr<ISequencer> Sequencer;
	/** The actual camera actor we are possessing */
	TWeakObjectPtr<ACameraActor> Camera;

	/** The thumbnail pool that we are sending all of our thumbnails to */
	TWeakPtr<FShotThumbnailPool> ThumbnailPool;

	/** A list of all thumbnails this shot section has */
	TArray< TSharedPtr<FShotThumbnail> > Thumbnails;
	/** The width of our thumbnails */
	uint32 ThumbnailWidth;
	/** The stored size of this section in the Slate geometry */
	FIntPoint StoredSize;
	/** The stored start time, to query for invalidations */
	float StoredStartTime;

	/** Cached Time Range of the visible parent section area */
	TRange<float> VisibleTimeRange;
	
	/** An internal viewport scene we use to render the thumbnails with */
	TSharedPtr<FSceneViewport> InternalViewportScene;
	/** An internal editor viewport client to render the thumbnails with */
	TSharedPtr<FLevelEditorViewportClient> InternalViewportClient;
};
