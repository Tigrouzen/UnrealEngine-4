// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	HTML5PlatformRunnableThread.h: HTML5 platform string classes, mostly implemented with ANSI C++
==============================================================================================*/

#pragma once

/**
 * @todo html5 threads: Dummy thread class
 */
class FHTML5RunnableThread : public FRunnableThread
{
public:

	virtual void SetThreadPriority (EThreadPriority NewPriority)
	{

	}

	virtual void SetThreadAffinityMask(uint64 AffinityMask) OVERRIDE
	{
	}

	virtual void Suspend (bool bShouldPause = 1)
	{

	}
	virtual bool Kill (bool bShouldWait = false)
	{
		return false;
	}

	virtual void WaitForCompletion ()
	{

	}

	virtual uint32 GetThreadID ()
	{
		return 0;
	}

	virtual FString GetThreadName ()
	{
		return TEXT("");
	}

public:


	/**
	 * Virtual destructor
	 */
	virtual ~FHTML5RunnableThread ()
	{

	}


protected:

	virtual bool CreateInternal (FRunnable* InRunnable, const TCHAR* ThreadName,
		bool bAutoDeleteSelf = 0,bool bAutoDeleteRunnable = 0,uint32 InStackSize = 0,
		EThreadPriority InThreadPri = TPri_Normal, uint64 InThreadAffinityMask = 0) 
	{
		return false;
	}

	virtual bool NotifyCreated() OVERRIDE
	{
		return false;
	}
};

