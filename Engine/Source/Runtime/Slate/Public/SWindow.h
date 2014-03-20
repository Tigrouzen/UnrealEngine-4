// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once


/** Notification that a window has been deactivated */
DECLARE_DELEGATE( FOnWindowDeactivated );

/** Notification that a window is about to be closed */
DECLARE_DELEGATE_OneParam( FOnWindowClosed, const TSharedRef<SWindow>& );

/** Notification that a window has been moved */
DECLARE_DELEGATE_OneParam( FOnWindowMoved, const TSharedRef<SWindow>& );

/** Override delegate for RequestDestroyWindow */
DECLARE_DELEGATE_OneParam( FRequestDestroyWindowOverride, const TSharedRef<SWindow>& );

/** Called when we need to switch game worlds for a window */
DECLARE_DELEGATE_RetVal_OneParam( int32, FOnSwitchWorldHack, int32 );

/** Enum to describe how to auto-center an SWindow */
namespace EAutoCenter
{
	enum Type
	{
		/** Dont auto-center the window */
		None,

		/** Auto-center the window on the primary work area */
		PrimaryWorkArea,

		/** Auto-center the window on the preferred work area, determined using GetPreferredWorkArea() */
		PreferredWorkArea,
	};
}

/** Enum to describe how windows are sized */
namespace ESizingRule
{
	enum Type
	{
		/* The windows size fixed and cannot be resized **/
		FixedSize,
		
		/** The window size is computed from its content and cannot be resized by users */
		Autosized,

		/** The window can be resized by users */
		UserSized,
	};
}

/**
 * SWindow is a platform-agnostic representation of a top-level window.
 */
