// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

/** Variable replication states */
namespace EVariableReplication
{
	enum Type
	{
		None,
		Replicated,
		RepNotify,
		MAX
	};
}

/** Details customization for variables selected in the MyBlueprint panel */
class FBlueprintVarActionDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<class IDetailCustomization> MakeInstance(TWeakPtr<SMyBlueprint> InMyBlueprint)
	{
		return MakeShareable(new FBlueprintVarActionDetails(InMyBlueprint));
	}

	FBlueprintVarActionDetails(TWeakPtr<SMyBlueprint> InMyBlueprint)
		: MyBlueprint(InMyBlueprint)
		, bIsVarNameInvalid(false)
	{
	}
	
	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailLayout ) OVERRIDE;
	
	static void PopulateCategories(SMyBlueprint* MyBlueprint, TArray<TSharedPtr<FString>>& CategorySource);

private:
	/** Accessors passed to parent */
	UBlueprint* GetBlueprintObj() const {return MyBlueprint.Pin()->GetBlueprintObj();}
	FEdGraphSchemaAction_K2Var* MyBlueprintSelectionAsVar() const {return MyBlueprint.Pin()->SelectionAsVar();}
	UK2Node_Variable* EdGraphSelectionAsVar() const;
	UProperty* SelectionAsProperty() const;
	UClass* SelectionAsClass() const;
	FName GetVariableName() const;

	/** Commonly queried attributes about the schema action */
	bool IsAComponentVariable(UProperty* VariableProperty) const;
	bool IsABlueprintVariable(UProperty* VariableProperty) const;

	// Callbacks for uproperty details customization
	bool GetVariableNameChangeEnabled() const;
	FText OnGetVarName() const;
	void OnVarNameChanged(const FText& InNewText);
	void OnVarNameCommitted(const FText& InNewName, ETextCommit::Type InTextCommit);
	bool GetVariableTypeChangeEnabled() const;
	FEdGraphPinType OnGetVarType() const;
	void OnVarTypeChanged(const FEdGraphPinType& NewPinType);
	EVisibility IsTooltipEditVisible() const;

	/** Callback to decide if the category drop down menu should be enabled */
	bool GetVariableCategoryChangeEnabled() const;

	FText OnGetTooltipText() const;
	void OnTooltipTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit, FName VarName);
	
	FText OnGetCategoryText() const;
	void OnCategoryTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit, FName VarName);
	TSharedRef< ITableRow > MakeCategoryViewWidget( TSharedPtr<FString> Item, const TSharedRef< STableViewBase >& OwnerTable );
	void OnCategorySelectionChanged( TSharedPtr<FString> ProposedSelection, ESelectInfo::Type /*SelectInfo*/ );
	
	EVisibility ShowEditableCheckboxVisibilty() const;
	ESlateCheckBoxState::Type OnEditableCheckboxState() const;
	void OnEditableChanged(ESlateCheckBoxState::Type InNewState);

	ESlateCheckBoxState::Type OnCreateWidgetCheckboxState() const;
	void OnCreateWidgetChanged(ESlateCheckBoxState::Type InNewState);
	EVisibility Show3DWidgetVisibility() const;
	bool Is3DWidgetEnabled();
	
	ESlateCheckBoxState::Type OnGetExposedToSpawnCheckboxState() const;
	void OnExposedToSpawnChanged(ESlateCheckBoxState::Type InNewState);
	EVisibility ExposeOnSpawnVisibility() const;

	ESlateCheckBoxState::Type OnGetPrivateCheckboxState() const;
	void OnPrivateChanged(ESlateCheckBoxState::Type InNewState);
	EVisibility ExposePrivateVisibility() const;
	
	ESlateCheckBoxState::Type OnGetExposedToMatineeCheckboxState() const;
	void OnExposedToMatineeChanged(ESlateCheckBoxState::Type InNewState);
	EVisibility ExposeToMatineeVisibility() const;

	FText OnGetSliderMinValue() const;
	void OnSliderMinValueChanged(const FText& NewMinValue, ETextCommit::Type CommitInfo);
	FText OnGetSliderMaxValue() const;
	void OnSliderMaxValueChanged(const FText& NewMaxValue, ETextCommit::Type CommitInfo);
	EVisibility SliderVisibility() const;
	
	TSharedPtr<FString> GetVariableReplicationType() const;
	void OnChangeReplication(TSharedPtr<FString> ItemSelected, ESelectInfo::Type SelectInfo);
	void ReplicationOnRepFuncChanged(const FString& NewOnRepFunc) const;
	EVisibility ReplicationVisibility() const;

	EVisibility GetTransientVisibility() const;
	ESlateCheckBoxState::Type OnGetTransientCheckboxState() const;
	void OnTransientChanged(ESlateCheckBoxState::Type InNewState);

	EVisibility GetSaveGameVisibility() const;
	ESlateCheckBoxState::Type OnGetSaveGameCheckboxState() const;
	void OnSaveGameChanged(ESlateCheckBoxState::Type InNewState);

	/** Refresh the property flags list */
	void RefreshPropertyFlags();

	/** Generates the widget for the property flag list */
	TSharedRef<ITableRow> OnGenerateWidgetForPropertyList( TSharedPtr< FString > Item, const TSharedRef<STableViewBase>& OwnerTable );

	/** Delegate to build variable events droplist menu */
	TSharedRef<SWidget> BuildEventsMenuForVariable() const;
	
