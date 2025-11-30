#include "BlendSpaceFactory.h"

#include "Animation/BlendSpace.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "BlendSpaceFactory"

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

	// Add samples with role-based default positions
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

	// Apply UE5 BlendSpace Analysis if requested
	if (Config.bApplyAnalysis)
	{
		ApplyAnalysisToBlendSpace(BlendSpace);
		AutoAdjustAxisRange(BlendSpace);
	}

	FinalizeAndSave(BlendSpace);

	return BlendSpace;
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

FVector2D FBlendSpaceFactory::CalculateRootMotionVelocity(const UAnimSequence* Animation)
{
	if (!Animation || !Animation->bEnableRootMotion)
	{
		return FVector2D::ZeroVector;
	}

	double PlayLength = Animation->GetPlayLength();
	if (PlayLength <= KINDA_SMALL_NUMBER)
	{
		return FVector2D::ZeroVector;
	}

	// Extract root motion from start to end of animation
	FAnimExtractContext ExtractionContext(0.0, true);
	FTransform RootMotion = Animation->ExtractRootMotionFromRange(0.0, PlayLength, ExtractionContext);

	// Calculate velocity (distance / time)
	FVector Translation = RootMotion.GetTranslation();
	FVector Velocity = Translation / PlayLength;

	// Apply rate scale
	Velocity *= Animation->RateScale;

	// Return X (Right) and Y (Forward) velocities
	return FVector2D(Velocity.X, Velocity.Y);
}

void FBlendSpaceFactory::ApplyAnalysisToBlendSpace(UBlendSpace* BlendSpace)
{
	if (!BlendSpace)
	{
		return;
	}

	// Apply analysis to all samples
	// Priority: RootMotion animations get analyzed, non-RootMotion keep original position
	for (int32 SampleIndex = 0; SampleIndex < BlendSpace->GetNumberOfBlendSamples(); ++SampleIndex)
	{
		const FBlendSample& Sample = BlendSpace->GetBlendSample(SampleIndex);
		if (!Sample.Animation)
		{
			continue;
		}

		// Check if animation has RootMotion enabled
		bool bHasRootMotion = Sample.Animation->bEnableRootMotion;

		if (bHasRootMotion)
		{
			// RootMotion animation: Calculate velocity from root motion
			FVector2D Velocity = CalculateRootMotionVelocity(Sample.Animation);

			if (!Velocity.IsNearlyZero())
			{
				FVector NewPosition(Velocity.X, Velocity.Y, 0.f);
				BlendSpace->EditSampleValue(SampleIndex, NewPosition);
			}
		}
		// Non-RootMotion animation: Keep original role-based position (fallback)
		// No action needed - position already set from GetPositionForRole()
	}
}

void FBlendSpaceFactory::AutoAdjustAxisRange(UBlendSpace* BlendSpace)
{
	if (!BlendSpace || BlendSpace->GetNumberOfBlendSamples() == 0)
	{
		return;
	}

	// Find min/max from analyzed sample positions
	float MinX = TNumericLimits<float>::Max();
	float MaxX = TNumericLimits<float>::Lowest();
	float MinY = TNumericLimits<float>::Max();
	float MaxY = TNumericLimits<float>::Lowest();

	for (int32 SampleIndex = 0; SampleIndex < BlendSpace->GetNumberOfBlendSamples(); ++SampleIndex)
	{
		const FBlendSample& Sample = BlendSpace->GetBlendSample(SampleIndex);
		MinX = FMath::Min(MinX, Sample.SampleValue.X);
		MaxX = FMath::Max(MaxX, Sample.SampleValue.X);
		MinY = FMath::Min(MinY, Sample.SampleValue.Y);
		MaxY = FMath::Max(MaxY, Sample.SampleValue.Y);
	}

	// Use symmetric range based on max absolute value
	float MaxAbsX = FMath::Max(FMath::Abs(MinX), FMath::Abs(MaxX));
	float MaxAbsY = FMath::Max(FMath::Abs(MinY), FMath::Abs(MaxY));

	// Apply padding (10%)
	const float Padding = 1.1f;
	MaxAbsX *= Padding;
	MaxAbsY *= Padding;

	// Round to nearest nice number (multiple of 50)
	MaxAbsX = FMath::CeilToFloat(MaxAbsX / 50.f) * 50.f;
	MaxAbsY = FMath::CeilToFloat(MaxAbsY / 50.f) * 50.f;

	// Ensure minimum range
	MaxAbsX = FMath::Max(MaxAbsX, 100.f);
	MaxAbsY = FMath::Max(MaxAbsY, 100.f);

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

	// Update axis ranges
	BlendParameters[0].Min = -MaxAbsX;
	BlendParameters[0].Max = MaxAbsX;
	BlendParameters[1].Min = -MaxAbsY;
	BlendParameters[1].Max = MaxAbsY;
}

#undef LOCTEXT_NAMESPACE
