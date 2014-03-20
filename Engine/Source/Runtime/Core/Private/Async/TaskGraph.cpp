// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "CorePrivate.h"
#include "TaskGraphInterfaces.h"

DEFINE_LOG_CATEGORY_STATIC(LogTaskGraph, Log, All);

DEFINE_STAT(STAT_FReturnGraphTask);
DEFINE_STAT(STAT_FNullGraphTask);
DEFINE_STAT(STAT_FTriggerEventGraphTask);
DEFINE_STAT(STAT_FSimpleDelegateGraphTask);
DEFINE_STAT(STAT_FDelegateGraphTask);

namespace ENamedThreads
{
	CORE_API Type RenderThread = ENamedThreads::GameThread; // defaults to game and is set and reset by the render thread itself
	CORE_API Type RenderThread_Local = ENamedThreads::GameThread_Local; // defaults to game local and is set and reset by the render thread itself
}


/** 
 *	Pointer to the task graph implementation singleton.
 *	Because of the multithreaded nature of this system an ordinary singleton cannot be used.
 *	FTaskGraphImplementation::Startup() creates the singleton and the constructor actually sets this value.
**/
static class FTaskGraphImplementation* TaskGraphImplementationSingleton = NULL;

/** 
 *	FTaskQueue
 *	High performance, SINGLE threaded, FIFO task queue for the private queue on named threads.
**/
class FTaskQueue
{
public:
	/** Constructor, sets the queue to the empty state. **/
	FTaskQueue()
		: StartIndex(0)
		, EndIndex(0)
	{
	}
	/** Returns the number of non-NULL items in the queue. **/
	int32 Num() const
	{
		CheckInvariants();
		return EndIndex - StartIndex;
	}
	/** 
	 *	Adds a task to the queue.
	 *	@param Task; the task to add to the queue
	**/
	void Enqueue(FBaseGraphTask* Task)
	{
		CheckInvariants();
		if (EndIndex >= Tasks.Num())
		{
			if (StartIndex >= ARRAY_EXPAND)
			{
				checkThreadGraph(Tasks[StartIndex - 1]==NULL);
				checkThreadGraph(Tasks[0]==NULL);
				FMemory::Memmove(Tasks.GetTypedData(),&Tasks[StartIndex],(EndIndex - StartIndex) * sizeof(FBaseGraphTask*));
				EndIndex -= StartIndex;
				FMemory::Memzero(&Tasks[EndIndex],StartIndex * sizeof(FBaseGraphTask*)); // not needed in final
				StartIndex = 0;
			}
			else
			{
				Tasks.AddZeroed(ARRAY_EXPAND);
			}
		}
		checkThreadGraph(EndIndex < Tasks.Num() && Tasks[EndIndex]==NULL);
		Tasks[EndIndex++] = Task;
	}
	/** 
	 *	Pops a task off the queue.
	 *	@return The oldest task in the queue or NULL if the queue is empty
	**/
	FBaseGraphTask* Dequeue()
	{
		if (!Num())
		{
			return NULL;
		}
		FBaseGraphTask* ReturnValue = Tasks[StartIndex];
		Tasks[StartIndex++] = NULL;
		if (StartIndex == EndIndex)
		{
			// Queue is empty, reset to start
			StartIndex = 0;
			EndIndex = 0;
		}
		return ReturnValue;
	}
private:
	enum
	{
		/** Number of tasks by which to expand and compact the queue on **/
		ARRAY_EXPAND=1024
	};

	/** Internal function to verify the state of the object is legal. **/
	void CheckInvariants() const
	{
		checkThreadGraph(StartIndex <= EndIndex);
		checkThreadGraph(EndIndex <= Tasks.Num());
		checkThreadGraph(StartIndex >= 0 && EndIndex >= 0);
	}

	/** Array to hold the tasks, only the [StartIndex,EndIndex) range contains non-NULL tasks. **/
	TArray<FBaseGraphTask*> Tasks;

	/** Index of first non-NULL task in the queue unless StartIndex == EndIndex (which means empty) **/
	int32 StartIndex;

	/** Index of first NULL task in the queue after the non-NULL tasks, unless StartIndex == EndIndex (which means empty) **/
	int32 EndIndex;

};


/** 
 *	FTaskThread
 *	A class for managing a worker or named thread. 
 *	This class implements the FRunnable API, but external threads don't use that because those threads are created elsewhere.
**/
class FTaskThread : public FRunnable, FSingleThreadRunnable
{
public:

	TArray<FBaseGraphTask*> NewTasks;

	// Calls meant to be called from a "main" or supervisor thread.

	/** Constructor, initializes everything to unusable values. Meant to be called from a "main" thread. **/
	FTaskThread()
		: ThreadId(ENamedThreads::AnyThread)
		, PerThreadIDTLSSlot(0xffffffff)
		, bAllowsStealsFromMe(false)
		, bStealsFromOthers(false)
	{
		NewTasks.Reset(128);
	}

