#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetUserData.h"
#include "BlendSpaceFactory.h"
#include "BlendSpaceConfigAssetUserData.generated.h"

/**
 * Metadata for BlendSpace axis configuration.
 * Stores analyzed axis range values.
 */
USTRUCT()
struct FBlendSpaceAxisMetadata
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Axis")
	FString AxisName;

	UPROPERTY(VisibleAnywhere, Category = "Axis")
	float AnalyzedMin = 0.f;

	UPROPERTY(VisibleAnywhere, Category = "Axis")
	float AnalyzedMax = 0.f;

	UPROPERTY(VisibleAnywhere, Category = "Axis")
	int32 GridNum = 4;
};

/**
 * Metadata for BlendSpace sample position.
 * Stores the analyzed position for each animation.
 */
USTRUCT()
struct FBlendSpaceSampleMetadata
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Sample")
	FSoftObjectPath AnimSequence;

	UPROPERTY(VisibleAnywhere, Category = "Sample")
	FVector2D Position = FVector2D::ZeroVector;
};

/**
 * Original Speed data for converted BlendSpace samples.
 * Stores the pre-conversion Speed position and inferred role for each animation.
 */
USTRUCT()
struct FBlendSpaceOriginalSpeedData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "ConversionData")
	FSoftObjectPath AnimSequence;

	UPROPERTY(VisibleAnywhere, Category = "ConversionData")
	FVector2D OriginalSpeedPosition = FVector2D::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = "ConversionData")
	ELocomotionRole InferredRole = ELocomotionRole::Idle;
};

/**
 * Asset User Data for storing BlendSpace build configuration.
 * Attached to BlendSpace assets to preserve the original analysis/build settings.
 */
UCLASS()
class BLENDSPACEBUILDER_API UBlendSpaceConfigAssetUserData : public UAssetUserData
{
	GENERATED_BODY()

public:
	/** Locomotion type used for building this BlendSpace */
	UPROPERTY(VisibleAnywhere, Category = "BuildConfig")
	EBlendSpaceLocomotionType LocomotionType = EBlendSpaceLocomotionType::SpeedBased;

	/** X axis configuration */
	UPROPERTY(VisibleAnywhere, Category = "BuildConfig")
	FBlendSpaceAxisMetadata XAxis;

	/** Y axis configuration */
	UPROPERTY(VisibleAnywhere, Category = "BuildConfig")
	FBlendSpaceAxisMetadata YAxis;

	/** Sample positions from analysis */
	UPROPERTY(VisibleAnywhere, Category = "BuildConfig")
	TArray<FBlendSpaceSampleMetadata> Samples;

	/** Whether analysis was applied during build */
	UPROPERTY(VisibleAnywhere, Category = "AnalysisConfig")
	bool bApplyAnalysis = false;

	/** Analysis type used (RootMotion, LocomotionSimple, LocomotionStride) */
	UPROPERTY(VisibleAnywhere, Category = "AnalysisConfig")
	EBlendSpaceAnalysisType AnalysisType = EBlendSpaceAnalysisType::RootMotion;

	/** Grid divisions used */
	UPROPERTY(VisibleAnywhere, Category = "AnalysisConfig")
	int32 GridDivisions = 4;

	/** Whether samples were snapped to grid */
	UPROPERTY(VisibleAnywhere, Category = "AnalysisConfig")
	bool bSnapToGrid = true;

	// ============== Analyzed Speed Values ==============
	// These are the actual analyzed speeds from animations (before GaitBased conversion)

	/** Analyzed walk animation speed (from WalkForward or similar) */
	UPROPERTY(VisibleAnywhere, Category = "AnalyzedSpeeds")
	float WalkSpeed = 0.f;

	/** Analyzed run animation speed (from RunForward or similar) */
	UPROPERTY(VisibleAnywhere, Category = "AnalyzedSpeeds")
	float RunSpeed = 0.f;

	/** Analyzed sprint animation speed (from SprintForward or similar) */
	UPROPERTY(VisibleAnywhere, Category = "AnalyzedSpeeds")
	float SprintSpeed = 0.f;

	// ============== Conversion Data ==============
	// Data preserved when converting from Speed-based to Gait-based

	/** Whether this BlendSpace was converted from Speed-based format */
	UPROPERTY(VisibleAnywhere, Category = "ConversionData")
	bool bConvertedFromSpeedBased = false;

	/** Original Speed positions for each sample before conversion */
	UPROPERTY(VisibleAnywhere, Category = "ConversionData")
	TArray<FBlendSpaceOriginalSpeedData> OriginalSpeedData;

	/** Original X axis configuration before conversion */
	UPROPERTY(VisibleAnywhere, Category = "ConversionData")
	FBlendSpaceAxisMetadata OriginalXAxis;

	/** Original Y axis configuration before conversion */
	UPROPERTY(VisibleAnywhere, Category = "ConversionData")
	FBlendSpaceAxisMetadata OriginalYAxis;
};
