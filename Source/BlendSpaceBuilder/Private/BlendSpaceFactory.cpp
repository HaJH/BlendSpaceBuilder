#include "BlendSpaceFactory.h"
#include "BlendSpaceBuilderSettings.h"
#include "BlendSpaceConfigAssetUserData.h"

#include "Animation/BlendSpace.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimTypes.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "UObject/UnrealType.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"

#define LOCTEXT_NAMESPACE "BlendSpaceFactory"

DEFINE_LOG_CATEGORY_STATIC(LogBlendSpaceBuilder, Log, All);

//=============================================================================
// Internal helper functions for analysis
//=============================================================================

namespace BlendSpaceAnalysisInternal
{
	// Get a "nice" step size for axis range (rounds to 1, 2, 2.5, 5 * 10^n pattern)
	float GetNiceStepSize(float RawStep)
	{
		if (RawStep <= 0.f)
		{
			return 1.f;
		}

		// Nice factors: 1, 2, 2.5, 5, 10 pattern
		static const float NiceFactors[] = {1.f, 2.f, 2.5f, 5.f, 10.f};

		// Find the magnitude (power of 10)
		float Magnitude = FMath::Pow(10.f, FMath::FloorToFloat(FMath::LogX(10.f, RawStep)));

		// Find the smallest nice value >= RawStep
		for (float Factor : NiceFactors)
		{
			float NiceValue = Magnitude * Factor;
			if (NiceValue >= RawStep - KINDA_SMALL_NUMBER)
			{
				return NiceValue;
			}
		}

		// Fallback: next magnitude
		return Magnitude * 10.f;
	}

	// Calculate component space transform by traversing parent chain
	FTransform GetComponentSpaceTransform(
		const UAnimSequence* Animation,
		const FReferenceSkeleton& RefSkeleton,
		int32 BoneIndex,
		double Time)
	{
		TArray<FTransform> BoneTransforms;

		// Collect all bone transforms from target to root
		int32 CurrentBone = BoneIndex;
		while (CurrentBone != INDEX_NONE)
		{
			FTransform BoneLocalTM;
			FAnimExtractContext ExtractContext(Time);
			Animation->GetBoneTransform(BoneLocalTM, FSkeletonPoseBoneIndex(CurrentBone), ExtractContext, false);
			BoneTransforms.Add(BoneLocalTM);
			CurrentBone = RefSkeleton.GetParentIndex(CurrentBone);
		}

		// Compose transforms from root to target
		FTransform ComponentSpaceTM = FTransform::Identity;
		for (int32 i = BoneTransforms.Num() - 1; i >= 0; --i)
		{
			ComponentSpaceTM = BoneTransforms[i] * ComponentSpaceTM;
		}

		return ComponentSpaceTM;
	}

	// Calculate root motion velocity from animation
	// Returns FVector(RightVelocity, ForwardVelocity, 0)
	FVector CalculateRootMotionVelocity(const UAnimSequence* Animation)
	{
		if (!Animation)
		{
			UE_LOG(LogBlendSpaceBuilder, Warning, TEXT("RootMotion: Animation is null"));
			return FVector::ZeroVector;
		}

		double PlayLength = Animation->GetPlayLength();
		if (PlayLength <= KINDA_SMALL_NUMBER)
		{
			UE_LOG(LogBlendSpaceBuilder, Warning, TEXT("RootMotion: '%s' has zero play length"), *Animation->GetName());
			return FVector::ZeroVector;
		}

		// Extract root motion from start to end of animation
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		FTransform RootMotion = Animation->ExtractRootMotionFromRange(0.0f, static_cast<float>(PlayLength));
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
		FVector Translation = RootMotion.GetTranslation();
		FVector Velocity = Translation / PlayLength;

		// Apply rate scale
		Velocity *= Animation->RateScale;

		// Check if velocity is below threshold (root motion enabled but no actual movement)
		const float MinVelocity = UBlendSpaceBuilderSettings::Get()->MinVelocityThreshold;
		const float Speed2D = FVector2D(Velocity.X, Velocity.Y).Size();
		if (Speed2D < MinVelocity)
		{
			UE_LOG(LogBlendSpaceBuilder, Warning,
				TEXT("RootMotion: '%s' velocity (%.2f) below threshold (%.2f)"),
				*Animation->GetName(), Speed2D, MinVelocity);
			return FVector::ZeroVector;
		}

		// ExtractRootMotionFromRange returns translation in character space
		// BlendSpace axes: X=RightVelocity, Y=ForwardVelocity (matches directly)
		UE_LOG(LogBlendSpaceBuilder, Verbose, TEXT("RootMotion: '%s' -> Velocity(%.1f, %.1f)"),
			*Animation->GetName(), Velocity.X, Velocity.Y);
		return FVector(Velocity.X, Velocity.Y, 0.f);
	}

