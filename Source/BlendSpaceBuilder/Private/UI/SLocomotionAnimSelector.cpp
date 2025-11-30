#include "SLocomotionAnimSelector.h"
#include "LocomotionAnimClassifier.h"
#include "Animation/AnimSequence.h"
#include "Widgets/Input/SComboBox.h"

#define LOCTEXT_NAMESPACE "SLocomotionAnimSelector"

void SLocomotionAnimSelector::Construct(const FArguments& InArgs)
{
	Role = InArgs._Role;
	CandidateItems = InArgs._CandidateItems;
	CurrentSelection = InArgs._InitialSelection;
	OnAnimationSelectedDelegate = InArgs._OnAnimationSelected;

	// Add a "None" option
	TSharedPtr<FClassifiedAnimation> NoneOption = MakeShared<FClassifiedAnimation>();
	NoneOption->Animation = nullptr;
	CandidateItems.Insert(NoneOption, 0);

	if (!CurrentSelection.IsValid() && CandidateItems.Num() > 1)
	{
		// Default to first actual animation if available
		CurrentSelection = CandidateItems[1];
	}

	ChildSlot
	[
		SNew(SComboBox<TSharedPtr<FClassifiedAnimation>>)
		.OptionsSource(&CandidateItems)
		.InitiallySelectedItem(CurrentSelection)
		.OnGenerateWidget(this, &SLocomotionAnimSelector::GenerateComboBoxItem)
		.OnSelectionChanged(this, &SLocomotionAnimSelector::OnSelectionChanged)
		.Content()
		[
			SNew(STextBlock)
			.Text(this, &SLocomotionAnimSelector::GetCurrentSelectionText)
		]
	];
}

TSharedRef<SWidget> SLocomotionAnimSelector::GenerateComboBoxItem(TSharedPtr<FClassifiedAnimation> Item)
{
	FString DisplayText;
	FSlateColor TextColor = FSlateColor::UseForeground();

	if (!Item.IsValid() || !Item->Animation.IsValid())
	{
		DisplayText = TEXT("(None)");
		TextColor = FSlateColor::UseSubduedForeground();
	}
	else
	{
		DisplayText = Item->GetDisplayName();
		if (Item->bHasRootMotion)
		{
			TextColor = FLinearColor::Green;
		}
	}

	return SNew(STextBlock)
		.Text(FText::FromString(DisplayText))
		.ColorAndOpacity(TextColor);
}

void SLocomotionAnimSelector::OnSelectionChanged(TSharedPtr<FClassifiedAnimation> Item, ESelectInfo::Type SelectInfo)
{
	CurrentSelection = Item;

	UAnimSequence* SelectedAnim = nullptr;
	if (Item.IsValid() && Item->Animation.IsValid())
	{
		SelectedAnim = Item->Animation.Get();
	}

	OnAnimationSelectedDelegate.ExecuteIfBound(SelectedAnim);
}

FText SLocomotionAnimSelector::GetCurrentSelectionText() const
{
	if (!CurrentSelection.IsValid() || !CurrentSelection->Animation.IsValid())
	{
		return LOCTEXT("NoneSelected", "(None)");
	}

	return FText::FromString(CurrentSelection->GetDisplayName());
}

#undef LOCTEXT_NAMESPACE
