#include "LocomotionPatternDataAsset.h"
#include "Internationalization/Regex.h"

ULocomotionPatternDataAsset::ULocomotionPatternDataAsset()
{
	InitializeDefaultPatterns();
}

void ULocomotionPatternDataAsset::InitializeDefaultPatterns()
{
	SpeedTiers.Empty();
	SpeedTiers.Add({TEXT("Walk"), 200.f});
	SpeedTiers.Add({TEXT("Run"), 400.f});
	SpeedTiers.Add({TEXT("Sprint"), 600.f});

	PatternEntries.Empty();

	// Idle - highest priority
	PatternEntries.Add({TEXT("idle"), true, ELocomotionRole::Idle, FVector2D::ZeroVector, 100});

	// Walk patterns - diagonal first (higher priority)
	PatternEntries.Add({TEXT("walk.*(fl|fwd.*l|forward.*left)"), true, ELocomotionRole::WalkForwardLeft, FVector2D::ZeroVector, 95});
	PatternEntries.Add({TEXT("walk.*(fr|fwd.*r|forward.*right)"), true, ELocomotionRole::WalkForwardRight, FVector2D::ZeroVector, 95});
	PatternEntries.Add({TEXT("walk.*(bl|bwd.*l|back.*left)"), true, ELocomotionRole::WalkBackwardLeft, FVector2D::ZeroVector, 95});
	PatternEntries.Add({TEXT("walk.*(br|bwd.*r|back.*right)"), true, ELocomotionRole::WalkBackwardRight, FVector2D::ZeroVector, 95});

	// Walk cardinal directions
	PatternEntries.Add({TEXT("walk.*(fwd|forward|front|_f$|_f_)"), true, ELocomotionRole::WalkForward, FVector2D::ZeroVector, 90});
	PatternEntries.Add({TEXT("walk.*(bwd|backward|back|_b$|_b_)"), true, ELocomotionRole::WalkBackward, FVector2D::ZeroVector, 90});
	PatternEntries.Add({TEXT("walk.*(left|_l$|_l_)"), true, ELocomotionRole::WalkLeft, FVector2D::ZeroVector, 90});
	PatternEntries.Add({TEXT("walk.*(right|_r$|_r_)"), true, ELocomotionRole::WalkRight, FVector2D::ZeroVector, 90});

	// Reverse patterns (direction first)
	PatternEntries.Add({TEXT("(fwd|forward|front).*walk"), true, ELocomotionRole::WalkForward, FVector2D::ZeroVector, 89});
	PatternEntries.Add({TEXT("(bwd|backward|back).*walk"), true, ELocomotionRole::WalkBackward, FVector2D::ZeroVector, 89});
	PatternEntries.Add({TEXT("left.*walk"), true, ELocomotionRole::WalkLeft, FVector2D::ZeroVector, 89});
	PatternEntries.Add({TEXT("right.*walk"), true, ELocomotionRole::WalkRight, FVector2D::ZeroVector, 89});

	// Run patterns - diagonal
	PatternEntries.Add({TEXT("run.*(fl|fwd.*l|forward.*left)"), true, ELocomotionRole::RunForwardLeft, FVector2D::ZeroVector, 95});
	PatternEntries.Add({TEXT("run.*(fr|fwd.*r|forward.*right)"), true, ELocomotionRole::RunForwardRight, FVector2D::ZeroVector, 95});
	PatternEntries.Add({TEXT("run.*(bl|bwd.*l|back.*left)"), true, ELocomotionRole::RunBackwardLeft, FVector2D::ZeroVector, 95});
	PatternEntries.Add({TEXT("run.*(br|bwd.*r|back.*right)"), true, ELocomotionRole::RunBackwardRight, FVector2D::ZeroVector, 95});

	// Run cardinal directions
	PatternEntries.Add({TEXT("run.*(fwd|forward|front|_f$|_f_)"), true, ELocomotionRole::RunForward, FVector2D::ZeroVector, 90});
	PatternEntries.Add({TEXT("run.*(bwd|backward|back|_b$|_b_)"), true, ELocomotionRole::RunBackward, FVector2D::ZeroVector, 90});
	PatternEntries.Add({TEXT("run.*(left|_l$|_l_)"), true, ELocomotionRole::RunLeft, FVector2D::ZeroVector, 90});
	PatternEntries.Add({TEXT("run.*(right|_r$|_r_)"), true, ELocomotionRole::RunRight, FVector2D::ZeroVector, 90});

	// Reverse patterns (direction first)
	PatternEntries.Add({TEXT("(fwd|forward|front).*run"), true, ELocomotionRole::RunForward, FVector2D::ZeroVector, 89});
	PatternEntries.Add({TEXT("(bwd|backward|back).*run"), true, ELocomotionRole::RunBackward, FVector2D::ZeroVector, 89});
	PatternEntries.Add({TEXT("left.*run"), true, ELocomotionRole::RunLeft, FVector2D::ZeroVector, 89});
	PatternEntries.Add({TEXT("right.*run"), true, ELocomotionRole::RunRight, FVector2D::ZeroVector, 89});

	// Sprint
	PatternEntries.Add({TEXT("sprint.*(fwd|forward|front|_f$|_f_)"), true, ELocomotionRole::SprintForward, FVector2D::ZeroVector, 90});
	PatternEntries.Add({TEXT("sprint"), true, ELocomotionRole::SprintForward, FVector2D::ZeroVector, 85});

	// Generic walk/run (no direction - assume forward)
	PatternEntries.Add({TEXT("^[^_]*walk[^_]*$"), true, ELocomotionRole::WalkForward, FVector2D::ZeroVector, 50});
	PatternEntries.Add({TEXT("^[^_]*run[^_]*$"), true, ELocomotionRole::RunForward, FVector2D::ZeroVector, 50});
}

bool ULocomotionPatternDataAsset::TryMatchPattern(const FString& AnimName, ELocomotionRole& OutRole, FVector2D& OutPosition, int32& OutPriority) const
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

FVector2D ULocomotionPatternDataAsset::GetPositionForRole(ELocomotionRole Role) const
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

float ULocomotionPatternDataAsset::GetSpeedForTier(const FString& TierName) const
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

FString ULocomotionPatternDataAsset::GetRoleDisplayName(ELocomotionRole Role)
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
