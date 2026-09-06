# macOS Keyboard Logo Fix

这是一个通过 macOS HID 输出报告恢复键盘 LOGO 灯效的小工具。目前已确认适用于以下实体键盘：

- SCC100；
- FMate98。

源码和 App 名称中的 `T100` 来自设备向 macOS 暴露的控制器识别信息，并不代表已经实测名为 T100 的实体键盘。程序当前匹配以下 HID 标识：

- USB 有线：`VID:PID 258A:010C`
- Bluetooth Low Energy：`VID:PID 3554:FA07`，可能显示为 `T100 5.0`

SCC100 和 FMate98 均有效，说明它们很可能使用了相同或兼容的控制器与输出报告协议。其他使用相同 VID/PID 和报告协议的键盘也可能适用，但仍需实机确认。使用不同设备标识或报告协议的型号目前不会被程序识别。

## 下载

从仓库的 [Releases](../../releases) 页面下载最新的 `T100-Logo-0.1.0-macOS.zip`，解压后将 App 拖入“应用程序”文件夹。首次运行如被 macOS 拦截，请在“系统设置 → 隐私与安全性”中选择“仍要打开”，并允许“输入监控”权限。

## 原理

这类键盘连接 macOS 后，LOGO 会被锁定指示灯覆盖为绿色常亮。实机测试发现，向标准键盘 HID 接口发送对应输出报告，可以解除绿色覆盖，使键盘恢复用户此前手动设置并保存在键盘中的 LOGO 灯效。程序本身不会指定灯效颜色或动画：

- 有线：Report ID `0x00`，报告数据 `01`
- 蓝牙：Report ID `0x01`，完整报告 `01 01`

程序每 50 ms 发送一次，共持续 30 秒。它不会刷写固件，也不会修改按键映射。

## 编译命令行程序

需要安装 Xcode Command Line Tools。默认生成同时支持 Apple Silicon 和 Intel Mac 的通用程序：

```sh
make
./t100-logo
```

可选参数为发送持续秒数，范围 1–300：

```sh
./t100-logo 10
```

## 打包 App

```sh
make app
```

生成结果位于：

```text
dist/T100 Logo 白色呼吸.app
```

首次运行需要在“系统设置 → 隐私与安全性 → 输入监控”中允许该 App。自行重新编译会改变临时签名哈希，可能需要先删除旧的输入监控项目，再重新添加新版本。

## 后台自动恢复

后台模式会监听匹配键盘重新连接和 macOS 唤醒事件，并在事件发生后自动恢复 LOGO。平时不会持续轮询蓝牙设备：

```sh
./t100-logo --daemon
```

正常双击 App 会自动根据 App 当前路径创建或更新用户 LaunchAgent，并立即启动后台服务。移动 App 后，再打开一次即可更新服务路径。

正常双击还会显示连接选择界面：

- 自动选择所有已连接的兼容设备；
- USB 有线 `258A:010C`；
- 蓝牙 5.0 `3554:FA07`。

界面会标明当前连接状态。选择会保存到 `~/Library/Application Support/T100Logo/preferred-connection`，后台服务在唤醒和重连后只控制所选连接；选择“自动”时处理所有已连接的兼容设备。

也可以使用 `local.codex.t100-logo-white.plist` 作为手动安装模板。模板假定 App 位于系统“应用程序”文件夹。

卸载后台服务：

```sh
./t100-logo --uninstall
```

该命令只停止并移除 LaunchAgent，不会删除 App 或日志。

## 自动构建与发布

GitHub Actions 会在每次推送和 Pull Request 时构建并校验 App，构建产物可从对应 workflow run 下载。推送 `v*` 标签时还会自动创建 GitHub Release：

```sh
git tag v0.1.0
git push origin v0.1.0
```

## 已知限制

- 已实测 SCC100；FMate98 由用户反馈验证有效。
- 当前只匹配 USB `258A:010C` 和 BLE `3554:FA07`；未测试其他蓝牙配置或 2.4G 接收器。
- 未安装 LaunchAgent 时，macOS 重新连接键盘后可能再次写入绿色状态。
