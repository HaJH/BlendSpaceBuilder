#include "SLocomotionAnimSelector.h"
#include "LocomotionAnimClassifier.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "PropertyCustomizationHelpers.h"
#include "Editor.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "SLocomotionAnimSelector"

void SLocomotionAnimSelector::Construct(const FArguments& InArgs)
{
	Role = InArgs._Role;
	CandidateItems = InArgs._CandidateItems;
	CurrentSelection = InArgs._InitialSelection;
	OnAnimationSelectedDelegate = InArgs._OnAnimationSelected;
	TargetSkeleton = InArgs._TargetSkeleton;

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

		// Pick Asset button
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2, 0, 0, 0)
		.VAlign(VAlign_Center)
		[
			SNew(SButton)
			.Text(LOCTEXT("PickAsset", "Pick..."))
			.ToolTipText(LOCTEXT("PickAssetTooltip", "Open asset picker to manually select an animation"))
			.OnClicked(this, &SLocomotionAnimSelector::OnPickAsset)
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

	const bool bIsManual = (Item->MatchPriority == -1);

	// Build row with [Manual] and [RM] indicators
	return SNew(SHorizontalBox)
		// [Manual] indicator for manually selected animations
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0, 0, 4, 0)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Manual", "[Manual]"))
			.ColorAndOpacity(FLinearColor::Yellow)
			.ToolTipText(LOCTEXT("ManuallySelected", "Manually Selected Animation"))
			.Visibility(bIsManual ? EVisibility::Visible : EVisibility::Collapsed)
		]
		// [RM] indicator for root motion animations
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

	const bool bIsManual = (CurrentSelection->MatchPriority == -1);
	FString DisplayText;

	// Include [Manual] indicator for manually selected animations
	if (bIsManual)
	{
		DisplayText += TEXT("[Manual] ");
	}

	// Include [RM] indicator for root motion animations
	if (CurrentSelection->bHasRootMotion)
	{
		DisplayText += TEXT("[RM] ");
	}

	DisplayText += CurrentSelection->GetDisplayName();
	return FText::FromString(DisplayText);
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
			// Validate skeleton match
			if (!ValidateSkeletonMatch(Anim))
			{
				continue;
			}

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

			// Not found in candidates - add as manual selection
			TSharedPtr<FClassifiedAnimation> NewItem = CreateManualItem(Anim);
			CandidateItems.Add(NewItem);
			ComboBox->RefreshOptions();
			ComboBox->SetSelectedItem(NewItem);
			OnSelectionChanged(NewItem, ESelectInfo::Direct);
			return;
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

FReply SLocomotionAnimSelector::OnPickAsset()
{
	// Close existing picker if any
	ClosePickerWindow();

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	FAssetPickerConfig AssetPickerConfig;
	AssetPickerConfig.SelectionMode = ESelectionMode::Single;
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
	AssetPickerConfig.bFocusSearchBoxWhenOpened = true;
	AssetPickerConfig.bAllowNullSelection = false;
	AssetPickerConfig.bShowBottomToolbar = true;
	AssetPickerConfig.bAutohideSearchBar = false;
	AssetPickerConfig.bCanShowClasses = false;

	// Filter: AnimSequence only
	AssetPickerConfig.Filter.ClassPaths.Add(UAnimSequence::StaticClass()->GetClassPathName());

	// Filter by skeleton to show only compatible animations
	AssetPickerConfig.OnShouldFilterAsset = FOnShouldFilterAsset::CreateLambda(
		[this](const FAssetData& AssetData) -> bool
		{
			if (!TargetSkeleton)
			{
				return false; // No target skeleton - show all
			}

			// Get skeleton path from asset metadata
			FAssetDataTagMapSharedView::FFindTagResult SkeletonTag =
				AssetData.TagsAndValues.FindTag(GET_MEMBER_NAME_CHECKED(UAnimSequence, Skeleton));

			if (!SkeletonTag.IsSet())
			{
				return true; // No skeleton info - filter out
			}

			// Compare skeleton paths
			FSoftObjectPath TargetSkeletonPath(TargetSkeleton);
			return SkeletonTag.GetValue() != TargetSkeletonPath.ToString();
		});

	AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateSP(this, &SLocomotionAnimSelector::OnManualAssetPicked);

	// Create asset picker widget
	TSharedRef<SWidget> AssetPickerWidget = ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig);

	// Create non-modal window (allows interaction with other windows)
	TSharedRef<SWindow> PickerWindow = SNew(SWindow)
		.Title(LOCTEXT("PickAnimation", "Pick Animation"))
		.ClientSize(FVector2D(800, 600))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.IsTopmostWindow(false)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				AssetPickerWidget
			]
		];

	// Store reference for later closing
	PickerWindowPtr = PickerWindow;

	// Add as non-modal window
	FSlateApplication::Get().AddWindow(PickerWindow);

	return FReply::Handled();
}

void SLocomotionAnimSelector::OnManualAssetPicked(const FAssetData& AssetData)
{
	UAnimSequence* PickedAnim = Cast<UAnimSequence>(AssetData.GetAsset());
	if (!PickedAnim)
	{
		return;
	}

	// Validate skeleton match
	if (!ValidateSkeletonMatch(PickedAnim))
	{
		FText ErrorTitle = LOCTEXT("SkeletonMismatch", "Skeleton Mismatch");
		FText ErrorMessage = LOCTEXT("SkeletonMismatchMsg",
			"Selected animation does not match the target skeleton.");
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage, ErrorTitle);
		return;
	}

	// Check if already in candidates
	for (const TSharedPtr<FClassifiedAnimation>& Item : CandidateItems)
	{
		if (Item.IsValid() && Item->Animation.Get() == PickedAnim)
		{
			ComboBox->SetSelectedItem(Item);
			OnSelectionChanged(Item, ESelectInfo::Direct);
			ClosePickerWindow();
			return;
		}
	}

	// Create new manual item
	TSharedPtr<FClassifiedAnimation> NewItem = CreateManualItem(PickedAnim);
	CandidateItems.Add(NewItem);
	ComboBox->RefreshOptions();
	ComboBox->SetSelectedItem(NewItem);
	OnSelectionChanged(NewItem, ESelectInfo::Direct);
	ClosePickerWindow();
}

bool SLocomotionAnimSelector::ValidateSkeletonMatch(UAnimSequence* Anim) const
{
	if (!Anim || !TargetSkeleton)
	{
		return true; // Allow if no skeleton to validate against
	}

	return Anim->GetSkeleton() == TargetSkeleton;
}

TSharedPtr<FClassifiedAnimation> SLocomotionAnimSelector::CreateManualItem(UAnimSequence* Anim)
{
	TSharedPtr<FClassifiedAnimation> Item = MakeShared<FClassifiedAnimation>();
	Item->Animation = Anim;
	Item->Role = Role;
	Item->bHasRootMotion = Anim->bEnableRootMotion;
	Item->MatchPriority = -1; // Mark as manual selection
	return Item;
}

void SLocomotionAnimSelector::ClosePickerWindow()
{
	if (TSharedPtr<SWindow> PickerWindow = PickerWindowPtr.Pin())
	{
		FSlateApplication::Get().RequestDestroyWindow(PickerWindow.ToSharedRef());
		PickerWindowPtr.Reset();
	}
}

#undef LOCTEXT_NAMESPACE