class SLATE_API SWindow : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS( SWindow )
		: _Style( &FCoreStyle::Get().GetWidgetStyle<FWindowStyle>("Window") )
		, _Title()
		, _bDragAnywhere( false )
		, _AutoCenter( EAutoCenter::PreferredWorkArea )
		, _ScreenPosition( FVector2D::ZeroVector )
		, _ClientSize( FVector2D::ZeroVector )
		, _SupportsTransparency( false )
		, _InitialOpacity( 1.0f )
		, _IsInitiallyMaximized( false )
		, _SizingRule( ESizingRule::UserSized )
		, _IsPopupWindow( false )
		, _FocusWhenFirstShown( true )
		, _ActivateWhenFirstShown( true )
		, _UseOSWindowBorder( false )
		, _SupportsMaximize( true )
		, _SupportsMinimize( true )
		, _CreateTitleBar( true )
		, _SaneWindowPlacement( true )
		{}

		/** Style used to draw this window */
		SLATE_STYLE_ARGUMENT( FWindowStyle, Style )

		/** Title of the window */
		SLATE_ATTRIBUTE( FText, Title )

		/** When true, the window can be dragged from anywhere. */
		SLATE_ARGUMENT( bool, bDragAnywhere )

		/** The windows auto-centering mode. If set to anything other than None, then the
		    ScreenPosition value will be ignored */
		SLATE_ARGUMENT( EAutoCenter::Type, AutoCenter )

		/** Screen-space position where the window should be initially located. */
		SLATE_ARGUMENT( FVector2D, ScreenPosition )

		/** What the initial size of the window should be. */
		SLATE_ARGUMENT( FVector2D, ClientSize )

		/** Should this window support transparency */
		SLATE_ARGUMENT( bool, SupportsTransparency )

		/** The initial opacity of the window */
		SLATE_ARGUMENT( float, InitialOpacity )

		/** Is the window initially maximized */
		SLATE_ARGUMENT( bool, IsInitiallyMaximized )
		
		/** How the window should be sized */
		SLATE_ARGUMENT( ESizingRule::Type, SizingRule )

		/** True if this should be a 'pop-up' window */
		SLATE_ARGUMENT( bool, IsPopupWindow )

		/** Should this window be focused immediately after it is shown? */
		SLATE_ARGUMENT( bool, FocusWhenFirstShown )
	
		/** Should this window be activated immediately after it is shown? */
		SLATE_ARGUMENT( bool, ActivateWhenFirstShown )

		/** Use the default os look for the border of the window */
		SLATE_ARGUMENT( bool, UseOSWindowBorder )

		/** Can this window be maximized? */
		SLATE_ARGUMENT( bool, SupportsMaximize )
		
		/** Can this window be minimized? */
		SLATE_ARGUMENT( bool, SupportsMinimize )

		/** True if we should initially create a traditional title bar area.  If false, the user must embed the title
		    area content into the window manually, taking into account platform-specific considerations!  Has no
			effect for certain types of windows (popups, tool-tips, etc.) */
		SLATE_ARGUMENT( bool, CreateTitleBar )

		/** If the window appears off screen or is too large to safely fit this flag will force realistic 
		    constraints on the window and bring it back into view. */
		SLATE_ARGUMENT( bool, SaneWindowPlacement )

		SLATE_DEFAULT_SLOT( FArguments, Content )

	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs);

	/**
	 * Make a tool tip window
	 *
	 * @return The new SWindow
	 */
	static TSharedRef<SWindow> MakeToolTipWindow();


	/**
	 * Make cursor decorator window
	 *
	 * @return The new SWindow
	 */
	static TSharedRef<SWindow> MakeCursorDecorator();


	/**
	 * Make a notification window
	 *
	 * @return The new SWindow
	 */
	static TSharedRef<SWindow> MakeNotificationWindow();


	/**
	 * @param ContentSize      The size of content that we want to accommodate 
	 *
	 * @return The size of the window necessary to accommodate the given content */
	static FVector2D ComputeWindowSizeForContent( FVector2D ContentSize );


	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) OVERRIDE;

	/**
	 * Grabs the current window title
	 *
	 * @return The window's title
	 */
	FText GetTitle() const
	{
		return Title.Get();
	}

	/**
	 * Sets the current window title
	 *
	 * @param InTitle	The new title of the window
	 */
	void SetTitle( const FText& InTitle )
	{
		if (NativeWindow.IsValid())
		{
			Title = InTitle;
			NativeWindow->SetText( *InTitle.ToString() );
		}
	}

	/**
	 * Returns the size of the title bar as a Slate size parameter.  Does not take into account application scale!
	 *
	 * @return  Title bar size
	 */
	FOptionalSize GetTitleBarSize() const;

	/**	@return The initially desired screen position of the slate window */
	FVector2D GetInitialDesiredSizeInScreen() const;

	/**	@return The initially desired size of the slate window */
	FVector2D GetInitialDesiredPositionInScreen() const;

	/** Get the Geometry that describes this window. Windows in Slate are unique in that they know their own geometry. */
	FGeometry GetWindowGeometryInScreen() const;

	/** @return The geometry of the window in window space (i.e. position and AbsolutePosition are 0) */
	FGeometry GetWindowGeometryInWindow() const;

	/** @return The position of the window in screen space */
	FVector2D GetPositionInScreen() const;

	/** @return the size of the window in screen pixels */
	FVector2D GetSizeInScreen() const;

	/** @return the rectangle of the window for its non-maximized state */
	FSlateRect GetNonMaximizedRectInScreen() const;

	/** @return Rectangle that this window occupies in screen space */
	FSlateRect GetRectInScreen() const;

	/** @return the size of the window's usable client area. */
	FVector2D GetClientSizeInScreen() const;

	/** @return a clipping rectangle that represents this window in Window Space (i.e. always starts at 0,0) */
	FSlateRect GetClippingRectangleInWindow() const;

	/** Returns the margins used for the window border. This varies based on whether it's maximized or not. */
	FMargin GetWindowBorderSize() const;

	/** Relocate the window to a screenspace position specified by NewPosition */
	void MoveWindowTo( FVector2D NewPosition );
	/** Relocate the window to a screenspace position specified by NewPosition and resize it to NewSize */
	void ReshapeWindow( FVector2D NewPosition, FVector2D NewSize );
	void ReshapeWindow( const FSlateRect& InNewShape );
	/** Resize the window to be NewSize immediately */
	void Resize( FVector2D NewSize );

	/** Returns the rectangle of the screen the window is associated with */
	FSlateRect GetFullScreenInfo() const;

	/** @return Returns true if the window is currently morphing to a new position, shape and/or opacity */
	bool IsMorphing() const;
	/** @return Returns true if the window is currently morphing and is morphing by size */
	bool IsMorphingSize() const;
	/** Animate the window to TargetOpacity and TargetPosition over a short period of time */
	void MorphToPosition( const FCurveSequence& Sequence, const float TargetOpacity, const FVector2D& TargetPosition );
	/** Animate the window to TargetOpacity and TargetShape over a short period of time */
	void MorphToShape( const FCurveSequence& Sequence, const float TargetOpacity, const FSlateRect& TargetShape );
	/** Set a new morph shape and force the morph to run for at least one frame in order to reach that target */
	void UpdateMorphTargetShape( const FSlateRect& TargetShape );
	/** Set a new morph position and force the morph to run for at least one frame in order to reach that target */
	void UpdateMorphTargetPosition( const FVector2D& TargetPosition );
	/** @return Returns the currently set morph target position */
	FVector2D GetMorphTargetPosition() const;
	/** @return Returns the currently set morph target shape */
	FSlateRect GetMorphTargetShape() const;

	/** Flashed the window, used for drawing attention to modal dialogs */
	void FlashWindow();

	/** 
	 * Bring the window to the front 
	 *
	 * @param bForce	Forces the window to the top of the Z order, even if that means stealing focus from other windows
	 *					In general do not pass force in.  It can be useful for some window types, like game windows where not forcing it to the front
	 *					would cause mouse capture and mouse lock to happen but without the window visible
	 */
	void BringToFront( bool bForce = false );

	/** @hack Force a window to front even if a different application is in front. */
	void HACK_ForceToFront();

	/** Sets the actual screen position of the window. THIS SHOULD ONLY BE CALLED BY THE OS */
	void SetCachedScreenPosition(FVector2D NewPosition);

	/**	Sets the actual size of the window. THIS SHOULD ONLY BE CALLED BY THE OS */
	void SetCachedSize(FVector2D NewSize);

	TSharedPtr<FGenericWindow> GetNativeWindow();
	TSharedPtr<const FGenericWindow> GetNativeWindow() const ;

	/** 
	 * Returns whether or not this window is a descendant of the specfied parent window
	 *
	 * @param ParentWindow	The window to check
	 * @return true if the window is a child of ParentWindow, false otherwise.
	 */
	bool IsDescendantOf( const TSharedPtr<SWindow>& ParentWindow ) const;

	/**
	 * Sets the native OS window associated with this SWindow
	 *
	 * @param InNativeWindow	The native window
	 */
	void SetNativeWindow( TSharedRef<FGenericWindow> InNativeWindow );
	
	/**
	 * Sets the widget content for this window
	 *
	 * @param	InContent	The widget to use as content for this window
	 */
	void SetContent( TSharedRef<SWidget> InContent );

	/**
	 * Gets the widget content for this window
	 *
	 * @return	The widget content for this window
	 */
	TSharedRef<const SWidget> GetContent() const;

	/**
	 * Adds content to draw on top of the entire window
	 *
	 * @param	InZOrder	Z-order to use for this widget
	 * 
	 * @return The added overlay slot so that it can be configured and populated
	 */
	SOverlay::FOverlaySlot& AddOverlaySlot( const int32 ZOrder = INDEX_NONE );

	/**
	 * Removes a widget that is being drawn over the entire window
	 *
	 * @param	InContent	The widget to remove
	 */
	void RemoveOverlaySlot( const TSharedRef<SWidget>& InContent );

	/** Return a new slot in the popup layer. Assumes that the window has a popup layer. */
	struct FPopupLayerSlot& AddPopupLayerSlot();

	/** Counterpart to AddPopupLayerSlot */
	void RemovePopupLayerSlot( const TSharedRef<SWidget>& WidgetToRemove );

	/**
	 * Sets a widget to use as a full window overlay, or clears an existing widget if set.  When set, this widget will be drawn on top of all other window content.
	 *
	 * @param	InContent	The widget to use for full window overlay content, or NULL for no overlay
	 */
	void SetFullWindowOverlayContent( TSharedPtr<SWidget> InContent );

	/**
	 * Begins a transition from showing regular window content to overlay content
	 * During the transition we show both sets of content
	 */
	void BeginFullWindowOverlayTransition();

	/**
	 * Ends a transition from showing regular window content to overlay content
	 * When this is called content occluded by the full window overlay(if there is one) will be physically hidden
	 */
	void EndFullWindowOverlayTransition();

	/**
	 * Checks to see if there is content assigned as a full window overlay
	 *
	 * @return	True if there is an overlay widget assigned
	 */
	bool HasFullWindowOverlayContent() const;

	/** @return should this window show up in the taskbar */
	bool AppearsInTaskbar() const;

	void SetOnWindowDeactivated( const FOnWindowDeactivated& InDelegate );

	/** Sets the delegate to execute right before the window is closed */
	void SetOnWindowClosed( const FOnWindowClosed& InDelegate );

	/** Sets the delegate to execute right after the window has been moved */
	void SetOnWindowMoved( const FOnWindowMoved& InDelegate);

	/** Sets the delegate to override RequestDestroyWindow */
	void SetRequestDestroyWindowOverride( const FRequestDestroyWindowOverride& InDelegate );

	/** Request that this window be destroyed. The window is not destroyed immediately. Instead it is placed in a queue for destruction on next Tick */
	void RequestDestroyWindow();

	/** Warning: use Request Destroy Window whenever possible!  This method destroys the window immediately! */
	void DestroyWindowImmediately();
 
	/** Calls the OnWindowClosed delegate when this window is about to be closed */
	void NotifyWindowBeingDestroyed();

	/** Make the window visible */
	void ShowWindow();

	/** Make the window invisible */
	void HideWindow();

	/**
	 * Enables or disables this window and all of its children
	 *
	 * @param bEnable	true to enable this window and its children false to diable this window and its children
	 */
	void EnableWindow( bool bEnable );

	/** Toggle window between window modes (fullscreen, windowed, etc) */
	void SetWindowMode( EWindowMode::Type WindowMode );

	/** @return The current window mode (fullscreen, windowed, etc) */
	EWindowMode::Type GetWindowMode() const { return NativeWindow->GetWindowMode(); }

	/** @return true if the window is visible, false otherwise*/
	bool IsVisible() const;

	/** @return true if the window is maximized, false otherwise*/
	bool IsWindowMaximized() const;

	/** Maximize the window if bInitiallyMaximized is set */
	void InitialMaximize();

	/**
	 * Sets the opacity of this window
	 *
	 * @param	InOpacity	The new window opacity represented as a floating point scalar
	 */
	void SetOpacity( const float InOpacity );

	/** @return the window's current opacity */
	float GetOpacity() const;

	/** @return true if the window supports transparency */
	bool SupportsTransparency() const;

	/** @return A String representation of the widget */
	virtual FString ToString() const;

	/**
	 * Sets a widget that should become focused when this window is next activated
	 *
	 * @param	InWidget	The widget to set focus to when this window is activated
	 */
	void SetWidgetToFocusOnActivate( TSharedPtr< SWidget > InWidget )
	{
		WidgetToFocusOnActivate = InWidget;
	}

	/** @return true if the window should be activated when first shown */
	bool ActivateWhenFirstShown() const;

	/** @return true if the window accepts input; false if the window is non-interactive */
	bool AcceptsInput() const;

	/** @return true if the user decides the size of the window */
	bool IsUserSized() const;

	/** @return true if the window is sized by the windows content */
	bool IsAutosized() const;

	/** Should this window automatically derive its size based on its content or be user-drive? */
	void SetSizingRule( ESizingRule::Type InSizingRule );

	/** @return true if this is a vanilla window, or one being used for some special purpose: e.g. tooltip or menu */
	bool IsRegularWindow() const;

	/** @return true if the window should be on top of all other windows; false otherwise */
	bool IsTopmostWindow() const;

	/** @return True if we expect the window size to change frequently. See description of bSizeWillChangeOften member variable. */
	bool SizeWillChangeOften() const
	{
		return bSizeWillChangeOften;
	}

	/** @return Returns the configured expected maximum width of the window, or INDEX_NONE if not specified.  Can be used to optimize performance for window size animation */
	int32 GetExpectedMaxWidth() const
	{
		return ExpectedMaxWidth;
	}

	/** @return Returns the configured expected maximum height of the window, or INDEX_NONE if not specified.  Can be used to optimize performance for window size animation */
	int32 GetExpectedMaxHeight() const
	{
		return ExpectedMaxHeight;
	}

	/** @return true if the window is using the os window border instead of a slate created one */
	bool HasOSWindowBorder() const { return bHasOSWindowBorder; }

	/** @return true if mouse coordinates is within this window */
	bool IsScreenspaceMouseWithin(FVector2D ScreenspaceMouseCoordinate) const;

	/** @return true if this is a user-sized window with a thick edge */
	bool HasSizingFrame() const;

	/** @return true if this window has a maximize button/box on the titlebar area */
	bool HasMaximizeBox() const;

	/** @return true if this window has a minimize button/box on the titlebar area */
	bool HasMinimizeBox() const;

	/** Set modal window related flags - called by Slate app code during FSlateApplication::AddModalWindow() */
	void SetAsModalWindow()
	{
		bHasMaximizeButton = false;
		bHasMinimizeButton = false;
	}

	// Events
	virtual FCursorReply OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const;

	/** The system will call this method to notify the window that it has been places in the foreground or background. */
	virtual bool OnIsActiveChanged( const FWindowActivateEvent& ActivateEvent );
	
	/** Windows functions */
	void Maximize();
	void Restore();
	void Minimize();

	/** Gets the current Window Zone that mouse position is over. */
	EWindowZone::Type GetCurrentWindowZone(FVector2D LocalMousePosition);

	/** @return Gets the radius of the corner rounding of the window. */
	int32 GetCornerRadius();

	virtual bool SupportsKeyboardFocus() const OVERRIDE;

	virtual FReply OnKeyboardFocusReceived( const FGeometry& MyGeometry, const FKeyboardFocusEvent& InKeyboardFocusEvent );

	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) OVERRIDE;
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) OVERRIDE;
	virtual FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) OVERRIDE;

	/** The window's desired size takes into account the ratio between the slate units and the pixel size */
	virtual FVector2D ComputeDesiredSize() const OVERRIDE;

	/** Marks this window as having been drawn at least once */
	inline void MarkAsDrawn()
	{
		bHasEverBeenDrawn = true;
	}

	/** @return Returns True if the window has even been drawn */
	inline bool HasEverBeenDrawn() const
	{
		return bHasEverBeenDrawn;
	}

	/** @return true if this window will be focused when it is first shown */
	inline bool IsFocusedInitially() const
	{
		return bFocusWhenFirstShown;
	}

	/** @return the list of this window's child windows */
	const TArray< TSharedRef<SWindow> >& GetChildWindows() const;
	
	/** @return the list of this window's child windows */
	TArray< TSharedRef<SWindow> >& GetChildWindows();

	/** Add ChildWindow as this window's child */
	void AddChildWindow( const TSharedRef<SWindow>& ChildWindow );

	/** @return the parent of this window; Invalid shared pointer if this window is not a child */
	TSharedPtr<SWindow> GetParentWindow() const;

	/** Look up the parent chain until we find the top-level window that owns this window */
	TSharedPtr<SWindow> GetTopmostAncestor();

	/** Remove DescendantToRemove from this window's children or their children. */
	bool RemoveDescendantWindow( const TSharedRef<SWindow>& DescendantToRemove );

	/** Sets the delegate to call when switching worlds in before ticking,drawing, or sending messages to widgets in this window */
	void SetOnWorldSwitchHack( FOnSwitchWorldHack& InOnWorldSwitchHack );

	/**
	 * Static: Marks the specified window as drawn
	 *
	 * @param	Window	The window to mark as drawn
	 */
	static void MarkWindowAsDrawn( SWindow* Window )
	{
		Window->MarkAsDrawn();
	}

	/**
	 * @return Returns a delegate that can be used to mark window as drawn. The delegate is safe to call from a different thread.
	 */
	FSimpleDelegate MakeMarkWindowAsDrawnDelegate()
	{
		return FSimpleDelegate::CreateStatic( &MarkWindowAsDrawn, this );
	}

	/**
	 * Hack to switch worlds
	 *
	 * @param WorldId: User ID for a world that should be restored or -1 if no restore
	 * @param The ID of the world restore later
	 */
	int32 SwitchWorlds( int32 WorldId ) const;

	/** Creates widgets for this window's title bar area.  This is an advanced method, only for fancy windows that want to
	    override the look of the title area by arranging those widgets itself */
	void MakeTitleBarContentWidgets( TSharedPtr< SWidget >& OutLeftContent, TSharedPtr< SWidget >& OutCenterContent, TSharedPtr< SWidget >& OutRightContent );

	/**
	 * Default constructor. Use SNew(SWindow) instead.
	 */
	SWindow();

