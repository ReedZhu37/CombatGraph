# CombatGraph for Unreal Engine

CombatGraph is the visual combo editor and Gameplay Ability System integration provided by the `RZCombat` plugin. Author ability transitions in a graph, compile them into a Combo Table, and trace the path a running character takes during Play in Editor.

- **Author combos.** Connect an entry and ability nodes, configure Gameplay Tag triggers and per-transition conditions, and validate the graph at compile time.
- **Resolve the next ability.** Use `URZCombatComboComponent` to resolve a trigger against the active table and current node. The host activates the selected ability and chooses how to commit the next node.
- **Trace execution.** Follow the actual node and transition in PIE, switch between matching instances, and read a bounded history of combo and GAS events.

## Install

Clone this repository into the `Plugins/RZCombat` directory of a C++ Unreal project and enable **RZCombat**. Build the project's Editor target and reopen the project. The plugin declares its dependency on Epic's **Gameplay Abilities** plugin, which ships with Unreal Engine. Unreal Engine's Gameplay Tags module also ships with the engine. The baseline for this extraction is UE 5.8.4 on Windows.

Create a **RZ Combat Combo Table** data asset and open it to start authoring. Assign the compiled table to a character with a **RZ Combat Combo** component. The project supplies the input binding, the granted abilities, and their effects or animations.

- [Two-step combo, GAS setup, and debugging](Docs/Usage.md)
- [Dependency boundaries and development checks](Docs/Development.md)
