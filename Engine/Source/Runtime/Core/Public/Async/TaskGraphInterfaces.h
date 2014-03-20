// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TaskGraphInterfaces.h: TaskGraph library
=============================================================================*/

#pragma once

#if !defined(STATS)
#error "STATS must be defined as either zero or one."
#endif
#include "LockFreeList.h"
#include "LockFreeFixedSizeAllocator.h"
#include "Stats.h"

// what level of checking to perform...normally checkSlow but could be ensure or check
#define checkThreadGraph checkSlow

DECLARE_STATS_GROUP(TEXT("Task Graph Tasks"), STATGROUP_TaskGraphTasks);

DECLARE_CYCLE_STAT_EXTERN(TEXT("FReturnGraphTask"), STAT_FReturnGraphTask, STATGROUP_TaskGraphTasks, CORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("FNullGraphTask"), STAT_FNullGraphTask, STATGROUP_TaskGraphTasks, CORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("FTriggerEventGraphTask"), STAT_FTriggerEventGraphTask, STATGROUP_TaskGraphTasks, CORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("FSimpleDelegateGraphTask"), STAT_FSimpleDelegateGraphTask, STATGROUP_TaskGraphTasks, CORE_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("FSimpleDelegateGraphTask"), STAT_FDelegateGraphTask, STATGROUP_TaskGraphTasks, CORE_API);

namespace ENamedThreads
{
	enum Type
	{
		/** not actually a thread index. Means "Unknown Thread" or "Any Unnamed Thread" **/
		AnyThread = -1, 

		/** The always-present, named threads are listed next **/
#if STATS
		StatsThread, 
#endif
		GameThread,
		// The render thread is sometimes the game thread and is sometimes the actual rendering thread
		ActualRenderingThread = GameThread + 1,
		// CAUTION ThreadedRenderingThread must be the last named thread, insert new named threads before it


		/** High bits are used for a queue index **/

		MainQueue =			0x000,
		LocalQueue =		0x100,

		NumQueues =			2,
		ThreadIndexMask =	0xff,
		QueueIndexMask =	0x100,
		QueueIndexShift =	8,

		/** Combinations **/
#if STATS
		StatsThread_Local = StatsThread | LocalQueue,
#endif
		GameThread_Local = GameThread | LocalQueue,
		ActualRenderingThread_Local = ActualRenderingThread | LocalQueue,
	};
	extern CORE_API Type RenderThread; // this is not an enum, because if there is no render thread, this is just the game thread.
	extern CORE_API Type RenderThread_Local; // this is not an enum, because if there is no render thread, this is just the game thread.

	FORCEINLINE Type GetThreadIndex(Type ThreadAndIndex)
	{
		return (ThreadAndIndex == AnyThread) ? AnyThread : Type(ThreadAndIndex & ThreadIndexMask);
	}

	FORCEINLINE int32 GetQueueIndex(Type ThreadAndIndex)
	{
		return (ThreadAndIndex & ~ThreadIndexMask) >> QueueIndexShift;
	}
}

namespace ESubsequentsMode
{
	enum Type
	{
		/** Necessary when another task will depend on this task. */
		TrackSubsequents,
		/** Can be used to save task graph overhead when firing off a task that will not be a dependency of other tasks. */
		FireAndForget
	};
}

/** Convenience typedef for a reference counted pointer to a graph event **/
typedef TRefCountPtr<class FGraphEvent> FGraphEventRef;

/** Convenience typedef for a an array a graph events **/
typedef TArray<FGraphEventRef, TInlineAllocator<4> > FGraphEventArray;

/** Interface tot he task graph system **/
class FTaskGraphInterface
{
	friend class FBaseGraphTask;
	/** 
	 *	Internal function to queue a task
	 *	@param	Task; the task to queue
	 *	@param	ThreadToExecuteOn; Either a named thread for a threadlocked task or ENamedThreads::AnyThread for a task that is to run on a worker thread
	 *	@param	CurrentThreadIfKnown; This should be the current thread if it is known, or otherwise use ENamedThreads::AnyThread and the current thread will be determined.
	**/
	virtual void QueueTask(class FBaseGraphTask* Task, ENamedThreads::Type ThreadToExecuteOn, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread) = 0;

public:

	virtual ~FTaskGraphInterface()
	{
	}

	// Startup, shutdown and singleton API

	/** 
	 *	Explicit start call for the system. The ordinary singleton pattern does not work because internal threads start asking for the singleton before the constructor has returned.
	 *	@param	NumThreads; Total number of threads in the system, must be 0 to disable separate taskgraph thread, at least 2 if !threadedrendering, else at least 3
	**/
	static CORE_API void Startup(int32 NumThreads);
	/** 
	 *	Explicit start call to shutdown the system. This is unlikely to work unless the system is idle.
	**/
	static CORE_API void Shutdown();
	/** 
	 *	Singleton for the system
	 *	@return a reference to the task graph system
	**/
	static CORE_API FTaskGraphInterface& Get();

	/** Return the current thread type, if known. **/
	virtual ENamedThreads::Type GetCurrentThreadIfKnown() = 0;

	/** Return the number of worker (non-named) threads **/
	virtual	int32 GetNumWorkerThreads() = 0;

	/** Return true if the given named thread is processing tasks. This is only a "guess" if you ask for a thread other than yourself because that can change before the function returns. **/
	virtual bool IsThreadProcessingTasks(ENamedThreads::Type ThreadToCheck) = 0;

	// External Thread API

	/** 
	 *	A one time call that "introduces" an external thread to the system. Basically, it just sets up the TLS info 
	 *	@param	CurrentThread; The name of the current thread.
	**/
	virtual void AttachToThread(ENamedThreads::Type CurrentThread)=0;