protected:

	/** @return An appropriate resource for the window title background depending on whether the window is active */
	const FSlateBrush* GetWindowTitlebackgroundImage() const;

	/** Get the desired color of titlebar items. These change during flashing. */
	float GetFlashValue() const;
	FSlateColor GetWindowTitleAreaColor() const;
	FSlateColor GetWindowTitleContentColor() const;

	/** Kick off a morph to whatever the target shape happens to be. */
	void StartMorph();

	/** Get the brush used to draw the window background */
	const FSlateBrush* GetWindowBackground() const;

	/** Get the brush used to draw the window outline */
	const FSlateBrush* GetWindowOutline() const;

	/** Get the color used to tint the window outline */
	FSlateColor GetWindowOutlineColor() const;

protected:

	/** Title of the window, displayed in the title bar as well as potentially in the task bar (Windows platform) */
	TAttribute<FText> Title;

	/** When true, grabbing anywhere on the window will allow it to be dragged. */
	bool bDragAnywhere;

	/** Current opacity of the window */
	float Opacity;

	/** How to size the window */
	ESizingRule::Type SizingRule;

	/** How to auto center the window */
	EAutoCenter::Type AutoCenterRule;

	/** true if the window supports transparency */
	bool bIsTransparent : 1;

	/** True if this is a pop up window */
	bool bIsPopupWindow : 1;

	/** True if this is a tool tip window */
	bool bIsToolTipWindow : 1;

	/** True if this is a topmost window */
	bool bIsTopmostWindow : 1;

	/** True if we expect the size of this window to change often, such as if its animated, or if it recycled for tool-tips,
	    and we'd like to avoid costly GPU buffer resizes when that happens.  Enabling this may incur memory overhead or
		other platform-specific side effects */
	bool bSizeWillChangeOften : 1;

	/** Whether this window is used to draw content next to the cursor; usually for drag and drop purposes. */
	bool bIsCursorDecoratorWindow : 1;

	/** true if this window is maximized when its created */
	bool bInitiallyMaximized : 1;

	/** True if this window has been shown yet */
	bool bHasEverBeenShown : 1;

	/** True if this window has ever been drawn */
	bool bHasEverBeenDrawn : 1;

	/** Focus this window immediately as it is shown */
	bool bFocusWhenFirstShown : 1;

	/** Activate this window immediately as it is shown */
	bool bActivateWhenFirstShown : 1;

	/** True if this window displays the os window border instead of drawing one in slate */
	bool bHasOSWindowBorder : 1;
	
	/** True if this window displays an enabled minimize button on the toolbar area */
	bool bHasMinimizeButton : 1;

	/** True if this window displays an enabled maximize button on the toolbar area */
	bool bHasMaximizeButton : 1;

	/** True if this window displays thick edge that can be used to resize the window */
	bool bHasSizingFrame : 1;

	/** Initial desired position of the window's content in screen space */
	FVector2D InitialDesiredScreenPosition;

	/** Initial desired size of the window's content in screen space */
	FVector2D InitialDesiredSize;

	/** Position of the window's content in screen space */
	FVector2D ScreenPosition;

	/** The position of the window before entering fullscreen */
	FVector2D PreFullscreenPosition;

	/** Size of the window's content area in screen space */
	FVector2D Size;

	/** Size of this window's title bar.  Can be zero.  Set at construction and should not be changed afterwards. */
	float TitleBarSize;

	/** Utility for animating the window size. */
	struct FMorpher
	{
		FMorpher()
			: StartingMorphShape( FSlateRect(0,0,100,100) )
			, TargetMorphShape( FSlateRect(0,0,100,100) )
			, bIsActive(false)
			, bIsPendingPlay(false)
			, bIsAnimatingWindowSize(false)
		{
		}

		/** Initial window opacity */
		float StartingOpacity;
		/** Desired opacity of the window */
		float TargetOpacity;

		/** Initial size of the window (i.e. at the start of animation) */
		FSlateRect StartingMorphShape;
		/** Desired size of the window (i.e. at the end of the animation) */
		FSlateRect TargetMorphShape;
		
		/** Animation sequence to hold on to the Handle */
		FCurveSequence Sequence;

		/** True if this morph is currently active */
		bool bIsActive : 1;

		/** True if this morph sequence is pending play (i.e. will start on next Tick) */
		bool bIsPendingPlay : 1;

		/** True if we're morphing size as well as position.  False if we're just morphing position */
		bool bIsAnimatingWindowSize : 1;

	} Morpher;

	FCurveSequence TitleFlashSequence;

	/** Cached "zone" the cursor was over in the window the last time that someone called GetCurrentWindowZone() */
	EWindowZone::Type WindowZone;
	

	TSharedPtr<SWidget> TitleArea;
	TSharedPtr<SButton> MinimizeButton;
	TSharedPtr<SButton> MaximizeRestoreButton;
	TSharedPtr<SButton> CloseButton;
	
	SVerticalBox::FSlot* ContentSlot;

	/** Widget to transfer keyboard focus to when this window becomes active, if any.  This is used to
	    restore focus to a widget after a popup has been dismissed. */
	TWeakPtr< SWidget > WidgetToFocusOnActivate;

	/** Style used to draw this window */
	const FWindowStyle* Style;
	const FSlateBrush* WindowBackground;

