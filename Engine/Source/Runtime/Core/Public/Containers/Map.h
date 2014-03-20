// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Map.h: Dynamic map definitions.
=============================================================================*/

#pragma once

#include "Core.h"
#include "Array.h"
#include "Set.h"

#define ExchangeB(A,B) {bool T=A; A=B; B=T;}

/** An initializer type for pairs that's passed to the pair set when adding a new pair. */
template <typename KeyInitType, typename ValueInitType>
class TPairInitializer
{
public:
	typename TRValueToLValueReference<KeyInitType  >::Type Key;
	typename TRValueToLValueReference<ValueInitType>::Type Value;

	/** Initialization constructor. */
	FORCEINLINE TPairInitializer(KeyInitType InKey,ValueInitType InValue)
		: Key  (InKey  )
		, Value(InValue)
	{
	}
};

/** An initializer type for keys that's passed to the pair set when adding a new key. */
template <typename KeyInitType>
class TKeyInitializer
{
public:
	typename TRValueToLValueReference<KeyInitType>::Type Key;

	/** Initialization constructor. */
	FORCEINLINE explicit TKeyInitializer(KeyInitType InKey)
		: Key(InKey)
	{
	}
};

/** A key-value pair in the map. */
template<typename KeyType,typename ValueType>
class TPair
{
public:
	typedef typename TTypeTraits<KeyType  >::ConstInitType KeyInitType;
	typedef typename TTypeTraits<ValueType>::ConstInitType ValueInitType;

	KeyType   Key;
	ValueType Value;

	/** Default constructor. */
	FORCEINLINE TPair()
	{}

	/** Initialization constructor. */
	template <typename InitKeyType, typename InitValueType>
	FORCEINLINE TPair(const TPairInitializer<InitKeyType, InitValueType>& InInitializer)
	:	Key  (StaticCast<InitKeyType  >(InInitializer.Key  ))
	,	Value(StaticCast<InitValueType>(InInitializer.Value))
	{
		// The seemingly-pointless casts above are to enforce a move (i.e. equivalent to using MoveTemp) when
		// the initializers are themselves rvalue references.
	}

	/** Key initialization constructor. */
	template <typename InitKeyType>
	FORCEINLINE explicit TPair(const TKeyInitializer<InitKeyType>& InInitializer)
		: Key  (StaticCast<InitKeyType>(InInitializer.Key))
		, Value()
	{
		// The seemingly-pointless cast above is to enforce a move (i.e. equivalent to using MoveTemp) when
		// the initializer is itself an rvalue reference.
	}

	/** Serializer. */
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar,TPair& Pair)
	{
		return Ar << Pair.Key << Pair.Value;
	}

	// Comparison operators
	FORCEINLINE bool operator==(const TPair& Other) const
	{
		return Key == Other.Key && Value == Other.Value;
	}

	FORCEINLINE bool operator!=(const TPair& Other) const
	{
		return Key != Other.Key || Value != Other.Value;
	}

	/** Implicit conversion to pair initializer. */
	FORCEINLINE operator TPairInitializer<KeyInitType, ValueInitType>() const
	{
		return TPairInitializer<KeyInitType, ValueInitType>(Key,Value);
	}
};

/** Defines how the map's pairs are hashed. */
template<typename KeyType, typename ValueType, bool bInAllowDuplicateKeys>
struct TDefaultMapKeyFuncs : BaseKeyFuncs<TPair<KeyType,ValueType>,KeyType,bInAllowDuplicateKeys>
{
	typedef typename TTypeTraits<KeyType>::ConstPointerType KeyInitType;
	typedef const TPairInitializer<typename TTypeTraits<KeyType>::ConstInitType, typename TTypeTraits<ValueType>::ConstInitType>& ElementInitType;

	static FORCEINLINE KeyInitType GetSetKey(ElementInitType Element)
	{
		return Element.Key;
	}
	static FORCEINLINE bool Matches(KeyInitType A,KeyInitType B)
	{
		return A == B;
	}
	static FORCEINLINE uint32 GetKeyHash(KeyInitType Key)
	{
		return GetTypeHash(Key);
	}
};

/** 
 * The base class of maps from keys to values.  Implemented using a TSet of key-value pairs with a custom KeyFuncs, 
 * with the same O(1) addition, removal, and finding. 
 **/
template<typename KeyType,typename ValueType,bool bInAllowDuplicateKeys,typename SetAllocator = FDefaultSetAllocator,typename KeyFuncs = TDefaultMapKeyFuncs<KeyType,ValueType,bInAllowDuplicateKeys> >
class TMapBase
{
public:
	typedef typename TTypeTraits<KeyType  >::ConstPointerType KeyConstPointerType;
	typedef typename TTypeTraits<KeyType  >::ConstInitType    KeyInitType;
	typedef typename TTypeTraits<ValueType>::ConstInitType    ValueInitType;

protected:
	typedef TPair<KeyType, ValueType> PairType;

public:
#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS

	TMapBase() = default;
	TMapBase(TMapBase&&) = default;
	TMapBase(const TMapBase&) = default;
	TMapBase& operator=(TMapBase&&) = default;
	TMapBase& operator=(const TMapBase&) = default;

#else

	FORCEINLINE TMapBase() {}
	FORCEINLINE TMapBase(      TMapBase&& Other) : Pairs(MoveTemp(Other.Pairs)) {}
	FORCEINLINE TMapBase(const TMapBase&  Other) : Pairs(         Other.Pairs ) {}
	FORCEINLINE TMapBase& operator=(      TMapBase&& Other) { Pairs = MoveTemp(Other.Pairs); return *this; }
	FORCEINLINE TMapBase& operator=(const TMapBase&  Other) { Pairs =          Other.Pairs ; return *this; }

#endif

