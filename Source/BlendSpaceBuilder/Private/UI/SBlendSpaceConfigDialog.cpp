#include "SBlendSpaceConfigDialog.h"
#include "SLocomotionAnimSelector.h"

#include "BlendSpaceBuilderSettings.h"
#include "LocomotionAnimClassifier.h"
#include "BlendSpaceFactory.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimSequence.h"

#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SWidgetSwitcher.h"

#define LOCTEXT_NAMESPACE "SBlendSpaceConfigDialog"

void SBlendSpaceConfigDialog::Construct(const FArguments& InArgs)
{
	Skeleton = InArgs._Skeleton;
	Classifier = InArgs._Classifier;
	BasePath = InArgs._BasePath;
	ParentWindow = InArgs._ParentWindow;

	UBlendSpaceBuilderSettings* Settings = UBlendSpaceBuilderSettings::Get();
	XAxisMin = Settings->DefaultMinSpeed;
	XAxisMax = Settings->DefaultMaxSpeed;
	YAxisMin = Settings->DefaultMinSpeed;
	YAxisMax = Settings->DefaultMaxSpeed;

	if (Skeleton)
	{
		OutputAssetName = Skeleton->GetName() + Settings->OutputAssetSuffix;
	}

	// Initialize selected animations with recommendations
	if (Classifier)
	{
		bool bPreferRootMotion = Settings->bPreferRootMotionAnimations;
		for (auto& Pair : Classifier->GetClassifiedResults())
		{
			FLocomotionRoleCandidates& Candidates = const_cast<FLocomotionRoleCandidates&>(Pair.Value);
			if (FClassifiedAnimation* Recommended = Candidates.GetRecommended(bPreferRootMotion))
			{
				SelectedAnimations.Add(Pair.Key, Recommended->Animation.Get());
			}
		}
	}

	// Detect foot bones for Locomotion analysis
	if (Skeleton)
	{
		DetectedLeftFootBone = Settings->FindLeftFootBone(Skeleton);
		DetectedRightFootBone = Settings->FindRightFootBone(Skeleton);
	}

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(8)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					BuildAnimationSelectionSection()
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 8)
				[
					SNew(SSeparator)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					BuildAnalysisSection()
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					BuildAnalysisResultsSection()
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 8)
				[
					SNew(SSeparator)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					BuildAxisConfigSection()
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 8)
				[
					SNew(SSeparator)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					BuildOutputPathSection()
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8)
		[
			BuildButtonSection()
		]
	];
}

TSharedRef<SWidget> SBlendSpaceConfigDialog::BuildAnalysisSection()
{
	return SNew(SExpandableArea)
		.AreaTitle(LOCTEXT("Analysis", "Analysis"))
		.InitiallyCollapsed(false)
		.BodyContent()
		[
			SNew(SVerticalBox)
			// Analysis Type Selection
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(0.3f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("AnalysisType", "Analysis Type:"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.7f)
				[
					SNew(SSegmentedControl<EBlendSpaceAnalysisType>)
					.Value_Lambda([this]() { return SelectedAnalysisType; })
					.OnValueChanged(this, &SBlendSpaceConfigDialog::OnAnalysisTypeChanged)
					+ SSegmentedControl<EBlendSpaceAnalysisType>::Slot(EBlendSpaceAnalysisType::RootMotion)
					.Text(LOCTEXT("RootMotion", "Root Motion"))
					.ToolTip(LOCTEXT("RootMotionTip", "Calculate position from root motion velocity"))
					+ SSegmentedControl<EBlendSpaceAnalysisType>::Slot(EBlendSpaceAnalysisType::Locomotion)
					.Text(LOCTEXT("Locomotion", "Locomotion"))
					.ToolTip(LOCTEXT("LocomotionTip", "Calculate position from foot movement (for in-place animations)"))
				]
			]
			// Foot Bone Info (only shown when Locomotion is selected)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4)
			[
				SNew(STextBlock)
				.Visibility(this, &SBlendSpaceConfigDialog::GetFootBoneVisibility)
				.Text(this, &SBlendSpaceConfigDialog::GetFootBoneText)
				.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))
			]
			// Analyze Button
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4, 8)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("Analyze", "Analyze Samples"))
					.OnClicked(this, &SBlendSpaceConfigDialog::OnAnalyzeClicked)
					.IsEnabled_Lambda([this]() { return HasSelectedAnimations(); })
					.ToolTipText(LOCTEXT("AnalyzeTip", "Calculate sample positions based on selected analysis type"))
				]
			]
		];
}

