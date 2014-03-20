// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "BlueprintUtilities.h"
#include "LatentActions.h"
#include "EngineLevelScriptClasses.h"
#if WITH_EDITORONLY_DATA
#include "Kismet2/BlueprintEditorUtils.h"
#endif
#include "DeferRegisterStaticComponents.h"

//////////////////////////////////////////////////////////////////////////
// USCS_Node

USCS_Node::USCS_Node(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	bIsFalseRoot_DEPRECATED = false;
	bIsNative_DEPRECATED = false;

	bIsParentComponentNative = false;

#if WITH_EDITOR
	EditorComponentInstance = NULL;
#endif
}

void USCS_Node::ExecuteNodeOnActor(AActor* Actor, USceneComponent* ParentComponent, const FTransform* RootTransform)
{
	check(Actor != NULL);
	check((ParentComponent != NULL && !ParentComponent->IsPendingKill()) || (RootTransform != NULL)); // must specify either a parent component or a world transform

	// Create a new component instance based on the template
	UActorComponent* NewActorComp = Actor->CreateComponentFromTemplate(ComponentTemplate, VariableName.ToString());
	if(NewActorComp != NULL)
	{
		bool bDeferRegisterStaticComponent = false;
		EComponentMobility::Type OriginalMobility = EComponentMobility::Movable;

		// SCS created components are net addressable
		NewActorComp->SetNetAddressable();

		// Special handling for scene components
		USceneComponent* NewSceneComp = Cast<USceneComponent>(NewActorComp);
		if (NewSceneComp != NULL)
		{
			// Components with Mobility set to EComponentMobility::Static or EComponentMobility::Stationary can't be properly set up in SCS (all changes will be rejected
			// due to EComponentMobility::Static flag) so we're going to temporarily change the flag and defer the registration until SCS has finished.
			bDeferRegisterStaticComponent = NewSceneComp->Mobility != EComponentMobility::Movable;
			OriginalMobility = NewSceneComp->Mobility;
			if (bDeferRegisterStaticComponent)
			{
				NewSceneComp->Mobility = EComponentMobility::Movable;
			}

			// If NULL is passed in, we are the root, so set transform and assign as RootComponent on Actor
			if (ParentComponent == NULL || (ParentComponent && ParentComponent->IsPendingKill()))
			{
				NewSceneComp->SetFlags(RF_Transactional);
				NewSceneComp->SetWorldTransform(*RootTransform);
				Actor->SetRootComponent(NewSceneComp);
			}
			// Otherwise, attach to parent component passed in
			else
			{
				NewSceneComp->AttachTo(ParentComponent, AttachToName);
			}
		}

		// Call function to notify component it has been created
		NewActorComp->OnComponentCreated();

		if (bDeferRegisterStaticComponent)
		{
			// Defer registration until after SCS has completed.
			FDeferRegisterStaticComponents::Get().DeferStaticComponent(Actor, NewSceneComp, OriginalMobility);
		}
		else
		{
			NewActorComp->RegisterComponent();
		}

		if (NewActorComp->GetIsReplicated())
		{
			// Make sure this component is added to owning actor's replicated list.
			NewActorComp->SetIsReplicated(true);
		}

		// If we want to save this to a property, do it here
		FName VarName = GetVariableName();
		if (VarName != NAME_None)
		{
			UClass* ActorClass = Actor->GetClass();
			if (UObjectPropertyBase* Prop = FindField<UObjectPropertyBase>(ActorClass, VarName))
			{
				Prop->SetObjectPropertyValue_InContainer(Actor, NewActorComp);
			}
			else
			{
				UE_LOG(LogBlueprint, Log, TEXT("ExecuteNodeOnActor: Couldn't find property '%s' on '%s"), *VarName.ToString(), *Actor->GetName());
#if WITH_EDITOR
				// If we're constructing editable components in the SCS editor, set the component instance corresponding to this node for editing purposes
				USimpleConstructionScript* SCS = GetSCS();
				if(SCS != NULL && (SCS->IsConstructingEditorComponents() || SCS->GetComponentEditorActorInstance() == Actor))
				{
					EditorComponentInstance = NewSceneComp;
				}
#endif
			}
		}

		// Determine the parent component for our children (it's still our parent if we're a non-scene component)
		USceneComponent* ParentSceneComponentOfChildren = (NewSceneComp != NULL) ? NewSceneComp : ParentComponent;

		// If we made a component, go ahead and process our children
		for (int32 NodeIdx = 0; NodeIdx < ChildNodes.Num(); NodeIdx++)
		{
			USCS_Node* Node = ChildNodes[NodeIdx];
			check(Node != NULL);
			Node->ExecuteNodeOnActor(Actor, ParentSceneComponentOfChildren, NULL);
		}
	}
}

