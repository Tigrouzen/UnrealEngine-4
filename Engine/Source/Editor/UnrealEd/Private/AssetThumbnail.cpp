// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "AssetThumbnail.h"
#include "Runtime/Engine/Public/Slate/SlateTextures.h"
#include "ObjectTools.h"

#include "AssetToolsModule.h"
#include "AssetRegistryModule.h"

class SAssetThumbnail : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SAssetThumbnail )
		: _Style("AssetThumbnail")
		, _ThumbnailPool(NULL)
		, _AllowFadeIn(false)
		, _ForceGenericThumbnail(false)
		, _AllowHintText(true)
		, _HighlightedText( FText::GetEmpty() )
		, _Label(EThumbnailLabel::ClassName)
		, _HintColorAndOpacity( FLinearColor(0.0f, 0.0f, 0.0f, 0.0f) )
		, _ClassThumbnailBrushOverride(NAME_None)
		, _ShowClassBackground( true )
		{}

		SLATE_ARGUMENT( FName, Style )
		SLATE_ARGUMENT( TSharedPtr<FAssetThumbnail>, AssetThumbnail )
		SLATE_ARGUMENT( TSharedPtr<FAssetThumbnailPool>, ThumbnailPool )
		SLATE_ARGUMENT( bool, AllowFadeIn )
		SLATE_ARGUMENT( bool, ForceGenericThumbnail )
		SLATE_ARGUMENT( bool, AllowHintText )
		SLATE_ARGUMENT( EThumbnailLabel::Type, Label )
		SLATE_ATTRIBUTE( FText, HighlightedText )
		SLATE_ATTRIBUTE( FLinearColor, HintColorAndOpacity )
		SLATE_ARGUMENT( FName, ClassThumbnailBrushOverride )
		SLATE_ARGUMENT( bool, ShowClassBackground )

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct( const FArguments& InArgs )
	{
		Style = InArgs._Style;
		HighlightedText = InArgs._HighlightedText;
		Label = InArgs._Label;
		HintColorAndOpacity = InArgs._HintColorAndOpacity;
		AllowHintText = InArgs._AllowHintText;
		ShowClassBackground = InArgs._ShowClassBackground;

		AssetThumbnail = InArgs._AssetThumbnail;
		bHasRenderedThumbnail = false;
		WidthLastFrame = 0;
		GenericThumbnailBorderPadding = 2.f;

		AssetThumbnail->OnAssetDataChanged().AddSP(this, &SAssetThumbnail::OnAssetDataChanged);

		const FAssetData& AssetData = AssetThumbnail->GetAssetData();

		UClass* Class = FindObject<UClass>(ANY_PACKAGE, *AssetData.AssetClass.ToString());
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		TWeakPtr<IAssetTypeActions> AssetTypeActions;
		if ( Class != NULL )
		{
			AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(Class);
		}

		AssetColor = FLinearColor::White;
		if ( AssetTypeActions.IsValid() )
		{
			AssetColor = AssetTypeActions.Pin()->GetTypeColor();
		}

		TSharedRef<SOverlay> OverlayWidget = SNew(SOverlay);

		FName Substyle;
		if ( Class == UClass::StaticClass() )
		{
			Substyle = FName(".ClassBackground");
		}
		else if(AssetTypeActions.IsValid())
		{
			Substyle = FName(".AssetBackground");
		}
		const FName BackgroundBrushName( *(Style.ToString() + Substyle.ToString()) );

		if ( InArgs._ClassThumbnailBrushOverride != NAME_None )
		{
			ClassThumbnailBrushName = InArgs._ClassThumbnailBrushOverride;
		}
		else
		{
			ClassThumbnailBrushName = MakeClassThumbnailName();
		}

		OverlayWidget->AddSlot()
		[
			SNew(SBorder)
			.BorderImage( this, &SAssetThumbnail::GetBackgroundBrush )
			.BorderBackgroundColor( this, &SAssetThumbnail::GetAssetColor )
			.Padding( GenericThumbnailBorderPadding )
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.Visibility(this, &SAssetThumbnail::GetClassThumbnailVisibility)
			[
				SNew( SImage )
				.Image( this, &SAssetThumbnail::GetClassThumbnailBrush )
			]
		];

		// The generic representation of the thumbnail, for use before the rendered version, if it exists
		OverlayWidget->AddSlot()
		[
			SNew(SBorder)
			.BorderImage( this, &SAssetThumbnail::GetBackgroundBrush )
			.BorderBackgroundColor( this, &SAssetThumbnail::GetAssetColor )
			.Padding( GenericThumbnailBorderPadding )
			.VAlign(VAlign_Center) .HAlign(HAlign_Center)
			.Visibility(this, &SAssetThumbnail::GetGenericThumbnailVisibility)
			[
				SAssignNew(LabelTextBlock, STextBlock)
				.Text( GetLabelText() )
				.Font( GetTextFont() )
				.ColorAndOpacity( FEditorStyle::GetColor(Style, ".ColorAndOpacity") )
				.ShadowOffset( FEditorStyle::GetVector(Style, ".ShadowOffset") )
				.ShadowColorAndOpacity( FEditorStyle::GetColor(Style, ".ShadowColorAndOpacity") )
				.WrapTextAt(this, &SAssetThumbnail::GetTextWrapWidth)
				.HighlightText( HighlightedText )
			]
		];

		if ( InArgs._ThumbnailPool.IsValid() && !InArgs._ForceGenericThumbnail && Class != UClass::StaticClass() )
		{
			ViewportFadeAnimation = FCurveSequence();
			ViewportFadeCurve = ViewportFadeAnimation.AddCurve(0.f, 0.25f, ECurveEaseFunction::QuadOut);

			TSharedPtr<SViewport> Viewport = 
				SNew( SViewport )
				.EnableGammaCorrection(false);

			Viewport->SetViewportInterface( AssetThumbnail.ToSharedRef() );
			AssetThumbnail->GetViewportRenderTargetTexture(); // Access the render texture to push it on the stack if it isnt already rendered

			InArgs._ThumbnailPool->OnThumbnailRendered().AddSP(this, &SAssetThumbnail::OnThumbnailRendered);
			InArgs._ThumbnailPool->OnThumbnailRenderFailed().AddSP(this, &SAssetThumbnail::OnThumbnailRenderFailed);

			if ( ShouldRender() && (!InArgs._AllowFadeIn || !InArgs._ThumbnailPool->IsInRenderStack(AssetThumbnail)) )
			{
				bHasRenderedThumbnail = true;
				ViewportFadeAnimation.JumpToEnd();
			}

			// The viewport for the rendered thumbnail, if it exists
			OverlayWidget->AddSlot()
			[
				SNew(SBorder)
				.Padding(2)
				.BorderImage(FEditorStyle::GetBrush(Style, ".Border"))
				//.BorderImage(FEditorStyle::GetBrush("NoBrush"))
				.BorderBackgroundColor(this, &SAssetThumbnail::GetViewportBorderColorAndOpacity)
				.ColorAndOpacity(this, &SAssetThumbnail::GetViewportColorAndOpacity)
				.Visibility(this, &SAssetThumbnail::GetViewportVisibility)
				[
					Viewport.ToSharedRef()
				]
			];
		}

		OverlayWidget->AddSlot()
		.HAlign( HAlign_Center )
		.VAlign( VAlign_Top )
		.Padding(FMargin(2,2,2,2))
		[
			SNew(SBorder)
			.BorderImage( FEditorStyle::GetBrush( Style, ".HintBackground" ) )
			.BorderBackgroundColor( this, &SAssetThumbnail::GetHintBackgroundColor) //Adjust the opacity of the border itself
			.ColorAndOpacity( HintColorAndOpacity ) //adjusts the opacity of the contents of the border
			.Visibility( this, &SAssetThumbnail::GetHintTextVisibility )
			.Padding(0)
			[
				SAssignNew( HintTextBlock, STextBlock )
				.Text( GetLabelText() )
				.Font( GetHintTextFont() )
				.ColorAndOpacity( FEditorStyle::GetColor( Style, ".HintColorAndOpacity" ) )
				.ShadowOffset( FEditorStyle::GetVector( Style, ".HintShadowOffset" ) )
				.ShadowColorAndOpacity( FEditorStyle::GetColor( Style, ".HintShadowColorAndOpacity" ) )
				.WrapTextAt(this, &SAssetThumbnail::GetTextWrapWidth)
				.HighlightText( HighlightedText )
			]
		];

		ChildSlot
		[
			OverlayWidget
		];
	}

	FSlateColor GetHintBackgroundColor() const
	{
		const FLinearColor Color = HintColorAndOpacity.Get();
		return FSlateColor( FLinearColor( Color.R, Color.G, Color.B, FMath::Lerp( 0.0f, 0.5f, Color.A ) ) );
	}

	// SWidget implementation
	virtual void OnMouseEnter( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) OVERRIDE
	{
		SCompoundWidget::OnMouseEnter(MyGeometry, MouseEvent);

		if (!GetDefault<UContentBrowserSettings>()->RealTimeThumbnails )
		{
			// Update hovered thumbnails if we are not already updating them in real-time
			AssetThumbnail->RefreshThumbnail();
		}
	}

	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) OVERRIDE
	{
		SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

		if ( WidthLastFrame != AllottedGeometry.Size.X )
		{
			WidthLastFrame = AllottedGeometry.Size.X;

			// The width changed, update the font
			if ( LabelTextBlock.IsValid() )
			{
				LabelTextBlock->SetFont( GetTextFont() );
			}

			if ( HintTextBlock.IsValid() )
			{
				HintTextBlock->SetFont( GetHintTextFont() );
			}
		}
	}

