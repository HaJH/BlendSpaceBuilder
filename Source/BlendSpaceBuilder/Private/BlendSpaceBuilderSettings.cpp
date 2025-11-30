#include "BlendSpaceBuilderSettings.h"
#include "Internationalization/Regex.h"
#include "Animation/Skeleton.h"

UBlendSpaceBuilderSettings::UBlendSpaceBuilderSettings()
{
	// Initialize default values if arrays are empty
	if (SpeedTiers.Num() == 0)
	{
		InitializeDefaultSpeedTiers();
	}
	if (PatternEntries.Num() == 0)
	{
		InitializeDefaultPatterns();
	}
	if (LeftFootBonePatterns.Num() == 0)
	{
		InitializeDefaultFootPatterns();
	}
}

void UBlendSpaceBuilderSettings::ResetToDefaultPatterns()
{
	InitializeDefaultSpeedTiers();
	InitializeDefaultPatterns();
	SaveConfig();
}

void UBlendSpaceBuilderSettings::InitializeDefaultSpeedTiers()
{
	SpeedTiers.Empty();
	SpeedTiers.Add({TEXT("Walk"), 200.f});
	SpeedTiers.Add({TEXT("Run"), 400.f});
	SpeedTiers.Add({TEXT("Sprint"), 600.f});
}

void UBlendSpaceBuilderSettings::InitializeDefaultPatterns()
{
	PatternEntries.Empty();

	// ============== Idle (Priority 100) ==============
	PatternEntries.Add({TEXT("idle"), true, ELocomotionRole::Idle, FVector2D::ZeroVector, 100});

	// ============== Walk Diagonal (Priority 95) ==============
	// FL patterns - frontL45, FrontL45, _FL, F_L_45
	PatternEntries.Add({TEXT("(walk|move.*walk|strafe.*walk).*(frontL45|FrontL45|_FL$|_FL_|F_L_45)"), true, ELocomotionRole::WalkForwardLeft, FVector2D::ZeroVector, 95});
	// FR patterns
	PatternEntries.Add({TEXT("(walk|move.*walk|strafe.*walk).*(frontR45|FrontR45|_FR$|_FR_|F_R_45)"), true, ELocomotionRole::WalkForwardRight, FVector2D::ZeroVector, 95});
	// BL patterns
	PatternEntries.Add({TEXT("(walk|move.*walk|strafe.*walk).*(backL45|BackL45|_BL$|_BL_|B_L_45)"), true, ELocomotionRole::WalkBackwardLeft, FVector2D::ZeroVector, 95});
	// BR patterns
	PatternEntries.Add({TEXT("(walk|move.*walk|strafe.*walk).*(backR45|BackR45|_BR$|_BR_|B_R_45)"), true, ELocomotionRole::WalkBackwardRight, FVector2D::ZeroVector, 95});

	// ============== Walk Cardinal (Priority 90) ==============
	PatternEntries.Add({TEXT("(walk|move.*walk|strafe.*walk).*(front|forward|fwd|_F$|_F_|_0$)"), true, ELocomotionRole::WalkForward, FVector2D::ZeroVector, 90});
	PatternEntries.Add({TEXT("(walk|move.*walk|strafe.*walk).*(back|backward|backwards|_B$|_B_|_180$)"), true, ELocomotionRole::WalkBackward, FVector2D::ZeroVector, 90});
	PatternEntries.Add({TEXT("(walk|move.*walk|strafe.*walk).*(left|_L$|_L_|L_90)"), true, ELocomotionRole::WalkLeft, FVector2D::ZeroVector, 90});
	PatternEntries.Add({TEXT("(walk|move.*walk|strafe.*walk).*(right|_R$|_R_|R_90)"), true, ELocomotionRole::WalkRight, FVector2D::ZeroVector, 90});

	// ============== Run Diagonal (Priority 95) ==============
	PatternEntries.Add({TEXT("(run|move.*run|strafe.*run).*(frontL45|FrontL45|_FL$|_FL_|F_L_45)"), true, ELocomotionRole::RunForwardLeft, FVector2D::ZeroVector, 95});
	PatternEntries.Add({TEXT("(run|move.*run|strafe.*run).*(frontR45|FrontR45|_FR$|_FR_|F_R_45)"), true, ELocomotionRole::RunForwardRight, FVector2D::ZeroVector, 95});
	PatternEntries.Add({TEXT("(run|move.*run|strafe.*run).*(backL45|BackL45|_BL$|_BL_|B_L_45)"), true, ELocomotionRole::RunBackwardLeft, FVector2D::ZeroVector, 95});
	PatternEntries.Add({TEXT("(run|move.*run|strafe.*run).*(backR45|BackR45|_BR$|_BR_|B_R_45)"), true, ELocomotionRole::RunBackwardRight, FVector2D::ZeroVector, 95});

	// ============== Run Cardinal (Priority 90) ==============
	PatternEntries.Add({TEXT("(run|move.*run|strafe.*run).*(front|forward|fwd|_F$|_F_|_0$)"), true, ELocomotionRole::RunForward, FVector2D::ZeroVector, 90});
	PatternEntries.Add({TEXT("(run|move.*run|strafe.*run).*(back|backward|backwards|_B$|_B_|_180$)"), true, ELocomotionRole::RunBackward, FVector2D::ZeroVector, 90});
	PatternEntries.Add({TEXT("(run|move.*run|strafe.*run).*(left|_L$|_L_|L_90)"), true, ELocomotionRole::RunLeft, FVector2D::ZeroVector, 90});
	PatternEntries.Add({TEXT("(run|move.*run|strafe.*run).*(right|_R$|_R_|R_90)"), true, ELocomotionRole::RunRight, FVector2D::ZeroVector, 90});

	// ============== Sprint (Priority 90) ==============
	PatternEntries.Add({TEXT("sprint.*(front|forward|_F$)"), true, ELocomotionRole::SprintForward, FVector2D::ZeroVector, 90});
	PatternEntries.Add({TEXT("sprint$"), true, ELocomotionRole::SprintForward, FVector2D::ZeroVector, 85});

	// ============== Strafe Independent (Priority 88) ==============
	PatternEntries.Add({TEXT("StrafeL"), true, ELocomotionRole::WalkLeft, FVector2D::ZeroVector, 88});
	PatternEntries.Add({TEXT("StrafeR"), true, ELocomotionRole::WalkRight, FVector2D::ZeroVector, 88});

	// ============== No Direction = Forward (Priority 50) ==============
	PatternEntries.Add({TEXT("_walk$"), true, ELocomotionRole::WalkForward, FVector2D::ZeroVector, 50});
	PatternEntries.Add({TEXT("_run$"), true, ELocomotionRole::RunForward, FVector2D::ZeroVector, 50});
	PatternEntries.Add({TEXT("RunFront$"), true, ELocomotionRole::RunForward, FVector2D::ZeroVector, 50});

	// ============== Simple Anim_ Style (Priority 40) ==============
	PatternEntries.Add({TEXT("Anim.*walk$"), true, ELocomotionRole::WalkForward, FVector2D::ZeroVector, 40});
	PatternEntries.Add({TEXT("Anim.*run$"), true, ELocomotionRole::RunForward, FVector2D::ZeroVector, 40});
}