	/** 
	 *	Requests that a named thread, which must be this thread, run until idle, then return.
	 *	@param	CurrentThread; The name of this thread
	**/
	virtual void ProcessThreadUntilIdle(ENamedThreads::Type CurrentThread)=0;

	/** 
	 *	Requests that a named thread, which must be this thread, run until an explicit return request is received, then return.
	 *	@param	CurrentThread; The name of this thread
	**/
	virtual void ProcessThreadUntilRequestReturn(ENamedThreads::Type CurrentThread)=0;

	/** 
	 *	Request that the given thread stop when it is idle
	**/
	virtual void RequestReturn(ENamedThreads::Type CurrentThread)=0;

	/** 
	 *	Requests that a named thread, which must be this thread, run until a list of tasks is complete.
	 *	@param	Tasks - tasks to wait for
	 *	@param	CurrentThread - This thread, must be a named thread
	**/
	virtual void WaitUntilTasksComplete(const FGraphEventArray& Tasks, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread)=0;

	/** 
	 *	When a set of tasks complete, fire a scoped event
	 *	@param	InEvent - event to fire when the task is done
	 *	@param	Tasks - tasks to wait for
	 *	@param	CurrentThreadIfKnown - This thread, if known
	**/
	virtual void TriggerEventWhenTasksComplete(FEvent* InEvent, const FGraphEventArray& Tasks, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread)=0;

	/** 
	 *	Requests that a named thread, which must be this thread, run until a task is complete
	 *	@param	Task - task to wait for
	 *	@param	CurrentThread - This thread, must be a named thread
	**/
	void WaitUntilTaskCompletes(const FGraphEventRef& Task, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread)
	{
		FGraphEventArray Prerequistes;
		Prerequistes.Add(Task);
		WaitUntilTasksComplete(Prerequistes, CurrentThreadIfKnown);
	}

	/** 
	 *	When a task completes, fire a scoped event
	 *	@param	InEvent - event to fire when the task is done
	 *	@param	Task - task to wait for
	 *	@param	CurrentThreadIfKnown - This thread, if known
	**/
	void TriggerEventWhenTaskCompletes(FEvent* InEvent, const FGraphEventRef& Task, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread)
	{
		FGraphEventArray Prerequistes;
		Prerequistes.Add(Task);
		TriggerEventWhenTasksComplete(InEvent, Prerequistes, CurrentThreadIfKnown);
	}

};

/** 
 *	Base class for all tasks. 
 *	Tasks go through a very specific life stage progression, and this is verified.
 **/
class FBaseGraphTask
{
protected:
	/** 
	 *	Constructor
	 *	@param InNumberOfPrerequistitesOutstanding; the number of prerequisites outstanding. We actually add one to this to prevent the task from firing while we are setting up the task
	 **/
	FBaseGraphTask(int32 InNumberOfPrerequistitesOutstanding)
		: ThreadToExecuteOn(ENamedThreads::AnyThread)
		, NumberOfPrerequistitesOutstanding(InNumberOfPrerequistitesOutstanding + 1) // + 1 is not a prerequisite, it is a lock to prevent it from executing while it is getting prerequisites, one it is safe to execute, call PrerequisitesComplete
	{
		checkThreadGraph(LifeStage.Increment() == int32(LS_Contructed));
	}
	/** 
	 *	Sets the desired execution thread. This is not part of the constructor because this information may not be known quite yet duiring construction.
	 *	@param InNumberOfPrerequistitesOutstanding; the number of prerequisites outstanding. We actually add one to this internally to prevent the task from firing while we are setting up the task
	 **/
	void SetThreadToExecuteOn(ENamedThreads::Type InThreadToExecuteOn)
	{
		ThreadToExecuteOn = InThreadToExecuteOn;
		checkThreadGraph(LifeStage.Increment() == int32(LS_ThreadSet));
	}

