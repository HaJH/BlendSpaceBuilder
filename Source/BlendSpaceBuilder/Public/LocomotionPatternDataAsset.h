#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LocomotionPatternDataAsset.generated.h"

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString NamePattern;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bCaseInsensitive = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ELocomotionRole Role = ELocomotionRole::Idle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "Role == ELocomotionRole::Custom"))
	FVector2D CustomPosition = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Priority = 0;
};

USTRUCT(BlueprintType)
struct FLocomotionSpeedTier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString TierName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Speed = 300.f;
};

UCLASS(BlueprintType)
class BLENDSPACEBUILDER_API ULocomotionPatternDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	ULocomotionPatternDataAsset();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Patterns")
	TArray<FLocomotionPatternEntry> PatternEntries;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed Tiers")
	TArray<FLocomotionSpeedTier> SpeedTiers;

	void InitializeDefaultPatterns();

	bool TryMatchPattern(const FString& AnimName, ELocomotionRole& OutRole, FVector2D& OutPosition, int32& OutPriority) const;

	FVector2D GetPositionForRole(ELocomotionRole Role) const;

	float GetSpeedForTier(const FString& TierName) const;

	static FString GetRoleDisplayName(ELocomotionRole Role);
};
