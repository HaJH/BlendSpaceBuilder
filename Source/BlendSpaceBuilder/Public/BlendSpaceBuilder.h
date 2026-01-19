#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class UAnimSequence;
class UBlendSpace;

class FBlendSpaceBuilderModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterContentBrowserMenuExtension();
	void UnregisterContentBrowserMenuExtension();

	TSharedRef<FExtender> OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets);
	void CreateBlendSpaceContextMenu(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets);
	void CreateBlendSpaceUtilityMenu(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets);

	void ExecuteGenerateLocomotionBlendSpace(TArray<FAssetData> SelectedAssets);
	void ExecuteApplyModifierToAllSamples(TArray<FAssetData> SelectedAssets);
	void ExecuteOpenAllSamplesInEditor(TArray<FAssetData> SelectedAssets);
	void ExecuteAdjustAxisRange(TArray<FAssetData> SelectedAssets);
	void ExecuteConvertToGaitBased(TArray<FAssetData> SelectedAssets);

	// Helper functions
	TArray<UAnimSequence*> GetAnimationsFromBlendSpace(UBlendSpace* BlendSpace);
	UClass* ShowModifierClassPicker();

	FDelegateHandle ContentBrowserExtenderDelegateHandle;
};
