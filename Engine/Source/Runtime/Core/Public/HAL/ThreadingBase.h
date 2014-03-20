// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ThreadingBase.h: Declares base level interfaces for threading support.
=============================================================================*/

#pragma once

#ifndef EXPERIMENTAL_PARALLEL_CODE
	#error EXPERIMENTAL_PARALLEL_CODE not defined!
#endif

/**
 * The list of enumerated thread priorities we support
 */
enum EThreadPriority
{
	TPri_Normal,
	TPri_AboveNormal,
	TPri_BelowNormal
};


/**
 * Interface for waitable events.
 *
 * This interface has platform-specific implementations that are used to wait for another
 * thread to signal that it is ready for the waiting thread to do some work. It can also
 * be used for telling groups of threads to exit.
 */
class FEvent
{
public:

	/**
	 * Creates the event.
	 *
	 * Manually reset events stay triggered until reset.
	 * Named events share the same underlying event.
	 *
	 * @param bIsManualReset - Whether the event requires manual reseting or not.
	 *
	 * @return true if the event was created, false otherwise.
	 */
	virtual bool Create (bool bIsManualReset = false) = 0;

	/**
	 * Triggers the event so any waiting threads are activated
	 */
	virtual void Trigger() = 0;

	/**
	 * Resets the event to an untriggered (waitable) state
	 */
	virtual void Reset() = 0;

	/**
	 * Waits the specified amount of time for the event to be triggered.
	 *
	 * A wait time of MAX_uint32 is treated as infinite wait.
	 *
	 * @param WaitTime - The time to wait (in milliseconds).
	 *
	 * @return true if the event was triggered, false if the wait timed out.
	 */
	virtual bool Wait (uint32 WaitTime) = 0;


public:

	/**
	 * Waits an infinite amount of time for the event to be triggered.
	 *
	 * @return true if the event was triggered.
	 */
	bool Wait ()
	{
		return Wait(MAX_uint32);
	}

	/**
	 * Waits the specified amount of time for the event to be triggered.
	 *
	 * @param WaitTime - The time to wait.
	 *
	 * @return true if the event was triggered, false if the wait timed out.
	 */
	bool Wait (const FTimespan& WaitTime)
	{
		return Wait(WaitTime.GetTotalMilliseconds());
	}


public:

	/**
	 * Virtual destructor.
	 */
	virtual ~FEvent () { }
};


/**
 * This class is allows a simple one-shot scoped event.
 *
 * Usage:
 * {
 *		FScopedEvent MyEvent;
 *		SendReferenceOrPointerToSomeOtherThread(&MyEvent); // Other thread calls MyEvent->Trigger();
 *		// MyEvent destructor is here, we wait here.
 * }
 */
class CORE_API FScopedEvent
{
public:

	/**
	 * Default constructor.
	 */
	FScopedEvent()
		: Event(GetEventFromPool())
	{
	}

	/**
	 * Destructor.
	 */
	~FScopedEvent()
	{
		Event->Wait();
		ReturnToPool(Event);
		Event = NULL;
	}

	/**
	 * Triggers the event.
	 */
	void Trigger()
	{
		Event->Trigger();
	}

	/**
	 * Retrieve the event, usually for passing around
	 */
	FEvent* Get()
	{
		return Event;
	}

protected:

	/**
	 * Returns an event object from the pool.
	 *
	 * @return An event object.
	 */
	static FEvent* GetEventFromPool();

	/**
	 * Returns an event object to the pool.
	 *
	 * @param Event - The event object to return.
	 */
	static void ReturnToPool(FEvent* Event);


private:

	// Holds the event.
	FEvent* Event;
};


/**
 * Interface for "runnable" objects.
 *
 * A runnable object is an object that is "run" on an arbitrary thread. The call usage pattern is
 * Init(), Run(), Exit(). The thread that is going to "run" this object always uses those calling
 * semantics. It does this on the thread that is created so that any thread specific uses (TLS, etc.)
 * are available in the contexts of those calls. A "runnable" does all initialization in Init().
 *
 * If initialization fails, the thread stops execution and returns an error code. If it succeeds,
 * Run() is called where the real threaded work is done. Upon completion, Exit() is called to allow
 * correct clean up.
 */
class CORE_API FRunnable
{
public:
	/**
	 * Initializes the runnable object.
	 *
	 * This method is called in the context of the thread object that aggregates this, not the
	 * thread that passes this runnable to a new thread.
	 *
	 * @return True if initialization was successful, false otherwise
	 */
	virtual bool Init ()
	{
		return true;
	}

