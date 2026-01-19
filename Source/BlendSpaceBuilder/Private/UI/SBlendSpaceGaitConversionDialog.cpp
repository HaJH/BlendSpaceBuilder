#include "UI/SBlendSpaceGaitConversionDialog.h"
#include "BlendSpaceBuilderSettings.h"

#include "Animation/BlendSpace.h"
#include "Animation/AnimSequence.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "SBlendSpaceGaitConversionDialog"

void SBlendSpaceGaitConversionDialog::Construct(const FArguments& InArgs)
{
	SourceBlendSpace = InArgs._BlendSpace;
	ParentWindow = InArgs._ParentWindow;

	// Default config - in-place conversion by default
	Config.bCreateCopy = false;
	Config.bOpenInEditor = true;
	Config.OutputSuffix = TEXT("_Gait");

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(10.f)
		[
			SNew(SVerticalBox)

			// Source Info Section
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 10.f)
			[
				BuildSourceInfoSection()
			]

			// Sample Analysis Section (scrollable)
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(0.f, 0.f, 0.f, 10.f)
			[
				BuildSampleAnalysisSection()
			]

			// Analyzed Speeds Section
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 10.f)
			[
				BuildAnalyzedSpeedsSection()
			]

			// Options Section
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 10.f)
			[
				BuildOptionsSection()
			]

			// Button Section
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				BuildButtonSection()
			]
		]
	];

	// Perform initial analysis
	OnAnalyzeClicked();
}

TSharedRef<SWidget> SBlendSpaceGaitConversionDialog::BuildSourceInfoSection()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("SourceInfoHeader", "Source BlendSpace"))
			.Font(FAppStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10.f, 5.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text(this, &SBlendSpaceGaitConversionDialog::GetSourceInfoText)
		];
}

TSharedRef<SWidget> SBlendSpaceGaitConversionDialog::BuildSampleAnalysisSection()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("SampleAnalysisHeader", "Sample Analysis"))
			.Font(FAppStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(10.f, 5.f, 0.f, 0.f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
			.Padding(5.f)
			.Visibility(this, &SBlendSpaceGaitConversionDialog::GetAnalysisResultsVisibility)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SAssignNew(SampleAnalysisTextBlock, STextBlock)
					.Text(this, &SBlendSpaceGaitConversionDialog::GetSampleAnalysisText)
				]
			]
		];
}

TSharedRef<SWidget> SBlendSpaceGaitConversionDialog::BuildAnalyzedSpeedsSection()
{
	return SNew(SVerticalBox)
		.Visibility(this, &SBlendSpaceGaitConversionDialog::GetAnalysisResultsVisibility)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("AnalyzedSpeedsHeader", "Analyzed Speeds"))
			.Font(FAppStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10.f, 5.f, 0.f, 0.f)
		[
			SAssignNew(AnalyzedSpeedsTextBlock, STextBlock)
			.Text(this, &SBlendSpaceGaitConversionDialog::GetAnalyzedSpeedsText)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 10.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("RecommendedThresholdsHeader", "Recommended Thresholds"))
			.Font(FAppStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10.f, 5.f, 0.f, 0.f)
		[
			SAssignNew(RecommendedThresholdsTextBlock, STextBlock)
			.Text(this, &SBlendSpaceGaitConversionDialog::GetRecommendedThresholdsText)
		];
}

TSharedRef<SWidget> SBlendSpaceGaitConversionDialog::BuildOptionsSection()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("OptionsHeader", "Options"))
			.Font(FAppStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10.f, 5.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SCheckBox)
				.IsChecked(Config.bCreateCopy ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
				.OnCheckStateChanged(this, &SBlendSpaceGaitConversionDialog::OnCreateCopyChanged)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CreateCopyLabel", "Create copy (suffix:"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5.f, 0.f, 0.f, 0.f)
			[
				SNew(SBox)
				.WidthOverride(80.f)
				[
					SNew(SEditableTextBox)
					.Text(FText::FromString(Config.OutputSuffix))
					.OnTextChanged(this, &SBlendSpaceGaitConversionDialog::OnOutputSuffixChanged)
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CreateCopySuffixEnd", ")"))
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10.f, 5.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SCheckBox)
				.IsChecked(Config.bOpenInEditor ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
				.OnCheckStateChanged(this, &SBlendSpaceGaitConversionDialog::OnOpenInEditorChanged)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("OpenInEditorLabel", "Open in editor after conversion"))
			]
		];
}