	// Helper: Collect foot positions from animation
	bool CollectFootPositions(
		const UAnimSequence* Animation,
		FName FootBoneName,
		TArray<FVector>& OutPositions,
		double& OutDeltaTime)
	{
		if (!Animation || FootBoneName == NAME_None)
		{
			UE_LOG(LogBlendSpaceBuilder, Warning, TEXT("Locomotion: Invalid input (Anim=%s, Bone=%s)"),
				Animation ? *Animation->GetName() : TEXT("null"), *FootBoneName.ToString());
			return false;
		}

		const int32 NumKeys = Animation->GetNumberOfSampledKeys();
		if (NumKeys <= 1)
		{
			UE_LOG(LogBlendSpaceBuilder, Warning, TEXT("Locomotion: '%s' has insufficient keys (%d)"),
				*Animation->GetName(), NumKeys);
			return false;
		}

		USkeleton* Skeleton = Animation->GetSkeleton();
		if (!Skeleton)
		{
			UE_LOG(LogBlendSpaceBuilder, Warning, TEXT("Locomotion: '%s' has no skeleton"),
				*Animation->GetName());
			return false;
		}

		int32 BoneIndex = Skeleton->GetReferenceSkeleton().FindBoneIndex(FootBoneName);
		if (BoneIndex == INDEX_NONE)
		{
			UE_LOG(LogBlendSpaceBuilder, Warning, TEXT("Locomotion: '%s' foot bone '%s' not found in skeleton"),
				*Animation->GetName(), *FootBoneName.ToString());
			return false;
		}

		const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
		OutDeltaTime = Animation->GetPlayLength() / double(NumKeys);

		OutPositions.SetNum(NumKeys);
		for (int32 Key = 0; Key < NumKeys; ++Key)
		{
			FTransform ComponentSpaceTM = GetComponentSpaceTransform(
				Animation, RefSkeleton, BoneIndex, Key * OutDeltaTime);
			OutPositions[Key] = ComponentSpaceTM.GetTranslation();
		}

		return true;
	}

	// Calculate locomotion velocity using simple average (no weighting)
	FVector CalculateLocomotionVelocityFromFootSimple(const UAnimSequence* Animation, FName FootBoneName)
	{
		TArray<FVector> Positions;
		double DeltaTime;
		if (!CollectFootPositions(Animation, FootBoneName, Positions, DeltaTime))
		{
			return FVector::ZeroVector;
		}

		const int32 NumKeys = Positions.Num();

		// Calculate velocities using forward difference
		TArray<FVector> Velocities;
		Velocities.SetNum(NumKeys);
		for (int32 Key = 0; Key < NumKeys - 1; ++Key)
		{
			Velocities[Key] = (Positions[Key + 1] - Positions[Key]) / DeltaTime;
		}
		Velocities[NumKeys - 1] = Velocities[NumKeys - 2];

		// Simple average of all velocities
		FVector AvgVelocity = FVector::ZeroVector;
		for (const FVector& Vel : Velocities)
		{
			AvgVelocity += Vel;
		}
		AvgVelocity /= NumKeys;

		FVector CharacterVelocity = -AvgVelocity * Animation->RateScale;
		UE_LOG(LogBlendSpaceBuilder, Verbose, TEXT("LocomotionSimple: '%s' foot '%s' -> Velocity(%.1f, %.1f)"),
			*Animation->GetName(), *FootBoneName.ToString(), CharacterVelocity.X, CharacterVelocity.Y);
		return FVector(CharacterVelocity.X, CharacterVelocity.Y, 0.f);
	}

