// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	WeakObjectPtrTemplates.h: FWeakObjectPtr template specializations.
=============================================================================*/

#pragma once

#include "AutoPointer.h"

/***
 * 
 * FWeakObjectPtr is a weak pointer to a UObject. 
 * It can return NULL later if the object is garbage collected.
 * It has no impact on if the object is garbage collected or not.
 * It can't be directly used across a network.
 *
 * Most often it is used when you explicitly do NOT want to prevent something from being garbage collected.
 **/
struct FWeakObjectPtr;
struct FIndexToObject;

/***
* 
* TWeakObjectPtr is templatized version of the generic FWeakObjectPtr
* 
**/
template<class T=UObject, class TWeakObjectPtrBase=FWeakObjectPtr, class TUObjectArray=FIndexToObject>
struct TWeakObjectPtr : private TWeakObjectPtrBase
{
public:
	/** NULL constructor **/
	FORCEINLINE TWeakObjectPtr()
	{
	}
	/**  
	 * Construct from an object pointer
	 * @param Object object to create a weak pointer to
	**/
	FORCEINLINE TWeakObjectPtr(const T *Object) :
		TWeakObjectPtrBase((UObject*)Object)
	{
		// This static assert is in here rather than in the body of the class because we want
		// to be able to define TWeakObjectPtr<UUndefinedClass>.
		static_assert(CanConvertPointerFromTo<T, UObject>::Result, "TWeakObjectPtr can only be constructed with UObject types");
	}

	/**  
	 * Construct from another weak pointer
	 * @param Other weak pointer to copy from
	**/
	FORCEINLINE TWeakObjectPtr(const TWeakObjectPtr<T> &Other) :
		TWeakObjectPtrBase(Other)
	{
	}

	/**
	 * Reset the weak pointer back to the NULL state
	 */
	FORCEINLINE void Reset()
	{
		TWeakObjectPtrBase::Reset();
	}

	/**  
	 * Copy from an object pointer
	 * @param Object object to create a weak pointer to
	**/
	template<class U>
	FORCEINLINE void operator=(const U *Object)
	{
		const T* TempObject = Object;
		TWeakObjectPtrBase::operator=(TempObject);
	}

	/**  
	 * Construct from another weak pointer
	 * @param Other weak pointer to copy from
	**/
	FORCEINLINE void operator=(const TWeakObjectPtr<T> &Other)
	{
		TWeakObjectPtrBase::operator=(Other);
	}

	/**  
	 * Compare weak pointers for equality
	 * @param Other weak pointer to compare to
	 * Caution: Two weak pointers might not be equal to each other, but they both might return NULL.
	**/
	FORCEINLINE bool operator==(const TWeakObjectPtr<T> &Other) const
	{
		return TWeakObjectPtrBase::operator==(Other);
	}

	/**  
	 * Compare weak pointers for equality
	 * @param Other weak pointer to compare to
	 * Caution: Two weak pointers might not be equal to each other, but they both might return NULL.
	**/
	FORCEINLINE bool operator!=(const TWeakObjectPtr<T> &Other) const
	{
		return TWeakObjectPtrBase::operator!=(Other);
	}

	/**  
	 * Dereference the weak pointer
	 * @param bEvenIfPendingKill, if this is true, pendingkill objects are considered valid	
	 * @return NULL if this object is gone or the weak pointer was NULL, otherwise a valid uobject pointer
	**/
	FORCEINLINE T *Get(bool bEvenIfPendingKill = false) const
	{
		UObjectBase *Result = NULL;
		if (IsValid(true))
		{
			Result = static_cast<UObjectBase *>(TUObjectArray::IndexToObject(this->GetObjectIndex(), bEvenIfPendingKill ));
		}
		return (T *)Result;
	}

	/** Deferences the weak pointer even if its marked RF_Unreachable. This is needed to resolve weak pointers during GC (such as ::AddReferenceObjects) */
	FORCEINLINE T *GetEvenIfUnreachable() const
	{
		UObjectBase *Result = NULL;
		if (IsValid(true, true))
		{
			Result = static_cast<UObjectBase *>(TUObjectArray::IndexToObject(this->GetObjectIndex(), true));
		}
		return (T *)Result;
	}

