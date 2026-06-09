# Blueprint Implement Inspect

`Blueprint Implement Inspect` 是一个 Unreal Engine 编辑器插件，用于批量扫描蓝图、判断蓝图是否包含可执行逻辑，并把选中的蓝图导出为适合喂给 LLM 的结构化文本。

![插件演示](Demo.png)

---

## 功能

- **扫描蓝图资产**：选择 Content 目录后递归扫描蓝图资源。
- **按基类过滤**：可指定 `AActor`、`UUserWidget`、`UObject` 等任意蓝图基类，只显示对应派生蓝图。
- **异步分帧扫描**：扫描过程不会长时间阻塞编辑器，扫描中可随时取消。
- **统计节点数量**：统计每个蓝图的总节点数和逻辑节点数。
- **判断是否有逻辑**：根据已连接执行引脚的节点判断蓝图是“有逻辑”还是“纯配置”。
- **结果排序查看**：支持按总节点数、逻辑节点数排序，方便快速找出复杂蓝图。
- **定位蓝图资产**：点击扫描结果中的蓝图，可在 Content Browser 中自动定位。
- **导出 LLM 文本**：选中蓝图后可复制或保存完整文本，包含类信息、变量、CDO 默认值、图表、函数、节点、Pin、连接关系和接近编辑器 Ctrl+C 的节点文本。
- **配合 Skill 还原脚本**：导出的文本可配合 `blueprint-text-to-script` Skill 转换为 UE Angelscript、C++ 或伪代码。

---

## 插件安装

该插件已放在项目目录中：

如果是首次拉取或插件未显示：

1. 关闭 Unreal Editor。
2. 重新生成项目文件。
3. 打开 `JinYiWei.uproject`。
4. 编译 Editor。
5. 在插件列表或菜单中确认 `Blueprint Implement Inspect` 已启用。

---

## 插件使用

### 打开窗口

插件窗口可从以下入口打开：

| 入口 | 路径 |
|------|------|
| 菜单栏 | **Tools** → **Blueprint Implement Inspect** |
| 菜单栏 | **Window** → **Blueprint Implement Inspect** |
| 工具栏 | Play 工具栏中的 **BII** 按钮 |

### 扫描蓝图

1. 点击 **选择目录**。
2. 选择要扫描的 Content 路径，默认可使用 `/Game`。
3. 在基类选择器中选择目标基类，例如 `AActor`、`UUserWidget`。
4. 点击 **3) 扫描蓝图**。
5. 等待扫描完成，或在扫描过程中点击按钮取消。

扫描结果会显示：

| 列名 | 说明 |
|------|------|
| 蓝图 | 蓝图资产名称 |
| 父类 | 蓝图直接父类 |
| 总节点数 | 蓝图所有图表中的节点数量 |
| 逻辑节点数 | 连接了执行引脚的逻辑节点数量 |
| 判定 | 有逻辑 / 纯配置 |
| 资产路径 | 蓝图资源路径 |

### 定位蓝图

点击结果列表中的任意蓝图行，插件会在 Content Browser 中定位并高亮该蓝图资产。

### 导出给 LLM 的蓝图文本

1. 先在结果列表中选中一个蓝图。
2. 点击 **4) 复制选中蓝图文本**，将文本复制到剪贴板。
3. 或点击 **保存为TXT**，把文本保存为文件。

导出文本包含：

| 区块 | 内容 |
|------|------|
| `Class` | 蓝图类型、父类、接口、编译状态 |
| `Blueprint Variables` | 变量名、分类、Pin 类型、默认值、元数据 |
| `CDO Defaults` | GeneratedClass CDO 上的可编辑属性默认值 |
| `Graphs` | Ubergraph、函数图、宏图、委托签名图 |
| `Nodes / Pins / Links` | 节点标题、节点类、Pin 类型、默认值、连接关系 |
| `CtrlCNodeText` | Unreal 节点导出文本，接近蓝图编辑器 Ctrl+C 格式 |

---

## Skill 安装

`blueprint-text-to-script` 是配套的 Copilot Skill，用于把本插件导出的蓝图文本还原为脚本草案。

Skill 文件位置：

`c:\Users\Ender\.copilot\skills\blueprint-text-to-script\SKILL.md`

如果需要手动安装：

1. 创建目录：`c:\Users\Ender\.copilot\skills\blueprint-text-to-script\`
2. 将 `SKILL.md` 放入该目录。
3. 重启 VS Code 或重新打开 Copilot Chat。
4. 在聊天中使用包含 `blueprint-text-to-script`、`BlueprintImplementInspect`、`# Blueprint LLM Export`、`还原成脚本`、`Angelscript` 等关键词的请求。

---

## Skill 使用

### 直接粘贴文本

复制插件导出的文本后，在 Copilot Chat 中输入：

> 使用 `blueprint-text-to-script`，把下面这个 `BlueprintImplementInspect` 导出的蓝图文本还原成 UE Angelscript。

然后粘贴完整导出文本。

### 使用导出文件

如果导出为 TXT，可以输入：

> 使用 `blueprint-text-to-script`，读取 `Saved/BlueprintExports/BP_Enemy.txt`，还原成 UE Angelscript，并创建到合适脚本目录。

### 只分析不生成文件

如果只想看逻辑，可以输入：

> 使用 `blueprint-text-to-script`，分析下面的蓝图导出文本，先输出伪代码和无法还原的节点，不要创建文件。

### 推荐使用方式

- 需要脚本时，明确目标语言：`Angelscript`、`C++` 或 `伪代码`。
- 需要落盘时，说明“创建文件”或提供目标目录。
- 对无法确定的节点，允许 Skill 用 TODO 保留原节点标题。
- 如果蓝图很复杂，建议先让 Skill 输出伪代码，再生成正式脚本。

---

## 适用场景

- 找出项目中真正包含逻辑的蓝图。
- 批量分析 Widget、Actor、Object 派生蓝图复杂度。
- 把蓝图逻辑整理成 LLM 可理解的文本。
- 让 LLM 基于导出文本生成 Angelscript、C++ 或伪代码。
- 辅助蓝图逻辑迁移、重构、审查和文档化。