private:
	/** The native window that is backing this Slate Window */
	TSharedPtr<FGenericWindow> NativeWindow;
	
	/** Invoked when the window has been deactivated. */
	FOnWindowDeactivated OnWindowDeactivated;
	
	/** Invoked when the window is about to be closed. */
	FOnWindowClosed OnWindowClosed;

	/** Invoked when the window is moved */
	FOnWindowMoved OnWindowMoved;

	/** Invoked when the window is requested to be destroyed. */
	FRequestDestroyWindowOverride RequestDestroyWindowOverride;

	/** Window overlay widget */
	TSharedPtr<SOverlay> WindowOverlay;	
	
	/**
	 * This layer provides mechanism for tooltips, drag-drop
	 * decorators, and popups without creating a new window.
	 */
	TSharedPtr<class SPopupLayer> PopupLayer;

	/** Full window overlay widget */
	TSharedPtr<SWidget> FullWindowOverlayWidget;



	/** When not null, this window will always appear on top of the parent and be closed when the parent is closed. */
	TWeakPtr<SWindow> ParentWindowPtr;

	/** Child windows of this window */
	TArray< TSharedRef<SWindow> > ChildWindows;
	
	/** World switch delegate */
	FOnSwitchWorldHack OnWorldSwitchHack;

	/** 
	 * Whether or not we should show content of the window which could be occluded by full screen window content. 
	 * This is used to hide content when there is a full screen overlay occluding it
	 */
	bool bShouldShowWindowContentDuringOverlay;

	/** The expected maximum width of the window.  May be used for performance optimization when bSizeWillChangeOften is set. */
	int32 ExpectedMaxWidth;

	/** The expected maximum height of the window.  May be used for performance optimization when bSizeWillChangeOften is set. */
	int32 ExpectedMaxHeight;

