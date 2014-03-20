// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Geometry mode module
 */
class FGeometryModeModule : public IModuleInterface
{
private:
	TSharedPtr<class FEdModeGeometry> EdModeGeometry;
public:
	// IModuleInterface
	virtual void StartupModule() OVERRIDE;
	virtual void ShutdownModule() OVERRIDE;
	// End of IModuleInterface
};



/**
 * Allows for BSP geometry to be edited directly
 */
class FEdModeGeometry : public FEdMode
{
public:
	/**
	 * Struct for cacheing of selected objects components midpoints for reselection when rebuilding the BSP
	 */
	struct HGeomMidPoints
	{
		/** The actor that the verts/edges/polys belong to */
		ABrush* ActualBrush;

		/** Arrays of the midpoints of all the selected verts/edges/polys */
		TArray<FVector> VertexPool;
		TArray<FVector> EdgePool;
		TArray<FVector> PolyPool;	
	};

	static TSharedRef< FEdModeGeometry > Create();
	virtual ~FEdModeGeometry();

	// FEdMode interface
	virtual void Render(const FSceneView* View,FViewport* Viewport,FPrimitiveDrawInterface* PDI) OVERRIDE;
	virtual bool ShowModeWidgets() const OVERRIDE;
	virtual bool UsesToolkits() const OVERRIDE;
	virtual bool ShouldDrawBrushWireframe( AActor* InActor ) const OVERRIDE;
	virtual bool GetCustomDrawingCoordinateSystem( FMatrix& InMatrix, void* InData ) OVERRIDE;
	virtual bool GetCustomInputCoordinateSystem( FMatrix& InMatrix, void* InData ) OVERRIDE;
	virtual void Enter() OVERRIDE;
	virtual void Exit() OVERRIDE;
	virtual void ActorSelectionChangeNotify() OVERRIDE;
	virtual void MapChangeNotify() OVERRIDE;
	virtual void SelectionChanged() OVERRIDE;
	virtual FVector GetWidgetLocation() const;
	// End of FEdMode interface

	// FGCObject interface
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) OVERRIDE;
	// End of FGCObject interface

	void UpdateModifierWindow();

	virtual void GeometrySelectNone(bool bStoreSelection, bool bResetPivot);
	
	/**
	* Returns the number of objects that are selected.
	*/
	virtual int32 CountObjectsSelected();

	/**
	* Returns the number of polygons that are selected.
	*/
	virtual int32 CountSelectedPolygons();

	/**
	* Returns the polygons that are selected.
	*
	* @param	InPolygons	An array to fill with the selected polygons.
	*/
	virtual void GetSelectedPolygons( TArray<FGeomPoly*>& InPolygons );

	/**
	* Returns true if the user has polygons selected.
	*/
	virtual bool HavePolygonsSelected();

	/**
	* Returns the number of edges that are selected.
	*/
	virtual int32 CountSelectedEdges();

	/**
	* Returns the edges that are selected.
	*
	* @param	InEdges	An array to fill with the selected edges.
	*/
	virtual void GetSelectedEdges( TArray<FGeomEdge*>& InEdges );

	/**
	* Returns true if the user has edges selected.
	*/
	virtual bool HaveEdgesSelected();

	/**
	* Returns the number of vertices that are selected.
	*/
	virtual int32 CountSelectedVertices();
	
	/**
	* Returns true if the user has vertices selected.
	*/
	virtual bool HaveVerticesSelected();

	/**
	 * Fills an array with all selected vertices.
	 */
	virtual void GetSelectedVertices( TArray<FGeomVertex*>& InVerts );

	/**
	 * Utility function that allow you to poll and see if certain sub elements are currently selected.
	 *
	 * Returns a combination of the flags in EGeomSelectionStatus.
	 */
	virtual int32 GetSelectionState();

	/**
	 * Cache all the selected geometry on the object, and add to the array if any is found
	 *
	 * Return true if new object has been added to the array.
	 */
	bool CacheSelectedData( TArray<HGeomMidPoints>& raGeomData, const FGeomObject& rGeomObject ) const;

	/**
	 * Attempt to find all the new geometry using the cached data, and cache those new ones out
	 *
	 * Return true everything was found (or there was nothing to find)
	 */
	bool FindFromCache( TArray<HGeomMidPoints>& raGeomData, FGeomObject& rGeomObject, TArray<FGeomBase*>& raSelectedGeom ) const;

	/**
	 * Select all the verts/edges/polys that were found
	 *
	 * Return true if successful
	 */
	bool SelectCachedData( TArray<FGeomBase*>& raSelectedGeom ) const;

	/**
	 * Compiles geometry mode information from the selected brushes.
	 */
	virtual void GetFromSource();
	
	/**
	 * Changes the source brushes to match the current geometry data.
	 */
	virtual void SendToSource();
	virtual bool FinalizeSourceData();
	virtual void PostUndo();
	virtual bool ExecDelete();
	virtual void UpdateInternalData();

	void RenderPoly( const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI );
	void RenderEdge( const FSceneView* View, FPrimitiveDrawInterface* PDI );
	void RenderVertex( const FSceneView* View, FPrimitiveDrawInterface* PDI );

	void ShowModifierWindow(bool bShouldShow);

	/** @name GeomObject iterators */
	//@{
	typedef TArray<FGeomObject*>::TIterator TGeomObjectIterator;
	typedef TArray<FGeomObject*>::TConstIterator TGeomObjectConstIterator;

	TGeomObjectIterator			GeomObjectItor()			{ return TGeomObjectIterator( GeomObjects ); }
	TGeomObjectConstIterator	GeomObjectConstItor() const	{ return TGeomObjectConstIterator( GeomObjects ); }

	// @todo DB: Get rid of these; requires changes to FGeomBase::ParentObjectIndex
	FGeomObject* GetGeomObject(int32 Index)					{ return GeomObjects[ Index ]; }
	const FGeomObject* GetGeomObject(int32 Index) const		{ return GeomObjects[ Index ]; }
	//@}