	/**
	 * Runs the runnable object.
	 *
	 * This is where all per object thread work is done. This is only called if the initialization was successful.
	 *
	 * @return The exit code of the runnable object
	 */
	virtual uint32 Run () = 0;

	/**
	 * Stops the runnable object.
	 *
	 * This is called if a thread is requested to terminate early.
	 */
	virtual void Stop () { }

	/**
	 * Exits the runnable object.
	 *
	 * Called in the context of the aggregating thread to perform any cleanup.
	 */
	virtual void Exit () { }

	/**
	 * Gets single thread interface pointer used for ticking this runnable when multithreading is disabled.
	 * If the interface is not implemented, this runnable will not be ticked when FPlatformProcess::SupportsMultithreading() is false.
	 *
	 * @return Pointer to the single thread interface or NULL if not implemented.
	 */
	virtual class FSingleThreadRunnable* GetSingleThreadInterface() { return NULL; }

public:

	/**
	 * Virtual destructor
	 */
	virtual ~FRunnable() { }
};

/**
 * Interface for runnable threads.
 *
 * This interface specifies the methods used to manage a thread's life cycle.
 */
class FRunnableThread
{
public:

	/**
	 * Factory method to create a thread with the specified stack size and thread priority.
	 *
	 * @param InRunnable The runnable object to execute
	 * @param ThreadName Name of the thread
	 * @param bAutoDeleteSelf Whether to delete this object on exit
	 * @param bAutoDeleteRunnable Whether to delete the runnable object on exit
	 * @param InStackSize The size of the stack to create. 0 means use the
	 * current thread's stack size
	 * @param InThreadPri Tells the thread whether it needs to adjust its
	 * priority or not. Defaults to normal priority
	 *
	 * @return The newly created thread or NULL if it failed
	 */
	CORE_API static FRunnableThread* Create (
		class FRunnable* InRunnable, 
		const TCHAR* ThreadName, 
		bool bAutoDeleteSelf = false,
		bool bAutoDeleteRunnable = false, 
		uint32 InStackSize = 0,
		EThreadPriority InThreadPri = TPri_Normal,
		uint64 InThreadAffinityMask = 0);

	/**
	 * Changes the thread priority of the currently running thread
	 *
	 * @param NewPriority The thread priority to change to
	 */
	virtual void SetThreadPriority (EThreadPriority NewPriority) = 0;

	/**	 
	 * Change the thread processor affinity
	 *
	 * @param AffinityMask A bitfield indicating what processors the thread is allowed to run on
	 */
	virtual void SetThreadAffinityMask(uint64 AffinityMask) = 0;

	/**
	 * Tells the thread to either pause execution or resume depending on the
	 * passed in value.
	 *
	 * @param bShouldPause Whether to pause the thread (true) or resume (false)
	 */
	virtual void Suspend (bool bShouldPause = true) = 0;

	/**
	 * Tells the thread to exit. If the caller needs to know when the thread
	 * has exited, it should use the bShouldWait value and tell it how long
	 * to wait before deciding that it is deadlocked and needs to be destroyed.
	 * The implementation is responsible for calling Stop() on the runnable.
	 * NOTE: having a thread forcibly destroyed can cause leaks in TLS, etc.
	 *
	 * @param bShouldWait If true, the call will wait for the thread to exit
	 *
	 * @return True if the thread exited graceful, false otherwise
	 */
	virtual bool Kill (bool bShouldWait = false) = 0;

	/**
	 * Halts the caller until this thread is has completed its work.
	 */
	virtual void WaitForCompletion () = 0;

	/**
	 * Thread ID for this thread 
	 *
	 * @return ID that was set by CreateThread
	 */
	virtual uint32 GetThreadID () = 0;

	/**
	 * Retrieves the given name of the thread
	 *
	 * @return Name that was set by CreateThread
	 */
	virtual FString GetThreadName () = 0;

	struct FThreadRegistry
	{
		void Add(uint32 ID, FRunnableThread* Thread)
		{
			Lock();
			Registry.FindOrAdd(ID) = Thread;
			Updated = true;
			Unlock();
		}

		void Remove(uint32 ID)
		{
			Lock();
			Registry.Remove(ID);
			Updated = true;
			Unlock();
		}

		int32 GetThreadCount()
		{
			Lock();
			int32 RetVal = Registry.Num();
			Unlock();
			return RetVal;
		}

		bool IsUpdated()
		{
			return Updated;
		}

		void Lock()
		{
			CriticalSection.Lock();
		}

		void Unlock()
		{
			CriticalSection.Unlock();
		}