private:
	// Behavior handlers
	FReply MinimizeButton_OnClicked();
	
	FReply MaximizeRestoreButton_OnClicked();
	
	FReply CloseButton_OnClicked();
	
	void ConstructWindowInternals( const bool bCreateTitleBar );

	/** Are any of our child windows active? */
	bool HasActiveChildren() const;

	/** Creates a traditional title bar for this window */
	TSharedRef<SWidget> MakeTitleBarContent();

	// Bindings
	const FSlateBrush* GetMinimizeImage() const;
	const FSlateBrush* GetMaximizeRestoreImage() const;
	const FSlateBrush* GetCloseImage() const;
	
	/**
	 * @return EVisibility::Visible if we are showing this viewports content.  EVisibility::Hidden otherwise (we hide the content during full screen overlays)
	 */
	EVisibility GetWindowContentVisibility() const;

	/**
	 * @return EVisibility::Visible if the window is flashing. Used to show/hide the white flash in the title area
	 */
	EVisibility GetWindowFlashVisibility() const;
};

/**
 * Popups, tooltips, drag and drop decorators all can be executed without creating a new window.
 * This slot along with the SWindow::AddPopupLayerSlot() API enabled it.
 */
struct FPopupLayerSlot
{
public:
	FPopupLayerSlot()
	: DesktopPosition_Attribute(FVector2D::ZeroVector)
	, WidthOverride_Attribute()
	, HeightOverride_Attribute()
	, Scale_Attribute(1.0f)
	, Widget(SNullWidget::NullWidget)
	{}