bool UBlendSpaceBuilderSettings::TryMatchPattern(const FString& AnimName, ELocomotionRole& OutRole, FVector2D& OutPosition, int32& OutPriority) const
{
	TArray<FLocomotionPatternEntry> SortedPatterns = PatternEntries;
	SortedPatterns.Sort([](const FLocomotionPatternEntry& A, const FLocomotionPatternEntry& B)
	{
		return A.Priority > B.Priority;
	});

	for (const FLocomotionPatternEntry& Entry : SortedPatterns)
	{
		FString Pattern = Entry.NamePattern;
		FRegexPattern RegexPattern(Pattern, Entry.bCaseInsensitive ? ERegexPatternFlags::CaseInsensitive : ERegexPatternFlags::None);
		FRegexMatcher Matcher(RegexPattern, AnimName);

		if (Matcher.FindNext())
		{
			OutRole = Entry.Role;
			OutPriority = Entry.Priority;
			OutPosition = Entry.Role == ELocomotionRole::Custom ? Entry.CustomPosition : GetPositionForRole(Entry.Role);
			return true;
		}
	}

	return false;
}

FVector2D UBlendSpaceBuilderSettings::GetPositionForRole(ELocomotionRole Role) const
{
	float WalkSpeed = GetSpeedForTier(TEXT("Walk"));
	float RunSpeed = GetSpeedForTier(TEXT("Run"));
	float SprintSpeed = GetSpeedForTier(TEXT("Sprint"));

	// X = Right velocity, Y = Forward velocity
	switch (Role)
	{
	case ELocomotionRole::Idle:
		return FVector2D(0, 0);

	// Walk
	case ELocomotionRole::WalkForward:
		return FVector2D(0, WalkSpeed);
	case ELocomotionRole::WalkBackward:
		return FVector2D(0, -WalkSpeed);
	case ELocomotionRole::WalkLeft:
		return FVector2D(-WalkSpeed, 0);
	case ELocomotionRole::WalkRight:
		return FVector2D(WalkSpeed, 0);
	case ELocomotionRole::WalkForwardLeft:
		return FVector2D(-WalkSpeed, WalkSpeed);
	case ELocomotionRole::WalkForwardRight:
		return FVector2D(WalkSpeed, WalkSpeed);
	case ELocomotionRole::WalkBackwardLeft:
		return FVector2D(-WalkSpeed, -WalkSpeed);
	case ELocomotionRole::WalkBackwardRight:
		return FVector2D(WalkSpeed, -WalkSpeed);

	// Run
	case ELocomotionRole::RunForward:
		return FVector2D(0, RunSpeed);
	case ELocomotionRole::RunBackward:
		return FVector2D(0, -RunSpeed);
	case ELocomotionRole::RunLeft:
		return FVector2D(-RunSpeed, 0);
	case ELocomotionRole::RunRight:
		return FVector2D(RunSpeed, 0);
	case ELocomotionRole::RunForwardLeft:
		return FVector2D(-RunSpeed, RunSpeed);
	case ELocomotionRole::RunForwardRight:
		return FVector2D(RunSpeed, RunSpeed);
	case ELocomotionRole::RunBackwardLeft:
		return FVector2D(-RunSpeed, -RunSpeed);
	case ELocomotionRole::RunBackwardRight:
		return FVector2D(RunSpeed, -RunSpeed);

	// Sprint
	case ELocomotionRole::SprintForward:
		return FVector2D(0, SprintSpeed);

	default:
		return FVector2D::ZeroVector;
	}
}

