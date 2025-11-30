#include "BlendSpaceBuilder.h"

#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Editor.h"

// Animation Modifier
#include "AnimationModifier.h"
#include "AnimationModifiersAssetUserData.h"

// Class Picker
#include "Kismet2/SClassPickerDialog.h"
#include "ClassViewerModule.h"
#include "ClassViewerFilter.h"

// Notifications
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

// Animation Editor
#include "IAnimationEditorModule.h"

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

	bool bHasSkeletonOrMesh = false;
	bool bHasBlendSpace = false;

	for (const FAssetData& Asset : SelectedAssets)
	{
		if (Asset.AssetClassPath == USkeleton::StaticClass()->GetClassPathName() ||
			Asset.AssetClassPath == USkeletalMesh::StaticClass()->GetClassPathName())
		{
			bHasSkeletonOrMesh = true;
		}
		else if (Asset.AssetClassPath == UBlendSpace::StaticClass()->GetClassPathName())
		{
			bHasBlendSpace = true;
		}
	}

	if (bHasSkeletonOrMesh)
	{
		Extender->AddMenuExtension(
			"GetAssetActions",
			EExtensionHook::After,
			nullptr,
			FMenuExtensionDelegate::CreateRaw(this, &FBlendSpaceBuilderModule::CreateBlendSpaceContextMenu, SelectedAssets)
		);
	}

	if (bHasBlendSpace)
	{
		Extender->AddMenuExtension(
			"GetAssetActions",
			EExtensionHook::After,
			nullptr,
			FMenuExtensionDelegate::CreateRaw(this, &FBlendSpaceBuilderModule::CreateBlendSpaceUtilityMenu, SelectedAssets)
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

	FLocomotionAnimClassifier Classifier;
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

//=============================================================================
// BlendSpace Utility Menu
//=============================================================================

void FBlendSpaceBuilderModule::CreateBlendSpaceUtilityMenu(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("ApplyModifierToAllSamples", "Apply Modifier to All Samples"),
		LOCTEXT("ApplyModifierToAllSamplesTooltip", "Apply an animation modifier to all sample animations in this BlendSpace"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.AnimationModifier"),
		FUIAction(FExecuteAction::CreateRaw(this, &FBlendSpaceBuilderModule::ExecuteApplyModifierToAllSamples, SelectedAssets))
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("OpenAllSamplesInEditor", "Open All Samples in Editor"),
		LOCTEXT("OpenAllSamplesInEditorTooltip", "Open all sample animations in separate editor windows"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.AnimSequence"),
		FUIAction(FExecuteAction::CreateRaw(this, &FBlendSpaceBuilderModule::ExecuteOpenAllSamplesInEditor, SelectedAssets))
	);
}

//=============================================================================
// Animation Modifier Class Filter
//=============================================================================

class FAnimationModifierClassFilter : public IClassViewerFilter
{
public:
	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions,
		const UClass* InClass, TSharedRef<FClassViewerFilterFuncs> InFilterFuncs) override
	{
		return InClass->IsChildOf(UAnimationModifier::StaticClass()) &&
			!InClass->HasAnyClassFlags(CLASS_Abstract);
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions,
		const TSharedRef<const IUnloadedBlueprintData> InUnloadedClassData,
		TSharedRef<FClassViewerFilterFuncs> InFilterFuncs) override
	{
		return InUnloadedClassData->IsChildOf(UAnimationModifier::StaticClass());
	}
};

//=============================================================================
// Helper Functions
//=============================================================================

TArray<UAnimSequence*> FBlendSpaceBuilderModule::GetAnimationsFromBlendSpace(UBlendSpace* BlendSpace)
{
	TArray<UAnimSequence*> UniqueAnimations;

	if (!BlendSpace)
	{
		return UniqueAnimations;
	}

	const TArray<FBlendSample>& Samples = BlendSpace->GetBlendSamples();
	for (const FBlendSample& Sample : Samples)
	{
		UAnimSequence* Anim = Sample.Animation;
		if (Anim && !UniqueAnimations.Contains(Anim))
		{
			UniqueAnimations.Add(Anim);
		}
	}

	return UniqueAnimations;
}

UClass* FBlendSpaceBuilderModule::ShowModifierClassPicker()
{
	UClass* SelectedClass = nullptr;

	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;
	Options.DisplayMode = EClassViewerDisplayMode::TreeView;
	Options.bShowNoneOption = false;
	Options.bShowUnloadedBlueprints = true;

	TSharedRef<FAnimationModifierClassFilter> Filter = MakeShared<FAnimationModifierClassFilter>();
	Options.ClassFilters.Add(Filter);

	const FText Title = LOCTEXT("PickModifierClass", "Select Animation Modifier");

	if (SClassPickerDialog::PickClass(Title, Options, SelectedClass, UAnimationModifier::StaticClass()))
	{
		return SelectedClass;
	}

	return nullptr;
}

//=============================================================================
// Apply Modifier to All Samples
//=============================================================================

void FBlendSpaceBuilderModule::ExecuteApplyModifierToAllSamples(TArray<FAssetData> SelectedAssets)
{
	// Collect all BlendSpaces
	TArray<UBlendSpace*> BlendSpaces;
	for (const FAssetData& Asset : SelectedAssets)
	{
		if (Asset.AssetClassPath == UBlendSpace::StaticClass()->GetClassPathName())
		{
			if (UBlendSpace* BlendSpace = Cast<UBlendSpace>(Asset.GetAsset()))
			{
				BlendSpaces.Add(BlendSpace);
			}
		}
	}

	if (BlendSpaces.IsEmpty())
	{
		return;
	}

	// Show class picker
	UClass* ModifierClass = ShowModifierClassPicker();
	if (!ModifierClass)
	{
		return;
	}

	// Collect all unique animations from all BlendSpaces
	TArray<UAnimSequence*> AllAnimations;
	for (UBlendSpace* BlendSpace : BlendSpaces)
	{
		TArray<UAnimSequence*> Animations = GetAnimationsFromBlendSpace(BlendSpace);
		for (UAnimSequence* Anim : Animations)
		{
			if (!AllAnimations.Contains(Anim))
			{
				AllAnimations.Add(Anim);
			}
		}
	}

	if (AllAnimations.IsEmpty())
	{
		FNotificationInfo Info(LOCTEXT("NoAnimationsFound", "No animations found in BlendSpace"));
		Info.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
		return;
	}

	// Apply modifier to each animation
	int32 SuccessCount = 0;
	int32 FailCount = 0;

	{
		// Suppress warnings during batch apply
		UE::Anim::FApplyModifiersScope Scope(UE::Anim::FApplyModifiersScope::SuppressWarning);

		for (UAnimSequence* Anim : AllAnimations)
		{
			if (!Anim)
			{
				continue;
			}

			// Get or create AssetUserData
			UAnimationModifiersAssetUserData* AssetUserData = Anim->GetAssetUserData<UAnimationModifiersAssetUserData>();

			// Check if modifier of this class already exists and revert it
			if (AssetUserData)
			{
				const TArray<UAnimationModifier*>& ExistingModifiers = AssetUserData->GetAnimationModifierInstances();
				for (UAnimationModifier* ExistingModifier : ExistingModifiers)
				{
					if (ExistingModifier && ExistingModifier->GetClass() == ModifierClass)
					{
						ExistingModifier->RevertFromAnimationSequence(Anim);
					}
				}
			}

			// Add and apply new modifier
			bool bSuccess = UAnimationModifiersAssetUserData::AddAnimationModifierOfClass(Anim, TSubclassOf<UAnimationModifier>(ModifierClass));
			if (bSuccess)
			{
				// Get the newly added modifier and apply it
				AssetUserData = Anim->GetAssetUserData<UAnimationModifiersAssetUserData>();
				if (AssetUserData)
				{
					const TArray<UAnimationModifier*>& Modifiers = AssetUserData->GetAnimationModifierInstances();
					for (UAnimationModifier* Modifier : Modifiers)
					{
						if (Modifier && Modifier->GetClass() == ModifierClass)
						{
							Modifier->ApplyToAnimationSequence(Anim);
							break;
						}
					}
				}
				SuccessCount++;
			}
			else
			{
				FailCount++;
			}
		}
	}

	// Show notification
	FText Message = FText::Format(
		LOCTEXT("ModifierApplyResult", "Applied {0} to {1} animations ({2} failed)"),
		FText::FromString(ModifierClass->GetName()),
		FText::AsNumber(SuccessCount),
		FText::AsNumber(FailCount));

	FNotificationInfo Info(Message);
	Info.ExpireDuration = 5.0f;
	Info.bUseSuccessFailIcons = true;
	TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
	if (Notification.IsValid())
	{
		Notification->SetCompletionState(FailCount == 0 ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
	}
}

//=============================================================================
// Open All Samples in Editor
//=============================================================================

void FBlendSpaceBuilderModule::ExecuteOpenAllSamplesInEditor(TArray<FAssetData> SelectedAssets)
{
	// Collect all unique animations from selected BlendSpaces
	TArray<UAnimSequence*> AllAnimations;

	for (const FAssetData& Asset : SelectedAssets)
	{
		if (Asset.AssetClassPath == UBlendSpace::StaticClass()->GetClassPathName())
		{
			if (UBlendSpace* BlendSpace = Cast<UBlendSpace>(Asset.GetAsset()))
			{
				TArray<UAnimSequence*> Animations = GetAnimationsFromBlendSpace(BlendSpace);
				for (UAnimSequence* Anim : Animations)
				{
					if (!AllAnimations.Contains(Anim))
					{
						AllAnimations.Add(Anim);
					}
				}
			}
		}
	}

	if (AllAnimations.IsEmpty())
	{
		FNotificationInfo Info(LOCTEXT("NoAnimationsToOpen", "No animations found in BlendSpace"));
		Info.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
		return;
	}

	// Open each animation in a standalone editor window
	IAnimationEditorModule& AnimationEditorModule = FModuleManager::LoadModuleChecked<IAnimationEditorModule>("AnimationEditor");

	for (UAnimSequence* Anim : AllAnimations)
	{
		if (Anim)
		{
			// EToolkitMode::Standalone opens each asset in a separate window
			AnimationEditorModule.CreateAnimationEditor(EToolkitMode::Standalone, TSharedPtr<IToolkitHost>(), Anim);
		}
	}

	// Show notification
	FNotificationInfo Info(FText::Format(
		LOCTEXT("OpenedAnimations", "Opened {0} animations in separate windows"),
		FText::AsNumber(AllAnimations.Num())));
	Info.ExpireDuration = 3.0f;
	FSlateNotificationManager::Get().AddNotification(Info);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBlendSpaceBuilderModule, BlendSpaceBuilder)
