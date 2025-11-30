#include "BlendSpaceBuilder.h"

#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"

#include "BlendSpaceBuilderSettings.h"
#include "LocomotionAnimClassifier.h"
#include "BlendSpaceFactory.h"
#include "UI/SBlendSpaceConfigDialog.h"

#define LOCTEXT_NAMESPACE "FBlendSpaceBuilderModule"

void FBlendSpaceBuilderModule::StartupModule()
{
	RegisterContentBrowserMenuExtension();
}

void FBlendSpaceBuilderModule::ShutdownModule()
{
	UnregisterContentBrowserMenuExtension();
}

void FBlendSpaceBuilderModule::RegisterContentBrowserMenuExtension()
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	TArray<FContentBrowserMenuExtender_SelectedAssets>& Extenders = ContentBrowserModule.GetAllAssetViewContextMenuExtenders();
	Extenders.Add(FContentBrowserMenuExtender_SelectedAssets::CreateRaw(
		this, &FBlendSpaceBuilderModule::OnExtendContentBrowserAssetSelectionMenu));
	ContentBrowserExtenderDelegateHandle = Extenders.Last().GetHandle();
}

void FBlendSpaceBuilderModule::UnregisterContentBrowserMenuExtension()
{
	if (FModuleManager::Get().IsModuleLoaded("ContentBrowser"))
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>("ContentBrowser");
		TArray<FContentBrowserMenuExtender_SelectedAssets>& Extenders = ContentBrowserModule.GetAllAssetViewContextMenuExtenders();
		Extenders.RemoveAll([this](const FContentBrowserMenuExtender_SelectedAssets& Delegate)
		{
			return Delegate.GetHandle() == ContentBrowserExtenderDelegateHandle;
		});
	}
}

TSharedRef<FExtender> FBlendSpaceBuilderModule::OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets)
{
	TSharedRef<FExtender> Extender = MakeShared<FExtender>();

	bool bHasValidAsset = false;
	for (const FAssetData& Asset : SelectedAssets)
	{
		if (Asset.AssetClassPath == USkeleton::StaticClass()->GetClassPathName() ||
			Asset.AssetClassPath == USkeletalMesh::StaticClass()->GetClassPathName())
		{
			bHasValidAsset = true;
			break;
		}
	}

	if (bHasValidAsset)
	{
		Extender->AddMenuExtension(
			"GetAssetActions",
			EExtensionHook::After,
			nullptr,
			FMenuExtensionDelegate::CreateRaw(this, &FBlendSpaceBuilderModule::CreateBlendSpaceContextMenu, SelectedAssets)
		);
	}

	return Extender;
}

void FBlendSpaceBuilderModule::CreateBlendSpaceContextMenu(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("GenerateLocomotionBlendSpace", "Generate Locomotion BlendSpace"),
		LOCTEXT("GenerateLocomotionBlendSpaceTooltip", "Automatically generate a 2D locomotion blend space for this skeleton"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.BlendSpace"),
		FUIAction(FExecuteAction::CreateRaw(this, &FBlendSpaceBuilderModule::ExecuteGenerateLocomotionBlendSpace, SelectedAssets))
	);
}

void FBlendSpaceBuilderModule::ExecuteGenerateLocomotionBlendSpace(TArray<FAssetData> SelectedAssets)
{
	USkeleton* TargetSkeleton = nullptr;
	FString BasePath;

	for (const FAssetData& Asset : SelectedAssets)
	{
		if (Asset.AssetClassPath == USkeleton::StaticClass()->GetClassPathName())
		{
			TargetSkeleton = Cast<USkeleton>(Asset.GetAsset());
			BasePath = Asset.PackagePath.ToString();
			break;
		}
		else if (Asset.AssetClassPath == USkeletalMesh::StaticClass()->GetClassPathName())
		{
			if (USkeletalMesh* Mesh = Cast<USkeletalMesh>(Asset.GetAsset()))
			{
				TargetSkeleton = Mesh->GetSkeleton();
				BasePath = Asset.PackagePath.ToString();
				break;
			}
		}
	}

	if (!TargetSkeleton)
	{
		return;
	}

	UBlendSpaceBuilderSettings* Settings = UBlendSpaceBuilderSettings::Get();
	ULocomotionPatternDataAsset* PatternAsset = Settings->DefaultPatternAsset.LoadSynchronous();

	FLocomotionAnimClassifier Classifier(PatternAsset);
	Classifier.FindAnimationsForSkeleton(TargetSkeleton);
	Classifier.ClassifyAnimations();

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("BlendSpaceConfigTitle", "Configure Locomotion BlendSpace"))
		.ClientSize(FVector2D(800, 600))
		.SupportsMinimize(false)
		.SupportsMaximize(false);

	TSharedRef<SBlendSpaceConfigDialog> Dialog = SNew(SBlendSpaceConfigDialog)
		.Skeleton(TargetSkeleton)
		.Classifier(&Classifier)
		.BasePath(BasePath)
		.ParentWindow(Window);

	Window->SetContent(Dialog);

	GEditor->EditorAddModalWindow(Window);

	if (Dialog->WasAccepted())
	{
		FBlendSpaceBuildConfig Config = Dialog->GetBuildConfig();
		UBlendSpace* CreatedBlendSpace = FBlendSpaceFactory::CreateLocomotionBlendSpace(Config);

		if (CreatedBlendSpace)
		{
			TArray<UObject*> AssetsToSync;
			AssetsToSync.Add(CreatedBlendSpace);
			GEditor->SyncBrowserToObjects(AssetsToSync);
		}
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBlendSpaceBuilderModule, BlendSpaceBuilder)