	/** 
	 *	Indicates that the prerequisites are set up and that the task can be executed as soon as the prerequisites are finished.
	 *	@param NumAlreadyFinishedPrequistes; the number of prerequisites that have not been set up because those tasks had already completed.
	 **/
	void PrerequisitesComplete(ENamedThreads::Type CurrentThread, int32 NumAlreadyFinishedPrequistes)
	{
		checkThreadGraph(LifeStage.Increment() == int32(LS_PrequisitesSetup));
		if (NumberOfPrerequistitesOutstanding.Subtract(NumAlreadyFinishedPrequistes + 1) == NumAlreadyFinishedPrequistes + 1) // the +1 is for the "lock" we set up in the constructor
		{
			QueueTask(CurrentThread);
		}
	}
	/** destructor, just checks the life stage **/
	virtual ~FBaseGraphTask()
	{
		checkThreadGraph(LifeStage.Increment() == int32(LS_Deconstucted));
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/** Logs a task name that may contain invalid subsequents. Debug only. */
	static void CORE_API LogPossiblyInvalidSubsequentsTask(const TCHAR* TaskName);
#endif

	// Allocator for small tasks.
	enum
	{
		/** Total size in bytes for a small task that will use the custom allocator **/
		SMALL_TASK_SIZE = 256
	};
	/** Singleton to retrieve the small task allocator **/
	static CORE_API TLockFreeFixedSizeAllocator<SMALL_TASK_SIZE>& GetSmallTaskAllocator();

private:
	friend class FTaskThread;
	friend class FGraphEvent;
	friend class FTaskGraphImplementation;

	// Subclass API

	/** 
	 *	Virtual call to actually execute the task. This should also call the destructor and free any memory.
	 *	@param CurrentThread; provides the index of the thread we are running on. This is handy for submitting new taks.
	 **/
	virtual void ExecuteTask(TArray<FBaseGraphTask*>& NewTasks, ENamedThreads::Type CurrentThread)=0;

	/** 
	 *	Virtual call to retrieve the task name.
	 *	@return the name of the task. This returned string should be static memory, usable until the end of time.
	 **/
	virtual const TCHAR* GetTaskName()=0;


	// API called from other parts of the system

	/** 
	 *	An indication that a prerequisite has been completed. Reduces the number of prerequisites by one and if no prerequisites are outstanding, it queues the task for execution.
	 *	@param CurrentThread; provides the index of the thread we are running on. This is handy for submitting new taks. Can be ENamedThreads::AnyThread if the current thread is unknown.
	 **/
	void ConditionalQueueTask(ENamedThreads::Type CurrentThread)
	{
		if (NumberOfPrerequistitesOutstanding.Decrement()==0)
		{
			QueueTask(CurrentThread);
		}
	}

	/** 
	 *	Called by the system to execute this task after it has been removed from an internal queue.
	 *	Just checks the life stage and passes off to the virtual ExecuteTask method.
	 *	@param CurrentThread; provides the index of the thread we are running on. This is handy for submitting new taks.
	 **/
	void Execute(TArray<FBaseGraphTask*>& NewTasks, ENamedThreads::Type CurrentThread)
	{
		checkThreadGraph(LifeStage.Increment() == int32(LS_Executing));
		ExecuteTask(NewTasks, CurrentThread);
	}

	/** 
	 *	Called by the system to retrieve a task name for debugging or profilers. Just calls the virtual version.
	 *	@return the name of the task. This returned string should be static memory, usable until the end of time.
	 **/
	const TCHAR* GetDebugName()
	{
		return GetTaskName();
	}


	// Internal Use

	/** 
	 *	Queues the task for execution.
	 *	@param CurrentThread; provides the index of the thread we are running on. This is handy for submitting new taks. Can be ENamedThreads::AnyThread if the current thread is unknown.
	 **/
	void QueueTask(ENamedThreads::Type CurrentThreadIfKnown)
	{
		checkThreadGraph(LifeStage.Increment() == int32(LS_Queued));
		FTaskGraphInterface::Get().QueueTask(this, ThreadToExecuteOn, CurrentThreadIfKnown);
	}

	/**	Thread to execute on, can be ENamedThreads::AnyThread to execute on any unnamed thread **/
	ENamedThreads::Type			ThreadToExecuteOn;
	/**	Number of prerequisites outstanding. When this drops to zero, the thread is queued for execution.  **/
	FThreadSafeCounter			NumberOfPrerequistitesOutstanding; 


#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// Life stage verification
	// Tasks go through 8 steps, in order. In non-final builds, we track them with a thread safe counter and verify that the progression is correct.
	enum ELifeStage
	{
		LS_BaseContructed = 0,
		LS_Contructed,
		LS_ThreadSet,
		LS_PrequisitesSetup,
		LS_Queued,
		LS_Executing,
		LS_Deconstucted,
	};
	FThreadSafeCounter			LifeStage;

#endif
};

/** 
 *	A FGraphEvent is a list of tasks waiting for something.
 *	These tasks are call the subsequents.
 *	A graph event is a prerequisite for each of its subsequents.
 *	Graph events have a lifetime managed by reference counting.
 **/
class FGraphEvent 
{
public:
	/**
	 *	A factory method to create a graph event. 
	 *	@return a reference counted pointer to the new graph event. Note this should be stored in a FGraphEventRef or it will be immediately destroyed!
	**/
	static CORE_API FGraphEventRef CreateGraphEvent();

	/**
	 *	Attempts to a new subsequent task. If this event has already fired, false is returned and action must be taken to ensure that the task will still fire even though this event cannot be a prerequisite (because it is already finished).
	 *	@return true if the task was sucessfully set up as a subsequent. false if the event has already fired.
	**/
	bool AddSubsequent(class FBaseGraphTask* Task)
	{
		return SubsequentList.PushIfNotClosed(Task);
	}

	/**
	 *	Verification function to ensure that nobody was tried to add WaitUntil's outside of the context of execution
	**/
	void CheckDontCompleteUntilIsEmpty()
	{
		checkThreadGraph(!EventsToWaitFor.Num());
	}

	/**
	 *	Delay the firing of this event until the given event fires.
	 *	CAUTION: This is only legal while executing the task associated with this event.
	 *	@param EventToWaitFor event to wait for until we fire.
	**/
	void DontCompleteUntil(FGraphEventRef EventToWaitFor)
	{
		checkThreadGraph(!IsComplete()); // it is not legal to add a DontCompleteUntil after the event has been completed. Basically, this is only legal within a task function.
		new (EventsToWaitFor) FGraphEventRef(EventToWaitFor);
	}

	/**
	 *	"Complete" the event. This grabs the list of subsequents and atomically closes it. Then for each subsequent it reduces the number of prerequisites outstanding and if that drops to zero, the task is queued.
	 *	@param CurrentThreadIfKnown if the current thread is known, provide it here. Otherwise it will be determined via TLS if any task ends up being queued.
	**/
	CORE_API void DispatchSubsequents(ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread);
	CORE_API void DispatchSubsequents(TArray<FBaseGraphTask*>& NewTasks, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread);

	/**
	 *	Determine if the event has been completed. This can be used to poll for completion. 
	 *	@return true if this event has completed. 
	 *	CAUTION: If this returns false, the event could still end up completing before this function even returns. In other words, a false return does not mean that event is not yet completed!
	**/
	bool IsComplete() const
	{
		return SubsequentList.IsClosed();
	}
private:
	friend class TRefCountPtr<FGraphEvent>;
	friend class TLockFreeClassAllocator<FGraphEvent>;

	/**
	 *	Internal function to retrieve the graph event allocation singleton
	 *	@return a reference to the graph event allocation singleton
	**/
	static CORE_API TLockFreeClassAllocator<FGraphEvent>& GetAllocator();

	/** 
	 *	Internal function to call the destructor and recycle a graph event
	 *	@param ToRecycle; graph event to recycle
	**/
	static CORE_API void Recycle(FGraphEvent* ToRecycle);

