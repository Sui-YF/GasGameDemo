# CultivationVsAliensDemo 工程约定

## 核心分层

- C++：联网权威、GAS、属性、伤害、背包、换装复制、AI、刷兵、存档、输入和 UI 数据接口。
- Blueprint：角色/敌人资产配置、Gameplay Ability 参数、任务事件连接和关卡摆放。
- Widget Blueprint：全部正式 UI 排版、锚点、样式、动画与焦点导航。
- DataTable：批量平衡数据。
- DataAsset：需要独立 Mesh、Icon 或类引用的物品定义。

## 已落地的数据表

- `/Game/CVAD/Data/DT_PlayerBalance`
- `/Game/CVAD/Data/DT_EnemyBalance`
- `/Game/CVAD/Data/DT_SpawnerProfiles`

## 已落地的 Widget Blueprint

- `WBP_HUD`：已有正式控件树，绑定 HealthBar、StaminaBar、SpiritBar、ObjectiveText、DefeatText。
- `WBP_Inventory`：已有正式控件树，绑定五个装备槽按钮与关闭按钮。
- MainMenu、Lobby、Pause、Settings、Result、NameEntry 已创建资产，仍需逐个完成排版和流程。

## 优先级待办

1. 玩家死亡、倒地、双人救援和单人护身符复起。
2. 小兵攻击动画、受击、击退、浮空、死亡表现与对象池。
3. 飞剑实体、召回轨迹、剑印 Gameplay Effect 与协同引爆。
4. 主菜单、单人开始、创建/加入 Listen Server、大厅准备流程。
5. 暂停、设置、名字输入、结算 UI 排版与逻辑。
6. Enhanced Input 的运行时重绑、冲突检测和存档。
7. 刷兵 Box 与战场阶段、占领点、防守核心、外星信标的事件连接。
8. Boss 资产导入后的 GAS、AI 与阶段机制。
9. 正式地图导入、导航、任务点迁移和性能预算。
10. 双进程联机测试、100ms 延迟测试和打包验证。

## 禁止事项

- 不在 Widget C++ 类中硬编码正式排版。
- 客户端不直接修改生命、装备、击杀数或任务状态。
- 不在战斗中保存 Actor 指针、临时 Gameplay Effect 或 AI 运行状态到磁盘。
- 不把大量独立资产引用集中进一张难以维护的 DataTable。