private:
	void OnAssetDataChanged()
	{
		if ( LabelTextBlock.IsValid() )
		{
			LabelTextBlock->SetText( GetLabelText() );
		}

		if ( HintTextBlock.IsValid() )
		{
			HintTextBlock->SetText( GetLabelText() );
		}
		
		ClassThumbnailBrushName = MakeClassThumbnailName();

		// Check if the asset has a thumbnail.
		const FObjectThumbnail* ObjectThumbnail = NULL;
		FThumbnailMap ThumbnailMap;
		if( AssetThumbnail->GetAsset() )
		{
			FName FullAssetName = FName( *(AssetThumbnail->GetAssetData().GetFullName()) );
			TArray<FName> ObjectNames;
			ObjectNames.Add( FullAssetName );
			ThumbnailTools::ConditionallyLoadThumbnailsForObjects(ObjectNames, ThumbnailMap);
			ObjectThumbnail = ThumbnailMap.Find( FullAssetName );
		}

		bHasRenderedThumbnail = ObjectThumbnail && !ObjectThumbnail->IsEmpty();
		ViewportFadeAnimation.JumpToEnd();
		AssetThumbnail->GetViewportRenderTargetTexture(); // Access the render texture to push it on the stack if it isnt already rendered

		const FAssetData& AssetData = AssetThumbnail->GetAssetData();

		UClass* Class = FindObject<UClass>(ANY_PACKAGE, *AssetData.AssetClass.ToString());
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		TWeakPtr<IAssetTypeActions> AssetTypeActions;
		if ( Class != NULL )
		{
			AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(Class);
		}

		AssetColor = FLinearColor(1.f, 1.f, 1.f, 1.f);
		if ( AssetTypeActions.IsValid() )
		{
			AssetColor = AssetTypeActions.Pin()->GetTypeColor();
		}
	}

	FSlateFontInfo GetTextFont() const
	{
		return FEditorStyle::GetFontStyle( WidthLastFrame <= 64 ? FEditorStyle::Join(Style, ".FontSmall") : FEditorStyle::Join(Style, ".Font") );
	}

	FSlateFontInfo GetHintTextFont() const
	{
		return FEditorStyle::GetFontStyle( WidthLastFrame <= 64 ? FEditorStyle::Join(Style, ".HintFontSmall") : FEditorStyle::Join(Style, ".HintFont") );
	}

	float GetTextWrapWidth() const
	{
		return WidthLastFrame - GenericThumbnailBorderPadding * 2.f;
	}

	const FSlateBrush* GetBackgroundBrush() const
	{
		const FAssetData& AssetData = AssetThumbnail->GetAssetData();

		UClass* Class = FindObject<UClass>(ANY_PACKAGE, *AssetData.AssetClass.ToString());

		if ( Class != nullptr && !ShowClassBackground )
		{
			//return nullptr;
		}
		
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		TWeakPtr<IAssetTypeActions> AssetTypeActions;
		if ( Class != NULL )
		{
			AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(Class);
		}

		FName Substyle;
		if ( Class == UClass::StaticClass() )
		{
			Substyle = FName(".ClassBackground");
		}
		else
		{
			Substyle = FName(".AssetBackground");
		}

		const FName BackgroundBrushName( *(Style.ToString() + Substyle.ToString()) );
		return FEditorStyle::GetBrush(BackgroundBrushName);
	}

	FSlateColor GetAssetColor() const
	{
		return AssetColor;
	}

	FSlateColor GetViewportBorderColorAndOpacity() const
	{
		return FLinearColor(AssetColor.R, AssetColor.G, AssetColor.B, ViewportFadeCurve.GetLerp());
	}

	FLinearColor GetViewportColorAndOpacity() const
	{
		return FLinearColor(1, 1, 1, ViewportFadeCurve.GetLerp());
	}
	
	EVisibility GetViewportVisibility() const
	{
		return bHasRenderedThumbnail ? EVisibility::Visible : EVisibility::Collapsed;
	}

	const FSlateBrush* GetClassThumbnailBrush() const
	{
		return FEditorStyle::GetBrush( ClassThumbnailBrushName );
	}

	EVisibility GetClassThumbnailVisibility() const
	{
		const FSlateBrush* ClassThumbnailBrush = FEditorStyle::GetBrush( ClassThumbnailBrushName );
		if(!bHasRenderedThumbnail && ClassThumbnailBrush != FEditorStyle::GetDefaultBrush())
		{
			const FAssetData& AssetData = AssetThumbnail->GetAssetData();
			FString AssetClass = AssetData.AssetClass.ToString();
			UClass* Class = FindObjectSafe<UClass>(ANY_PACKAGE, *AssetClass);
			if(Class == UClass::StaticClass())
			{
				return EVisibility::Visible;
			}
		}

		return EVisibility::Collapsed;
	}

	EVisibility GetGenericThumbnailVisibility() const
	{
		return (GetClassThumbnailVisibility() == EVisibility::Visible || (bHasRenderedThumbnail && ViewportFadeAnimation.IsAtEnd())) ? EVisibility::Collapsed : EVisibility::Visible;
	}

	EVisibility GetHintTextVisibility() const
	{
		if ( AllowHintText && ( bHasRenderedThumbnail || !LabelTextBlock.IsValid() ) && HintColorAndOpacity.Get().A > 0 )
		{
			return EVisibility::Visible;
		}

		return EVisibility::Collapsed;
	}

	void OnThumbnailRendered(const FAssetData& AssetData)
	{
		if ( !bHasRenderedThumbnail && AssetData == AssetThumbnail->GetAssetData() && ShouldRender() )
		{
			bHasRenderedThumbnail = true;
			ViewportFadeAnimation.Play();
		}
	}

	void OnThumbnailRenderFailed(const FAssetData& AssetData)
	{
		if ( bHasRenderedThumbnail && AssetData == AssetThumbnail->GetAssetData() )
		{
			bHasRenderedThumbnail = false;
		}
	}

	bool ShouldRender() const
	{
		const FAssetData& AssetData = AssetThumbnail->GetAssetData();

		// Never render a thumbnail for an invalid asset
		if ( !AssetData.IsValid() )
		{
			return false;
		}

		if( AssetData.IsAssetLoaded() )
		{
			// Loaded asset, return true if there is a rendering info for it
			UObject* Asset = AssetData.GetAsset();
			FThumbnailRenderingInfo* RenderInfo = GUnrealEd->GetThumbnailManager()->GetRenderingInfo( Asset );
			if ( RenderInfo != NULL && RenderInfo->Renderer != NULL )
			{
				return true;
			}
		}

		const FObjectThumbnail* CachedThumbnail = ThumbnailTools::FindCachedThumbnail(*AssetData.GetFullName());
		if ( CachedThumbnail != NULL )
		{
			// There is a cached thumbnail for this asset, we should render it
			return !CachedThumbnail->IsEmpty();
		}

		if ( AssetData.AssetClass != UBlueprint::StaticClass()->GetFName() )
		{
			// If we are not a blueprint, see if the CDO of the asset's class has a rendering info
			// Blueprints can't do this because the rendering info is based on the generated class
			UClass* AssetClass = FindObject<UClass>(ANY_PACKAGE, *AssetData.AssetClass.ToString());

			if ( AssetClass )
			{
				FThumbnailRenderingInfo* RenderInfo = GUnrealEd->GetThumbnailManager()->GetRenderingInfo( AssetClass->GetDefaultObject() );
				if ( RenderInfo != NULL && RenderInfo->Renderer != NULL )
				{
					return true;
				}
			}
		}
		
		// Unloaded blueprint or asset that may have a custom thumbnail, check to see if there is a thumbnail in the package to render
		FString PackageFilename;
		if ( FPackageName::DoesPackageExist(AssetData.PackageName.ToString(), NULL, &PackageFilename) )
		{
			TSet<FName> ObjectFullNames;
			FThumbnailMap ThumbnailMap;

			FName ObjectFullName = FName(*AssetData.GetFullName());
			ObjectFullNames.Add(ObjectFullName);

			ThumbnailTools::LoadThumbnailsFromPackage(PackageFilename, ObjectFullNames, ThumbnailMap);

			const FObjectThumbnail* ThumbnailPtr = ThumbnailMap.Find(ObjectFullName);
			if (ThumbnailPtr)
			{
				const FObjectThumbnail& ObjectThumbnail = *ThumbnailPtr;
				return ObjectThumbnail.GetImageWidth() > 0 && ObjectThumbnail.GetImageHeight() > 0 && ObjectThumbnail.GetUncompressedImageData().Num() > 0;
			}
		}

		return false;
	}

	FString GetLabelText() const
	{
		if ( Label == EThumbnailLabel::ClassName )
		{
			return GetAssetClassDisplayName();
		}
		else if ( Label == EThumbnailLabel::AssetName ) 
		{
			return GetAssetDisplayName();
		}

		return FString();
	}

	FString GetDisplayNameForClass( UClass* Class ) const
	{
		FText ClassDisplayName;
		if ( Class )
		{
			FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
			TWeakPtr<IAssetTypeActions> AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(Class);
			if ( AssetTypeActions.IsValid() )
			{
				ClassDisplayName = AssetTypeActions.Pin()->GetName();
			}

			if ( ClassDisplayName.IsEmpty() )
			{
				ClassDisplayName = FText::FromString( EngineUtils::SanitizeDisplayName(*Class->GetName(), false) );
			}
		}

		return ClassDisplayName.ToString();
	}

	FString GetAssetClassDisplayName() const
	{
		const FAssetData& AssetData = AssetThumbnail->GetAssetData();
		FString AssetClass = AssetData.AssetClass.ToString();
		UClass* Class = FindObjectSafe<UClass>(ANY_PACKAGE, *AssetClass);

		if ( Class )
		{
			return GetDisplayNameForClass( Class );
		}

		return AssetClass;
	}

	FString GetAssetDisplayName() const
	{
		const FAssetData& AssetData = AssetThumbnail->GetAssetData();

		if ( AssetData.GetClass() == UClass::StaticClass() )
		{
			UClass* Class = Cast<UClass>( AssetData.GetAsset() );
			return GetDisplayNameForClass( Class );
		}

		return AssetData.AssetName.ToString();
	}

	FName MakeClassThumbnailName() const
	{
		const FAssetData& AssetData = AssetThumbnail->GetAssetData();
		return FName(*FString::Printf( TEXT( "ClassThumbnail.%s" ), *AssetData.AssetName.ToString() ));
	}