	// Calculate locomotion velocity using stride length (max - min position)
	FVector CalculateLocomotionVelocityFromFootStride(const UAnimSequence* Animation, FName FootBoneName)
	{
		TArray<FVector> Positions;
		double DeltaTime;
		if (!CollectFootPositions(Animation, FootBoneName, Positions, DeltaTime))
		{
			return FVector::ZeroVector;
		}

		const int32 NumKeys = Positions.Num();
		double PlayLength = Animation->GetPlayLength();
		if (PlayLength <= KINDA_SMALL_NUMBER)
		{
			return FVector::ZeroVector;
		}

		// Find min/max positions for X and Y
		double MinX = DBL_MAX, MaxX = -DBL_MAX;
		double MinY = DBL_MAX, MaxY = -DBL_MAX;

		for (int32 Key = 0; Key < NumKeys; ++Key)
		{
			MinX = FMath::Min(MinX, Positions[Key].X);
			MaxX = FMath::Max(MaxX, Positions[Key].X);
			MinY = FMath::Min(MinY, Positions[Key].Y);
			MaxY = FMath::Max(MaxY, Positions[Key].Y);
		}

		// Stride length = max - min for each axis
		double StrideX = MaxX - MinX;
		double StrideY = MaxY - MinY;

		// Velocity = Stride / PlayLength (assuming one full cycle)
		// Note: This gives speed magnitude, we need to determine direction
		// For locomotion, Y is forward direction, X is right direction
		FVector CharacterVelocity;
		CharacterVelocity.X = StrideX / PlayLength * Animation->RateScale;
		CharacterVelocity.Y = StrideY / PlayLength * Animation->RateScale;
		CharacterVelocity.Z = 0.f;

		UE_LOG(LogBlendSpaceBuilder, Verbose, TEXT("LocomotionStride: '%s' foot '%s' -> Stride(%.1f, %.1f) Velocity(%.1f, %.1f)"),
			*Animation->GetName(), *FootBoneName.ToString(), StrideX, StrideY, CharacterVelocity.X, CharacterVelocity.Y);
		return CharacterVelocity;
	}

	// Calculate locomotion velocity using both feet (Simple average)
	FVector CalculateLocomotionVelocitySimple(const UAnimSequence* Animation, FName LeftFootBone, FName RightFootBone)
	{
		FVector LeftVel = CalculateLocomotionVelocityFromFootSimple(Animation, LeftFootBone);
		FVector RightVel = CalculateLocomotionVelocityFromFootSimple(Animation, RightFootBone);

		int32 Count = 0;
		FVector TotalVel = FVector::ZeroVector;

		if (!LeftVel.IsNearlyZero())
		{
			TotalVel += LeftVel;
			Count++;
		}
		if (!RightVel.IsNearlyZero())
		{
			TotalVel += RightVel;
			Count++;
		}

		return (Count > 0) ? TotalVel / Count : FVector::ZeroVector;
	}

	// Calculate locomotion velocity using both feet (Stride-based)
	// Combines Simple (direction) + Stride (magnitude) for accurate results
	FVector CalculateLocomotionVelocityStride(const UAnimSequence* Animation, FName LeftFootBone, FName RightFootBone)
	{
		// Get direction from Simple method (accurate direction, but magnitude may be off)
		FVector SimpleVel = CalculateLocomotionVelocitySimple(Animation, LeftFootBone, RightFootBone);

		// Get magnitude from Stride method (accurate magnitude, but always positive)
		FVector LeftStride = CalculateLocomotionVelocityFromFootStride(Animation, LeftFootBone);
		FVector RightStride = CalculateLocomotionVelocityFromFootStride(Animation, RightFootBone);
		FVector StrideVel = LeftStride + RightStride;  // Sum for 2-step cycle

		// Combine: direction from Simple, magnitude from Stride
		FVector Direction = SimpleVel.GetSafeNormal();
		float Magnitude = StrideVel.Size2D();  // Use 2D magnitude (X, Y only)

		FVector Result = Direction * Magnitude;
		UE_LOG(LogBlendSpaceBuilder, Verbose, TEXT("LocomotionStride: Combined Simple dir(%.2f, %.2f) * Stride mag(%.1f) = (%.1f, %.1f)"),
			Direction.X, Direction.Y, Magnitude, Result.X, Result.Y);

		return FVector(Result.X, Result.Y, 0.f);
	}
}

//=============================================================================
// FBlendSpaceFactory Implementation
//=============================================================================

