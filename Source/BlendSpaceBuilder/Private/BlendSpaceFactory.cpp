#include "BlendSpaceFactory.h"

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

//=============================================================================
// Internal helper functions for analysis
//=============================================================================

namespace BlendSpaceAnalysisInternal
{
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
			return FVector::ZeroVector;
		}

		double PlayLength = Animation->GetPlayLength();
		if (PlayLength <= KINDA_SMALL_NUMBER)
		{
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

		// UE coordinate system: X=Forward, Y=Right, Z=Up
		// BlendSpace axes: X=RightVelocity, Y=ForwardVelocity
		return FVector(Velocity.Y, Velocity.X, 0.f);
	}

	// Calculate locomotion velocity from a single foot bone
	// Based on engine's LocomotionAnalysis.cpp implementation
	FVector CalculateLocomotionVelocityFromFoot(const UAnimSequence* Animation, FName FootBoneName)
	{
		if (!Animation || FootBoneName == NAME_None)
		{
			return FVector::ZeroVector;
		}

		const int32 NumKeys = Animation->GetNumberOfSampledKeys();
		if (NumKeys <= 1)
		{
			return FVector::ZeroVector;
		}

		USkeleton* Skeleton = Animation->GetSkeleton();
		if (!Skeleton)
		{
			return FVector::ZeroVector;
		}

		int32 BoneIndex = Skeleton->GetReferenceSkeleton().FindBoneIndex(FootBoneName);
		if (BoneIndex == INDEX_NONE)
		{
			UE_LOG(LogTemp, Warning, TEXT("BlendSpaceFactory: Foot bone '%s' not found in skeleton"), *FootBoneName.ToString());
			return FVector::ZeroVector;
		}

		const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
		double DeltaTime = Animation->GetPlayLength() / double(NumKeys);

		// 1. Collect positions at each key (in component space)
		TArray<FVector> Positions;
		Positions.SetNum(NumKeys);

		double MinHeight = DBL_MAX;
		double MaxHeight = -DBL_MAX;

		for (int32 Key = 0; Key < NumKeys; ++Key)
		{
			// Use component space transform (not bone local)
			FTransform ComponentSpaceTM = GetComponentSpaceTransform(
				Animation, RefSkeleton, BoneIndex, Key * DeltaTime);
			Positions[Key] = ComponentSpaceTM.GetTranslation();

			double Height = Positions[Key].Z;
			MinHeight = FMath::Min(MinHeight, Height);
			MaxHeight = FMath::Max(MaxHeight, Height);
		}

		// Handle case where foot doesn't move vertically
		if (MaxHeight - MinHeight < KINDA_SMALL_NUMBER)
		{
			return FVector::ZeroVector;
		}

		// 2. Calculate velocities using central difference
		TArray<FVector> Velocities;
		Velocities.SetNum(NumKeys);

		for (int32 Key = 0; Key < NumKeys; ++Key)
		{
			int32 PrevKey = (Key + NumKeys - 1) % NumKeys;
			int32 NextKey = (Key + 1) % NumKeys;
			Velocities[Key] = (Positions[NextKey] - Positions[PrevKey]) / (2.0 * DeltaTime);
		}

		// 3. Calculate height-biased velocity (lower = more weight = ground contact)
		FVector BiasedVelocity = FVector::ZeroVector;
		double TotalWeight = 0;

		for (int32 Key = 0; Key < NumKeys; ++Key)
		{
			double Height = Positions[Key].Z;
			double Weight = 1.0 - (Height - MinHeight) / (MaxHeight - MinHeight + KINDA_SMALL_NUMBER);
			BiasedVelocity += Velocities[Key] * Weight;
			TotalWeight += Weight;
		}

		if (TotalWeight > KINDA_SMALL_NUMBER)
		{
			BiasedVelocity /= TotalWeight;
		}

		// Character velocity is opposite of foot velocity during ground contact
		FVector CharacterVelocity = -BiasedVelocity * Animation->RateScale;

		// UE coordinates: X=Forward, Y=Right, Z=Up
		// BlendSpace: X=Right, Y=Forward (2D only, Z must be 0)
		// Note: Component space from GetBoneTransform uses Y=Forward, X=Right convention
		return FVector(CharacterVelocity.X, CharacterVelocity.Y, 0.f);
	}

	// Calculate locomotion velocity using both feet
	FVector CalculateLocomotionVelocity(const UAnimSequence* Animation, FName LeftFootBone, FName RightFootBone)
	{
		FVector LeftVel = CalculateLocomotionVelocityFromFoot(Animation, LeftFootBone);
		FVector RightVel = CalculateLocomotionVelocityFromFoot(Animation, RightFootBone);

		// Average velocities from both feet
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

		if (Count > 0)
		{
			return TotalVel / Count;
		}

		return FVector::ZeroVector;
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

	FinalizeAndSave(BlendSpace);

	if (Config.bOpenInEditor)
	{
		OpenAssetInEditor(BlendSpace);
	}

	return BlendSpace;
}

TMap<UAnimSequence*, FVector> FBlendSpaceFactory::AnalyzeSamplePositions(
	const TMap<ELocomotionRole, UAnimSequence*>& Animations,
	EBlendSpaceAnalysisType AnalysisType,
	FName LeftFootBone,
	FName RightFootBone)
{
	TMap<UAnimSequence*, FVector> Result;

	for (const auto& Pair : Animations)
	{
		UAnimSequence* Anim = Pair.Value;
		if (!Anim)
		{
			continue;
		}

		FVector Position;
		if (AnalysisType == EBlendSpaceAnalysisType::RootMotion)
		{
			Position = BlendSpaceAnalysisInternal::CalculateRootMotionVelocity(Anim);
		}
		else
		{
			Position = BlendSpaceAnalysisInternal::CalculateLocomotionVelocity(Anim, LeftFootBone, RightFootBone);
		}

		Result.Add(Anim, Position);
	}

	return Result;
}

void FBlendSpaceFactory::CalculateAxisRangeFromAnalysis(
	const TMap<UAnimSequence*, FVector>& AnalyzedPositions,
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

	// Apply padding (10%) and round to nearest 50
	MaxAbsX = FMath::CeilToFloat(MaxAbsX * 1.1f / 50.f) * 50.f;
	MaxAbsY = FMath::CeilToFloat(MaxAbsY * 1.1f / 50.f) * 50.f;

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

	// X Axis (Horizontal - Right Velocity)
	BlendParameters[0].DisplayName = Config.XAxisName;
	BlendParameters[0].Min = Config.XAxisMin;
	BlendParameters[0].Max = Config.XAxisMax;
	BlendParameters[0].GridNum = 4;

	// Y Axis (Vertical - Forward Velocity)
	BlendParameters[1].DisplayName = Config.YAxisName;
	BlendParameters[1].Min = Config.YAxisMin;
	BlendParameters[1].Max = Config.YAxisMax;
	BlendParameters[1].GridNum = 4;
}

void FBlendSpaceFactory::AddSampleToBlendSpace(UBlendSpace* BlendSpace, UAnimSequence* Animation, const FVector& Position)
{
	if (!BlendSpace || !Animation)
	{
		return;
	}

	BlendSpace->AddSample(Animation, Position);
}

void FBlendSpaceFactory::FinalizeAndSave(UBlendSpace* BlendSpace)
{
	if (!BlendSpace)
	{
		return;
	}

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

#undef LOCTEXT_NAMESPACE