TSharedRef<SWidget> SBlendSpaceConfigDialog::BuildAnalysisResultsSection()
{
	return SNew(SBorder)
		.Visibility(this, &SBlendSpaceConfigDialog::GetAnalysisResultsVisibility)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(8)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 4)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AnalysisResults", "Analysis Results:"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
			]
			// Results list
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4)
			[
				SNew(STextBlock)
				.Text(this, &SBlendSpaceConfigDialog::GetAnalysisResultsText)
				.AutoWrapText(true)
			]
			// Calculated axis range
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4)
			[
				SNew(STextBlock)
				.Text(this, &SBlendSpaceConfigDialog::GetAxisRangeText)
				.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.8f, 0.6f)))
			]
			// Use analyzed positions checkbox
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4, 8, 4, 0)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([this]() { return bUseAnalyzedPositions ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState) { bUseAnalyzedPositions = (NewState == ECheckBoxState::Checked); })
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4, 0, 0, 0)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("UseAnalyzed", "Use analyzed positions"))
					.ToolTipText(LOCTEXT("UseAnalyzedTip", "If unchecked, role-based default positions will be used instead"))
				]
			]
		];
}

TSharedRef<SWidget> SBlendSpaceConfigDialog::BuildAxisConfigSection()
{
	return SNew(SExpandableArea)
		.AreaTitle(LOCTEXT("AxisConfig", "Axis Configuration"))
		.InitiallyCollapsed(false)
		.BodyContent()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(0.3f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("XAxisRange", "X Axis (Right):"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.35f)
				.Padding(4, 0)
				[
					SNew(SNumericEntryBox<float>)
					.Value_Lambda([this]() { return XAxisMin; })
					.OnValueCommitted_Lambda([this](float Value, ETextCommit::Type) { XAxisMin = Value; })
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.35f)
				.Padding(4, 0)
				[
					SNew(SNumericEntryBox<float>)
					.Value_Lambda([this]() { return XAxisMax; })
					.OnValueCommitted_Lambda([this](float Value, ETextCommit::Type) { XAxisMax = Value; })
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(0.3f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("YAxisRange", "Y Axis (Forward):"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.35f)
				.Padding(4, 0)
				[
					SNew(SNumericEntryBox<float>)
					.Value_Lambda([this]() { return YAxisMin; })
					.OnValueCommitted_Lambda([this](float Value, ETextCommit::Type) { YAxisMin = Value; })
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.35f)
				.Padding(4, 0)
				[
					SNew(SNumericEntryBox<float>)
					.Value_Lambda([this]() { return YAxisMax; })
					.OnValueCommitted_Lambda([this](float Value, ETextCommit::Type) { YAxisMax = Value; })
				]
			]
		];
}

TSharedRef<SWidget> SBlendSpaceConfigDialog::BuildAnimationSelectionSection()
{
	TSharedRef<SVerticalBox> AnimationList = SNew(SVerticalBox);

	if (Classifier)
	{
		// Show classified animations
		for (auto& Pair : Classifier->GetClassifiedResults())
		{
			FLocomotionRoleCandidates& Candidates = const_cast<FLocomotionRoleCandidates&>(Pair.Value);
			AnimationList->AddSlot()
				.AutoHeight()
				.Padding(4)
				[
					BuildRoleRow(Pair.Key, &Candidates)
				];
		}

		// Show statistics
		int32 Total = Classifier->GetTotalAnimationCount();
		int32 Classified = Classifier->GetClassifiedCount();
		int32 Unclassified = Classifier->GetUnclassifiedAnimations().Num();

		AnimationList->AddSlot()
			.AutoHeight()
			.Padding(4, 8)
			[
				SNew(STextBlock)
				.Text(FText::Format(
					LOCTEXT("AnimStats", "Found {0} animations: {1} classified, {2} unclassified"),
					FText::AsNumber(Total),
					FText::AsNumber(Classified),
					FText::AsNumber(Unclassified)))
			];
	}

	return SNew(SExpandableArea)
		.AreaTitle(LOCTEXT("AnimationSelection", "Animation Selection"))
		.InitiallyCollapsed(false)
		.BodyContent()
		[
			AnimationList
		];
}

TSharedRef<SWidget> SBlendSpaceConfigDialog::BuildRoleRow(ELocomotionRole Role, FLocomotionRoleCandidates* Candidates)
{
	FString RoleName = UBlendSpaceBuilderSettings::GetRoleDisplayName(Role);
	UAnimSequence* CurrentSelection = SelectedAnimations.FindRef(Role);

	TArray<TSharedPtr<FClassifiedAnimation>> CandidateItems;
	if (Candidates)
	{
		for (FClassifiedAnimation& Candidate : Candidates->Candidates)
		{
			CandidateItems.Add(MakeShared<FClassifiedAnimation>(Candidate));
		}
	}

	// Add candidate count to role name
	int32 CandidateCount = CandidateItems.Num();
	FString RoleNameWithCount = FString::Printf(TEXT("%s (%d)"), *RoleName, CandidateCount);

	TSharedPtr<FClassifiedAnimation> CurrentItem;
	if (CurrentSelection)
	{
		for (auto& Item : CandidateItems)
		{
			if (Item->Animation.Get() == CurrentSelection)
			{
				CurrentItem = Item;
				break;
			}
		}
	}

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(0.25f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromString(RoleNameWithCount))
		]
		+ SHorizontalBox::Slot()
		.FillWidth(0.75f)
		[
			SNew(SLocomotionAnimSelector)
			.Role(Role)
			.CandidateItems(CandidateItems)
			.InitialSelection(CurrentItem)
			.OnAnimationSelected_Lambda([this, Role](UAnimSequence* Anim)
			{
				OnAnimationSelected(Role, Anim);
			})
		];
}