private:
	/** Pointer back to my parent tab */
	TWeakPtr<SMyBlueprint> MyBlueprint;

	/** Array of replication options for our combo text box */
	TArray<TSharedPtr<FString>> ReplicationOptions;

	/** The widget used when in variable name editing mode */ 
	TSharedPtr<SEditableTextBox> VarNameEditableTextBox;

	/** Flag to indicate whether or not the variable name is invalid */
	bool bIsVarNameInvalid;
	
	/** A list of all category names to choose from */
	TArray<TSharedPtr<FString>> CategorySource;
	/** Widgets for the categories */
	TWeakPtr<SComboButton> CategoryComboButton;
	TWeakPtr<SListView<TSharedPtr<FString>>> CategoryListView;

	/** Array of names of property flags on the selected property */
	TArray< TSharedPtr< FString > > PropertyFlags;

	/** The listview widget for displaying property flags */
	TWeakPtr< SListView< TSharedPtr< FString > > > PropertyFlagWidget;
};

/** Details customization for local variables selected in the MyBlueprint panel */
class FBlueprintLocalVarActionDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<class IDetailCustomization> MakeInstance(TWeakPtr<SMyBlueprint> InMyBlueprint)
	{
		return MakeShareable(new FBlueprintLocalVarActionDetails(InMyBlueprint));
	}

	FBlueprintLocalVarActionDetails(TWeakPtr<SMyBlueprint> InMyBlueprint)
		: MyBlueprint(InMyBlueprint)
		, bIsVarNameInvalid(false)
	{
	}

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailLayout ) OVERRIDE;

private:
	/** Accessors passed to parent */
	UBlueprint* GetBlueprintObj() const {return MyBlueprint.Pin()->GetBlueprintObj();}
	UK2Node_LocalVariable* GetDetailsSelection() const;
	UK2Node_LocalVariable* MyBlueprintSelectionAsLocalVar() const {return MyBlueprint.Pin()->SelectionAsLocalVar();}
	UK2Node_LocalVariable* EdGraphSelectionAsLocalVar() const;
	FName GetVariableName() const;

	// Callbacks for uproperty details customization
	FText OnGetVarName() const;
	void OnLocalVarNameChanged(const FText& InNewText);
	void OnLocalVarNameCommitted(const FText& InNewName, ETextCommit::Type InTextCommit);
	bool GetVariableTypeChangeEnabled() const;
	FEdGraphPinType OnGetVarType() const;
	void OnVarTypeChanged(const FEdGraphPinType& NewPinType);

	FText OnGetTooltipText() const;
	void OnTooltipTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit);

private:
	/** Pointer back to my parent tab */
	TWeakPtr<SMyBlueprint> MyBlueprint;

	/** The widget used when in variable name editing mode */ 
	TSharedPtr<SEditableTextBox> VarNameEditableTextBox;

	/** Flag to indicate whether or not the variable name is invalid */
	bool bIsVarNameInvalid;
};