		void ClearUpdated()
		{
			Updated = false;
		}

		TMap<uint32, FRunnableThread*>::TConstIterator CreateConstIterator()
		{
			return Registry.CreateConstIterator();
		}

		FRunnableThread* GetThread(uint32 ID)
		{
			return Registry.FindRef(ID);
		}

	private:
		TMap<uint32, FRunnableThread*> Registry;
		bool Updated;
		FCriticalSection CriticalSection;
	};

	static CORE_API FThreadRegistry& GetThreadRegistry()
	{
		static FThreadRegistry ThreadRegistry;
		return ThreadRegistry;
	}

	/** @return a delegate that is called when this runnable has been destroyed. */
	FSimpleMulticastDelegate& OnThreadDestroyed()
	{
		return ThreadDestroyedDelegate;
	}

	/**
	 * Virtual destructor
	 */
	virtual ~FRunnableThread ();


protected:

	/**
	 * Creates the thread with the specified stack size and thread priority.
	 *
	 * @param InRunnable The runnable object to execute
	 * @param ThreadName Name of the thread
	 * @param bAutoDeleteSelf Whether to delete this object on exit
	 * @param bAutoDeleteRunnable Whether to delete the runnable object on exit
	 * @param InStackSize The size of the stack to create. 0 means use the
	 * current thread's stack size
	 * @param InThreadPri Tells the thread whether it needs to adjust its
	 * priority or not. Defaults to normal priority
	 *
	 * @return True if the thread and all of its initialization was successful, false otherwise
	 */
	virtual bool CreateInternal (FRunnable* InRunnable, const TCHAR* ThreadName,
		bool bAutoDeleteSelf = 0,bool bAutoDeleteRunnable = 0,uint32 InStackSize = 0,
		EThreadPriority InThreadPri = TPri_Normal, uint64 InThreadAffinityMask = 0) = 0;
	/**
	 * Lets this thread know it has been created it case it has already finished its execution
	 * and wants to delete itself.
	 *
	 * @return true if the thread has already finished execution.
	 */
	virtual bool NotifyCreated() = 0;

	/** Called when the this runnable has been destroyed, so we should clean-up memory allocated by misc classes. */
	FSimpleMulticastDelegate ThreadDestroyedDelegate;
};


/**
 * Fake event object used when running with only one thread.
 */
class FSingleThreadEvent : public FEvent
{
	/** Flag to know whether this event has been triggered. */
	bool bTriggered;
	/** Should this event reset automatically or not. */
	bool bManualReset;

public:
	FSingleThreadEvent()
		: bTriggered(false)
		, bManualReset(false)
	{}

	// FEvent Interface
	virtual bool Create(bool bIsManualReset = false) 
	{ 
		bManualReset = bIsManualReset;
		return true; 
	}
	virtual void Trigger() { bTriggered = true; }
	virtual void Reset() { bTriggered = false; }
	virtual bool Wait(uint32 WaitTime) 
	{ 
		// With only one thread it's assumed the event has been triggered
		// before Wait is called, otherwise it would end up waiting forever or always fail.
		check(bTriggered);
		bTriggered = bManualReset;
		return true; 
	}
};


/** 
 * Interface for ticking runnables when there's only one thread available and 
 * multithreading is disabled.
 */
class CORE_API FSingleThreadRunnable
{
public:
	virtual ~FSingleThreadRunnable() {}

	/* Tick function. */
	virtual void Tick() = 0;
};


/**
 * Manages runnables and runnable threads when multithreading is disabled.
 */
class CORE_API FSingleThreadManager
{
	/** List of thread objects to be ticked. */
	TArray<class FFakeThread*> ThreadList;

public:

	/**
	 * Used internally to add a new thread object when multithreading is disabled.
	 *
	 * @param Thread Fake thread object.
	 */
	void AddThread(class FFakeThread* Thread);

	/**
	 * Used internally to remove fake thread object.
	 *
	 * @param Thread Fake thread object to be removed.
	 */
	void RemoveThread(class FFakeThread* Thread);

	/**
	 * Ticks all fake threads and their runnable objects.
	 */
	void Tick();

	/**
	 * Access to the singleton object.
	 */
	static FSingleThreadManager& Get();
};


/**
 * Interface for queued work objects.
 *
 * This interface is a type of runnable object that requires no per thread
 * initialization. It is meant to be used with pools of threads in an
 * abstract way that prevents the pool from needing to know any details
 * about the object being run. This allows queuing of disparate tasks and
 * servicing those tasks with a generic thread pool.
 */