TSharedRef<SWidget> SBlendSpaceConfigDialog::BuildOutputPathSection()
{
	return SNew(SExpandableArea)
		.AreaTitle(LOCTEXT("OutputConfig", "Output"))
		.InitiallyCollapsed(false)
		.BodyContent()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(0.25f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("OutputPath", "Path:"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.75f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(BasePath))
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(0.25f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("AssetName", "Name:"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.75f)
				[
					SNew(SEditableTextBox)
					.Text_Lambda([this]() { return FText::FromString(OutputAssetName); })
					.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type)
					{
						OutputAssetName = Text.ToString();
					})
				]
			]
		];
}

TSharedRef<SWidget> SBlendSpaceConfigDialog::BuildButtonSection()
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(4, 0)
		[
			SNew(SButton)
			.Text(LOCTEXT("Cancel", "Cancel"))
			.OnClicked(this, &SBlendSpaceConfigDialog::OnCancelClicked)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(4, 0)
		[
			SNew(SButton)
			.Text(LOCTEXT("Create", "Create BlendSpace"))
			.OnClicked(this, &SBlendSpaceConfigDialog::OnAcceptClicked)
		];
}

FReply SBlendSpaceConfigDialog::OnAnalyzeClicked()
{
	// Run analysis
	AnalyzedPositions = FBlendSpaceFactory::AnalyzeSamplePositions(
		SelectedAnimations,
		SelectedAnalysisType,
		DetectedLeftFootBone,
		DetectedRightFootBone);

	// Calculate axis range
	FBlendSpaceFactory::CalculateAxisRangeFromAnalysis(
		AnalyzedPositions,
		AnalyzedXMin, AnalyzedXMax, AnalyzedYMin, AnalyzedYMax);

	// Update axis fields with analyzed range
	XAxisMin = AnalyzedXMin;
	XAxisMax = AnalyzedXMax;
	YAxisMin = AnalyzedYMin;
	YAxisMax = AnalyzedYMax;

	bAnalysisPerformed = true;
	bUseAnalyzedPositions = true;

	return FReply::Handled();
}

void SBlendSpaceConfigDialog::OnAnalysisTypeChanged(EBlendSpaceAnalysisType NewType)
{
	SelectedAnalysisType = NewType;
	// Clear previous analysis when type changes
	bAnalysisPerformed = false;
	AnalyzedPositions.Empty();
}

FReply SBlendSpaceConfigDialog::OnAcceptClicked()
{
	bWasAccepted = true;
	if (ParentWindow.IsValid())
	{
		ParentWindow->RequestDestroyWindow();
	}
	return FReply::Handled();
}

FReply SBlendSpaceConfigDialog::OnCancelClicked()
{
	bWasAccepted = false;
	if (ParentWindow.IsValid())
	{
		ParentWindow->RequestDestroyWindow();
	}
	return FReply::Handled();
}