float UBlendSpaceBuilderSettings::GetSpeedForTier(const FString& TierName) const
{
	for (const FLocomotionSpeedTier& Tier : SpeedTiers)
	{
		if (Tier.TierName.Equals(TierName, ESearchCase::IgnoreCase))
		{
			return Tier.Speed;
		}
	}
	return 300.f;
}

FString UBlendSpaceBuilderSettings::GetRoleDisplayName(ELocomotionRole Role)
{
	switch (Role)
	{
	case ELocomotionRole::Idle: return TEXT("Idle");
	case ELocomotionRole::WalkForward: return TEXT("Walk Forward");
	case ELocomotionRole::WalkBackward: return TEXT("Walk Backward");
	case ELocomotionRole::WalkLeft: return TEXT("Walk Left");
	case ELocomotionRole::WalkRight: return TEXT("Walk Right");
	case ELocomotionRole::WalkForwardLeft: return TEXT("Walk Forward-Left");
	case ELocomotionRole::WalkForwardRight: return TEXT("Walk Forward-Right");
	case ELocomotionRole::WalkBackwardLeft: return TEXT("Walk Backward-Left");
	case ELocomotionRole::WalkBackwardRight: return TEXT("Walk Backward-Right");
	case ELocomotionRole::RunForward: return TEXT("Run Forward");
	case ELocomotionRole::RunBackward: return TEXT("Run Backward");
	case ELocomotionRole::RunLeft: return TEXT("Run Left");
	case ELocomotionRole::RunRight: return TEXT("Run Right");
	case ELocomotionRole::RunForwardLeft: return TEXT("Run Forward-Left");
	case ELocomotionRole::RunForwardRight: return TEXT("Run Forward-Right");
	case ELocomotionRole::RunBackwardLeft: return TEXT("Run Backward-Left");
	case ELocomotionRole::RunBackwardRight: return TEXT("Run Backward-Right");
	case ELocomotionRole::SprintForward: return TEXT("Sprint Forward");
	case ELocomotionRole::Custom: return TEXT("Custom");
	default: return TEXT("Unknown");
	}
}

void UBlendSpaceBuilderSettings::InitializeDefaultFootPatterns()
{
	LeftFootBonePatterns.Empty();
	LeftFootBonePatterns.Add(TEXT("foot_l"));
	LeftFootBonePatterns.Add(TEXT("Foot_L"));
	LeftFootBonePatterns.Add(TEXT("LeftFoot"));
	LeftFootBonePatterns.Add(TEXT("Left_Foot"));
	LeftFootBonePatterns.Add(TEXT("l_foot"));
	LeftFootBonePatterns.Add(TEXT("L_Foot"));

	RightFootBonePatterns.Empty();
	RightFootBonePatterns.Add(TEXT("foot_r"));
	RightFootBonePatterns.Add(TEXT("Foot_R"));
	RightFootBonePatterns.Add(TEXT("RightFoot"));
	RightFootBonePatterns.Add(TEXT("Right_Foot"));
	RightFootBonePatterns.Add(TEXT("r_foot"));
	RightFootBonePatterns.Add(TEXT("R_Foot"));
}

void UBlendSpaceBuilderSettings::ResetToDefaultFootPatterns()
{
	InitializeDefaultFootPatterns();
	SaveConfig();
}

FName UBlendSpaceBuilderSettings::FindLeftFootBone(const USkeleton* Skeleton) const
{
	if (!Skeleton)
	{
		return NAME_None;
	}

	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();

	for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); ++BoneIndex)
	{
		FString BoneName = RefSkeleton.GetBoneName(BoneIndex).ToString();

		for (const FString& Pattern : LeftFootBonePatterns)
		{
			if (BoneName.Contains(Pattern, ESearchCase::IgnoreCase))
			{
				return RefSkeleton.GetBoneName(BoneIndex);
			}
		}
	}

	return NAME_None;
}

FName UBlendSpaceBuilderSettings::FindRightFootBone(const USkeleton* Skeleton) const
{
	if (!Skeleton)
	{
		return NAME_None;
	}

	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();

	for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); ++BoneIndex)
	{
		FString BoneName = RefSkeleton.GetBoneName(BoneIndex).ToString();

		for (const FString& Pattern : RightFootBonePatterns)
		{
			if (BoneName.Contains(Pattern, ESearchCase::IgnoreCase))
			{
				return RefSkeleton.GetBoneName(BoneIndex);
			}
		}
	}

	return NAME_None;
}