	// Legacy comparison operators.  Note that these also test whether the map's key-value pairs were added in the same order!
	friend bool LegacyCompareEqual(const TMapBase& A,const TMapBase& B)
	{
		return LegacyCompareEqual(A.Pairs,B.Pairs);
	}
	friend bool LegacyCompareNotEqual(const TMapBase& A,const TMapBase& B)
	{
		return LegacyCompareNotEqual(A.Pairs,B.Pairs);
	}

	/**
	 * Removes all elements from the map, potentially leaving space allocated for an expected number of elements about to be added.
	 * @param ExpectedNumElements - The number of elements about to be added to the set.
	 */
	FORCEINLINE void Empty(int32 ExpectedNumElements = 0)
	{
		Pairs.Empty(ExpectedNumElements);
	}

    /** Efficiently empties out the map but preserves all allocations and capacities */
    FORCEINLINE void Reset()
    {
        Empty(Num());
    }

	/** Shrinks the pair set to avoid slack. */
	FORCEINLINE void Shrink()
	{
		Pairs.Shrink();
	}

	/** Compacts the pair set to remove holes */
	FORCEINLINE void Compact()
	{
		Pairs.Compact();
	}

	/** @return The number of elements in the map. */
	FORCEINLINE int32 Num() const
	{
		return Pairs.Num();
	}

	/**
	 * Returns the unique keys contained within this map
	 * @param	OutKeys	- Upon return, contains the set of unique keys in this map.
	 * @return The number of unique keys in the map.
	 */
	int32 GetKeys(TArray<KeyType>& OutKeys) const
	{
		TSet<KeyType> VisitedKeys;
		for(typename PairSetType::TConstIterator It(Pairs);It;++It)
		{
			if ( !VisitedKeys.Contains(It->Key) )
			{
				OutKeys.Add(It->Key);
				VisitedKeys.Add(It->Key);
			}
		}
		return OutKeys.Num();
	}

	/** 
	 * Helper function to return the amount of memory allocated by this container 
	 * @return number of bytes allocated by this container
	 */
	FORCEINLINE uint32 GetAllocatedSize() const
	{
		return Pairs.GetAllocatedSize();
	}

	/** Tracks the container's memory use through an archive. */
	FORCEINLINE void CountBytes(FArchive& Ar)
	{
		Pairs.CountBytes(Ar);
	}

	/**
	 * Sets the value associated with a key.
	 * If the key is already associated with any values, the existing values are replaced by the new value.
	 *
	 * @param InKey - The key to associate the value with.
	 * @param InValue - The value to associate with the key.
	 * @return A reference to the value as stored in the map.  The reference is only valid until the next change to any key in the map.
	 */
	FORCEINLINE ValueType& Add(const KeyType&  InKey, const ValueType&  InValue) { return Emplace(         InKey ,          InValue ); }
	FORCEINLINE ValueType& Add(const KeyType&  InKey,       ValueType&& InValue) { return Emplace(         InKey , MoveTemp(InValue)); }
	FORCEINLINE ValueType& Add(      KeyType&& InKey, const ValueType&  InValue) { return Emplace(MoveTemp(InKey),          InValue ); }
	FORCEINLINE ValueType& Add(      KeyType&& InKey,       ValueType&& InValue) { return Emplace(MoveTemp(InKey), MoveTemp(InValue)); }

	/**
	 * Sets a default value associated with a key.
	 * If the key is already associated with any values, the existing values are replaced by the new value.
	 *
	 * @param InKey - The key to associate the value with.
	 * @return A reference to the value as stored in the map.  The reference is only valid until the next change to any key in the map.
	 */
	FORCEINLINE ValueType& Add(const KeyType&  InKey) { return Emplace(         InKey ); }
	FORCEINLINE ValueType& Add(      KeyType&& InKey) { return Emplace(MoveTemp(InKey)); }

	/**
	 * Sets the value associated with a key.
	 * If the key is already associated with any values, the existing values are replaced by the new value.
	 *
	 * @param InKey - The key to associate the value with.
	 * @param InValue - The value to associate with the key.
	 * @return A reference to the value as stored in the map.  The reference is only valid until the next change to any key in the map.
	 */
	template <typename InitKeyType, typename InitValueType>
	ValueType& Emplace(InitKeyType&& InKey, InitValueType&& InValue)
	{
		// Remove existing values associated with the specified key.
		// This is only necessary if the TSet allows duplicate keys; otherwise TSet::Add replaces the existing key-value pair.
		if(KeyFuncs::bAllowDuplicateKeys)
		{
			for(typename PairSetType::TKeyIterator It(Pairs,InKey);It;++It)
			{
				It.RemoveCurrent();
			}
		}

		// Add the key-value pair to the set.  TSet::Add will replace any existing key-value pair that has the same key.
		const FSetElementId PairId = Pairs.Emplace(TPairInitializer<InitKeyType&&, InitValueType&&>(Forward<InitKeyType>(InKey), Forward<InitValueType>(InValue)));

		return Pairs(PairId).Value;
	}

