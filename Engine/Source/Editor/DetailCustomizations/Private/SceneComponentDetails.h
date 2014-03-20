// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

class FSceneComponentDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) OVERRIDE;
	
private:
	void MakeTransformDetails( IDetailLayoutBuilder& DetailBuilder );
	
	/**
	 * When a scene component's Mobility is altered, we need to make sure the scene hierarchy is
	 * updated. Parents can't be more mobile than their children. This means that certain
	 * mobility hierarchy structures are disallowed, like:
	 *
	 *   Movable
	 *   |-Stationary   <-- NOT allowed
	 *   Movable
	 *   |-Static       <-- NOT allowed
	 *   Stationary
	 *   |-Static       <-- NOT allowed
	 *
	 * This method walks the hierarchy and alters parent/child component's Mobility as a result of
	 * this property change.
	 *
	 * @param   MobilityPropertyHandle	A handle to the Mobility property that was altered
	 */
	void OnMobilityChanged(TSharedPtr<IPropertyHandle> MobilityPropertyHandle);

	/** A collection of USceneComponents that were selected last time CustomizeDetails() was ran */
	TArray< TWeakObjectPtr<UObject> > CachedSelectedSceneComponents;
};