	/** 
	 *	Sets up some basic information for a thread. Meant to be called from a "main" thread. Also creates the stall event.
	 *	@param InThreadId; Thread index for this thread.
	 *	@param InPerThreadIDTLSSlot; TLS slot to store the pointer to me into (later)
	 *	@param bInAllowsStealsFromMe; If true, this is a worker thread and any other thread can steal tasks from my incoming queue.
	 *	@param bInStealsFromOthers If true, this is a worker thread and I will attempt to steal tasks when I run out of work.
	**/
	void Setup(ENamedThreads::Type InThreadId, uint32 InPerThreadIDTLSSlot, bool bInAllowsStealsFromMe, bool bInStealsFromOthers)
	{
		ThreadId = InThreadId;
		check(ThreadId >= 0);
		PerThreadIDTLSSlot = InPerThreadIDTLSSlot;
		bAllowsStealsFromMe = bInAllowsStealsFromMe;
		bStealsFromOthers = bInStealsFromOthers;
	}

	// Calls meant to be called from "this thread".

	/** A one-time call to set the TLS entry for this thread. **/
	void InitializeForCurrentThread()
	{
		FPlatformTLS::SetTlsValue(PerThreadIDTLSSlot,this);
	}

	/** Used for named threads to start processing tasks until the thread is idle and RequestQuit has been called. **/
	void ProcessTasksUntilQuit(int32 QueueIndex)
	{
		Queue(QueueIndex).QuitWhenIdle.Reset();
		while (Queue(QueueIndex).QuitWhenIdle.GetValue() == 0)
		{
			ProcessTasks(QueueIndex, true);
			// @Hack - quit now when running with only one thread.
			if (!FPlatformProcess::SupportsMultithreading())
			{
				break;
			}
		}
	}

	/** Tick single-threaded. */
	virtual void Tick() OVERRIDE
	{
		if (Queue(0).QuitWhenIdle.GetValue() == 0)
		{
			ProcessTasks(0, true);
		}
	}

	/** 
	 *	Process tasks until idle. May block if bAllowStall is true
	 *	@param QueueIndex, Queue to process tasks from
	 *	@param bAllowStall,  if true, the thread will block on the stall event when it runs out of tasks.
	 **/
	void ProcessTasks(int32 QueueIndex, bool bAllowStall)
	{
#if STATS
		checkAtCompileTime(ENamedThreads::StatsThread == 0, delete_this_testing_a_build_fail);
		TStatId StatName;
		FCycleCounter ProcessingTasks;
		if (ThreadId == ENamedThreads::GameThread)
		{
			StatName = GET_STATID(STAT_TaskGraph_GameTasks);
		}
		else if (ThreadId == ENamedThreads::RenderThread)
		{
			//StatName = none, we need to let the scope empty so that the render thread submits tasks in a timely manner
		}
		else if (ThreadId != ENamedThreads::StatsThread)
		{
			StatName = GET_STATID(STAT_TaskGraph_OtherTasks);
		}
		bool bTasksOpen = false;
#endif
		check(!Queue(QueueIndex).RecursionGuard.GetValue());
		Queue(QueueIndex).RecursionGuard.Increment();
		while (1)
		{
			FBaseGraphTask* Task = NULL;
			Task = Queue(QueueIndex).PrivateQueue.Dequeue();
			if (!Task)
			{
				if (!bAllowsStealsFromMe)
				{
					// no steals so we will take all of the items, and this also ensures ordering
					for (int32 Count = SPIN_COUNT + 1; Count && !NewTasks.Num() ; Count--)
					{
						Queue(QueueIndex).IncomingQueue.PopAll(NewTasks);
					}
					if (FPlatformProcess::SupportsMultithreading())
					{
						for (int32 Count = SLEEP_COUNT; Count && !NewTasks.Num() ; Count--)
						{
							FPlatformProcess::Sleep(0.0f);
							Queue(QueueIndex).IncomingQueue.PopAll(NewTasks);
						}
					}
					if (!NewTasks.Num())
					{
						if (bAllowStall)
						{
#if STATS
							if(bTasksOpen)
							{
								ProcessingTasks.Stop();
								bTasksOpen = false;
							}
#endif
							if (Stall(QueueIndex))
							{
								Queue(QueueIndex).IncomingQueue.PopAll(NewTasks);
							}
						}
					}
					if (NewTasks.Num())
					{
						for (int32 Index = NewTasks.Num() - 1; Index >= 0 ; Index--) // reverse the order since PopAll is implicitly backwards
						{
							Queue(QueueIndex).PrivateQueue.Enqueue(NewTasks[Index]);
						}
						Task = Queue(QueueIndex).PrivateQueue.Dequeue();
						NewTasks.Reset();
					}
				}
				else
				{
					// because of stealing, we are only going to take one item
					for (int32 Count = SPIN_COUNT + 1; !Task && Count ; Count--)
					{
						Task = Queue(QueueIndex).IncomingQueue.PopIfNotClosed();
						if (!Task)
						{
							Task = FindWork();
						}
					}
					if (FPlatformProcess::SupportsMultithreading())
					{
						for (int32 Count = SLEEP_COUNT; !Task && Count ; Count--)
						{
							FPlatformProcess::Sleep(0.0f);
							Task = Queue(QueueIndex).IncomingQueue.PopIfNotClosed();
							if (!Task)
							{
								Task = FindWork();
							}
						}
					}
					if (!Task)
					{
						if (bAllowStall)
						{
#if STATS
							if(bTasksOpen)
							{
								ProcessingTasks.Stop();
								bTasksOpen = false;
							}
#endif
							if (Stall(QueueIndex))
							{
								Task = Queue(QueueIndex).IncomingQueue.PopIfNotClosed();
							}
						}
					}
				}
			}
			if (Task)
			{
#if STATS
				if (!bTasksOpen && FThreadStats::IsCollectingData(StatName))
				{
					bTasksOpen = true;
					ProcessingTasks.Start(*StatName);
				}
#endif
				Task->Execute(NewTasks, ENamedThreads::Type(ThreadId | (QueueIndex << ENamedThreads::QueueIndexShift)));
			}
			else
			{
				break;
			}
		}
#if STATS
		if(bTasksOpen)
		{
			ProcessingTasks.Stop();
			bTasksOpen = false;
		}
#endif
		Queue(QueueIndex).RecursionGuard.Decrement();
		check(!Queue(QueueIndex).RecursionGuard.GetValue());
	}

