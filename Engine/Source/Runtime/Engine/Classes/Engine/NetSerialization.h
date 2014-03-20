// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	NetworkSerialization.h: 
	Contains custom network serialization functionality.
=============================================================================*/

#pragma once
#include "NetSerialization.generated.h"


/**
 *	===================== NetSerialize and NetDeltaSerialize customization. =====================
 *
 *	The main purpose of this file it to hold custom methods for NetSerialization and NetDeltaSerialization. A longer explanation on how this all works is
 *	covered below. For quick reference however, this is how to customize net serialization for structs.
 *
 *		
 *	To define your own NetSerialize and NetDeltaSerialize on a structure:
 *		(of course you don't need to define both! Usually you only want to define one, but for brevity Im showing both at once)
 */
#if 0

USTRUCT()
struct FExampleStruct
{
	GENERATED_USTRUCT_BODY()
	
	/**
	 * @param Ar			FArchive to read or write from.
	 * @param Map			PackageMap used to resolve references to UObject*
	 * @param bOutSuccess	return value to signify if the serialization was succesfull (if false, an error will be logged by the calling function)
	 *
	 * @return return true if the serialization was fully mapped. If false, the property will be considered 'dirty' and will replicate again on the next update.
	 *	This is needed for UActor* properties. If an actor's Actorchannel is not fully mapped, properties referencing it must stay dirty.
	 *	Note that UPackageMap::SerializeObject returns false if an object is unmapped. Generally, you will want to return false from your ::NetSerialize
	 *  if you make any calls to ::SerializeObject that return false.
	 *
	*/
	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
	{
		// Your code here!
		return true;
	}

	/**
	 * @param DeltaParms	Generic struct of input parameters for delta serialization
	 *
	 * @return return true if the serialization was fully mapped. If false, the property will be considered 'dirty' and will replicate again on the next update.
	 *	This is needed for UActor* properties. If an actor's Actorchannel is not fully mapped, properties referencing it must stay dirty.
	 *	Note that UPackageMap::SerializeObject returns false if an object is unmapped. Generally, you will want to return false from your ::NetSerialize
	 *  if you make any calls to ::SerializeObject that return false.
	 *
	*/
	bool NetDeltaSerialize(FNetDeltaSerializeInfo & DeltaParms)
	{
		// Your code here!
		return true;
	}
}

template<>
struct TStructOpsTypeTraits< FExampleStruct > : public TStructOpsTypeTraitsBase
{
	enum 
	{
		WithNetSerializer = true,
		WithNetDeltaSerializer = true,
	};
};

#endif

/**
 *	===================== Fast TArray Replication ===================== 
 *
 *	Fast TArray Replication is a custom implementation of NetDeltaSerialize that is suitable for TArrays of UStructs. It offers performance
 *	improvements for large data sets, it serializes removals from anywhere in the array optimally, and allows events to be called on clients
 *	for adds and removals. The downside is that you will need to have game code mark items in the array as dirty, and well as the *order* of the list
 *	is not garuanteed to be identical between client and server in all cases.
 *
 *	Using FTR is more complicated, but this is the code you need:
 *
 */
#if 0
	
/** Step 1: Make your struct inherit from FFastArraySerializerItem */
USTRUCT()
struct FExampleItemEntry : public FFastArraySerializerItem
{
	GENERATED_USTRUCT_BODY()

	// Your data:
	UPROPERTY()
	int32		ExampleIntProperty;	

	UPROPERTY()
	float		ExampleFloatProperty;


	/** Optional functions you can implement for client side notification of changes to items */
	void PreReplicatedRemove();
	void PostReplicatedAdd();
	void PostReplicatedChange();
};

/** Step 2: You MUST wrap your TArray in another struct that inherits from FFastArraySerializer */
USTRUCT()
struct FExampleArray: public FFastArraySerializer
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FExampleItemEntry>	Items;	/** Step 3: You MUST have a TArray named Items of the struct you made in step 1. */

	/** Step 4: Copy this, replace example with your names */
	bool NetDeltaSerialize(FNetDeltaSerializeInfo & DeltaParms)
	{
	   return FastArrayDeltaSerialize<FExampleItemEntry>( Items, DeltaParms );
	}
};

/** Step 5: Copy and paste this struct trait, replacing FExampleArray with your Step 2 struct. */
template<>
struct TStructOpsTypeTraits< FExampleArray > : public TStructOpsTypeTraitsBase
{
       enum 
       {
			WithNetDeltaSerializer = true,
       };
};

#endif

/** Step 6 and beyond: 
 *		-Declare a UPROPERTY of your FExampleArray (step 2) type.
 *		-You MUST call MarkItemDirty on the FExampleArray when you change an item in the array. You pass in a reference to the item you dirtied. 
 *			See FFastArraySerializer::MarkItemDirty.
 *		-You MUST call MarkArrayDirty on the FExampleArray if you remove something from the array.
 *		-In your classes GetLifetimeReplicatedProps, use DOREPLIFETIME(YourClass, YourArrayStructPropertyName);
 *
 *		You can override the following virtual functions in your structure (step 1) to get notifies before add/deletes/removes:
 *			-void PreReplicatedRemove()
 *			-void PostReplicatedAdd()
 *			-void PostReplicatedChange()
 *
 *		Thats it!
 */ 




