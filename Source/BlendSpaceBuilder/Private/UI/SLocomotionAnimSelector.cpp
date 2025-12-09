#include "SLocomotionAnimSelector.h"
#include "LocomotionAnimClassifier.h"
#include "Animation/AnimSequence.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SBoxPanel.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "PropertyCustomizationHelpers.h"
#include "Editor.h"

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
		// Notify parent of default selection so it gets added to SelectedAnimations
		if (CurrentSelection.IsValid() && CurrentSelection->Animation.IsValid())
		{
			OnAnimationSelectedDelegate.ExecuteIfBound(CurrentSelection->Animation.Get());
		}
	}

	ChildSlot
	[
		SNew(SHorizontalBox)

		// ComboBox
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SAssignNew(ComboBox, SComboBox<TSharedPtr<FClassifiedAnimation>>)
			.OptionsSource(&CandidateItems)
			.InitiallySelectedItem(CurrentSelection)
			.OnGenerateWidget(this, &SLocomotionAnimSelector::GenerateComboBoxItem)
			.OnSelectionChanged(this, &SLocomotionAnimSelector::OnSelectionChanged)
			.Content()
			[
				SNew(STextBlock)
				.Text(this, &SLocomotionAnimSelector::GetCurrentSelectionText)
			]
		]

		// Use Selected Asset button
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2, 0, 0, 0)
		.VAlign(VAlign_Center)
		[
			PropertyCustomizationHelpers::MakeUseSelectedButton(
				FSimpleDelegate::CreateSP(this, &SLocomotionAnimSelector::OnUseSelectedAsset),
				LOCTEXT("UseSelectedAssetTooltip", "Use the AnimSequence selected in Content Browser"))
		]

		// Browse to Asset button
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2, 0, 0, 0)
		.VAlign(VAlign_Center)
		[
			PropertyCustomizationHelpers::MakeBrowseButton(
				FSimpleDelegate::CreateSP(this, &SLocomotionAnimSelector::OnBrowseToAsset),
				LOCTEXT("BrowseToAssetTooltip", "Browse to this animation in Content Browser"))
		]
	];
}

TSharedRef<SWidget> SLocomotionAnimSelector::GenerateComboBoxItem(TSharedPtr<FClassifiedAnimation> Item)
{
	if (!Item.IsValid() || !Item->Animation.IsValid())
	{
		return SNew(STextBlock)
			.Text(LOCTEXT("None", "(None)"))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground());
	}

	// Build row with [RM] indicator for root motion animations
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0, 0, 4, 0)
		[
			SNew(STextBlock)
			.Text(Item->bHasRootMotion ? LOCTEXT("RM", "[RM]") : FText::GetEmpty())
			.ColorAndOpacity(FLinearColor::Green)
			.ToolTipText(LOCTEXT("RootMotionEnabled", "Root Motion Enabled"))
			.Visibility(Item->bHasRootMotion ? EVisibility::Visible : EVisibility::Hidden)
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Item->GetDisplayName()))
		];
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

	// Include [RM] indicator in selected text
	if (CurrentSelection->bHasRootMotion)
	{
		return FText::Format(LOCTEXT("SelectedWithRM", "[RM] {0}"),
			FText::FromString(CurrentSelection->GetDisplayName()));
	}

	return FText::FromString(CurrentSelection->GetDisplayName());
}

void SLocomotionAnimSelector::OnUseSelectedAsset()
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	TArray<FAssetData> SelectedAssets;
	ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);

	for (const FAssetData& AssetData : SelectedAssets)
	{
		if (UAnimSequence* Anim = Cast<UAnimSequence>(AssetData.GetAsset()))
		{
			// Find this animation in CandidateItems
			for (const TSharedPtr<FClassifiedAnimation>& Item : CandidateItems)
			{
				if (Item.IsValid() && Item->Animation.Get() == Anim)
				{
					ComboBox->SetSelectedItem(Item);
					OnSelectionChanged(Item, ESelectInfo::Direct);
					return;
				}
			}
		}
	}
}

void SLocomotionAnimSelector::OnBrowseToAsset()
{
	if (CurrentSelection.IsValid() && CurrentSelection->Animation.IsValid())
	{
		TArray<UObject*> Objects;
		Objects.Add(CurrentSelection->Animation.Get());
		GEditor->SyncBrowserToObjects(Objects);
	}
}

#undef LOCTEXT_NAMESPACE
