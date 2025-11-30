#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BlendSpaceBuilderSettings.generated.h"

class ULocomotionPatternDataAsset;

UCLASS(config = EditorPerProjectUserSettings, defaultconfig, meta = (DisplayName = "BlendSpace Builder"))
class BLENDSPACEBUILDER_API UBlendSpaceBuilderSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static UBlendSpaceBuilderSettings* Get()
	{
		UBlendSpaceBuilderSettings* Settings = GetMutableDefault<UBlendSpaceBuilderSettings>();
		check(Settings);
		return Settings;
	}

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	UPROPERTY(config, EditAnywhere, Category = "BlendSpace|Axis")
	float DefaultMinSpeed = -500.f;

	UPROPERTY(config, EditAnywhere, Category = "BlendSpace|Axis")
	float DefaultMaxSpeed = 500.f;

	UPROPERTY(config, EditAnywhere, Category = "BlendSpace|Axis")
	FString XAxisName = TEXT("RightVelocity");

	UPROPERTY(config, EditAnywhere, Category = "BlendSpace|Axis")
	FString YAxisName = TEXT("ForwardVelocity");

	UPROPERTY(config, EditAnywhere, Category = "BlendSpace|Pattern")
	TSoftObjectPtr<ULocomotionPatternDataAsset> DefaultPatternAsset;

	UPROPERTY(config, EditAnywhere, Category = "BlendSpace|Preference")
	bool bPreferRootMotionAnimations = true;

	UPROPERTY(config, EditAnywhere, Category = "BlendSpace|Output")
	FString OutputAssetSuffix = TEXT("_Locomotion");
};
