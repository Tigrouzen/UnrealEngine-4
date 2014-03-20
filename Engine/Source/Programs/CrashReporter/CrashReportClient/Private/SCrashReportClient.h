// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CrashReportClient.h"

#if !CRASH_REPORT_UNATTENDED_ONLY

/**
 * UI for the crash report client app
 */
class SCrashReportClient : public SCompoundWidget
{
public:
	/**
	 * Slate arguments
	 */
	SLATE_BEGIN_ARGS(SCrashReportClient)
	{
	}

	SLATE_END_ARGS()

	/**
	 * Construct this Slate ui
	 * @param InArgs Slate arguments, not used
	 * @param Client Crash report client implementation object
	 */
	void Construct(const FArguments& InArgs, TSharedRef<FCrashReportClient> Client);

	/**
	 * Give the edit box focus (seems this has to be done after the window has been created)
	 */
	void SetDefaultFocus();

private:
	/**
	 * Keyboard short-cut handler
	 * @param InKeyboardEvent Which key was released, and which auxiliary keys were pressed
	 * @return Whether the event was handled
	 */
	FReply OnUnhandledKeyDown(const FKeyboardEvent& InKeyboardEvent);

	/** Crash report client implementation object */
	TSharedPtr<FCrashReportClient> CrashReportClient;

	/** The edit box that has keyboard focus to receive the user's comment */
	TSharedPtr<SEditableTextBox> UserCommentBox;
};

#endif // !CRASH_REPORT_UNATTENDED_ONLY