UBlendSpace* FBlendSpaceFactory::CreateLocomotionBlendSpace(const FBlendSpaceBuildConfig& Config)
{
	if (!Config.Skeleton)
	{
		return nullptr;
	}

	UBlendSpace* BlendSpace = CreateBlendSpaceAsset(Config.PackagePath, Config.AssetName, Config.Skeleton);
	if (!BlendSpace)
	{
		return nullptr;
	}

	ConfigureAxes(BlendSpace, Config);

	// Add samples
	if (Config.bApplyAnalysis && Config.PreAnalyzedPositions.Num() > 0)
	{
		// Use pre-analyzed positions from UI
		for (const auto& Pair : Config.SelectedAnimations)
		{
			UAnimSequence* Anim = Pair.Value;
			if (!Anim)
			{
				continue;
			}

			const FVector* PositionPtr = Config.PreAnalyzedPositions.Find(Anim);
			FVector Position = PositionPtr ? *PositionPtr : FVector::ZeroVector;
			AddSampleToBlendSpace(BlendSpace, Anim, Position);
		}
	}
	else
	{
		// Use role-based default positions
		for (const auto& Pair : Config.SelectedAnimations)
		{
			ELocomotionRole Role = Pair.Key;
			UAnimSequence* Anim = Pair.Value;

			if (!Anim)
			{
				continue;
			}

			FVector2D Position = GetPositionForRole(Role, Config);
			FVector Position3D(Position.X, Position.Y, 0.f);
			AddSampleToBlendSpace(BlendSpace, Anim, Position3D);
		}
	}

	// Save build configuration as metadata
	SaveBuildConfigAsMetadata(BlendSpace, Config);

	FinalizeAndSave(BlendSpace);

	if (Config.bOpenInEditor)
	{
		OpenAssetInEditor(BlendSpace);
	}

	return BlendSpace;
}

// Get direction sign based on locomotion role
FVector2D GetRoleDirectionSign(ELocomotionRole Role)
{
	switch (Role)
	{
	case ELocomotionRole::Idle:
		return FVector2D(0, 0);
	case ELocomotionRole::WalkForward:
	case ELocomotionRole::RunForward:
	case ELocomotionRole::SprintForward:
		return FVector2D(0, 1);
	case ELocomotionRole::WalkBackward:
	case ELocomotionRole::RunBackward:
		return FVector2D(0, -1);
	case ELocomotionRole::WalkLeft:
	case ELocomotionRole::RunLeft:
		return FVector2D(-1, 0);
	case ELocomotionRole::WalkRight:
	case ELocomotionRole::RunRight:
		return FVector2D(1, 0);
	case ELocomotionRole::WalkForwardLeft:
	case ELocomotionRole::RunForwardLeft:
		return FVector2D(-1, 1);
	case ELocomotionRole::WalkForwardRight:
	case ELocomotionRole::RunForwardRight:
		return FVector2D(1, 1);
	case ELocomotionRole::WalkBackwardLeft:
	case ELocomotionRole::RunBackwardLeft:
		return FVector2D(-1, -1);
	case ELocomotionRole::WalkBackwardRight:
	case ELocomotionRole::RunBackwardRight:
		return FVector2D(1, -1);
	default:
		return FVector2D(0, 1);  // Default to forward
	}
}