	/**
	 * Sets a default value associated with a key.
	 * If the key is already associated with any values, the existing values are replaced by the new value.
	 *
	 * @param InKey - The key to associate the value with.
	 * @return A reference to the value as stored in the map.  The reference is only valid until the next change to any key in the map.
	 */
	template <typename InitKeyType>
	ValueType& Emplace(InitKeyType&& InKey)
	{
		// Remove existing values associated with the specified key.
		// This is only necessary if the TSet allows duplicate keys; otherwise TSet::Add replaces the existing key-value pair.
		if(KeyFuncs::bAllowDuplicateKeys)
		{
			for(typename PairSetType::TKeyIterator It(Pairs,InKey);It;++It)
			{
				It.RemoveCurrent();
			}
		}

		// Add the key-value pair to the set.  TSet::Add will replace any existing key-value pair that has the same key.
		const FSetElementId PairId = Pairs.Emplace(TKeyInitializer<InitKeyType&&>(Forward<InitKeyType>(InKey)));

		return Pairs(PairId).Value;
	}

	/**
	 * Removes all value associations for a key.
	 * @param InKey - The key to remove associated values for.
	 * @return The number of values that were associated with the key.
	 */
	FORCEINLINE int32 Remove(KeyConstPointerType InKey)
	{
		const int32 NumRemovedPairs = Pairs.Remove(InKey);
		return NumRemovedPairs;
	}

	/**
	 * Returns the key associated with the specified value.  The time taken is O(N) in the number of pairs.
	 * @param	Value - The value to search for
	 * @return	A pointer to the key associated with the specified value, or NULL if the value isn't contained in this map.  The pointer
	 *			is only valid until the next change to any key in the map.
	 */
	const KeyType* FindKey(ValueInitType Value) const
	{
		for(typename PairSetType::TConstIterator PairIt(Pairs);PairIt;++PairIt)
		{
			if(PairIt->Value == Value)
			{
				return &PairIt->Key;
			}
		}
		return NULL;
	}

	/**
	 * Returns the value associated with a specified key.
	 * @param	Key - The key to search for.
	 * @return	A pointer to the value associated with the specified key, or NULL if the key isn't contained in this map.  The pointer
	 *			is only valid until the next change to any key in the map.
	 */
	FORCEINLINE ValueType* Find(KeyConstPointerType Key)
	{
		if (auto* Pair = Pairs.Find(Key))
		{
			return &Pair->Value;
		}

		return NULL;
	}
	FORCEINLINE const ValueType* Find(KeyConstPointerType Key) const
	{
		return const_cast<TMapBase*>(this)->Find(Key);
	}

private:
	/**
	 * Returns the value associated with a specified key, or if none exists, 
	 * adds a value using the default constructor.
	 * @param	Key - The key to search for.
	 * @return	A reference to the value associated with the specified key.
	 */
	template <typename ArgType>
	FORCEINLINE ValueType& FindOrAddImpl(ArgType&& Arg)
	{
		if (auto* Pair = Pairs.Find(Arg))
			return Pair->Value;

		return Add(Forward<ArgType>(Arg));
	}

public:
	/**
	 * Returns the value associated with a specified key, or if none exists, 
	 * adds a value using the default constructor.
	 * @param	Key - The key to search for.
	 * @return	A reference to the value associated with the specified key.
	 */
	FORCEINLINE ValueType& FindOrAdd(const KeyType&  Key) { return FindOrAddImpl(         Key ); }
	FORCEINLINE ValueType& FindOrAdd(      KeyType&& Key) { return FindOrAddImpl(MoveTemp(Key)); }

	/**
	 * Returns the value associated with a specified key, or if none exists, 
	 * adds a value using the key as the constructor parameter.
	 * @param	Key - The key to search for.
	 * @return	A reference to the value associated with the specified key.
	 */
	//@todo UE4 merge - this prevents FConfigCacheIni from compiling
	/*ValueType& FindOrAddKey(KeyInitType Key)
	{
		TPair* Pair = Pairs.Find(Key);
		if( Pair )
		{
			return Pair->Value;
		}
		else
		{
			return Set(Key, ValueType(Key));
		}
	}*/

	/**
	 * Returns a reference to the value associated with a specified key.
	 * @param	Key - The key to search for.
	 * @return	The value associated with the specified key, or triggers an assertion if the key does not exist.
	 */
	FORCEINLINE const ValueType& FindChecked(KeyConstPointerType Key) const
	{
		const auto* Pair = Pairs.Find(Key);
		check( Pair != NULL );
		return Pair->Value;
	}

	/**
	 * Returns a reference to the alue associated with a specified key.
	 * @param	Key - The key to search for.
	 * @return	The value associated with the specified key, or triggers an assertion if the key does not exist.
	 */
	FORCEINLINE ValueType& FindChecked(KeyConstPointerType Key)
	{
		auto* Pair = Pairs.Find(Key);
		check( Pair != NULL );
		return Pair->Value;
	}

	/**
	 * Returns the value associated with a specified key.
	 * @param	Key - The key to search for.
	 * @return	The value associated with the specified key, or the default value for the ValueType if the key isn't contained in this map.
	 */
	FORCEINLINE ValueType FindRef(KeyConstPointerType Key) const
	{
		if (const auto* Pair = Pairs.Find(Key))
		{
			return Pair->Value;
		}

		return ValueType();
	}

	/**
	 * Checks if map contains the specified key.
	 * @return Key - The key to check for.
	 * @return true if the map contains the key.
	 */
	FORCEINLINE bool Contains(KeyConstPointerType Key) const
	{
		return Pairs.Contains(Key);
	}

	/**
	 * Generates an array from the keys in this map.
	 */
	void GenerateKeyArray(TArray<KeyType>& OutArray) const
	{
		OutArray.Empty(Pairs.Num());
		for(typename PairSetType::TConstIterator PairIt(Pairs);PairIt;++PairIt)
		{
			new(OutArray) KeyType(PairIt->Key);
		}
	}

