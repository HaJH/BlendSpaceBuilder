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
	if (IgnorableSuffixes.Num() == 0)
	{
		InitializeDefaultIgnorableSuffixes();
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
	// ForwardLeft: frontL45, FrontL45, frontL, FrontL, FrontLeft, _FL, F_L_45, _F_L_
	PatternEntries.Add({TEXT("walk.*(frontL|FrontL|FrontLeft|_FL$|_FL_|F_L_|_F_L_$)"), true, ELocomotionRole::WalkForwardLeft, FVector2D::ZeroVector, 95});
	// ForwardRight: frontR45, FrontR45, frontR, FrontR, FrontRight, _FR, F_R_45, _F_R_
	PatternEntries.Add({TEXT("walk.*(frontR|FrontR|FrontRight|_FR$|_FR_|F_R_|_F_R_$)"), true, ELocomotionRole::WalkForwardRight, FVector2D::ZeroVector, 95});
	// BackwardLeft: backL45, BackL45, backL, BackL, BackLeft, _BL, B_L_45, _B_L_
	PatternEntries.Add({TEXT("walk.*(backL|BackL|BackLeft|_BL$|_BL_|B_L_|_B_L_$)"), true, ELocomotionRole::WalkBackwardLeft, FVector2D::ZeroVector, 95});
	// BackwardRight: backR45, BackR45, backR, BackR, BackRight, _BR, B_R_45, _B_R_
	PatternEntries.Add({TEXT("walk.*(backR|BackR|BackRight|_BR$|_BR_|B_R_|_B_R_$)"), true, ELocomotionRole::WalkBackwardRight, FVector2D::ZeroVector, 95});

	// ============== Walk Cardinal (Priority 90-92) ==============
	// Left 90 - _L_90 becomes _L_ after suffix strip
	PatternEntries.Add({TEXT("walk.*(_L_90|_F_L_90|_L_$)"), true, ELocomotionRole::WalkLeft, FVector2D::ZeroVector, 92});
	// Right 90
	PatternEntries.Add({TEXT("walk.*(_R_90|_F_R_90|_R_$)"), true, ELocomotionRole::WalkRight, FVector2D::ZeroVector, 92});
	// Backward 180
	PatternEntries.Add({TEXT("walk.*(_B_180|_B_$)"), true, ELocomotionRole::WalkBackward, FVector2D::ZeroVector, 92});
	// Forward 0
	PatternEntries.Add({TEXT("walk.*(_F_0|_F_$)"), true, ELocomotionRole::WalkForward, FVector2D::ZeroVector, 91});
	// Standard cardinal patterns
	PatternEntries.Add({TEXT("walk.*(forward|fwd|_F$)"), true, ELocomotionRole::WalkForward, FVector2D::ZeroVector, 90});
	PatternEntries.Add({TEXT("walk.*(backward|backwards|_B$)"), true, ELocomotionRole::WalkBackward, FVector2D::ZeroVector, 90});
	PatternEntries.Add({TEXT("walk.*(left|_L$)"), true, ELocomotionRole::WalkLeft, FVector2D::ZeroVector, 90});
	PatternEntries.Add({TEXT("walk.*(right|_R$)"), true, ELocomotionRole::WalkRight, FVector2D::ZeroVector, 90});
	// front/back alone (lower priority to avoid matching frontL, backR, etc.)
	PatternEntries.Add({TEXT("walk.*front$"), true, ELocomotionRole::WalkForward, FVector2D::ZeroVector, 85});
	PatternEntries.Add({TEXT("walk.*back$"), true, ELocomotionRole::WalkBackward, FVector2D::ZeroVector, 85});

	// ============== Run Diagonal (Priority 95) ==============
	// ForwardLeft: frontL45, FrontL45, frontL, FrontL, FrontLeft, _FL, F_L_45, _F_L_
	PatternEntries.Add({TEXT("run.*(frontL|FrontL|FrontLeft|_FL$|_FL_|F_L_|_F_L_$)"), true, ELocomotionRole::RunForwardLeft, FVector2D::ZeroVector, 95});
	// ForwardRight: frontR45, FrontR45, frontR, FrontR, FrontRight, _FR, F_R_45, _F_R_
	PatternEntries.Add({TEXT("run.*(frontR|FrontR|FrontRight|_FR$|_FR_|F_R_|_F_R_$)"), true, ELocomotionRole::RunForwardRight, FVector2D::ZeroVector, 95});
	// BackwardLeft: backL45, BackL45, backL, BackL, BackLeft, _BL, B_L_45, _B_L_
	PatternEntries.Add({TEXT("run.*(backL|BackL|BackLeft|_BL$|_BL_|B_L_|_B_L_$)"), true, ELocomotionRole::RunBackwardLeft, FVector2D::ZeroVector, 95});
	// BackwardRight: backR45, BackR45, backR, BackR, BackRight, _BR, B_R_45, _B_R_
	PatternEntries.Add({TEXT("run.*(backR|BackR|BackRight|_BR$|_BR_|B_R_|_B_R_$)"), true, ELocomotionRole::RunBackwardRight, FVector2D::ZeroVector, 95});

	// ============== Run Cardinal (Priority 90-92) ==============
	// Left 90 - _L_90 becomes _L_ after suffix strip
	PatternEntries.Add({TEXT("run.*(_L_90|_F_L_90|_L_$)"), true, ELocomotionRole::RunLeft, FVector2D::ZeroVector, 92});
	// Right 90
	PatternEntries.Add({TEXT("run.*(_R_90|_F_R_90|_R_$)"), true, ELocomotionRole::RunRight, FVector2D::ZeroVector, 92});
	// Backward 180
	PatternEntries.Add({TEXT("run.*(_B_180|_B_$)"), true, ELocomotionRole::RunBackward, FVector2D::ZeroVector, 92});
	// Forward 0
	PatternEntries.Add({TEXT("run.*(_F_0|_F_$)"), true, ELocomotionRole::RunForward, FVector2D::ZeroVector, 91});
	// Standard cardinal patterns
	PatternEntries.Add({TEXT("run.*(forward|fwd|_F$)"), true, ELocomotionRole::RunForward, FVector2D::ZeroVector, 90});
	PatternEntries.Add({TEXT("run.*(backward|backwards|_B$)"), true, ELocomotionRole::RunBackward, FVector2D::ZeroVector, 90});
	PatternEntries.Add({TEXT("run.*(left|_L$)"), true, ELocomotionRole::RunLeft, FVector2D::ZeroVector, 90});
	PatternEntries.Add({TEXT("run.*(right|_R$)"), true, ELocomotionRole::RunRight, FVector2D::ZeroVector, 90});
	// front/back alone (lower priority to avoid matching frontL, backR, etc.)
	PatternEntries.Add({TEXT("run.*front$"), true, ELocomotionRole::RunForward, FVector2D::ZeroVector, 85});
	PatternEntries.Add({TEXT("run.*back$"), true, ELocomotionRole::RunBackward, FVector2D::ZeroVector, 85});

	// ============== Sprint (Priority 85-90) ==============
	PatternEntries.Add({TEXT("sprint.*(forward|_F$)"), true, ELocomotionRole::SprintForward, FVector2D::ZeroVector, 90});
	PatternEntries.Add({TEXT("sprint.*(front$|$)"), true, ELocomotionRole::SprintForward, FVector2D::ZeroVector, 85});

	// ============== Strafe Independent (Priority 88) ==============
	PatternEntries.Add({TEXT("StrafeL"), true, ELocomotionRole::WalkLeft, FVector2D::ZeroVector, 88});
	PatternEntries.Add({TEXT("StrafeR"), true, ELocomotionRole::WalkRight, FVector2D::ZeroVector, 88});

	// ============== No Direction = Forward (Priority 50) ==============
	PatternEntries.Add({TEXT("_walk$"), true, ELocomotionRole::WalkForward, FVector2D::ZeroVector, 50});
	PatternEntries.Add({TEXT("_run$"), true, ELocomotionRole::RunForward, FVector2D::ZeroVector, 50});

	// ============== Simple Anim_ Style (Priority 40) ==============
	PatternEntries.Add({TEXT("Anim.*walk$"), true, ELocomotionRole::WalkForward, FVector2D::ZeroVector, 40});
	PatternEntries.Add({TEXT("Anim.*run$"), true, ELocomotionRole::RunForward, FVector2D::ZeroVector, 40});

	// ============== Walking/Running variants (Priority 50) ==============
	// Walking variants: Walking, walking
	PatternEntries.Add({TEXT("walking"), true, ELocomotionRole::WalkForward, FVector2D::ZeroVector, 50});
	// Running variants: Running, running
	PatternEntries.Add({TEXT("running"), true, ELocomotionRole::RunForward, FVector2D::ZeroVector, 50});

	// ============== Standalone Walk/Run (Priority 30) ==============
	// Matches: Walk, Run, Walk_1, Run_1 (after suffix strip)
	PatternEntries.Add({TEXT("^walk$"), true, ELocomotionRole::WalkForward, FVector2D::ZeroVector, 30});
	PatternEntries.Add({TEXT("^run$"), true, ELocomotionRole::RunForward, FVector2D::ZeroVector, 30});

	// ============== Ending with Walk/Run (Priority 25) ==============
	// Matches: AS_walk, AS_run, Char_walk, Char_run (after suffix strip)
	PatternEntries.Add({TEXT("walk$"), true, ELocomotionRole::WalkForward, FVector2D::ZeroVector, 25});
	PatternEntries.Add({TEXT("run$"), true, ELocomotionRole::RunForward, FVector2D::ZeroVector, 25});
}