	/**
	 *	Hidden Constructor
	**/
	FGraphEvent()
	{
	}

	/**
	 *	Destructor. Verifies we aren't destroying it prematurely. 
	**/
	~FGraphEvent();

	// Interface for TRefCountPtr

public:
	/** 
	 *	Increases the reference count 
	 *	@return the new reference count
	**/
	uint32 AddRef()
	{
		return ReferenceCount.Increment();
	}
	/** 
	 *	Decreases the reference count and destroys the graph event if it is zero.
	 *	@return the new reference count
	**/
	uint32 Release()
	{
		int32 RefCount = ReferenceCount.Decrement();
		checkThreadGraph(RefCount >= 0);
		if (RefCount == 0)
		{
			Recycle(this);
		}
		return RefCount;
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/**
	 * Checks if the pointers look 'ok'. Used for debugging (temporarily)
	 */
	bool CheckSubsequents() const
	{
		return SubsequentList.CheckPointers();
	}
#endif

private:

	/** Number of outstanding references to this graph event **/
	FThreadSafeCounter									ReferenceCount;
	/** Threadsafe list of subsequents for the event **/
	TClosableLockFreePointerList<FBaseGraphTask>		SubsequentList;
	/** List of events to wait for until firing. This is not thread safe as it is only legal to fill it in within the context of an executing task. **/
	FGraphEventArray									EventsToWaitFor;
};

/** 
 The user defined task type can take arguments to a constructor. These arguments (unfortunately) must not be references.
 The API required of TTask:

class FGenericTask
{
	TSomeType	SomeArgument;
public:
	FGenericTask(TSomeType InSomeArgument) // CAUTION!: Must not use references in the constructor args; use pointers instead if you need by reference
		: SomeArgument(InSomeArgument)
	{
		// Usually the constructor doesn't do anything except save the arguments for use in DoWork or GetDesiredThread.
	}
	~FGenericTask()
	{
		// you will be destroyed immediately after you execute. Might as well do cleanup in DoWork, but you could also use a destructor.
	}
	static const TCHAR* GetTaskName()
	{
		return TEXT("FGenericTask"); // this must return a pointer to memory usable until the end of time.
	}
	FORCEINLINE static TStatId GetStatId()
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FGenericTask, STATGROUP_TaskGraphTasks);
	}

	[static] ENamedThreads::Type GetDesiredThread()
	{
		return ENamedThreads::[named thread or AnyThread];
	}
	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		// The arguments are useful for setting up other tasks. 
		// Do work here, probably using SomeArgument.
		MyCompletionEvent->DontCompleteUntil(TGraphTask<FSomeChildTask>::CreateTask(NULL,CurrentThread).ConstructAndDispatchWhenReady());
	}
};
**/


/** 
 *	TGraphTask
 *	Embeds a user defined task, as exemplified above, for doing the work and provides the functionality for setting up and handling prerequisites and subsequents
 **/
template<typename TTask>
class TGraphTask : public FBaseGraphTask
{
public:
	/** 
	 *	This is a helper class returned from the factory. It constructs the embeded task with a set of arguments and sets the task up and makes it ready to execute.
	 *	The task may complete before these routines even return.
	 **/
	class FConstructor
	{
	public:
		/** Passthrough internal task constructor and dispatch. Note! Generally speaking references will not pass through; use pointers */
		FGraphEventRef ConstructAndDispatchWhenReady()
		{
			new ((void *)&Owner->TaskStorage) TTask();
			return Owner->Setup(Prerequisites, CurrentThreadIfKnown);
		}
		/** Passthrough internal task constructor and dispatch. Note! Generally speaking references will not pass through; use pointers */
		template<typename T>
		FGraphEventRef ConstructAndDispatchWhenReady(const T& Arg)
		{
			new ((void *)&Owner->TaskStorage) TTask(Arg);
			return Owner->Setup(Prerequisites, CurrentThreadIfKnown);
		}
		/** Passthrough internal task constructor and dispatch. Note! Generally speaking references will not pass through; use pointers */
		template<typename T1,typename T2>
		FGraphEventRef ConstructAndDispatchWhenReady(const T1& Arg1, const T2& Arg2)
		{
			new ((void *)&Owner->TaskStorage) TTask(Arg1,Arg2);
			return Owner->Setup(Prerequisites, CurrentThreadIfKnown);
		}
		/** Passthrough internal task constructor and dispatch. Note! Generally speaking references will not pass through; use pointers */
		template<typename T1,typename T2, typename T3>
		FGraphEventRef ConstructAndDispatchWhenReady(const T1& Arg1, const T2& Arg2, const T3& Arg3)
		{
			new ((void *)&Owner->TaskStorage) TTask(Arg1,Arg2,Arg3);
			return Owner->Setup(Prerequisites, CurrentThreadIfKnown);
		}
		/** Passthrough internal task constructor and dispatch. Note! Generally speaking references will not pass through; use pointers */
		template<typename T1,typename T2, typename T3, typename T4>
		FGraphEventRef ConstructAndDispatchWhenReady(const T1& Arg1, const T2& Arg2, const T3& Arg3, const T4& Arg4)
		{
			new ((void *)&Owner->TaskStorage) TTask(Arg1,Arg2,Arg3,Arg4);
			return Owner->Setup(Prerequisites, CurrentThreadIfKnown);
		}
		/** Passthrough internal task constructor and dispatch. Note! Generally speaking references will not pass through; use pointers */
		template<typename T1,typename T2, typename T3, typename T4, typename T5>
		FGraphEventRef ConstructAndDispatchWhenReady(const T1& Arg1, const T2& Arg2, const T3& Arg3, const T4& Arg4, const T5& Arg5)
		{
			new ((void *)&Owner->TaskStorage) TTask(Arg1,Arg2,Arg3,Arg4,Arg5);
			return Owner->Setup(Prerequisites, CurrentThreadIfKnown);
		}
	private:
		friend class TGraphTask;