TArray<USCS_Node*> USCS_Node::GetAllNodes()
{
	TArray<USCS_Node*> AllNodes;

	//  first add ourself
	AllNodes.Add(this);

	// then add each child (including all their children)
	for(int32 ChildIdx=0; ChildIdx<ChildNodes.Num(); ChildIdx++)
	{
		USCS_Node* ChildNode = ChildNodes[ChildIdx];
		check(ChildNode != NULL);
		AllNodes.Append( ChildNode->GetAllNodes() );
	}

	return AllNodes;
}

void USCS_Node::AddChildNode(USCS_Node* InNode)
{
	if(InNode != NULL && !ChildNodes.Contains(InNode))
	{
		ChildNodes.Add(InNode);
	}
}

TArray<const USCS_Node*> USCS_Node::GetAllNodes() const
{
	TArray<const USCS_Node*> AllNodes;

	//  first add ourself
	AllNodes.Add(this);

	// then add each child (including all their children)
	for(int32 ChildIdx=0; ChildIdx<ChildNodes.Num(); ChildIdx++)
	{
		const USCS_Node* ChildNode = ChildNodes[ChildIdx];
		check(ChildNode != NULL);
		AllNodes.Append( ChildNode->GetAllNodes() );
	}

	return AllNodes;
}

bool USCS_Node::IsChildOf(USCS_Node* TestParent)
{
	TArray<USCS_Node*> AllNodes;
	if(TestParent != NULL)
	{
		AllNodes = TestParent->GetAllNodes();
	}
	return AllNodes.Contains(this);
}

void USCS_Node::PreloadChain()
{
	if( HasAnyFlags(RF_NeedLoad) )
	{
		GetLinker()->Preload(this);
	}

	for( TArray<USCS_Node*>::TIterator ChildIt(ChildNodes); ChildIt; ++ChildIt )
	{
		USCS_Node* CurrentChild = *ChildIt;
		if( CurrentChild )
		{
			CurrentChild->PreloadChain();
		}
	}
}

bool USCS_Node::IsRootNode() const
{
	USimpleConstructionScript* SCS = GetSCS();
	return(SCS->GetRootNodes().Contains(const_cast<USCS_Node*>(this)));
}

FName USCS_Node::GetVariableName() const
{
	// Name specified
	if(VariableName != NAME_None)
	{
		return VariableName;
	}
	// Not specified, make variable based on template name
	// Note that since SCS_Nodes should all have auto generated names, this code shouldn't be hit unless
	// the auto naming code fails.
	else if(ComponentTemplate != NULL)
	{
		FString VarString = FString::Printf(TEXT("%s_Var"), *ComponentTemplate->GetName());
		return FName(*VarString);
	}
	else
	{
		return NAME_None;
	}
}

void USCS_Node::NameWasModified()
{
	OnNameChangedExternal.ExecuteIfBound(VariableName);
}

void USCS_Node::SetOnNameChanged( const FSCSNodeNameChanged& OnChange )
{
	OnNameChangedExternal = OnChange;
}