	/** 
	 *	Queue a task, assuming that this thread is the same as the current thread.
	 *	For named threads, these go directly into the private queue.
	 *	@param QueueIndex, Queue to enqueue for
	 *	@param Task Task to queue.
	 **/
	void EnqueueFromThisThread(int32 QueueIndex, FBaseGraphTask* Task)
	{
		checkThreadGraph(Queue(QueueIndex).StallRestartEvent); // make sure we are started up
		if (bAllowsStealsFromMe)
		{
			bool bWasReopenedByMe = Queue(QueueIndex).IncomingQueue.ReopenIfClosedAndPush(Task);
			checkThreadGraph(!bWasReopenedByMe); // if I am stalled, why am I here?
		}
		else
		{
			checkThreadGraph((FTaskThread*)FPlatformTLS::GetTlsValue(PerThreadIDTLSSlot) == this); // verify that we are the thread they say we are
			Queue(QueueIndex).PrivateQueue.Enqueue(Task);
		}
	}


	// Calls meant to be called from any thread.

	/** 
	 *	Will cause the thread to return to the caller when it becomes idle. Used to return from ProcessTasksUntilQuit for named threads or to shut down unnamed threads. 
	 *	CAUTION: This will not work under arbitrary circumstances. For example you should not attempt to stop unnamed threads unless they are known to be idle.
	 *	Return requests for named threads should be submitted from that named thread as FReturnGraphTask does.
	 *	@param QueueIndex, Queue to request quit from
	**/
	void RequestQuit(int32 QueueIndex)
	{
		// this will not work under arbitrary circumstances. For example you should not attempt to stop threads unless they are known to be idle.
		checkThreadGraph(Queue(QueueIndex).StallRestartEvent); // make sure we are started up
		Queue(QueueIndex).QuitWhenIdle.Increment();
		Queue(QueueIndex).StallRestartEvent->Trigger(); 
	}

	/** 
	 *	Check the (unsafe) status of a thread.
	 *	@return true if this thread was idle. 
	 *	CAUTION the status of the thread can easily change before this routine returns.
	 **/
	bool IsProbablyStalled() const
	{
		checkThreadGraph(Queue(0).StallRestartEvent); // make sure we are started up
		return IsStalled.GetValue() != 0;
	}

	/** Return the index of this thread. **/
	ENamedThreads::Type GetThreadId() const
	{
		checkThreadGraph(Queue(0).StallRestartEvent); // make sure we are started up
		return ThreadId;
	}

	/** 
	 *	Queue a task, assuming that this thread is not the same as the current thread.
	 *	@param QueueIndex, Queue to enqueue into
	 *	@param Task; Task to queue.
	 **/
	bool EnqueueFromOtherThread(int32 QueueIndex, FBaseGraphTask* Task)
	{
		checkThreadGraph(Queue(QueueIndex).StallRestartEvent); // make sure we are started up
		bool bWasReopenedByMe = Queue(QueueIndex).IncomingQueue.ReopenIfClosedAndPush(Task);
		if (bWasReopenedByMe)
		{
			Queue(QueueIndex).StallRestartEvent->Trigger();
		}
		return bWasReopenedByMe;
	}

	/** 
	 *	Attempt to give up a task for another thread.
	 *	@return Task; Stolen task, if one was found, otherwise NULL.
	 **/
	FBaseGraphTask* RequestSteal()
	{
		checkThreadGraph(bAllowsStealsFromMe); 
		return Queue(0).IncomingQueue.PopIfNotClosed();
	}

	/** 
	 *Return true if this thread is processing tasks. This is only a "guess" if you ask for a thread other than yourself because that can change before the function returns.
	 *@param QueueIndex, Queue to request quit from
	 **/
	bool IsProcessingTasks(int32 QueueIndex)
	{
		return !!Queue(QueueIndex).RecursionGuard.GetValue();
	}

	// FRunnable API

	/**
	 * Allows per runnable object initialization. NOTE: This is called in the
	 * context of the thread object that aggregates this, not the thread that
	 * passes this runnable to a new thread.
	 *
	 * @return True if initialization was successful, false otherwise
	 */
	virtual bool Init()
	{
		InitializeForCurrentThread();
		return true;
	}

	/**
	 * This is where all per object thread work is done. This is only called
	 * if the initialization was successful.
	 *
	 * @return The exit code of the runnable object
	 */
	virtual uint32 Run()
	{
		ProcessTasksUntilQuit(0);
		return 0;
	}

	/**
	 * This is called if a thread is requested to terminate early
	 */
	virtual void Stop()
	{
		RequestQuit(0);
	}

	/**
	 * Called in the context of the aggregating thread to perform any cleanup.
	 */
	virtual void Exit()
	{
	}

	/**
	 * Return single threaded interface when multithreading is disabled.
	 */
	virtual FSingleThreadRunnable* GetSingleThreadInterface() OVERRIDE
	{
		return this;
	}

private:

	enum
	{
		/** The number of times to look for work before deciding to block on the stall event **/
		SPIN_COUNT=0,
		/** The number of times to call FPlatformProcess::Sleep(0) and look for work before deciding to block on the stall event **/
		SLEEP_COUNT=0,
	};

	/** Grouping of the data for an individual queue. **/
	struct FThreadTaskQueue
	{
		/** A non-threas safe queue for the thread locked tasks of a named thread **/
		FTaskQueue											PrivateQueue;
		/** 
		 *	For named threads, this is a queue of thread locked tasks coming from other threads. They are not stealable.
		 *	For unnamed thread this is the public queue, subject to stealing.
		 *	In either case this queue is closely related to the stall event. Other threads that reopen the incoming queue must trigger the stall event to allow the thread to run.
		**/
		TReopenableLockFreePointerList<FBaseGraphTask>		IncomingQueue;
		/** Used to signal the thread to quit when idle. **/
		FThreadSafeCounter									QuitWhenIdle;
		/** We need to disallow reentry of the processing loop **/
		FThreadSafeCounter									RecursionGuard;
		/** Event that this thread blocks on when it runs out of work. **/
		FEvent*												StallRestartEvent;

		FThreadTaskQueue()
			: StallRestartEvent(FPlatformProcess::CreateSynchEvent(true))
		{

		}
		~FThreadTaskQueue()
		{
			delete StallRestartEvent;
			StallRestartEvent = NULL;
		}
	};

	/**
	 *	Internal function to block on the stall wait event.
	 *  @param QueueIndex, Queue to stall
	 *	@return true if the thread actually stalled; false in the case of a stop request or a task arrived while we were trying to stall.
	 */
	bool Stall(int32 QueueIndex)
	{
		checkThreadGraph(Queue(QueueIndex).StallRestartEvent); // make sure we are started up
		if (Queue(QueueIndex).QuitWhenIdle.GetValue() == 0)
		{
			// @Hack - Only stall when multithreading is enabled.
			if (FPlatformProcess::SupportsMultithreading())
			{
				Queue(QueueIndex).StallRestartEvent->Reset();
				FPlatformMisc::MemoryBarrier();
				if (Queue(QueueIndex).IncomingQueue.CloseIfEmpty())
				{
					int32 NewValue = IsStalled.Increment();
					NotifyStalling();
					checkThreadGraph(NewValue == 1); // there should be no concurrent calls to Stall!
					Queue(QueueIndex).StallRestartEvent->Wait();
					NewValue = IsStalled.Decrement();
					checkThreadGraph(NewValue == 0); // there should be no concurrent calls to Stall!
					return true;
				}
			}
			else 
			{
				return true;
			}
		}
		return false;
	}

	/**
	 *	Internal function to call the system looking for work. Called from this thread.
	 *	@return New task to process.
	 */
	FBaseGraphTask* FindWork();

	/**
	 *	Internal function to notify the that system that I am stalling. This is a hint to give me a job asap.
	 */
	void NotifyStalling();

	FORCEINLINE FThreadTaskQueue& Queue(int32 QueueIndex)
	{
		checkThreadGraph(QueueIndex >= 0 && QueueIndex < ENamedThreads::NumQueues && (!bAllowsStealsFromMe || !QueueIndex)); // range check, unnamed threads cannot use an alternate queue
		return Queues[QueueIndex];
	}
	FORCEINLINE const FThreadTaskQueue& Queue(int32 QueueIndex) const
	{
		checkThreadGraph(QueueIndex >= 0 && QueueIndex < ENamedThreads::NumQueues && (!bAllowsStealsFromMe || !QueueIndex)); // range check, unnamed threads cannot use an alternate queue
		return Queues[QueueIndex];
	}

	/** Array of queues, only the first one is used for unnamed threads. **/
	FThreadTaskQueue Queues[ENamedThreads::NumQueues];

	/** Id / Index of this thread. **/
	ENamedThreads::Type									ThreadId;
	/** TLS SLot that we store the FTaskThread* this pointer in. **/
	uint32												PerThreadIDTLSSlot;
	/** Used to signal stalling. Not safe for synchronization in most cases. **/
	FThreadSafeCounter									IsStalled;
	/** If true, this is a worker thread and any other thread can steal tasks from my incoming queue. **/
	bool												bAllowsStealsFromMe;
	/** If true, this is a worker thread and I will attempt to steal tasks when I run out of work. **/
	bool												bStealsFromOthers;

};




/** 
 *	FTaskGraphImplementation
 *	Implementation of the centralized part of the task graph system.
 *	These parts of the system have no knowledge of the dependency graph, they exclusively work on tasks.
**/
class FTaskGraphImplementation : public FTaskGraphInterface  
{
	/** 
	 *	FWorkerThread
	 *	Helper structure to aggregate a few items related to the individual threads.
	**/
	struct FWorkerThread
	{
		/** The actual FTaskThread that manager this task **/
		FTaskThread			TaskGraphWorker;
		/** For internal threads, the is non-NULL and holds the information about the runable thread that was created. **/
		FRunnableThread*	RunnableThread;
		/** For external threads, this determines if they have been "attached" yet. Attachment is mostly setting up TLS for this individual thread. **/
		bool				bAttached;

		/** Constructor to set reasonable defaults. **/
		FWorkerThread()
			: RunnableThread(NULL)
			, bAttached(false)
		{
		}
	};

public:

	// API related to life cycle of the system and singletons

	/** 
	 *	Singleton returning the one and only FTaskGraphImplementation.
	 *	Note that unlike most singletons, a manual call to FTaskGraphInterface::Startup is required before the singleton will return a valid reference.
	**/
	static FTaskGraphImplementation& Get()
	{		
		checkThreadGraph(TaskGraphImplementationSingleton);
		return *TaskGraphImplementationSingleton;
	}

	/** 
	 *	Constructor - initializes the data structures, sets the singleton pointer and creates the internal threads.
	 *	@param InNumThreads; total number of threads in the system, including named threads, unnamed threads, internal threads and external threads. Must be at least 1 + the number of named threads.
	**/
	FTaskGraphImplementation(int32 InNumThreads)
	{
		// if we don't want any performance-based threads, then force the task graph to not create any worker threads, and run in game thread
		if (!FPlatformProcess::SupportsMultithreading())
		{
			// this is the logic that used to be spread over a couple of places, that will make the rest of this function disable a worker thread
			// @todo: it could probably be made simpler/clearer
			InNumThreads = 1;
			// this - 1 tells the below code there is no rendering thread
			LastExternalThread = (ENamedThreads::Type)(ENamedThreads::ActualRenderingThread - 1);
		}
		else
		{
			LastExternalThread = ENamedThreads::ActualRenderingThread;
		}
		
		NumNamedThreads = LastExternalThread + 1;
		NumThreads = FMath::Max<int32>(FMath::Min<int32>(InNumThreads,MAX_THREADS),NumNamedThreads + 1);
		// Cap number of extra threads to the platform worker thread count
		NumThreads = FMath::Min(NumThreads, NumNamedThreads + FPlatformMisc::NumberOfWorkerThreadsToSpawn());
		UE_LOG(LogTaskGraph, Log, TEXT("Started task graph with %d named threads and %d total threads."), NumNamedThreads, NumThreads);
		check(NumThreads - NumNamedThreads >= 1);  // need at least one pure worker thread
		check(NumThreads <= MAX_THREADS);
		check(!NextStealFromThread.GetValue()); // reentrant?
		NextStealFromThread.Increment(); // just checking for reentrancy
		PerThreadIDTLSSlot = FPlatformTLS::AllocTlsSlot();

		NextUnnamedThreadMod = NumThreads - NumNamedThreads;

		for (int32 ThreadIndex = 0; ThreadIndex < NumThreads; ThreadIndex++)
		{
			check(!WorkerThreads[ThreadIndex].bAttached); // reentrant?
			bool bAllowsStealsFromMe = ThreadIndex >= NumNamedThreads;
			bool bStealsFromOthers = ThreadIndex >= NumNamedThreads;
			WorkerThreads[ThreadIndex].TaskGraphWorker.Setup(ENamedThreads::Type(ThreadIndex), PerThreadIDTLSSlot, bAllowsStealsFromMe, bStealsFromOthers);
		}

		TaskGraphImplementationSingleton = this; // now reentrancy is ok

		for (int32 ThreadIndex = LastExternalThread + 1; ThreadIndex < NumThreads; ThreadIndex++)
		{
			FString Name = FString::Printf(TEXT("TaskGraphThread_%d"), ThreadIndex - (LastExternalThread + 1));
			uint32 StackSize = 256 * 1024;
			WorkerThreads[ThreadIndex].RunnableThread = FRunnableThread::Create( &Thread(ThreadIndex), *Name, false, false, StackSize, TPri_Normal ); // these are below normal threads? so that they sleep when the named threads are active
			WorkerThreads[ThreadIndex].bAttached = true;
		}
	}

	/** 
	 *	Destructor - probably only works reliably when the system is completely idle. The system has no idea if it is idle or not.
	**/
	virtual ~FTaskGraphImplementation()
	{
		for (int32 ThreadIndex = 0; ThreadIndex < NumThreads; ThreadIndex++)
		{
			Thread(ThreadIndex).RequestQuit(0);
		}
		for (int32 ThreadIndex = 0; ThreadIndex < NumThreads; ThreadIndex++)
		{
			if (ThreadIndex > LastExternalThread)
			{
				WorkerThreads[ThreadIndex].RunnableThread->WaitForCompletion();
				delete WorkerThreads[ThreadIndex].RunnableThread;
				WorkerThreads[ThreadIndex].RunnableThread = NULL;
			}
			WorkerThreads[ThreadIndex].bAttached = false;
		}
		TaskGraphImplementationSingleton = NULL;
		NextUnnamedThreadMod = 0;
		TArray<FTaskThread*> NotProperlyUnstalled;
		StalledUnnamedThreads.PopAll(NotProperlyUnstalled);
		FPlatformTLS::FreeTlsSlot(PerThreadIDTLSSlot);
	}


	// API inherited from FTaskGraphInterface