class FBaseBlueprintGraphActionDetails : public IDetailCustomization
{
public:
	virtual ~FBaseBlueprintGraphActionDetails(){};

	FBaseBlueprintGraphActionDetails(TWeakPtr<SMyBlueprint> InMyBlueprint)
		: MyBlueprint(InMyBlueprint)
	{
	}
	
	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailLayout ) OVERRIDE 
	{
		check(false);
	}

	/** Gets the graph that we are currently editing */
	virtual UEdGraph* GetGraph() const;

	/** Refreshes the graph and ensures the target node is up to date */
	void OnParamsChanged(UK2Node_EditablePinBase* TargetNode, bool bForceRefresh = false);

	/** Refreshes the graph and ensures the target node is up to date */
	bool OnPinRenamed(UK2Node_EditablePinBase* TargetNode, const FString& OldName, const FString& NewName);

	/** Called to potentially remove the result node (if there are no output args), returns true if it was cleaned up */
	bool ConditionallyCleanUpResultNode();
	
	/** Gets the blueprint we're editing */
	TWeakPtr<SMyBlueprint> GetMyBlueprint() const {return MyBlueprint.Pin();}

	/** Gets the node for the function entry point */
	TWeakObjectPtr<class UK2Node_EditablePinBase> GetFunctionEntryNode() const {return FunctionEntryNodePtr;}

	/** Sets the delegate to be called when refreshing our children */
	void SetRefreshDelegate(FSimpleDelegate RefreshDelegate, bool bForInputs);

	/** Accessors passed to parent */
	UBlueprint* GetBlueprintObj() const {return MyBlueprint.Pin()->GetBlueprintObj();}
	
	FReply OnAddNewInputClicked();

	/** Utility functions for pin names */
	bool IsPinNameUnique(const FString& TestName) const;
	void GenerateUniqueParameterName( const FString &BaseName, FString &Result ) const;

protected:
	/** Tries to create the result node (if there are output args) */
	bool AttemptToCreateResultNode();

protected:
	/** Pointer to the parent */
	TWeakPtr<SMyBlueprint> MyBlueprint;
	
	/** The entry node in the graph */
	TWeakObjectPtr<class UK2Node_EditablePinBase> FunctionEntryNodePtr;

	/** The result node in the graph, if the function has any return or out params.  This can be the same as the entry point */
	TWeakObjectPtr<class UK2Node_EditablePinBase> FunctionResultNodePtr;

	/** Delegates to regenerate the lists of children */
	FSimpleDelegate RegenerateInputsChildrenDelegate;
	FSimpleDelegate RegenerateOutputsChildrenDelegate;

	/** Details layout builder we need to hold on to to refresh it at times */
	IDetailLayoutBuilder* DetailsLayoutPtr;
	
	/** Array of nodes were were constructed to represent */
	TArray< TWeakObjectPtr<UObject> > ObjectsBeingEdited;
};

class FBlueprintDelegateActionDetails : public FBaseBlueprintGraphActionDetails
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<class IDetailCustomization> MakeInstance(TWeakPtr<SMyBlueprint> InMyBlueprint)
	{
		return MakeShareable(new FBlueprintDelegateActionDetails(InMyBlueprint));
	}

	FBlueprintDelegateActionDetails(TWeakPtr<SMyBlueprint> InMyBlueprint)
		: FBaseBlueprintGraphActionDetails(InMyBlueprint)
	{
	}
	
	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailLayout ) OVERRIDE;

	/** Gets the graph that we are currently editing */
	virtual UEdGraph* GetGraph() const OVERRIDE;

private:

	void SetEntryNode();

	UMulticastDelegateProperty* GetDelegatePoperty() const;
	FText OnGetTooltipText() const;
	void OnTooltipTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit);
	FText OnGetCategoryText() const;
	void OnCategoryTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit);
	TSharedRef< ITableRow > MakeCategoryViewWidget( TSharedPtr<FString> Item, const TSharedRef< STableViewBase >& OwnerTable );
	void OnCategorySelectionChanged( TSharedPtr<FString> ProposedSelection, ESelectInfo::Type /*SelectInfo*/ );

	void CollectAvailibleSignatures();
	void OnFunctionSelected(TSharedPtr<FString> FunctionItemData, ESelectInfo::Type SelectInfo);
	bool IsBlueprintProperty() const;

