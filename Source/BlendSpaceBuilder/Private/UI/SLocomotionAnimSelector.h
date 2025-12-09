#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "BlendSpaceBuilderSettings.h"

class UAnimSequence;
struct FClassifiedAnimation;

DECLARE_DELEGATE_OneParam(FOnAnimationSelectedDelegate, UAnimSequence*);

class SLocomotionAnimSelector : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLocomotionAnimSelector) {}
		SLATE_ARGUMENT(ELocomotionRole, Role)
		SLATE_ARGUMENT(TArray<TSharedPtr<FClassifiedAnimation>>, CandidateItems)
		SLATE_ARGUMENT(TSharedPtr<FClassifiedAnimation>, InitialSelection)
		SLATE_EVENT(FOnAnimationSelectedDelegate, OnAnimationSelected)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> GenerateComboBoxItem(TSharedPtr<FClassifiedAnimation> Item);
	void OnSelectionChanged(TSharedPtr<FClassifiedAnimation> Item, ESelectInfo::Type SelectInfo);
	FText GetCurrentSelectionText() const;

	void OnUseSelectedAsset();
	void OnBrowseToAsset();

	ELocomotionRole Role;
	TArray<TSharedPtr<FClassifiedAnimation>> CandidateItems;
	TSharedPtr<FClassifiedAnimation> CurrentSelection;
	FOnAnimationSelectedDelegate OnAnimationSelectedDelegate;

	TSharedPtr<SComboBox<TSharedPtr<FClassifiedAnimation>>> ComboBox;
};