/**
 *	
 *	===================== An Overview of Net Serialization and how this all works =====================
 *
 *		Everything originates in UNetDriver::ServerReplicateActors.
 *		Actors are chosen to replicate, create actor channels, and UActorChannel::ReplicateActor is called.
 *		ReplicateActor is ultmately responsible for deciding what properties have changed, and constructing a FOutBUnch to send to clients.
 *
 *	The UActorChannel has 2 ways to decide what properties need to be sent.
 *		The traditional way, which is a flat TArray<uint8> buffer: UActorChannel::Recent. This represents a flat block of the actor properties.
 *		This block literally can be cast to an AActor* and property values can be looked up if you know the UProperty offset.
 *		The Recent buffer represents the values that the client using this actor channel has. We use recent to compare to current, and decide what to send.
 *
 *		This works great for 'atomic' properties; ints, floats, object*, etc.
 *		It does not work for 'dynamic' properties such as TArrays, which store values Num/Max but also a pointer to their array data,
 *		The array data has no where to fit in the flat ::Recent buffer. (Dynamic is probably a bad name for these properties)
 *
 *		To get around this, UActorChannel also has a TMap for 'dynamic' state. UActorChannel::RecentDynamicState. This map allows us to look up
 *		a 'base state' for a property given a property's RepIndex.
 *
 *	NetSerialize & NetDeltaSerialize
 *		Properties that fit into the flat Recent buffer can be serialized entirely with NetSerialize. NetSerialize just reads or writes to an FArchive.
 *		Since the replication can just look at the Recent[] buffer and do a direct comparison, it can tell what properties are dirty. NetSerialize just
 *		reads or writes.
 *
 *		Dynamic properties can only be serialized with NetDeltaSerialize. NetDeltaSerialize is serialization from a given base state, and produces
 *		both a 'delta' state (which gets sent to the client) and a 'full' state (which is saved to be used as the base state in future delta serializes).
 *		NetDeltaSerialize essentially does the diffing as well as the serialization. It must do the diffing so it can know what parts of the property it must
 *		send.
 *	
 *	Base States and dynamic properties replication.
 *		As far as the replication system / UActorChannel is concerned, a base state can be anything. The base state only deals with INetDeltaBaseState*.
 *
 *		UActorChannel::ReplicateActor will ultimately decide whether to call UProperty::NetSerializeItem or UProperty::NetDeltaSerializeItem.
 *
 *		As mentioned above NetDeltaSerialize takes in an extra base state and produces a diff state and a full state. The full state produced is used
 *		as the base state for future delta serialization. NetDeltaSerialize uses the base state and the current values of the actor to determine what parts
 *		it needs to send.
 *		
 *		The INetDeltaBaseStates are created within the NetDeltaSerialize functions. The replication system / UActorChannel does not know about the details.
 *
 *		Right now, there are 2 forms of delta serialization: Generic Replication and Fast Array Replication.
 *
 *	
 *	Generic Delta Replication
 *		Generic Delta Replication is implemented by UStructProperty::NetDeltaSerializeItem, UArrayProperty::NetDeltaSerializeItem, UProperty::NetDeltaSerializeItem.
 *		It works by first NetSerializing the current state of the object (the 'full' state) and using memcmp to compare it to previous base state. UProperty
 *		is what actually implements the comparison, writing the current state to the diff state if it has changed, and always writing to the full state otherwise.
 *		The UStructProperty and UArrayProperty functions work by iterating their fields or array elements and calling the UProperty function, while also embedding
 *		meta data. 
 *
 *		For example UArrayProperty basically writes: 
 *			"Array has X elements now" -> "Here is element Y" -> Output from UProperty::NetDeltaSerialize -> "Here is element Z" -> etc
 *
 *		Generic Data Replication is the 'default' way of handling UArrayProperty and UStructProperty serialization. This will work for any array or struct with any 
 *		sub properties as long as those properties can NetSerialize.
 *
 *	Custom Net Delta Serialiation
 *		Custom Net Delta Serialiation works by using the struct trait system. If a struct has the WithNetDeltaSerializer trait, then its native NetDeltaSerialize
 *		function will be called instead of going through the Generic Delta Replication code path in UStructProperty::NetDeltaSerializeItem.
 *
 *	Fast TArray Replication
 *		Fast TArray Replication is implemented through custom net delta serialization. Instead of a flat TArray buffer to repesent states, it only is concerned
 *		with a TMap of IDs and ReplicationKeys. The IDs map to items in the array, which all have a ReplicationID field defined in FFastArraySerializerItem.
 *		FFastArraySerializerItem also has a ReplicationKey field. When items are marked dirty with MarkItemDirty, they are given a new ReplicationKey, and assigned
 *		a new ReplicationID if they don't have one.
 *
 *		FastArrayDeltaSerialize (defined below)
 *		During server serialization (writing), we compare the old base state (e.g, the old ID<->Key map) with the current state of the array. If items are missing
 *		we write them out as deletes in the bunch. If they are new or changed, they are written out as changed along with their state, serialized via a NetSerialize call.
 *
 *		For example, what actually is written may look like:
 *			"Array has X changed elements, Y deleted elements" -> "element A changed" -> Output from NetSerialize on rest of the struct item -> "Element B was deleted" -> etc
 *
 *		Note that the ReplicationID is replicated and in sync between client and server. The indices are not.
 *
 *		During client serialization (reading), the client reads in the number of changed and number of deleted elements. It also builds a mapping of ReplicationID -> local index of the current array.
 *		As it deserializes IDs, it looks up the element and then does what it needs to (create if necessary, serialize in the current state, or delete).
 *
 *		There is currently no delta serialization done on the inner structures. If a ReplicationKey changes, the entire item is serialized. If we had
 *		use cases where we needed it, we could delta serialization on the inner dynamic properties. This could be done with more struct customization.
 *
 *		ReplicationID and ReplicationKeys are set by the MarkItemDirty function on FFastArraySerializer. These are just int32s that are assigned in order as things change.
 *		There is nothing special about them other than being unique.
 */