private:

	/** A list of all category names to choose from */
	TArray<TSharedPtr<FString>> CategorySource;

	/** Widgets for the categories */
	TWeakPtr<SComboButton> CategoryComboButton;
	TWeakPtr<SListView<TSharedPtr<FString>>> CategoryListView;

	TArray<TSharedPtr<FString>> FunctionsToCopySignatureFrom;
	TSharedPtr<STextComboBox> CopySignatureComboButton;
};

/** Custom struct for each group of arguments in the function editing details */
class FBlueprintGraphArgumentGroupLayout : public IDetailCustomNodeBuilder, public TSharedFromThis<FBlueprintGraphArgumentGroupLayout>
{
public:
	FBlueprintGraphArgumentGroupLayout(TWeakPtr<class FBaseBlueprintGraphActionDetails> InGraphActionDetails, UK2Node_EditablePinBase* InTargetNode)
		: TargetNode(InTargetNode)
		, GraphActionDetailsPtr(InGraphActionDetails) {}

private:
	/** IDetailCustomNodeBuilder Interface*/
	virtual void SetOnRebuildChildren( FSimpleDelegate InOnRegenerateChildren ) OVERRIDE;
	virtual void GenerateHeaderRowContent( FDetailWidgetRow& NodeRow ) OVERRIDE {}
	virtual void GenerateChildContent( IDetailChildrenBuilder& ChildrenBuilder ) OVERRIDE;
	virtual void Tick( float DeltaTime ) OVERRIDE {}
	virtual bool RequiresTick() const OVERRIDE { return false; }
	virtual FName GetName() const OVERRIDE { return NAME_None; }
	virtual bool InitiallyCollapsed() const OVERRIDE { return false; }
	
private:
	/** The parent graph action details customization */
	TWeakPtr<class FBaseBlueprintGraphActionDetails> GraphActionDetailsPtr;

	/** The target node that this argument is on */
	TWeakObjectPtr<UK2Node_EditablePinBase> TargetNode;
};

/** Custom struct for each argument in the function editing details */
class FBlueprintGraphArgumentLayout : public IDetailCustomNodeBuilder, public TSharedFromThis<FBlueprintGraphArgumentLayout>
{
public:
	FBlueprintGraphArgumentLayout(TWeakPtr<FUserPinInfo> PinInfo, UK2Node_EditablePinBase* InTargetNode, TWeakPtr<class FBaseBlueprintGraphActionDetails> InGraphActionDetails, FName InArgName, bool bInHasDefaultValue)
		: ParamItemPtr(PinInfo)
		, TargetNode(InTargetNode)
		, GraphActionDetailsPtr(InGraphActionDetails)
		, ArgumentName(InArgName)
		, bHasDefaultValue(bInHasDefaultValue) {}
	
private:
	/** IDetailCustomNodeBuilder Interface*/
	virtual void SetOnRebuildChildren( FSimpleDelegate InOnRegenerateChildren ) OVERRIDE {}
	virtual void GenerateHeaderRowContent( FDetailWidgetRow& NodeRow ) OVERRIDE;
	virtual void GenerateChildContent( IDetailChildrenBuilder& ChildrenBuilder ) OVERRIDE;
	virtual void Tick( float DeltaTime ) OVERRIDE {}
	virtual bool RequiresTick() const OVERRIDE { return false; }
	virtual FName GetName() const OVERRIDE { return ArgumentName; }
	virtual bool InitiallyCollapsed() const OVERRIDE { return true; }

private:
	/** Determines if this pin should not be editable */
	bool ShouldPinBeReadOnly() const;
	
	/** Callbacks for all the functionality for modifying arguments */
	void OnRemoveClicked();
	FReply OnArgMoveUp();
	FReply OnArgMoveDown();

