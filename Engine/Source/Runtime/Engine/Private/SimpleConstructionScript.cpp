// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "BlueprintUtilities.h"
#if WITH_EDITOR
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/Kismet2NameValidators.h"
#endif

//////////////////////////////////////////////////////////////////////////
// USimpleConstructionScript

#if WITH_EDITOR
const FName USimpleConstructionScript::DefaultSceneRootVariableName = FName(TEXT("DefaultSceneRoot"));
#endif

USimpleConstructionScript::USimpleConstructionScript(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	RootNode_DEPRECATED = NULL;
	DefaultSceneRootNode = NULL;

#if WITH_EDITOR
	bIsConstructingEditorComponents = false;
#endif

	// Don't create a default scene root for the CDO and defer it for objects about to be loaded so we don't conflict with existing nodes
	if(!HasAnyFlags(RF_ClassDefaultObject|RF_NeedLoad))
	{
		ValidateSceneRootNodes();
	}
}

void USimpleConstructionScript::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if(Ar.IsLoading())
	{
		if(Ar.UE4Ver() < VER_UE4_REMOVE_NATIVE_COMPONENTS_FROM_BLUEPRINT_SCS)
		{
			int32 NodeIndex;

			// If we previously had a root node, we need to move it into the new RootNodes array. This is done in Serialize() in order to support SCS preloading (which relies on a valid RootNodes array).
			if(RootNode_DEPRECATED != NULL)
			{
				// Ensure it's been loaded so that its properties are valid
				if(RootNode_DEPRECATED->HasAnyFlags(RF_NeedLoad))
				{
					RootNode_DEPRECATED->GetLinker()->Preload(RootNode_DEPRECATED);
				}

				// If the root node was not native
				if(!RootNode_DEPRECATED->bIsNative_DEPRECATED)
				{
					// Add the node to the root set
					RootNodes.Add(RootNode_DEPRECATED);
				}
				else
				{
					// For each child of the previously-native root node
					for (NodeIndex=0; NodeIndex < RootNode_DEPRECATED->ChildNodes.Num(); ++NodeIndex)
					{
						USCS_Node* Node = RootNode_DEPRECATED->ChildNodes[NodeIndex];
						if(Node != NULL)
						{
							// Ensure it's been loaded (may not have been yet if we're preloading the SCS)
							if(Node->HasAnyFlags(RF_NeedLoad))
							{
								Node->GetLinker()->Preload(Node);
							}

							// We only care about non-native child nodes (non-native nodes could only be attached to the root node in the previous version, so we don't need to examine native child nodes)
							if(!Node->bIsNative_DEPRECATED)
							{
								// Add the node to the root set
								RootNodes.Add(Node);

								// Set the previously-native root node as its parent component
								Node->bIsParentComponentNative = true;
								Node->ParentComponentOrVariableName = RootNode_DEPRECATED->NativeComponentName_DEPRECATED;
							}
						}
					}
				}

				// Clear the deprecated reference
				RootNode_DEPRECATED = NULL;
			}

			// Add any user-defined actor components to the root set
			for(NodeIndex = 0; NodeIndex < ActorComponentNodes_DEPRECATED.Num(); ++NodeIndex)
			{
				USCS_Node* Node = ActorComponentNodes_DEPRECATED[NodeIndex];
				if(Node != NULL)
				{
					// Ensure it's been loaded (may not have been yet if we're preloading the SCS)
					if(Node->HasAnyFlags(RF_NeedLoad))
					{
						Node->GetLinker()->Preload(Node);
					}

					if(!Node->bIsNative_DEPRECATED)
					{
						RootNodes.Add(Node);
					}
				}
			}

			// Clear the deprecated ActorComponent list
			ActorComponentNodes_DEPRECATED.Empty();
		}
	}
}

