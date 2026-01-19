#include "BlendSpaceGaitConverter.h"
#include "BlendSpaceConfigAssetUserData.h"

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
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "ObjectTools.h"

#define LOCTEXT_NAMESPACE "BlendSpaceGaitConverter"

DEFINE_LOG_CATEGORY_STATIC(LogGaitConverter, Log, All);

//=============================================================================
// FBlendSpaceGaitConverter Implementation
//=============================================================================

bool FBlendSpaceGaitConverter::IsSpeedBasedBlendSpace(const UBlendSpace* BlendSpace)
{
	if (!BlendSpace)
	{
		return false;
	}

	// Check if already Gait-based by examining axis ranges
	const FBlendParameter& XParam = BlendSpace->GetBlendParameter(0);
	const FBlendParameter& YParam = BlendSpace->GetBlendParameter(1);

	// Gait-based has fixed ranges: X=[-1, 1], Y=[-2, 2]
	bool bIsGaitX = FMath::IsNearlyEqual(XParam.Min, -1.f, 0.1f) && FMath::IsNearlyEqual(XParam.Max, 1.f, 0.1f);
	bool bIsGaitY = FMath::IsNearlyEqual(YParam.Min, -2.f, 0.1f) && FMath::IsNearlyEqual(YParam.Max, 2.f, 0.1f);

	// Also check metadata if available (const_cast needed for GetAssetUserData)
	if (const UBlendSpaceConfigAssetUserData* Metadata = const_cast<UBlendSpace*>(BlendSpace)->GetAssetUserData<UBlendSpaceConfigAssetUserData>())
	{
		if (Metadata->LocomotionType == EBlendSpaceLocomotionType::GaitBased)
		{
			return false;  // Already Gait-based
		}
	}

	// If both axes match Gait range, it's probably already converted
	if (bIsGaitX && bIsGaitY)
	{
		return false;
	}

	// Assume it's Speed-based if ranges are larger than typical Gait ranges
	return (XParam.Max - XParam.Min > 5.f) || (YParam.Max - YParam.Min > 5.f);
}

FGaitConversionResult FBlendSpaceGaitConverter::AnalyzeBlendSpace(UBlendSpace* BlendSpace, const FGaitConversionConfig& Config)
{
	FGaitConversionResult Result;

	if (!BlendSpace)
	{
		Result.ErrorMessage = TEXT("BlendSpace is null");
		return Result;
	}

	if (!IsSpeedBasedBlendSpace(BlendSpace))
	{
		Result.ErrorMessage = TEXT("BlendSpace appears to be already Gait-based or invalid");
		return Result;
	}

	// Get axis ranges
	const FBlendParameter& XParam = BlendSpace->GetBlendParameter(0);
	const FBlendParameter& YParam = BlendSpace->GetBlendParameter(1);

	Result.OriginalXMin = XParam.Min;
	Result.OriginalXMax = XParam.Max;
	Result.OriginalYMin = YParam.Min;
	Result.OriginalYMax = YParam.Max;

	// Calculate max speeds from axis ranges
	float MaxForwardSpeed = YParam.Max;
	float MaxBackwardSpeed = FMath::Abs(YParam.Min);
	float MaxRightSpeed = XParam.Max;
	float MaxLeftSpeed = FMath::Abs(XParam.Min);

	// Process each sample
	const TArray<FBlendSample>& Samples = BlendSpace->GetBlendSamples();

	for (const FBlendSample& Sample : Samples)
	{
		UAnimSequence* Anim = Cast<UAnimSequence>(Sample.Animation);
		if (!Anim)
		{
			continue;
		}

		FVector2D SpeedPos(Sample.SampleValue.X, Sample.SampleValue.Y);
		Result.OriginalSpeedPositions.Add(Anim, SpeedPos);

		// Infer role from speed position
		ELocomotionRole Role = InferRoleFromSpeedPosition(
			SpeedPos, MaxForwardSpeed, MaxBackwardSpeed, MaxRightSpeed, MaxLeftSpeed, Config);
		Result.InferredRoles.Add(Anim, Role);

		// Get new Gait position for this role
		FVector2D GaitPos = GetGaitPositionForRole(Role);
		Result.NewGaitPositions.Add(Anim, GaitPos);

		// Track speeds by role for metadata
		float Speed2D = SpeedPos.Size();
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
			Result.AnalyzedWalkSpeed = FMath::Max(Result.AnalyzedWalkSpeed, Speed2D);
			break;

		case ELocomotionRole::RunForward:
		case ELocomotionRole::RunBackward:
		case ELocomotionRole::RunLeft:
		case ELocomotionRole::RunRight:
		case ELocomotionRole::RunForwardLeft:
		case ELocomotionRole::RunForwardRight:
		case ELocomotionRole::RunBackwardLeft:
		case ELocomotionRole::RunBackwardRight:
		case ELocomotionRole::SprintForward:
			Result.AnalyzedRunSpeed = FMath::Max(Result.AnalyzedRunSpeed, Speed2D);
			break;

		default:
			break;
		}
	}

	Result.bSuccess = true;
	return Result;
}

