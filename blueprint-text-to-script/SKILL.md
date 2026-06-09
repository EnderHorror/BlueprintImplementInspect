---
name: blueprint-text-to-script
description: Convert BlueprintImplementInspect exported Blueprint LLM text back into corresponding UE script, especially Unreal Engine Angelscript gameplay code. Use when the user pastes text exported by the Blueprint Implement Inspect plugin and asks to restore, translate, generate, recreate, or convert it into script/code.
---

# Blueprint Text To Script

这个 Skill 用于把 `BlueprintImplementInspect` 插件导出的蓝图文本还原成脚本草案。默认优先生成 Unreal Engine Angelscript，也可以按用户要求生成 C++、伪代码或逻辑说明。

## 什么时候使用

当用户提供或引用 `BlueprintImplementInspect` 导出的文本，并提出以下需求时使用：

- 把蓝图文本还原成脚本。
- 根据蓝图导出内容生成 UE Angelscript。
- 把 `Graphs`、节点、Pin、Link、`CtrlCNodeText` 翻译成代码逻辑。
- 从蓝图变量、CDO 默认值、函数图、事件图中整理可维护脚本。
- 分析蓝图逻辑并输出伪代码、迁移方案或重构建议。

常见触发语句：

- “把下面这个蓝图导出文本还原成 Angelscript”
- “根据这个 `# Blueprint LLM Export` 生成脚本文件”
- “把这个蓝图逻辑转成 C++/伪代码”
- “根据 `CtrlCNodeText` 和 `Graphs` 还原函数逻辑”

## 用户怎么使用

这个 Skill 放在 `c:\Users\Ender\.copilot\skills\blueprint-text-to-script\SKILL.md` 后，VS Code Copilot 会在匹配描述时自动选择它。为了提高触发概率，提问时建议同时包含 `BlueprintImplementInspect`、`# Blueprint LLM Export`、`蓝图文本`、`还原成脚本`、`Angelscript` 等关键词。

也可以在聊天里直接点名：

- “使用 `blueprint-text-to-script`，把下面蓝图文本还原成 Angelscript。”
- “用我的 `blueprint-text-to-script` skill 分析这个导出文件。”
- “调用 `blueprint-text-to-script`，根据 `CtrlCNodeText` 生成伪代码。”

### 方式一：直接粘贴导出文本

用户可以把插件导出的完整文本粘贴到聊天中，并说明目标语言：

- “把下面内容还原成 Angelscript”
- “生成脚本文件，放到项目合适目录”
- “先输出伪代码，不要写文件”

如果目标语言没有说明，默认生成 UE Angelscript。

### 方式二：提供导出文本文件路径

用户也可以提供 `.txt` 或 `.md` 文件路径：

- “读取 `Saved/BlueprintExports/BP_Enemy.txt`，还原成 Angelscript”
- “把这个导出文件转换成脚本并创建文件”

读取文件后，应优先解析结构化区块，再决定是否需要读取 `CtrlCNodeText` 补充细节。

### 方式三：只要求分析逻辑

如果用户暂时不想生成代码，可以输出：

- 蓝图类职责总结。
- 变量和默认值清单。
- 事件/函数调用流程。
- 节点逻辑伪代码。
- 无法可靠还原的节点列表。

### 推荐提问格式

用户最好提供以下信息：

- 导出的蓝图文本或文件路径。
- 目标语言：`Angelscript`、`C++`、`伪代码`。
- 是否需要创建文件。
- 期望文件目录或模块位置。
- 是否允许用 TODO 保留无法确定的节点。

示例：

> 读取 `Projects/JinYiWei/Saved/BlueprintExports/BP_Test.txt`，把它还原成 UE Angelscript，并创建到合适的脚本目录。无法确定的节点用 TODO 保留。

## 输入格式

导出文本通常包含以下结构：

- `# Blueprint LLM Export`
- `Name`、`AssetPath`、`GeneratedClass`、`ParentClass`
- `## Class`
- `## Blueprint Variables`
- `## CDO Defaults`
- `## Graphs`
- `### Ubergraph`
- `### Function`
- `### Macro`
- `### DelegateSignature`
- `Node`、`Title`、`Class`、`Comment`、`Position`
- `Pins`、默认值、Pin 类型、Pin 连接
- `CtrlCNodeText`

优先级规则：

- `Blueprint Variables` 用于生成成员变量。
- `CDO Defaults` 用于生成默认值或初始化逻辑。
- `Graphs` 用于还原函数、事件、控制流和数据流。
- `CtrlCNodeText` 只作为补充参考，用于确认节点标题、Pin 默认值、节点类名或复制格式中的缺失信息。

## 工作流程

### 1. 识别蓝图身份