void USimpleConstructionScript::PostLoad()
{
	Super::PostLoad();

	int32 NodeIndex;
	TArray<USCS_Node*> Nodes = GetAllNodes();

	if (GetLinkerUE4Version() < VER_UE4_REMOVE_INPUT_COMPONENTS_FROM_BLUEPRINTS)
	{
		for (NodeIndex=0; NodeIndex < Nodes.Num(); ++NodeIndex)
		{
			USCS_Node* Node = Nodes[NodeIndex];
			if (!Node->bIsNative_DEPRECATED && Node->ComponentTemplate->IsA<UInputComponent>())
			{
				RemoveNodeAndPromoteChildren(Node);
			}
		}
	}

#if WITH_EDITOR
	// Get the Blueprint that owns the SCS
	UBlueprint* Blueprint = GetBlueprint();
	check(Blueprint != NULL);

	// Fix up any uninitialized category names
	for (NodeIndex=0; NodeIndex < Nodes.Num(); ++NodeIndex)
	{
		USCS_Node* Node = Nodes[NodeIndex];
		if(Node->CategoryName == NAME_None)
		{
			Node->CategoryName = TEXT("Default");
		}
	}
#endif // WITH_EDITOR
	// Fix up native/inherited parent attachments, in case anything has changed
	FixupRootNodeParentReferences();

	// Ensure that we have a valid scene root
	ValidateSceneRootNodes();
}

void USimpleConstructionScript::FixupRootNodeParentReferences()
{
	// Get the BlueprintGeneratedClass that owns the SCS
	UClass* BPGeneratedClass = GetOwnerClass();
	if(BPGeneratedClass == NULL)
	{
		UE_LOG(LogBlueprint, Warning, TEXT("USimpleConstructionScript::FixupRootNodeParentReferences() - owner class is NULL; skipping."));

		// Cannot do fixup without a BPGC
		return;
	}

	for (int32 NodeIndex=0; NodeIndex < RootNodes.Num(); ++NodeIndex)
	{
		// If this root node is parented to a native/inherited component template
		USCS_Node* RootNode = RootNodes[NodeIndex];
		if(RootNode->ParentComponentOrVariableName != NAME_None)
		{
			bool bWasFound = false;

			// If the node is parented to a native component
			if(RootNode->bIsParentComponentNative)
			{
				// Get the Blueprint class default object
				AActor* CDO = Cast<AActor>(BPGeneratedClass->GetDefaultObject(false));
				if(CDO != NULL)
				{
					// Look for the parent component in the CDO's components array
					TArray<UActorComponent*> Components;
					CDO->GetComponents(Components);

					for (auto CompIter = Components.CreateConstIterator(); CompIter && !bWasFound; ++CompIter)
					{
						UActorComponent* ComponentTemplate = *CompIter;
						bWasFound = ComponentTemplate->GetFName() == RootNode->ParentComponentOrVariableName;
					}
				}
				else 
				{ 
					// SCS and BGClass depends on each other (while their construction).
					// Class is not ready, so one have to break the dependency circle.
					continue;
				}
			}
			// Otherwise the node is parented to an inherited SCS node from a parent Blueprint
			else
			{
				// Get the Blueprint hierarchy
				TArray<const UBlueprintGeneratedClass*> ParentBPClassStack;
				const bool bErrorFree = UBlueprintGeneratedClass::GetGeneratedClassesHierarchy(BPGeneratedClass, ParentBPClassStack);

				// Find the parent Blueprint in the hierarchy
				for(int32 StackIndex = ParentBPClassStack.Num() - 1; StackIndex > 0; --StackIndex)
				{
					auto ParentClass = ParentBPClassStack[StackIndex];
					if( ParentClass != NULL
						&& ParentClass->SimpleConstructionScript != NULL
						&& ParentClass->GetFName() == RootNode->ParentComponentOwnerClassName)
					{
						// Attempt to locate a match by searching all the nodes that belong to the parent Blueprint's SCS
						TArray<USCS_Node*> ParentNodes = ParentClass->SimpleConstructionScript->GetAllNodes();
						for (int32 ParentNodeIndex=0; ParentNodeIndex < ParentNodes.Num() && !bWasFound; ++ParentNodeIndex)
						{
							USCS_Node* ParentNode = ParentNodes[ParentNodeIndex];
							bWasFound = ParentNode != NULL && ParentNode->VariableName == RootNode->ParentComponentOrVariableName;
						}

						// We found a match; no need to continue searching the hierarchy
						break;
					}
				}
			}

			// Clear parent info if we couldn't find the parent component instance
			if(!bWasFound)
			{
				UE_LOG(LogBlueprint, Warning, TEXT("USimpleConstructionScript::FixupRootNodeParentReferences() - Couldn't find %s parent component '%s' for '%s' in BlueprintGeneratedClass '%s' (it may have been removed)"), RootNode->bIsParentComponentNative ? TEXT("native") : TEXT("inherited"), *RootNode->ParentComponentOrVariableName.ToString(), *RootNode->GetVariableName().ToString(), *BPGeneratedClass->GetName());

				RootNode->bIsParentComponentNative = false;
				RootNode->ParentComponentOrVariableName = NAME_None;
				RootNode->ParentComponentOwnerClassName = NAME_None;
			}
		}
	}
}