	/**
	 * Generates an array from the values in this map.
	 */
	void GenerateValueArray(TArray<ValueType>& OutArray) const
	{
		OutArray.Empty(Pairs.Num());
		for(typename PairSetType::TConstIterator PairIt(Pairs);PairIt;++PairIt)
		{
			new(OutArray) ValueType(PairIt->Value);
		}
	}

	/** Serializer. */
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar,TMapBase& Map)
	{
		return Ar << Map.Pairs;
	}

	/**
	 * Describes the map's contents through an output device.
	 * @param Ar - The output device to describe the map's contents through.
	 */
	void Dump(FOutputDevice& Ar)
	{
		Pairs.Dump(Ar);
	}

protected:

	typedef TSet<PairType,KeyFuncs,SetAllocator> PairSetType;

	/** The base of TMapBase iterators. */
	template<bool bConst>
	class TBaseIterator
	{
	public:
		typedef typename TChooseClass<bConst,typename PairSetType::TConstIterator,typename PairSetType::TIterator>::Result PairItType;
	private:
		typedef typename TChooseClass<bConst,const TMapBase,TMapBase>::Result MapType;
		typedef typename TChooseClass<bConst,const KeyType,KeyType>::Result ItKeyType;
		typedef typename TChooseClass<bConst,const ValueType,ValueType>::Result ItValueType;
		typedef typename TChooseClass<bConst,const typename PairSetType::ElementType, typename PairSetType::ElementType>::Result PairType;

	protected:
		FORCEINLINE TBaseIterator(const PairItType& InElementIt)
			: PairIt(InElementIt)
		{
		}

	public:
		FORCEINLINE TBaseIterator& operator++()
		{
			++PairIt;
			return *this;
		}

		/** conversion to "bool" returning true if the iterator is valid. */
		FORCEINLINE_EXPLICIT_OPERATOR_BOOL() const
		{ 
			return !!PairIt; 
		}
		/** inverse of the "bool" operator */
		FORCEINLINE bool operator !() const 
		{
			return !(bool)*this;
		}

		FORCEINLINE friend bool operator==(const TBaseIterator& Lhs, const TBaseIterator& Rhs) { return Lhs.PairIt == Rhs.PairIt; }
		FORCEINLINE friend bool operator!=(const TBaseIterator& Lhs, const TBaseIterator& Rhs) { return Lhs.PairIt != Rhs.PairIt; }

		FORCEINLINE ItKeyType&   Key()   const { return PairIt->Key; }
		FORCEINLINE ItValueType& Value() const { return PairIt->Value; }

		FORCEINLINE PairType& operator* () const { return  *PairIt; }
		FORCEINLINE PairType* operator->() const { return &*PairIt; }

	protected:
		PairItType PairIt;
	};

	/** The base type of iterators that iterate over the values associated with a specified key. */
	template<bool bConst>
	class TBaseKeyIterator
	{
	private:
		typedef typename TChooseClass<bConst,typename PairSetType::TConstKeyIterator,typename PairSetType::TKeyIterator>::Result SetItType;
		typedef typename TChooseClass<bConst,const KeyType,KeyType>::Result ItKeyType;
		typedef typename TChooseClass<bConst,const ValueType,ValueType>::Result ItValueType;

	public:
		/** Initialization constructor. */
		FORCEINLINE TBaseKeyIterator(const SetItType& InSetIt)
			: SetIt(InSetIt)
		{
		}

		FORCEINLINE TBaseKeyIterator& operator++()
		{
			++SetIt;
			return *this;
		}

		SAFE_BOOL_OPERATORS(TBaseKeyIterator<bConst>)

		/** conversion to "bool" returning true if the iterator is valid. */
		FORCEINLINE_EXPLICIT_OPERATOR_BOOL() const
		{ 
			return !!SetIt; 
		}
		/** inverse of the "bool" operator */
		FORCEINLINE bool operator !() const 
		{
			return !(bool)*this;
		}

		FORCEINLINE ItKeyType&   Key  () const { return SetIt->Key; }
		FORCEINLINE ItValueType& Value() const { return SetIt->Value; }

	protected:
		SetItType SetIt;
	};

	/** A set of the key-value pairs in the map. */
	PairSetType Pairs;