void SBlendSpaceConfigDialog::OnAnimationSelected(ELocomotionRole Role, UAnimSequence* SelectedAnim)
{
	if (SelectedAnim)
	{
		SelectedAnimations.Add(Role, SelectedAnim);
	}
	else
	{
		SelectedAnimations.Remove(Role);
	}

	// Clear analysis when animations change
	if (bAnalysisPerformed)
	{
		bAnalysisPerformed = false;
		AnalyzedPositions.Empty();
	}
}

FBlendSpaceBuildConfig SBlendSpaceConfigDialog::GetBuildConfig() const
{
	FBlendSpaceBuildConfig Config;
	Config.Skeleton = Skeleton;
	Config.XAxisMin = XAxisMin;
	Config.XAxisMax = XAxisMax;
	Config.YAxisMin = YAxisMin;
	Config.YAxisMax = YAxisMax;
	Config.XAxisName = UBlendSpaceBuilderSettings::Get()->XAxisName;
	Config.YAxisName = UBlendSpaceBuilderSettings::Get()->YAxisName;
	Config.PackagePath = BasePath;
	Config.AssetName = OutputAssetName;
	Config.SelectedAnimations = SelectedAnimations;
	Config.AnalysisType = SelectedAnalysisType;
	Config.LeftFootBoneName = DetectedLeftFootBone;
	Config.RightFootBoneName = DetectedRightFootBone;
	Config.bOpenInEditor = true;

	// Set analysis results
	Config.bApplyAnalysis = bAnalysisPerformed && bUseAnalyzedPositions;
	if (Config.bApplyAnalysis)
	{
		Config.PreAnalyzedPositions = AnalyzedPositions;
	}

	return Config;
}

EVisibility SBlendSpaceConfigDialog::GetFootBoneVisibility() const
{
	return (SelectedAnalysisType == EBlendSpaceAnalysisType::Locomotion)
		? EVisibility::Visible
		: EVisibility::Collapsed;
}

EVisibility SBlendSpaceConfigDialog::GetAnalysisResultsVisibility() const
{
	return bAnalysisPerformed ? EVisibility::Visible : EVisibility::Collapsed;
}

FText SBlendSpaceConfigDialog::GetFootBoneText() const
{
	FString LeftBoneStr = DetectedLeftFootBone.IsNone() ? TEXT("Not Found") : DetectedLeftFootBone.ToString();
	FString RightBoneStr = DetectedRightFootBone.IsNone() ? TEXT("Not Found") : DetectedRightFootBone.ToString();

	return FText::Format(
		LOCTEXT("FootBones", "Detected Foot Bones: L={0}, R={1}"),
		FText::FromString(LeftBoneStr),
		FText::FromString(RightBoneStr));
}

FText SBlendSpaceConfigDialog::GetAnalysisResultsText() const
{
	if (!bAnalysisPerformed || AnalyzedPositions.Num() == 0)
	{
		return FText::GetEmpty();
	}

	FString ResultStr;
	for (const auto& Pair : SelectedAnimations)
	{
		UAnimSequence* Anim = Pair.Value;
		if (!Anim)
		{
			continue;
		}

		const FVector* PosPtr = AnalyzedPositions.Find(Anim);
		if (PosPtr)
		{
			FString AnimName = Anim->GetName();
			// Truncate long names
			if (AnimName.Len() > 20)
			{
				AnimName = AnimName.Left(17) + TEXT("...");
			}

			ResultStr += FString::Printf(TEXT("%s: (%.0f, %.0f)\n"),
				*AnimName, PosPtr->X, PosPtr->Y);
		}
	}

	return FText::FromString(ResultStr.TrimEnd());
}

FText SBlendSpaceConfigDialog::GetAxisRangeText() const
{
	return FText::Format(
		LOCTEXT("AxisRange", "Calculated Axis Range: X({0} ~ {1}), Y({2} ~ {3})"),
		FText::AsNumber(static_cast<int32>(AnalyzedXMin)),
		FText::AsNumber(static_cast<int32>(AnalyzedXMax)),
		FText::AsNumber(static_cast<int32>(AnalyzedYMin)),
		FText::AsNumber(static_cast<int32>(AnalyzedYMax)));
}

bool SBlendSpaceConfigDialog::HasSelectedAnimations() const
{
	return SelectedAnimations.Num() > 0;
}

#undef LOCTEXT_NAMESPACE
