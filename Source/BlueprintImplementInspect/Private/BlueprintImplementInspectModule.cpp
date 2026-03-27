#include "BlueprintImplementInspectModule.h"

#include "LevelEditor.h"
#include "Styling/AppStyle.h"
#include "SBlueprintImplementInspectWidget.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "FBlueprintImplementInspectModule"

static const FName BlueprintImplementInspectTabName(TEXT("BlueprintImplementInspect"));

void FBlueprintImplementInspectModule::StartupModule()
{
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FBlueprintImplementInspectModule::RegisterMenus));

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(BlueprintImplementInspectTabName, FOnSpawnTab::CreateRaw(this, &FBlueprintImplementInspectModule::SpawnPluginTab))
		.SetDisplayName(LOCTEXT("TabTitle", "Blueprint Implement Inspect"))
		.SetTooltipText(LOCTEXT("TabTooltip", "Scan blueprints and inspect whether they contain blueprint logic."))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FBlueprintImplementInspectModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	if (FGlobalTabmanager::Get()->HasTabSpawner(BlueprintImplementInspectTabName))
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(BlueprintImplementInspectTabName);
	}
}

void FBlueprintImplementInspectModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
	if (ToolsMenu != nullptr)
	{
		FToolMenuSection& Section = ToolsMenu->FindOrAddSection(TEXT("BlueprintImplementInspectSection"));
		Section.AddMenuEntry(
			TEXT("OpenBlueprintImplementInspect"),
			LOCTEXT("OpenMenuLabel", "Blueprint Implement Inspect"),
			LOCTEXT("OpenMenuTooltip", "Scan blueprints under a content path and detect whether they have graph logic."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(this, &FBlueprintImplementInspectModule::OpenPluginWindow)));
	}

	UToolMenu* WindowMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Window"));
	if (WindowMenu != nullptr)
	{
		FToolMenuSection& Section = WindowMenu->FindOrAddSection(TEXT("BlueprintImplementInspectSection"));
		Section.AddMenuEntry(
			TEXT("OpenBlueprintImplementInspectInWindow"),
			LOCTEXT("OpenWindowMenuLabel", "Blueprint Implement Inspect"),
			LOCTEXT("OpenWindowMenuTooltip", "Open Blueprint Implement Inspect window."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(this, &FBlueprintImplementInspectModule::OpenPluginWindow)));
	}

	UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.LevelEditorToolBar.PlayToolBar"));
	if (ToolbarMenu != nullptr)
	{
		FToolMenuSection& Section = ToolbarMenu->FindOrAddSection(TEXT("BlueprintImplementInspect"));
		FToolMenuEntry Entry = FToolMenuEntry::InitToolBarButton(
			TEXT("OpenBlueprintImplementInspectToolbar"),
			FUIAction(FExecuteAction::CreateRaw(this, &FBlueprintImplementInspectModule::OpenPluginWindow)),
			LOCTEXT("ToolbarLabel", "BP检查"),
			LOCTEXT("ToolbarTooltip", "打开 Blueprint Implement Inspect"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));
		Section.AddEntry(Entry);
	}
}

void FBlueprintImplementInspectModule::OpenPluginWindow()
{
	FGlobalTabmanager::Get()->TryInvokeTab(BlueprintImplementInspectTabName);
}

TSharedRef<SDockTab> FBlueprintImplementInspectModule::SpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SBlueprintImplementInspectWidget)
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBlueprintImplementInspectModule, BlueprintImplementInspect)