		/** The task that created me to assist with embeded task construction and preparation. **/
		TGraphTask*						Owner;
		/** The list of prerequisites. **/
		const FGraphEventArray*			Prerequisites;
		/** If known, the current thread.  ENamedThreads::AnyThread is also fine, and if that is the value, we will determine the current thread, as needed, via TLS. **/
		ENamedThreads::Type				CurrentThreadIfKnown;

		/** Constructor, simply saves off the arguments for later use after we actually construct the embeded task. **/
		FConstructor(TGraphTask* InOwner, const FGraphEventArray* InPrerequisites, ENamedThreads::Type InCurrentThreadIfKnown)
			: Owner(InOwner)
			, Prerequisites(InPrerequisites)
			, CurrentThreadIfKnown(InCurrentThreadIfKnown)
		{
		}
		/** Prohibited copy construction **/
		FConstructor(const FConstructor& Other)
		{
			check(0);
		}
		/** Prohibited copy **/
		void operator=(const FConstructor& Other)
		{
			check(0);
		}
	};

	/** 
	 *	Factory to create a task and return the helper object to construct the embedded task and set it up for execution.
	 *	@param Prerequisites; the list of FGraphEvents that must be completed prior to this task executing.
	 *	@param CurrentThreadIfKnown; provides the index of the thread we are running on. Can be ENamedThreads::AnyThread if the current thread is unknown.
	 *	@return a temporary helper class which can be used to complete the process.
	**/
	static FConstructor CreateTask(const FGraphEventArray* Prerequisites = NULL, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread)
	{
		if (sizeof(TGraphTask) <= FBaseGraphTask::SMALL_TASK_SIZE)
		{
			void *Mem = FBaseGraphTask::GetSmallTaskAllocator().Allocate();
			return FConstructor(new (Mem) TGraphTask(TTask::GetSubsequentsMode() == ESubsequentsMode::FireAndForget ? NULL : FGraphEvent::CreateGraphEvent(), Prerequisites ? Prerequisites->Num() : 0), Prerequisites, CurrentThreadIfKnown);
		}
		return FConstructor(new TGraphTask(TTask::GetSubsequentsMode() == ESubsequentsMode::FireAndForget ? NULL : FGraphEvent::CreateGraphEvent(), Prerequisites ? Prerequisites->Num() : 0), Prerequisites, CurrentThreadIfKnown);
	}

private:
	friend class FConstructor;
	friend class FGraphEvent;

	// API derived from FBaseGraphTask

	/** 
	 *	Virtual call to actually execute the task. 
	 *	@param CurrentThread; provides the index of the thread we are running on. This is handy for submitting new taks.
	 *	Executes the embedded task. 
	 *  Destroys the embedded task.
	 *	Dispatches the subsequents.
	 *	Destroys myself.
	 **/
	virtual void ExecuteTask(TArray<FBaseGraphTask*>& NewTasks, ENamedThreads::Type CurrentThread)
	{
		checkThreadGraph(TaskConstructed);

		// Fire and forget mode must not have subsequents
		// Track subsequents mode must have subsequents
		checkThreadGraph(XOR(TTask::GetSubsequentsMode() == ESubsequentsMode::FireAndForget, IsValidRef(Subsequents)));

		if (TTask::GetSubsequentsMode() == ESubsequentsMode::TrackSubsequents)
		{
			Subsequents->CheckDontCompleteUntilIsEmpty(); // we can only add wait for tasks while executing the task
		}
		
		TTask& Task = *(TTask*)&TaskStorage;
		{
			FScopeCycleCounter Scope(TTask::GetStatId()); 
			Task.DoTask(CurrentThread, Subsequents);
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (TTask::GetSubsequentsMode() == ESubsequentsMode::TrackSubsequents)
			{
				if (!Subsequents->CheckSubsequents())
				{
					LogPossiblyInvalidSubsequentsTask(Task.GetTaskName());					
				}
			}	
#endif

			Task.~TTask();
		}
		
		TaskConstructed = false;

		if (TTask::GetSubsequentsMode() == ESubsequentsMode::TrackSubsequents)
		{
			FPlatformMisc::MemoryBarrier();
			Subsequents->DispatchSubsequents(NewTasks, CurrentThread);
		}

		if (sizeof(TGraphTask) <= FBaseGraphTask::SMALL_TASK_SIZE)
		{
			this->~TGraphTask();
			FBaseGraphTask::GetSmallTaskAllocator().Free(this);
		}
		else
		{
			delete this;
		}
	}
	/** 
	 *	Virtual call to retrieve the task name.
	 *	@return the name of the task. This returned string should be static memory, usable until the end of time.
	 **/
	virtual const TCHAR* GetTaskName()
	{
		checkThreadGraph(TaskConstructed);
		TTask& Task = *(TTask*)&TaskStorage;
		return Task.GetTaskName();
	}


	// Internals 

	/** 
	 *	Private constructor, constructs the base class with the number of prerequisites.
	 *	@param InSubsequents subsequents to associate with this task
	 *	@param NumberOfPrerequistitesOutstanding the number of prerequisites this task will have when it is built.
	**/
	TGraphTask(FGraphEventRef InSubsequents, int32 NumberOfPrerequistitesOutstanding)
		: FBaseGraphTask(NumberOfPrerequistitesOutstanding)
		, TaskConstructed(false)
		, Subsequents(InSubsequents)
	{
	}

	/** 
	 *	Private destructor, just checks that the task appears to be completed
	**/
	virtual ~TGraphTask()
	{
		checkThreadGraph(!TaskConstructed);
	}