/** Custom INetDeltaBaseState used by Fast Array Serialization */
class FNetFastTArrayBaseState : public INetDeltaBaseState
{
public:

	FNetFastTArrayBaseState() : ArrayReplicationKey(INDEX_NONE) { }

	virtual bool IsStateEqual(INetDeltaBaseState* OtherState)
	{
		FNetFastTArrayBaseState * Other = static_cast<FNetFastTArrayBaseState*>(OtherState);
		for (auto It = IDToCLMap.CreateIterator(); It; ++It)
		{
			auto Ptr = Other->IDToCLMap.Find(It.Key());
			if (!Ptr || *Ptr != It.Value())
			{
				return false;
			}
		}
		return true;
	}

	TMap<int32, int32> IDToCLMap;
	int32 ArrayReplicationKey;
};


/** Base struct for items using Fast TArray Replication */
USTRUCT()
struct FFastArraySerializerItem
{
	GENERATED_USTRUCT_BODY()

	FFastArraySerializerItem()
		: ReplicationID(INDEX_NONE), ReplicationKey(INDEX_NONE)
	{

	}

	FFastArraySerializerItem(const FFastArraySerializerItem &InItem)
		: ReplicationID(INDEX_NONE), ReplicationKey(INDEX_NONE)
	{

	}

	FFastArraySerializerItem& operator=(const FFastArraySerializerItem& In)
	{
		if (&In != this)
		{
			ReplicationID = INDEX_NONE;
			ReplicationKey = INDEX_NONE;
		}
		return *this;
	}

	UPROPERTY(NotReplicated)
	int32 ReplicationID;

	UPROPERTY(NotReplicated)
	int32 ReplicationKey;
	
	/**
	 * Called right before deleting element during replication.
	 * 
	 * NOTE: intentionally not virtual; invoked via templated code, @see FExampleItemEntry
	 */
	FORCEINLINE void PreReplicatedRemove() { }
	/**
	 * Called after adding and serializing a new element
	 *
	 * NOTE: intentionally not virtual; invoked via templated code, @see FExampleItemEntry
	 */
	FORCEINLINE void PostReplicatedAdd() { }
	/**
	 * Called after updating an existing element with new data
	 *
	 * NOTE: intentionally not virtual; invoked via templated code, @see FExampleItemEntry
	 */
	FORCEINLINE void PostReplicatedChange() { }
};

/** Base struct for wrapping the array used in Fast TArray Replication */
USTRUCT()
struct FFastArraySerializer
{
	GENERATED_USTRUCT_BODY()

	FFastArraySerializer() : IDCounter(0), ArrayReplicationKey(0) { }
	TMap<int32, int32>	ItemMap;
	int32	IDCounter;
	int32	ArrayReplicationKey;

	/** This must be called if you add or change an item in the array */
	void MarkItemDirty(FFastArraySerializerItem & Item)
	{
		if (Item.ReplicationID == INDEX_NONE)
		{
			Item.ReplicationID = ++IDCounter;
			if (IDCounter == INDEX_NONE)
				IDCounter++;
		}

		Item.ReplicationKey++;
		MarkArrayDirty();
	}

	/** This must be called if you just remove something from the array */
	void MarkArrayDirty()
	{
		ArrayReplicationKey++;
		if (ArrayReplicationKey == INDEX_NONE)
			ArrayReplicationKey++;
	}