private:
	TSharedPtr<STextBlock> LabelTextBlock;
	TSharedPtr<STextBlock> HintTextBlock;
	TSharedPtr<FAssetThumbnail> AssetThumbnail;
	FCurveSequence ViewportFadeAnimation;
	FCurveHandle ViewportFadeCurve;

	FLinearColor AssetColor;
	float WidthLastFrame;
	float GenericThumbnailBorderPadding;
	bool bHasRenderedThumbnail;
	FName Style;
	TAttribute< FText > HighlightedText;
	EThumbnailLabel::Type Label;

	TAttribute< FLinearColor > HintColorAndOpacity;
	bool AllowHintText;

	bool ShowClassBackground;

	/** Brush name for rendering the class thumbnail */
	FName ClassThumbnailBrushName;
};




FAssetThumbnail::FAssetThumbnail( UObject* InAsset, uint32 InWidth, uint32 InHeight, const TSharedPtr<class FAssetThumbnailPool>& InThumbnailPool )
	: AssetData ( InAsset ? FAssetData(InAsset) : FAssetData() )
	, Width( InWidth )
	, Height( InHeight )
	, ThumbnailPool( InThumbnailPool )
{
	if ( InThumbnailPool.IsValid() )
	{
		InThumbnailPool->AddReferencer(*this);
	}
}

