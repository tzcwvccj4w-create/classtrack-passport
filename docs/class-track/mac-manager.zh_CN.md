<p align="right"><strong>简体中文</strong> · <a href="mac-manager.md">English</a></p>

# ClassTrack Mac 管理工具

原生 macOS 应用把时间同步和班级 CSV 上传整合在一个只在前台运行的工具中。它不创建 LaunchAgent、不加入登录项、不定时扫描设备；关闭最后一个窗口后进程立即退出。

打开 `dist/ClassTrack-Manager-macOS.dmg`，将应用拖入“应用程序”，仅在需要时启动。连接并唤醒 AI Passport、刷新设备列表，然后点击“同步 Mac 当前时间”或“上传课表到设备”。应用会预览内置六班课表，也可选择其他 UTF-8 CSV；表头必须严格为 `id,name,course`。

源码位于 `tools/mac_manager/`。`build_app.sh` 编译原生 arm64 AppKit 应用、运行自检、添加本机临时签名，并生成 ZIP 与 DMG。可选命令行工具使用 `tools/classtrack_native_serial.py` 的无第三方依赖 POSIX 串口实现。

设备无法仅凭 USB 自行读取 macOS 时间，必须有一个 Mac 进程回复串口请求。本项目按用户要求不安装常驻监听器；需要校时或上传时由用户主动打开管理工具。