	template< typename Type >
	bool FastArrayDeltaSerialize( TArray<Type> &Items, FNetDeltaSerializeInfo& Parms );
};


/** The function that implements Fast TArray Replication  */
template< typename Type >
bool FFastArraySerializer::FastArrayDeltaSerialize( TArray<Type> &Items, FNetDeltaSerializeInfo& Parms )
{
	UStruct* InnerStruct = Type::StaticStruct();

	if (Parms.OutBunch)
	{
		//-----------------------------
		// Saving
		//-----------------------------	
		check(Parms.Struct);
		FBitWriter& OutBunch = *Parms.OutBunch;

		// Create a new map from the current state of the array		
		FNetFastTArrayBaseState * NewState = new FNetFastTArrayBaseState();
		check(Parms.NewState);
		*Parms.NewState = TSharedPtr<INetDeltaBaseState>( NewState );
		TMap<int32, int32> & NewMap = NewState->IDToCLMap;
		NewState->ArrayReplicationKey = ArrayReplicationKey;

		// Get the old map if its there
		TMap<int32, int32> * OldMap = Parms.OldState ? &((FNetFastTArrayBaseState*)Parms.OldState)->IDToCLMap : NULL;

		// See if the array changed at all. If the ArrayReplicationKey matches we can skip checking individual items
		if (Parms.OldState && (ArrayReplicationKey == ((FNetFastTArrayBaseState*)Parms.OldState)->ArrayReplicationKey))
		{
			// Double check the old/new maps are the same size.
			ensure(OldMap->Num() == Items.Num());
			return false;
		}


		struct FIdxIDPair
		{
			FIdxIDPair(int32 _idx, int32 _id) : Idx(_idx), ID(_id) { }
			int32	Idx;
			int32	ID;
		};

		TArray<FIdxIDPair>	ChangedElements;
		TArray<int32>		DeletedElements;

		int32 DeleteCount = (OldMap ? OldMap->Num() : 0) - Items.Num(); // Note: this is incremented when we add new items below.
		UE_LOG(LogNetSerialization, Verbose, TEXT("NetSerializeItemDeltaFast: %s. DeleteCount: %d"), *Parms.DebugName, DeleteCount);

		//--------------------------------------------
		// Find out what is new or what has changed
		//--------------------------------------------
		for (int32 i=0; i < Items.Num(); ++i)
		{
			UE_LOG(LogNetSerialization, Log, TEXT("    Array[%d] - ID %d. CL %d."), i, Items[i].ReplicationID, Items[i].ReplicationKey);
			if (Items[i].ReplicationID == INDEX_NONE)
			{
				MarkItemDirty(Items[i]);
			}
			NewMap.Add( Items[i].ReplicationID, Items[i].ReplicationKey );

			int32* OldValuePtr = OldMap ? OldMap->Find( Items[i].ReplicationID ) : NULL;
			if (OldValuePtr)
			{
				if (*OldValuePtr == Items[i].ReplicationKey)
				{
					UE_LOG(LogNetSerialization, Log, TEXT("       Stayed The Same - Skipping"));

					// Stayed the same, it might have moved but we dont care
					continue;
				}
				else
				{
					UE_LOG(LogNetSerialization, Log, TEXT("       Changed! Was: %d"), *OldValuePtr);

					// Changed
					new (ChangedElements)FIdxIDPair(i, Items[i].ReplicationID);
				}
			}
			else
			{
				UE_LOG(LogNetSerialization, Log, TEXT("       New!"));
				
				// The item really should have a valid ReplicationID but in the case of loading from a save game,
				// items may not have been marked dirty individually. Its ok to just assign them one here.
				// New
				new (ChangedElements)FIdxIDPair(i, Items[i].ReplicationID);
				DeleteCount++; // We added something new, so our initial DeleteCount value must be incremented.
			}
		}

		// Find out what was deleted
		if( DeleteCount > 0 && OldMap)
		{
			for (auto It = OldMap->CreateIterator(); It; ++It)
			{
				if (!NewMap.Contains(It.Key()))
				{
					UE_LOG(LogNetSerialization, Log, TEXT("   Deleting ID: %d"), It.Key());

					DeletedElements.Add( It.Key() );
					if (--DeleteCount <= 0)
						break;
				}
			}
		}

		// return if nothing changed
		if ( ChangedElements.Num() == 0  && DeletedElements.Num() == 0)
		{
			// Nothing changed
			UE_LOG(LogNetSerialization, Verbose, TEXT("   No Changed Elements in this array - skipping write"));
			return false;
		}

		//----------------------
		// Write it out.
		//----------------------

		uint32 ElemNum = ChangedElements.Num();
		OutBunch << ElemNum;

		ElemNum = DeletedElements.Num();
		OutBunch << ElemNum;

		UE_LOG(LogNetSerialization, Log, TEXT("   Writing Bunch. NumChange: %d. NumDel: %d"), ChangedElements.Num(), DeletedElements.Num() );

		// Serialized new elements with their payload
		for (auto It = ChangedElements.CreateIterator(); It; ++It)
		{
			void * ThisElement = &Items[It->Idx];

			// Dont pack this, want property to be byte aligned
			uint32 ID = It->ID;
			OutBunch << ID;

			UE_LOG(LogNetSerialization, Log, TEXT("   Changed ElementID: %d"), ID);

			bool bHasUnmapped = false;

			Parms.NetSerializeCB->NetSerializeStruct( InnerStruct, OutBunch, Parms.Map, ThisElement, bHasUnmapped );

			if ( bHasUnmapped )
			{
				// Set to 0 to mean 'unmapped'. This will force reserialization on next update.
				*NewMap.Find(ID) = 0;
				NewState->ArrayReplicationKey = INDEX_NONE;
				UE_LOG( LogNetSerialization, Log, TEXT("   Property: %s is unmapped. Will reserialize."), *InnerStruct->GetName() );
			}
		}

		// Serialize deleted items, just by their ID
		for (auto It = DeletedElements.CreateIterator(); It; ++It)
		{
			int32 ID = *It;
			OutBunch << ID;
			UE_LOG(LogNetSerialization, Log, TEXT("   Deleted ElementID: %d"), ID);
		}
	}
	else
	{
		//-----------------------------
		// Loading
		//-----------------------------	
		check(Parms.InArchive);
		FArchive &Ar = *Parms.InArchive;

		//---------------
		// Build ItemMap if necessary. This maps ReplicationID to our local index into the Items array.
		//---------------
		if (ItemMap.Num() != Items.Num())
		{
			SCOPE_CYCLE_COUNTER(STAT_NetSerializeFast_Array);
			UE_LOG(LogNetSerialization, Verbose, TEXT("Recreating Items map. Items.Num: %d Map.Num: %d"), Items.Num(), ItemMap.Num() );

			ItemMap.Empty();
			for (int32 i=0; i < Items.Num(); ++i)
			{
				ItemMap.Add( Items[i].ReplicationID, i );
			}
		}
		
		static const int32 MAX_NUM_CHANGED = 2048;
		static const int32 MAX_NUM_DELETED = 2048;

		//---------------
		// Read header
		//---------------
		uint32 NumChanged;
		Ar << NumChanged;

		if ( NumChanged > MAX_NUM_CHANGED )
		{
			UE_LOG( LogNetSerialization, Warning, TEXT( "NumChanged > MAX_NUM_CHANGED: %d." ), NumChanged );
			Ar.SetError();
			return false;;
		}

		uint32 NumDeletes;
		Ar << NumDeletes;
	
		if ( NumDeletes > MAX_NUM_DELETED )
		{
			UE_LOG( LogNetSerialization, Warning, TEXT( "NumDeletes > MAX_NUM_DELETED: %d." ), NumDeletes );
			Ar.SetError();
			return false;;
		}

		UE_LOG( LogNetSerialization, Verbose, TEXT( "Read NumChanged: %d NumDeletes: %d." ), NumChanged, NumDeletes );

		//---------------
		// Read Changed/New elements
		//---------------
		for(uint32 i=0; i < NumChanged; ++i)
		{
			int32 ElementID;
			Ar << ElementID;

			int32 * ElementIndexPtr = ItemMap.Find(ElementID);
			int32	ElementIndex = 0;
			Type *	ThisElement = NULL;

			bool NewElement = false;
		
			if (!ElementIndexPtr)
			{
				UE_LOG(LogNetSerialization, Log, TEXT("   New. ID: %d. New Element!"), ElementID);

				ThisElement = new (Items)Type();
				ThisElement->ReplicationID = ElementID;

				ElementIndex = Items.Num()-1;
				ItemMap.Add(ElementID, ElementIndex);

				NewElement = true;
			}
			else
			{
				UE_LOG(LogNetSerialization, Log, TEXT("   Changed. ID: %d -> Idx: %d"), ElementID, *ElementIndexPtr);
				ElementIndex = *ElementIndexPtr;
				ThisElement = &Items[ElementIndex];
			}
			
			bool bHasUnmapped = false;
			Parms.NetSerializeCB->NetSerializeStruct( InnerStruct, Ar, Parms.Map, ThisElement, bHasUnmapped );

			if ( Ar.IsError() )
			{
				UE_LOG( LogNetSerialization, Warning, TEXT( "Parms.NetSerializeCB->NetSerializeStruct: Ar.IsError() == true" ) );
				return false;
			}

			if (NewElement)
			{
				ThisElement->PostReplicatedAdd();
			} 
			else
			{
				ThisElement->PostReplicatedChange();
			}

		}

		//---------------
		// Read Changed/New elements
		//---------------
		if (NumDeletes > 0)
		{
			TArray<int32> DeleteIndices;
			for (uint32 i=0; i < NumDeletes; ++i)
			{
				int32 ElementID;
				Ar << ElementID;

				int32* ElementIndexPtr = ItemMap.Find(ElementID);
				if (ElementIndexPtr)
				{
					DeleteIndices.Add(*ElementIndexPtr);
				}
				else
				{
					UE_LOG(LogNetSerialization, Warning, TEXT("   Couldn't find ElementID: %d for deletion!"), ElementID);
				}
			}

			DeleteIndices.Sort();
			for (int32 i = DeleteIndices.Num()-1; i >=0 ; --i)
			{
				int32 DeleteIndex = DeleteIndices[i];
				if (Items.IsValidIndex(DeleteIndex))
				{
					Items[DeleteIndex].PreReplicatedRemove();
					Items.RemoveAt(DeleteIndex);

					UE_LOG(LogNetSerialization, Log, TEXT("   Deleting: %d"), DeleteIndex);
				}
			}
			
			// Clear the map now that the indices are all shifted around. This kind of sucks, we could use slightly better data structures here I think.
			ItemMap.Empty();
		}
	}

	return true;
}




