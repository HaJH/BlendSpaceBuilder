#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class SAxisRangeDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAxisRangeDialog) {}
		SLATE_ARGUMENT(float, InitialXMin)
		SLATE_ARGUMENT(float, InitialXMax)
		SLATE_ARGUMENT(float, InitialYMin)
		SLATE_ARGUMENT(float, InitialYMax)
		SLATE_ARGUMENT(TSharedPtr<SWindow>, ParentWindow)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	bool WasAccepted() const { return bWasAccepted; }

	float GetXMin() const { return XMin; }
	float GetXMax() const { return XMax; }
	float GetYMin() const { return YMin; }
	float GetYMax() const { return YMax; }

private:
	FReply OnAcceptClicked();
	FReply OnCancelClicked();

	TSharedPtr<SWindow> ParentWindow;

	float XMin = -500.f;
	float XMax = 500.f;
	float YMin = -500.f;
	float YMax = 500.f;

	bool bWasAccepted = false;
};
