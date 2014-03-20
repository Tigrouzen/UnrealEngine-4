// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MessageEndpointConfigurator.h: Declares the FMessageEndpointConfigurator class.
=============================================================================*/

#pragma once


/**
 * Implements a message endpoint builder.
 */
struct FMessageEndpointBuilder
{
public:

	/**
	 * Creates and initializes a new builder using the default message bus.
	 *
	 * WARNING: This constructor must be called from the main thread.
	 *
	 * @param InName - The endpoint's name (for debugging purposes).
	 * @param InBus - The message bus to attach the endpoint to.
	 */
	FMessageEndpointBuilder( const FName& InName )
		: BusPtr(IMessagingModule::Get().GetDefaultBus())
		, Disabled(false)
		, InboxEnabled(false)
		, Name(InName)
		, RecipientThread(FTaskGraphInterface::Get().GetCurrentThreadIfKnown())
	{ }

	/**
	 * Creates and initializes a new builder using the specified message bus.
	 *
	 * @param InName - The endpoint's name (for debugging purposes).
	 * @param InBus - The message bus to attach the endpoint to.
	 */
	FMessageEndpointBuilder( const FName& InName, const IMessageBusRef& InBus )
		: BusPtr(InBus)
		, Disabled(false)
		, InboxEnabled(false)
		, Name(InName)
		, RecipientThread(FTaskGraphInterface::Get().GetCurrentThreadIfKnown())
	{ }

public:

	/**
	 * Adds a message handler for the given type of messages.
	 *
	 * @param HandlerType - The type of the object handling the messages.
	 * @param MessageType - The type of messages to handle.
	 * @param Handler - The class handling the messages.
	 * @param HandlerFunc - The class function handling the messages.
	 * @param ReceivingThread - The thread on which to handle the message.
	 *
	 * @return This instance (for method chaining).
	 */
	template<typename MessageType, typename HandlerType>
	FMessageEndpointBuilder& Handling( HandlerType* Handler, typename TMessageHandlerFunc<MessageType, HandlerType>::Type HandlerFunc )
	{
		// @todo gmp: implement proper async message deserialization, so this can be removed
		checkAtCompileTime(TStructOpsTypeTraits<MessageType>::WithMessageHandling == true, Please_Add_A_WithMessageHandling_TypeTrait);

		Handlers.Add(MakeShareable(new TMessageHandler<MessageType, HandlerType>(Handler, HandlerFunc)));

		return *this;
	}

	/**
	 * Configures the endpoint to receive messages on any thread.
	 *
	 * By default, the builder initializes the message endpoint to receive on the
	 * current thread. Use this method to receive on any available thread instead.
	 *
	 * ThreadAny is the fastest way to receive messages. It should be used if the receiving
	 * code is completely thread-safe and if it is sufficiently fast. ThreadAny MUST NOT
	 * be used if the receiving code is not thread-safe. It also SHOULD NOT be used if the
	 * code includes time consuming operations, because it will block the message router,
	 * causing no other messages to be delivered in the meantime.
	 *
	 * @return This instance (for method chaining).
	 *
	 * @ReceivingOnThread
	 */
	FMessageEndpointBuilder& ReceivingOnAnyThread( )
	{
		RecipientThread = ENamedThreads::AnyThread;

		return *this;
	}

	/**
	 * Configured the endpoint to receive messages on a specific thread.
	 *
	 * By default, the builder initializes the message endpoint to receive on the
	 * current thread. Use this method to receive on a different thread instead.
	 *
	 * Also see the additional notes for ReceivingOnAnyThread().
	 *
	 * @param NamedThread - The name of the thread to receive messages on.
	 *
	 * @return This instance (for method chaining).
	 *
	 * @seeReceivingOnAnyThread
	 */
	FMessageEndpointBuilder& ReceivingOnThread( ENamedThreads::Type NamedThread )
	{
		RecipientThread = NamedThread;

		return *this;
	}

	/**
	 * Disables the endpoint.
	 *
	 * @return This instance (for method chaining).
	 */
	FMessageEndpointBuilder& ThatIsDisabled( )
	{
		Disabled = true;

		return *this;
	}

	/**
	 * Registers a message handler with the endpoint.
	 *
	 * @param Handler - The handler to add.
	 *
	 * @return This instance (for method chaining).
	 */
	FMessageEndpointBuilder& WithHandler( const IMessageHandlerRef& Handler )
	{
		Handlers.Add(Handler);

		return *this;
	}

	/**
	 * Enables the endpoint's message inbox.
	 *
	 * The inbox is disabled by default.
	 *
	 * @return This instance (for method chaining).
	 */
	FMessageEndpointBuilder& WithInbox( )
	{
		InboxEnabled = true;

		return *this;
	}

public:

	/**
	 * Builds the message endpoint as configured.
	 *
	 * @return A new message endpoint, or NULL if it couldn't be built.
	 */
	FMessageEndpointPtr Build( )
	{
		FMessageEndpointPtr Endpoint;

		IMessageBusPtr Bus = BusPtr.Pin();
		
		if (Bus.IsValid())
		{
			Endpoint = MakeShareable(new FMessageEndpoint(Name, Bus.ToSharedRef(), Handlers));
			Bus->Register(Endpoint->GetAddress(), Endpoint.ToSharedRef());

			if (Disabled)
			{
				Endpoint->Disable();
			}

			if (InboxEnabled)
			{
				Endpoint->EnableInbox();
				Endpoint->SetRecipientThread(ENamedThreads::AnyThread);
			}
			else
			{
				Endpoint->SetRecipientThread(RecipientThread);
			}
		}

		return Endpoint;
	}

	/**
	 * Implicit conversion operator to build the message endpoint as configured.
	 *
	 * @return The message endpoint.
	 */
	operator FMessageEndpointPtr( )
	{
		return Build();
	}
	
private:

	// Holds a reference to the message bus to attach to.
	IMessageBusWeakPtr BusPtr;

	// Holds a flag indicating whether the endpoint should be disabled.
	bool Disabled;

	// Holds the collection of message handlers to register.
	TArray<IMessageHandlerPtr> Handlers;

	// Holds a flag indicating whether the inbox should be enabled.
	bool InboxEnabled;

	// Holds the endpoint's name (for debugging purposes).
	FName Name;

	// Holds the name of the thread on which to receive messages.
	ENamedThreads::Type RecipientThread;
};