	/** 
	 *	Call from FConstructor to complete the setup process
	 *	@param Prerequisites; the list of FGraphEvents that must be completed prior to this task executing.
	 *	@param CurrentThreadIfKnown; provides the index of the thread we are running on. Can be ENamedThreads::AnyThread if the current thread is unknown.
	 *	@return A new graph event which represents the completion of this task.
	 * 
	 *	Create the completed event
	 *	Set the thread to execute on based on the embedded task
	 *	Attempt to add myself as a subsequent to each prerequisite
	 *	Tell the base task that I am ready to start as soon as my prerequisites are ready.
	 **/
	FGraphEventRef Setup(const FGraphEventArray* Prerequisites = NULL, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread)
	{
		checkThreadGraph(!TaskConstructed);
		TaskConstructed = true;
		TTask& Task = *(TTask*)&TaskStorage;
		SetThreadToExecuteOn(Task.GetDesiredThread());
		int32 AlreadyCompletedPrerequisites = 0;
		if (Prerequisites)
		{
			for (int32 Index = 0; Index < Prerequisites->Num(); Index++)
			{
				check((*Prerequisites)[Index]);
				if (!(*Prerequisites)[Index]->AddSubsequent(this))
				{
					AlreadyCompletedPrerequisites++;
				}
			}
		}
		FGraphEventRef ReturnedEventRef = Subsequents; // very important so that this doesn't get destroyed before we return
		PrerequisitesComplete(CurrentThreadIfKnown, AlreadyCompletedPrerequisites); // can execute now if all of the prereqs are done
		return ReturnedEventRef;
	}

	/** 
	 *	Factory to create a gather task which assumes the given subsequent list from some other tasks.
	 *	This is used to support "WaitFor" during a task execution.
	 *	@param SubsequentsToAssume; subsequents to "assume" from an existing task
	 *	@param Prerequisites; the list of FGraphEvents that must be completed prior to dispatching my seubsequents.
	 *	@param CurrentThreadIfKnown; provides the index of the thread we are running on. Can be ENamedThreads::AnyThread if the current thread is unknown.
	**/
	static FConstructor CreateTask(FGraphEventRef SubsequentsToAssume, const FGraphEventArray* Prerequisites = NULL, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread)
	{
		if (sizeof(TGraphTask) <= FBaseGraphTask::SMALL_TASK_SIZE)
		{
			void *Mem = FBaseGraphTask::GetSmallTaskAllocator().Allocate();
			return FConstructor(new (Mem) TGraphTask(SubsequentsToAssume, Prerequisites ? Prerequisites->Num() : 0), Prerequisites, CurrentThreadIfKnown);
		}
		return FConstructor(new TGraphTask(SubsequentsToAssume, Prerequisites ? Prerequisites->Num() : 0), Prerequisites, CurrentThreadIfKnown);
	}

	/** An aligned bit of storage to hold the embedded task **/
	TAlignedBytes<sizeof(TTask),ALIGNOF(TTask)> TaskStorage;
	/** Used to sanity check the state of the object **/
	bool						TaskConstructed;
	/** A reference counted pointer to the completion event which lists the tasks that have me as a prerequisite. **/
	FGraphEventRef				Subsequents;
};


/** 
 *	FReturnGraphTask is a task used to return flow control from a named thread back to the original caller of ProcessThreadUntilRequestReturn
 **/
class FReturnGraphTask
{
public:

	/** 
	 *	Constructor
	 *	@param InThreadToReturnFrom; named thread to cause to return
	**/
	FReturnGraphTask(ENamedThreads::Type InThreadToReturnFrom)
		: ThreadToReturnFrom(InThreadToReturnFrom)
	{
		checkThreadGraph(ThreadToReturnFrom != ENamedThreads::AnyThread); // doesn't make any sense to return from any thread
	}
	/** 
	 *	Retrieve the task name.
	 *	@return the name of the task. This returned string should be static memory, usable until the end of time.
	 **/
	static const TCHAR* GetTaskName()
	{
		return TEXT("FReturnGraphTask");
	}
	FORCEINLINE static TStatId GetStatId()
	{
		return GET_STATID(STAT_FReturnGraphTask);
	}

	/** 
	 *	Retrieve the thread that this task wants to run on.
	 *	@return the thread that this task should run on.
	 **/
	ENamedThreads::Type GetDesiredThread()
	{
		return ThreadToReturnFrom;
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	/** 
	 *	Actually execute the task.
	 *	@param	CurrentThread; the thread we are running on
	 *	@param	MyCompletionGraphEvent; my completion event. Not always useful since at the end of DoWork, you can assume you are done and hence further tasks do not need you as a prerequisite. 
	 *	However, MyCompletionGraphEvent can be useful for passing to other routines or when it is handy to set up subsequents before you actually do work.
	 **/
	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		checkThreadGraph(ThreadToReturnFrom == CurrentThread); // we somehow are executing on the wrong thread.
		FTaskGraphInterface::Get().RequestReturn(ThreadToReturnFrom);
	}

private:
	/** Named thread that we want to cause to return to the caller of ProcessThreadUntilRequestReturn. **/
	ENamedThreads::Type ThreadToReturnFrom;
};

/** 
 *	FNullGraphTask is a task that does nothing. It can be used to "gather" tasks into one prerequisite.
 **/
class FNullGraphTask
{
public:
	/** 
	 *	Constructor
	 *	@param InTaskName; Name of the task (usually for debugging and tools)
	 *	@param InDesiredThread; Thread to run on, can be ENamedThreads::AnyThread
	**/
	FNullGraphTask(const TCHAR*	InTaskName, ENamedThreads::Type InDesiredThread = ENamedThreads::AnyThread)
		: DesiredThread(InDesiredThread)
		, TaskName(InTaskName)
	{
	}

