<p align="right"><strong>简体中文</strong> · <a href="README.md">English</a></p>

# FoloToy AI Passport 版 ClassTrack

<p align="center">
  <a href="https://tzcwvccj4w-create.github.io/classtrack-passport"><strong>产品介绍与完整使用方法</strong></a> ·
  <a href="https://github.com/tzcwvccj4w-create/classtrack-passport">源码</a> ·
  <a href="https://tzcwvccj4w-create.github.io/classtrack-passport/downloads/ClassTrack-Manager-macOS.dmg">Mac 管理工具</a>
</p>

ClassTrack（课到哪了？）是一款运行在 ESP32-C3 FoloToy AI Passport 上的离线教学进度记录器。每班只保留最近一条录音，保存录制时间，替换前可以试听，并通过 USB 串口与 Mac 交换时间、班级 CSV 和备份数据。


> ClassTrack 是 FoloToy AI Passport 社区项目，不是 FoloToy 官方产品。

## 直接使用

1. 用上、下键选择班级，按 OK 进入。
2. 按 OK 开始录音，再按一次结束，单条最长 20 秒。
3. 试听后选择确认保存、重新录制或放弃修改。
4. 需要校时或上传课表时，打开 [Mac 管理工具](https://tzcwvccj4w-create.github.io/classtrack-passport/downloads/ClassTrack-Manager-macOS.dmg)；关闭窗口后应用完全退出，不会常驻后台。

课表使用 UTF-8 CSV，表头必须为 `id,name,course`：

```csv
id,name,course
0,26机电2班,高职英语
1,26语言1班,汉语听力
```

精确的固件校验值、已完成功能和未完成实机验收见
[`records/test-record-2026-10-29.zh_CN.md`](records/test-record-2026-10-29.zh_CN.md)。Mac 使用方法见
[`docs/class-track/mac-manager.zh_CN.md`](docs/class-track/mac-manager.zh_CN.md)。

## 当前第一阶段

- 修复后的 240×320 浏览器原型，七个页面均完成截图和安全区检查。
- ESP-IDF 5.5.3 应用骨架，遵循官方屏幕、按键、ES8311、电量计、共享 I2C 和深睡眠 BSP 约定。
- 16 kHz 流式 IMA ADPCM，不申请 20 秒 PCM 大缓冲。
- 双银行、追加写 Flash 存储：最终提交标记有效前，新录音不可见；录制或保存中断时仍选择旧录音。
- 最多 12 个班级，每条最长 20 秒，每个银行保留两个事务备用槽位。
- USB 串口协议、只在前台运行的原生 Mac 应用（校时、导入 CSV），以及命令行 ADPCM 备份工具。
- 覆盖编码边界、替换中断、银行整理、布局矩形、CSV 校验和管理器协议的主机测试。

## 验证

```text
./tools/validate_classtrack.sh --host
./tools/validate.sh --firmware
```

固件验证需要激活 ESP-IDF v5.5.3。编译不会自动刷写设备。

存储事务、内存预算、USB 协议、休眠顺序和待完成的实机验收见 `docs/class-track/architecture.zh_CN.md`。

## 开源与状态

项目使用 [MIT 许可证](LICENSE)。当前固件已经编译、刷写并完成基本启动与 USB 验证，但录音/回放音质、分阶段断电、深睡电流和长时运行仍需完整实机验收。请查看 [2026-10-29 测试记录](records/test-record-2026-10-29.zh_CN.md) 了解精确边界。
