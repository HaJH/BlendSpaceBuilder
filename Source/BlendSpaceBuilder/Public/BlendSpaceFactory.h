#pragma once

#include "CoreMinimal.h"
#include "BlendSpaceBuilderSettings.h"

class UBlendSpace;
class UAnimSequence;
class USkeleton;

/** Analysis type for BlendSpace axis calculation */
enum class EBlendSpaceAnalysisType : uint8
{
	/** Root Motion based velocity analysis */
	RootMotion,
	/** Foot movement based locomotion analysis - simple average of velocities */
	LocomotionSimple,
	/** Foot movement based locomotion analysis - stride length / play time */
	LocomotionStride,
};

struct FBlendSpaceBuildConfig
{
	USkeleton* Skeleton = nullptr;

	float XAxisMin = -500.f;
	float XAxisMax = 500.f;
	float YAxisMin = -500.f;
	float YAxisMax = 500.f;

	FString XAxisName = TEXT("RightVelocity");
	FString YAxisName = TEXT("ForwardVelocity");

	FString PackagePath;
	FString AssetName;

	TMap<ELocomotionRole, UAnimSequence*> SelectedAnimations;

	/** Whether to use pre-analyzed positions (from UI Analyze button) */
	bool bApplyAnalysis = true;

	/** Analysis type to use (RootMotion or Locomotion) */
	EBlendSpaceAnalysisType AnalysisType = EBlendSpaceAnalysisType::RootMotion;

	/** Left foot bone name for Locomotion analysis */
	FName LeftFootBoneName = NAME_None;

	/** Right foot bone name for Locomotion analysis */
	FName RightFootBoneName = NAME_None;

	/** Whether to open the asset in editor after creation */
	bool bOpenInEditor = true;

	/** Pre-analyzed sample positions (calculated by UI Analyze button) */
	TMap<UAnimSequence*, FVector> PreAnalyzedPositions;

	/** Grid divisions (applied to both X and Y axes) */
	int32 GridDivisions = 4;

	/** Snap samples to grid */
	bool bSnapToGrid = true;
};

class BLENDSPACEBUILDER_API FBlendSpaceFactory
{
public:
	static UBlendSpace* CreateLocomotionBlendSpace(const FBlendSpaceBuildConfig& Config);

	/**
	 * Analyze animations and calculate sample positions.
	 * Call this from UI before Create to preview/validate analysis results.
	 * @param StrideMultiplier Multiplier for Stride analysis (default 1.4 to compensate for underestimation)
	 * @return Map of Animation -> calculated position (X=Right, Y=Forward, Z=0)
	 */
	static TMap<UAnimSequence*, FVector> AnalyzeSamplePositions(
		const TMap<ELocomotionRole, UAnimSequence*>& Animations,
		EBlendSpaceAnalysisType AnalysisType,
		FName LeftFootBone = NAME_None,
		FName RightFootBone = NAME_None,
		float StrideMultiplier = 1.0f);

	/**
	 * Calculate symmetric axis range from analyzed positions with padding.
	 * @param GridDivisions Number of grid divisions (affects step size calculation)
	 * @param bUseNiceNumbers If true, rounds to nice numbers (10, 25, 50...); if false, uses exact step
	 */
	static void CalculateAxisRangeFromAnalysis(
		const TMap<UAnimSequence*, FVector>& AnalyzedPositions,
		int32 GridDivisions,
		bool bUseNiceNumbers,
		float& OutMinX, float& OutMaxX, float& OutMinY, float& OutMaxY);

private:
	static UBlendSpace* CreateBlendSpaceAsset(const FString& PackagePath, const FString& AssetName, USkeleton* Skeleton);
	static void ConfigureAxes(UBlendSpace* BlendSpace, const FBlendSpaceBuildConfig& Config);
	static void AddSampleToBlendSpace(UBlendSpace* BlendSpace, UAnimSequence* Animation, const FVector& Position);
	static void FinalizeAndSave(UBlendSpace* BlendSpace);
	static FVector2D GetPositionForRole(ELocomotionRole Role, const FBlendSpaceBuildConfig& Config);

	/** Open the BlendSpace asset in editor */
	static void OpenAssetInEditor(UBlendSpace* BlendSpace);
};