	FText OnGetArgNameText() const;
	void OnArgNameTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit);
	
	FEdGraphPinType OnGetPinInfo() const;
	void PinInfoChanged(const FEdGraphPinType& PinType);
	void OnPrePinInfoChange(const FEdGraphPinType& PinType);

	/** Returns whether the "Pass-by-Reference" checkbox is checked or not */
	ESlateCheckBoxState::Type IsRefChecked() const;

	/** Handles toggling the "Pass-by-Reference" checkbox */
	void OnRefCheckStateChanged(ESlateCheckBoxState::Type InState);

	FText OnGetArgDefaultValueText() const;
	void OnArgDefaultValueCommitted(const FText& NewText, ETextCommit::Type InTextCommit);

private:
	/** The parent graph action details customization */
	TWeakPtr<class FBaseBlueprintGraphActionDetails> GraphActionDetailsPtr;

	/** The argument pin that this layout reflects */
	TWeakPtr<FUserPinInfo> ParamItemPtr;

	/** The target node that this argument is on */
	UK2Node_EditablePinBase* TargetNode;

	/** Whether or not this builder should have a default value edit control (input args only) */
	bool bHasDefaultValue;

	/** The name of this argument for remembering expansion state */
	FName ArgumentName;
};

/** Details customization for functions and graphs selected in the MyBlueprint panel */
class FBlueprintGraphActionDetails : public FBaseBlueprintGraphActionDetails
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<class IDetailCustomization> MakeInstance(TWeakPtr<SMyBlueprint> InMyBlueprint)
	{
		return MakeShareable(new FBlueprintGraphActionDetails(InMyBlueprint));
	}

	FBlueprintGraphActionDetails(TWeakPtr<SMyBlueprint> InMyBlueprint)
		: FBaseBlueprintGraphActionDetails(InMyBlueprint)
	{
	}
	
	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailLayout ) OVERRIDE;

private:
	struct FAccessSpecifierLabel
	{
		FText LocalizedName;
		EFunctionFlags SpecifierFlag;

		FAccessSpecifierLabel( FText InLocalizedName, EFunctionFlags InSpecifierFlag) :
			LocalizedName( InLocalizedName ), SpecifierFlag( InSpecifierFlag )
		{}
	};

private:

	/** Setup for the nodes this details customizer needs to access */
	void SetEntryAndResultNodes();

	/** Gets the node we are currently editing if available */
	UK2Node_EditablePinBase* GetEditableNode() const;
	
	/* Get function associated with the selected graph*/
	UFunction* FindFunction() const;
	
	/** Utility for editing metadata on the function */
	FKismetUserDeclaredFunctionMetadata* GetMetadataBlock() const;

	// Callbacks for uproperty details customization
	FText OnGetTooltipText() const;
	void OnTooltipTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit);
	
	FText OnGetCategoryText() const;
	void OnCategoryTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit);
	
	FText AccessSpecifierProperName( uint32 AccessSpecifierFlag ) const;
	bool IsAccessSpecifierVisible() const;
	TSharedRef<ITableRow> HandleGenerateRowAccessSpecifier( TSharedPtr<FAccessSpecifierLabel> SpecifierName, const TSharedRef<STableViewBase>& OwnerTable );
	FString GetCurrentAccessSpecifierName() const;
	void OnAccessSpecifierSelected( TSharedPtr<FAccessSpecifierLabel> SpecifierName, ESelectInfo::Type SelectInfo );

	bool GetInstanceColorVisibility() const;
	FLinearColor GetNodeTitleColor() const;
	FReply ColorBlock_OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	
	bool IsCustomEvent() const;
	void OnIsReliableReplicationFunctionModified(const ESlateCheckBoxState::Type NewCheckedState);
	ESlateCheckBoxState::Type GetIsReliableReplicatedFunction() const;
	bool GetIsReplicatedFunction() const;

	struct FReplicationSpecifierLabel
	{
		FText LocalizedName;
		FText LocalizedToolTip;
		uint32 SpecifierFlag;

		FReplicationSpecifierLabel( FText InLocalizedName, uint32 InSpecifierFlag, FText InLocalizedToolTip ) :
			LocalizedName( InLocalizedName ), SpecifierFlag( InSpecifierFlag ), LocalizedToolTip( InLocalizedToolTip )
		{}
	};

	FString GetCurrentReplicatedEventString() const;
	FText ReplicationSpecifierProperName( uint32 ReplicationSpecifierFlag ) const;
	TSharedRef<ITableRow> OnGenerateReplicationComboWidget( TSharedPtr<FReplicationSpecifierLabel> InNetFlag, const TSharedRef<STableViewBase>& OwnerTable );
	
	bool IsPureFunctionVisible() const;
	void OnIsPureFunctionModified(const ESlateCheckBoxState::Type NewCheckedState);
	ESlateCheckBoxState::Type GetIsPureFunction() const;
	
	FReply OnAddNewOutputClicked();

	/** Called to set the replication type from the details view combo */
	static void SetNetFlags( TWeakObjectPtr<UK2Node_EditablePinBase> FunctionEntryNode, uint32 NetFlags);

