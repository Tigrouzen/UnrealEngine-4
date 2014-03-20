// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "CorePrivate.h"
#include "IOSWindow.h"
#include "IOSAppDelegate.h"
#include "EAGLView.h"

FIOSWindow::~FIOSWindow()
{
	// NOTE: The Window is invalid here!
	//       Use NativeWindow_Destroy() instead.
}

TSharedRef<FIOSWindow> FIOSWindow::Make()
{
	return MakeShareable( new FIOSWindow() );
}

FIOSWindow::FIOSWindow()
{
}

void FIOSWindow::Initialize( class FIOSApplication* const Application, const TSharedRef< FGenericWindowDefinition >& InDefinition, const TSharedPtr< FIOSWindow >& InParent, const bool bShowImmediately )
{
	OwningApplication = Application;
	Definition = InDefinition;

	Window = [[UIApplication sharedApplication] keyWindow];

	if(InParent.Get() != NULL)
	{
		dispatch_async(dispatch_get_main_queue(),^ {
			UIAlertView* AlertView = [[UIAlertView alloc] initWithTitle:@""
															message:@"Error: Only one UIWindow may be created on iOS."
															delegate:nil
															cancelButtonTitle:NSLocalizedString(@"Ok", nil)
															otherButtonTitles: nil];

			[AlertView show];
			[AlertView release];
		} );
	}
}

FPlatformRect FIOSWindow::GetScreenRect()
{
	// get the main view's frame
	IOSAppDelegate* AppDelegate = (IOSAppDelegate*)[[UIApplication sharedApplication] delegate];
	CGRect Frame = [AppDelegate.GLView frame];
	CGFloat Scale = AppDelegate.GLView.contentScaleFactor;

	FPlatformRect ScreenRect;
	ScreenRect.Top = Frame.origin.y * Scale;
	ScreenRect.Bottom = (Frame.origin.y + Frame.size.height) * Scale;
	ScreenRect.Left = Frame.origin.x * Scale;
	ScreenRect.Right = (Frame.origin.x + Frame.size.width) * Scale;

	return ScreenRect;
}


bool FIOSWindow::GetFullScreenInfo( int32& X, int32& Y, int32& Width, int32& Height ) const
{
	FPlatformRect ScreenRect = GetScreenRect();

	X = ScreenRect.Left;
	Y = ScreenRect.Top;
	Width = ScreenRect.Right - ScreenRect.Left;
	Height = ScreenRect.Bottom - ScreenRect.Top;

	return true;
}
