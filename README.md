<div align="center">

# ETS2 / ATS TruckersMP Chat Translator

[![Platform: Windows](https://img.shields.io/badge/Platform-Windows%20x64-blue?style=flat-square&logo=windows)](https://github.com/)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg?style=flat-square)](https://opensource.org/licenses/MIT)

**TruckersMP 实时聊天翻译插件**

[功能特点](#-功能特点) • [快速开始](#-快速开始) • [支持的翻译平台](#-支持的翻译平台) • [常见问题](#-日志与排错)

</div>

---

## 📖 简介

ETS2 / ATS / TruckersMP 聊天翻译插件。DLL 以 **SCS Telemetry** 插件形式加载，自动读取 `ETS2MP` 或 `ATSMP` 聊天日志，将最新聊天内容异步翻译后，实时显示在游戏内的透明覆盖窗口中。

自带一个现代化的 **Electron 管理器**，让你可以轻松实现一键安装、更新 DLL、以及可视化配置各种翻译平台参数！

> [!IMPORTANT]
> **安全与免责声明**：本项目为纯绿色辅助插件。软件**不修改游戏内存**、**不进行任何 API Hook**、**不读取游戏进程内存**。其运行机制仅为**异步读取 TruckersMP 本地生成的聊天日志文本**（`.txt` 格式）并通过 HTTP 请求调用第三方翻译接口，最后将翻译结果展示在透明的游戏覆盖窗口中。完全符合安全规范，请放心使用。有意见起诉我吧。

---

## ✨ 功能特点

- 🎮 **沉浸式覆盖窗口**：无缝贴合游戏，支持自定义字体大小、透明度和快捷键显示/隐藏。
- ⚡ **高性能异步翻译**：多 Worker 并发处理、同文本请求合并、队列限制、内存缓存；悬浮窗隐藏时不再发起翻译请求，减少无效 API 消耗。
- 🔄 **只读最新消息**：智能从文件末尾开始监听，拒绝历史日志刷屏。
- 🔌 **热重载支持**：修改配置后立即生效，下一条聊天自动重启翻译引擎。
- 🧠 **多引擎回退机制**：支持配置多个翻译 Provider，主通道失败自动回退备用通道，并带 Provider 级限流、快失败回退和失败冷却。
- 🖥️ **长文本悬浮窗优化**：原文和译文会按真实显示宽度换行，超长 ID / 链接 / 连续字符自动截断，滚动高度同步修正。
- ✅ **配置连通性测试**：管理器可直接测试当前配置的 Provider 是否可用，显示 HTTP 状态、耗时和错误摘要。
- 📖 **本地短语字典**：极速兜底常见简写和 TruckersMP 断线/重连提示，如 `sry`、`pls`、`gg`、`brb`。
- 🛠️ **一体化管理器**：自动识别 ETS2 / ATS 目录，支持 Steam 多库路径检测、一键安装/卸载、可视化配置 API Key，并支持多配置预设快速切换。
- 📝 **完善的日志记录**：HTTP 状态和响应预览写入 `game.log.txt`，方便排查。

---

## 🚀 快速开始

### 1. 安装与启动

1. **下载并运行安装包**：
   ```bash
   build\installer\ETS2-Chat-Translator-Manager-Setup-0.3.1.exe
   ```
2. **打开管理器**：启动 `ETS2 Chat Translator Manager`。
3. **定位游戏目录**：选择 ETS2 或 ATS，并自动识别或手动选择对应安装目录。
4. **一键安装**：点击 `安装 / 更新 DLL`。
5. **配置翻译平台**：在 `主翻译平台` 里选择你想用的接口协议或服务商，填写 `base_url`、`API Key`、`模型` 等字段（详见下方平台支持）。
6. **保存并生效**：点击 `保存配置`。
7. **进入游戏**：启动或重启 ETS2 / ATS 的 TruckersMP，享受跨越语言障碍的联机体验！

> 💡 **调试版运行**：如果你是开发者，可以直接运行 `build\ets2_chat_translator_app\ETS2 Chat Translator Manager.exe`。

### 2. 文件位置参考

- **DLL 安装位置**：`[游戏安装目录]\bin\win_x64\plugins\ets2_chat_translator.dll`
- **配置文件位置**：`[游戏安装目录]\bin\win_x64\plugins\ets2_chat_translator_config.json`

---

## 🌐 翻译 API 获取教程与配置指南

插件内置了极其强大的**语言检测分层处理**，确保针对 `source=auto` 能做出最准确的判断，不仅限于英语，完美适配 TruckersMP 的多语言环境（例如土耳其语、俄语、波兰语等）。以下是支持的各个平台介绍以及如何获取它们的 API Key 的详细教程。

<details>
<summary><b>🤖 OpenAI 协议 (支持各大兼容服务)</b></summary>

**获取地址与教程：**
- **OpenAI 官方**: 前往 [OpenAI Platform](https://platform.openai.com/api-keys) 注册并创建 API Key。
- **DeepSeek (深度求索)**: 极具性价比的国产大模型。前往 [DeepSeek 开放平台](https://platform.deepseek.com/) 注册并获取 API Key。Base URL 填 `https://api.deepseek.com/v1`。
- **硅基流动 (SiliconFlow)**: 提供各种开源大模型的高速 API。前往 [硅基流动控制台](https://cloud.siliconflow.cn/account/ak) 注册获取密钥。Base URL 填 `https://api.siliconflow.cn/v1`。
- **本地化部署 (Ollama / LM Studio)**: 下载并运行本地客户端，Base URL 填 `http://localhost:11434/v1` (Ollama) 或 `http://localhost:1234/v1` (LM Studio)。API Key 可留空。

**配置说明 (`kind: openai_compatible`)**:
- 接口形式为 `POST {base_url}/chat/completions`。
- `model` 需要填写服务商提供的具体模型名称，例如 `gpt-4o-mini` 或 `deepseek-chat`。

  ```json
  {
    "kind": "openai_compatible",
    "label": "DeepSeek",
    "enabled": true,
    "base_url": "https://api.deepseek.com/v1",
    "api_key": "你的API_KEY",
    "model": "deepseek-chat",
    "source": "auto",
    "target": "zh-CN"
  }
  ```
</details>

<details>
<summary><b>🧠 Anthropic 协议 (Claude)</b></summary>

**获取地址与教程：**
- **Anthropic 官方**: 前往 [Anthropic Console](https://console.anthropic.com/settings/keys) 获取 API Key。
- **说明**: 推荐使用 `claude-3-5-haiku-latest` 模型，速度快且翻译质量极佳。

**配置说明 (`kind: anthropic`)**:
- 接口形式为 `POST {base_url}/messages`。

  ```json
  {
    "kind": "anthropic",
    "label": "Claude",
    "enabled": true,
    "base_url": "https://api.anthropic.com/v1",
    "api_key": "你的API_KEY",
    "model": "claude-3-5-haiku-latest",
    "source": "auto",
    "target": "zh-CN"
  }
  ```
</details>

<details>
<summary><b>🌍 常用大厂云翻译 (DeepL, Google, Microsoft, 百度等)</b></summary>

**DeepL (`deepl`)**:
- **获取教程**: 前往 [DeepL API](https://www.deepl.com/pro-api) 注册并开通 API 服务（分免费版和 Pro 版）。然后在账户页获取 Authentication Key。
- **配置注意**: 免费版 Base URL 为 `https://api-free.deepl.com`，Pro 版为 `https://api.deepl.com`。

**Microsoft Translator (Azure) (`microsoft`)**:
- **获取教程**: 登录 [Azure 门户](https://portal.azure.com/)，搜索并创建 "Translator" (翻译) 资源。创建后在资源的 "Keys and Endpoint" 页面复制密钥 1，并记下你选择的 Region (如 `eastasia`)。
- **配置注意**: 需要在配置文件中的 `api_secret` 字段填写你对应的 Azure Region（如 `eastasia`、`global` 等）。

**百度翻译 (`baidu`)**:
- **获取教程**: 前往 [百度翻译开放平台](https://fanyi-api.baidu.com/)，注册成为开发者，进入管理控制台开通“通用翻译 API”。在“开发者信息”中可以找到 `APP ID` 和 `密钥`。
- **配置注意**: `api_key` 填 `APP ID`，`api_secret` 填 `密钥`。

**有道智云 (`youdao`)**:
- **获取教程**: 前往 [有道智云控制台](https://ai.youdao.com/)，创建“自然语言翻译”应用，获取 `应用 ID` (APP Key) 和 `应用密钥` (APP Secret)。
- **配置注意**: `api_key` 填应用 ID，`api_secret` 填应用密钥。

**腾讯云机器翻译 (`tencent`)**:
- **获取教程**: 前往 [腾讯云机器翻译控制台](https://console.cloud.tencent.com/tmt)，开通机器翻译服务，并在访问管理中获取 `SecretId` 和 `SecretKey`。
- **配置注意**: `api_key` 填 `SecretId`，`api_secret` 填 `SecretKey`，`model` 字段用作 Region（默认 `ap-guangzhou`）。

**阿里云机器翻译 (`aliyun`)**:
- **获取教程**: 前往 [阿里云机器翻译控制台](https://mt.console.aliyun.com/)，开通机器翻译服务，并在 RAM/AccessKey 管理中获取 `AccessKey ID` 和 `AccessKey Secret`。
- **配置注意**: `api_key` 填 `AccessKey ID`，`api_secret` 填 `AccessKey Secret`，`model` 字段用作 Scene（默认 `general`），默认 Base URL 为 `https://mt.cn-hangzhou.aliyuncs.com`。

**火山翻译 (`volcengine`)**:
- **获取教程**: 前往 [火山引擎机器翻译控制台](https://console.volcengine.com/translate)，开通文本翻译服务，并在访问控制中获取 `Access Key ID` 和 `Secret Access Key`。
- **配置注意**: `api_key` 填 `Access Key ID`，`api_secret` 填 `Secret Access Key`，`model` 字段用作 Region（默认 `cn-north-1`），默认 Base URL 为 `https://translate.volcengineapi.com`。

**Google Cloud Translate (`google_cloud`)**:
- **获取教程**: 登录 [Google Cloud Console](https://console.cloud.google.com/)，启用 "Cloud Translation API"，然后在凭据管理中创建 API Key。
- **Base URL**: 默认为 `https://translation.googleapis.com`。
</details>

<details>
<summary><b>🆓 免费 / 兜底翻译 (无需 Key)</b></summary>

如果你暂时没有申请到任何 API Key，或者想留一个失败时的兜底通道，可以使用以下免费服务：

- **MyMemory (`mymemory`)**: 完全免费的公共接口。不需要填写 API Key 和 Base URL。对长句或方言/俚语（如土耳其语、俄语拼音）效果一般，但无需 Key。
- **LibreTranslate (`libretranslate`)**: 开源机器翻译。有些公共实例无需 Key，只需找到一个可用的公共 Base URL 填入即可。
</details>

---

## ⚙️ 配置文件说明 (`config.json`)

你可以通过管理器界面修改，或者直接编辑 JSON：

| 字段 | 说明 | 示例 |
| --- | --- | --- |
| `target_lang` | 默认目标语言 | `zh-CN` |
| `overlay_hotkey` | 悬浮窗显示/隐藏快捷键 | `Ctrl+Shift+T` |
| `workers` | 并发翻译 Worker 数 (1-32) | `8` |
| `queue_limit` | 等待翻译队列上限 | `1000` |
| `cache_limit` | 翻译缓存条数 | `1500` |
| `timeout_ms` | HTTP 超时 (毫秒)，运行时会限制在 `1500-6000` | `5000` |
| `font_size` | 覆盖窗口字体大小 | `18` |
| `providers` | 翻译服务提供商数组，**按顺序尝试，失败自动回退** | `[{...}, {...}]` |

> 配置文件支持热重载：保存后，下一条聊天日志到来时会自动重启翻译引擎，并同步覆盖窗口字体、快捷键和布局参数。

---

## 🛠️ 构建项目

本项目需要 Windows x64、Visual Studio 2019/2022 Build Tools、Node.js / npm 环境。

```bat
# 运行构建脚本
build.bat --no-pause
```

<details>
<summary><b>查看构建输出目录</b></summary>

```text
build\ets2_chat_translator.dll                        # 核心 DLL
build\installer\ETS2-Chat-Translator-Manager-Setup-0.3.1.exe # 安装包
build\ets2_chat_translator_app\ETS2 Chat Translator Manager.exe # 免安装管理器
```
*注：安装包使用 NSIS 制作，DLL 会自动打包进管理器的 `resources` 目录。*
</details>

---

## 🔍 日志与排错

如果遇到问题，请检查游戏日志文件：
`C:\Users\<你的用户名>\Documents\Euro Truck Simulator 2\game.log.txt`

ATS 对应日志为：
`C:\Users\<你的用户名>\Documents\American Truck Simulator\game.log.txt`

搜索以下关键字快速定位：`[ChatTranslator]`、`[TranslateHTTP]`、`[Translate]`。

翻译日志会记录 Provider、请求时间、耗时、重试次数和响应预览，例如 `at=08:52:31.123 ms=420 attempts=1`。

**常见 HTTP 错误对照：**

| 错误代码 / 提示 | 可能的原因 |
| --- | --- |
| `HTTP 400` | 模型名不被该 endpoint 支持，请检查大小写和模型 ID |
| `HTTP 401` | API Key 无效，或者 Key 不属于当前接口/区域 |
| `HTTP 403` | 鉴权失败、服务未开通、或缺少必需的参数 |
| `HTTP 429` | 请求过快或额度耗尽，请降低 `workers` 或等待恢复 |
| `cannot parse response`| 返回 JSON 格式与解析逻辑不匹配，请查看日志中的 payload 预览 |
| `returned original...`| 翻译平台原样返回了源文本，插件已拒绝将其视为有效翻译 |

---

## 🔮 后续规划

- [x] Provider 级别的细粒度限流和自动重试机制
- [x] 翻译耗时统计与请求时间戳记录
- [x] 智能合并相同文本的翻译请求
- [x] Provider 错误熔断与自动恢复功能
- [x] 游戏内热更新覆盖窗口的字体和布局
- [x] 接入腾讯云机器翻译（TC3-HMAC-SHA256 签名）
- [x] 接入阿里云、火山翻译等带签名的国内 API
- [x] 支持 ATS / ATSMP 日志监听和安装目标
- [x] App 设置快捷键控制悬浮窗显示/隐藏

---

## 📄 开源协议

本项目基于 [MIT](https://opensource.org/licenses/MIT) 协议开源。

你可以自由地使用、修改和分发本项目的代码，即使是用于商业用途，只需在副本中包含原作者的版权声明和许可声明即可。

<div align="center">
  <i>Made with ❤️ for the TruckersMP Community</i>
</div>