/**
 *	===================== Vector NetSerialization customization. =====================
 *	Provides custom NetSerilization for FVectors.
 *
 *	There are two types of net quantization available:
 *
 *	Fixed Quantization (SerializeFixedVector)
 *		-Fixed number of bits
 *		-Max Value specified as template parameter
 *
 *		Serialized value is scaled based on num bits and max value. Precision is determined by MaxValue and NumBits
 *		(if 2^NumBits is > MaxValue, you will have room for extra precision).
 *
 *		This format is good for things like normals, where the magnitudes are often similiar. For example normal values may often
 *		be in the 0.1f - 1.f range. In a packed format, the overhead in serializing num of bits per component would outweigh savings from
 *		serializiing very small ( < 0.1f ) values.
 *
 *		It is also good for performance critical sections since you can garuntee byte alignment if that is important.
 *	
 *
 *
 *	Packed Quantization (SerializePackedVector)
 *		-Scaling factor (usually 10, 100, etc)
 *		-Max number of bits per component (this is maximum, not a constant)
 *
 *		The format is <num of bits per component> <N bits for X> <N bits for Y> <N bits for Z>
 *
 *		The advantages to this format are that packed nature. You may support large magnitudes and have as much precision as you want. All while
 *		having small magnitudes take less space.
 *
 *		The trade off is that there is overhead in serializing how many bits are used for each component, and byte alignment is almost always thrown
 *		off.
 *
*/