FAssetThumbnail::FAssetThumbnail( const FAssetData& InAssetData , uint32 InWidth, uint32 InHeight, const TSharedPtr<class FAssetThumbnailPool>& InThumbnailPool )
	: AssetData ( InAssetData )
	, Width( InWidth )
	, Height( InHeight )
	, ThumbnailPool( InThumbnailPool )
{
	if ( InThumbnailPool.IsValid() )
	{
		InThumbnailPool->AddReferencer(*this);
	}
}

FAssetThumbnail::~FAssetThumbnail()
{
	if ( ThumbnailPool.IsValid() )
	{
		ThumbnailPool.Pin()->RemoveReferencer(*this);
	}
}

FIntPoint FAssetThumbnail::GetSize() const
{
	return FIntPoint( Width, Height );
}

FSlateShaderResource* FAssetThumbnail::GetViewportRenderTargetTexture() const
{
	FSlateTexture2DRHIRef* Texture = NULL;
	if ( ThumbnailPool.IsValid() )
	{
		Texture = ThumbnailPool.Pin()->AccessTexture( AssetData, Width, Height );
	}
	if( !Texture || !Texture->IsValid() )
	{
		return NULL;
	}

	return Texture;
}

UObject* FAssetThumbnail::GetAsset() const
{
	if ( AssetData.ObjectPath != NAME_None )
	{
		return FindObject<UObject>(NULL, *AssetData.ObjectPath.ToString());
	}
	else
	{
		return NULL;
	}
}

const FAssetData& FAssetThumbnail::GetAssetData() const
{
	return AssetData;
}

void FAssetThumbnail::SetAsset( const UObject* InAsset )
{
	SetAsset( FAssetData(InAsset) );
}

void FAssetThumbnail::SetAsset( const FAssetData& InAssetData )
{
	if ( ThumbnailPool.IsValid() )
	{
		ThumbnailPool.Pin()->RemoveReferencer(*this);
	}

	if ( InAssetData.IsValid() )
	{
		AssetData = InAssetData;
		if ( ThumbnailPool.IsValid() )
		{
			ThumbnailPool.Pin()->AddReferencer(*this);
		}
	}
	else
	{
		AssetData = FAssetData();
	}

	AssetDataChangedEvent.Broadcast();
}

TSharedRef<SWidget> FAssetThumbnail::MakeThumbnailWidget(
	bool bAllowFadeIn,
	bool bForceGenericThumbnail,
	EThumbnailLabel::Type ThumbnailLabel,
	const TAttribute< FText >& HighlightedText,
	const TAttribute< FLinearColor >& HintColorAndOpacity,
	bool AllowHintText,
	FName ClassThumbnailBrushOverride,
	bool ShowClassBackground )
{
	return
		SNew(SAssetThumbnail)
		.AssetThumbnail( SharedThis(this) )
		.ThumbnailPool(ThumbnailPool.Pin())
		.AllowFadeIn(bAllowFadeIn)
		.ForceGenericThumbnail(bForceGenericThumbnail)
		.Label( ThumbnailLabel )
		.HighlightedText( HighlightedText )
		.HintColorAndOpacity( HintColorAndOpacity )
		.AllowHintText( AllowHintText )
		.ClassThumbnailBrushOverride( ClassThumbnailBrushOverride )
		.ShowClassBackground( ShowClassBackground );
}