	/**  
	 * Dereference the weak pointer
	**/
	FORCEINLINE T & operator*() const
	{
		return *Get();
	}

	/**  
	 * Dereference the weak pointer
	**/
	FORCEINLINE T * operator->() const
	{
		return Get();
	}

	/**  
	 * Test if this points to a live UObject
	 * @param bEvenIfPendingKill, if this is true, pendingkill objects are considered valid
	 * @param bThreadsafeTest, if true then function will just give you information whether referenced 
	 *							UObject is gone forever (@return false) or if it is still there (@return true, no object flags checked).
	 * @return true if Get() would return a valid non-null pointer
	**/
	FORCEINLINE bool IsValid(bool bEvenIfPendingKill = false, bool bThreadsafeTest = false) const
	{
		return TWeakObjectPtrBase::IsValid(bEvenIfPendingKill, bThreadsafeTest);
	}

	/**  
	 * Slightly different than !IsValid(), returns true if this used to point to a UObject, but doesn't any more and has not been assigned or reset in the mean time.
	 * @param bIncludingIfPendingKill, if this is true, pendingkill objects are considered stale
	 * @param bThreadsafeTest, set it to true when testing outside of Game Thread. Results in false if WeakObjPtr point to an existing object (no flags checked) 
	 * @return true if this used to point at a real object but no longer does.
	**/
	FORCEINLINE bool IsStale(bool bIncludingIfPendingKill = false, bool bThreadsafeTest = false) const
	{
		return TWeakObjectPtrBase::IsStale(bIncludingIfPendingKill, bThreadsafeTest);
	}

	/** Hash function. */
	FORCEINLINE friend uint32 GetTypeHash(const TWeakObjectPtr<T>& WeakObjectPtr)
	{
		return GetTypeHash(static_cast<const TWeakObjectPtrBase&>(WeakObjectPtr));
	}

	friend FArchive& operator<<( FArchive& Ar, TWeakObjectPtr<T>& WeakObjectPtr )
	{
		Ar << static_cast<TWeakObjectPtrBase&>(WeakObjectPtr);
		return Ar;
	}
};

template<class T> struct TIsPODType<TWeakObjectPtr<T> > { enum { Value = true }; };
template<class T> struct TIsZeroConstructType<TWeakObjectPtr<T> > { enum { Value = true }; };
template<class T> struct TIsWeakPointerType<TWeakObjectPtr<T> > { enum { Value = true }; };

/**
  * Automatic version of the weak object pointer
**/
template<class T> 
class TAutoWeakObjectPtr : public TAutoPointer<T, TWeakObjectPtr<T> >
{
public:
	/** NULL constructor **/
	FORCEINLINE TAutoWeakObjectPtr()
	{
	}
	/** Construct from a raw pointer **/
	FORCEINLINE TAutoWeakObjectPtr(const T *Target) 
		: TAutoPointer<T, TWeakObjectPtr<T> >(Target)
	{
	}
	/**  Construct from the base type **/
	FORCEINLINE TAutoWeakObjectPtr(const TWeakObjectPtr<T> &Other) 
		: TAutoPointer<T, TWeakObjectPtr<T> >(Other)
	{
	}
	/**  Construct from another auto pointer **/
	FORCEINLINE TAutoWeakObjectPtr(const TAutoWeakObjectPtr &Other) 
		: TAutoPointer<T, TWeakObjectPtr<T> >(Other)
	{
	}
};

template<class T> struct TIsPODType<TAutoWeakObjectPtr<T> > { enum { Value = true }; };
template<class T> struct TIsZeroConstructType<TAutoWeakObjectPtr<T> > { enum { Value = true }; };
template<class T> struct TIsWeakPointerType<TAutoWeakObjectPtr<T> > { enum { Value = true }; };

template<class T, class U>
void CopyFromWeakArray(T& Dest,const U& Src)
{
	Dest.Empty(Src.Num());
	for (int32 Index = 0; Index < Src.Num(); Index++)
	{
		if (Src(Index).Get())
		{
			Dest.AddItem(Src(Index).Get());
		}
	}
}


