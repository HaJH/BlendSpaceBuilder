#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

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

	void ExecuteGenerateLocomotionBlendSpace(TArray<FAssetData> SelectedAssets);

	FDelegateHandle ContentBrowserExtenderDelegateHandle;
};
