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
};
