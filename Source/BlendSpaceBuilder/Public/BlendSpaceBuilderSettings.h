#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BlendSpaceBuilderSettings.generated.h"

UENUM(BlueprintType)
enum class ELocomotionRole : uint8
{
	Idle,
	WalkForward,
	WalkBackward,
	WalkLeft,
	WalkRight,
	WalkForwardLeft,
	WalkForwardRight,
	WalkBackwardLeft,
	WalkBackwardRight,
	RunForward,
	RunBackward,
	RunLeft,
	RunRight,
	RunForwardLeft,
	RunForwardRight,
	RunBackwardLeft,
	RunBackwardRight,
	SprintForward,
	Custom,
	MAX UMETA(Hidden)
};

USTRUCT(BlueprintType)
struct FLocomotionPatternEntry
{
	GENERATED_BODY()

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly)
	FString NamePattern;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly)
	bool bCaseInsensitive = true;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly)
	ELocomotionRole Role = ELocomotionRole::Idle;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Role == ELocomotionRole::Custom"))
	FVector2D CustomPosition = FVector2D::ZeroVector;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly)
	int32 Priority = 0;
};

USTRUCT(BlueprintType)
struct FLocomotionSpeedTier
{
	GENERATED_BODY()

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly)
	FString TierName;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly)
	float Speed = 300.f;
};

UCLASS(config = EditorPerProjectUserSettings, defaultconfig, meta = (DisplayName = "BlendSpace Builder"))
class BLENDSPACEBUILDER_API UBlendSpaceBuilderSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UBlendSpaceBuilderSettings();

	static UBlendSpaceBuilderSettings* Get()
	{
		UBlendSpaceBuilderSettings* Settings = GetMutableDefault<UBlendSpaceBuilderSettings>();
		check(Settings);
		return Settings;
	}

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	// ============== Axis Settings ==============
	UPROPERTY(config, EditAnywhere, Category = "BlendSpace|Axis")
	float DefaultMinSpeed = -500.f;

	UPROPERTY(config, EditAnywhere, Category = "BlendSpace|Axis")
	float DefaultMaxSpeed = 500.f;

	UPROPERTY(config, EditAnywhere, Category = "BlendSpace|Axis")
	FString XAxisName = TEXT("Right");

	UPROPERTY(config, EditAnywhere, Category = "BlendSpace|Axis")
	FString YAxisName = TEXT("Forward");

	// ============== Preference Settings ==============
	UPROPERTY(config, EditAnywhere, Category = "BlendSpace|Preference")
	bool bPreferRootMotionAnimations = true;

	UPROPERTY(config, EditAnywhere, Category = "BlendSpace|Output")
	FString OutputAssetSuffix = TEXT("_Locomotion");

	// ============== Locomotion Analysis Settings ==============
	/** Minimum velocity threshold for root motion analysis (cm/s). Animations below this are considered stationary. */
	UPROPERTY(config, EditAnywhere, Category = "BlendSpace|Analysis", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float MinVelocityThreshold = 1.0f;

	/** Left foot bone name patterns for locomotion analysis (case-insensitive contains match) */
	UPROPERTY(config, EditAnywhere, Category = "BlendSpace|Analysis", meta = (TitleProperty = ""))
	TArray<FString> LeftFootBonePatterns;

	/** Right foot bone name patterns for locomotion analysis (case-insensitive contains match) */
	UPROPERTY(config, EditAnywhere, Category = "BlendSpace|Analysis", meta = (TitleProperty = ""))
	TArray<FString> RightFootBonePatterns;

	// ============== Speed Tiers ==============
	UPROPERTY(config, EditAnywhere, Category = "BlendSpace|Speed", meta = (TitleProperty = "TierName"))
	TArray<FLocomotionSpeedTier> SpeedTiers;

	// ============== Pattern Entries ==============
	UPROPERTY(config, EditAnywhere, Category = "BlendSpace|Patterns", meta = (TitleProperty = "NamePattern"))
	TArray<FLocomotionPatternEntry> PatternEntries;

	/** Suffixes to strip before pattern matching (e.g., _RM, _RootMotion, _IP, _InPlace) */
	UPROPERTY(config, EditAnywhere, Category = "BlendSpace|Patterns", meta = (TitleProperty = ""))
	TArray<FString> IgnorableSuffixes;

	// ============== Functions ==============
	UFUNCTION(CallInEditor, Category = "BlendSpace|Patterns")
	void ResetToDefaultPatterns();

	UFUNCTION(CallInEditor, Category = "BlendSpace|Patterns")
	void ResetToDefaultIgnorableSuffixes();

	UFUNCTION(CallInEditor, Category = "BlendSpace|Analysis")
	void ResetToDefaultFootPatterns();

	bool TryMatchPattern(const FString& AnimName, ELocomotionRole& OutRole, FVector2D& OutPosition, int32& OutPriority) const;
	FVector2D GetPositionForRole(ELocomotionRole Role) const;
	float GetSpeedForTier(const FString& TierName) const;

	/** Strip ignorable suffixes from animation name for pattern matching */
	FString StripIgnorableSuffixes(const FString& AnimName) const;

	/** Find left foot bone from skeleton using LeftFootBonePatterns */
	FName FindLeftFootBone(const class USkeleton* Skeleton) const;

	/** Find right foot bone from skeleton using RightFootBonePatterns */
	FName FindRightFootBone(const class USkeleton* Skeleton) const;

	static FString GetRoleDisplayName(ELocomotionRole Role);

private:
	void InitializeDefaultPatterns();
	void InitializeDefaultSpeedTiers();
	void InitializeDefaultFootPatterns();
	void InitializeDefaultIgnorableSuffixes();
};
