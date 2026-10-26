# TaskCalendar

紧凑的 Qt 桌面任务时间轴。每行一个任务，可添加任意数量的自定义节点，显示节点名称、日期和倒计时。节点统一使用浅色样式，没有颜色分类、图例或选色器。

## 使用

- 点击「＋ 任务」创建任务；点击任务名称编辑，动态增删节点。日期可直接粘贴 YYYY-MM-DD、YYYY/M/D、YYYYMMDD，或打开日历；留空表示待定。
- 节点显示名称、日期和剩余天数；已过期显示经过天数。点击节点展开完整详情，可勾选完成；单个节点标签长按 0.5 秒显示圆环读条并切换完成状态，移出取消，短按仍展开详情，聚合标签只展开。
- 疏松标签显示名称、日期和倒计时；相互拥挤的标签只显示完整名称，以紧凑的两行错开；非常密集时合并节点为「名称 +N」，点击可展开全部节点，没有固定节点数量限制。
- 左侧标题区滚轮上下滚动任务列表；右侧时间轴滚轮平移时间，Ctrl + 滚轮缩放。日期表头和今天标注固定在顶部。今天越界时点击边缘箭头归位；每次启动默认显示今天。
- 拖动六点柄排序；Alt + ↑ / ↓ 也可排序。锁图标锁定窗口位置和大小。
- 保留半透明灰色背景和默认桌面伴随逻辑；不启用 WorkerW 实验挂载。

## 运行与数据

直接打开 build-task/TaskCalendar.exe，已附运行依赖。

数据独立保存于 Qt AppDataLocation 下的 TaskCalendar/TaskCalendar/tasks.json 和 settings.ini，与 PaperCalendar 的会议数据分离。支持 --data-dir 指定便携数据目录。--demo 仅在内存生成演示任务，建议配合独立 --data-dir；演示数据发生编辑后会正常保存到该目录。

## 构建与验证

需要 CMake、C++17、Qt Widgets，测试还需要 Qt Test。使用新目录 build-task，复制来的 build-calendar 缓存不能复用。

```powershell
cmake -S . -B build-task -G Ninja -DCMAKE_PREFIX_PATH="你的 Qt 安装目录"
cmake --build build-task
ctest --test-dir build-task --output-on-failure
```

测试覆盖任意节点持久化、日期和倒计时、400 个密集节点布局、动态编辑、完成和排序保存、损坏数据保护、平移缩放、节点展开及锁定。Windows 原生平台也已运行通过。

复制保留的旧会议模型、旧测试及 HTML 文档属于原项目参考，当前 CMake 仅编译任务版本。界面截图见 build-task/task-demo.png。
