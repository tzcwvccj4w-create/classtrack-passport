<p align="right"><strong>简体中文</strong> · <a href="test-record-2026-10-29.md">English</a></p>

# ClassTrack 打包前检查记录

检查日期：2026-10-29（Asia/Shanghai）

| 项目 | 结果 | 说明 |
| --- | --- | --- |
| 五个 FoloToy 工程技能链接 | PASS | `passport-develop/setup/build/device-test/debug` 均可从 `.agents/skills` 读取。 |
| 最新固件归档校验 | PASS | 分区边界、应用/ELF 身份、合并镜像和各组成文件哈希一致。 |
| ClassTrack 主机门禁 | PASS | 编解码、事务存储、CSV/协议、240×320 布局契约、Mac CSV 校验及 6 个运行时契约测试通过。 |
| 原生 Mac 管理应用 | PASS | arm64 AppKit 应用自检、伪串口协议测试、CSV 上传、时间命令、DMG/ZIP 完整性及应用签名通过。 |
| 前台退出行为 | PASS | 实际打开应用后点击“关闭应用”，系统应用清单显示进程已退出；未创建登录项或 LaunchAgent。 |
| 浏览器 240×320 实测 | PASS | 七个页面无安全区越界、测量溢出或元素相交；截图已刷新。 |
| ESP-IDF 固件编译 | PASS（现有构建） | 2026-10-27 生成的匹配构建已复核归档；2026-10-29 未重新编译。 |
| FoloToy 完整官方门禁 | NOT RUN（本次） | macOS 系统 `git` 被未接受的 Xcode 许可阻断；未把旧门禁结果冒充本次结果。当前 ClassTrack 门禁和固件归档校验均已通过。 |
| 设备刷写与启动 | PASS（既有实机记录） | 同一 SHA-256 镜像已刷入 ESP32-C3 rev1.1，芯片端哈希成功；显示、ES8311、ADC 按键、CW2017 初始化正常。 |
| USB 状态/校时/CSV | PASS（既有实机记录） | 命令行管理器的 `HELLO`、Mac 手动校时和六条 UTF-8 班级导入均收到成功响应；新图形应用按钮本次未重新连接实机验收。 |
| 录音、回放、掉电、深睡完整验收 | NOT RUN | 详见 [`docs/class-track/architecture.zh_CN.md`](../docs/class-track/architecture.zh_CN.md) 的实机验收清单。 |

## 固件校验值

```text
full.bin  1d1fa86c0b662e15feff2eb8c577652752f53f9f66511dc618e47b194998e0be
app.bin   c7ce23d6756ef87753609119890057e788ddafe5a1bbae7f3ffe01d7bc465aa8
ELF       4b4a229fd870e0f8c999fdd849b12711ccc5832de1c0f860a359c3780cecb0a8
```

## 本次实际执行

```text
python3 tools/install_passport_skills.py --check
python3 tools/archive_firmware.py verify build/firmware/1d1fa86...e0be
CC='<Apple clang with macOS SDK>' ./tools/validate_classtrack.sh
node prototype/tests/layout.spec.js
./tools/mac_manager/build_app.sh
codesign --verify --deep --strict <extracted-app>
hdiutil verify dist/ClassTrack-Manager-macOS.dmg
```