class FQueuedWork
{
public:

	/**
	 * This is where the real thread work is done. All work that is done for
	 * this queued object should be done from within the call to this function.
	 */
	virtual void DoThreadedWork () = 0;

	/**
	 * Tells the queued work that it is being abandoned so that it can do
	 * per object clean up as needed. This will only be called if it is being
	 * abandoned before completion. NOTE: This requires the object to delete
	 * itself using whatever heap it was allocated in.
	 */
	virtual void Abandon () = 0;


public:

	/**
	 * Virtual destructor so that child implementations are guaranteed a chance
	 * to clean up any resources they allocated.
	 */
	virtual ~FQueuedWork () {}
};


/**
 * Interface for queued thread pools.
 *
 * This interface is used by all queued thread pools. It used as a callback by
 * FQueuedThreads and is used to queue asynchronous work for callers.
 */
class CORE_API FQueuedThreadPool
{
public:

	/**
	 * Creates the thread pool with the specified number of threads
	 *
	 * @param InNumQueuedThreads Specifies the number of threads to use in the pool
	 * @param StackSize The size of stack the threads in the pool need (32K default)
	 * @param ThreadPriority priority of new pool thread
	 *
	 * @return Whether the pool creation was successful or not
	 */
	virtual bool Create (uint32 InNumQueuedThreads,uint32 StackSize = (32 * 1024),EThreadPriority ThreadPriority=TPri_Normal) = 0;

	/**
	 * Tells the pool to clean up all background threads
	 */
	virtual void Destroy () = 0;

	/**
	 * Checks to see if there is a thread available to perform the task. If not,
	 * it queues the work for later. Otherwise it is immediately dispatched.
	 *
	 * @param InQueuedWork The work that needs to be done asynchronously
	 */
	virtual void AddQueuedWork (FQueuedWork* InQueuedWork) = 0;

	/**
	 * Attempts to retract a previously queued task.
	 *
	 * @param InQueuedWork The work to try to retract
	 * @return true if the work was retracted
	 */
	virtual bool RetractQueuedWork (FQueuedWork* InQueuedWork) = 0;

	/**
	 * Places a thread back into the available pool
	 *
	 * @param InQueuedThread The thread that is ready to be pooled
	 * @return next job or null if there is no job available now
	 */
	virtual FQueuedWork* ReturnToPoolOrGetNextJob (class FQueuedThread* InQueuedThread) = 0;


public:

	/**
	 * Virtual destructor.
	 */
	virtual ~FQueuedThreadPool () 
	{
	}


public:

	/**
	 * Allocates a thread pool
	 */
	static FQueuedThreadPool* Allocate ();
};


/*
 *  Global thread pool for shared async operations
 */
extern CORE_API FQueuedThreadPool* GThreadPool;


/** Thread safe counter */
class FThreadSafeCounter
{
public:

	/**
	 * Default constructor.
	 *
	 * Initializes the counter to 0.
	 */
	FThreadSafeCounter()
	{
		Counter = 0;
	}

	/**
	 * Copy Constructor.
	 *
	 * If the counter in the Other parameter is changing from other threads, there are no
	 * guarantees as to which values you will get up to the caller to not care, synchronize
	 * or other way to make those guarantees.
	 *
	 * @param Other - The other thread safe counter to copy
	 */
	FThreadSafeCounter (const FThreadSafeCounter& Other)
	{
		Counter = Other.GetValue();
	}

	/**
	 * Constructor, initializing counter to passed in value.
	 *
	 * @param Value	Value to initialize counter to
	 */
	FThreadSafeCounter (int32 Value)
	{
		Counter = Value;
	}

	/**
	 * Increment and return new value.	
	 *
	 * @return the new, incremented value
	 */
	int32 Increment ()
	{
		return FPlatformAtomics::InterlockedIncrement(&Counter);
	}

	/**
	 * Adds an amount and returns the old value.	
	 *
	 * @param Amount Amount to increase the counter by
	 * @return the old value
	 */
	int32 Add (int32 Amount)
	{
		return FPlatformAtomics::InterlockedAdd(&Counter, Amount);
	}

	/**
	 * Decrement and return new value.
	 *
	 * @return the new, decremented value
	 */
	int32 Decrement ()
	{
		return FPlatformAtomics::InterlockedDecrement(&Counter);
	}

	/**
	 * Subtracts an amount and returns the old value.	
	 *
	 * @param Amount Amount to decrease the counter by
	 * @return the old value
	 */
	int32 Subtract (int32 Amount)
	{
		return FPlatformAtomics::InterlockedAdd(&Counter, -Amount);
	}

