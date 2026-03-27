#pragma once

#include "AssetRegistry/AssetData.h"
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UClass;
class UEdGraphNode;

enum class EBlueprintLogicState : uint8
{
	NoLogic,
	HasLogic
};

struct FBlueprintInspectItem
{
	FString BlueprintName;
	FString ParentClassName;
	FString AssetPath;
	int32 TotalNodeCount = 0;
	int32 LogicNodeCount = 0;
	EBlueprintLogicState LogicState = EBlueprintLogicState::NoLogic;
};

class SBlueprintImplementInspectWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlueprintImplementInspectWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	void OnContentPathSelected(const FString& InPath);
	TSharedRef<class SWidget> CreatePathPickerMenuContent();
	const UClass* GetSelectedBaseClass() const;
	void OnSetBaseClass(const UClass* InClass);
	FReply OnScanButtonClicked();
	void OnResultSelectionChanged(TSharedPtr<FBlueprintInspectItem> InItem, ESelectInfo::Type SelectInfo);
	void OnResultSortChanged(EColumnSortPriority::Type Priority, const FName& ColumnId, EColumnSortMode::Type NewSortMode);
	EColumnSortMode::Type GetColumnSortMode(const FName ColumnId) const;
	void SortResultItems();

	void StartScan();
	void StopScan();
	EActiveTimerReturnType TickScan(double InCurrentTime, float InDeltaTime);
	int32 CountAllNodes(class UBlueprint* Blueprint) const;
	int32 CountLogicNodes(class UBlueprint* Blueprint) const;
	bool IsLogicNode(const UEdGraphNode* Node) const;
	void RefreshSummaryText();

private:
	FString SelectedContentPath = TEXT("/Game");
	UClass* SelectedBaseClass = UObject::StaticClass();

	TArray<TSharedPtr<FBlueprintInspectItem>> ScanItems;
	TSharedPtr<class SListView<TSharedPtr<FBlueprintInspectItem>>> ResultListView;
	TSharedPtr<class STextBlock> SummaryText;
	TSharedPtr<class SProgressBar> ScanProgressBar;
	TSharedPtr<class SComboButton> PathPickerComboButton;
	FName SortColumn = TEXT("LogicNodes");
	EColumnSortMode::Type SortMode = EColumnSortMode::Descending;

	bool bScanning = false;
	int32 TotalAssetCount = 0;
	int32 ScanIndex = 0;
	TArray<FAssetData> PendingAssets;
};