TSharedRef<SWidget> SBlendSpaceGaitConversionDialog::BuildButtonSection()
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(5.f, 0.f)
		[
			SNew(SButton)
			.Text(LOCTEXT("AnalyzeButton", "Analyze"))
			.OnClicked(this, &SBlendSpaceGaitConversionDialog::OnAnalyzeClicked)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(5.f, 0.f)
		[
			SNew(SButton)
			.Text(LOCTEXT("ConvertButton", "Convert"))
			.OnClicked(this, &SBlendSpaceGaitConversionDialog::OnConvertClicked)
			.IsEnabled_Lambda([this]() { return bAnalysisPerformed && AnalysisResult.bSuccess; })
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(5.f, 0.f)
		[
			SNew(SButton)
			.Text(LOCTEXT("CancelButton", "Cancel"))
			.OnClicked(this, &SBlendSpaceGaitConversionDialog::OnCancelClicked)
		];
}

FReply SBlendSpaceGaitConversionDialog::OnAnalyzeClicked()
{
	if (SourceBlendSpace)
	{
		AnalysisResult = FBlendSpaceGaitConverter::AnalyzeBlendSpace(SourceBlendSpace, Config);
		bAnalysisPerformed = true;
	}
	return FReply::Handled();
}

FReply SBlendSpaceGaitConversionDialog::OnConvertClicked()
{
	if (SourceBlendSpace && bAnalysisPerformed && AnalysisResult.bSuccess)
	{
		FGaitConversionResult ConversionResult;
		ConvertedBlendSpace = FBlendSpaceGaitConverter::ConvertToGaitBased(SourceBlendSpace, Config, ConversionResult);

		if (ConvertedBlendSpace)
		{
			bWasAccepted = true;
			if (ParentWindow.IsValid())
			{
				ParentWindow->RequestDestroyWindow();
			}
		}
	}
	return FReply::Handled();
}

FReply SBlendSpaceGaitConversionDialog::OnCancelClicked()
{
	bWasAccepted = false;
	if (ParentWindow.IsValid())
	{
		ParentWindow->RequestDestroyWindow();
	}
	return FReply::Handled();
}

void SBlendSpaceGaitConversionDialog::OnCreateCopyChanged(ECheckBoxState NewState)
{
	Config.bCreateCopy = (NewState == ECheckBoxState::Checked);
}

void SBlendSpaceGaitConversionDialog::OnOpenInEditorChanged(ECheckBoxState NewState)
{
	Config.bOpenInEditor = (NewState == ECheckBoxState::Checked);
}

void SBlendSpaceGaitConversionDialog::OnOutputSuffixChanged(const FText& NewText)
{
	Config.OutputSuffix = NewText.ToString();
}

FText SBlendSpaceGaitConversionDialog::GetSourceInfoText() const
{
	if (!SourceBlendSpace)
	{
		return LOCTEXT("NoBlendSpace", "No BlendSpace selected");
	}

	const FBlendParameter& XParam = SourceBlendSpace->GetBlendParameter(0);
	const FBlendParameter& YParam = SourceBlendSpace->GetBlendParameter(1);

	return FText::Format(
		LOCTEXT("SourceInfoFormat", "Name: {0}\nCurrent: Speed-Based (X: {1} ~ {2}, Y: {3} ~ {4})\nSamples: {5}"),
		FText::FromString(SourceBlendSpace->GetName()),
		FText::AsNumber(XParam.Min),
		FText::AsNumber(XParam.Max),
		FText::AsNumber(YParam.Min),
		FText::AsNumber(YParam.Max),
		FText::AsNumber(SourceBlendSpace->GetBlendSamples().Num()));
}

FText SBlendSpaceGaitConversionDialog::GetSampleAnalysisText() const
{
	if (!bAnalysisPerformed)
	{
		return LOCTEXT("ClickAnalyze", "Click Analyze to preview conversion");
	}

	if (!AnalysisResult.bSuccess)
	{
		return FText::Format(LOCTEXT("AnalysisError", "Error: {0}"), FText::FromString(AnalysisResult.ErrorMessage));
	}

	FString ResultText;
	for (const auto& Pair : AnalysisResult.OriginalSpeedPositions)
	{
		UAnimSequence* Anim = Pair.Key;
		if (!Anim)
		{
			continue;
		}

		FVector2D OriginalPos = Pair.Value;
		ELocomotionRole Role = AnalysisResult.InferredRoles.FindRef(Anim);
		FVector2D NewPos = AnalysisResult.NewGaitPositions.FindRef(Anim);

		FString RoleName = UBlendSpaceBuilderSettings::GetRoleDisplayName(Role);

		ResultText += FString::Printf(
			TEXT("%s\n  (%.0f, %.0f) -> %s (%.0f, %.0f)\n"),
			*Anim->GetName(),
			OriginalPos.X, OriginalPos.Y,
			*RoleName,
			NewPos.X, NewPos.Y);
	}

	return FText::FromString(ResultText);
}

FText SBlendSpaceGaitConversionDialog::GetAnalyzedSpeedsText() const
{
	if (!bAnalysisPerformed || !AnalysisResult.bSuccess)
	{
		return FText::GetEmpty();
	}

	return FText::Format(
		LOCTEXT("AnalyzedSpeedsFormat", "Walk: {0} cm/s | Run: {1} cm/s"),
		FText::AsNumber(FMath::RoundToInt(AnalysisResult.AnalyzedWalkSpeed)),
		FText::AsNumber(FMath::RoundToInt(AnalysisResult.AnalyzedRunSpeed)));
}

FText SBlendSpaceGaitConversionDialog::GetRecommendedThresholdsText() const
{
	if (!bAnalysisPerformed || !AnalysisResult.bSuccess)
	{
		return FText::GetEmpty();
	}

	// Calculate recommended thresholds based on analyzed speeds
	float IdleToWalk = AnalysisResult.AnalyzedWalkSpeed * 0.1f;
	float WalkToRun = (AnalysisResult.AnalyzedWalkSpeed + AnalysisResult.AnalyzedRunSpeed) * 0.5f;

	return FText::Format(
		LOCTEXT("RecommendedThresholdsFormat", "IdleToWalk: {0} | WalkToRun: {1}\nWalkAnimSpeed: {2} | RunAnimSpeed: {3}"),
		FText::AsNumber(FMath::RoundToInt(IdleToWalk)),
		FText::AsNumber(FMath::RoundToInt(WalkToRun)),
		FText::AsNumber(FMath::RoundToInt(AnalysisResult.AnalyzedWalkSpeed)),
		FText::AsNumber(FMath::RoundToInt(AnalysisResult.AnalyzedRunSpeed)));
}

EVisibility SBlendSpaceGaitConversionDialog::GetAnalysisResultsVisibility() const
{
	return bAnalysisPerformed ? EVisibility::Visible : EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE
