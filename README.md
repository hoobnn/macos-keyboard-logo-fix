# Keyboard Logo Fix for macOS

Keyboard Logo Fix 是一个 macOS HID 小工具，用于解除系统写入的绿色指示灯覆盖，并恢复键盘内部保存的用户自定义 LOGO 灯效。程序不会指定灯效颜色或动画。

## 已验证键盘

- SCC100；
- FMate98。

两个型号均使用程序当前支持的 HID 标识与输出报告协议：

- USB 有线：`VID:PID 258A:010C`；
- Bluetooth Low Energy：`VID:PID 3554:FA07`，可能显示为 `T100 5.0`。

这里的 `T100` 是设备向 macOS 暴露的控制器识别信息，并非实体键盘型号。其他使用相同 VID/PID 和报告协议的键盘也可能适用，但仍需实机确认；使用不同标识或协议的设备目前不会被程序识别。

## 下载与安装

从仓库的 [Releases](../../releases) 页面下载最新的 `Keyboard-Logo-Fix-0.2.0-macOS.zip`：

1. 解压并将 `Keyboard Logo Fix.app` 拖入“应用程序”文件夹；
2. 双击 App，在连接选择窗口中选择自动、USB 或蓝牙；
3. 首次运行如被 macOS 拦截，在“系统设置 → 隐私与安全性”中选择“仍要打开”；
4. 在“系统设置 → 隐私与安全性 → 输入监控”中允许该 App，然后重新打开。

从 `0.1.x` 升级时，新版首次启动会停止并移除旧的 `local.codex.t100-logo-white` 后台服务，并继续读取旧版保存的连接偏好。旧的 `T100 Logo 白色呼吸.app` 文件不会被自动删除，可以手动移入废纸篓。

## 工作方式

App 首次打开时会安装用户级 LaunchAgent：

```text
~/Library/LaunchAgents/com.ikuyu.keyboard-logo-fix.plist
```

后台程序随用户登录启动并保持运行，平时只监听兼容键盘连接和 Mac 唤醒事件，不会持续向键盘发送指令。发生以下事件时，它会短暂发送恢复报告：

- 后台服务启动；
- 兼容键盘连接或重新连接；
- Mac 从睡眠中唤醒。

每次触发会以 50 ms 间隔发送约 3 秒，即约 60 次。重复发送用于覆盖 macOS 在设备初始化期间再次写入绿色指示状态的情况。报告只解除指示灯覆盖，使键盘恢复此前由用户手动设置并保存在键盘中的 LOGO 灯效。

程序不会刷写固件、修改按键映射、记录按键或访问网络。

连接偏好与日志分别保存在：

```text
~/Library/Application Support/KeyboardLogoFix/preferred-connection
~/Library/Logs/KeyboardLogoFix.log
```

## 命令行使用

后台模式：

```sh
./keyboard-logo-fix --daemon
```

手动发送指定秒数，范围为 1–300：

```sh
./keyboard-logo-fix 10
```

卸载新版和旧版后台服务：

```sh
./keyboard-logo-fix --uninstall
```

卸载命令不会删除 App、连接偏好或日志。

## 从源码构建

需要安装 Xcode Command Line Tools。默认生成同时支持 Apple Silicon 和 Intel Mac 的通用程序：

```sh
make
./keyboard-logo-fix 10
```

打包 App：

```sh
make app
```

产物位于：

```text
dist/Keyboard Logo Fix.app
```

`com.ikuyu.keyboard-logo-fix.plist` 是 App 位于系统“应用程序”文件夹时可使用的手动 LaunchAgent 模板。

## 自动构建与发布

GitHub Actions 会在每次推送和 Pull Request 时构建并校验 App，构建产物可从对应 workflow run 下载。推送 `v*` 标签时会自动创建 GitHub Release：

```sh
git tag v0.2.0
git push origin v0.2.0
```

## 已知限制

- SCC100 已实测，FMate98 由用户反馈验证有效；
- 当前只匹配 USB `258A:010C` 和 BLE `3554:FA07`；
- 尚未验证其他蓝牙配置或 2.4G 接收器；
- App 使用临时签名且未经过 Apple 公证，其他用户首次运行时需要手动允许。