bool UBlendSpaceBuilderSettings::TryMatchPattern(const FString& AnimName, ELocomotionRole& OutRole, FVector2D& OutPosition, int32& OutPriority) const
{
	// Strip ignorable suffixes before pattern matching
	const FString NameForMatching = StripIgnorableSuffixes(AnimName);

	TArray<FLocomotionPatternEntry> SortedPatterns = PatternEntries;
	SortedPatterns.Sort([](const FLocomotionPatternEntry& A, const FLocomotionPatternEntry& B)
	{
		return A.Priority > B.Priority;
	});

	for (const FLocomotionPatternEntry& Entry : SortedPatterns)
	{
		FString Pattern = Entry.NamePattern;
		FRegexPattern RegexPattern(Pattern, Entry.bCaseInsensitive ? ERegexPatternFlags::CaseInsensitive : ERegexPatternFlags::None);
		FRegexMatcher Matcher(RegexPattern, NameForMatching);

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

		// Skip IK bones - they don't have animation data
		if (BoneName.Contains(TEXT("ik_"), ESearchCase::IgnoreCase) ||
			BoneName.Contains(TEXT("_ik"), ESearchCase::IgnoreCase) ||
			BoneName.StartsWith(TEXT("IK"), ESearchCase::CaseSensitive))
		{
			continue;
		}

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

		// Skip IK bones - they don't have animation data
		if (BoneName.Contains(TEXT("ik_"), ESearchCase::IgnoreCase) ||
			BoneName.Contains(TEXT("_ik"), ESearchCase::IgnoreCase) ||
			BoneName.StartsWith(TEXT("IK"), ESearchCase::CaseSensitive))
		{
			continue;
		}

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

void UBlendSpaceBuilderSettings::InitializeDefaultIgnorableSuffixes()
{
	IgnorableSuffixes.Empty();
	// Root Motion related
	IgnorableSuffixes.Add(TEXT("_RootMotion"));
	IgnorableSuffixes.Add(TEXT("_root_motion"));
	IgnorableSuffixes.Add(TEXT("_RM"));
	// In Place related
	IgnorableSuffixes.Add(TEXT("_InPlace"));
	IgnorableSuffixes.Add(TEXT("_inplace"));
	IgnorableSuffixes.Add(TEXT("_in_place"));
	IgnorableSuffixes.Add(TEXT("_IP"));
	// Other ignorable suffixes
	IgnorableSuffixes.Add(TEXT("_NEW"));
}

void UBlendSpaceBuilderSettings::ResetToDefaultIgnorableSuffixes()
{
	InitializeDefaultIgnorableSuffixes();
	SaveConfig();
}

FString UBlendSpaceBuilderSettings::StripIgnorableSuffixes(const FString& AnimName) const
{
	FString Result = AnimName;

	// 1. Strip numeric suffixes first (_01, _02, _1, _2, 01, 02, etc.)
	//    Regex: (_?\d+)$ - optional underscore + digits at the end
	const FRegexPattern NumberPattern(TEXT("_?\\d+$"));
	FRegexMatcher NumberMatcher(NumberPattern, Result);
	if (NumberMatcher.FindNext())
	{
		const int32 MatchBegin = NumberMatcher.GetMatchBeginning();
		Result = Result.Left(MatchBegin);
	}

	// 2. Sort suffixes by length (longer first, so _RootMotion is checked before _RM)
	TArray<FString> SortedSuffixes = IgnorableSuffixes;
	SortedSuffixes.Sort([](const FString& A, const FString& B)
	{
		return A.Len() > B.Len();
	});

	// 3. Strip suffixes (handle nested suffixes like _RM_Montage)
	bool bFound = true;
	while (bFound)
	{
		bFound = false;
		for (const FString& Suffix : SortedSuffixes)
		{
			if (Result.EndsWith(Suffix, ESearchCase::IgnoreCase))
			{
				Result = Result.LeftChop(Suffix.Len());
				bFound = true;
				break;
			}
		}
	}

	return Result;
}
