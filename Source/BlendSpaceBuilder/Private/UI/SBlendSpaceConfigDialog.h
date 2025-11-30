#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "BlendSpaceFactory.h"

class USkeleton;
class FLocomotionAnimClassifier;
class UAnimSequence;
struct FLocomotionRoleCandidates;
enum class ELocomotionRole : uint8;

class SBlendSpaceConfigDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlendSpaceConfigDialog) {}
		SLATE_ARGUMENT(USkeleton*, Skeleton)
		SLATE_ARGUMENT(FLocomotionAnimClassifier*, Classifier)
		SLATE_ARGUMENT(FString, BasePath)
		SLATE_ARGUMENT(TSharedPtr<SWindow>, ParentWindow)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	bool WasAccepted() const { return bWasAccepted; }
	FBlendSpaceBuildConfig GetBuildConfig() const;

private:
	TSharedRef<SWidget> BuildAxisConfigSection();
	TSharedRef<SWidget> BuildAnimationSelectionSection();
	TSharedRef<SWidget> BuildOutputPathSection();
	TSharedRef<SWidget> BuildButtonSection();

	TSharedRef<SWidget> BuildRoleRow(ELocomotionRole Role, FLocomotionRoleCandidates* Candidates);

	FReply OnAcceptClicked();
	FReply OnCancelClicked();

	void OnAnimationSelected(ELocomotionRole Role, UAnimSequence* SelectedAnim);

	USkeleton* Skeleton = nullptr;
	FLocomotionAnimClassifier* Classifier = nullptr;
	FString BasePath;
	TSharedPtr<SWindow> ParentWindow;

	float XAxisMin = -500.f;
	float XAxisMax = 500.f;
	float YAxisMin = -500.f;
	float YAxisMax = 500.f;
	FString OutputAssetName;

	TMap<ELocomotionRole, UAnimSequence*> SelectedAnimations;
	bool bWasAccepted = false;
};