	/**
	 * Sets the counter to a specific value and returns the old value.
	 *
	 * @param Value	Value to set the counter to
	 * @return The old value
	 */
	int32 Set (int32 Value)
	{
		return FPlatformAtomics::InterlockedExchange(&Counter, Value);
	}

	/**
	 * Resets the counter's value to zero.
	 *
	 * @return the old value.
	 */
	int32 Reset ()
	{
		return FPlatformAtomics::InterlockedExchange(&Counter, 0);
	}

	/**
	 * Gets the current value.
	 *
	 * @return the current value
	 */
	int32 GetValue () const
	{
		return Counter;
	}


private:
	// Hidden on purpose as usage wouldn't be thread safe.
	void operator=(const FThreadSafeCounter& Other){}

	/** Thread-safe counter */
	volatile int32 Counter;
};


/**
 * Implements a scope lock.
 *
 * This is a utility class that handles scope level locking. It's very useful
 * to keep from causing deadlocks due to exceptions being caught and knowing
 * about the number of locks a given thread has on a resource. Example:
 *
 * <code>
 *	{
 *		// Synchronize thread access to the following data
 *		FScopeLock ScopeLock(SynchObject);
 *		// Access data that is shared among multiple threads
 *		...
 *		// When ScopeLock goes out of scope, other threads can access data
 *	}
 * </code>
 */
class FScopeLock
{
public:

	/**
	 * Constructor that performs a lock on the synchronization object
	 *
	 * @param InSynchObject The synchronization object to manage
	 */
	FScopeLock (FCriticalSection* InSynchObject)
		: SynchObject(InSynchObject)
	{
		check(SynchObject);

		SynchObject->Lock();
	}

	/**
	 * Destructor that performs a release on the synchronization object
	 */
	~FScopeLock ()
	{
		check(SynchObject);

		SynchObject->Unlock();
	}


private:

	// Default constructor (hidden on purpose).
	FScopeLock ();

	// Copy constructor( hidden on purpose).
	FScopeLock (FScopeLock* InScopeLock);

	// Assignment operator (hidden on purpose).
	FScopeLock& operator= (FScopeLock& InScopeLock)
	{
		return *this;
	}


private:

	// Holds the synchronization object to aggregate and scope manage.
	FCriticalSection* SynchObject;
};


/** @return True if called from the game thread. */
extern CORE_API bool IsInGameThread ();

/** @return True if called from the slate thread, and not merely a thread calling slate functions. */
extern CORE_API bool IsInSlateThread ();


/** the stats system increments this and when pool threads notice it has been 
*   incremented, they call GStatManager.AdvanceFrameForThread() to advance any
*   stats that have been collected on a pool thread.
*/
STAT(extern FThreadSafeCounter GStatsFrameForPoolThreads);

/**
* This a special version of singleton. It means that there is created only one instance for each thread.
* Calling Get() method is thread-safe, but first call should be done on the game thread. 
* @see TForceInitAtBoot usage.
*/
template< class T >
class CORE_API FThreadSingleton
{
	/** TLS slot that holds a FThreadSingleton. */
	static uint32 TlsSlot;

protected:
	FThreadSingleton()
		: ThreadId(FPlatformTLS::GetCurrentThreadId())
	{}

	/** Thread ID this singleton was created on. */
	const uint32 ThreadId;

public:
	/**
	 * @return an instance of a singleton for the current thread.
	 */
	static T& Get()
	{
		if( !TlsSlot )
		{
			check(IsInGameThread());
			TlsSlot = FPlatformTLS::AllocTlsSlot();
		}
		T* ThreadSingleton = (T*)FPlatformTLS::GetTlsValue(TlsSlot);
		if( !ThreadSingleton )
		{
			const uint32 ThreadId = FPlatformTLS::GetCurrentThreadId();
			ThreadSingleton = new T();
			FRunnableThread::GetThreadRegistry().Lock();
			FRunnableThread* RunnableThread = FRunnableThread::GetThreadRegistry().GetThread(ThreadId);
			if( RunnableThread )
			{
				RunnableThread->OnThreadDestroyed().AddRaw( ThreadSingleton, &FThreadSingleton<T>::Shutdown );
			}
			FRunnableThread::GetThreadRegistry().Unlock();
			FPlatformTLS::SetTlsValue(TlsSlot, ThreadSingleton);
		}
		return *ThreadSingleton;
	}

	void Shutdown()
	{
		T* This = (T*)this;
		delete This;
	}
};