	/** Support for using brackets in slate delcarative syntax */
	FPopupLayerSlot& operator[]( TSharedRef<SWidget> InWidget )
	{
		Widget = InWidget;
		return *this;
	}

	/** Pixel position in desktop space */
	FPopupLayerSlot& DesktopPosition( const TAttribute<FVector2D>& InDesktopPosition )
	{
		DesktopPosition_Attribute = InDesktopPosition;
		return *this;
	}

	/** Width override in pixels */
	FPopupLayerSlot& WidthOverride( const TAttribute<float>& InWidthOverride )
	{
		WidthOverride_Attribute = InWidthOverride;
		return *this;
	}

	/** Width override in pixels */
	FPopupLayerSlot& HeightOverride( const TAttribute<float>& InHeightOverride )
	{
		HeightOverride_Attribute = InHeightOverride;
		return *this;
	}

	/** DPI scaling to be applied to the contents of this slot */
	FPopupLayerSlot& Scale( const TAttribute<float>& InScale )
	{
		Scale_Attribute = InScale;
		return *this;
	}

	/** @return the widget present in this slot */
	const TSharedRef<SWidget>& GetWidget()
	{
		return Widget;
	}

private:
	/** SPopupLayer arranges FPopupLayerSlots, so it needs to know all about */
	friend class SPopupLayer;
	/** TPanelChildren need access to the Widget member */
	friend class TPanelChildren<FPopupLayerSlot>;