TMap<UAnimSequence*, FVector> FBlendSpaceFactory::AnalyzeSamplePositions(
	const TMap<ELocomotionRole, UAnimSequence*>& Animations,
	EBlendSpaceAnalysisType AnalysisType,
	FName LeftFootBone,
	FName RightFootBone,
	float StrideMultiplier)
{
	TMap<UAnimSequence*, FVector> Result;

	for (const auto& Pair : Animations)
	{
		ELocomotionRole Role = Pair.Key;
		UAnimSequence* Anim = Pair.Value;
		if (!Anim)
		{
			continue;
		}

		// Get analyzed velocity based on analysis type
		FVector AnalyzedVelocity;
		switch (AnalysisType)
		{
		case EBlendSpaceAnalysisType::RootMotion:
			AnalyzedVelocity = BlendSpaceAnalysisInternal::CalculateRootMotionVelocity(Anim);
			break;
		case EBlendSpaceAnalysisType::LocomotionSimple:
			AnalyzedVelocity = BlendSpaceAnalysisInternal::CalculateLocomotionVelocitySimple(Anim, LeftFootBone, RightFootBone);
			break;
		case EBlendSpaceAnalysisType::LocomotionStride:
			AnalyzedVelocity = BlendSpaceAnalysisInternal::CalculateLocomotionVelocityStride(Anim, LeftFootBone, RightFootBone);
			AnalyzedVelocity *= StrideMultiplier;  // Apply multiplier for stride
			break;
		}

		// Apply Role-based direction with analyzed magnitude
		// This ensures samples don't overlap (e.g., Run_Left, Run_Forward, Run_Right all at different positions)
		FVector2D DirSign = GetRoleDirectionSign(Role);
		FVector Position;

		if (DirSign.IsNearlyZero())
		{
			// Idle: use zero position
			Position = FVector::ZeroVector;
		}
		else
		{
			// Get speed magnitude from analyzed velocity (use 2D magnitude)
			float Magnitude = AnalyzedVelocity.Size2D();

			// Normalize direction and apply magnitude
			FVector2D Dir = DirSign.GetSafeNormal();
			Position = FVector(Dir.X * Magnitude, Dir.Y * Magnitude, 0.f);
		}

		Result.Add(Anim, Position);
	}

	return Result;
}

FVector FBlendSpaceFactory::AnalyzeAnimationVelocity(
	UAnimSequence* Animation,
	EBlendSpaceAnalysisType AnalysisType,
	FName LeftFootBone,
	FName RightFootBone)
{
	if (!Animation)
	{
		return FVector::ZeroVector;
	}

	switch (AnalysisType)
	{
	case EBlendSpaceAnalysisType::RootMotion:
		return BlendSpaceAnalysisInternal::CalculateRootMotionVelocity(Animation);
	case EBlendSpaceAnalysisType::LocomotionSimple:
		return BlendSpaceAnalysisInternal::CalculateLocomotionVelocitySimple(Animation, LeftFootBone, RightFootBone);
	case EBlendSpaceAnalysisType::LocomotionStride:
		return BlendSpaceAnalysisInternal::CalculateLocomotionVelocityStride(Animation, LeftFootBone, RightFootBone);
	default:
		return FVector::ZeroVector;
	}
}

void FBlendSpaceFactory::CalculateAxisRangeFromAnalysis(
	const TMap<UAnimSequence*, FVector>& AnalyzedPositions,
	int32 GridDivisions,
	bool bUseNiceNumbers,
	float& OutMinX, float& OutMaxX, float& OutMinY, float& OutMaxY)
{
	OutMinX = 0.f;
	OutMaxX = 0.f;
	OutMinY = 0.f;
	OutMaxY = 0.f;

	for (const auto& Pair : AnalyzedPositions)
	{
		FVector Pos = Pair.Value;
		OutMinX = FMath::Min(OutMinX, Pos.X);
		OutMaxX = FMath::Max(OutMaxX, Pos.X);
		OutMinY = FMath::Min(OutMinY, Pos.Y);
		OutMaxY = FMath::Max(OutMaxY, Pos.Y);
	}

	// Apply symmetric range
	float MaxAbsX = FMath::Max(FMath::Abs(OutMinX), FMath::Abs(OutMaxX));
	float MaxAbsY = FMath::Max(FMath::Abs(OutMinY), FMath::Abs(OutMaxY));

	// Grid step rounding provides natural padding, no explicit padding needed

	// Grid divisions determines step count from center to edge (half the total range)
	// For symmetric range: TotalRange = 2 * MaxAbs, divided into GridDivisions steps
	// So: StepSize = TotalRange / GridDivisions = 2 * MaxAbs / GridDivisions
	// And: MaxAbs = StepSize * (GridDivisions / 2)
	float HalfDivisions = GridDivisions / 2.0f;

	// Calculate raw step sizes
	float RawStepX = MaxAbsX / HalfDivisions;
	float RawStepY = MaxAbsY / HalfDivisions;

	float FinalStepX, FinalStepY;
	if (bUseNiceNumbers)
	{
		// Round to nice numbers (10, 25, 50, 100...)
		FinalStepX = BlendSpaceAnalysisInternal::GetNiceStepSize(RawStepX);
		FinalStepY = BlendSpaceAnalysisInternal::GetNiceStepSize(RawStepY);
	}
	else
	{
		// Round up to nearest integer for exact divisions
		FinalStepX = FMath::CeilToFloat(RawStepX);
		FinalStepY = FMath::CeilToFloat(RawStepY);
	}

	// Calculate final range from step size
	MaxAbsX = FinalStepX * HalfDivisions;
	MaxAbsY = FinalStepY * HalfDivisions;

	// Ensure minimum range
	MaxAbsX = FMath::Max(MaxAbsX, 100.f);
	MaxAbsY = FMath::Max(MaxAbsY, 100.f);

	OutMinX = -MaxAbsX;
	OutMaxX = MaxAbsX;
	OutMinY = -MaxAbsY;
	OutMaxY = MaxAbsY;
}

