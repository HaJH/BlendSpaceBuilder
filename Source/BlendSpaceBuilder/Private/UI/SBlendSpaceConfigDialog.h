#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "BlendSpaceFactory.h"

class USkeleton;
class FLocomotionAnimClassifier;
class UAnimSequence;
struct FLocomotionRoleCandidates;
enum class ELocomotionRole : uint8;

DECLARE_DELEGATE_OneParam(FOnBlendSpaceConfigAccepted, const FBlendSpaceBuildConfig&);

class SBlendSpaceConfigDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlendSpaceConfigDialog) {}
		SLATE_ARGUMENT(USkeleton*, Skeleton)
		SLATE_ARGUMENT(TSharedPtr<FLocomotionAnimClassifier>, Classifier)
		SLATE_ARGUMENT(FString, BasePath)
		SLATE_ARGUMENT(TSharedPtr<SWindow>, ParentWindow)
		SLATE_EVENT(FOnBlendSpaceConfigAccepted, OnAccepted)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	bool WasAccepted() const { return bWasAccepted; }
	FBlendSpaceBuildConfig GetBuildConfig() const;

private:
	TSharedRef<SWidget> BuildAxisConfigSection();
	TSharedRef<SWidget> BuildAnimationSelectionSection();
	TSharedRef<SWidget> BuildOutputPathSection();
	TSharedRef<SWidget> BuildButtonSection();

	TSharedRef<SWidget> BuildRoleRow(ELocomotionRole Role, FLocomotionRoleCandidates* Candidates);

	FReply OnAcceptClicked();
	FReply OnCancelClicked();

	void OnAnimationSelected(ELocomotionRole Role, UAnimSequence* SelectedAnim);

	USkeleton* Skeleton = nullptr;
	TSharedPtr<FLocomotionAnimClassifier> Classifier;
	FString BasePath;
	TSharedPtr<SWindow> ParentWindow;
	FOnBlendSpaceConfigAccepted OnAcceptedDelegate;

	float XAxisMin = -500.f;
	float XAxisMax = 500.f;
	float YAxisMin = -500.f;
	float YAxisMax = 500.f;
	FString OutputAssetName;

	TMap<ELocomotionRole, UAnimSequence*> SelectedAnimations;
	bool bWasAccepted = false;

	// Locomotion type selection (Speed-based or Gait-based)
	EBlendSpaceLocomotionType SelectedLocomotionType = EBlendSpaceLocomotionType::SpeedBased;

	// Analysis type selection
	EBlendSpaceAnalysisType SelectedAnalysisType = EBlendSpaceAnalysisType::RootMotion;

	// Detected foot bones for Locomotion analysis
	FName DetectedLeftFootBone = NAME_None;
	FName DetectedRightFootBone = NAME_None;

	// Custom foot bone override
	bool bUseCustomFootBones = false;
	FName CustomLeftFootBone = NAME_None;
	FName CustomRightFootBone = NAME_None;

	// Analysis results (populated by Analyze button)
	TMap<UAnimSequence*, FVector> AnalyzedPositions;
	bool bAnalysisPerformed = false;
	bool bUseAnalyzedPositions = true;

	// Max speed from analysis (used for Reset to Role Defaults)
	float AnalyzedMaxSpeed = 0.f;

	// Calculated axis range from analysis
	float AnalyzedXMin = -500.f;
	float AnalyzedXMax = 500.f;
	float AnalyzedYMin = -500.f;
	float AnalyzedYMax = 500.f;

	// UI builders
	TSharedRef<SWidget> BuildLocomotionTypeSection();
	TSharedRef<SWidget> BuildAnalysisSection();
	TSharedRef<SWidget> BuildAnalysisResultsSection();
	TSharedRef<SWidget> BuildGridConfigSection();

	// Event handlers
	FReply OnAnalyzeClicked();
	FReply OnResetToRoleDefaultsClicked();
	void OnAnalysisTypeChanged(EBlendSpaceAnalysisType NewType);
	void OnGridDivisionsChanged(int32 NewValue);
	void OnSnapToGridChanged(ECheckBoxState NewState);
	void OnUseNiceNumbersChanged(ECheckBoxState NewState);
	void RecalculateAxisRange();

	// UI helpers
	EVisibility GetFootBoneVisibility() const;
	EVisibility GetAnalysisResultsVisibility() const;
	FText GetFootBoneText() const;
	FText GetAnalysisResultsText() const;
	FText GetAxisRangeText() const;
	bool HasSelectedAnimations() const;

	// Grid configuration
	int32 GridDivisions = 4;
	bool bSnapToGrid = true;
	bool bUseNiceNumbers = false;

	// Stride analysis multiplier (to compensate for underestimation)
	float StrideMultiplier = 1.0f;

	// Scale divisor (to handle different skeleton scales)
	float ScaleDivisor = 1.0f;
};
