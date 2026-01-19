#pragma once

#include "CoreMinimal.h"
#include "BlendSpaceBuilderSettings.h"
#include "BlendSpaceFactory.h"

class UBlendSpace;
class UAnimSequence;

/**
 * Configuration for Gait conversion process.
 */
struct FGaitConversionConfig
{
	/** Threshold for idle detection (cm/s) */
	float IdleSpeedThreshold = 25.f;

	/** Ratio between walk and run speed for classification (0.0 ~ 1.0) */
	float WalkToRunSpeedRatio = 0.6f;

	/** Whether to create a copy instead of modifying original */
	bool bCreateCopy = true;

	/** Suffix to append when creating a copy */
	FString OutputSuffix = TEXT("_Gait");

	/** Whether to open the result in editor */
	bool bOpenInEditor = true;
};

/**
 * Result of Gait conversion analysis or conversion.
 */
struct FGaitConversionResult
{
	bool bSuccess = false;
	FString ErrorMessage;

	/** Map of animation to inferred role */
	TMap<UAnimSequence*, ELocomotionRole> InferredRoles;

	/** Map of animation to original Speed position */
	TMap<UAnimSequence*, FVector2D> OriginalSpeedPositions;

	/** Map of animation to new Gait position */
	TMap<UAnimSequence*, FVector2D> NewGaitPositions;

	/** Analyzed walk speed (max of Walk roles) */
	float AnalyzedWalkSpeed = 0.f;

	/** Analyzed run speed (max of Run roles) */
	float AnalyzedRunSpeed = 0.f;

	/** Original axis ranges */
	float OriginalXMin = 0.f;
	float OriginalXMax = 0.f;
	float OriginalYMin = 0.f;
	float OriginalYMax = 0.f;
};

/**
 * Converter for Speed-based BlendSpace to Gait-based format.
 * Handles sample role inference, position remapping, and metadata preservation.
 */
class BLENDSPACEBUILDER_API FBlendSpaceGaitConverter
{
public:
	/**
	 * Analyze BlendSpace without modification.
	 * Use for preview/validation before conversion.
	 */
	static FGaitConversionResult AnalyzeBlendSpace(UBlendSpace* BlendSpace, const FGaitConversionConfig& Config = FGaitConversionConfig());

	/**
	 * Convert Speed-based BlendSpace to Gait-based format.
	 * @param Source Source BlendSpace to convert
	 * @param Config Conversion configuration
	 * @param OutResult Detailed conversion result
	 * @return Converted BlendSpace (copy if bCreateCopy=true, otherwise modified original)
	 */
	static UBlendSpace* ConvertToGaitBased(
		UBlendSpace* Source,
		const FGaitConversionConfig& Config,
		FGaitConversionResult& OutResult);

	/**
	 * Infer locomotion role from Speed position.
	 * @param SpeedPosition Sample position in Speed space (X=Right, Y=Forward)
	 * @param MaxForwardSpeed Maximum forward speed in the BlendSpace
	 * @param MaxBackwardSpeed Maximum backward speed (absolute value)
	 * @param MaxRightSpeed Maximum right speed
	 * @param MaxLeftSpeed Maximum left speed (absolute value)
	 * @param Config Conversion configuration
	 * @return Inferred locomotion role
	 */
	static ELocomotionRole InferRoleFromSpeedPosition(
		const FVector2D& SpeedPosition,
		float MaxForwardSpeed, float MaxBackwardSpeed,
		float MaxRightSpeed, float MaxLeftSpeed,
		const FGaitConversionConfig& Config);

	/**
	 * Check if BlendSpace is Speed-based (suitable for conversion).
	 */
	static bool IsSpeedBasedBlendSpace(const UBlendSpace* BlendSpace);

private:
	/** Create a copy of the BlendSpace in the same folder */
	static UBlendSpace* CreateBlendSpaceCopy(UBlendSpace* Source, const FString& Suffix);

	/** Apply Gait-based axis configuration */
	static void ConfigureGaitAxes(UBlendSpace* BlendSpace);

	/** Get Gait position for a locomotion role */
	static FVector2D GetGaitPositionForRole(ELocomotionRole Role);

	/** Determine gait tier (Walk/Run) based on normalized speed */
	static bool IsRunGait(float NormalizedSpeed, const FGaitConversionConfig& Config);

	/** Calculate direction angle from Speed position (returns degrees, 0=Right, 90=Forward) */
	static float GetDirectionAngle(const FVector2D& SpeedPosition);

	/** Determine direction enum from angle */
	enum class EDirectionCategory : uint8
	{
		Forward,
		ForwardLeft,
		Left,
		BackwardLeft,
		Backward,
		BackwardRight,
		Right,
		ForwardRight
	};
	static EDirectionCategory GetDirectionCategory(float AngleDegrees);

	/** Save conversion metadata to BlendSpace */
	static void SaveConversionMetadata(
		UBlendSpace* BlendSpace,
		const FGaitConversionResult& Result,
		float OriginalXMin, float OriginalXMax,
		float OriginalYMin, float OriginalYMax);
};
