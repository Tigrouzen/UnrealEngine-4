// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DrawFrsutumComponent.cpp: UDrawFrsutumComponent implementation.
=============================================================================*/

#include "EnginePrivate.h"
#include "LevelUtils.h"


/** Represents a draw frustum to the scene manager. */
class FDrawFrustumSceneProxy : public FPrimitiveSceneProxy
{
public:

	/** 
	* Initialization constructor. 
	* @param	InComponent - game component to draw in the scene
	*/
	FDrawFrustumSceneProxy(const UDrawFrustumComponent* InComponent)
	:	FPrimitiveSceneProxy(InComponent)
	,	FrustumColor(InComponent->FrustumColor)
	,	FrustumAngle(InComponent->FrustumAngle)
	,	FrustumAspectRatio(InComponent->FrustumAspectRatio)
	,	FrustumStartDist(InComponent->FrustumStartDist)
	,	FrustumEndDist(InComponent->FrustumEndDist)
	{		
		bWillEverBeLit = false;
	}

	// FPrimitiveSceneProxy interface.

	/** 
	* Draw the scene proxy as a dynamic element
	*
	* @param	PDI - draw interface to render to
	* @param	View - current view
	*/
	virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View)
	{
		QUICK_SCOPE_CYCLE_COUNTER( STAT_DrawFrustumSceneProxy_DrawDynamicElements );

		FVector Direction(1,0,0);
		FVector LeftVector(0,1,0);
		FVector UpVector(0,0,1);

		FVector Verts[8];

		// FOVAngle controls the horizontal angle.
		float HozHalfAngle = (FrustumAngle) * ((float)PI/360.f);
		float HozLength = FrustumStartDist * FMath::Tan(HozHalfAngle);
		float VertLength = HozLength/FrustumAspectRatio;

		// near plane verts
		Verts[0] = (Direction * FrustumStartDist) + (UpVector * VertLength) + (LeftVector * HozLength);
		Verts[1] = (Direction * FrustumStartDist) + (UpVector * VertLength) - (LeftVector * HozLength);
		Verts[2] = (Direction * FrustumStartDist) - (UpVector * VertLength) - (LeftVector * HozLength);
		Verts[3] = (Direction * FrustumStartDist) - (UpVector * VertLength) + (LeftVector * HozLength);

		HozLength = FrustumEndDist * FMath::Tan(HozHalfAngle);
		VertLength = HozLength/FrustumAspectRatio;

		// far plane verts
		Verts[4] = (Direction * FrustumEndDist) + (UpVector * VertLength) + (LeftVector * HozLength);
		Verts[5] = (Direction * FrustumEndDist) + (UpVector * VertLength) - (LeftVector * HozLength);
		Verts[6] = (Direction * FrustumEndDist) - (UpVector * VertLength) - (LeftVector * HozLength);
		Verts[7] = (Direction * FrustumEndDist) - (UpVector * VertLength) + (LeftVector * HozLength);

		for( int32 x = 0 ; x < 8 ; ++x )
		{
			Verts[x] = GetLocalToWorld().TransformPosition( Verts[x] );
		}

		const uint8 DepthPriorityGroup = GetDepthPriorityGroup(View);
		PDI->DrawLine( Verts[0], Verts[1], FrustumColor, DepthPriorityGroup );
		PDI->DrawLine( Verts[1], Verts[2], FrustumColor, DepthPriorityGroup );
		PDI->DrawLine( Verts[2], Verts[3], FrustumColor, DepthPriorityGroup );
		PDI->DrawLine( Verts[3], Verts[0], FrustumColor, DepthPriorityGroup );

		PDI->DrawLine( Verts[4], Verts[5], FrustumColor, DepthPriorityGroup );
		PDI->DrawLine( Verts[5], Verts[6], FrustumColor, DepthPriorityGroup );
		PDI->DrawLine( Verts[6], Verts[7], FrustumColor, DepthPriorityGroup );
		PDI->DrawLine( Verts[7], Verts[4], FrustumColor, DepthPriorityGroup );

		PDI->DrawLine( Verts[0], Verts[4], FrustumColor, DepthPriorityGroup );
		PDI->DrawLine( Verts[1], Verts[5], FrustumColor, DepthPriorityGroup );
		PDI->DrawLine( Verts[2], Verts[6], FrustumColor, DepthPriorityGroup );
		PDI->DrawLine( Verts[3], Verts[7], FrustumColor, DepthPriorityGroup );
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View)
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View) && View->Family->EngineShowFlags.CameraFrustums;
		Result.bDynamicRelevance = true;
		Result.bShadowRelevance = IsShadowCast(View);
		Result.bEditorPrimitiveRelevance = UseEditorCompositing(View);
		return Result;
	}

	virtual uint32 GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }
	uint32 GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }

private:
	FColor FrustumColor;
	float FrustumAngle;
	float FrustumAspectRatio;
	float FrustumStartDist;
	float FrustumEndDist;
};

UDrawFrustumComponent::UDrawFrustumComponent(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	FrustumColor = FColor(255, 0, 255, 255);

	FrustumAngle = 90.0f;
	FrustumAspectRatio = 1.333333f;
	FrustumStartDist = 100.0f;
	FrustumEndDist = 1000.0f;
	bUseEditorCompositing = true;
	bHiddenInGame = true;
	BodyInstance.bEnableCollision_DEPRECATED = false;
	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	bGenerateOverlapEvents = false;
}

FPrimitiveSceneProxy* UDrawFrustumComponent::CreateSceneProxy()
{
	return new FDrawFrustumSceneProxy(this);
}


FBoxSphereBounds UDrawFrustumComponent::CalcBounds(const FTransform & LocalToWorld) const
{
	return FBoxSphereBounds( LocalToWorld.TransformPosition(FVector::ZeroVector), FVector(FrustumEndDist), FrustumEndDist );
}
