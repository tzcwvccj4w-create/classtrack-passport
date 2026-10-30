<p align="right"><a href="mac-manager.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# ClassTrack Mac manager

The native macOS app combines time synchronization and class CSV import in one foreground-only tool. It creates no LaunchAgent or login item, performs no periodic device scan, and exits when its last window closes.

Open `dist/ClassTrack-Manager-macOS.dmg`, drag the app to Applications, and launch it only when needed. Connect and wake the AI Passport, refresh the device list, then use **Sync Mac Time** or **Upload Classes**. The app previews the bundled six-class CSV and can select another UTF-8 CSV with the exact `id,name,course` header.

The source is under `tools/mac_manager/`. `build_app.sh` compiles a native arm64 AppKit bundle, runs its self-test, applies an ad-hoc local signature, and creates ZIP and DMG deliverables. The optional command-line manager uses the dependency-free POSIX serial transport in `tools/classtrack_native_serial.py`.

The device cannot read macOS time from USB by itself. A Mac process must reply to the serial request. This project deliberately does not install a persistent listener; the user explicitly opens the manager when synchronization or import is needed.

