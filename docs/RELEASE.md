# 发布流程

打 `v*` 标签即触发 CI：构建 → Developer ID 签名 → Apple 公证 → 生成 zip/dmg → 发布 GitHub Release。

签名与公证逻辑放在 [`hoobnn/ci-workflows`](https://github.com/hoobnn/ci-workflows)，本仓库只描述"怎么构建"。

## 一次性配置

### 1. 导出 Developer ID Application 证书

钥匙串访问 → 找到 `Developer ID Application: <你的名字> (<TeamID>)` → 右键导出为 `.p12`，设一个密码。

```bash
base64 -i DeveloperID.p12 | pbcopy
```

> **同名证书会导致签名失败。** 证书续期后旧证书若未删除，`security find-identity -v -p codesigning`
> 会出现两条一模一样的记录，`codesign --sign "<名字>"` 报 `ambiguous`。
> 此时用证书指纹（SHA-1，`find-identity` 输出最左边那串）代替名字即可。

### 2. 创建 App Store Connect API Key

App Store Connect → 用户和访问 → 集成 → 密钥 → 新建，角色选 **Developer**。
`.p8` 只能下载一次。

```bash
base64 -i AuthKey_XXXXXXXX.p8 | pbcopy
```

用 API Key 而不是 Apple ID + 专用密码：不绑定个人账号、不受 2FA 影响、可单独吊销。

### 3. 配置仓库 Secrets

`Settings → Secrets and variables → Actions → Secrets`：

| 名称 | 内容 |
|---|---|
| `APPLE_CERT_APPLICATION_P12_BASE64` | 步骤 1 的 base64 |
| `APPLE_CERT_APPLICATION_P12_PASSWORD` | 导出 `.p12` 时设的密码 |
| `APPLE_NOTARY_KEY_ID` | API Key ID，如 `2X9R4HXF34` |
| `APPLE_NOTARY_ISSUER_ID` | Issuer ID（UUID） |
| `APPLE_NOTARY_KEY_P8_BASE64` | 步骤 2 的 base64 |

做 `.pkg` 分发时再加（Developer ID **Installer** 证书）：

| 名称 | 内容 |
|---|---|
| `APPLE_CERT_INSTALLER_P12_BASE64` | Installer 证书 base64 |
| `APPLE_CERT_INSTALLER_P12_PASSWORD` | 其导出密码 |

### 4. 配置仓库 Variables

`Settings → Secrets and variables → Actions → Variables`：

| 名称 | 内容 |
|---|---|
| `APPLE_TEAM_ID` | 如 `8FUPL8QHFH` |
| `APPLE_SIGN_IDENTITY_APPLICATION` | `Developer ID Application: 名字 (TeamID)`，同名冲突时填 SHA-1 指纹 |

做 `.pkg` 时再加 `APPLE_SIGN_IDENTITY_INSTALLER`。

Team ID 和 identity 放 Variables 而非 Secrets：它们本来就印在签名产物里（`codesign -dv` 可见），
存成 Secret 反而会让 Actions 日志把它们打码成 `***`，排查签名问题时看不见。

## 发布一个版本

```bash
# 1. 改版本号（CFBundleShortVersionString 与 CFBundleVersion）
vim Info.plist

# 2. 提交并打标签，标签必须与 Info.plist 一致，否则 CI 会拒绝
git commit -am "🔖 chore: 发布 0.3.0"
git tag v0.3.0
git push && git push --tags
```

CI 会校验 tag 与 `CFBundleShortVersionString` 是否一致，不一致直接失败。

## 本地构建

```bash
make app                    # ad-hoc 签名，仅本机可用
```

用真实证书签名（产物具备公证资格）：

```bash
APPLE_SIGN_IDENTITY_APPLICATION="187F48A4...（指纹或完整名称）" make app
codesign -dv "dist/Keyboard Logo Fix.app"   # flags 应含 runtime
```

## 排查

**公证被拒** — CI 会自动拉取 `notarytool log` 打印完整原因。最常见的两种：

- 缺 hardened runtime：签名时漏了 `--options runtime`
- 缺时间戳：漏了 `--timestamp`

**验证已发布产物**

```bash
spctl --assess --type execute --verbose=2 "/Applications/Keyboard Logo Fix.app"
xcrun stapler validate "/Applications/Keyboard Logo Fix.app"
```

## 维护

- Developer ID 证书有效期 5 年，到期后更新 `APPLE_CERT_APPLICATION_P12_BASE64`
  和 `APPLE_SIGN_IDENTITY_APPLICATION`（指纹会变）
- API Key 不过期，泄露时在 App Store Connect 吊销并重新生成
- 每个仓库各自维护这套 secrets；新增 macOS 项目时复制本文档的配置步骤