public:

	/** Map iterator. */
	class TIterator : public TBaseIterator<false>
	{
	public:

		/** Initialization constructor. */
		FORCEINLINE TIterator(TMapBase& InMap, bool bInRequiresRehashOnRemoval = false)
			: TBaseIterator<false>    (begin(InMap.Pairs))
			, Map                     (InMap)
			, bElementsHaveBeenRemoved(false)
			, bRequiresRehashOnRemoval(bInRequiresRehashOnRemoval)
		{
		}

		/** Initialization constructor. */
		FORCEINLINE TIterator(TMapBase& InMap, const typename TBaseIterator<false>::PairItType& InPairIt)
			: TBaseIterator<false>    (InPairIt)
			, Map                     (InMap)
			, bElementsHaveBeenRemoved(false)
			, bRequiresRehashOnRemoval(false)
		{
		}

		/** Destructor. */
		FORCEINLINE ~TIterator()
		{
			if(bElementsHaveBeenRemoved && bRequiresRehashOnRemoval)
			{
				Map.Pairs.Relax();
			}
		}

		/** Removes the current pair from the map. */
		FORCEINLINE void RemoveCurrent()
		{
			TBaseIterator<false>::PairIt.RemoveCurrent();
			bElementsHaveBeenRemoved = true;
		}

	private:
		TMapBase& Map;
		bool      bElementsHaveBeenRemoved;
		bool      bRequiresRehashOnRemoval;
	};

	/** Const map iterator. */
	class TConstIterator : public TBaseIterator<true>
	{
	public:
		FORCEINLINE TConstIterator(const TMapBase& InMap)
			: TBaseIterator<true>(begin(InMap.Pairs))
		{
		}

		FORCEINLINE TConstIterator(const typename TBaseIterator<true>::PairItType& InPairIt)
			: TBaseIterator<true>(InPairIt)
		{
		}
	};

	/** Iterates over values associated with a specified key in a const map. */
	class TConstKeyIterator : public TBaseKeyIterator<true>
	{
	public:
		FORCEINLINE TConstKeyIterator(const TMapBase& InMap,KeyInitType InKey)
		:	TBaseKeyIterator<true>(typename PairSetType::TConstKeyIterator(InMap.Pairs,InKey))
		{}
	};

	/** Iterates over values associated with a specified key in a map. */
	class TKeyIterator : public TBaseKeyIterator<false>
	{
	public:
		FORCEINLINE TKeyIterator(TMapBase& InMap,KeyInitType InKey)
		:	TBaseKeyIterator<false>(typename PairSetType::TKeyIterator(InMap.Pairs,InKey))
		{}

		/** Removes the current key-value pair from the map. */
		FORCEINLINE void RemoveCurrent()
		{
			TBaseKeyIterator<false>::SetIt.RemoveCurrent();
		}
	};

	/** Creates an iterator over all the pairs in this map */
	FORCEINLINE TIterator CreateIterator()
	{
		return TIterator(*this);
	}

	/** Creates a const iterator over all the pairs in this map */
	FORCEINLINE TConstIterator CreateConstIterator() const
	{
		return TConstIterator(*this);
	}

	/** Creates an iterator over the values associated with a specified key in a map */
	FORCEINLINE TKeyIterator CreateKeyIterator(KeyInitType InKey)
	{
		return TKeyIterator(*this, InKey);
	}

	/** Creates a const iterator over the values associated with a specified key in a map */
	FORCEINLINE TConstKeyIterator CreateConstKeyIterator(KeyInitType InKey) const
	{
		return TConstKeyIterator(*this, InKey);
	}

private:
	/**
	 * DO NOT USE DIRECTLY
	 * STL-like iterators to enable range-based for loop support.
	 */
	FORCEINLINE friend TIterator      begin(      TMapBase& MapBase) { return TIterator     (MapBase, begin(MapBase.Pairs)); }
	FORCEINLINE friend TConstIterator begin(const TMapBase& MapBase) { return TConstIterator(         begin(MapBase.Pairs)); }
	FORCEINLINE friend TIterator      end  (      TMapBase& MapBase) { return TIterator     (MapBase, end  (MapBase.Pairs)); }
	FORCEINLINE friend TConstIterator end  (const TMapBase& MapBase) { return TConstIterator(         end  (MapBase.Pairs)); }
};

/** The base type of sortable maps. */
template<typename KeyType,typename ValueType,bool bInAllowDuplicateKeys,typename SetAllocator,typename KeyFuncs>
class TSortableMapBase : public TMapBase<KeyType,ValueType,bInAllowDuplicateKeys,SetAllocator,KeyFuncs>
{
public:
	typedef TMapBase<KeyType,ValueType,bInAllowDuplicateKeys,SetAllocator,KeyFuncs> Super;

#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS

	TSortableMapBase() = default;
	TSortableMapBase(TSortableMapBase&&) = default;
	TSortableMapBase(const TSortableMapBase&) = default;
	TSortableMapBase& operator=(TSortableMapBase&&) = default;
	TSortableMapBase& operator=(const TSortableMapBase&) = default;

#else

	FORCEINLINE TSortableMapBase() {}
	FORCEINLINE TSortableMapBase(      TSortableMapBase&& Other) : Super(MoveTemp(Other)) {}
	FORCEINLINE TSortableMapBase(const TSortableMapBase&  Other) : Super(         Other ) {}
	FORCEINLINE TSortableMapBase& operator=(      TSortableMapBase&& Other) { Super::operator=(MoveTemp(Other)); return *this; }
	FORCEINLINE TSortableMapBase& operator=(const TSortableMapBase&  Other) { Super::operator=(         Other ); return *this; }

#endif

	/**
	 * Sorts the pairs array using each pair's Key as the sort criteria, then rebuilds the map's hash.
	 * Invoked using "MyMapVar.KeySort( PREDICATE_CLASS() );"
	 */
	template<typename PREDICATE_CLASS>
	FORCEINLINE void KeySort( const PREDICATE_CLASS& Predicate )
	{
		Super::Pairs.Sort( FKeyComparisonClass<PREDICATE_CLASS>( Predicate ) );
	}

	/**
	 * Sorts the pairs array using each pair's Value as the sort criteria, then rebuilds the map's hash.
	 * Invoked using "MyMapVar.ValueSort( PREDICATE_CLASS() );"
	 */
	template<typename PREDICATE_CLASS>
	FORCEINLINE void ValueSort( const PREDICATE_CLASS& Predicate )
	{
		Super::Pairs.Sort( FValueComparisonClass<PREDICATE_CLASS>( Predicate ) );
	}

private:

	/** Extracts the pair's key from the map's pair structure and passes it to the user provided comparison class. */
	template<typename PREDICATE_CLASS>
	class FKeyComparisonClass
	{
		TDereferenceWrapper< KeyType, PREDICATE_CLASS> Predicate;

