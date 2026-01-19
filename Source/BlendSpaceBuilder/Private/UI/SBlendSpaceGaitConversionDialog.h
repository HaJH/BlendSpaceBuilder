#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "BlendSpaceGaitConverter.h"

class UBlendSpace;

/**
 * Dialog for converting Speed-based BlendSpace to Gait-based format.
 * Shows preview of sample role inference and conversion results.
 */
class SBlendSpaceGaitConversionDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlendSpaceGaitConversionDialog) {}
		SLATE_ARGUMENT(UBlendSpace*, BlendSpace)
		SLATE_ARGUMENT(TSharedPtr<SWindow>, ParentWindow)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	bool WasAccepted() const { return bWasAccepted; }
	UBlendSpace* GetConvertedBlendSpace() const { return ConvertedBlendSpace; }

private:
	TSharedRef<SWidget> BuildSourceInfoSection();
	TSharedRef<SWidget> BuildSampleAnalysisSection();
	TSharedRef<SWidget> BuildAnalyzedSpeedsSection();
	TSharedRef<SWidget> BuildOptionsSection();
	TSharedRef<SWidget> BuildButtonSection();

	FReply OnAnalyzeClicked();
	FReply OnConvertClicked();
	FReply OnCancelClicked();

	void OnCreateCopyChanged(ECheckBoxState NewState);
	void OnOpenInEditorChanged(ECheckBoxState NewState);
	void OnOutputSuffixChanged(const FText& NewText);

	FText GetSourceInfoText() const;
	FText GetSampleAnalysisText() const;
	FText GetAnalyzedSpeedsText() const;
	FText GetRecommendedThresholdsText() const;

	EVisibility GetAnalysisResultsVisibility() const;

	UBlendSpace* SourceBlendSpace = nullptr;
	TSharedPtr<SWindow> ParentWindow;

	FGaitConversionConfig Config;
	FGaitConversionResult AnalysisResult;
	bool bAnalysisPerformed = false;

	bool bWasAccepted = false;
	UBlendSpace* ConvertedBlendSpace = nullptr;

	// UI update
	TSharedPtr<STextBlock> SampleAnalysisTextBlock;
	TSharedPtr<STextBlock> AnalyzedSpeedsTextBlock;
	TSharedPtr<STextBlock> RecommendedThresholdsTextBlock;
};