	/** 
	 *	Function to queue a task, called from a FBaseGraphTask
	 *	@param	Task; the task to queue
	 *	@param	ThreadToExecuteOn; Either a named thread for a threadlocked task or ENamedThreads::AnyThread for a task that is to run on a worker thread
	 *	@param	CurrentThreadIfKnown; This should be the current thread if it is known, or otherwise use ENamedThreads::AnyThread and the current thread will be determined.
	**/
	virtual void QueueTask(FBaseGraphTask* Task, ENamedThreads::Type ThreadToExecuteOn, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread) OVERRIDE
	{
		checkThreadGraph(NextUnnamedThreadMod);
		if (CurrentThreadIfKnown == ENamedThreads::AnyThread)
		{
			 CurrentThreadIfKnown = GetCurrentThread();
		}
		else
		{
			CurrentThreadIfKnown = ENamedThreads::GetThreadIndex(CurrentThreadIfKnown);
			checkThreadGraph(CurrentThreadIfKnown == GetCurrentThread());
		}
		if (ThreadToExecuteOn == ENamedThreads::AnyThread)
		{
			FTaskThread* TempTarget = StalledUnnamedThreads.Pop(); //@todo it is possible that a thread is in the process of stalling and we just missed it, non-fatal, but we could lose a whole task of potential parallelism.
			if (TempTarget)
			{
				ThreadToExecuteOn = TempTarget->GetThreadId();
			}
			else
			{
				// Run everything on the game thread if multithreading is disabled.
				if (FPlatformProcess::SupportsMultithreading())
				{
					ThreadToExecuteOn = ENamedThreads::Type((uint32(NextUnnamedThreadForTaskFromUnknownThread.Increment()) % uint32(NextUnnamedThreadMod)) + NumNamedThreads);
				}
				else
				{
					ThreadToExecuteOn = ENamedThreads::GameThread;
				}
			}
		}
		int32 QueueToExecuteOn = ENamedThreads::GetQueueIndex(ThreadToExecuteOn);
		ThreadToExecuteOn = ENamedThreads::GetThreadIndex(ThreadToExecuteOn);
		FTaskThread* Target = Target = &Thread(ThreadToExecuteOn);
		if (ThreadToExecuteOn == CurrentThreadIfKnown)
		{
			Target->EnqueueFromThisThread(QueueToExecuteOn, Task);
		}
		else
		{
			Target->EnqueueFromOtherThread(QueueToExecuteOn, Task);
		}
	}


	virtual	int32 GetNumWorkerThreads() OVERRIDE
	{
		return NumThreads - NumNamedThreads;
	}

	virtual ENamedThreads::Type GetCurrentThreadIfKnown() OVERRIDE
	{
		return GetCurrentThread();
	}

	virtual bool IsThreadProcessingTasks(ENamedThreads::Type ThreadToCheck) OVERRIDE
	{
		int32 QueueIndex = ENamedThreads::GetQueueIndex(ThreadToCheck);
		ThreadToCheck = ENamedThreads::GetThreadIndex(ThreadToCheck);
		check(ThreadToCheck >= 0 && ThreadToCheck < NumNamedThreads);
		return Thread(ThreadToCheck).IsProcessingTasks(QueueIndex);
	}

	// External Thread API

	virtual void AttachToThread(ENamedThreads::Type CurrentThread) OVERRIDE
	{
		CurrentThread = ENamedThreads::GetThreadIndex(CurrentThread);
		check(NextUnnamedThreadMod);
		check(CurrentThread >= 0 && CurrentThread < NumNamedThreads);
		check(!WorkerThreads[CurrentThread].bAttached);
		Thread(CurrentThread).InitializeForCurrentThread();
	}

	virtual void ProcessThreadUntilIdle(ENamedThreads::Type CurrentThread) OVERRIDE
	{
		int32 QueueIndex = ENamedThreads::GetQueueIndex(CurrentThread);
		CurrentThread = ENamedThreads::GetThreadIndex(CurrentThread);
		check(CurrentThread >= 0 && CurrentThread < NumNamedThreads);
		check(CurrentThread == GetCurrentThread());
		Thread(CurrentThread).ProcessTasks(QueueIndex, false);
	}

	virtual void ProcessThreadUntilRequestReturn(ENamedThreads::Type CurrentThread) OVERRIDE
	{
		int32 QueueIndex = ENamedThreads::GetQueueIndex(CurrentThread);
		CurrentThread = ENamedThreads::GetThreadIndex(CurrentThread);
		check(CurrentThread >= 0 && CurrentThread < NumNamedThreads);
		check(CurrentThread == GetCurrentThread());
		Thread(CurrentThread).ProcessTasksUntilQuit(QueueIndex);
	}

	virtual void RequestReturn(ENamedThreads::Type CurrentThread) OVERRIDE
	{
		int32 QueueIndex = ENamedThreads::GetQueueIndex(CurrentThread);
		CurrentThread = ENamedThreads::GetThreadIndex(CurrentThread);
		check(CurrentThread != ENamedThreads::AnyThread);
		Thread(CurrentThread).RequestQuit(QueueIndex);
	}

