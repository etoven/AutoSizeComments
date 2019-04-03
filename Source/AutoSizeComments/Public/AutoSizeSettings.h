// Copyright 2018 fpwong, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "AutoSizeSettings.generated.h"

USTRUCT()
struct FPresetCommentStyle 
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, config, Category = Default)
	FLinearColor Color = FLinearColor::White;

	UPROPERTY(EditAnywhere, config, Category = Default)
	int FontSize = 18;
};


UCLASS(config = EditorSettings)
class AUTOSIZECOMMENTS_API UAutoSizeSettings : public UObject
{
	GENERATED_BODY()
		
public:
	UAutoSizeSettings(const FObjectInitializer& ObjectInitializer);

	/** Amount of padding for around the contents of a comment node */
	UPROPERTY(EditAnywhere, config, Category = Default)
	FVector2D CommentNodePadding;

	/** If enabled, comment boxes will spawn with a random color. If disabled, use default color */
	UPROPERTY(EditAnywhere, config, Category = Default)
	bool bUseRandomColor;

	/** If Use Random Color is not enabled, comment boxes will spawn with this default color */
	UPROPERTY(EditAnywhere, config, Category = Default)
	FLinearColor DefaultCommentColor;

	/** Color for comments which are floating (they do not resize to nodes) */
	UPROPERTY(EditAnywhere, config, Category = Default)
	FLinearColor FloatingColor;

	/** Preset styles (each style will have its own button on the comment box) */
	UPROPERTY(EditAnywhere, config, Category = Default)
	TArray<FPresetCommentStyle> PresetStyles;

	/** Set all nodes in the graph to the default color */
	UPROPERTY(EditAnywhere, config, Category = Default)
	bool bAggressivelyUseDefaultColor;

	/** If enabled, empty comment boxes will move out of the way of other comment boxes */
	UPROPERTY(EditAnywhere, config, Category = Default)
	bool bMoveEmptyCommentBoxes;

	/** The speed at which empty comment boxes move */
	UPROPERTY(EditAnywhere, config, Category = Default)
	float EmptyCommentBoxSpeed;

	/** If enabled, this should set "Color Bubble" to true for every comment box that is created or loaded */
	UPROPERTY(EditAnywhere, config, Category = Default)
	bool bForceColorCommentBubbles;
};