	TAttribute<FVector2D> DesktopPosition_Attribute;
	TAttribute<float> WidthOverride_Attribute;
	TAttribute<float> HeightOverride_Attribute;
	TAttribute<float> Scale_Attribute;
	TSharedRef<SWidget> Widget;
};

/**
 * Hack to switch wolds in a scope and switch back when we fall out of scope                   
 */
struct FScopedSwitchWorldHack
{
	FScopedSwitchWorldHack( FWidgetPath& WidgetPath )
		: Window( WidgetPath.TopLevelWindow )
		, WorldId( -1 )
	{
		if( Window.IsValid() )
		{
			WorldId = Window->SwitchWorlds( WorldId );
		}
	}

	FScopedSwitchWorldHack( TSharedPtr<SWindow> InWindow )
		: Window( InWindow )
		, WorldId( -1 )
	{
		if( Window.IsValid() )
		{
			WorldId = Window->SwitchWorlds( WorldId );
		}
	}

	~FScopedSwitchWorldHack()
	{
		if( Window.IsValid() )
		{
			Window->SwitchWorlds( WorldId );
		}
	}
private:
	/** The window to switch worlds for */
	TSharedPtr<SWindow> Window;
	/** The worldID serves as identification to the user about the world.  It can be anything although -1 is assumed to be always invalid */
	int32 WorldId;
};
