#include "SBlueprintImplementInspectWidget.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ContentBrowserModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "IContentBrowserSingleton.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_Knot.h"
#include "K2Node_Tunnel.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SBlueprintImplementInspectWidget"

namespace BlueprintImplementInspect
{
	static const FName ColumnBlueprint(TEXT("Blueprint"));
	static const FName ColumnParentClass(TEXT("ParentClass"));
	static const FName ColumnTotalNodes(TEXT("TotalNodes"));
	static const FName ColumnLogicNodes(TEXT("LogicNodes"));
	static const FName ColumnLogicState(TEXT("LogicState"));
	static const FName ColumnAssetPath(TEXT("AssetPath"));
}

class SBlueprintInspectRow : public SMultiColumnTableRow<TSharedPtr<FBlueprintInspectItem>>
{
public:
	SLATE_BEGIN_ARGS(SBlueprintInspectRow) {}
		SLATE_ARGUMENT(TSharedPtr<FBlueprintInspectItem>, Item)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTableView)
	{
		Item = InArgs._Item;
		SMultiColumnTableRow<TSharedPtr<FBlueprintInspectItem>>::Construct(
			FSuperRowType::FArguments().Padding(2.0f),
			OwnerTableView);
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		if (!Item.IsValid())
		{
			return SNew(STextBlock).Text(LOCTEXT("InvalidItem", "Invalid"));
		}

		if (ColumnName == BlueprintImplementInspect::ColumnBlueprint)
		{
			return SNew(STextBlock).Text(FText::FromString(Item->BlueprintName));
		}

		if (ColumnName == BlueprintImplementInspect::ColumnParentClass)
		{
			return SNew(STextBlock).Text(FText::FromString(Item->ParentClassName));
		}

		if (ColumnName == BlueprintImplementInspect::ColumnTotalNodes)
		{
			return SNew(STextBlock).Text(FText::AsNumber(Item->TotalNodeCount));
		}

		if (ColumnName == BlueprintImplementInspect::ColumnLogicNodes)
		{
			return SNew(STextBlock).Text(FText::AsNumber(Item->LogicNodeCount));
		}

		if (ColumnName == BlueprintImplementInspect::ColumnLogicState)
		{
			const FText StateText = Item->LogicState == EBlueprintLogicState::HasLogic
				? LOCTEXT("HasLogic", "有逻辑")
				: LOCTEXT("NoLogic", "纯配置");
			return SNew(STextBlock).Text(StateText);
		}

		if (ColumnName == BlueprintImplementInspect::ColumnAssetPath)
		{
			return SNew(STextBlock).Text(FText::FromString(Item->AssetPath));
		}

		return SNew(STextBlock).Text(FText::GetEmpty());
	}

private:
	TSharedPtr<FBlueprintInspectItem> Item;
};