UBlendSpace* FBlendSpaceGaitConverter::ConvertToGaitBased(
	UBlendSpace* Source,
	const FGaitConversionConfig& Config,
	FGaitConversionResult& OutResult)
{
	// First analyze
	OutResult = AnalyzeBlendSpace(Source, Config);
	if (!OutResult.bSuccess)
	{
		return nullptr;
	}

	// Create copy or use original
	UBlendSpace* TargetBS = nullptr;
	if (Config.bCreateCopy)
	{
		TargetBS = CreateBlendSpaceCopy(Source, Config.OutputSuffix);
		if (!TargetBS)
		{
			OutResult.bSuccess = false;
			OutResult.ErrorMessage = TEXT("Failed to create BlendSpace copy");
			return nullptr;
		}
	}
	else
	{
		TargetBS = Source;
		// Mark for modification when editing in-place
		TargetBS->Modify();
	}

	// Re-analyze target to get correct animation references after copy
	FGaitConversionResult TargetAnalysis = AnalyzeBlendSpace(TargetBS, Config);

	// Configure Gait axes
	ConfigureGaitAxes(TargetBS);

	// Update sample positions
	FProperty* SampleDataProperty = UBlendSpace::StaticClass()->FindPropertyByName(TEXT("SampleData"));
	if (SampleDataProperty)
	{
		TArray<FBlendSample>* SampleData = SampleDataProperty->ContainerPtrToValuePtr<TArray<FBlendSample>>(TargetBS);
		if (SampleData)
		{
			for (FBlendSample& Sample : *SampleData)
			{
				UAnimSequence* Anim = Cast<UAnimSequence>(Sample.Animation);
				if (!Anim)
				{
					continue;
				}

				// Find the new Gait position for this animation
				if (const FVector2D* NewPos = TargetAnalysis.NewGaitPositions.Find(Anim))
				{
					Sample.SampleValue.X = NewPos->X;
					Sample.SampleValue.Y = NewPos->Y;
				}
			}
		}
	}

	// Save conversion metadata
	SaveConversionMetadata(TargetBS, TargetAnalysis,
		OutResult.OriginalXMin, OutResult.OriginalXMax,
		OutResult.OriginalYMin, OutResult.OriginalYMax);

	// Validate changes
	TargetBS->ValidateSampleData();
	TargetBS->PostEditChange();
	TargetBS->MarkPackageDirty();

	// Only save package for new copies (not in-place conversion)
	if (Config.bCreateCopy)
	{
		UPackage* Package = TargetBS->GetOutermost();
		FString PackageFilename;
		if (FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(), PackageFilename, FPackageName::GetAssetPackageExtension()))
		{
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
			UPackage::SavePackage(Package, TargetBS, *PackageFilename, SaveArgs);
		}
	}

	// Open in editor if requested
	if (Config.bOpenInEditor && GEditor)
	{
		UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		if (AssetEditorSubsystem)
		{
			AssetEditorSubsystem->OpenEditorForAsset(TargetBS);
		}
	}

	// Show notification
	FNotificationInfo Info(FText::Format(
		LOCTEXT("GaitConversionSuccess", "Converted BlendSpace to Gait-based: {0}"),
		FText::FromString(TargetBS->GetName())));
	Info.ExpireDuration = 5.0f;
	Info.bUseSuccessFailIcons = true;
	TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
	if (Notification.IsValid())
	{
		Notification->SetCompletionState(SNotificationItem::CS_Success);
	}

	OutResult.bSuccess = true;
	return TargetBS;
}