private:

	/** List of available localized access specifiers names */
	TArray<TSharedPtr<FAccessSpecifierLabel> > AccessSpecifierLabels;

	/** ComboButton with access specifiers */
	TSharedPtr< class SComboButton > AccessSpecifierComboButton;

	/** Color block for parenting the color picker */
	TSharedPtr<class SColorBlock> ColorBlock;
};

/** Blueprint Interface List Details */
class FBlueprintInterfaceLayout : public IDetailCustomNodeBuilder, public TSharedFromThis<FBlueprintInterfaceLayout>
{
public:
	FBlueprintInterfaceLayout(TWeakPtr<class FBlueprintGlobalOptionsDetails> InGlobalOptionsDetails, bool bInShowsInheritedInterfaces)
		: GlobalOptionsDetailsPtr(InGlobalOptionsDetails)
		, bShowsInheritedInterfaces(bInShowsInheritedInterfaces) {}

private:
	/** IDetailCustomNodeBuilder Interface*/
	virtual void SetOnRebuildChildren( FSimpleDelegate InOnRegenerateChildren ) OVERRIDE {RegenerateChildrenDelegate = InOnRegenerateChildren;}
	virtual void GenerateHeaderRowContent( FDetailWidgetRow& NodeRow ) OVERRIDE;
	virtual void GenerateChildContent( IDetailChildrenBuilder& ChildrenBuilder ) OVERRIDE;
	virtual void Tick( float DeltaTime ) OVERRIDE {}
	virtual bool RequiresTick() const OVERRIDE { return false; }
	virtual FName GetName() const OVERRIDE { return NAME_None; }
	virtual bool InitiallyCollapsed() const OVERRIDE { return false; }
	
private:
	/** Callbacks for details UI */
	void OnBrowseToInterface(TWeakObjectPtr<UObject> Asset);
	void OnRemoveInterface(FString InterfaceName);

	TSharedRef<SWidget> OnGetAddInterfaceMenuContent();
	TSharedRef<ITableRow> GenerateInterfaceListRow( TSharedPtr<FString> InterfaceName, const TSharedRef<STableViewBase>& OwningList );
	void OnInterfaceListSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo);

private:
	/** The parent graph action details customization */
	TWeakPtr<class FBlueprintGlobalOptionsDetails> GlobalOptionsDetailsPtr;

	/** Whether we show inherited interfaces versus implemented interfaces */
	bool bShowsInheritedInterfaces;

	/** List of unimplemented interfaces, for source for a list view */
	TArray<TSharedPtr<FString>> UnimplementedInterfaces;

	/** The add interface combo button */
	TSharedPtr<SComboButton> AddInterfaceComboButton;

	/** A delegate to regenerate this list of children */
	FSimpleDelegate RegenerateChildrenDelegate;
};