	/** 
	 *	Retrieve the task name.
	 *	@return the name of the task. This returned string should be static memory, usable until the end of time.
	 **/
	const TCHAR* GetTaskName()
	{
		return TaskName;
	}
	FORCEINLINE static TStatId GetStatId()
	{
		return GET_STATID(STAT_FNullGraphTask);
	}

	/** 
	 *	Retrieve the thread that this task wants to run on.
	 *	@return the thread that this task should run on.
	 **/
	ENamedThreads::Type GetDesiredThread()
	{
		return DesiredThread;
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	/** 
	 *	Actually execute the task.
	 *	@param	CurrentThread; the thread we are running on
	 *	@param	MyCompletionGraphEvent; my completion event. Not always useful since at the end of DoWork, you can assume you are done and hence further tasks do not need you as a prerequisite. 
	 *	However, MyCompletionGraphEvent can be useful for passing to other routines or when it is handy to set up subsequents before you actually do work.
	 **/
	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
	}
private:
	/** Thread to run on, can be ENamedThreads::AnyThread **/
	ENamedThreads::Type DesiredThread;
	/** Name of the task **/
	const TCHAR*		TaskName;
};

/** 
 * FTriggerEventGraphTask is a task that triggers an event
 */
class FTriggerEventGraphTask
{
public:
	/** 
	 *	Constructor
	 *	@param InScopedEvent; Scoped event to fire
	**/
	FTriggerEventGraphTask(FEvent* InEvent)
		: Event(InEvent) 
	{
		check(Event);
	}

	static const TCHAR* GetTaskName()
	{
		return TEXT("FTriggerEventGraphTask");
	}
	FORCEINLINE static TStatId GetStatId()
	{
		return GET_STATID(STAT_FTriggerEventGraphTask);
	}

	ENamedThreads::Type GetDesiredThread()
	{
		return ENamedThreads::AnyThread;
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		Event->Trigger();
	}
private:
	FEvent* Event;
};

/** Task class for simple delegate based tasks. This is less efficient than a custom task, doesn't provide the task arguments, doesn't allow specification of the current thread, etc. **/
class FSimpleDelegateGraphTask
{
public:
	DECLARE_DELEGATE(FDelegate);

	/** Delegate to fire when task runs **/
	FDelegate							TaskDelegate;
	/** Name of the task, static memory **/
	const TCHAR*						TaskName;
	/** Thread to run delegate on **/
	const ENamedThreads::Type			DesiredThread;
public:
	ENamedThreads::Type GetDesiredThread()
	{
		return DesiredThread;
	}
	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	const TCHAR* GetTaskName()
	{
		return TaskName;
	}
	FORCEINLINE static TStatId GetStatId()
	{
		return GET_STATID(STAT_FSimpleDelegateGraphTask);
	}
	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		TaskDelegate.ExecuteIfBound();
	}
	/**
	  * Task constructor
	  * @param InTaskDelegate - delegate to execute when the prerequisites are complete
	  * @param InTaskName - Name of task for debugging or analysis tools, must be static memory valid forever
	  * @param InDesiredThread - Thread to run on
	**/
	FSimpleDelegateGraphTask(const FDelegate& InTaskDeletegate, const TCHAR* InTaskName, ENamedThreads::Type InDesiredThread)
		: TaskDelegate(InTaskDeletegate)
		, TaskName(InTaskName)
		, DesiredThread(InDesiredThread)
	{
	}

	/**
	  * Create a task and dispatch it when the prerequisites are complete
	  * @param InTaskDelegate - delegate to execute when the prerequisites are complete
	  * @param InTaskName - Name of task for debugging or analysis tools, must be static memory valid forever
	  * @param InPrerequisites - Handles for prerequisites for this task, can be NULL if there are no prerequisites
	  * @param InDesiredThread - Thread to run on
	  * @return completion handle for the new task 
	**/
	static FGraphEventRef CreateAndDispatchWhenReady(const FDelegate& InTaskDeletegate, const TCHAR* InTaskName, const FGraphEventArray* InPrerequisites = NULL, ENamedThreads::Type InDesiredThread = ENamedThreads::AnyThread)
	{
		return TGraphTask<FSimpleDelegateGraphTask>::CreateTask(InPrerequisites).ConstructAndDispatchWhenReady<const FDelegate&>(InTaskDeletegate, InTaskName, InDesiredThread);
	}
	/**
	  * Create a task and dispatch it when the prerequisites are complete
	  * @param InTaskDelegate - delegate to execute when the prerequisites are complete
	  * @param InTaskName - Name of task for debugging or analysis tools, must be static memory valid forever
	  * @param InDesiredThread - Thread to run on
	  * @param InPrerequisite - Handle for a single prerequisite for this task
	  * @return completion handle for the new task 
	**/
	static FGraphEventRef CreateAndDispatchWhenReady(const FDelegate& InTaskDeletegate, const TCHAR* InTaskName, const FGraphEventRef& InPrerequisite, ENamedThreads::Type InDesiredThread = ENamedThreads::AnyThread)
	{
		FGraphEventArray Prerequisites;
		check(InPrerequisite.GetReference());
		Prerequisites.Add(InPrerequisite);
		return CreateAndDispatchWhenReady(InTaskDeletegate, InTaskName, &Prerequisites, InDesiredThread);
	}
};

/** Task class for more full featured delegate based tasks. Still less efficient than a custom task, but provides all of the args **/
class FDelegateGraphTask
{
public:
	DECLARE_DELEGATE_TwoParams( FDelegate,ENamedThreads::Type, const  FGraphEventRef& );