void FAssetThumbnail::RefreshThumbnail()
{
	if ( ThumbnailPool.IsValid() && AssetData.IsValid() )
	{
		ThumbnailPool.Pin()->RefreshThumbnail( SharedThis(this) );
	}
}

FAssetThumbnailPool::FAssetThumbnailPool( uint32 InNumInPool, const TAttribute<bool>& InAreRealTimeThumbnailsAllowed, double InMaxFrameTimeAllowance, uint32 InMaxRealTimeThumbnailsPerFrame )
	: NumInPool( InNumInPool )
	, AreRealTimeThumbnailsAllowed( InAreRealTimeThumbnailsAllowed )
	, MaxFrameTimeAllowance( InMaxFrameTimeAllowance )
	, MaxRealTimeThumbnailsPerFrame( InMaxRealTimeThumbnailsPerFrame )
{
	FCoreDelegates::OnObjectPropertyChanged.Add(FCoreDelegates::FOnObjectPropertyChanged::FDelegate::CreateRaw(this, &FAssetThumbnailPool::OnObjectPropertyChanged));
	FCoreDelegates::OnAssetLoaded.Add(FCoreDelegates::FOnAssetLoaded::FDelegate::CreateRaw(this, &FAssetThumbnailPool::OnAssetLoaded));
}

FAssetThumbnailPool::~FAssetThumbnailPool()
{
	FCoreDelegates::OnObjectPropertyChanged.Remove(FCoreDelegates::FOnObjectPropertyChanged::FDelegate::CreateRaw(this, &FAssetThumbnailPool::OnObjectPropertyChanged));
	FCoreDelegates::OnAssetLoaded.Remove(FCoreDelegates::FOnAssetLoaded::FDelegate::CreateRaw(this, &FAssetThumbnailPool::OnAssetLoaded));

	// Release all the texture resources
	ReleaseResources();
}

FAssetThumbnailPool::FThumbnailInfo::~FThumbnailInfo()
{
	if( ThumbnailTexture )
	{
		delete ThumbnailTexture;
		ThumbnailTexture = NULL;
	}

	if( ThumbnailRenderTarget )
	{
		delete ThumbnailRenderTarget;
		ThumbnailRenderTarget = NULL;
	}
}

void FAssetThumbnailPool::ReleaseResources()
{
	// Clear all pending render requests
	ThumbnailsToRenderStack.Empty();
	RealTimeThumbnails.Empty();
	RealTimeThumbnailsToRender.Empty();

	TArray< TSharedRef<FThumbnailInfo> > ThumbnailsToRelease;

	for( auto ThumbIt = ThumbnailToTextureMap.CreateConstIterator(); ThumbIt; ++ThumbIt )
	{
		ThumbnailsToRelease.Add(ThumbIt.Value());
	}
	ThumbnailToTextureMap.Empty();

	for( auto ThumbIt = FreeThumbnails.CreateConstIterator(); ThumbIt; ++ThumbIt )
	{
		ThumbnailsToRelease.Add(*ThumbIt);
	}
	FreeThumbnails.Empty();

	for ( auto ThumbIt = ThumbnailsToRelease.CreateConstIterator(); ThumbIt; ++ThumbIt )
	{
		const TSharedRef<FThumbnailInfo>& Thumb = *ThumbIt;

			// Release rendering resources
			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER( 
				ReleaseThumbnailResources,
				FThumbnailInfo_RenderThread, ThumbInfo, Thumb.Get(),
			{
				ThumbInfo.ThumbnailTexture->ClearTextureData();
				ThumbInfo.ThumbnailTexture->ReleaseResource();
				ThumbInfo.ThumbnailRenderTarget->ReleaseResource();
			});
		}

	// Wait for all resources to be released
	FlushRenderingCommands();

	// Make sure there are no more references to any of our thumbnails now that rendering commands have been flushed
	for ( auto ThumbIt = ThumbnailsToRelease.CreateConstIterator(); ThumbIt; ++ThumbIt )
	{
		const TSharedRef<FThumbnailInfo>& Thumb = *ThumbIt;
		if ( !Thumb.IsUnique() )
		{
			ensureMsgf(0, TEXT("Thumbnail info for '%s' is still referenced by '%d' other objects"), *Thumb->AssetData.ObjectPath.ToString(), Thumb.GetSharedReferenceCount());
		}
	}
}

