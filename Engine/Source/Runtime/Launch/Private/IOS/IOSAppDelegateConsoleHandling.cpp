// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "LaunchPrivatePCH.h"
#include "IOSAppDelegateConsoleHandling.h"
#include "LaunchPrivatePCH.h"

@implementation IOSAppDelegate (ConsoleHandling)

#if !UE_BUILD_SHIPPING
/** 
 * Shows the console and brings up an on-screen keyboard for input
 */
- (void)ShowConsole
{
	// start at the end of the list for history
	self.ConsoleHistoryValuesIndex = [self.ConsoleHistoryValues count];

	// Set up a containing alert message and buttons
	self.ConsoleAlert = [[UIAlertView alloc] initWithTitle:@"Type a console command"
													message:@""
													delegate:self
													cancelButtonTitle:NSLocalizedString(@"Cancel", nil)
													otherButtonTitles:NSLocalizedString(@"OK", nil), nil];

	self.ConsoleAlert.alertViewStyle = UIAlertViewStylePlainTextInput;

	// The property is now the owner
	[self.ConsoleAlert release];

	UITextField* TextField = [self.ConsoleAlert textFieldAtIndex:0];
	TextField.clearsOnBeginEditing = NO;
	TextField.autocorrectionType = UITextAutocorrectionTypeNo;
	TextField.autocapitalizationType = UITextAutocapitalizationTypeNone;
	TextField.placeholder = @"or swipe for history";
	TextField.clearButtonMode = UITextFieldViewModeWhileEditing;
	TextField.delegate = self;

	// Add gesture recognizers
	UISwipeGestureRecognizer* SwipeLeftGesture = [[UISwipeGestureRecognizer alloc] initWithTarget:self action:@selector(SwipeLeftAction:)];
	SwipeLeftGesture.direction = UISwipeGestureRecognizerDirectionLeft;
	[TextField addGestureRecognizer:SwipeLeftGesture];

	UISwipeGestureRecognizer* SwipeRightGesture = [[UISwipeGestureRecognizer alloc] initWithTarget:self action:@selector(SwipeRightAction:)];
	SwipeRightGesture.direction = UISwipeGestureRecognizerDirectionRight;
	[TextField addGestureRecognizer:SwipeRightGesture];

	[self.ConsoleAlert show];
}

/**
 * Handles processing of an input console command
 */
- (void)HandleConsoleCommand:(NSString*)ConsoleCommand
{
	if ([ConsoleCommand length] > 0)
	{
		if (self.bEngineInit)
		{
			TArray<TCHAR> Ch;
			Ch.AddZeroed([ConsoleCommand length]);

			FPlatformString::CFStringToTCHAR((CFStringRef)ConsoleCommand, Ch.GetTypedData());
			new(GEngine->DeferredCommands) FString(Ch.GetTypedData());
		}

		uint32 ExistingCommand = [self.ConsoleHistoryValues indexOfObjectPassingTest:
			^ BOOL (id obj, NSUInteger idx, BOOL *stop)
			{ 
				return [obj caseInsensitiveCompare:ConsoleCommand] == NSOrderedSame; 
			}
		];

		// add the command to the command history if it's unique
		if (ExistingCommand == NSNotFound)
		{
			[self.ConsoleHistoryValues addObject:ConsoleCommand];
		}
	}
}

/** 
 * Shows an alert with up to 3 buttons. A delegate callback will later set AlertResponse property
 */
- (void)ShowAlert:(NSMutableArray*)StringArray
{
	// set up the alert message and buttons
	UIAlertView* Alert = [[[UIAlertView alloc] initWithTitle:[StringArray objectAtIndex:0]
													 message:[StringArray objectAtIndex:1]
													delegate:self // use ourself to handle the button clicks 
										   cancelButtonTitle:[StringArray objectAtIndex:2]
										   otherButtonTitles:nil] autorelease];

	Alert.alertViewStyle = UIAlertViewStyleDefault;

	// add any extra buttons
	for (int OptionalButtonIndex = 3; OptionalButtonIndex < [StringArray count]; OptionalButtonIndex++)
	{
		[Alert addButtonWithTitle:[StringArray objectAtIndex:OptionalButtonIndex]];
	}

	// show it!
	[Alert show];
}

- (BOOL)textFieldShouldReturn:(UITextField *)alertTextField 
{
	[alertTextField resignFirstResponder];// to dismiss the keyboard.
	[self.ConsoleAlert dismissWithClickedButtonIndex:1 animated:YES];//this is called on alertview to dismiss it.

	return YES;
}
/**
 * An alert button was pressed
 */
- (void)alertView:(UIAlertView*)AlertView didDismissWithButtonIndex:(NSInteger)ButtonIndex
{
	// just set our AlertResponse property, all we need to do
	self.AlertResponse = ButtonIndex;

	if (AlertView.alertViewStyle == UIAlertViewStylePlainTextInput)
	{
		UITextField* TextField = [AlertView textFieldAtIndex:0];
		// if we clicked Ok (not Cancel at index 0), submit the console command
		if (ButtonIndex > 0)
		{
			[self HandleConsoleCommand:TextField.text];
		}
	}
}

- (void)SwipeLeftAction:(id)Ignored
{
	// Populate the text field with the previous entry in the history array
	if( self.ConsoleHistoryValues.count > 0 &&
		self.ConsoleHistoryValuesIndex + 1 < self.ConsoleHistoryValues.count )
	{
		self.ConsoleHistoryValuesIndex++;
		UITextField* TextField = [self.ConsoleAlert textFieldAtIndex:0];
		TextField.text = [self.ConsoleHistoryValues objectAtIndex:self.ConsoleHistoryValuesIndex];
	}
}

- (void)SwipeRightAction:(id)Ignored
{
	// Populate the text field with the next entry in the history array
	if( self.ConsoleHistoryValues.count > 0 &&
		self.ConsoleHistoryValuesIndex > 0 )
	{
		self.ConsoleHistoryValuesIndex--;
		UITextField* TextField = [self.ConsoleAlert textFieldAtIndex:0];
		TextField.text = [self.ConsoleHistoryValues objectAtIndex:self.ConsoleHistoryValuesIndex];
	}
}

#endif // !UE_BUILD_SHIPPING


@end