void USimpleConstructionScript::ExecuteScriptOnActor(AActor* Actor, const FTransform& RootTransform)
{
	if(RootNodes.Num() > 0)
	{
		for(auto NodeIt = RootNodes.CreateIterator(); NodeIt; ++NodeIt)
		{
			USCS_Node* RootNode = *NodeIt;
			if(RootNode != NULL)
			{
				// If the root node specifies that it has a parent
				USceneComponent* ParentComponent = NULL;
				if(RootNode->ParentComponentOrVariableName != NAME_None)
				{
					// Get the Actor class object
					UClass* ActorClass = Actor->GetClass();
					check(ActorClass != NULL);

					// If the root node is parented to a "native" component (i.e. in the 'Components' array)
					if(RootNode->bIsParentComponentNative)
					{
						TArray<USceneComponent*> Components;
						Actor->GetComponents(Components);

						for(int32 CompIndex = 0; CompIndex < Components.Num(); ++CompIndex)
						{
							// If we found a match, remember the index
							if(Components[CompIndex]->GetFName() == RootNode->ParentComponentOrVariableName)
							{
								ParentComponent = Components[CompIndex];
								break;
							}
						}
					}
					else
					{
						// In the non-native case, the SCS node's variable name property is used as the parent identifier
						UObjectPropertyBase* Property = FindField<UObjectPropertyBase>(ActorClass, RootNode->ParentComponentOrVariableName);
						if(Property != NULL)
						{
							// If we found a matching property, grab its value and use that as the parent for this node
							ParentComponent = Cast<USceneComponent>(Property->GetObjectPropertyValue_InContainer(Actor));
						}
					}
				}

				RootNode->ExecuteNodeOnActor(Actor, ParentComponent != NULL ? ParentComponent : Actor->GetRootComponent(), &RootTransform);
			}
		}
	}
	else if(Actor->GetRootComponent() == NULL) // Must have a root component at the end of SCS, so if we don't have one already (from base class), create a SceneComponent now
	{
		USceneComponent* SceneComp = NewObject<USceneComponent>(Actor);
		SceneComp->SetFlags(RF_Transactional);
		SceneComp->bCreatedByConstructionScript = true;
		SceneComp->SetWorldTransform(RootTransform);
		Actor->SetRootComponent(SceneComp);
		SceneComp->RegisterComponent();
	}
}

#if WITH_EDITOR
UBlueprint* USimpleConstructionScript::GetBlueprint() const
{
	if(auto OwnerClass = GetOwnerClass())
	{
		return Cast<UBlueprint>(OwnerClass->ClassGeneratedBy);
	}
// >>> Backwards Compatibility:  VER_UE4_EDITORONLY_BLUEPRINTS
	if(auto BP = Cast<UBlueprint>(GetOuter()))
	{
		return BP;
	}
// <<< End Backwards Compatibility
	return NULL;
}
#endif

UClass* USimpleConstructionScript::GetOwnerClass() const
{
	if(auto OwnerClass = Cast<UClass>(GetOuter()))
	{
		return OwnerClass;
	}
// >>> Backwards Compatibility:  VER_UE4_EDITORONLY_BLUEPRINTS
#if WITH_EDITOR
	if(auto BP = Cast<UBlueprint>(GetOuter()))
	{
		return BP->GeneratedClass;
	}
#endif
// <<< End Backwards Compatibility
	return NULL;
}

TArray<USCS_Node*> USimpleConstructionScript::GetAllNodes() const
{
	TArray<USCS_Node*> AllNodes;
	if(RootNodes.Num() > 0)
	{
		for(auto NodeIt = RootNodes.CreateConstIterator(); NodeIt; ++NodeIt)
		{
			USCS_Node* RootNode = *NodeIt;
			if(RootNode != NULL)
			{
				AllNodes.Append(RootNode->GetAllNodes());
			}
		}
	}

	return AllNodes;
}

