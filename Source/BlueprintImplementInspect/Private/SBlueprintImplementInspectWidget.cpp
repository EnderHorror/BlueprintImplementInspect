#include "SBlueprintImplementInspectWidget.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ContentBrowserModule.h"
#include "DesktopPlatformModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphUtilities.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformApplicationMisc.h"
#include "IContentBrowserSingleton.h"
#include "IDesktopPlatform.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_Knot.h"
#include "K2Node_Tunnel.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "PropertyCustomizationHelpers.h"
#include "UObject/UnrealType.h"
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
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
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
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("CopySelectedBlueprintTextButton", "4) 复制选中蓝图文本"))
				.OnClicked(this, &SBlueprintImplementInspectWidget::OnCopySelectedBlueprintTextClicked)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("SaveSelectedBlueprintTextButton", "保存为TXT"))
				.OnClicked(this, &SBlueprintImplementInspectWidget::OnSaveSelectedBlueprintTextClicked)
			]
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
	SelectedItem = InItem;

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

FReply SBlueprintImplementInspectWidget::OnCopySelectedBlueprintTextClicked()
{
	UBlueprint* Blueprint = LoadBlueprintFromItem(SelectedItem);
	if (Blueprint == nullptr)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("CopyBlueprintTextNoBlueprint", "没有可导出的蓝图，请先在列表中选中一个蓝图。"));
		return FReply::Handled();
	}

	const FString ExportText = ExportBlueprintToText(Blueprint, SelectedItem->AssetPath);
	FPlatformApplicationMisc::ClipboardCopy(*ExportText);
	FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
		LOCTEXT("CopyBlueprintTextDone", "已复制蓝图文本到剪贴板。字符数：{0}"),
		FText::AsNumber(ExportText.Len())));
	return FReply::Handled();
}

FReply SBlueprintImplementInspectWidget::OnSaveSelectedBlueprintTextClicked()
{
	UBlueprint* Blueprint = LoadBlueprintFromItem(SelectedItem);
	if (Blueprint == nullptr)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("SaveBlueprintTextNoBlueprint", "没有可导出的蓝图，请先在列表中选中一个蓝图。"));
		return FReply::Handled();
	}

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform == nullptr)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("SaveBlueprintTextNoDesktopPlatform", "无法打开保存文件对话框。"));
		return FReply::Handled();
	}

	TArray<FString> SaveFilenames;
	const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
	const FString DefaultFileName = FString::Printf(TEXT("%s_BlueprintLLM.txt"), *Blueprint->GetName());
	const bool bSaved = DesktopPlatform->SaveFileDialog(
		ParentWindowHandle,
		TEXT("保存蓝图文本"),
		FPaths::ProjectSavedDir(),
		DefaultFileName,
		TEXT("Text files (*.txt)|*.txt|All files (*.*)|*.*"),
		EFileDialogFlags::None,
		SaveFilenames);

	if (!bSaved || SaveFilenames.Num() == 0)
	{
		return FReply::Handled();
	}

	const FString ExportText = ExportBlueprintToText(Blueprint, SelectedItem->AssetPath);
	if (!FFileHelper::SaveStringToFile(ExportText, *SaveFilenames[0], FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("SaveBlueprintTextFailed", "保存蓝图文本失败。"));
		return FReply::Handled();
	}

	FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
		LOCTEXT("SaveBlueprintTextDone", "已保存蓝图文本：{0}"),
		FText::FromString(SaveFilenames[0])));
	return FReply::Handled();
}