UBlendSpace* FBlendSpaceFactory::CreateBlendSpaceAsset(const FString& PackagePath, const FString& AssetName, USkeleton* Skeleton)
{
	FString FullPath = PackagePath / AssetName;
	UPackage* Package = CreatePackage(*FullPath);

	if (!Package)
	{
		return nullptr;
	}

	Package->FullyLoad();

	UBlendSpace* BlendSpace = NewObject<UBlendSpace>(Package, *AssetName, RF_Public | RF_Standalone);
	if (!BlendSpace)
	{
		return nullptr;
	}

	BlendSpace->SetSkeleton(Skeleton);

	FAssetRegistryModule::AssetCreated(BlendSpace);

	return BlendSpace;
}

void FBlendSpaceFactory::ConfigureAxes(UBlendSpace* BlendSpace, const FBlendSpaceBuildConfig& Config)
{
	if (!BlendSpace)
	{
		return;
	}

	// Access BlendParameters via reflection (it's protected)
	FProperty* BlendParametersProperty = UBlendSpace::StaticClass()->FindPropertyByName(TEXT("BlendParameters"));
	if (!BlendParametersProperty)
	{
		return;
	}

	FBlendParameter* BlendParameters = BlendParametersProperty->ContainerPtrToValuePtr<FBlendParameter>(BlendSpace);
	if (!BlendParameters)
	{
		return;
	}

	if (Config.LocomotionType == EBlendSpaceLocomotionType::GaitBased)
	{
		// Gait-based axis configuration (fixed ranges)
		// X Axis: Direction (-1 to 1)
		BlendParameters[0].DisplayName = TEXT("Direction");
		BlendParameters[0].Min = -1.f;
		BlendParameters[0].Max = 1.f;
		BlendParameters[0].GridNum = 2;
		BlendParameters[0].bSnapToGrid = true;

		// Y Axis: GaitIndex (-2 to 2)
		BlendParameters[1].DisplayName = TEXT("GaitIndex");
		BlendParameters[1].Min = -2.f;
		BlendParameters[1].Max = 2.f;
		BlendParameters[1].GridNum = 4;
		BlendParameters[1].bSnapToGrid = true;
	}
	else
	{
		// Speed-based axis configuration (configurable ranges)
		// X Axis (Horizontal - Right Velocity)
		BlendParameters[0].DisplayName = Config.XAxisName;
		BlendParameters[0].Min = Config.XAxisMin;
		BlendParameters[0].Max = Config.XAxisMax;
		BlendParameters[0].GridNum = Config.GridDivisions;
		BlendParameters[0].bSnapToGrid = Config.bSnapToGrid;

		// Y Axis (Vertical - Forward Velocity)
		BlendParameters[1].DisplayName = Config.YAxisName;
		BlendParameters[1].Min = Config.YAxisMin;
		BlendParameters[1].Max = Config.YAxisMax;
		BlendParameters[1].GridNum = Config.GridDivisions;
		BlendParameters[1].bSnapToGrid = Config.bSnapToGrid;
	}
}

void FBlendSpaceFactory::AddSampleToBlendSpace(UBlendSpace* BlendSpace, UAnimSequence* Animation, const FVector& Position)
{
	if (!BlendSpace || !Animation)
	{
		return;
	}

	BlendSpace->Modify();
	BlendSpace->AddSample(Animation, Position);
}

