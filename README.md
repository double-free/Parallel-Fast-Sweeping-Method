# Parallel-Fast-Sweeping-Method
Parallel implementation of fast sweeping method for 2D path planning

快速扫描法的并行实现，用于2D路径规划

简介
---
本项目实现了快速行进法(FM)，快速扫描法(FS)，FM2，FS2，以及并行的FS。

`map/`目录下是地图文件，我们使用 csv 文件表述。障碍物为0，可行域为1。可以通过 matlab 转化
二值地图得到。

绘图可以使用 matlab 读取这些 csv 文件并作相应处理。

使用方法
---
1. 切换到 `cpp/` 目录，`make` 生成可执行文件
2. 修改 `cpp/config.ini` 中的配置
3. 执行`./PathPlanner config.ini`
4. 在 `result/` 目录查看结果

依赖的第三方库
---
1. [带 WaitAll 方法的线程池](https://github.com/stfx/ThreadPool2.git)
2. [ini配置文件读取](https://github.com/benhoyt/inih.git)

参考的论文
---
Detrixhe, Miles, Frédéric Gibou, and Chohong Min. "A parallel fast sweeping method for the eikonal equation." Journal of Computational Physics 237 (2013): 46-55.