void SBlueprintImplementInspectWidget::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 8.0f, 8.0f, 4.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("PathLabel", "1) 选择扫描目录（弹出路径选择）"))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 0.0f, 8.0f, 8.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SAssignNew(PathPickerComboButton, SComboButton)
				.ButtonContent()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("PickPathButton", "选择目录"))
				]
				.MenuContent()
				[
					CreatePathPickerMenuContent()
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					return FText::FromString(SelectedContentPath);
				})
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 2.0f, 8.0f, 4.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("BaseClassLabel", "2) 选择蓝图基类"))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 0.0f, 8.0f, 8.0f)
		[
			SNew(SClassPropertyEntryBox)
			.MetaClass(UObject::StaticClass())
			.AllowAbstract(true)
			.IsBlueprintBaseOnly(false)
			.AllowNone(false)
			.SelectedClass(this, &SBlueprintImplementInspectWidget::GetSelectedBaseClass)
			.OnSetClass(this, &SBlueprintImplementInspectWidget::OnSetBaseClass)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 0.0f, 8.0f, 8.0f)
		[
			SNew(SButton)
			.Text_Lambda([this]()
			{
				return bScanning
					? LOCTEXT("StopScanButton", "取消扫描")
					: LOCTEXT("ScanButton", "3) 扫描蓝图");
			})
			.OnClicked(this, &SBlueprintImplementInspectWidget::OnScanButtonClicked)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 0.0f, 8.0f, 4.0f)
		[
			SAssignNew(ScanProgressBar, SProgressBar)
			.Percent_Lambda([this]()
			{
				return TotalAssetCount > 0 ? (float)ScanIndex / (float)TotalAssetCount : 0.0f;
			})
			.Visibility_Lambda([this]()
			{
				return bScanning ? EVisibility::Visible : EVisibility::Collapsed;
			})
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 0.0f, 8.0f, 8.0f)
		[
			SAssignNew(SummaryText, STextBlock)
			.Text(LOCTEXT("InitialSummary", "尚未扫描"))
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(8.0f, 0.0f, 8.0f, 8.0f)
		[
			SAssignNew(ResultListView, SListView<TSharedPtr<FBlueprintInspectItem>>)
			.ItemHeight(22.0f)
			.ListItemsSource(&ScanItems)
			.OnSelectionChanged(this, &SBlueprintImplementInspectWidget::OnResultSelectionChanged)
			.OnGenerateRow_Lambda([](TSharedPtr<FBlueprintInspectItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
			{
				return SNew(SBlueprintInspectRow, OwnerTable).Item(Item);
			})
			.HeaderRow(
				SNew(SHeaderRow)
				+ SHeaderRow::Column(BlueprintImplementInspect::ColumnBlueprint)
				.DefaultLabel(LOCTEXT("ColBlueprint", "蓝图"))
				.FillWidth(0.18f)
				+ SHeaderRow::Column(BlueprintImplementInspect::ColumnParentClass)
				.DefaultLabel(LOCTEXT("ColParentClass", "父类"))
				.FillWidth(0.18f)
				+ SHeaderRow::Column(BlueprintImplementInspect::ColumnTotalNodes)
				.DefaultLabel(LOCTEXT("ColTotalNodes", "总节点数"))
				.SortMode(this, &SBlueprintImplementInspectWidget::GetColumnSortMode, BlueprintImplementInspect::ColumnTotalNodes)
				.OnSort(this, &SBlueprintImplementInspectWidget::OnResultSortChanged)
				.FixedWidth(100.0f)
				+ SHeaderRow::Column(BlueprintImplementInspect::ColumnLogicNodes)
				.DefaultLabel(LOCTEXT("ColLogicNodes", "逻辑节点数"))
				.SortMode(this, &SBlueprintImplementInspectWidget::GetColumnSortMode, BlueprintImplementInspect::ColumnLogicNodes)
				.OnSort(this, &SBlueprintImplementInspectWidget::OnResultSortChanged)
				.FixedWidth(100.0f)
				+ SHeaderRow::Column(BlueprintImplementInspect::ColumnLogicState)
				.DefaultLabel(LOCTEXT("ColLogicState", "判定"))
				.FixedWidth(80.0f)
				+ SHeaderRow::Column(BlueprintImplementInspect::ColumnAssetPath)
				.DefaultLabel(LOCTEXT("ColAssetPath", "资产路径"))
				.FillWidth(0.56f)
			)
		]
	];
}

void SBlueprintImplementInspectWidget::OnContentPathSelected(const FString& InPath)
{
	SelectedContentPath = InPath;
	if (PathPickerComboButton.IsValid())
	{
		PathPickerComboButton->SetIsOpen(false);
	}
}

TSharedRef<SWidget> SBlueprintImplementInspectWidget::CreatePathPickerMenuContent()
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	FPathPickerConfig PathPickerConfig;
	PathPickerConfig.DefaultPath = SelectedContentPath;
	PathPickerConfig.bAllowContextMenu = false;
	PathPickerConfig.bFocusSearchBoxWhenOpened = true;
	PathPickerConfig.OnPathSelected = FOnPathSelected::CreateSP(this, &SBlueprintImplementInspectWidget::OnContentPathSelected);

	return SNew(SBox)
		.WidthOverride(420.0f)
		.HeightOverride(320.0f)
		[
			ContentBrowserModule.Get().CreatePathPicker(PathPickerConfig)
		];
}

const UClass* SBlueprintImplementInspectWidget::GetSelectedBaseClass() const
{
	return SelectedBaseClass;
}

void SBlueprintImplementInspectWidget::OnSetBaseClass(const UClass* InClass)
{
	SelectedBaseClass = const_cast<UClass*>(InClass ? InClass : UObject::StaticClass());
}