void USimpleConstructionScript::AddNode(USCS_Node* Node)
{
	if(!RootNodes.Contains(Node))
	{
		RootNodes.Add(Node);

		ValidateSceneRootNodes();
	}
}

void USimpleConstructionScript::RemoveNode(USCS_Node* Node)
{
	// If it's a root node we are removing, clear it from the list
	if(RootNodes.Contains(Node))
	{
		RootNodes.Remove(Node);

		Node->bIsParentComponentNative = false;
		Node->ParentComponentOrVariableName = NAME_None;
		Node->ParentComponentOwnerClassName = NAME_None;

		ValidateSceneRootNodes();
	}
	// Not the root, so iterate over all nodes looking for the one with us in its ChildNodes array
	else
	{
		USCS_Node* ParentNode = FindParentNode(Node);
		if(ParentNode != NULL)
		{
			ParentNode->ChildNodes.Remove(Node);
		}
	}
}

USCS_Node* USimpleConstructionScript::RemoveNodeAndPromoteChildren(USCS_Node* Node)
{
	USCS_Node* ChildToPromote = NULL;

	// Pick the first child to promote as a new 'root'
	if (Node->ChildNodes.Num() > 0)
	{
		int32 PromoteIndex = 0;
		ChildToPromote = Node->ChildNodes[PromoteIndex];

		// if this is an editor-only component, then it can't have any game-component children (better make sure that's the case)
		if (ChildToPromote->ComponentTemplate != NULL && ChildToPromote->ComponentTemplate->IsEditorOnly())
		{
			for (int32 ChildIndex = 1; ChildIndex < Node->ChildNodes.Num(); ++ChildIndex)
			{
				USCS_Node* Child = Node->ChildNodes[ChildIndex];
				// we found a game-component sibling, better make it the child to promote
				if (Child->ComponentTemplate != NULL && !Child->ComponentTemplate->IsEditorOnly())
				{
					ChildToPromote = Child;
					PromoteIndex = ChildIndex;
					break;
				}
			}
		}
		Node->ChildNodes.RemoveAt(PromoteIndex);
	}

	if (RootNodes.Contains(Node))
	{
		if(ChildToPromote != NULL)
		{
			RootNodes.Add(ChildToPromote);
			ChildToPromote->ChildNodes.Append(Node->ChildNodes);

			ChildToPromote->bIsParentComponentNative = Node->bIsParentComponentNative;
			ChildToPromote->ParentComponentOrVariableName = Node->ParentComponentOrVariableName;
			ChildToPromote->ParentComponentOwnerClassName = Node->ParentComponentOwnerClassName;
		}
		
		RootNodes.Remove(Node);

		Node->bIsParentComponentNative = false;
		Node->ParentComponentOrVariableName = NAME_None;
		Node->ParentComponentOwnerClassName = NAME_None;

		ValidateSceneRootNodes();
	}
	// Not the root so need to promote in place of node.
	else
	{
		USCS_Node* ParentNode = FindParentNode(Node);
		checkSlow(ParentNode);

		if ( ChildToPromote != NULL )
		{
			// Insert promoted node next to node being removed.
			int32 Location = ParentNode->ChildNodes.Find(Node);
			ParentNode->ChildNodes.Insert(ChildToPromote,Location);
			ChildToPromote->ChildNodes.Append(Node->ChildNodes);	
		}
		// remove node
		ParentNode->ChildNodes.Remove(Node);
	}

	// Clear out references to promoted children
	Node->ChildNodes.Empty();

	return ChildToPromote;
}


USCS_Node* USimpleConstructionScript::FindParentNode(USCS_Node* InNode) const
{
	TArray<USCS_Node*> AllNodes = GetAllNodes();
	for(int32 NodeIdx=0; NodeIdx<AllNodes.Num(); NodeIdx++)
	{
		USCS_Node* TestNode = AllNodes[NodeIdx];
		check(TestNode != NULL);
		if(TestNode->ChildNodes.Contains(InNode))
		{
			return TestNode;
		}
	}
	return NULL;
}