	/** Delegate to fire when task runs **/
	FDelegate							TaskDelegate;
	/** Name of the task, static memory **/
	const TCHAR*						TaskName;
	/** Thread to run delegate on **/
	const ENamedThreads::Type			DesiredThread;
public:
	ENamedThreads::Type GetDesiredThread()
	{
		return DesiredThread;
	}
	static ESubsequentsMode::Type GetSubsequentsMode() 
	{ 
		return ESubsequentsMode::TrackSubsequents; 
	}
	const TCHAR* GetTaskName()
	{
		return TaskName;
	}
	FORCEINLINE static TStatId GetStatId()
	{
		return GET_STATID(STAT_FDelegateGraphTask);
	}
	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		TaskDelegate.ExecuteIfBound(CurrentThread, MyCompletionGraphEvent);
	}
	/**
	  * Task constructor
	  * @param InTaskDelegate - delegate to execute when the prerequisites are complete
	  * @param InTaskName - Name of task for debugging or analysis tools, must be static memory valid forever
	  * @param InDesiredThread - Thread to run on
	**/
	FDelegateGraphTask(const FDelegate& InTaskDeletegate, const TCHAR* InTaskName, ENamedThreads::Type InDesiredThread)
		: TaskDelegate(InTaskDeletegate)
		, TaskName(InTaskName)
		, DesiredThread(InDesiredThread)
	{
	}

	/**
	  * Create a task and dispatch it when the prerequisites are complete
	  * @param InTaskDelegate - delegate to execute when the prerequisites are complete
	  * @param InTaskName - Name of task for debugging or analysis tools, must be static memory valid forever
	  * @param InPrerequisites - Handles for prerequisites for this task, can be NULL if there are no prerequisites
	  * @param InCurrentThreadIfKnown - This thread, if known
	  * @param InDesiredThread - Thread to run on
	  * @return completion handle for the new task 
	**/
	static FGraphEventRef CreateAndDispatchWhenReady(const FDelegate& InTaskDeletegate, const TCHAR* InTaskName, const FGraphEventArray* InPrerequisites = NULL, ENamedThreads::Type InCurrentThreadIfKnown = ENamedThreads::AnyThread, ENamedThreads::Type InDesiredThread = ENamedThreads::AnyThread)
	{
		return TGraphTask<FDelegateGraphTask>::CreateTask(InPrerequisites, InCurrentThreadIfKnown).ConstructAndDispatchWhenReady<const FDelegate&>(InTaskDeletegate, InTaskName, InDesiredThread);
	}
	/**
	  * Create a task and dispatch it when the prerequisites are complete
	  * @param InTaskDelegate - delegate to execute when the prerequisites are complete
	  * @param InTaskName - Name of task for debugging or analysis tools, must be static memory valid forever
	  * @param InPrerequisite - Handle for a single prerequisite for this task
	  * @param InCurrentThreadIfKnown - This thread, if known
	  * @param InDesiredThread - Thread to run on
	  * @return completion handle for the new task 
	**/
	static FGraphEventRef CreateAndDispatchWhenReady(const FDelegate& InTaskDeletegate, const TCHAR* InTaskName, const FGraphEventRef& InPrerequisite, ENamedThreads::Type InCurrentThreadIfKnown = ENamedThreads::AnyThread, ENamedThreads::Type InDesiredThread = ENamedThreads::AnyThread)
	{
		FGraphEventArray Prerequisites;
		check(InPrerequisite.GetReference());
		Prerequisites.Add(InPrerequisite);
		return CreateAndDispatchWhenReady(InTaskDeletegate, InTaskName, &Prerequisites, InCurrentThreadIfKnown, InDesiredThread);
	}
};

/**
 * List of tasks that can be "joined" into one task which can be waited on or used as a prerequisite.
 * Note, these are FGraphEventRef's, but we manually manage the reference count instead of using a smart pointer
**/
class FCompletionList
{
	TLockFreePointerList<FGraphEvent>	Prerequisites;
public:
	/**
	 * Adds a task to the completion list, can be called from any thread
	 * @param TaskToAdd, task to add
	 */
	void Add(const FGraphEventRef& TaskToAdd)
	{
		FGraphEvent* Task = TaskToAdd.GetReference();
		checkSlow(Task);
		Task->AddRef(); // manually increasing the ref count
		Prerequisites.Push(Task);
	}
	/**
	  * Task function that waits until any newly added pending commands complete before it completes, forming a chain
	**/
	void ChainWaitForPrerequisites(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		// this is tricky...
		// we have waited for a set of pending tasks to execute. However, they may have added more pending tasks that also need to be waited for.
		FGraphEventRef PendingComplete = CreatePrerequisiteCompletionHandle();
		if (PendingComplete.GetReference())
		{
			MyCompletionGraphEvent->DontCompleteUntil(PendingComplete);
		}
	}
	/**
	 * Create a completion handle that represents the completion of all pending tasks
	 * This is complicated by the fact that some of the tasks we are waiting for might also add tasks
	 * So it is recursive and the task we call here uses DontCompleteUntil to build the chain
	 * this should always be called from the same thread.
	 * @return The task that when completed, indicates all tasks in the list are completed, including any tasks they added recursively. Will be a NULL reference if there are no tasks
	 */
	FGraphEventRef CreatePrerequisiteCompletionHandle()
	{
		FGraphEventRef CompleteHandle;
		TArray<FGraphEvent*> Pending;
		// grab all pending command completion handles
		Prerequisites.PopAll(Pending);
		if (Pending.Num())
		{
			FGraphEventArray PendingHandles;
			// convert the pointer list to a list of handles
			for (int32 Index = 0; Index < Pending.Num(); Index++)
			{
				new (PendingHandles) FGraphEventRef(Pending[Index]); 
				Pending[Index]->Release(); // remove the ref count we added when we added it to the lock free list
			}
			// start a new task that won't complete until all of these tasks have executed, plus any tasks that they create when they run
			CompleteHandle = FDelegateGraphTask::CreateAndDispatchWhenReady(FDelegateGraphTask::FDelegate::CreateRaw(this, &FCompletionList::ChainWaitForPrerequisites), TEXT("WaitOnCompletionList"), &PendingHandles);
		}
		return CompleteHandle;
	}
};