void FAssetThumbnailPool::Tick( float DeltaTime )
{
	// If there were any assets loaded since last frame that we are currently displaying thumbnails for, push them on the render stack now.
	if ( RecentlyLoadedAssets.Num() > 0 )
	{
		for ( int32 LoadedAssetIdx = 0; LoadedAssetIdx < RecentlyLoadedAssets.Num(); ++LoadedAssetIdx )
		{
			RefreshThumbnailsFor(RecentlyLoadedAssets[LoadedAssetIdx]);
		}

		RecentlyLoadedAssets.Empty();
	}

	// If we have dynamic thumbnails are we are done rendering the last batch of dynamic thumbnails, start a new batch as long as real-time thumbnails are enabled
	const bool bIsInPIEOrSimulate = GEditor->PlayWorld != NULL || GEditor->bIsSimulatingInEditor;
	const bool bShouldUseRealtimeThumbnails = AreRealTimeThumbnailsAllowed.Get() && GetDefault<UContentBrowserSettings>()->RealTimeThumbnails && !bIsInPIEOrSimulate;
	if ( bShouldUseRealtimeThumbnails && RealTimeThumbnails.Num() > 0 && RealTimeThumbnailsToRender.Num() == 0 )
	{
		double CurrentTime = FPlatformTime::Seconds();
		for ( int32 ThumbIdx = RealTimeThumbnails.Num() - 1; ThumbIdx >= 0; --ThumbIdx )
		{
			const TSharedRef<FThumbnailInfo>& Thumb = RealTimeThumbnails[ThumbIdx];
			if ( Thumb->AssetData.IsAssetLoaded() )
			{
				// Only render thumbnails that have been requested recently
				if ( (CurrentTime - Thumb->LastAccessTime) < 1.f )
				{
					RealTimeThumbnailsToRender.Add(Thumb);
				}
			}
			else
			{
				RealTimeThumbnails.RemoveAt(ThumbIdx);
			}
		}
	}

	uint32 NumRealTimeThumbnailsRenderedThisFrame = 0;
	// If there are any thumbnails to render, pop one off the stack and render it.
	if( ThumbnailsToRenderStack.Num() + RealTimeThumbnailsToRender.Num() > 0 )
	{
		double FrameStartTime = FPlatformTime::Seconds();
		// Render as many thumbnails as we are allowed to
		while( ThumbnailsToRenderStack.Num() + RealTimeThumbnailsToRender.Num() > 0 && FPlatformTime::Seconds() - FrameStartTime < MaxFrameTimeAllowance )
		{
			TSharedPtr<FThumbnailInfo> Info;
			if ( ThumbnailsToRenderStack.Num() > 0 )
			{
				Info = ThumbnailsToRenderStack.Pop();
			}
			else if ( FSlateThrottleManager::Get().IsAllowingExpensiveTasks() && RealTimeThumbnailsToRender.Num() > 0 && NumRealTimeThumbnailsRenderedThisFrame < MaxRealTimeThumbnailsPerFrame )
			{
				Info = RealTimeThumbnailsToRender.Pop();
				NumRealTimeThumbnailsRenderedThisFrame++;
			}
			else
			{
				// No thumbnails left to render or we don't want to render any more
				break;
			}

			if( Info.IsValid() )
			{
				TSharedRef<FThumbnailInfo> InfoRef = Info.ToSharedRef();

				if ( InfoRef->AssetData.IsValid() )
				{
					const FObjectThumbnail* ObjectThumbnail = NULL;
					bool bLoadedThumbnail = false;

					// If this is a loaded asset and we have a rendering info for it, render a fresh thumbnail here
					if( InfoRef->AssetData.IsAssetLoaded() )
					{
						UObject* Asset = InfoRef->AssetData.GetAsset();
						FThumbnailRenderingInfo* RenderInfo = GUnrealEd->GetThumbnailManager()->GetRenderingInfo( Asset );
						if ( RenderInfo != NULL && RenderInfo->Renderer != NULL )
						{
							ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
								SyncSlateTextureCommand,
								FThumbnailInfo_RenderThread, ThumbInfo, InfoRef.Get(),
							{
								if ( ThumbInfo.ThumbnailTexture->GetTypedResource() != ThumbInfo.ThumbnailRenderTarget->GetTextureRHI() )
								{
									ThumbInfo.ThumbnailTexture->ClearTextureData();
									ThumbInfo.ThumbnailTexture->ReleaseDynamicRHI();
									ThumbInfo.ThumbnailTexture->SetRHIRef(ThumbInfo.ThumbnailRenderTarget->GetTextureRHI(), ThumbInfo.Width, ThumbInfo.Height);
								}
							});

							//@todo: this should be done on the GPU only but it is not supported by thumbnail tools yet
							ThumbnailTools::RenderThumbnail(
								Asset,
								InfoRef->Width,
								InfoRef->Height,
								ThumbnailTools::EThumbnailTextureFlushMode::NeverFlush,
								InfoRef->ThumbnailRenderTarget
								);

							bLoadedThumbnail = true;

							// Since this was rendered, add it to the list of thumbnails that can be rendered in real-time
							RealTimeThumbnails.AddUnique(InfoRef);
						}
					}
				
					FThumbnailMap ThumbnailMap;
					// If we could not render a fresh thumbnail, see if we already have a cached one to load
					if ( !bLoadedThumbnail )
					{
						// Unloaded asset
						const FObjectThumbnail* FoundThumbnail = ThumbnailTools::FindCachedThumbnail(InfoRef->AssetData.GetFullName());
						if ( FoundThumbnail )
						{
							ObjectThumbnail = FoundThumbnail;
						}
						else
						{
							// If we don't have a cached thumbnail, try to find it on disk
							FString PackageFilename;
							if ( FPackageName::DoesPackageExist(InfoRef->AssetData.PackageName.ToString(), NULL, &PackageFilename) )
							{
								TSet<FName> ObjectFullNames;
								
 
								FName ObjectFullName = FName(*InfoRef->AssetData.GetFullName());
								ObjectFullNames.Add(ObjectFullName);
 
								ThumbnailTools::LoadThumbnailsFromPackage(PackageFilename, ObjectFullNames, ThumbnailMap);
 
								const FObjectThumbnail* ThumbnailPtr = ThumbnailMap.Find(ObjectFullName);
								if (ThumbnailPtr)
								{
									ObjectThumbnail = ThumbnailPtr;
								}
							}
						}
					}

					if ( ObjectThumbnail )
					{
						if ( ObjectThumbnail->GetImageWidth() > 0 && ObjectThumbnail->GetImageHeight() > 0 && ObjectThumbnail->GetUncompressedImageData().Num() > 0 )
						{
							// Make bulk data for updating the texture memory later
							FSlateTextureData* BulkData = new FSlateTextureData(ObjectThumbnail->GetImageWidth(),ObjectThumbnail->GetImageHeight(),GPixelFormats[PF_B8G8R8A8].BlockBytes, ObjectThumbnail->AccessImageData() );

							// Update the texture RHI
							ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
								ClearSlateTextureCommand,
								FThumbnailInfo_RenderThread, ThumbInfo, InfoRef.Get(),
								FSlateTextureData*, BulkData, BulkData,
							{
								if ( ThumbInfo.ThumbnailTexture->GetTypedResource() == ThumbInfo.ThumbnailRenderTarget->GetTextureRHI() )
								{
									ThumbInfo.ThumbnailTexture->SetRHIRef(NULL, ThumbInfo.Width, ThumbInfo.Height);
								}

								ThumbInfo.ThumbnailTexture->SetTextureData( MakeShareable(BulkData) );
								ThumbInfo.ThumbnailTexture->UpdateRHI();
							});

							bLoadedThumbnail = true;
						}
						else
						{
							bLoadedThumbnail = false;
						}
					}

					if ( bLoadedThumbnail )
					{
						// Notify listeners that a thumbnail has been rendered
						ThumbnailRenderedEvent.Broadcast(InfoRef->AssetData);
					}
					else
					{
						// Notify listeners that a thumbnail has been rendered
						ThumbnailRenderFailedEvent.Broadcast(InfoRef->AssetData);
					}
				}
			}
		}
	}
}

