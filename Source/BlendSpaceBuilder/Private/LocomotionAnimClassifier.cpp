#include "LocomotionAnimClassifier.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "BlendSpaceBuilderSettings.h"

FString FClassifiedAnimation::GetDisplayName() const
{
	if (UAnimSequence* Anim = Animation.Get())
	{
		FString Name = Anim->GetName();
		if (bHasRootMotion)
		{
			Name += TEXT(" [RM]");
		}
		return Name;
	}
	return TEXT("Invalid");
}

FClassifiedAnimation* FLocomotionRoleCandidates::GetRecommended(bool bPreferRootMotion)
{
	if (Candidates.Num() == 0)
	{
		return nullptr;
	}

	if (bPreferRootMotion)
	{
		for (FClassifiedAnimation& Candidate : Candidates)
		{
			if (Candidate.bHasRootMotion)
			{
				return &Candidate;
			}
		}
	}

	// Return highest priority
	FClassifiedAnimation* Best = &Candidates[0];
	for (FClassifiedAnimation& Candidate : Candidates)
	{
		if (Candidate.MatchPriority > Best->MatchPriority)
		{
			Best = &Candidate;
		}
	}
	return Best;
}

FLocomotionAnimClassifier::FLocomotionAnimClassifier()
{
}

void FLocomotionAnimClassifier::FindAnimationsForSkeleton(const USkeleton* Skeleton)
{
	AllAnimations.Empty();
	ClassifiedResults.Empty();
	UnclassifiedAnimations.Empty();

	if (!Skeleton)
	{
		return;
	}

	QueryAnimationsFromAssetRegistry(Skeleton);
}

void FLocomotionAnimClassifier::QueryAnimationsFromAssetRegistry(const USkeleton* Skeleton)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(UAnimSequence::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	FSoftObjectPath SkeletonPath(Skeleton);
	FString SkeletonPathString = SkeletonPath.ToString();

	for (const FAssetData& AssetData : AssetDataList)
	{
		FAssetTagValueRef SkeletonTag = AssetData.TagsAndValues.FindTag(TEXT("Skeleton"));
		if (SkeletonTag.IsSet())
		{
			FString AnimSkeletonPath = SkeletonTag.AsString();

			if (AnimSkeletonPath == SkeletonPathString || AnimSkeletonPath.Contains(Skeleton->GetName()))
			{
				if (UAnimSequence* Anim = Cast<UAnimSequence>(AssetData.GetAsset()))
				{
					AllAnimations.Add(Anim);
				}
			}
		}
	}
}

void FLocomotionAnimClassifier::ClassifyAnimations()
{
	ClassifiedResults.Empty();
	UnclassifiedAnimations.Empty();

	for (const TWeakObjectPtr<UAnimSequence>& AnimPtr : AllAnimations)
	{
		UAnimSequence* Anim = AnimPtr.Get();
		if (!Anim)
		{
			continue;
		}

		FClassifiedAnimation Classified;
		if (ClassifySingleAnimation(Anim, Classified))
		{
			FLocomotionRoleCandidates& Candidates = ClassifiedResults.FindOrAdd(Classified.Role);
			Candidates.Role = Classified.Role;
			Candidates.Candidates.Add(Classified);
		}
		else
		{
			UnclassifiedAnimations.Add(Anim);
		}
	}

	// Sort candidates by root motion preference and priority
	bool bPreferRootMotion = UBlendSpaceBuilderSettings::Get()->bPreferRootMotionAnimations;
	for (auto& Pair : ClassifiedResults)
	{
		Pair.Value.Candidates.Sort([bPreferRootMotion](const FClassifiedAnimation& A, const FClassifiedAnimation& B)
		{
			if (bPreferRootMotion && A.bHasRootMotion != B.bHasRootMotion)
			{
				return A.bHasRootMotion;
			}
			return A.MatchPriority > B.MatchPriority;
		});
	}
}

bool FLocomotionAnimClassifier::ClassifySingleAnimation(UAnimSequence* Anim, FClassifiedAnimation& OutClassified)
{
	const UBlendSpaceBuilderSettings* Settings = UBlendSpaceBuilderSettings::Get();
	if (!Settings)
	{
		return false;
	}

	FString AnimName = Anim->GetName();
	ELocomotionRole MatchedRole;
	FVector2D Position;
	int32 Priority;

	if (Settings->TryMatchPattern(AnimName, MatchedRole, Position, Priority))
	{
		OutClassified.Animation = Anim;
		OutClassified.Role = MatchedRole;
		OutClassified.BlendSpacePosition = Position;
		OutClassified.bHasRootMotion = HasRootMotion(Anim);
		OutClassified.MatchPriority = Priority;
		return true;
	}

	return false;
}

bool FLocomotionAnimClassifier::HasRootMotion(const UAnimSequence* Anim) const
{
	if (!Anim)
	{
		return false;
	}
	return Anim->bEnableRootMotion;
}

int32 FLocomotionAnimClassifier::GetClassifiedCount() const
{
	int32 Count = 0;
	for (const auto& Pair : ClassifiedResults)
	{
		Count += Pair.Value.Candidates.Num();
	}
	return Count;
}