	public:

		FORCEINLINE FKeyComparisonClass( const PREDICATE_CLASS& InPredicate )
			: Predicate( InPredicate )
		{}

		FORCEINLINE bool operator()( const typename Super::PairType& A, const typename Super::PairType& B ) const
		{
			return Predicate( A.Key, B.Key );
		}
	};

	/** Extracts the pair's value from the map's pair structure and passes it to the user provided comparison class. */
	template<typename PREDICATE_CLASS>
	class FValueComparisonClass
	{
		TDereferenceWrapper< ValueType, PREDICATE_CLASS> Predicate;

	public:

		FORCEINLINE FValueComparisonClass( const PREDICATE_CLASS& InPredicate )
			: Predicate( InPredicate )
		{}

		FORCEINLINE bool operator()( const typename Super::PairType& A, const typename Super::PairType& B ) const
		{
			return Predicate( A.Value, B.Value );
		}
	};
};

/** A TMapBase specialization that only allows a single value associated with each key.*/
template<typename KeyType,typename ValueType,typename SetAllocator /*= FDefaultSetAllocator*/,typename KeyFuncs /*= TDefaultMapKeyFuncs<KeyType,ValueType,false>*/>
class TMap : public TSortableMapBase<KeyType,ValueType,false,SetAllocator,KeyFuncs>
{
public:

	typedef TSortableMapBase<KeyType,ValueType,false,SetAllocator,KeyFuncs> Super;
	typedef typename Super::KeyInitType KeyInitType;
	typedef typename Super::KeyConstPointerType KeyConstPointerType;

#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS

	TMap() = default;
	TMap(TMap&&) = default;
	TMap(const TMap&) = default;
	TMap& operator=(TMap&&) = default;
	TMap& operator=(const TMap&) = default;

#else

	FORCEINLINE TMap() {}
	FORCEINLINE TMap(      TMap&& Other) : Super(MoveTemp(Other)) {}
	FORCEINLINE TMap(const TMap&  Other) : Super(         Other ) {}
	FORCEINLINE TMap& operator=(      TMap&& Other) { Super::operator=(MoveTemp(Other)); return *this; }
	FORCEINLINE TMap& operator=(const TMap&  Other) { Super::operator=(         Other ); return *this; }

#endif

	/**
	 * Removes the pair with the specified key and copies the value that was removed to the ref parameter
	 * @param Key - the key to search for
	 * @param OutRemovedValue - if found, the value that was removed (not modified if the key was not found)
	 * @return whether or not the key was found
	 */
	FORCEINLINE bool RemoveAndCopyValue(KeyInitType Key,ValueType& OutRemovedValue)
	{
		const FSetElementId PairId = Super::Pairs.FindId(Key);
		if(!PairId.IsValidId())
			return false;

		OutRemovedValue = Super::Pairs(PairId).Value;
		Super::Pairs.Remove(PairId);
		return true;
	}
	
	/**
	 * Finds a pair with the specified key, removes it from the map, and returns the value part of the pair.
	 * If no pair was found, an exception is thrown.
	 * @param Key - the key to search for
	 * @return whether or not the key was found
	 */
	FORCEINLINE ValueType FindAndRemoveChecked(KeyConstPointerType Key)
	{
		const FSetElementId PairId = Super::Pairs.FindId(Key);
		check(PairId.IsValidId());
		ValueType Result = Super::Pairs(PairId).Value;
		Super::Pairs.Remove(PairId);
		return Result;
	}

	/**
	 * Move all items from another map into our map (if any keys are in both, the value from the other map wins) and empty the other map.
	 * @param OtherMap - The other map of items to move the elements from.
	 */
	void Append(TMap&& OtherMap)
	{
		for (auto MapIt = OtherMap.CreateIterator(); MapIt; ++MapIt)
		{
			this->Add(MoveTemp(MapIt->Key), MoveTemp(MapIt->Value));
		}

		OtherMap.Empty();
	}

	/**
	 * Add all items from another map to our map (if any keys are in both, the value from the other map wins)
	 * @param OtherMap - The other map of items to add.
	 */
	void Append(const TMap& OtherMap)
	{
		for (auto MapIt = OtherMap.CreateConstIterator(); MapIt; ++MapIt)
		{
			this->Add(MapIt->Key, MapIt->Value);
		}
	}

	FORCEINLINE       ValueType& operator[](KeyConstPointerType Key)       { return this->FindChecked(Key); }
	FORCEINLINE const ValueType& operator[](KeyConstPointerType Key) const { return this->FindChecked(Key); }
};

/** A TMapBase specialization that allows multiple values to be associated with each key. */
template<typename KeyType,typename ValueType,typename SetAllocator /* = FDefaultSetAllocator */,typename KeyFuncs /*= TDefaultMapKeyFuncs<KeyType,ValueType,true>*/>
class TMultiMap : public TSortableMapBase<KeyType,ValueType,true,SetAllocator,KeyFuncs>
{
public:

	typedef TSortableMapBase<KeyType,ValueType,true,SetAllocator,KeyFuncs> Super;
	typedef typename Super::KeyConstPointerType KeyConstPointerType;
	typedef typename Super::KeyInitType KeyInitType;
	typedef typename Super::ValueInitType ValueInitType;

#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS

	TMultiMap() = default;
	TMultiMap(TMultiMap&&) = default;
	TMultiMap(const TMultiMap&) = default;
	TMultiMap& operator=(TMultiMap&&) = default;
	TMultiMap& operator=(const TMultiMap&) = default;

#else

