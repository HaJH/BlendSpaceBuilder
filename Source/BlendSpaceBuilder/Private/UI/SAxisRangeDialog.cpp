#include "SAxisRangeDialog.h"

#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SUniformGridPanel.h"

#define LOCTEXT_NAMESPACE "SAxisRangeDialog"

void SAxisRangeDialog::Construct(const FArguments& InArgs)
{
	ParentWindow = InArgs._ParentWindow;
	XMin = InArgs._InitialXMin;
	XMax = InArgs._InitialXMax;
	YMin = InArgs._InitialYMin;
	YMax = InArgs._InitialYMax;

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8)
		[
			SNew(SVerticalBox)
			// X Axis
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
					.Text(LOCTEXT("XAxis", "X Axis (Right):"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.35f)
				.Padding(4, 0)
				[
					SNew(SNumericEntryBox<float>)
					.Value_Lambda([this]() { return XMin; })
					.OnValueCommitted_Lambda([this](float Value, ETextCommit::Type) { XMin = Value; })
					.Label()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Min", "Min"))
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.35f)
				.Padding(4, 0)
				[
					SNew(SNumericEntryBox<float>)
					.Value_Lambda([this]() { return XMax; })
					.OnValueCommitted_Lambda([this](float Value, ETextCommit::Type) { XMax = Value; })
					.Label()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Max", "Max"))
					]
				]
			]
			// Y Axis
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
					.Text(LOCTEXT("YAxis", "Y Axis (Forward):"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.35f)
				.Padding(4, 0)
				[
					SNew(SNumericEntryBox<float>)
					.Value_Lambda([this]() { return YMin; })
					.OnValueCommitted_Lambda([this](float Value, ETextCommit::Type) { YMin = Value; })
					.Label()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Min", "Min"))
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.35f)
				.Padding(4, 0)
				[
					SNew(SNumericEntryBox<float>)
					.Value_Lambda([this]() { return YMax; })
					.OnValueCommitted_Lambda([this](float Value, ETextCommit::Type) { YMax = Value; })
					.Label()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Max", "Max"))
					]
				]
			]
		]
		// Buttons
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8)
		.HAlign(HAlign_Right)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("Cancel", "Cancel"))
				.OnClicked(this, &SAxisRangeDialog::OnCancelClicked)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("Apply", "Apply"))
				.OnClicked(this, &SAxisRangeDialog::OnAcceptClicked)
			]
		]
	];
}

FReply SAxisRangeDialog::OnAcceptClicked()
{
	bWasAccepted = true;
	if (ParentWindow.IsValid())
	{
		ParentWindow->RequestDestroyWindow();
	}
	return FReply::Handled();
}

FReply SAxisRangeDialog::OnCancelClicked()
{
	bWasAccepted = false;
	if (ParentWindow.IsValid())
	{
		ParentWindow->RequestDestroyWindow();
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