template<int32 ScaleFactor, int32 MaxBitsPerComponent>
bool WritePackedVector(FVector Value, FArchive& Ar)	// Note Value is intended to not be a reference since we are scaling it before serializing!
{
	check(Ar.IsSaving());

	// Nan Check
	if( Value.ContainsNaN() )
	{
		FVector	Dummy(0, 0, 0);
		WritePackedVector<ScaleFactor, MaxBitsPerComponent>(Dummy, Ar);
		return false;
	}

	// Scale vector by quant factor first
	Value *= ScaleFactor;

	// Do basically FVector::SerializeCompressed
	int32 IntX	= FMath::Round(Value.X);
	int32 IntY	= FMath::Round(Value.Y);
	int32 IntZ	= FMath::Round(Value.Z);
			
	uint32 Bits	= FMath::Clamp<uint32>( FMath::CeilLogTwo( 1 + FMath::Max3( FMath::Abs(IntX), FMath::Abs(IntY), FMath::Abs(IntZ) ) ), 1, MaxBitsPerComponent ) - 1;

	// Serialize how many bits each component will have
	Ar.SerializeInt( Bits, MaxBitsPerComponent );

	int32  Bias	= 1<<(Bits+1);
	uint32 Max	= 1<<(Bits+2);
	uint32 DX	= IntX + Bias;
	uint32 DY	= IntY + Bias;
	uint32 DZ	= IntZ + Bias;

	bool clamp=false;
	
	if (DX >= Max) { clamp=true; DX = static_cast<int32>(DX) > 0 ? Max-1 : 0; }
	if (DY >= Max) { clamp=true; DY = static_cast<int32>(DY) > 0 ? Max-1 : 0; }
	if (DZ >= Max) { clamp=true; DZ = static_cast<int32>(DZ) > 0 ? Max-1 : 0; }
	
	Ar.SerializeInt( DX, Max );
	Ar.SerializeInt( DY, Max );
	Ar.SerializeInt( DZ, Max );

	return !clamp;
}