/** Details customization for Blueprint settings */
class FBlueprintGlobalOptionsDetails : public IDetailCustomization
{
public:
	/** Constructor */
	FBlueprintGlobalOptionsDetails(TWeakPtr<FBlueprintEditor> InBlueprintEditorPtr)
		:BlueprintEditorPtr(InBlueprintEditorPtr)
	{

	}

	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<class IDetailCustomization> MakeInstance(TWeakPtr<FBlueprintEditor> InBlueprintEditorPtr)
	{
		return MakeShareable(new FBlueprintGlobalOptionsDetails(InBlueprintEditorPtr));
	}

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailLayout ) OVERRIDE;
	
	/** Gets the Blueprint being edited */
	UBlueprint* GetBlueprintObj() const;

	/** Gets the Blueprint editor */
	TWeakPtr<FBlueprintEditor> GetBlueprintEditorPtr() const {return BlueprintEditorPtr;}

protected:
	/** Gets the Blueprint parent class name text */
	FString GetParentClassName() const;

	// Determine whether or not we should be allowed to reparent (but still display the parent class regardless)
	bool CanReparent() const;

	/** Gets the menu content that's displayed when the parent class combo box is clicked */
	TSharedRef<SWidget>	GetParentClassMenuContent();

	/** Delegate called when a class is selected from the class picker */
	void OnClassPicked(UClass* SelectedClass);

private:
	/** Weak reference to the Blueprint editor */
	TWeakPtr<FBlueprintEditor> BlueprintEditorPtr;

	/** Combo button used to choose a parent class */
	TSharedPtr<SComboButton> ParentClassComboButton;
};



/** Details customization for Blueprint Component settings */
class FBlueprintComponentDetails : public IDetailCustomization
{
public:
	/** Constructor */
	FBlueprintComponentDetails(TWeakPtr<FBlueprintEditor> InBlueprintEditorPtr)
		: BlueprintEditorPtr(InBlueprintEditorPtr)
		, bIsVariableNameInvalid(false)
	{

	}

	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<class IDetailCustomization> MakeInstance(TWeakPtr<FBlueprintEditor> InBlueprintEditorPtr)
	{
		return MakeShareable(new FBlueprintComponentDetails(InBlueprintEditorPtr));
	}

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailLayout ) OVERRIDE;

protected:
	/** Accessors passed to parent */
	UBlueprint* GetBlueprintObj() const {return BlueprintEditorPtr.Pin()->GetBlueprintObj();}

	/** Callbacks for widgets */
	FText OnGetVariableText() const;
	void OnVariableTextChanged(const FText& InNewText);
	void OnVariableTextCommitted(const FText& InNewName, ETextCommit::Type InTextCommit);
	FText OnGetTooltipText() const;
	void OnTooltipTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit, FName VarName);
	bool OnVariableCategoryChangeEnabled() const;
	FText OnGetVariableCategoryText() const;
	void OnVariableCategoryTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit, FName VarName);
	void OnVariableCategorySelectionChanged(TSharedPtr<FString> ProposedSelection, ESelectInfo::Type /*SelectInfo*/);
	TSharedRef<ITableRow> MakeVariableCategoryViewWidget(TSharedPtr<FString> Item, const TSharedRef< STableViewBase >& OwnerTable);

	/** Find common base class from current selection */
	UClass* FindCommonBaseClassFromSelected() const;

	/** Build Event Menu for currently selected components */
	TSharedRef<SWidget> BuildEventsMenuForComponents() const;
	
	/** True if the selected node can be attached to sockets */
	bool IsNodeAttachable() const;

	FText GetSocketName() const;
	void OnBrowseSocket();
	void OnClearSocket();
	
	void OnSocketSelection( FName SocketName );

	void PopulateVariableCategories();

private:
	/** Weak reference to the Blueprint editor */
	TWeakPtr<FBlueprintEditor> BlueprintEditorPtr;

	/** The cached tree Node we're editing */
	TSharedPtr<class FSCSEditorTreeNode> CachedNodePtr;

	/** The widget used when in variable name editing mode */ 
	TSharedPtr<SEditableTextBox> VariableNameEditableTextBox;

	/** Flag to indicate whether or not the variable name is invalid */
	bool bIsVariableNameInvalid;

	/** A list of all category names to choose from */
	TArray<TSharedPtr<FString>> VariableCategorySource;

	/** Widgets for the categories */
	TSharedPtr<SComboButton> VariableCategoryComboButton;
	TSharedPtr<SListView<TSharedPtr<FString>>> VariableCategoryListView;
};