void USimpleConstructionScript::ValidateSceneRootNodes()
{
#if WITH_EDITOR
	UBlueprint* Blueprint = GetBlueprint();

	if(DefaultSceneRootNode == NULL)
	{
		// If applicable, create a default scene component node
		if(Blueprint != NULL
			&& FBlueprintEditorUtils::IsActorBased(Blueprint)
			&& Blueprint->BlueprintType != BPTYPE_MacroLibrary)
		{
			DefaultSceneRootNode = CreateNode(USceneComponent::StaticClass(), DefaultSceneRootVariableName);
		}
	}

	if(DefaultSceneRootNode != NULL)
	{
		UClass* GeneratedClass = GetOwnerClass();

		// Get the Blueprint class default object
		AActor* CDO = NULL;
		if(GeneratedClass != NULL)
		{
			CDO = Cast<AActor>(GeneratedClass->GetDefaultObject(false));
		}

		// If the generated class does not yet have a CDO, defer to the parent class
		if(CDO == NULL && Blueprint->ParentClass != NULL)
		{
			CDO = Cast<AActor>(Blueprint->ParentClass->GetDefaultObject(false));
		}

		// Check the native root component property; don't add the default scene root if it's set
		bool bHasSceneComponentRootNodes = (CDO != NULL && CDO->GetRootComponent() != NULL);
		if(!bHasSceneComponentRootNodes)
		{
			// Get the Blueprint hierarchy
			TArray<UBlueprint*> BPStack;
			if(Blueprint->GeneratedClass != NULL)
			{
				UBlueprint::GetBlueprintHierarchyFromClass(Blueprint->GeneratedClass, BPStack);
			}
			else if(Blueprint->ParentClass != NULL)
			{
				UBlueprint::GetBlueprintHierarchyFromClass(Blueprint->ParentClass, BPStack);
			}

			// Note: Normally if the Blueprint has a parent, we can assume that the parent already has a scene root component set,
			// ...but we'll run through the hierarchy just in case there are legacy BPs out there that might not adhere to this assumption.
			TArray<USimpleConstructionScript*> SCSStack;
			SCSStack.Add(this);

			for(int32 StackIndex = 0; StackIndex < BPStack.Num(); ++StackIndex)
			{
				if(BPStack[StackIndex] && BPStack[StackIndex]->SimpleConstructionScript)
				{
					SCSStack.AddUnique(BPStack[StackIndex]->SimpleConstructionScript);
				}
			}

			for(int32 StackIndex = 0; StackIndex < SCSStack.Num() && !bHasSceneComponentRootNodes; ++StackIndex)
			{
				// Check for any scene component nodes in the root set that are not the default scene root
				const TArray<USCS_Node*>& SCSRootNodes = SCSStack[StackIndex]->GetRootNodes();
				for(int32 RootNodeIndex = 0; RootNodeIndex < SCSRootNodes.Num() && !bHasSceneComponentRootNodes; ++RootNodeIndex)
				{
					USCS_Node* RootNode = SCSRootNodes[RootNodeIndex];
					bHasSceneComponentRootNodes = RootNode != NULL
						&& RootNode != DefaultSceneRootNode
						&& RootNode->ComponentTemplate != NULL
						&& RootNode->ComponentTemplate->IsA<USceneComponent>();
				}
			}
		}

		// Add the default scene root back in if there are no other scene component nodes that can be used as root; otherwise, remove it
		if(!bHasSceneComponentRootNodes
			&& !RootNodes.Contains(DefaultSceneRootNode))
		{
			RootNodes.Add(DefaultSceneRootNode);
		}
		else if(bHasSceneComponentRootNodes
			&& RootNodes.Contains(DefaultSceneRootNode))
		{
			RootNodes.Remove(DefaultSceneRootNode);

			// These shouldn't be set, but just in case...
			DefaultSceneRootNode->bIsParentComponentNative = false;
			DefaultSceneRootNode->ParentComponentOrVariableName = NAME_None;
			DefaultSceneRootNode->ParentComponentOwnerClassName = NAME_None;
		}
	}
#endif // WITH_EDITOR
}

#if WITH_EDITOR