template<uint32 ScaleFactor, int32 MaxBitsPerComponent>
bool ReadPackedVector(FVector &Value, FArchive& Ar)
{
	uint32 Bits	= 0;

	// Serialize how many bits each component will have
	Ar.SerializeInt( Bits, MaxBitsPerComponent );

	int32  Bias = 1<<(Bits+1);
	uint32 Max	= 1<<(Bits+2);
	uint32 DX	= 0;
	uint32 DY	= 0;
	uint32 DZ	= 0;
	
	Ar.SerializeInt( DX, Max );
	Ar.SerializeInt( DY, Max );
	Ar.SerializeInt( DZ, Max );
	
	
	float fact = (float)ScaleFactor;

	Value.X = (float)(static_cast<int32>(DX)-Bias) / fact;
	Value.Y = (float)(static_cast<int32>(DY)-Bias) / fact;
	Value.Z = (float)(static_cast<int32>(DZ)-Bias) / fact;

	return true;
}

// ScaleFactor is multiplied before send and divided by post receive. A higher ScaleFactor means more precision.
// MaxBitsPerComponent is the maximum number of bits to use per component. This is only a maximum. A header is
// writter (size = Log2 (MaxBitsPerComponent)) to indicate how many bits are actually used. 

template<uint32 ScaleFactor, int32 MaxBitsPerComponent>
bool SerializePackedVector(FVector &Vector, FArchive& Ar)
{
	if (Ar.IsSaving())
	{
		return  WritePackedVector<ScaleFactor, MaxBitsPerComponent>(Vector, Ar);
	}

	ReadPackedVector<ScaleFactor, MaxBitsPerComponent>(Vector, Ar);
	return true;
}

// --------------------------------------------------------------

template<int32 MaxValue, int32 NumBits>
bool WriteFixedCompressedFloat(const float Value, FArchive& Ar)
{
	// Note: enums are used in this function to force bit shifting to be done at compile time

														// NumBits = 8:
	enum { MaxBitValue	= (1 << (NumBits - 1)) - 1 };	//   0111 1111 - Max abs value we will serialize
	enum { Bias			= (1 << (NumBits - 1)) };		//   1000 0000 - Bias to pivot around (in order to support signed values)
	enum { SerIntMax	= (1 << (NumBits - 0)) };		// 1 0000 0000 - What we pass into SerializeInt
	enum { MaxDelta		= (1 << (NumBits - 0)) - 1 };	//   1111 1111 - Max delta is

	bool clamp = false;
	int32 ScaledValue;
	if ( MaxValue > MaxBitValue )
	{
		// We have to scale this down, scale needs to be a float:
		const float scale = (float)MaxBitValue / (float)MaxValue;
		ScaledValue = FMath::Trunc(scale * Value);
	}
	else
	{
		// We will scale up to get extra precision. But keep is a whole number preserve whole values
		enum { scale = MaxBitValue / MaxValue };
		ScaledValue = FMath::Round( scale * Value );
	}

	uint32 Delta = static_cast<uint32>(ScaledValue + Bias);

	if (Delta > MaxDelta)
	{
		clamp = true;
		Delta = static_cast<int32>(Delta) > 0 ? MaxDelta : 0;
	}

	Ar.SerializeInt( Delta, SerIntMax );

	return !clamp;
}

template<int32 MaxValue, int32 NumBits>
bool ReadFixedCompressedFloat(float &Value, FArchive& Ar)
{
	// Note: enums are used in this function to force bit shifting to be done at compile time

														// NumBits = 8:
	enum { MaxBitValue	= (1 << (NumBits - 1)) - 1 };	//   0111 1111 - Max abs value we will serialize
	enum { Bias			= (1 << (NumBits - 1)) };		//   1000 0000 - Bias to pivot around (in order to support signed values)
	enum { SerIntMax	= (1 << (NumBits - 0)) };		// 1 0000 0000 - What we pass into SerializeInt
	enum { MaxDelta		= (1 << (NumBits - 0)) - 1 };	//   1111 1111 - Max delta is
	
	uint32 Delta;
	Ar.SerializeInt(Delta, SerIntMax);
	float UnscaledValue = static_cast<float>( static_cast<int32>(Delta) - Bias );

	if ( MaxValue > MaxBitValue )
	{
		// We have to scale down, scale needs to be a float:
		const float InvScale = MaxValue / (float)MaxBitValue;
		Value = UnscaledValue * InvScale;
	}
	else
	{
		enum { scale = MaxBitValue / MaxValue };
		const float InvScale = 1.f / (float)scale;

		Value = UnscaledValue * InvScale;
	}

	return true;
}

// --------------------------------------------------------------
// MaxValue is the max abs value to serialize. If abs value of any vector components exceeds this, the serialized value will be clamped.
// NumBits is the total number of bits to use - this includes the sign bit!
//
// So passing in NumBits = 8, and MaxValue = 2^8, you will scale down to fit into 7 bits so you can leave 1 for the sign bit.
template<int32 MaxValue, int32 NumBits>
bool SerializeFixedVector(FVector &Vector, FArchive& Ar)
{
	if (Ar.IsSaving())
	{
		bool success = true;
		success &= WriteFixedCompressedFloat<MaxValue, NumBits>(Vector.X, Ar);
		success &= WriteFixedCompressedFloat<MaxValue, NumBits>(Vector.Y, Ar);
		success &= WriteFixedCompressedFloat<MaxValue, NumBits>(Vector.Z, Ar);
		return success;
	}

	ReadFixedCompressedFloat<MaxValue, NumBits>(Vector.X, Ar);
	ReadFixedCompressedFloat<MaxValue, NumBits>(Vector.Y, Ar);
	ReadFixedCompressedFloat<MaxValue, NumBits>(Vector.Z, Ar);
	return true;
}

