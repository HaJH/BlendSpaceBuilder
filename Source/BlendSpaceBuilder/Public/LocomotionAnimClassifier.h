#pragma once

#include "CoreMinimal.h"
#include "BlendSpaceBuilderSettings.h"

class UAnimSequence;
class USkeleton;

struct FClassifiedAnimation
{
	TWeakObjectPtr<UAnimSequence> Animation;
	ELocomotionRole Role = ELocomotionRole::Idle;
	FVector2D BlendSpacePosition = FVector2D::ZeroVector;
	bool bHasRootMotion = false;
	float AnalyzedSpeed = 0.f;
	int32 MatchPriority = 0;

	FString GetDisplayName() const;
};

struct FLocomotionRoleCandidates
{
	ELocomotionRole Role = ELocomotionRole::Idle;
	TArray<FClassifiedAnimation> Candidates;

	FClassifiedAnimation* GetRecommended(bool bPreferRootMotion = true);
};

class BLENDSPACEBUILDER_API FLocomotionAnimClassifier
{
public:
	FLocomotionAnimClassifier();

	void FindAnimationsForSkeleton(const USkeleton* Skeleton);
	void ClassifyAnimations();

	const TMap<ELocomotionRole, FLocomotionRoleCandidates>& GetClassifiedResults() const { return ClassifiedResults; }
	const TArray<TWeakObjectPtr<UAnimSequence>>& GetUnclassifiedAnimations() const { return UnclassifiedAnimations; }

	int32 GetTotalAnimationCount() const { return AllAnimations.Num(); }
	int32 GetClassifiedCount() const;

private:
	void QueryAnimationsFromAssetRegistry(const USkeleton* Skeleton);
	bool ClassifySingleAnimation(UAnimSequence* Anim, FClassifiedAnimation& OutClassified);
	bool HasRootMotion(const UAnimSequence* Anim) const;

	TArray<TWeakObjectPtr<UAnimSequence>> AllAnimations;
	TMap<ELocomotionRole, FLocomotionRoleCandidates> ClassifiedResults;
	TArray<TWeakObjectPtr<UAnimSequence>> UnclassifiedAnimations;
};
