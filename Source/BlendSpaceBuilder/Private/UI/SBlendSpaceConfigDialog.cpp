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
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SExpandableArea.h"

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
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4, 8, 4, 4)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([this]() { return bApplyAnalysis ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState) { bApplyAnalysis = (NewState == ECheckBoxState::Checked); })
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4, 0, 0, 0)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ApplyAnalysis", "Apply Animation Analysis"))
					.ToolTipText(LOCTEXT("ApplyAnalysisTooltip", "Automatically analyze root motion velocity and adjust sample positions after creation"))
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
	Config.bApplyAnalysis = bApplyAnalysis;
	return Config;
}

#undef LOCTEXT_NAMESPACE