FSlateTexture2DRHIRef* FAssetThumbnailPool::AccessTexture( const FAssetData& AssetData, uint32 Width, uint32 Height )
{
	if(AssetData.ObjectPath == NAME_None || Width == 0 || Height == 0)
	{
		return NULL;
	}
	else
	{
		FThumbId ThumbId( AssetData.ObjectPath, Width, Height ) ;
		// Check to see if a thumbnail for this asset exists.  If so we don't need to render it
		const TSharedRef<FThumbnailInfo>* ThumbnailInfoPtr = ThumbnailToTextureMap.Find( ThumbId );
		TSharedPtr<FThumbnailInfo> ThumbnailInfo;
		if( ThumbnailInfoPtr )
		{
			ThumbnailInfo = *ThumbnailInfoPtr;
		}
		else
		{
			// If a the max number of thumbnails allowed by the pool exists then reuse its rendering resource for the new thumbnail
			if( FreeThumbnails.Num() == 0 && ThumbnailToTextureMap.Num() == NumInPool )
			{
				// Find the thumbnail which was rendered last and use it for the new thumbnail
				float LastAccessTime = FLT_MAX;
				const FThumbId* AssetToRemove = NULL;
				for( TMap< FThumbId, TSharedRef<FThumbnailInfo> >::TConstIterator It(ThumbnailToTextureMap); It; ++It )
				{
					if( It.Value()->LastAccessTime < LastAccessTime )
					{
						LastAccessTime = It.Value()->LastAccessTime;
						AssetToRemove = &It.Key();
					}
				}

				check( AssetToRemove );

				// Remove the old mapping 
				ThumbnailInfo = ThumbnailToTextureMap.FindAndRemoveChecked( *AssetToRemove );
			}
			else if( FreeThumbnails.Num() > 0 )
			{
				ThumbnailInfo = FreeThumbnails.Pop();

				ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER( SlateUpdateThumbSizeCommand,
					FSlateTextureRenderTarget2DResource*, ThumbnailRenderTarget, ThumbnailInfo->ThumbnailRenderTarget,
					uint32, Width, Width,
					uint32, Height, Height,
				{
					ThumbnailRenderTarget->SetSize(Width, Height);
				});
			}
			else
			{
				// There are no free thumbnail resources
				check( (uint32)ThumbnailToTextureMap.Num() <= NumInPool );
				check( !ThumbnailInfo.IsValid() );
				// The pool isn't used up so just make a new texture 

				// Make new thumbnail info if it doesn't exist
				// This happens when the pool is not yet full
				ThumbnailInfo = MakeShareable( new FThumbnailInfo );
				
				// Set the thumbnail and asset on the info. It is NOT safe to change or NULL these pointers until ReleaseResources.
				ThumbnailInfo->ThumbnailTexture = new FSlateTexture2DRHIRef( Width, Height, PF_B8G8R8A8, NULL, TexCreate_Dynamic );
				ThumbnailInfo->ThumbnailRenderTarget = new FSlateTextureRenderTarget2DResource(FLinearColor::Black, Width, Height, PF_B8G8R8A8, SF_Point, TA_Wrap, TA_Wrap, 0.0f);

				BeginInitResource( ThumbnailInfo->ThumbnailTexture );
				BeginInitResource( ThumbnailInfo->ThumbnailRenderTarget );
			}


			check( ThumbnailInfo.IsValid() );
			TSharedRef<FThumbnailInfo> ThumbnailRef = ThumbnailInfo.ToSharedRef();

			// Map the object to its thumbnail info
			ThumbnailToTextureMap.Add( ThumbId, ThumbnailRef );

			ThumbnailInfo->AssetData = AssetData;
			ThumbnailInfo->Width = Width;
			ThumbnailInfo->Height = Height;
		
			// Request that the thumbnail be rendered as soon as possible
			ThumbnailsToRenderStack.Push( ThumbnailRef );
		}

		// This thumbnail was accessed, update its last time to the current time
		// We'll use LastAccessTime to determine the order to recycle thumbnails if the pool is full
		ThumbnailInfo->LastAccessTime = FPlatformTime::Seconds();

		return ThumbnailInfo->ThumbnailTexture;
	}
}

void FAssetThumbnailPool::AddReferencer( const FAssetThumbnail& AssetThumbnail )
{
	FIntPoint Size = AssetThumbnail.GetSize();
	if ( AssetThumbnail.GetAssetData().ObjectPath == NAME_None || Size.X == 0 || Size.Y == 0 )
	{
		// Invalid referencer
		return;
	}

	// Generate a key and look up the number of references in the RefCountMap
	FThumbId ThumbId( AssetThumbnail.GetAssetData().ObjectPath, Size.X, Size.Y ) ;
	int32* RefCountPtr = RefCountMap.Find(ThumbId);

	if ( RefCountPtr )
	{
		// Already in the map, increment a reference
		(*RefCountPtr)++;
	}
	else
	{
		// New referencer, add it to the map with a RefCount of 1
		RefCountMap.Add(ThumbId, 1);
	}
}

void FAssetThumbnailPool::RemoveReferencer( const FAssetThumbnail& AssetThumbnail )
{
	FIntPoint Size = AssetThumbnail.GetSize();
	const FName ObjectPath = AssetThumbnail.GetAssetData().ObjectPath;
	if ( ObjectPath == NAME_None || Size.X == 0 || Size.Y == 0 )
	{
		// Invalid referencer
		return;
	}

	// Generate a key and look up the number of references in the RefCountMap
	FThumbId ThumbId( ObjectPath, Size.X, Size.Y ) ;
	int32* RefCountPtr = RefCountMap.Find(ThumbId);

	// This should complement an AddReferencer so this entry should be in the map
	if ( RefCountPtr )
	{
		// Decrement the RefCount
		(*RefCountPtr)--;

		// If we reached zero, free the thumbnail and remove it from the map.
		if ( (*RefCountPtr) <= 0 )
		{
			RefCountMap.Remove(ThumbId);
			FreeThumbnail(ObjectPath, Size.X, Size.Y);
		}
	}
	else
	{
		// This FAssetThumbnail did not reference anything or was deleted after the pool was deleted.
	}
}

