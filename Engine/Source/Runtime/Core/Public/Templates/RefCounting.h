// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RefCounting.h: Reference counting definitions.
=============================================================================*/

#pragma once

/** A virtual interface for ref counted objects to implement. */
class IRefCountedObject
{
public:
	virtual uint32 AddRef() const = 0;
	virtual uint32 Release() const = 0;
	virtual uint32 GetRefCount() const = 0;
};

/**
 * The base class of reference counted objects.
 */
class CORE_API FRefCountedObject
{
public:
	FRefCountedObject(): NumRefs(0) {}
	virtual ~FRefCountedObject() { check(!NumRefs); }
	uint32 AddRef() const
	{
		return uint32(++NumRefs);
	}
	uint32 Release() const
	{
		uint32 Refs = uint32(--NumRefs);
		if(Refs == 0)
		{
			delete this;
		}
		return Refs;
	}
	uint32 GetRefCount() const
	{
		return uint32(NumRefs);
	}
private:
	mutable int32 NumRefs;
};

/**
 * A smart pointer to an object which implements AddRef/Release.
 */
template<typename ReferencedType>
class TRefCountPtr
{
	typedef ReferencedType* ReferenceType;
public:

	TRefCountPtr():
		Reference(NULL)
	{}

	TRefCountPtr(ReferencedType* InReference,bool bAddRef = true)
	{
		Reference = InReference;
		if(Reference && bAddRef)
		{
			Reference->AddRef();
		}
	}

	TRefCountPtr(const TRefCountPtr& Copy)
	{
		Reference = Copy.Reference;
		if(Reference)
		{
			Reference->AddRef();
		}
	}

	~TRefCountPtr()
	{
		if(Reference)
		{
			Reference->Release();
		}
	}

	TRefCountPtr& operator=(ReferencedType* InReference)
	{
		// Call AddRef before Release, in case the new reference is the same as the old reference.
		ReferencedType* OldReference = Reference;
		Reference = InReference;
		if(Reference)
		{
			Reference->AddRef();
		}
		if(OldReference)
		{
			OldReference->Release();
		}
		return *this;
	}

	TRefCountPtr& operator=(const TRefCountPtr& InPtr)
	{
		return *this = InPtr.Reference;
	}

	bool operator==(const TRefCountPtr& Other) const
	{
		return Reference == Other.Reference;
	}

	ReferencedType* operator->() const
	{
		return Reference;
	}

	operator ReferenceType() const
	{
		return Reference;
	}

	ReferencedType** GetInitReference()
	{
		*this = NULL;
		return &Reference;
	}

	ReferencedType* GetReference() const
	{
		return Reference;
	}

	friend bool IsValidRef(const TRefCountPtr& InReference)
	{
		return InReference.Reference != NULL;
	}

	void SafeRelease()
	{
		*this = NULL;
	}

	uint32 GetRefCount()
	{
		if(Reference)
		{
			Reference->AddRef();
			return Reference->Release();
		}
		else
		{
			return 0;
		}
	}

	void Swap(TRefCountPtr& InPtr) // this does not change the reference count, and so is faster
	{
		ReferencedType* OldReference = Reference;
		Reference = InPtr.Reference;
		InPtr.Reference = OldReference;
	}

	friend FArchive& operator<<(FArchive& Ar,TRefCountPtr& Ptr)
	{
		ReferenceType PtrReference = Ptr.Reference;
		Ar << PtrReference;
		if(Ar.IsLoading())
		{
			Ptr = PtrReference;
		}
		return Ar;
	}

private:
	ReferencedType* Reference;
};