ELocomotionRole FBlendSpaceGaitConverter::InferRoleFromSpeedPosition(
	const FVector2D& SpeedPosition,
	float MaxForwardSpeed, float MaxBackwardSpeed,
	float MaxRightSpeed, float MaxLeftSpeed,
	const FGaitConversionConfig& Config)
{
	float Speed2D = SpeedPosition.Size();

	// Idle detection
	if (Speed2D < Config.IdleSpeedThreshold)
	{
		return ELocomotionRole::Idle;
	}

	// Calculate direction angle (degrees)
	float AngleDegrees = GetDirectionAngle(SpeedPosition);
	EDirectionCategory Direction = GetDirectionCategory(AngleDegrees);

	// Calculate normalized speed for gait tier
	// Normalize against the max speed in the dominant direction
	float NormalizedSpeed = 0.f;
	if (FMath::Abs(SpeedPosition.Y) >= FMath::Abs(SpeedPosition.X))
	{
		// Forward/Backward dominant
		float MaxSpeed = (SpeedPosition.Y >= 0) ? MaxForwardSpeed : MaxBackwardSpeed;
		NormalizedSpeed = (MaxSpeed > 0) ? Speed2D / MaxSpeed : 0.f;
	}
	else
	{
		// Left/Right dominant
		float MaxSpeed = (SpeedPosition.X >= 0) ? MaxRightSpeed : MaxLeftSpeed;
		NormalizedSpeed = (MaxSpeed > 0) ? Speed2D / MaxSpeed : 0.f;
	}

	bool bIsRun = IsRunGait(NormalizedSpeed, Config);

	// Map direction + gait to role
	switch (Direction)
	{
	case EDirectionCategory::Forward:
		return bIsRun ? ELocomotionRole::RunForward : ELocomotionRole::WalkForward;

	case EDirectionCategory::ForwardLeft:
		return bIsRun ? ELocomotionRole::RunForwardLeft : ELocomotionRole::WalkForwardLeft;

	case EDirectionCategory::Left:
		return bIsRun ? ELocomotionRole::RunLeft : ELocomotionRole::WalkLeft;

	case EDirectionCategory::BackwardLeft:
		return bIsRun ? ELocomotionRole::RunBackwardLeft : ELocomotionRole::WalkBackwardLeft;

	case EDirectionCategory::Backward:
		return bIsRun ? ELocomotionRole::RunBackward : ELocomotionRole::WalkBackward;

	case EDirectionCategory::BackwardRight:
		return bIsRun ? ELocomotionRole::RunBackwardRight : ELocomotionRole::WalkBackwardRight;

	case EDirectionCategory::Right:
		return bIsRun ? ELocomotionRole::RunRight : ELocomotionRole::WalkRight;

	case EDirectionCategory::ForwardRight:
		return bIsRun ? ELocomotionRole::RunForwardRight : ELocomotionRole::WalkForwardRight;

	default:
		return bIsRun ? ELocomotionRole::RunForward : ELocomotionRole::WalkForward;
	}
}

UBlendSpace* FBlendSpaceGaitConverter::CreateBlendSpaceCopy(UBlendSpace* Source, const FString& Suffix)
{
	if (!Source)
	{
		return nullptr;
	}

	// Get source package path
	FString SourcePackagePath = FPackageName::GetLongPackagePath(Source->GetOutermost()->GetName());
	FString NewAssetName = Source->GetName() + Suffix;
	FString NewPackagePath = SourcePackagePath / NewAssetName;

	// Create new package
	UPackage* NewPackage = CreatePackage(*NewPackagePath);
	if (!NewPackage)
	{
		return nullptr;
	}
	NewPackage->FullyLoad();

	// Duplicate the BlendSpace
	UBlendSpace* NewBlendSpace = DuplicateObject<UBlendSpace>(Source, NewPackage, *NewAssetName);
	if (!NewBlendSpace)
	{
		return nullptr;
	}

	NewBlendSpace->SetFlags(RF_Public | RF_Standalone);
	NewBlendSpace->ClearFlags(RF_Transient);

	FAssetRegistryModule::AssetCreated(NewBlendSpace);

	return NewBlendSpace;
}