void FBlendSpaceFactory::SaveBuildConfigAsMetadata(UBlendSpace* BlendSpace, const FBlendSpaceBuildConfig& Config)
{
	if (!BlendSpace)
	{
		return;
	}

	UBlendSpaceConfigAssetUserData* UserData = BlendSpace->GetAssetUserData<UBlendSpaceConfigAssetUserData>();
	if (!UserData)
	{
		UserData = NewObject<UBlendSpaceConfigAssetUserData>(BlendSpace);
		BlendSpace->AddAssetUserData(UserData);
	}

	// Store locomotion type
	UserData->LocomotionType = Config.LocomotionType;

	// Store X axis configuration
	UserData->XAxis.AxisName = Config.XAxisName;
	UserData->XAxis.AnalyzedMin = Config.XAxisMin;
	UserData->XAxis.AnalyzedMax = Config.XAxisMax;
	UserData->XAxis.GridNum = Config.GridDivisions;

	// Store Y axis configuration
	UserData->YAxis.AxisName = Config.YAxisName;
	UserData->YAxis.AnalyzedMin = Config.YAxisMin;
	UserData->YAxis.AnalyzedMax = Config.YAxisMax;
	UserData->YAxis.GridNum = Config.GridDivisions;

	// Store sample positions
	UserData->Samples.Reset();
	for (const auto& Pair : Config.PreAnalyzedPositions)
	{
		FBlendSpaceSampleMetadata Sample;
		Sample.AnimSequence = FSoftObjectPath(Pair.Key);
		Sample.Position = FVector2D(Pair.Value.X, Pair.Value.Y);
		UserData->Samples.Add(Sample);
	}

	// Store analysis settings
	UserData->bApplyAnalysis = Config.bApplyAnalysis;
	UserData->AnalysisType = Config.AnalysisType;
	UserData->GridDivisions = Config.GridDivisions;
	UserData->bSnapToGrid = Config.bSnapToGrid;

	// Store analyzed speeds (calculated from Role-based animations)
	UserData->WalkSpeed = Config.AnalyzedWalkSpeed;
	UserData->RunSpeed = Config.AnalyzedRunSpeed;
	UserData->SprintSpeed = Config.AnalyzedSprintSpeed;

	// If speeds are not pre-calculated, try to extract from PreAnalyzedPositions + SelectedAnimations
	if (UserData->WalkSpeed <= 0.f || UserData->RunSpeed <= 0.f)
	{
		for (const auto& AnimPair : Config.SelectedAnimations)
		{
			ELocomotionRole Role = AnimPair.Key;
			UAnimSequence* Anim = AnimPair.Value;
			if (!Anim)
			{
				continue;
			}

			const FVector* PosPtr = Config.PreAnalyzedPositions.Find(Anim);
			if (!PosPtr)
			{
				continue;
			}

			// Get speed magnitude from analyzed position (Y = Forward velocity)
			float Speed = FMath::Abs(PosPtr->Y);
			if (Speed <= 0.f)
			{
				Speed = PosPtr->Size2D();  // Fallback to 2D magnitude
			}

			// Categorize by role
			switch (Role)
			{
			case ELocomotionRole::WalkForward:
			case ELocomotionRole::WalkBackward:
			case ELocomotionRole::WalkLeft:
			case ELocomotionRole::WalkRight:
			case ELocomotionRole::WalkForwardLeft:
			case ELocomotionRole::WalkForwardRight:
			case ELocomotionRole::WalkBackwardLeft:
			case ELocomotionRole::WalkBackwardRight:
				UserData->WalkSpeed = FMath::Max(UserData->WalkSpeed, Speed);
				break;

			case ELocomotionRole::RunForward:
			case ELocomotionRole::RunBackward:
			case ELocomotionRole::RunLeft:
			case ELocomotionRole::RunRight:
			case ELocomotionRole::RunForwardLeft:
			case ELocomotionRole::RunForwardRight:
			case ELocomotionRole::RunBackwardLeft:
			case ELocomotionRole::RunBackwardRight:
				UserData->RunSpeed = FMath::Max(UserData->RunSpeed, Speed);
				break;

			case ELocomotionRole::SprintForward:
				UserData->SprintSpeed = FMath::Max(UserData->SprintSpeed, Speed);
				break;

			default:
				break;
			}
		}
	}

	UE_LOG(LogBlendSpaceBuilder, Log, TEXT("Saved build config metadata to BlendSpace: %s (Walk=%.1f, Run=%.1f, Sprint=%.1f)"),
		*BlendSpace->GetName(), UserData->WalkSpeed, UserData->RunSpeed, UserData->SprintSpeed);
}

