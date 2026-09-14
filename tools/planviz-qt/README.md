# PlanViz Qt

Qt 6 desktop visualizer copied from the established `planviz-qt` interface and
adapted in this repository for LifelongTA debug traces. The existing LoRR
`--map` + `--plan` workflow is retained.

Build with vcpkg:

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Release
```

Run:

```powershell
.\build\planviz-qt.exe --map C:\path\to\map.map --plan C:\path\to\output.json --start 0 --end 500 --n 4000
```

For a LifelongTA trace, the map is embedded in the trace file, so only one
input path is needed:

```powershell
.\build\Release\planviz-qt.exe --trace C:\gitcloud\LifelongTA\results\trace_smoke\heap_summary.json.trace.json --n 100
```

On this machine, use the wrapper script so Qt DLLs and plugins are found:

```powershell
.\run_planviz_qt.ps1 -Map C:\path\to\map.map -Plan C:\path\to\output.json -Start 0 -End 500 -Agents 4000
```

LifelongTA trace launcher:

```powershell
.\run_planviz_qt.ps1 -Trace C:\gitcloud\LifelongTA\results\trace_smoke\heap_summary.json.trace.json -Agents 100
```

Controls:

- Mouse wheel: zoom
- Left drag: pan
- Left click agent: select/deselect and show its future path plus a dashed arrow to its next task errand
- Left click empty map: clear selection
- Play/Pause/Prev/Next: timestep controls

示例warehouse地图

```
.\run_planviz_qt.ps1 `
   -Map C:\gitcloud\PlanViz\example\warehouse_small.map `
   -Plan C:\gitcloud\PlanViz\example\warehouse_small_2026.json
```

自建game地图

```
.\run_planviz_qt.ps1 `
   -Map C:\gitcloud\auto-ssfc-task-scheduler\lorr-code\guess_errands\game.domain\maps\orz900d.map `
   -Plan C:\gitcloud\auto-ssfc-task-scheduler\guess-errand-script\orz-example_4000.output.json
```



```
.\build-vs18-release\planviz-qt.exe `
  --map C:\gitcloud\auto-ssfc-task-scheduler\lorr-code\guess_errands\game.domain\maps\orz900d.map `
  --plan C:\gitcloud\auto-ssfc-task-scheduler\guess-errand-script\orz-example_4000.output.json
```

沙箱走廊地图（小型）

```
.\run_planviz_qt.ps1 `
   -Map C:\gitcloud\auto-ssfc-task-scheduler\lorr-code\sandbox_output\single_lane_bridge.map `
   -Plan C:\gitcloud\auto-ssfc-task-scheduler\lorr-code\sandbox_output\single_lane_bridge_kei18_lacam0_config_search.output.json
```

沙箱走廊地图（中型）

```
.\run_planviz_qt.ps1 `
  -Map C:\gitcloud\auto-ssfc-task-scheduler\lorr-code\sandbox_output\single_lane_bridge.map `
  -Plan C:\gitcloud\auto-ssfc-task-scheduler\lorr-code\sandbox_output\single_lane_bridge_single_lane_left_priority.output.json
```

沙箱room地图

```
cd C:\gitcloud\auto-ssfc-task-scheduler\planviz-qt

.\run_planviz_qt.ps1 `
  -Map C:\gitcloud\auto-ssfc-task-scheduler\lorr-code\sandbox_output\room_door_test\room_door_congestion.map `
  -Plan C:\gitcloud\auto-ssfc-task-scheduler\lorr-code\sandbox_output\room_door_test\room_door_congestion_pure_pibt.output.json `
  -Agents 20 `
  -Start 0 `
  -End 200

```



```
cd C:\gitcloud\auto-ssfc-task-scheduler\planviz-qt

.\run_planviz_qt.ps1 `
  -Map C:\gitcloud\auto-ssfc-task-scheduler\lorr-code\sandbox_output\room_door_left_priority_test\room_door_congestion.map `
  -Plan C:\gitcloud\auto-ssfc-task-scheduler\lorr-code\sandbox_output\room_door_left_priority_test\room_door_congestion_room_door_left_priority.output.json `
  -Agents 20 `
  -Start 0 `
  -End 200

```

竞赛room地图

```
.\run_planviz_qt.ps1 `
  -Map C:\gitcloud\auto-ssfc-task-scheduler\lorr-code\guess_errands\room.domain\maps\room-64-64-16.map `
  -Plan C:\gitcloud\auto-ssfc-task-scheduler\guess-errand-long\room-example_200.output.json
```