void FBlendSpaceGaitConverter::ConfigureGaitAxes(UBlendSpace* BlendSpace)
{
	if (!BlendSpace)
	{
		return;
	}

	// Access BlendParameters via reflection
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

FVector2D FBlendSpaceGaitConverter::GetGaitPositionForRole(ELocomotionRole Role)
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

bool FBlendSpaceGaitConverter::IsRunGait(float NormalizedSpeed, const FGaitConversionConfig& Config)
{
	return NormalizedSpeed >= Config.WalkToRunSpeedRatio;
}

float FBlendSpaceGaitConverter::GetDirectionAngle(const FVector2D& SpeedPosition)
{
	// Returns angle in degrees, with:
	// 0° = Right (+X)
	// 90° = Forward (+Y)
	// 180° / -180° = Left (-X)
	// -90° = Backward (-Y)
	return FMath::RadiansToDegrees(FMath::Atan2(SpeedPosition.Y, SpeedPosition.X));
}

FBlendSpaceGaitConverter::EDirectionCategory FBlendSpaceGaitConverter::GetDirectionCategory(float AngleDegrees)
{
	// 8-direction classification with 45° sectors
	// Forward: 67.5° ~ 112.5°
	// ForwardLeft: 112.5° ~ 157.5°
	// Left: 157.5° ~ 180° or -180° ~ -157.5°
	// BackwardLeft: -157.5° ~ -112.5°
	// Backward: -112.5° ~ -67.5°
	// BackwardRight: -67.5° ~ -22.5°
	// Right: -22.5° ~ 22.5°
	// ForwardRight: 22.5° ~ 67.5°

	if (AngleDegrees >= 67.5f && AngleDegrees < 112.5f)
	{
		return EDirectionCategory::Forward;
	}
	else if (AngleDegrees >= 112.5f && AngleDegrees < 157.5f)
	{
		return EDirectionCategory::ForwardLeft;
	}
	else if (AngleDegrees >= 157.5f || AngleDegrees < -157.5f)
	{
		return EDirectionCategory::Left;
	}
	else if (AngleDegrees >= -157.5f && AngleDegrees < -112.5f)
	{
		return EDirectionCategory::BackwardLeft;
	}
	else if (AngleDegrees >= -112.5f && AngleDegrees < -67.5f)
	{
		return EDirectionCategory::Backward;
	}
	else if (AngleDegrees >= -67.5f && AngleDegrees < -22.5f)
	{
		return EDirectionCategory::BackwardRight;
	}
	else if (AngleDegrees >= -22.5f && AngleDegrees < 22.5f)
	{
		return EDirectionCategory::Right;
	}
	else // 22.5f ~ 67.5f
	{
		return EDirectionCategory::ForwardRight;
	}
}

void FBlendSpaceGaitConverter::SaveConversionMetadata(
	UBlendSpace* BlendSpace,
	const FGaitConversionResult& Result,
	float OriginalXMin, float OriginalXMax,
	float OriginalYMin, float OriginalYMax)
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

	// Mark as converted
	UserData->bConvertedFromSpeedBased = true;
	UserData->LocomotionType = EBlendSpaceLocomotionType::GaitBased;

	// Store original axis ranges
	UserData->OriginalXAxis.AxisName = TEXT("RightVelocity");
	UserData->OriginalXAxis.AnalyzedMin = OriginalXMin;
	UserData->OriginalXAxis.AnalyzedMax = OriginalXMax;

	UserData->OriginalYAxis.AxisName = TEXT("ForwardVelocity");
	UserData->OriginalYAxis.AnalyzedMin = OriginalYMin;
	UserData->OriginalYAxis.AnalyzedMax = OriginalYMax;

	// Store current axis ranges
	UserData->XAxis.AxisName = TEXT("Direction");
	UserData->XAxis.AnalyzedMin = -1.f;
	UserData->XAxis.AnalyzedMax = 1.f;
	UserData->XAxis.GridNum = 2;

	UserData->YAxis.AxisName = TEXT("GaitIndex");
	UserData->YAxis.AnalyzedMin = -2.f;
	UserData->YAxis.AnalyzedMax = 2.f;
	UserData->YAxis.GridNum = 4;

	// Store original speed data for each sample
	UserData->OriginalSpeedData.Reset();
	for (const auto& Pair : Result.OriginalSpeedPositions)
	{
		FBlendSpaceOriginalSpeedData SpeedData;
		SpeedData.AnimSequence = FSoftObjectPath(Pair.Key);
		SpeedData.OriginalSpeedPosition = Pair.Value;

		if (const ELocomotionRole* RolePtr = Result.InferredRoles.Find(Pair.Key))
		{
			SpeedData.InferredRole = *RolePtr;
		}

		UserData->OriginalSpeedData.Add(SpeedData);
	}

	// Store sample metadata (current Gait positions)
	UserData->Samples.Reset();
	for (const auto& Pair : Result.NewGaitPositions)
	{
		FBlendSpaceSampleMetadata Sample;
		Sample.AnimSequence = FSoftObjectPath(Pair.Key);
		Sample.Position = Pair.Value;
		UserData->Samples.Add(Sample);
	}

	// Store analyzed speeds for AnimInstance threshold calculation
	UserData->WalkSpeed = Result.AnalyzedWalkSpeed;
	UserData->RunSpeed = Result.AnalyzedRunSpeed;

	UE_LOG(LogGaitConverter, Log,
		TEXT("Saved conversion metadata to BlendSpace: %s (Walk=%.1f, Run=%.1f, OriginalRange: X[%.1f~%.1f], Y[%.1f~%.1f])"),
		*BlendSpace->GetName(), UserData->WalkSpeed, UserData->RunSpeed,
		OriginalXMin, OriginalXMax, OriginalYMin, OriginalYMax);
}

#undef LOCTEXT_NAMESPACE
