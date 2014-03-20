// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace NetworkProfiler
{
	/**
	 * Encapsulates one or more frames worth of tokens together with a summary.
	 */
	class PartialNetworkStream
	{
		// Time/ frames

		/** Normalized start time of partial stream. */
		public float StartTime = -1;
		/** Normalized end time of partial stream. */
		public float EndTime = 0;
		/** Number of frames this summary covers. */
		public int NumFrames = 0;
		/** Number of events in this frame. */
		public int NumEvents = 0;

		// Actor/ Property replication
		
		/** Number of actors in this partial stream. 
		 * NOTE - This used to be the number of actors replicating properties
		*/
		public int ActorCount = 0;

		/** Total actor replication time in ms. */
		public float ActorReplicateTimeInMS = 0.0f;

		/** Number of properties replicated. */
		public int PropertyCount = 0;
		/** Total size of properties replicated. */
		public int ReplicatedSizeBits = 0;
		
		// RPC

		/** Number of RPCs replicated. */
		public int RPCCount = 0;
		/** Total size of RPC replication. */
		public int RPCSizeBits = 0;

		// SendBunch

		/** Number of times SendBunch was called. */
		public int SendBunchCount = 0;
		/** Total size of bytes sent. */
		public int SendBunchSizeBits = 0;		
		/** Call count per channel type. */
		public int[] SendBunchCountPerChannel = Enumerable.Repeat(0, (int)EChannelTypes.Max).ToArray();
		/** Size per channel type */
		public int[] SendBunchSizeBitsPerChannel = Enumerable.Repeat(0, (int)EChannelTypes.Max).ToArray();

		// Low level socket

		/** Number of low level socket sends on "Unreal" socket type. */
		public int UnrealSocketCount = 0;
		/** Total size of bytes sent on "Unreal" socket type. */
		public int UnrealSocketSize = 0;
		/** Number of low level socket sends on non-"Unreal" socket types. */
		public int OtherSocketCount = 0;
		/** Total size of bytes sent on non-"Unreal" socket type. */
		public int OtherSocketSize = 0;

		// Detailed information.

		/** List of all tokens in this substream. */
		protected List<TokenBase> Tokens = new List<TokenBase>();
		
		// Cached internal data.

		/** Delta time of first frame. Passed to constructor as we can't calculate it. */
		float FirstFrameDeltaTime = 0;
		/** Index of "Unreal" name in network stream name table. */
		int NameIndexUnreal = -1;

		/**
		 * Constructor, initializing this substream based on network tokens. 
		 * 
		 * @param	InTokens			Tokens to build partial stream from
		 * @param	InNameIndexUnreal	Index of "Unreal" name, used as optimization
		 * @param	InDeltaTime			DeltaTime of first frame as we can't calculate it
		 */
		public PartialNetworkStream( List<TokenBase> InTokens, int InNameIndexUnreal, float InDeltaTime )
		{
			NameIndexUnreal = InNameIndexUnreal;
			FirstFrameDeltaTime = InDeltaTime;

			// Populate tokens array, weeding out unwanted ones.
			foreach( TokenBase Token in InTokens )
			{
				// Don't add tokens that don't have any replicated properties.
				// NOTE - We now allow properties that didn't replicate anything so we can show performance stats
				//if( (Token.TokenType != ETokenTypes.ReplicateActor) || (((TokenReplicateActor) Token).Properties.Count > 0) )
				{
					Tokens.Add(Token);
				}
			}

			CreateSummary( NameIndexUnreal, FirstFrameDeltaTime, "", "", "" );
		}

		/**
		 * Constructor, initializing this substream based on other substreams. 
		 * @param	Streams				Array of CONSECUTIVE partial streams (frames) to combine
		 * @param	InNameIndexUnreal	Index of "Unreal" name, used as optimization
		 * @param	InDeltaTime			DeltaTime of first frame as we can't calculate it 
		 */
		public PartialNetworkStream( List<PartialNetworkStream> Streams, int InNameIndexUnreal, float InDeltaTime )
		{
			NameIndexUnreal = InNameIndexUnreal;
			FirstFrameDeltaTime = InDeltaTime;

			// Merge tokens from passed in streams.
			foreach( PartialNetworkStream PartialNetworkStream in Streams )
			{
				Tokens.AddRange( PartialNetworkStream.Tokens );
			}

			CreateSummary( NameIndexUnreal, FirstFrameDeltaTime, "", "", "" );
		}

		/**
		 * Constructor, duplicating the passed in stream while applying the passed in filters.
		 * 
		 * @param	InStream		Stream to duplicate
		 * @param	ActorFilter		Actor filter to match against
		 * @param	PropertyFilter	Property filter to match against
		 * @param	RPCFilter		RPC filter to match against
		 */
		public PartialNetworkStream( PartialNetworkStream InStream, string InActorFilter, string InPropertyFilter, string InRPCFilter )
		{
			NameIndexUnreal = InStream.NameIndexUnreal;
			FirstFrameDeltaTime = InStream.FirstFrameDeltaTime;

			// Merge tokens from passed in stream based on filter criteria.
			foreach( var Token in InStream.Tokens )
			{
				if( Token.MatchesFilters( InActorFilter, InPropertyFilter, InRPCFilter ) )
				{
					Tokens.Add( Token );
				}
			}
			CreateSummary( NameIndexUnreal, FirstFrameDeltaTime, InActorFilter, InPropertyFilter, InRPCFilter );
		}

		/**
		 * Filters based on the passed in filters and returns a new partial network stream.
		 * 
		 * @param	ActorFilter		Actor filter to match against
		 * @param	PropertyFilter	Property filter to match against
		 * @param	RPCFilter		RPC filter to match against
		 * 
		 * @return	new filtered network stream
		 */
		public PartialNetworkStream Filter( string ActorFilter, string PropertyFilter, string RPCFilter )
		{
			return new PartialNetworkStream( this, ActorFilter, PropertyFilter, RPCFilter );
		}

		/**
		 * Parses tokens to create summary.	
		 */
		protected void CreateSummary( int NameIndexUnreal, float DeltaTime, string ActorFilter, string PropertyFilter, string RPCFilter )
		{
			foreach( TokenBase Token in Tokens )
			{
				switch( Token.TokenType )
				{
					case ETokenTypes.FrameMarker:
						var TokenFrameMarker = (TokenFrameMarker) Token;
						if( StartTime < 0 )
						{
							StartTime = TokenFrameMarker.RelativeTime;
							EndTime = TokenFrameMarker.RelativeTime;
						}
						else
						{
							EndTime = TokenFrameMarker.RelativeTime;
						}
						NumFrames++;
						break;
					case ETokenTypes.SocketSendTo:
						var TokenSocketSendTo = (TokenSocketSendTo) Token;
						// Unreal game socket
						if( TokenSocketSendTo.SocketNameIndex == NameIndexUnreal )
						{
							UnrealSocketCount++;
							UnrealSocketSize += TokenSocketSendTo.BytesSent;
						}
						else
						{
							OtherSocketCount++;
							OtherSocketSize += TokenSocketSendTo.BytesSent;
						}
						break;
					case ETokenTypes.SendBunch:
						var TokenSendBunch = (TokenSendBunch) Token;
						SendBunchCount++;
						SendBunchSizeBits += TokenSendBunch.NumBits;
						SendBunchCountPerChannel[TokenSendBunch.ChannelType]++;
						SendBunchSizeBitsPerChannel[TokenSendBunch.ChannelType] += TokenSendBunch.NumBits;
						break;
					case ETokenTypes.SendRPC:
						var TokenSendRPC = (TokenSendRPC) Token;
						RPCCount++;
						RPCSizeBits += TokenSendRPC.NumBits;
						break;
					case ETokenTypes.ReplicateActor:
						var TokenReplicateActor = (TokenReplicateActor) Token;
						ActorCount++;
                        ActorReplicateTimeInMS += TokenReplicateActor.TimeInMS;

						foreach( var Property in TokenReplicateActor.Properties )
						{
							if( Property.MatchesFilters( ActorFilter, PropertyFilter, RPCFilter ) )
							{
								PropertyCount++;
								ReplicatedSizeBits += Property.NumBits;
							}
						}
						break;
					case ETokenTypes.Event:
						NumEvents++;
						break;
					case ETokenTypes.RawSocketData:
						break;
					default:
						throw new System.IO.InvalidDataException();
				}
			}

			EndTime += DeltaTime;
		}

		/**
		 * Dumps detailed token into into string array and returns it.
		 *
		 * @return	Array of strings with detailed token descriptions
		 */
		public string[] ToDetailedStringArray( string ActorFilter, string PropertyFilter, string RPCFilter )
		{			
			var Details = new List<string>();
			foreach( TokenBase Token in Tokens )
			{
				Details.AddRange( Token.ToDetailedStringList( ActorFilter, PropertyFilter, RPCFilter ) );
			}
			return Details.ToArray();
		}

		/**
		 * Dumps actor/property tokens into a tree view for viewing performance timings
		 */
		public void ToActorPerformanceView( NetworkStream NetworkStream, TreeView TreeView, string ActorFilter, string PropertyFilter, string RPCFilter )
        {
			UniqueItemTracker<UniqueActor, TokenReplicateActor> UniqueActors = new UniqueItemTracker<UniqueActor, TokenReplicateActor>();
			foreach (TokenBase Token in Tokens)
			{
				TokenReplicateActor ActorToken = Token as TokenReplicateActor;

				if (ActorToken == null)
				{
					continue;
				}

				UniqueActors.AddItem(ActorToken, NetworkStream.GetClassNameIndex(ActorToken.ActorNameIndex));
			}

			var ActorDetailList = UniqueActors.UniqueItems.OrderByDescending(s => s.Value.TimeInMS).ToList();

			TreeView.Nodes.Clear();

			foreach (var UniqueActor in ActorDetailList)
			{
				long NumPotentialActorBytes = ( UniqueActor.Value.PotentialSizeBits + 7 ) / 8;
				long NumActorBytes = ( UniqueActor.Value.SizeBits + 7 ) / 8;
				string ActorStr = string.Format( "{0,-32} : {1:0.00} ({2:00}/{3:00}) ({4:00})", NetworkStream.GetName( UniqueActor.Key ), UniqueActor.Value.TimeInMS, NumPotentialActorBytes, NumActorBytes, UniqueActor.Value.Count );
				TreeView.Nodes.Add(ActorStr);

				var PropertyDetailList = UniqueActor.Value.Properties.UniqueItems.OrderByDescending(s => s.Value.TimeInMS).ToList();

				foreach (var Property in PropertyDetailList)
				{
					long NumPotentialPropBytes = ( Property.Value.PotentialSizeBits + 7 ) / 8;
					long NumPropBytes = ( Property.Value.SizeBits + 7 ) / 8;

					string PropName = NetworkStream.GetName( Property.Key );
					String FlagsStr = Property.Value.FirstItem.GenerateFlagsString();

					string PropStr = string.Format( "{0,-22} {1,-8} : {2:0.00} ({3:00}/{4:00}) ({5:00})", PropName, FlagsStr, Property.Value.TimeInMS, NumPotentialPropBytes, NumPropBytes, Property.Value.Count );
					TreeView.Nodes[TreeView.Nodes.Count - 1].Nodes.Add(PropStr);
				}
			}
        }
        
        /**
         * Converts the passed in number of bytes to a string formatted as Bytes, KByte, MByte depending
         * on magnitude.
         * 
         * @param	SizeInBytes		Size in bytes to conver to formatted string
         * 
         * @return string representation of value, either Bytes, KByte or MByte
         */ 
		public string ConvertToSizeString( float SizeInBytes )
		{
			// Format as MByte if size > 1 MByte.
			if( SizeInBytes > 1024 * 1024 )
			{
				return (SizeInBytes / 1024 / 1024).ToString("0.0").PadLeft(8) + " MByte";
			}
			// Format as KByte if size is > 1 KByte and <= 1 MByte
			else if( SizeInBytes > 1024 )
			{
				return (SizeInBytes / 1024).ToString("0.0").PadLeft(8) + " KByte";
			}
			// Format as Byte if size is <= 1 KByte
			else
			{
				return SizeInBytes.ToString("0.0").PadLeft(8) + " Bytes";
			}
		}

		/**
		 * Converts passed in value to string with appropriate formatting and padding
		 * 
		 * @param	Count	Value to convert
		 * @return	string reprentation with sufficient padding
		 */
		public string ConvertToCountString( float Count )
		{
			return Count.ToString("0.0").PadLeft(8);
		}

		/**
		 * Converts the summary to a human readable array of strings.
		 */		
		public string[] ToStringArray()
		{
			string FormatString =	"Data Summary^"						+
									"^"									+
									"Frame Count            : {0}^"		+
									"Duration (ms)          : {1}^"		+
									"Duration (sec)         : {2}^"		+
									"^"									+
									"^"									+
									"Network Summary^"					+
									"^"									+
									"Actor Count            : {3}^"		+
									"Property Count         : {4}^"		+
									"Replicated Size        : {5}^"		+
									"RPC Count              : {6}^"		+
									"RPC Size               : {7}^"		+
									"SendBunch Count        : {8}^"		+
									"   Control             : {9}^"		+
									"   Actor               : {10}^"	+
									"   File                : {11}^"	+
									"   Voice               : {12}^"	+
									"SendBunch Size         : {13}^"	+
									"   Control             : {14}^"	+
									"   Actor               : {15}^"	+
									"   File                : {16}^"	+
									"   Voice               : {17}^"	+
									"Game Socket Send Count : {18}^"	+
									"Game Socket Send Size  : {19}^"	+
									"Misc Socket Send Count : {20}^"	+
									"Misc Socket Send Size  : {21}^"	+
									"Outgoing bandwidth     : {22}^"	+
									"^"									+
									"^"									+
									"Network Summary per second^"		+
									"^"									+
									"Actor Count            : {23}^"	+
									"Property Count         : {24}^"	+
									"Replicated Size        : {25}^"	+
									"RPC Count              : {26}^"	+
									"RPC Size               : {27}^"	+
									"SendBunch Count        : {28}^"	+
									"   Control             : {29}^"	+
									"   Actor               : {30}^"	+
									"   File                : {31}^"	+
									"   Voice               : {32}^"	+
									"SendBunch Size         : {33}^"	+
									"   Control             : {34}^"	+
									"   Actor               : {35}^"	+
									"   File                : {36}^"	+
									"   Voice               : {37}^"	+
									"Game Socket Send Count : {38}^"	+
									"Game Socket Send Size  : {39}^"	+
									"Misc Socket Send Count : {40}^"	+
									"Misc Socket Send Size  : {41}^"	+
									"Outgoing bandwidth     : {42}^";

			float OneOverDeltaTime = 1 / (EndTime - StartTime);

			string Summary = String.Format( FormatString, 
									ConvertToCountString(NumFrames),
									ConvertToCountString((EndTime - StartTime) * 1000),
									ConvertToCountString((EndTime - StartTime)),
									ConvertToCountString(ActorCount),
									ConvertToCountString(PropertyCount),
									ConvertToSizeString(ReplicatedSizeBits / 8.0f),
									ConvertToCountString(RPCCount),
									ConvertToSizeString(RPCSizeBits / 8.0f),
									ConvertToCountString(SendBunchCount),
									ConvertToCountString(SendBunchCountPerChannel[(int)EChannelTypes.Control]),
									ConvertToCountString(SendBunchCountPerChannel[(int)EChannelTypes.Actor]),
									ConvertToCountString(SendBunchCountPerChannel[(int)EChannelTypes.File]),
									ConvertToCountString(SendBunchCountPerChannel[(int)EChannelTypes.Voice]),
									ConvertToSizeString(SendBunchSizeBits / 8.0f),
									ConvertToSizeString(SendBunchSizeBitsPerChannel[(int)EChannelTypes.Control] / 8.0f),
									ConvertToSizeString(SendBunchSizeBitsPerChannel[(int)EChannelTypes.Actor] / 8.0f),
									ConvertToSizeString(SendBunchSizeBitsPerChannel[(int)EChannelTypes.File] / 8.0f),
									ConvertToSizeString(SendBunchSizeBitsPerChannel[(int)EChannelTypes.Voice] / 8.0f),
									ConvertToCountString(UnrealSocketCount),
									ConvertToSizeString(UnrealSocketSize),
									ConvertToCountString(OtherSocketCount),
									ConvertToSizeString(OtherSocketSize),
									ConvertToSizeString(UnrealSocketSize + OtherSocketSize + NetworkStream.PacketOverhead * (UnrealSocketCount + OtherSocketCount)),
									ConvertToCountString(ActorCount * OneOverDeltaTime),
									ConvertToCountString(PropertyCount * OneOverDeltaTime),
									ConvertToSizeString(ReplicatedSizeBits / 8.0f * OneOverDeltaTime),
									ConvertToCountString(RPCCount * OneOverDeltaTime),
									ConvertToSizeString(RPCSizeBits / 8.0f * OneOverDeltaTime),
									ConvertToCountString(SendBunchCount * OneOverDeltaTime),
									ConvertToCountString(SendBunchCountPerChannel[(int)EChannelTypes.Control] * OneOverDeltaTime),
									ConvertToCountString(SendBunchCountPerChannel[(int)EChannelTypes.Actor] * OneOverDeltaTime),
									ConvertToCountString(SendBunchCountPerChannel[(int)EChannelTypes.File] * OneOverDeltaTime),
									ConvertToCountString(SendBunchCountPerChannel[(int)EChannelTypes.Voice] * OneOverDeltaTime),
									ConvertToSizeString(SendBunchSizeBits / 8.0f * OneOverDeltaTime),
									ConvertToSizeString(SendBunchSizeBitsPerChannel[(int)EChannelTypes.Control] / 8.0f * OneOverDeltaTime),
									ConvertToSizeString(SendBunchSizeBitsPerChannel[(int)EChannelTypes.Actor] / 8.0f * OneOverDeltaTime),
									ConvertToSizeString(SendBunchSizeBitsPerChannel[(int)EChannelTypes.File] / 8.0f * OneOverDeltaTime),
									ConvertToSizeString(SendBunchSizeBitsPerChannel[(int)EChannelTypes.Voice] / 8.0f * OneOverDeltaTime),
									ConvertToCountString(UnrealSocketCount * OneOverDeltaTime),
									ConvertToSizeString(UnrealSocketSize * OneOverDeltaTime),
									ConvertToCountString(OtherSocketCount * OneOverDeltaTime),
									ConvertToSizeString(OtherSocketSize * OneOverDeltaTime),
									ConvertToSizeString((UnrealSocketSize + OtherSocketSize + NetworkStream.PacketOverhead * (UnrealSocketCount + OtherSocketCount)) * OneOverDeltaTime)
									);

			return Summary.Split('^');
		}
	}

	/**
	 * UniqueItem
	 * Abstract base class for items stored in a UniqueItemTracker class
	 * Holds a unique item, and tracks references to the non unique items that are of the same type
	*/
	class UniqueItem<T> where T : class
	{
		public int	Count		= 0;		// Number of non unique items
		public T	FirstItem	= null;

		public virtual void OnItemAdded( T Item )
		{
			if ( FirstItem == null )
			{
				FirstItem = Item;
			}
			Count++;
		}
	}

	/**
	 * UniqueItemTracker
	 * Helps consolidate a list of items into a unique list
	 * UniqueItems track reference to the non unique items of the same type
	*/
	class UniqueItemTracker<UniqueT, ItemT>
		where ItemT : class
		where UniqueT : UniqueItem<ItemT>, new()
	{
		public Dictionary<int, UniqueT> UniqueItems = new Dictionary<int, UniqueT>();

		public void AddItem(ItemT Item, int Key)
		{
			if ( !UniqueItems.ContainsKey(Key) )
			{
				UniqueItems.Add(Key, new UniqueT());
			}

			UniqueItems[Key].OnItemAdded( Item );
		}
	}

	/**
	 * UniqueProperty
	 * Tracks all the unique properties of the same type
	*/
	class UniqueProperty : UniqueItem<TokenReplicateProperty>
	{
		public float	TimeInMS = 0;
		public long		PotentialSizeBits = 0;
		public long		SizeBits = 0;

		override public void OnItemAdded(TokenReplicateProperty Item)
		{
			base.OnItemAdded(Item);
			TimeInMS += Item.TimeInMS;
			PotentialSizeBits += Item.NumPotentialBits;
			SizeBits += Item.NumBits;
		}
	}

	/**
	 * UniqueActor
	 * Tracks all the unique actors of the same type
	*/
	class UniqueActor : UniqueItem<TokenReplicateActor>
    {
		public long		PotentialSizeBits = 0;
		public long		SizeBits = 0;
        public float	TimeInMS = 0;

		public UniqueItemTracker<UniqueProperty, TokenReplicateProperty> Properties = new UniqueItemTracker<UniqueProperty, TokenReplicateProperty>();

		override public void OnItemAdded(TokenReplicateActor Item)
		{
			base.OnItemAdded(Item);
			TimeInMS += Item.TimeInMS;

			foreach (var Property in Item.Properties)
			{
				SizeBits += Property.NumBits;
				PotentialSizeBits += Property.NumPotentialBits;
				Properties.AddItem(Property, Property.PropertyNameIndex);
			}
		}
	}
}
