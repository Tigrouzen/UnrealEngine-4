// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UObjectIterator.h: High level iterators for uobject
=============================================================================*/

#ifndef __UOBJECTITERATOR_H__
#define __UOBJECTITERATOR_H__


/**
 * Class for iterating through all objects, including class default objects, unreachable objects...all UObjects
 */
class FRawObjectIterator : public FUObjectArray::TIterator
{
public:
	/**
	 * Constructor
	 * @param	bOnlyGCedObjects	if true, skip all of the permanent objects
	 */
	FRawObjectIterator(bool bOnlyGCedObjects = false) :	
	  FUObjectArray::TIterator( GUObjectArray, bOnlyGCedObjects )
	{
	}
	/**
	 * Iterator dereference
	 * @return	the object pointer pointed at by the iterator
	 */
	FORCEINLINE UObject* operator*() const
	{
		// casting UObjectBase to UObject for clients
		return (UObject *)GetObject();
	}
	/**
	 * Iterator dereference
	 * @return	the object pointer pointed at by the iterator
	 */
	FORCEINLINE UObject* operator->() const
	{
		return (UObject *)GetObject();
	}
};



/**
 * Class for iterating through all objects, including class default objects.
 * Note that when Playing In Editor, this will find objects in the
 * editor as well as the PIE world, in an indeterminate order.
 */
class FObjectIterator : public FUObjectArray::TIterator
{
public:
	/**
	 * Constructor
	 *
	 * @param	InClass						return only object of the class or a subclass
	 * @param	bOnlyGCedObjects			if true, skip all of the permanent objects
	 * @param	AdditionalExclusionFlags	RF_* flags that should not be included in results
	 */
	FObjectIterator( UClass* InClass=UObject::StaticClass(), bool bOnlyGCedObjects = false, EObjectFlags AdditionalExclusionFlags = RF_NoFlags ) :
		FUObjectArray::TIterator( GUObjectArray, bOnlyGCedObjects ),
		Class( InClass ),
		ExclusionFlags(AdditionalExclusionFlags)
	{
		// We don't want to return any objects that are currently being background loaded unless we're using the object iterator during async loading.
		ExclusionFlags = EObjectFlags(ExclusionFlags | RF_Unreachable);
		if( !GIsAsyncLoading )
		{
			ExclusionFlags = EObjectFlags(ExclusionFlags | RF_AsyncLoading);
		}
		check(Class);

		do
		{
			UObject *Object = **this;
			if (!(Object->HasAnyFlags(ExclusionFlags) || (Class != UObject::StaticClass() && !Object->IsA(Class))))
			{
				break;
			}
		} while(Advance());
	}
	/**
	 * Iterator advance
	 */
	void operator++()
	{
		//@warning: behavior is partially mirrored in UnObjGC.cpp. Make sure to adapt code there as well if you make changes below.
		// verify that the async loading exclusion flag still matches (i.e. we didn't start/stop async loading within the scope of the iterator)
		checkSlow(GIsAsyncLoading || (ExclusionFlags & RF_AsyncLoading));

		while(Advance())
		{
			UObject *Object = **this;
			if (!(Object->HasAnyFlags(ExclusionFlags) || (Class != UObject::StaticClass() && !Object->IsA(Class))))
			{
				break;
			}
		}
	}
	/**
	 * Iterator dereference
	 * @return	the object pointer pointed at by the iterator
	 */
	FORCEINLINE UObject* operator*() const
	{
		// casting UObjectBase to UObject for clients
		return (UObject *)GetObject();
	}
	/**
	 * Iterator dereference
	 * @return	the object pointer pointed at by the iterator
	 */
	FORCEINLINE UObject* operator->() const
	{
		return (UObject *)GetObject();
	}
private:
	/** Class to restrict results to */
	UClass* Class;
protected:
	/** Flags that returned objects must not have */
	EObjectFlags ExclusionFlags;
};

/**
 * Class for iterating through all objects which inherit from a
 * specified base class.  Does not include any class default objects.
 * Note that when Playing In Editor, this will find objects in the
 * editor as well as the PIE world, in an indeterminate order.
 */
template< class T > class TObjectIterator
{
public:
	/**
	 * Constructor
	 */
	TObjectIterator(EObjectFlags AdditionalExclusionFlags = RF_ClassDefaultObject, bool bIncludeDerivedClasses = true)
		: Index(-1)
	{
		GetObjectsOfClass(T::StaticClass(), ObjectArray, bIncludeDerivedClasses, AdditionalExclusionFlags);
		Advance();
	}

	/**
	 * Iterator advance
	 */
	FORCEINLINE void operator++()
	{
		Advance();
	}

	SAFE_BOOL_OPERATORS(TObjectIterator<T>)

	/** Conversion to "bool" returning true if the iterator is valid. */
	FORCEINLINE_EXPLICIT_OPERATOR_BOOL() const
	{ 
		return ObjectArray.IsValidIndex(Index); 
	}
	/** Conversion to "bool" returning true if the iterator is valid. */
	FORCEINLINE bool operator !() const 
	{
		return !(bool)*this;
	}

	/**
	 * Iterator dereference
	 * @return	the object pointer pointed at by the iterator
	 */
	FORCEINLINE T* operator* () const
	{
		return (T*)GetObject();
	}
	/**
	 * Iterator dereference
	 * @return	the object pointer pointed at by the iterator
	 */
	FORCEINLINE T* operator-> () const
	{
		return (T*)GetObject();
	}

protected:
	/**
	 * Dereferences the iterator with an ordinary name for clarity in derived classes
	 *
	 * @return	the UObject at the iterator
	 */
	FORCEINLINE UObject* GetObject() const 
	{ 
		return ObjectArray[Index];
	}
	
	/**
	 * Iterator advance with ordinary name for clarity in subclasses
	 * @return	true if the iterator points to a valid object, false if iteration is complete
	 */
	FORCEINLINE bool Advance()
	{
		//@todo UE4 check this for LHS on Index on consoles
		while(++Index < ObjectArray.Num())
		{
			if (GetObject())
			{
				return true;
			}
		}
		return false;
	}

protected:
	/** Results from the GetObjectsOfClass query */
	TArray<UObject*> ObjectArray;
	/** index of the current element in the object array */
	int32 Index;
};

/** specialization for T == UObject that does not call IsA() unnecessarily */
template<> class TObjectIterator<UObject> : public FObjectIterator
{
public:
	/**
	 * Constructor
	 *
	 * @param	bOnlyGCedObjects			if true, skip all of the permanent objects
	 */
	TObjectIterator(bool bOnlyGCedObjects = false) :
		FObjectIterator( UObject::StaticClass(),bOnlyGCedObjects, RF_ClassDefaultObject )

	{
		// there will be one unnecessary IsA in the base class constructor
	}

	/**
	 * Iterator advance
	 */
	void operator++()
	{
		// verify that the async loading exclusion flag still matches (i.e. we didn't start/stop async loading within the scope of the iterator)
		checkSlow(GIsAsyncLoading || (ExclusionFlags & RF_AsyncLoading));
		while(Advance())
		{
			if (!(*this)->HasAnyFlags(ExclusionFlags))
			{
				break;
			}
		}
	}
};


#endif	// __UOBJECTITERATOR_H__