bool FAssetThumbnailPool::IsInRenderStack( const TSharedPtr<FAssetThumbnail>& Thumbnail ) const
{
	const FAssetData& AssetData = Thumbnail->GetAssetData();
	const uint32 Width = Thumbnail->GetSize().X;
	const uint32 Height = Thumbnail->GetSize().Y;

	if ( ensure(AssetData.ObjectPath != NAME_None) && ensure(Width > 0) && ensure(Height > 0) )
	{
		FThumbId ThumbId( AssetData.ObjectPath, Width, Height ) ;
		const TSharedRef<FThumbnailInfo>* ThumbnailInfoPtr = ThumbnailToTextureMap.Find( ThumbId );
		if ( ThumbnailInfoPtr )
		{
			return ThumbnailsToRenderStack.Contains(*ThumbnailInfoPtr);
		}
	}

	return false;
}

void FAssetThumbnailPool::PrioritizeThumbnails( const TArray< TSharedPtr<FAssetThumbnail> >& ThumbnailsToPrioritize, uint32 Width, uint32 Height )
{
	if ( ensure(Width > 0) && ensure(Height > 0) )
	{
		TSet<FName> ObjectPathList;
		for ( int32 ThumbIdx = 0; ThumbIdx < ThumbnailsToPrioritize.Num(); ++ThumbIdx )
		{
			ObjectPathList.Add(ThumbnailsToPrioritize[ThumbIdx]->GetAssetData().ObjectPath);
		}

		TArray< TSharedRef<FThumbnailInfo> > FoundThumbnails;
		for ( int32 ThumbIdx = ThumbnailsToRenderStack.Num() - 1; ThumbIdx >= 0; --ThumbIdx )
		{
			const TSharedRef<FThumbnailInfo>& ThumbnailInfo = ThumbnailsToRenderStack[ThumbIdx];
			if ( ThumbnailInfo->Width == Width && ThumbnailInfo->Height == Height && ObjectPathList.Contains(ThumbnailInfo->AssetData.ObjectPath) )
			{
				FoundThumbnails.Add(ThumbnailInfo);
				ThumbnailsToRenderStack.RemoveAt(ThumbIdx);
			}
		}

		for ( int32 ThumbIdx = 0; ThumbIdx < FoundThumbnails.Num(); ++ThumbIdx )
		{
			ThumbnailsToRenderStack.Push(FoundThumbnails[ThumbIdx]);
		}
	}
}

void FAssetThumbnailPool::RefreshThumbnail( const TSharedPtr<FAssetThumbnail>& ThumbnailToRefresh )
{
	const FAssetData& AssetData = ThumbnailToRefresh->GetAssetData();
	const uint32 Width = ThumbnailToRefresh->GetSize().X;
	const uint32 Height = ThumbnailToRefresh->GetSize().Y;

	if ( ensure(AssetData.ObjectPath != NAME_None) && ensure(Width > 0) && ensure(Height > 0) )
	{
		FThumbId ThumbId( AssetData.ObjectPath, Width, Height ) ;
		const TSharedRef<FThumbnailInfo>* ThumbnailInfoPtr = ThumbnailToTextureMap.Find( ThumbId );
		if ( ThumbnailInfoPtr )
		{
			ThumbnailsToRenderStack.AddUnique(*ThumbnailInfoPtr);
		}
	}
}

void FAssetThumbnailPool::FreeThumbnail( const FName& ObjectPath, uint32 Width, uint32 Height )
{
	if(ObjectPath != NAME_None && Width != 0 && Height != 0)
	{
		FThumbId ThumbId( ObjectPath, Width, Height ) ;

		const TSharedRef<FThumbnailInfo>* ThumbnailInfoPtr = ThumbnailToTextureMap.Find(ThumbId);
		if( ThumbnailInfoPtr )
		{
			TSharedRef<FThumbnailInfo> ThumbnailInfo = *ThumbnailInfoPtr;
			ThumbnailToTextureMap.Remove(ThumbId);
			ThumbnailsToRenderStack.Remove(ThumbnailInfo);
			RealTimeThumbnails.Remove(ThumbnailInfo);
			RealTimeThumbnailsToRender.Remove(ThumbnailInfo);

			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER( 
				ReleaseThumbnailTextureData,
				FSlateTexture2DRHIRef*, ThumbnailTexture, ThumbnailInfo->ThumbnailTexture,
			{
				ThumbnailTexture->ClearTextureData();
			});

			FreeThumbnails.Add( ThumbnailInfo );
		}
	}
			
}

void FAssetThumbnailPool::RefreshThumbnailsFor( FName ObjectPath )
{
	for ( auto ThumbIt = ThumbnailToTextureMap.CreateIterator(); ThumbIt; ++ThumbIt)
	{
		if ( ThumbIt.Key().ObjectPath == ObjectPath )
		{
			ThumbnailsToRenderStack.Push( ThumbIt.Value() );
		}
	}
}

void FAssetThumbnailPool::OnAssetLoaded( UObject* Asset )
{
	if ( Asset != NULL )
	{
		RecentlyLoadedAssets.Add( FName(*Asset->GetPathName()) );
	}
}

void FAssetThumbnailPool::OnObjectPropertyChanged( UObject* ObjectBeingModified )
{
	if ( ObjectBeingModified->HasAnyFlags(RF_ClassDefaultObject) && ObjectBeingModified->GetClass()->ClassGeneratedBy != NULL )
	{
		// This is a blueprint modification. Check to see if this thumbnail is the blueprint of the modified CDO
		ObjectBeingModified = ObjectBeingModified->GetClass()->ClassGeneratedBy;
	}

	RefreshThumbnailsFor( FName(*ObjectBeingModified->GetPathName()) );
}
