#pragma once

#include "CoreMinimal.h"
#include "BlendSpaceBuilderSettings.h"

class UBlendSpace;
class UAnimSequence;
class USkeleton;

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

	/** Whether to apply UE5 BlendSpace Analysis after creation */
	bool bApplyAnalysis = true;
};

class BLENDSPACEBUILDER_API FBlendSpaceFactory
{
public:
	static UBlendSpace* CreateLocomotionBlendSpace(const FBlendSpaceBuildConfig& Config);

private:
	static UBlendSpace* CreateBlendSpaceAsset(const FString& PackagePath, const FString& AssetName, USkeleton* Skeleton);
	static void ConfigureAxes(UBlendSpace* BlendSpace, const FBlendSpaceBuildConfig& Config);
	static void AddSampleToBlendSpace(UBlendSpace* BlendSpace, UAnimSequence* Animation, const FVector& Position);
	static void FinalizeAndSave(UBlendSpace* BlendSpace);
	static FVector2D GetPositionForRole(ELocomotionRole Role, const FBlendSpaceBuildConfig& Config);

	/** Calculate root motion velocity from animation (X=Right, Y=Forward) */
	static FVector2D CalculateRootMotionVelocity(const UAnimSequence* Animation);

	/** Apply root motion analysis to the created blend space */
	static void ApplyAnalysisToBlendSpace(UBlendSpace* BlendSpace);

	/** Auto-adjust axis range based on analyzed sample positions */
	static void AutoAdjustAxisRange(UBlendSpace* BlendSpace);
};