int32 USCS_Node::FindMetaDataEntryIndexForKey(const FName& Key)
{
	for(int32 i=0; i<MetaDataArray.Num(); i++)
	{
		if(MetaDataArray[i].DataKey == Key)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

FString USCS_Node::GetMetaData(const FName& Key)
{
	int32 EntryIndex = FindMetaDataEntryIndexForKey(Key);
	check(EntryIndex != INDEX_NONE);
	return MetaDataArray[EntryIndex].DataValue;
}

void USCS_Node::SetMetaData(const FName& Key, const FString& Value)
{
	int32 EntryIndex = FindMetaDataEntryIndexForKey(Key);
	if(EntryIndex != INDEX_NONE)
	{
		MetaDataArray[EntryIndex].DataValue = Value;
	}
	else
	{
		MetaDataArray.Add( FBPVariableMetaDataEntry(Key, Value) );
	}
}

void USCS_Node::RemoveMetaData(const FName& Key)
{
	int32 EntryIndex = FindMetaDataEntryIndexForKey(Key);
	if(EntryIndex != INDEX_NONE)
	{
		MetaDataArray.RemoveAt(EntryIndex);
	}
}

#if WITH_EDITOR
void USCS_Node::SetParent(USCS_Node* InParentNode)
{
	check(InParentNode != NULL);
	check(InParentNode->GetSCS() != NULL);
	check(InParentNode->GetSCS()->GetBlueprint() != NULL);
	check(InParentNode->GetSCS()->GetBlueprint()->GeneratedClass != NULL);

	bIsParentComponentNative = false;
	ParentComponentOrVariableName = InParentNode->VariableName;
	ParentComponentOwnerClassName = InParentNode->GetSCS()->GetBlueprint()->GeneratedClass->GetFName();
}

void USCS_Node::SetParent(USceneComponent* InParentComponent)
{
	check(InParentComponent != NULL);

	bIsParentComponentNative = true;
	ParentComponentOrVariableName = InParentComponent->GetFName();
	ParentComponentOwnerClassName = NAME_None;
}

USceneComponent* USCS_Node::GetParentComponentTemplate(UBlueprint* InBlueprint) const
{
	USceneComponent* ParentComponentTemplate = NULL;
	if(ParentComponentOrVariableName != NAME_None)
	{
		check(InBlueprint != NULL && InBlueprint->GeneratedClass != NULL);

		// If the parent component template is found in the 'Components' array of the CDO (i.e. native)
		if(bIsParentComponentNative)
		{
			// Access the Blueprint CDO
			AActor* CDO = InBlueprint->GeneratedClass->GetDefaultObject<AActor>();
			if(CDO != NULL)
			{
				// Find the component template in the CDO that matches the specified name
				TArray<USceneComponent*> Components;
				CDO->GetComponents(Components);

				for(auto CompIt = Components.CreateIterator(); CompIt; ++CompIt)
				{
					USceneComponent* CompTemplate = *CompIt;
					if(CompTemplate->GetFName() == ParentComponentOrVariableName)
					{
						// Found a match; this is our parent, we're done
						ParentComponentTemplate = CompTemplate;
						break;
					}
				}
			}
		}
		// Otherwise the parent component template is found in a parent Blueprint's SCS tree (i.e. non-native)
		else
		{
			// Get the Blueprint hierarchy
			TArray<UBlueprint*> ParentBPStack;
			UBlueprint::GetBlueprintHierarchyFromClass(InBlueprint->GeneratedClass, ParentBPStack);

			// Find the parent Blueprint in the hierarchy
			for(int32 StackIndex = ParentBPStack.Num() - 1; StackIndex > 0; --StackIndex)
			{
				UBlueprint* ParentBlueprint = ParentBPStack[StackIndex];
				if(ParentBlueprint != NULL
					&& ParentBlueprint->SimpleConstructionScript != NULL
					&& ParentBlueprint->GeneratedClass->GetFName() == ParentComponentOwnerClassName)
				{
					// Find the SCS node with a variable name that matches the specified name
					TArray<USCS_Node*> ParentSCSNodes = ParentBlueprint->SimpleConstructionScript->GetAllNodes();
					for(int32 ParentNodeIndex = 0; ParentNodeIndex < ParentSCSNodes.Num(); ++ParentNodeIndex)
					{
						USceneComponent* CompTemplate = Cast<USceneComponent>(ParentSCSNodes[ParentNodeIndex]->ComponentTemplate);
						if(CompTemplate != NULL && ParentSCSNodes[ParentNodeIndex]->VariableName == ParentComponentOrVariableName)
						{
							// Found a match; this is our parent, we're done
							ParentComponentTemplate = CompTemplate;
							break;
						}
					}
				}
			}
		}
	}

	return ParentComponentTemplate;
}

bool USCS_Node::IsValidVariableNameString(const FString& InString)
{
	// First test to make sure the string is not empty and does not equate to the DefaultSceneRoot node name
	bool bIsValid = !InString.IsEmpty() && !InString.Equals(USimpleConstructionScript::DefaultSceneRootVariableName.ToString());
	if(bIsValid && ComponentTemplate != NULL)
	{
		// Next test to make sure the string doesn't conflict with the format that MakeUniqueObjectName() generates
		FString MakeUniqueObjectNamePrefix = FString::Printf(TEXT("%s_"), *ComponentTemplate->GetClass()->GetName());
		if(InString.StartsWith(MakeUniqueObjectNamePrefix))
		{
			bIsValid = !InString.Replace(*MakeUniqueObjectNamePrefix, TEXT("")).IsNumeric();
		}
	}

	return bIsValid;
}

void USCS_Node::GenerateListOfExistingNames( TArray<FName>& CurrentNames ) const
{
	const USimpleConstructionScript* SCS = GetSCS();
	check(SCS);

	const UBlueprintGeneratedClass* OwnerClass = Cast<const UBlueprintGeneratedClass>(SCS->GetOuter());
	const UBlueprint* Blueprint = Cast<const UBlueprint>(OwnerClass ? OwnerClass->ClassGeneratedBy : NULL);
	// >>> Backwards Compatibility:  VER_UE4_EDITORONLY_BLUEPRINTS
	if(!Blueprint)
	{
		Blueprint = Cast<UBlueprint>(SCS->GetOuter());
	}
	// <<< End Backwards Compatibility
	check(Blueprint);

	if(Blueprint->SkeletonGeneratedClass)
	{
		// First add the class variables.
		FBlueprintEditorUtils::GetClassVariableList(Blueprint, CurrentNames, true);
		// Then the function names.
		FBlueprintEditorUtils::GetFunctionNameList(Blueprint, CurrentNames);
	}
	
	// Get the list of child nodes and add each child (including all their children)
	TArray<const USCS_Node*> ChildrenNodes = GetAllNodes();
	// And add their names
	for (int32 NodeIndex=0; NodeIndex < ChildrenNodes.Num(); ++NodeIndex)
	{
		const USCS_Node* ChildNode = ChildrenNodes[NodeIndex];
		if( ChildNode )
		{
			if( ChildNode->VariableName != NAME_None )
			{
				CurrentNames.Add( ChildNode->VariableName );
			}
		}
	}

	if(SCS->GetDefaultSceneRootNode())
	{
		CurrentNames.AddUnique(SCS->GetDefaultSceneRootNode()->GetVariableName());
	}
}

FName USCS_Node::GenerateNewComponentName( TArray<FName>& CurrentNames, FName DesiredName ) const
{
	FName NewName;
	if( ComponentTemplate )
	{
		if(DesiredName != NAME_None && !CurrentNames.Contains(DesiredName))
		{
			NewName = DesiredName;
		}
		else
		{
			FString ComponentName;
			if(DesiredName != NAME_None)
			{
				ComponentName = DesiredName.ToString();
			}
			else
			{
				ComponentName = ComponentTemplate->GetClass()->GetName().Replace(TEXT("Component"), TEXT(""));
			}
			
			int32 Counter = 1;
			do
			{
				NewName = FName(*( FString::Printf(TEXT("%s%d"), *ComponentName, Counter++) ));		
			} 
			while( CurrentNames.Contains(NewName) );
		}
	}
	return NewName;
}

#endif
