# CombatGraph 使用指南

## 创建两段连招

1. 在 C++ Unreal 工程的 `Plugins/RZCombat` 安装并启用插件。`GameplayAbilities` 是描述符声明的引擎内置依赖。
2. 在 **Project Settings → Gameplay Tags** 添加一个项目自己的触发标签，例如 `Input.Combo.Light`。创建 `GA_ComboFirst` 和 `GA_ComboSecond` 两个 Gameplay Ability，可先在各自的激活流程显示不同文字并结束，以便观察。
3. 在内容浏览器创建 Data Asset，选择 `RZCombatComboTableAsset`（编辑器显示为 **RZ Combat Combo Table**），双击打开。在固定的 `Entry` 下新增 `First`、`Second` 两个 Combo 节点，分别指定两个 Ability。
4. 从 `Entry` 的 `Out` 连到 `First` 的 `In`，再从 `First` 连到 `Second`。选中每条连线的 Transition 或目标节点内的 `From` 项，在 Details 给两条线填入同一个 `TriggerTag`。点击 **Compile** 并保存。
5. 给角色添加 `URZCombatComboComponent`，把这张表填入 **Combo Tables**，并设为 **Active Combo Table**。运行时也可以用 `AddComboTable` 和 `SetActiveComboTable` 设置。
6. 初始化角色的 `UAbilitySystemComponent`，通过 GAS 授予两个 Ability，绑定一个攻击输入或测试按钮。每次输入先调用 `RequestCombo`，再由宿主激活返回的 `AbilityClass`。重复两次应依次触发 `First` 和 `Second`。下一轮前调用 `ResetCombo`，或为末段配置显式的后继路线。

## 连接 Gameplay Ability System

以下示例假设传入的组件和 ASC 已初始化，且 Ability 已在本地测试角色上获得授予。启用组件的 `bObserveAbilitySystemLifecycle`，保留默认的 `bCommitComboOnAbilityActivated`，由观察器在匹配的 Ability 激活时推进连段。

```cpp
#include "AbilitySystemComponent.h"
#include "RZCombatComboComponent.h"

bool TriggerCombo(URZCombatComboComponent& Combo, UAbilitySystemComponent& ASC, FGameplayTag Trigger)
{
    const FRZCombatComboRequestResult Result = Combo.RequestCombo(Trigger);
    if (Result.Result != ERZCombatComboRequestResultType::Success || !Result.AbilityClass)
    {
        return false;
    }

    return ASC.TryActivateAbilityByClass(Result.AbilityClass);
}
```

在宿主 C++ 模块的 `Build.cs` 添加 `RZCombatRuntime`。直接使用 GAS 和 Gameplay Tags API 的代码还应声明 `GameplayAbilities` 和 `GameplayTags` 模块依赖。蓝图接线可用 `RequestCombo`、拆分结果结构体并按类激活 Ability。

ASC 可以位于角色或其 PlayerState。自定义拓扑可使用 `AbilitySystemComponentOverride`。默认在 BeginPlay 尝试绑定，后续请求也会重试尚未就绪的 ASC。宿主也可以关闭观察器，并在确认执行成功后主动调用 `CommitResolvedCombo(Result)`。每个角色选定一种提交方式。启用观察器后，取消 Ability 默认重置连段，正常结束默认保留当前节点。

如需把过渡配置传进 Gameplay Ability，从 `FRZCombatComboRequestResult` 读取 `Payload`，由宿主通过 `RZCombatComboTargetData::MakeTargetDataHandle` 将 `FRZCombatComboActivationContext` 放进 `FGameplayEventData.TargetData`，再通过 GAS 事件激活。Ability 可以通过 `FindActivationContext` 读取。当前 TargetData 用于本地传递。

## 配置分支

- 同一个源节点可以按不同输入 Tag 连接多个目标。同一触发标签有多个候选时，Compile 按目标节点从左到右生成优先级。组件按顺序检查，条件拒绝后继续尝试后续候选。
- 在 Transition 上添加 `URZCombatComboTransitionChecker` 子类，可从上下文读取 Owner、Pawn、请求、源节点和目标节点。插件提供 And、Or 和 Not 组合。全组件的附加条件可用 `CanUseComboTransition`。
- `URZCombatComboPayload` 子类可承载招式参数。编译时有效配置会复制到运行时表，执行代码从解析结果读取 payload。`ConditionKey` 可由宿主用于自定义条件。
- Compile 会检查缺失 Ability、无效 Trigger Tag、重复节点和无效连接。现有没有编辑器图的 Combo Table 打开时可从运行时数据生成初始图。旧 `Combo Move` 和 `Combo Route` 数据类型保留供现有项目读取。

## 调试运行时路径

1. 打开角色实际使用的 Combo Table，保持 **PIE Debug** 开启。点击编辑器的 **Play** 或 **Simulate**。
2. 调试器自动选择 Active Combo Table 与当前图匹配的组件。用 **Debug Object** 菜单主动切换目标。
3. 执行两次输入，核对活动节点、实际 Transition，以及 **Runtime Debug** 中的 Trigger、Result、Current 和 Pending。前后状态可能在同一帧变化，按事件序号查看顺序。
4. **Combo Commit** 表示当前连段节点已推进。**GA Commit** 表示观察器收到对应 Ability 的 GAS Commit 回调。浅色候选线表示图结构里的可选路线，条件结果以实际执行事件为准。

调试器支持当前编辑器进程中的 PIE 和 Simulate。非 Shipping 构建最多保留最近 128 条事件。若一直显示 **Waiting**，先核对角色是否启用了当前打开的表。若只看到 Pending，核对 Ability 是否已授予并成功激活，以及观察器或宿主提交是否已接线。