	FORCEINLINE TMultiMap() {}
	FORCEINLINE TMultiMap(      TMultiMap&& Other) : Super(MoveTemp(Other)) {}
	FORCEINLINE TMultiMap(const TMultiMap&  Other) : Super(         Other ) {}
	FORCEINLINE TMultiMap& operator=(      TMultiMap&& Other) { Super::operator=(MoveTemp(Other)); return *this; }
	FORCEINLINE TMultiMap& operator=(const TMultiMap&  Other) { Super::operator=(         Other ); return *this; }

#endif

	/**
	 * Finds all values associated with the specified key.
	 * @param Key - The key to find associated values for.
	 * @param OutValues - Upon return, contains the values associated with the key.
	 * @param bMaintainOrder - true if the Values array should be in the same order as the map's pairs.
	 */
	void MultiFind(KeyInitType Key,TArray<ValueType>& OutValues,bool bMaintainOrder = false) const
	{
		for(typename Super::PairSetType::TConstKeyIterator It(Super::Pairs,Key);It;++It)
		{
			new(OutValues) ValueType(It->Value);
		}

		if(bMaintainOrder)
		{
			// Create an array with the values in reverse enumerated order; i.e. the order they were inserted in the map.
			TArray<ValueType> OrderedValues;
			OrderedValues.Empty(OutValues.Num());
			for(int32 Index = OutValues.Num() - 1;Index >= 0;Index--)
			{
				new(OrderedValues) ValueType(OutValues[Index]);
			}

			// Swap the ordered array into the output array.
			Exchange(OrderedValues,OutValues);
		}
	}

	/**
	 * Finds all values associated with the specified key.
	 * @param Key - The key to find associated values for.
	 * @param OutValues - Upon return, contains pointers to the values associated with the key.
	 *					Pointers are only valid until the next change to any key in the map.
	 * @param bMaintainOrder - true if the Values array should be in the same order as the map's pairs.
	 */
	template<typename Allocator> void MultiFindPointer(KeyInitType Key,TArray<const ValueType*, Allocator>& OutValues,bool bMaintainOrder = false) const
	{
		for(typename Super::PairSetType::TConstKeyIterator It(Super::Pairs,Key);It;++It)
		{
			new(OutValues) const ValueType*(&It->Value);
		}

		if(bMaintainOrder)
		{
			// Create an array with the values in reverse enumerated order; i.e. the order they were inserted in the map.
			TArray<const ValueType*, Allocator> OrderedValues;
			OrderedValues.Empty(OutValues.Num());
			for(int32 Index = OutValues.Num() - 1;Index >= 0;Index--)
			{
				new(OrderedValues) const ValueType*(OutValues(Index));
			}

			// Swap the ordered array into the output array.
			Exchange(OrderedValues,OutValues);
		}
	}

	/**
	 * Adds a key-value association to the map.  The association doesn't replace any of the key's existing associations.
	 *
	 * @param InKey - The key to associate.
	 * @param InValue - The value to associate.
	 * @return A reference to the value as stored in the map; the reference is only valid until the next change to any key in the map.
	 */
	FORCEINLINE ValueType& Add(const KeyType&  InKey, const ValueType&  InValue) { return Emplace(         InKey ,          InValue ); }
	FORCEINLINE ValueType& Add(const KeyType&  InKey,       ValueType&& InValue) { return Emplace(         InKey , MoveTemp(InValue)); }
	FORCEINLINE ValueType& Add(      KeyType&& InKey, const ValueType&  InValue) { return Emplace(MoveTemp(InKey),          InValue ); }
	FORCEINLINE ValueType& Add(      KeyType&& InKey,       ValueType&& InValue) { return Emplace(MoveTemp(InKey), MoveTemp(InValue)); }

	/**
	 * Adds a key-value association to the map.  The association doesn't replace any of the key's existing associations.
	 * The value is default-constructed.
	 *
	 * @param InKey - The key to associate.
	 * @return A reference to the value as stored in the map; the reference is only valid until the next change to any key in the map.
	 */
	FORCEINLINE ValueType& Add(const KeyType&  InKey) { return Emplace(         InKey ); }
	FORCEINLINE ValueType& Add(      KeyType&& InKey) { return Emplace(MoveTemp(InKey)); }

	/**
	 * Adds a key-value association to the map.  The association doesn't replace any of the key's existing associations.
	 *
	 * @param InKey - The key to associate.
	 * @param InValue - The value to associate.
	 * @return A reference to the value as stored in the map; the reference is only valid until the next change to any key in the map.
	 */
	template <typename InitKeyType, typename InitValueType>
	FORCEINLINE ValueType& Emplace(InitKeyType&& InKey, InitValueType&& InValue)
	{
		const FSetElementId PairId = Super::Pairs.Emplace(TPairInitializer<InitKeyType&&, InitValueType&&>(Forward<InitKeyType>(InKey), Forward<InitValueType>(InValue)));
		return Super::Pairs(PairId).Value;
	}

	/**
	 * Adds a key-value association to the map.  The association doesn't replace any of the key's existing associations.
	 * The value is default-constructed.
	 *
	 * @param InKey - The key to associate.
	 * @param InValue - The value to associate.
	 * @return A reference to the value as stored in the map; the reference is only valid until the next change to any key in the map.
	 */
	template <typename InitKeyType>
	FORCEINLINE ValueType& Emplace(InitKeyType&& InKey)
	{
		const FSetElementId KeyId = Super::Pairs.Emplace(TKeyInitializer<InitKeyType&&>(Forward<InitKeyType>(InKey)));
		return Super::Pairs(KeyId).Value;
	}

