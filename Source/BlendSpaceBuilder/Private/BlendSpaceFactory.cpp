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

	// X Axis (Horizontal - Right Velocity)
	FBlendParameter XAxis = BlendSpace->GetBlendParameter(0);
	XAxis.DisplayName = Config.XAxisName;
	XAxis.Min = Config.XAxisMin;
	XAxis.Max = Config.XAxisMax;
	XAxis.GridNum = 4;
	BlendSpace->UpdateParameter(0, XAxis);

	// Y Axis (Vertical - Forward Velocity)
	FBlendParameter YAxis = BlendSpace->GetBlendParameter(1);
	YAxis.DisplayName = Config.YAxisName;
	YAxis.Min = Config.YAxisMin;
	YAxis.Max = Config.YAxisMax;
	YAxis.GridNum = 4;
	BlendSpace->UpdateParameter(1, YAxis);
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

#undef LOCTEXT_NAMESPACE