void SBlueprintImplementInspectWidget::OnResultSelectionChanged(TSharedPtr<FBlueprintInspectItem> InItem, ESelectInfo::Type SelectInfo)
{
	if (!InItem.IsValid() || InItem->AssetPath.IsEmpty())
	{
		return;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	const FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(InItem->AssetPath));
	if (!AssetData.IsValid())
	{
		return;
	}

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	TArray<FAssetData> AssetsToSync;
	AssetsToSync.Add(AssetData);
	ContentBrowserModule.Get().SyncBrowserToAssets(AssetsToSync);
}

void SBlueprintImplementInspectWidget::OnResultSortChanged(EColumnSortPriority::Type Priority, const FName& ColumnId, EColumnSortMode::Type NewSortMode)
{
	SortColumn = ColumnId;
	SortMode = NewSortMode;
	SortResultItems();

	if (ResultListView.IsValid())
	{
		ResultListView->RequestListRefresh();
	}
}

EColumnSortMode::Type SBlueprintImplementInspectWidget::GetColumnSortMode(const FName ColumnId) const
{
	return SortColumn == ColumnId ? SortMode : EColumnSortMode::None;
}

FReply SBlueprintImplementInspectWidget::OnScanButtonClicked()
{
	if (bScanning)
	{
		StopScan();
	}
	else
	{
		StartScan();
	}
	return FReply::Handled();
}

void SBlueprintImplementInspectWidget::StartScan()
{
	ScanItems.Reset();
	PendingAssets.Reset();
	ScanIndex = 0;
	TotalAssetCount = 0;
	bScanning = true;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Add(FName(*SelectedContentPath));
	AssetRegistryModule.Get().GetAssets(Filter, PendingAssets);

	TotalAssetCount = PendingAssets.Num();

	if (SummaryText.IsValid())
	{
		SummaryText->SetText(FText::Format(
			LOCTEXT("ScanningFormat", "正在扫描，共 {0} 个资产..."),
			FText::AsNumber(TotalAssetCount)));
	}

	if (TotalAssetCount == 0)
	{
		StopScan();
		return;
	}

	RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SBlueprintImplementInspectWidget::TickScan));
}

void SBlueprintImplementInspectWidget::StopScan()
{
	bScanning = false;
	PendingAssets.Reset();
	SortResultItems();
	if (ResultListView.IsValid())
	{
		ResultListView->RequestListRefresh();
	}
	RefreshSummaryText();
}

EActiveTimerReturnType SBlueprintImplementInspectWidget::TickScan(double InCurrentTime, float InDeltaTime)
{
	if (!bScanning)
	{
		return EActiveTimerReturnType::Stop;
	}

	const int32 BatchSize = 10;
	const int32 BatchEnd = FMath::Min(ScanIndex + BatchSize, TotalAssetCount);

	for (; ScanIndex < BatchEnd; ++ScanIndex)
	{
		const FAssetData& AssetData = PendingAssets[ScanIndex];

		UBlueprint* Blueprint = Cast<UBlueprint>(AssetData.GetAsset());
		if (Blueprint == nullptr)
		{
			continue;
		}

		UClass* ClassForFilter = Blueprint->GeneratedClass != nullptr ? Blueprint->GeneratedClass : Blueprint->ParentClass;
		if (SelectedBaseClass != nullptr && (ClassForFilter == nullptr || !ClassForFilter->IsChildOf(SelectedBaseClass)))
		{
			continue;
		}

		TSharedPtr<FBlueprintInspectItem> Item = MakeShared<FBlueprintInspectItem>();
		Item->BlueprintName = Blueprint->GetName();
		Item->AssetPath = AssetData.GetObjectPathString();
		if (Blueprint->ParentClass != nullptr)
		{
			Item->ParentClassName = Blueprint->ParentClass->GetName();
		}
		Item->TotalNodeCount = CountAllNodes(Blueprint);
		Item->LogicNodeCount = CountLogicNodes(Blueprint);
		Item->LogicState = Item->LogicNodeCount > 0 ? EBlueprintLogicState::HasLogic : EBlueprintLogicState::NoLogic;
		ScanItems.Add(Item);
	}

	if (ResultListView.IsValid())
	{
		ResultListView->RequestListRefresh();
	}

	if (ScanIndex >= TotalAssetCount)
	{
		StopScan();
		return EActiveTimerReturnType::Stop;
	}

	return EActiveTimerReturnType::Continue;
}

