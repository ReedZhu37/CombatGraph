# 依赖与验证

| 范围 | 依赖 |
|---|---|
| `RZCombatRuntime` | Unreal Engine 的 Core、CoreUObject、Engine、GameplayTags 模块，以及引擎内置 Gameplay Abilities 插件。 |
| `RZCombatEditor` | 同仓库的 `RZCombatRuntime` 和 Unreal Engine 的编辑器模块，包括 GraphEditor、Slate、AssetTools、ToolMenus 和 UnrealEd。 |
| 宿主集成 | 项目自己的角色、输入绑定、ASC、Ability、Montage 和命中等玩法逻辑。Duelist 的原输入处理器用 `RZISInputSystem`，原连招 Ability 父类用 `RZGAS`，武器与命中示例接入 `RZWeapon` 和 `RZTrace`。这些接线和 Hero、`CT_Nodachi`、`Lvl_InputTest` 资产由宿主维护。 |

`RZCombat.uplugin` 保持原插件名和模块名，已有资产使用的 `/Script/RZCombatRuntime` 和 `/Script/RZCombatEditor` 路径因此仍可解析。迁移具体 Combo Table 时，另外检查它所引用的 Ability、Checker 和 Payload 类是否在目标工程可用。

## 运行自动化测试

插件源码内含 `RZCombat.Combo` 测试，覆盖运行时解析、GAS 生命周期、图编译和调试实例选择。在 Editor 的 **Session Frontend → Automation** 里可运行，也可以在装有插件且已构建的宿主工程中执行以下命令。测试自带其使用的 Native Gameplay Tags。

```powershell
& $UnrealEditorCmd $HostProject -Unattended -NullRHI -NoSplash -NoSound -nop4 `
  '-ExecCmds=Automation RunTests RZCombat.Combo' `
  '-TestExit=Automation Test Queue Empty' `
  "-ReportExportPath=$ReportPath"
```

`$UnrealEditorCmd` 是 `UnrealEditor-Cmd.exe` 的路径，`$HostProject` 是测试工程的 `.uproject` 路径，`$ReportPath` 是报告目录。查看报告中的失败和未运行数量。真实角色的 PIE 高亮及界面操作另需现场验证。

## 初始独立版验证

UE 5.8.4 / Win64 Development Editor 在空白 C++ 宿主工程中构建成功。宿主只显式启用 `RZCombat`，使用引擎内置 Gameplay Abilities，无 Duelist 资产或配置。无窗口运行 73 项测试，72 项无警告通过，1 项通过并报告空白工程未配置 `GameplayCueNotifyPaths`，失败与未运行均为 0。