USCS_Node* USimpleConstructionScript::CreateNode(UClass* NewComponentClass, FName NewComponentVariableName)
{
	UBlueprint* Blueprint = GetBlueprint();
	check(Blueprint != NULL);

	// Ensure that the given class is of type UActorComponent
	check(NewComponentClass->IsChildOf(UActorComponent::StaticClass()));

	ensure(NULL != Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass));
	UActorComponent* NewTemplate = ConstructObject<UActorComponent>(NewComponentClass, Blueprint->GeneratedClass);
	NewTemplate->SetFlags(RF_ArchetypeObject|RF_Transactional);

	// Create a node for the script, and save a pointer to the template.
	USCS_Node* NewNode = NewObject<USCS_Node>(this);
	NewNode->SetFlags(RF_Transactional);
	NewNode->ComponentTemplate = NewTemplate;

	// Get a list of names currently in use.
	TArray<FName> CurrentNames;
	NewNode->GenerateListOfExistingNames( CurrentNames );

	// Now create a name for the new component.
	NewNode->VariableName = NewNode->GenerateNewComponentName( CurrentNames, NewComponentVariableName );

	// Note: This should match up with UEdGraphSchema_K2::VR_DefaultCategory
	NewNode->CategoryName = TEXT("Default");

	return NewNode;
}

void USimpleConstructionScript::ValidateNodeVariableNames(FCompilerResultsLog& MessageLog)
{
	UBlueprint* Blueprint = GetBlueprint();
	check(Blueprint);

	TSharedPtr<FKismetNameValidator> ParentBPNameValidator;
	if( Blueprint->ParentClass != NULL )
	{
		UBlueprint* ParentBP = Cast<UBlueprint>(Blueprint->ParentClass->ClassGeneratedBy);
		if( ParentBP != NULL )
		{
			ParentBPNameValidator = MakeShareable(new FKismetNameValidator(ParentBP));
		}
	}

	TSharedPtr<FKismetNameValidator> CurrentBPNameValidator = MakeShareable(new FKismetNameValidator(Blueprint));

	TArray<USCS_Node*> Nodes = GetAllNodes();
	int32 Counter=0;

	for (int32 NodeIndex=0; NodeIndex < Nodes.Num(); ++NodeIndex)
	{
		USCS_Node* Node = Nodes[NodeIndex];
		if( Node && Node->ComponentTemplate && Node != DefaultSceneRootNode )
		{
			// Replace missing or invalid component variable names
			if( Node->VariableName == NAME_None
				|| Node->bVariableNameAutoGenerated_DEPRECATED
				|| !Node->IsValidVariableNameString(Node->VariableName.ToString()) )
			{
				FName OldName = Node->VariableName;

				// Get a list of names currently in use.
				TArray<FName> CurrentNames;
				Node->GenerateListOfExistingNames( CurrentNames );

				// Generate a new default variable name for the component.
				Node->VariableName = Node->GenerateNewComponentName( CurrentNames );
				Node->bVariableNameAutoGenerated_DEPRECATED = false;

				if( OldName != NAME_None )
				{
					FBlueprintEditorUtils::ReplaceVariableReferences(Blueprint, OldName, Node->VariableName);

					MessageLog.Warning(*FString::Printf(TEXT("Found a component variable with an invalid name (%s) - changed to %s."), *OldName.ToString(), *Node->VariableName.ToString()));
				}
			}
			else if( ParentBPNameValidator.IsValid() && ParentBPNameValidator->IsValid(Node->VariableName) != EValidatorResult::Ok )
			{
				FName OldName = Node->VariableName;

				FName NewVariableName = FBlueprintEditorUtils::FindUniqueKismetName(Blueprint, OldName.ToString());
				FBlueprintEditorUtils::RenameMemberVariable(Blueprint, OldName, NewVariableName );

				MessageLog.Warning(*FString::Printf(TEXT("Found a component variable with a conflicting name (%s) - changed to %s."), *OldName.ToString(), *Node->VariableName.ToString()));
			}
		}
	}
}

void USimpleConstructionScript::ClearEditorComponentReferences()
{
	TArray<USCS_Node*> Nodes = GetAllNodes();
	for(int32 i = 0; i < Nodes.Num(); ++i)
	{
		Nodes[i]->EditorComponentInstance = NULL;
	}
}

void USimpleConstructionScript::BeginEditorComponentConstruction()
{
	if(!bIsConstructingEditorComponents)
	{
		ClearEditorComponentReferences();

		bIsConstructingEditorComponents = true;
	}
}

void USimpleConstructionScript::EndEditorComponentConstruction()
{
	bIsConstructingEditorComponents = false;
}
#endif
