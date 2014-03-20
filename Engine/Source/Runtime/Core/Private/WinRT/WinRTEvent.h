// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	WinRTEvent.h: Declares the FEventWin class.
=============================================================================*/

#pragma once

#include "AllowWinRTPlatformTypes.h"

/**
 * Implements the WinRT version of the FEvent interface.
 */
class FEventWinRT : public FEvent
{
public:

	/**
	 * Default constructor.
	 */
	FEventWinRT()
		: Event(NULL)
	{
	}

	/**
	 * Destructor.
	 */
	virtual ~FEventWinRT()
	{
		if (Event != NULL)
		{
			CloseHandle(Event);
		}
	}


public:

	virtual bool Create (bool bIsManualReset = false) OVERRIDE
	{
		// Create the event and default it to non-signaled
//		Event = CreateEvent(NULL, bIsManualReset, 0, NULL);
		Event = CreateEventEx(NULL, NULL, bIsManualReset ? CREATE_EVENT_MANUAL_RESET : 0, EVENT_ALL_ACCESS);

		return Event != NULL;
	}

	virtual void Trigger () OVERRIDE
	{
		check(Event);

		SetEvent(Event);
	}

	virtual void Reset () OVERRIDE
	{
		check(Event);

		ResetEvent(Event);
	}

	virtual bool Wait (uint32 WaitTime) OVERRIDE;


private:

	// Holds the handle to the event
	HANDLE Event;
};

#include "HideWinRTPlatformTypes.h"