void SBlueprintImplementInspectWidget::SortResultItems()
{
	const bool bAscending = SortMode == EColumnSortMode::Ascending;

	ScanItems.Sort([this, bAscending](const TSharedPtr<FBlueprintInspectItem>& A, const TSharedPtr<FBlueprintInspectItem>& B)
	{
		if (!A.IsValid() || !B.IsValid())
		{
			return A.IsValid();
		}

		auto CompareInt = [bAscending](int32 Lhs, int32 Rhs)
		{
			return bAscending ? (Lhs < Rhs) : (Lhs > Rhs);
		};

		if (SortColumn == BlueprintImplementInspect::ColumnTotalNodes)
		{
			if (A->TotalNodeCount != B->TotalNodeCount)
			{
				return CompareInt(A->TotalNodeCount, B->TotalNodeCount);
			}
		}
		else
		{
			if (A->LogicNodeCount != B->LogicNodeCount)
			{
				return CompareInt(A->LogicNodeCount, B->LogicNodeCount);
			}
		}

		return A->BlueprintName < B->BlueprintName;
	});
}

int32 SBlueprintImplementInspectWidget::CountAllNodes(UBlueprint* Blueprint) const
{
	int32 NodeCount = 0;
	if (Blueprint == nullptr)
	{
		return NodeCount;
	}

	auto CountGraphNodes = [&NodeCount](const UEdGraph* Graph)
	{
		if (Graph == nullptr)
		{
			return;
		}
		for (const UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node != nullptr)
			{
				++NodeCount;
			}
		}
	};

	for (const UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		CountGraphNodes(Graph);
	}
	for (const UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		CountGraphNodes(Graph);
	}
	for (const UEdGraph* Graph : Blueprint->MacroGraphs)
	{
		CountGraphNodes(Graph);
	}

	return NodeCount;
}

int32 SBlueprintImplementInspectWidget::CountLogicNodes(UBlueprint* Blueprint) const
{
	int32 NodeCount = 0;
	if (Blueprint == nullptr)
	{
		return NodeCount;
	}

	auto CountGraphNodes = [this, &NodeCount](const UEdGraph* Graph)
	{
		if (Graph == nullptr)
		{
			return;
		}
		for (const UEdGraphNode* Node : Graph->Nodes)
		{
			if (IsLogicNode(Node))
			{
				++NodeCount;
			}
		}
	};

	for (const UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		CountGraphNodes(Graph);
	}
	for (const UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		CountGraphNodes(Graph);
	}
	for (const UEdGraph* Graph : Blueprint->MacroGraphs)
	{
		CountGraphNodes(Graph);
	}

	return NodeCount;
}

bool SBlueprintImplementInspectWidget::IsLogicNode(const UEdGraphNode* Node) const
{
	if (Node == nullptr)
	{
		return false;
	}

	if (Node->IsA<UK2Node_FunctionEntry>() || Node->IsA<UK2Node_FunctionResult>() || Node->IsA<UK2Node_Knot>() || Node->IsA<UK2Node_Tunnel>())
	{
		return false;
	}

	for (const UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin == nullptr)
		{
			continue;
		}
		if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec && Pin->LinkedTo.Num() > 0)
		{
			return true;
		}
	}

	return false;
}

void SBlueprintImplementInspectWidget::RefreshSummaryText()
{
	int32 HasLogicCount = 0;
	for (const TSharedPtr<FBlueprintInspectItem>& Item : ScanItems)
	{
		if (Item.IsValid() && Item->LogicState == EBlueprintLogicState::HasLogic)
		{
			++HasLogicCount;
		}
	}

	if (SummaryText.IsValid())
	{
		SummaryText->SetText(FText::Format(
			LOCTEXT("SummaryFormat", "共扫描 {0} 个蓝图，其中有逻辑实现 {1} 个，纯配置 {2} 个。"),
			FText::AsNumber(ScanItems.Num()),
			FText::AsNumber(HasLogicCount),
			FText::AsNumber(ScanItems.Num() - HasLogicCount)));
	}
}

#undef LOCTEXT_NAMESPACE