void FBlendSpaceFactory::FinalizeAndSave(UBlendSpace* BlendSpace)
{
	if (!BlendSpace)
	{
		return;
	}

	// Validate and update internal data
	BlendSpace->ValidateSampleData();

	// Notify editor of changes
	BlendSpace->Modify();
	BlendSpace->PostEditChange();
	BlendSpace->MarkPackageDirty();

	UPackage* Package = BlendSpace->GetOutermost();
	FString PackageFilename;
	if (FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(), PackageFilename, FPackageName::GetAssetPackageExtension()))
	{
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		UPackage::SavePackage(Package, BlendSpace, *PackageFilename, SaveArgs);
	}

	FNotificationInfo Info(FText::Format(
		LOCTEXT("BlendSpaceCreated", "Created BlendSpace: {0}"),
		FText::FromString(BlendSpace->GetName())));
	Info.ExpireDuration = 5.0f;
	Info.bUseSuccessFailIcons = true;
	TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
	if (Notification.IsValid())
	{
		Notification->SetCompletionState(SNotificationItem::CS_Success);
	}
}

FVector2D FBlendSpaceFactory::GetPositionForRole(ELocomotionRole Role, const FBlendSpaceBuildConfig& Config)
{
	// Gait-based mode uses fixed positions based on role
	if (Config.LocomotionType == EBlendSpaceLocomotionType::GaitBased)
	{
		return GetPositionForRoleGait(Role);
	}

	// Speed-based mode uses velocity-based positions
	float MaxSpeed = Config.YAxisMax;
	float WalkSpeed = MaxSpeed * 0.4f;
	float RunSpeed = MaxSpeed * 0.8f;
	float SprintSpeed = MaxSpeed;

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

void FBlendSpaceFactory::OpenAssetInEditor(UBlendSpace* BlendSpace)
{
	if (!BlendSpace || !GEditor)
	{
		return;
	}

	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (AssetEditorSubsystem)
	{
		AssetEditorSubsystem->OpenEditorForAsset(BlendSpace);
	}
}

FVector2D FBlendSpaceFactory::GetPositionForRoleGait(ELocomotionRole Role)
{
	// Gait-based mapping: X=Direction (-1~1), Y=GaitIndex (-2~2)
	// GaitIndex: RunBackward=-2, WalkBackward=-1, Idle=0, Walk=1, Run=2
	// Direction: Left=-1, Center=0, Right=1

	switch (Role)
	{
	case ELocomotionRole::Idle:
		return FVector2D(0.f, 0.f);

	// Walk (GaitIndex = 1)
	case ELocomotionRole::WalkForward:
		return FVector2D(0.f, 1.f);
	case ELocomotionRole::WalkLeft:
	case ELocomotionRole::WalkForwardLeft:
		return FVector2D(-1.f, 1.f);
	case ELocomotionRole::WalkRight:
	case ELocomotionRole::WalkForwardRight:
		return FVector2D(1.f, 1.f);

	// WalkBackward (GaitIndex = -1)
	case ELocomotionRole::WalkBackward:
		return FVector2D(0.f, -1.f);
	case ELocomotionRole::WalkBackwardLeft:
		return FVector2D(-1.f, -1.f);
	case ELocomotionRole::WalkBackwardRight:
		return FVector2D(1.f, -1.f);

	// Run (GaitIndex = 2)
	case ELocomotionRole::RunForward:
	case ELocomotionRole::SprintForward:
		return FVector2D(0.f, 2.f);
	case ELocomotionRole::RunLeft:
	case ELocomotionRole::RunForwardLeft:
		return FVector2D(-1.f, 2.f);
	case ELocomotionRole::RunRight:
	case ELocomotionRole::RunForwardRight:
		return FVector2D(1.f, 2.f);

	// RunBackward (GaitIndex = -2)
	case ELocomotionRole::RunBackward:
		return FVector2D(0.f, -2.f);
	case ELocomotionRole::RunBackwardLeft:
		return FVector2D(-1.f, -2.f);
	case ELocomotionRole::RunBackwardRight:
		return FVector2D(1.f, -2.f);

	default:
		return FVector2D::ZeroVector;
	}
}

#undef LOCTEXT_NAMESPACE