- 提取 `Name`、`AssetPath`、`GeneratedClass`、`ParentClass`。
- 根据蓝图名推导脚本类名。
- 根据 `ParentClass` 推导父类。
- 如果父类不明确，保留 TODO，不要凭空指定复杂继承。

### 2. 还原变量和默认值

- 从 `Blueprint Variables` 提取变量名、类型、分类、可编辑性和说明。
- 从 `CDO Defaults` 提取默认值。
- 安全默认值可以写成字段初始化。
- 复杂对象引用、资产引用、组件引用应保留 TODO 或注释说明。

### 3. 还原事件和函数

- `Ubergraph` 中的事件节点还原为事件函数或生命周期函数。
- `Function` 图还原为普通函数。
- `Macro` 图优先还原为私有辅助函数。
- `DelegateSignature` 图还原为委托签名或事件说明。

### 4. 还原执行流

- 根据 Exec Pin 的连接顺序生成语句顺序。
- `Branch` 还原为 `if / else`。
- `Sequence` 还原为顺序语句。
- `ForLoop`、`ForEachLoop` 还原为循环。
- `Switch` 还原为 `switch` 或 `if` 链。
- `Delay`、`Timeline`、异步/潜伏节点无法安全等价时保留 TODO。
- 函数调用节点还原为直接调用。
- Set/Get 变量节点还原为赋值和表达式。

### 5. 还原数据流

- 根据非 Exec Pin 连接构造表达式。
- 简单表达式直接内联。
- 复杂表达式拆成局部变量。
- 无法推断的输入值使用 TODO，并保留原节点标题方便追溯。

### 6. 输出脚本

- 默认生成 UE Angelscript。
- 如果用户明确要求 C++ 或伪代码，应按用户要求输出。
- 代码应优先可读、可维护，不需要逐节点机械翻译。
- 无法确定的蓝图行为必须用 TODO 标记。

## Angelscript 输出规范

生成 UE Angelscript 时遵循以下规则：

- 蓝图类还原为 `class UName : UParent` 或 `class AName : AParent`。
- Actor 蓝图通常使用 `A` 前缀，Object/UserWidget 等通常使用 `U` 前缀。
- 可编辑变量使用 `UPROPERTY()`。
- 可调用函数使用 `UFUNCTION()`。
- 生命周期事件尽量映射为父类支持的重写或事件方法。
- CDO 默认值优先写成字段默认值；复杂默认值放到初始化函数中。
- 组件、资产、DataTable、Curve、Timeline、Widget 绑定等不确定内容保留 TODO。
- 如果需要写 Angelscript 文件，应遵循当前项目已有 Angelscript 目录和命名习惯。

## 输出格式

### 用户要求创建文件时

- 直接在工作区创建或修改文件。
- 如果能从项目结构判断目录，就选择合理位置。
- 如果无法判断脚本目录或目标语言，只问一个简短问题确认。
- 完成后只简要说明创建了哪些文件和主要内容。

### 用户只要求生成内容时

- 可以直接输出脚本、伪代码或分析结果。
- 输出应简洁，不要解释过多无关细节。
- 如果代码很长，优先建议写入文件。

### 用户要求分析时

输出建议包含：

- 类职责。
- 变量和默认值。
- 事件入口。
- 函数逻辑。
- 无法还原的节点和原因。
- 建议的脚本结构。

## 还原质量要求

- 不要编造节点连接中不存在的行为。
- 不要把 `CtrlCNodeText` 当成唯一真相。
- 保留原节点标题、函数名、变量名，便于用户核对蓝图。
- 对不确定类型、资产路径、组件引用、潜伏行为使用 TODO。
- 优先输出可维护脚本，而不是逐节点堆砌。
- 如果导出文本不完整，先输出结构化伪代码，再询问是否继续生成脚本。

## 常见节点映射

- `K2Node_Event` -> 事件函数。
- `K2Node_CallFunction` -> 函数调用。
- `K2Node_VariableGet` -> 变量读取。
- `K2Node_VariableSet` -> 变量赋值。
- `K2Node_IfThenElse` -> `if / else`。
- `K2Node_ExecutionSequence` -> 顺序语句。
- `K2Node_ForLoop` -> `for` 循环。
- `K2Node_ForEachElementInEnum` / ForEach 类节点 -> `for` / `foreach`。
- `K2Node_DynamicCast` -> 类型转换和空检查。
- `K2Node_Switch` -> `switch` 或 `if` 链。
- Timeline、Delay、Async Action -> TODO 或项目特定封装。

## 限制说明

蓝图文本无法 100% 还原所有编辑器语义。以下内容需要谨慎处理：

- Timeline 曲线和轨道数据。
- Latent Action、异步节点和延迟行为。
- 动画蓝图状态机。
- UMG 设计器层级和绑定。
- 插件自定义 K2 节点。
- 运行时动态创建的组件或资源引用。

遇到这些情况时，应明确说明假设，并在代码中保留 TODO。