	/**
	 * Adds a key-value association to the map.  The association doesn't replace any of the key's existing associations.
	 * However, if both the key and value match an existing association in the map, no new association is made and the existing association's
	 * value is returned.
	 * @param InKey - The key to associate.
	 * @param InValue - The value to associate.
	 * @return A reference to the value as stored in the map; the reference is only valid until the next change to any key in the map.
	 */
	FORCEINLINE ValueType& AddUnique(const KeyType&  InKey, const ValueType&  InValue) { return EmplaceUnique(         InKey ,          InValue ); }
	FORCEINLINE ValueType& AddUnique(const KeyType&  InKey,       ValueType&& InValue) { return EmplaceUnique(         InKey , MoveTemp(InValue)); }
	FORCEINLINE ValueType& AddUnique(      KeyType&& InKey, const ValueType&  InValue) { return EmplaceUnique(MoveTemp(InKey),          InValue ); }
	FORCEINLINE ValueType& AddUnique(      KeyType&& InKey,       ValueType&& InValue) { return EmplaceUnique(MoveTemp(InKey), MoveTemp(InValue)); }

	/**
	 * Adds a key-value association to the map.  The association doesn't replace any of the key's existing associations.
	 * However, if both the key and value match an existing association in the map, no new association is made and the existing association's
	 * value is returned.
	 * @param InKey - The key to associate.
	 * @param InValue - The value to associate.
	 * @return A reference to the value as stored in the map; the reference is only valid until the next change to any key in the map.
	 */
	template <typename InitKeyType, typename InitValueType>
	ValueType& EmplaceUnique(InitKeyType&& InKey, InitValueType&& InValue)
	{
		if (ValueType* Found = FindPair(InKey, InValue))
			return *Found;

		// If there's no existing association with the same key and value, create one.
		return Add(Forward<InitKeyType>(InKey), Forward<InitValueType>(InValue));
	}

	/**
	 * Removes all value associations for a key.
	 * @param InKey - The key to remove associated values for.
	 * @return The number of values that were associated with the key.
	 */
	FORCEINLINE int32 Remove(KeyConstPointerType InKey)
	{
		return Super::Remove(InKey);
	}

	/**
	 * Removes associations between the specified key and value from the map.
	 * @param InKey - The key part of the pair to remove.
	 * @param InValue - The value part of the pair to remove.
	 * @return The number of associations removed.
	 */
	int32 Remove(KeyInitType InKey,ValueInitType InValue)
	{
		// Iterate over pairs with a matching key.
		int32 NumRemovedPairs = 0;
		for(typename Super::PairSetType::TKeyIterator It(Super::Pairs,InKey);It;++It)
		{
			// If this pair has a matching value as well, remove it.
			if(It->Value == InValue)
			{
				It.RemoveCurrent();
				++NumRemovedPairs;
			}
		}
		return NumRemovedPairs;
	}

	/**
	 * Removes the first association between the specified key and value from the map.
	 * @param InKey - The key part of the pair to remove.
	 * @param InValue - The value part of the pair to remove.
	 * @return The number of associations removed.
	 */
	int32 RemoveSingle(KeyInitType InKey,ValueInitType InValue)
	{
		// Iterate over pairs with a matching key.
		int32 NumRemovedPairs = 0;
		for(typename Super::PairSetType::TKeyIterator It(Super::Pairs,InKey);It;++It)
		{
			// If this pair has a matching value as well, remove it.
			if(It->Value == InValue)
			{
				It.RemoveCurrent();
				++NumRemovedPairs;

				// We were asked to remove only the first association, so bail out.
				break;
			}
		}
		return NumRemovedPairs;
	}

	/**
	 * Finds an association between a specified key and value. (const)
	 * @param Key - The key to find.
	 * @param Value - The value to find.
	 * @return If the map contains a matching association, a pointer to the value in the map is returned.  Otherwise NULL is returned.
	 *			The pointer is only valid as long as the map isn't changed.
	 */
	FORCEINLINE const ValueType* FindPair(KeyInitType Key,ValueInitType Value) const
	{
		return const_cast<TMultiMap*>(this)->FindPair(Key, Value);
	}

	/**
	 * Finds an association between a specified key and value.
	 * @param Key - The key to find.
	 * @param Value - The value to find.
	 * @return If the map contains a matching association, a pointer to the value in the map is returned.  Otherwise NULL is returned.
	 *			The pointer is only valid as long as the map isn't changed.
	 */
	ValueType* FindPair(KeyInitType Key,ValueInitType Value)
	{
		// Iterate over pairs with a matching key.
		for(typename Super::PairSetType::TKeyIterator It(Super::Pairs,Key);It;++It)
		{
			// If the pair's value matches, return a pointer to it.
			if(It->Value == Value)
			{
				return &It->Value;
			}
		}

		return NULL;
	}

	/** Returns the number of values within this map associated with the specified key */
	int32 Num(KeyInitType Key) const
	{
		// Iterate over pairs with a matching key.
		int32 NumMatchingPairs = 0;
		for(typename Super::PairSetType::TConstKeyIterator It(Super::Pairs,Key);It;++It)
		{
			++NumMatchingPairs;
		}
		return NumMatchingPairs;
	}

	// Since we implement an overloaded Num() function in TMultiMap, we need to reimplement TMapBase::Num to make it visible.
	FORCEINLINE int32 Num() const
	{
		return Super::Num();
	}
};