private:
	FEdModeGeometry();


protected:
	/** 
	 * Custom data compiled when this mode is entered, based on currently
	 * selected brushes.  This data is what is drawn and what the LD
	 * interacts with while in this mode.  Changes done here are
	 * reflected back to the real data in the level at specific times.
	 */
	TArray<FGeomObject*> GeomObjects;
};



class UGeomModifier;

/**
 * Widget manipulation of geometry.
 */
class FModeTool_GeometryModify : public FModeTool
{
public:
	FModeTool_GeometryModify();

	virtual FString GetName() const		{ return TEXT("Modifier"); }

	void SetCurrentModifier( UGeomModifier* InModifier );
	UGeomModifier* GetCurrentModifier();

	int32 GetNumModifiers();

	/**
	 * @return		true if the delta was handled by this editor mode tool.
	 */
	virtual bool InputDelta(FLevelEditorViewportClient* InViewportClient,FViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale);

	virtual bool StartModify();
	virtual bool EndModify();

	virtual void StartTrans();
	virtual void EndTrans();

	virtual void SelectNone();
	virtual bool BoxSelect( FBox& InBox, bool InSelect = true );
	virtual bool FrustumSelect( const FConvexVolume& InFrustum, bool InSelect = true );

	/** @name Modifier iterators */
	//@{
	typedef TArray<UGeomModifier*>::TIterator TModifierIterator;
	typedef TArray<UGeomModifier*>::TConstIterator TModifierConstIterator;

	TModifierIterator		ModifierIterator()				{ return TModifierIterator( Modifiers ); }
	TModifierConstIterator	ModifierConstIterator() const	{ return TModifierConstIterator( Modifiers ); }

	// @todo DB: Get rid of these; requires changes to EditorGeometry.cpp
	UGeomModifier* GetModifier(int32 Index)					{ return Modifiers[ Index ]; }
	const UGeomModifier* GetModifier(int32 Index) const		{ return Modifiers[ Index ]; }
	//@}

	virtual void Tick(FLevelEditorViewportClient* ViewportClient,float DeltaTime);

	/** @return		true if the key was handled by this editor mode tool. */
	virtual bool InputKey(FLevelEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) OVERRIDE;

	virtual void Render(const FSceneView* View,FViewport* Viewport,FPrimitiveDrawInterface* PDI);
	virtual void DrawHUD(FLevelEditorViewportClient* ViewportClient,FViewport* Viewport,const FSceneView* View,FCanvas* Canvas);
	
	/**
	 * Store the current geom selections for all geom objects
	 */
	void StoreAllCurrentGeomSelections();

	/** Used to track when actualy modification takes place */
	bool bGeomModified;

protected:
	/** All available modifiers. */
	TArray<UGeomModifier*> Modifiers;

	/** The current modifier. */
	UGeomModifier* CurrentModifier;	
};