	virtual void WaitUntilTasksComplete(const FGraphEventArray& Tasks, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread) OVERRIDE
	{
		ENamedThreads::Type CurrentThread = CurrentThreadIfKnown;
		if (CurrentThreadIfKnown == ENamedThreads::AnyThread)
		{
			CurrentThreadIfKnown = GetCurrentThread();
			CurrentThread = CurrentThreadIfKnown;
		}
		else
		{
			CurrentThreadIfKnown = ENamedThreads::GetThreadIndex(CurrentThreadIfKnown);
			check(CurrentThreadIfKnown == GetCurrentThread());
			// we don't modify CurrentThread here because it might be a local queue
		}

		if (CurrentThreadIfKnown != ENamedThreads::AnyThread && CurrentThreadIfKnown < NumNamedThreads && !IsThreadProcessingTasks(CurrentThread))
		{
			bool bAnyPending = false;
			for (int32 Index = 0; Index < Tasks.Num(); Index++)
			{
				if (!Tasks[Index]->IsComplete())
				{
					bAnyPending = true;
					break;
				}
			}
			if (!bAnyPending)
			{
				return;
			}
			// named thread process tasks while we wait
			TGraphTask<FReturnGraphTask>::CreateTask(&Tasks, CurrentThread).ConstructAndDispatchWhenReady(CurrentThread);
			ProcessThreadUntilRequestReturn(CurrentThread);
		}
		else
		{
			// We will just stall this thread on an event while we wait
			FScopedEvent Event;
			TriggerEventWhenTasksComplete(Event.Get(), Tasks, CurrentThreadIfKnown);
		}
	}

	virtual void TriggerEventWhenTasksComplete(FEvent* InEvent, const FGraphEventArray& Tasks, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread)
	{
		check(InEvent);
		bool bAnyPending = false;
		for (int32 Index = 0; Index < Tasks.Num(); Index++)
		{
			if (!Tasks[Index]->IsComplete())
			{
				bAnyPending = true;
				break;
			}
		}
		if (!bAnyPending)
		{
			InEvent->Trigger();
			return;
		}
		TGraphTask<FTriggerEventGraphTask>::CreateTask(&Tasks, CurrentThreadIfKnown).ConstructAndDispatchWhenReady(InEvent);
	}

	// API used by FWorkerThread's

	/** 
	 *	Attempt to steal some work from another thread.
	 *	@param	ThreadInNeed; Id of the thread requesting work.
	 *	@return Task that was stolen if any was found.
	**/
	FBaseGraphTask* FindWork(ENamedThreads::Type ThreadInNeed)
	{
		// this can be called before my constructor is finished
		for (int32 Pass = 0; Pass < 2; Pass++)
		{
			for (int32 Test = ThreadInNeed - 1; Test >= NumNamedThreads; Test--)
			{
				if (Pass || !Thread(Test).IsProbablyStalled())
				{
					FBaseGraphTask* Task = Thread(Test).RequestSteal();
					if (Task)
					{
						return Task;
					}
				}
			}
			for (int32 Test = NumThreads - 1; Test > ThreadInNeed; Test--)
			{
				if (Pass || !Thread(Test).IsProbablyStalled())
				{
					FBaseGraphTask* Task = Thread(Test).RequestSteal();
					if (Task)
					{
						return Task;
					}
				}
			}
		}
		return NULL;
	}

	/** 
	 *	Hint from a worker thread that it is stalling.
	 *	@param	StallingThread; Id of the thread that is stalling.
	**/
	void NotifyStalling(ENamedThreads::Type StallingThread)
	{
		if (StallingThread >= NumNamedThreads)
		{
			StalledUnnamedThreads.Push(&Thread(StallingThread));
		}
	}

private:

	// Internals

	/** 
	 *	Internal function to verify an index and return the corresponding FTaskThread
	 *	@param	Index; Id of the thread to retrieve.
	 *	@return	Reference to the corresponding thread.
	**/
	FTaskThread& Thread(int32 Index)
	{
		checkThreadGraph(Index >= 0 && Index < NumThreads);
		checkThreadGraph(WorkerThreads[Index].TaskGraphWorker.GetThreadId() == Index);
		return WorkerThreads[Index].TaskGraphWorker;
	}

	/** 
	 *	Examines the TLS to determine the identity of the current thread.
	 *	@return	Id of the thread that is this thread or ENamedThreads::AnyThread if this thread is unknown or is a named thread that has not attached yet.
	**/
	ENamedThreads::Type GetCurrentThread()
	{
		checkAtCompileTime(offsetof(FWorkerThread,TaskGraphWorker)==0,taskgraph_worker_must_be_first_member);
		ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread;
		FWorkerThread* TLSPointer = (FWorkerThread*)FPlatformTLS::GetTlsValue(PerThreadIDTLSSlot);
		if (TLSPointer)
		{
			checkThreadGraph(TLSPointer - WorkerThreads >= 0 && TLSPointer - WorkerThreads < NumThreads);
			CurrentThreadIfKnown = ENamedThreads::Type(TLSPointer - WorkerThreads);
			checkThreadGraph(Thread(CurrentThreadIfKnown).GetThreadId() == CurrentThreadIfKnown);
		}
		return CurrentThreadIfKnown;
	}


	enum
	{
		/** Compile time maximum number of threads. @todo Didn't really need to be a compile time constant. **/
		MAX_THREADS=8
	};

	/** Per thread data. **/
	FWorkerThread		WorkerThreads[MAX_THREADS];
	/** Number of threads actually in use. **/
	int32				NumThreads;
	/** Number of named threads actually in use. **/
	int32				NumNamedThreads;
	/** 
	 * "External Threads" are not created, the thread is created elsewhere and makes an explicit call to run 
	 * Here all of the named threads are external but that need not be the case.
	 * All unnamed threads must be internal
	**/
	ENamedThreads::Type LastExternalThread;
	/** Counter used to distribute new jobs to "any thread". **/
	FThreadSafeCounter	NextUnnamedThreadForTaskFromUnknownThread;
	/** Number of unnamed threads. **/
	int32					NextUnnamedThreadMod;
	/** Counter used to determine next thread to attempt to steal from. **/
	FThreadSafeCounter	NextStealFromThread;
	/** Index of TLS slot for FWorkerThread* pointer. **/
	uint32				PerThreadIDTLSSlot;
	/** Thread safe list of stalled thread "Hints". **/
	TLockFreePointerList<FTaskThread>		StalledUnnamedThreads; 
};