// --------------------------------------------------------------

/**
 *	FVector_NetQuantize
 *
 *	0 decimal place of precision.
 *	Up to 20 bits per component.
 *	Valid range: 2^20 = +/- 1048576
 *
 *	Note: this is the historical UE format for vector net serialization
 *
 */
USTRUCT()
struct FVector_NetQuantize : public FVector
{
	GENERATED_USTRUCT_BODY()

	FORCEINLINE FVector_NetQuantize()
	{}

	explicit FORCEINLINE FVector_NetQuantize(EForceInit E)
	: FVector(E)
	{}

	FORCEINLINE FVector_NetQuantize( float InX, float InY, float InZ )
	: FVector(InX, InY, InX)
	{}

	FORCEINLINE FVector_NetQuantize(const FVector &InVec)
	{
		FVector::operator=(InVec);
	}

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
	{
		bOutSuccess = SerializePackedVector<1, 20>(*this, Ar);
		return true;
	}
};

template<>
struct TStructOpsTypeTraits< FVector_NetQuantize > : public TStructOpsTypeTraitsBase
{
	enum 
	{
		WithNetSerializer = true
	};
};

/**
 *	FVector_NetQuantize10
 *
 *	1 decimal place of precision.
 *	Up to 24 bits per component.
 *	Valid range: 2^24 / 10 = +/- 1677721.6
 *
 */
USTRUCT()
struct FVector_NetQuantize10 : public FVector
{
	GENERATED_USTRUCT_BODY()

	FORCEINLINE FVector_NetQuantize10()
	{}

	explicit FORCEINLINE FVector_NetQuantize10(EForceInit E)
	: FVector(E)
	{}

	FORCEINLINE FVector_NetQuantize10( float InX, float InY, float InZ )
	: FVector(InX, InY, InX)
	{}

	FORCEINLINE FVector_NetQuantize10(const FVector &InVec)
	{
		FVector::operator=(InVec);
	}

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
	{
		bOutSuccess = SerializePackedVector<10, 24>(*this, Ar);
		return true;
	}
};

template<>
struct TStructOpsTypeTraits< FVector_NetQuantize10 > : public TStructOpsTypeTraitsBase
{
	enum 
	{
		WithNetSerializer = true
	};
};

/**
 *	FVector_NetQuantize100
 *
 *	3 decimal place of precision.
 *	Up to 31 bits per component.
 *	Valid range: 2^31 / 1000 = +/- 2147483.648
 *
 */
USTRUCT()
struct FVector_NetQuantize100 : public FVector
{
	GENERATED_USTRUCT_BODY()

	FORCEINLINE FVector_NetQuantize100()
	{}

	explicit FORCEINLINE FVector_NetQuantize100(EForceInit E)
	: FVector(E)
	{}

	FORCEINLINE FVector_NetQuantize100( float InX, float InY, float InZ )
	: FVector(InX, InY, InX)
	{}

	FORCEINLINE FVector_NetQuantize100(const FVector &InVec)
	{
		FVector::operator=(InVec);
	}

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
	{
		bOutSuccess = SerializePackedVector<1000, 31>(*this, Ar);
		return true;
	}
};

template<>
struct TStructOpsTypeTraits< FVector_NetQuantize100 > : public TStructOpsTypeTraitsBase
{
	enum 
	{
		WithNetSerializer = true
	};
};

/**
 *	FVector_NetQuantizeNormal
 *
 *	16 bits per component
 *	Valid range: [-1..+1] (inclusive)
 */
USTRUCT()
struct FVector_NetQuantizeNormal : public FVector
{
	GENERATED_USTRUCT_BODY()

	FORCEINLINE FVector_NetQuantizeNormal()
	{}

	explicit FORCEINLINE FVector_NetQuantizeNormal(EForceInit E)
	: FVector(E)
	{}

	FORCEINLINE FVector_NetQuantizeNormal( float InX, float InY, float InZ )
	: FVector(InX, InY, InX)
	{}

	FORCEINLINE FVector_NetQuantizeNormal(const FVector &InVec)
	{
		FVector::operator=(InVec);
	}

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
	{
		bOutSuccess = SerializeFixedVector<1, 16>(*this, Ar);
		return true;
	}
};

template<>
struct TStructOpsTypeTraits< FVector_NetQuantizeNormal > : public TStructOpsTypeTraitsBase
{
	enum 
	{
		WithNetSerializer = true
	};
};

// --------------------------------------------------------------