void SBlueprintImplementInspectWidget::StartScan()
{
	ScanItems.Reset();
	SelectedItem.Reset();
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

UBlueprint* SBlueprintImplementInspectWidget::LoadBlueprintFromItem(const TSharedPtr<FBlueprintInspectItem>& Item) const
{
	if (!Item.IsValid() || Item->AssetPath.IsEmpty())
	{
		return nullptr;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	const FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(Item->AssetPath));
	return AssetData.IsValid() ? Cast<UBlueprint>(AssetData.GetAsset()) : nullptr;
}

FString SBlueprintImplementInspectWidget::ExportBlueprintToText(UBlueprint* Blueprint, const FString& AssetPath) const
{
	if (Blueprint == nullptr)
	{
		return TEXT("# Blueprint LLM Export\nInvalid Blueprint\n");
	}

	FStringBuilderBase Builder;
	Builder.Append(TEXT("# Blueprint LLM Export\n"));
	Builder.Appendf(TEXT("Name: %s\n"), *Blueprint->GetName());
	Builder.Appendf(TEXT("AssetPath: %s\n"), *AssetPath);
	Builder.Appendf(TEXT("Package: %s\n"), *Blueprint->GetOutermost()->GetName());
	Builder.Appendf(TEXT("GeneratedClass: %s\n"), Blueprint->GeneratedClass ? *Blueprint->GeneratedClass->GetPathName() : TEXT("None"));
	Builder.Appendf(TEXT("ParentClass: %s\n"), Blueprint->ParentClass ? *Blueprint->ParentClass->GetPathName() : TEXT("None"));
	Builder.Append(TEXT("\n"));

	AppendBlueprintClassInfo(Builder, Blueprint);
	AppendBlueprintVariables(Builder, Blueprint);
	AppendClassDefaults(Builder, Blueprint);
	AppendBlueprintGraphs(Builder, Blueprint);

	return Builder.ToString();
}

void SBlueprintImplementInspectWidget::AppendBlueprintClassInfo(FStringBuilderBase& Builder, UBlueprint* Blueprint) const
{
	Builder.Append(TEXT("## Class\n"));
	Builder.Appendf(TEXT("BlueprintType: %s\n"), *UEnum::GetValueAsString(Blueprint->BlueprintType));
	Builder.Appendf(TEXT("BlueprintStatus: %s\n"), *UEnum::GetValueAsString(Blueprint->Status));
	Builder.Appendf(TEXT("ImplementedInterfaces: %d\n"), Blueprint->ImplementedInterfaces.Num());
	for (const FBPInterfaceDescription& InterfaceDescription : Blueprint->ImplementedInterfaces)
	{
		Builder.Appendf(TEXT("- %s\n"), InterfaceDescription.Interface ? *InterfaceDescription.Interface->GetPathName() : TEXT("None"));
	}
	Builder.Append(TEXT("\n"));
}

void SBlueprintImplementInspectWidget::AppendBlueprintVariables(FStringBuilderBase& Builder, UBlueprint* Blueprint) const
{
	Builder.Append(TEXT("## Blueprint Variables\n"));
	if (Blueprint->NewVariables.Num() == 0)
	{
		Builder.Append(TEXT("(none)\n\n"));
		return;
	}

	for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
	{
		Builder.Appendf(TEXT("- Name: %s\n"), *Variable.VarName.ToString());
		Builder.Appendf(TEXT("  Category: %s\n"), *Variable.Category.ToString());
		Builder.Appendf(TEXT("  Type: %s\n"), *Variable.VarType.PinCategory.ToString());
		Builder.Appendf(TEXT("  SubCategory: %s\n"), *Variable.VarType.PinSubCategory.ToString());
		Builder.Appendf(TEXT("  SubCategoryObject: %s\n"), Variable.VarType.PinSubCategoryObject.IsValid() ? *Variable.VarType.PinSubCategoryObject->GetPathName() : TEXT("None"));
		Builder.Appendf(TEXT("  Container: %s\n"), *UEnum::GetValueAsString(Variable.VarType.ContainerType));
		Builder.Appendf(TEXT("  DefaultValue: %s\n"), Variable.DefaultValue.IsEmpty() ? TEXT("<empty>") : *Variable.DefaultValue);
		Builder.Append(TEXT("  MetaData:\n"));
		if (Variable.MetaDataArray.Num() == 0)
		{
			Builder.Append(TEXT("    (none)\n"));
		}
		else
		{
			for (const FBPVariableMetaDataEntry& MetaDataEntry : Variable.MetaDataArray)
			{
				Builder.Appendf(TEXT("    - %s: %s\n"), *MetaDataEntry.DataKey.ToString(), MetaDataEntry.DataValue.IsEmpty() ? TEXT("<empty>") : *MetaDataEntry.DataValue);
			}
		}
	}
	Builder.Append(TEXT("\n"));
}

void SBlueprintImplementInspectWidget::AppendClassDefaults(FStringBuilderBase& Builder, UBlueprint* Blueprint) const
{
	Builder.Append(TEXT("## CDO Defaults\n"));
	UClass* GeneratedClass = Blueprint->GeneratedClass;
	UObject* CDO = GeneratedClass ? GeneratedClass->GetDefaultObject() : nullptr;
	if (GeneratedClass == nullptr || CDO == nullptr)
	{
		Builder.Append(TEXT("(no generated class CDO)\n\n"));
		return;
	}

	for (TFieldIterator<FProperty> PropertyIt(GeneratedClass, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
	{
		FProperty* Property = *PropertyIt;
		if (Property == nullptr || Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated | CPF_DisableEditOnInstance))
		{
			continue;
		}

		FString ValueText;
		Property->ExportText_InContainer(0, ValueText, CDO, CDO, CDO, PPF_None);
		Builder.Appendf(TEXT("- %s (%s): %s\n"), *Property->GetName(), *Property->GetCPPType(), ValueText.IsEmpty() ? TEXT("<empty>") : *ValueText);
	}
	Builder.Append(TEXT("\n"));
}

void SBlueprintImplementInspectWidget::AppendBlueprintGraphs(FStringBuilderBase& Builder, UBlueprint* Blueprint) const
{
	auto AppendGraph = [&Builder](const TCHAR* SectionName, const UEdGraph* Graph)
	{
		if (Graph == nullptr)
		{
			return;
		}

		Builder.Appendf(TEXT("### %s: %s\n"), SectionName, *Graph->GetName());
		Builder.Appendf(TEXT("NodeCount: %d\n"), Graph->Nodes.Num());

		for (const UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node == nullptr)
			{
				continue;
			}

			Builder.Appendf(TEXT("\nNode: %s\n"), *Node->GetName());
			Builder.Appendf(TEXT("Title: %s\n"), *Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
			Builder.Appendf(TEXT("Class: %s\n"), *Node->GetClass()->GetPathName());
			Builder.Appendf(TEXT("Comment: %s\n"), Node->NodeComment.IsEmpty() ? TEXT("<empty>") : *Node->NodeComment);
			Builder.Appendf(TEXT("Position: X=%d Y=%d\n"), Node->NodePosX, Node->NodePosY);
			Builder.Append(TEXT("Pins:\n"));

			for (const UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin == nullptr)
				{
					continue;
				}

				Builder.Appendf(TEXT("  - %s [%s] Type=%s Default=%s Links="),
					*Pin->PinName.ToString(),
					Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"),
					*Pin->PinType.PinCategory.ToString(),
					Pin->DefaultValue.IsEmpty() ? TEXT("<empty>") : *Pin->DefaultValue);

				if (Pin->LinkedTo.Num() == 0)
				{
					Builder.Append(TEXT("None\n"));
					continue;
				}

				for (int32 LinkIndex = 0; LinkIndex < Pin->LinkedTo.Num(); ++LinkIndex)
				{
					const UEdGraphPin* LinkedPin = Pin->LinkedTo[LinkIndex];
					Builder.Appendf(TEXT("%s%s.%s"),
						LinkIndex > 0 ? TEXT(", ") : TEXT(""),
						LinkedPin && LinkedPin->GetOwningNode() ? *LinkedPin->GetOwningNode()->GetName() : TEXT("None"),
						LinkedPin ? *LinkedPin->PinName.ToString() : TEXT("None"));
				}
				Builder.Append(TEXT("\n"));
			}
		}

		TSet<UObject*> NodesToExport;
		for (UEdGraphNode* NodeToExport : Graph->Nodes)
		{
			if (NodeToExport != nullptr)
			{
				NodesToExport.Add(NodeToExport);
			}
		}

		FString GraphCopyText;
		FEdGraphUtilities::ExportNodesToText(NodesToExport, GraphCopyText);
		Builder.Append(TEXT("\nCtrlCNodeText:\n"));
		Builder.Append(GraphCopyText.IsEmpty() ? TEXT("<empty>") : GraphCopyText);
		Builder.Append(TEXT("\n\n"));
	};

	Builder.Append(TEXT("## Graphs\n"));
	for (const UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		AppendGraph(TEXT("Ubergraph"), Graph);
	}
	for (const UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		AppendGraph(TEXT("Function"), Graph);
	}
	for (const UEdGraph* Graph : Blueprint->MacroGraphs)
	{
		AppendGraph(TEXT("Macro"), Graph);
	}
	for (const UEdGraph* Graph : Blueprint->DelegateSignatureGraphs)
	{
		AppendGraph(TEXT("DelegateSignature"), Graph);
	}
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