// Implementations of FTaskThread function that require knowledge of FTaskGraphImplementation

FBaseGraphTask* FTaskThread::FindWork()
{
	return FTaskGraphImplementation::Get().FindWork(ThreadId);
}

void FTaskThread::NotifyStalling()
{
	return FTaskGraphImplementation::Get().NotifyStalling(ThreadId);
}



// Statics in FTaskGraphInterface

void FTaskGraphInterface::Startup(int32 NumThreads)
{
	// TaskGraphImplementationSingleton is actually set in the constructor because find work will be called before this returns.
	new FTaskGraphImplementation(NumThreads); 
}

void FTaskGraphInterface::Shutdown()
{
	delete TaskGraphImplementationSingleton;
	TaskGraphImplementationSingleton = NULL;
}

FTaskGraphInterface& FTaskGraphInterface::Get()
{
	checkThreadGraph(TaskGraphImplementationSingleton);
	return *TaskGraphImplementationSingleton;
}


// Statics and some implementations from FBaseGraphTask and FGraphEvent

TLockFreeFixedSizeAllocator<FBaseGraphTask::SMALL_TASK_SIZE>& FBaseGraphTask::GetSmallTaskAllocator()
{
	static TLockFreeFixedSizeAllocator<FBaseGraphTask::SMALL_TASK_SIZE> TheAllocator;
	return TheAllocator;
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
void FBaseGraphTask::LogPossiblyInvalidSubsequentsTask(const TCHAR* TaskName)
{
	UE_LOG(LogTaskGraph, Warning, TEXT("Subsequents of %s look like they contain invalid pointer(s)."), TaskName);
}
#endif

TLockFreeClassAllocator<FGraphEvent>& FGraphEvent::GetAllocator()
{
	static TLockFreeClassAllocator<FGraphEvent> TheAllocator;
	return TheAllocator;
}

FGraphEventRef FGraphEvent::CreateGraphEvent()
{
	return GetAllocator().New();
}

void FGraphEvent::DispatchSubsequents(ENamedThreads::Type CurrentThreadIfKnown)
{
	if (EventsToWaitFor.Num())
	{
		// need to save this first and empty the actual tail of the task might be recycled faster than it is cleared.
		FGraphEventArray TempEventsToWaitFor;
		Exchange(EventsToWaitFor, TempEventsToWaitFor);
		// create the Gather...this uses a special version of private CreateTask that "assumes" the subsequent list (which other threads might still be adding too).
		TGraphTask<FNullGraphTask>::CreateTask(TRefCountPtr<FGraphEvent>(this), &TempEventsToWaitFor, CurrentThreadIfKnown).ConstructAndDispatchWhenReady(TEXT("DontCompleteUntil"));
		return;
	}
	//@todo need to get rid of these arrays, or at least use inline allocators
	TArray<FBaseGraphTask*> NewTasks;
	NewTasks.Reset(128);
	SubsequentList.PopAllAndClose(NewTasks);
	for (int32 Index = NewTasks.Num() - 1; Index >= 0 ; Index--) // reverse the order since PopAll is implicitly backwards
	{
		FBaseGraphTask* NewTask = NewTasks[Index];
		checkThreadGraph(NewTask);
		NewTask->ConditionalQueueTask(CurrentThreadIfKnown);
	}
}

void FGraphEvent::DispatchSubsequents(TArray<FBaseGraphTask*>& NewTasks, ENamedThreads::Type CurrentThreadIfKnown)
{
	if (EventsToWaitFor.Num())
	{
		// need to save this first and empty the actual tail of the task might be recycled faster than it is cleared.
		FGraphEventArray TempEventsToWaitFor;
		Exchange(EventsToWaitFor, TempEventsToWaitFor);
		// create the Gather...this uses a special version of private CreateTask that "assumes" the subsequent list (which other threads might still be adding too).
		TGraphTask<FNullGraphTask>::CreateTask(TRefCountPtr<FGraphEvent>(this), &TempEventsToWaitFor, CurrentThreadIfKnown).ConstructAndDispatchWhenReady(TEXT("DontCompleteUntil"));
		return;
	}

	SubsequentList.PopAllAndClose(NewTasks);
	for (int32 Index = NewTasks.Num() - 1; Index >= 0 ; Index--) // reverse the order since PopAll is implicitly backwards
	{
		FBaseGraphTask* NewTask = NewTasks[Index];
		checkThreadGraph(NewTask);
		NewTask->ConditionalQueueTask(CurrentThreadIfKnown);
	}
	NewTasks.Reset();
}

void FGraphEvent::Recycle(FGraphEvent* ToRecycle)
{
	GetAllocator().Free(ToRecycle);
}

FGraphEvent::~FGraphEvent()
{
#if DO_CHECK
	if (!IsComplete())
	{
		// Verifies that the event is completed. We do not allow events to die before completion.
		TArray<FBaseGraphTask*> NewTasks;
		SubsequentList.PopAllAndClose(NewTasks);
		checkThreadGraph(!NewTasks.Num());
	}
#endif
	CheckDontCompleteUntilIsEmpty(); // We should not have any wait untils outstanding
}




