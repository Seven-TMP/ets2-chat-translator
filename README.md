<div align="center">

# TruckersMP 聊天翻译

<p align="center">
  <img src="https://img.shields.io/badge/Platform-Windows%20x64-blue?style=for-the-badge&logo=windows" alt="Platform">
  <img src="https://img.shields.io/badge/Language-C++%20%2F%20JavaScript-orange?style=for-the-badge&logo=c%2B%2B" alt="Language">
  <img src="https://img.shields.io/badge/Framework-Electron-478aef?style=for-the-badge&logo=electron" alt="Framework">
  <img src="https://img.shields.io/badge/License-MIT-green.svg?style=for-the-badge" alt="License">
</p>

<h3>TruckersMP 实时聊天翻译插件</h3>

[功能特点](#-功能特点) • [工作原理](#-工作原理) • [快速开始](#-快速开始) • [API 配置指南](#-翻译-api-获取教程与配置指南) • [常见问题与排错](#-日志与排错)

</div>

---

## 📖 简介

**ETS2 / ATS TruckersMP Chat Translator** 是一款专为《欧洲卡车模拟 2》（ETS2）和《美洲卡车模拟》（ATS）的 TruckersMP 联机模式打造的**实时聊天翻译插件**。

本插件的 DLL 模块以标准的 **SCS Telemetry 插件** 形式加载，在后台以极低的资源占用异步读取游戏聊天日志，并将翻译结果实时、无缝地呈现在游戏内的透明悬浮窗中。

项目自带一个**现代化的 Electron 桌面管理器**，支持一键安装、更新 DLL、多套配置切换以及可视化 API 连通性测试。

> [!IMPORTANT]
> **🛡️ 安全与免责声明**
> 本项目为**纯绿色辅助插件**。
> * **不修改游戏内存**，**不进行任何 API Hook**，**不读取游戏进程内存**。
> * 其运行机制仅为**异步读取 TruckersMP 本地生成的聊天日志文本**（`.txt` 格式），通过 HTTP 请求调用您配置的第三方翻译接口，再通过游戏 Overlay 渲染层将翻译结果展示在悬浮窗中。
> * 完全符合官方安全规范，绝无封号风险，请放心使用。如果有意见起诉我吧。

---

## 🌀 工作原理

以下是本系统的数据流与模块协作关系，展示了其安全且高效的架构设计：

```mermaid
graph TD
    subgraph Game ["🎮 游戏运行环境 (ETS2/ATS + TruckersMP)"]
        A[游戏内玩家聊天] -->|自动输出日志| B("📄 聊天日志 (chat_log.txt)")
        F["📺 游戏内透明悬浮窗 (DirectX/GDI)"]
    end

    subgraph Plugin ["🔌 C++ 插件 (ets2_chat_translator.dll)"]
        B -->|智能尾部监听| C[只读日志流]
        C -->|文本提取与清洗| D[本地高频字典过滤]
        D -->|未匹配文本| E[异步翻译队列]
        E -->|多 Worker 并发请求| G{主/备翻译引擎}
        H[内存二级缓存] <-->|避免重复请求| E
    end

    subgraph API ["🌐 云端翻译服务 (HTTPS)"]
        G -->|主通道失败 自动回退| I[备用翻译通道]
        G -->|支持| J[DeepSeek / Claude / OpenAI / 百度 / 腾讯等]
    end

    subgraph Manager ["🖥️ 管理器 (Electron App)"]
        K[图形化设置界面] -->|一键测试与保存| L("⚙️ 配置文件 (config.json)")
        L -.->|热重载监控| Plugin
        M[GitHub Release API] -->|一键安装/更新| Plugin
    end

    I -->|返回译文| E
    J -->|返回译文| E
    E -->|翻译结果| F
    D -->|匹配常用语| F
```

---

## ✨ 功能特点

* **🎮 沉浸式覆盖窗口**：悬浮窗完美贴合游戏画面。支持自定义字体大小、背景透明度、快捷键显示/隐藏，并会自动保存并记住上一次的窗口位置与大小。
* **⚡ 高性能异步翻译**：多 Worker 并发处理、相同文本请求合并、队列限制、内存二级缓存。当悬浮窗隐藏时，自动挂起翻译请求，将 API 消耗降至最低。
* **🔄 智能日志流监听**：从文件末尾开始增量监听，绝不读取历史冗余日志，拒绝刷屏。
* **🔌 配置实时热重载**：修改配置文件后，DLL 自动感知并热重载，无需重启游戏，下一条消息立即应用新配置。
* **🧠 多引擎熔断与回退**：支持配置多个 API 提供商。当主通道遭遇限流 (429)、故障 (500) 或网络超时时，自动降级回退至备用通道，并带熔断冷却保护。
* **🛡️ 长文本自适应排版**：原文与译文按真实显示宽度换行，超长 ID / 链接 / 连续字符自动截断，防止排版溢出，滚动高度同步更新。
* **🛠️ 一体化管理器 (Manager)**：
  - 自动识别 ETS2 / ATS 游戏路径（支持多 Steam 库路径自动检索）。
  - DLL 一键安装与卸载，无痛升级。
  - API 连通性一键测试，提前排查 Key 错误。
  - 支持配置多套翻译预设，并内置多条 GitHub 代理下载加速节点。

---

## 🚀 快速开始

### 1. 安装与配置步骤

```
 ┌─────────────────┐      ┌─────────────────┐      ┌─────────────────┐
 │  下载并运行安装包 │ ───> │  定位游戏安装目录 │ ───> │  安装并配置 API  │
 └─────────────────┘      └─────────────────┘      └─────────────────┘
                                                                    │
 ┌─────────────────┐      ┌─────────────────┐                       │
 │   启动游戏畅玩   │ <─── │   保存配置生效  │ <─────────────────────┘
 └─────────────────┘      └─────────────────┘
```

1. **下载并运行安装包**：
   双击运行 `build\installer\ETS2-Chat-Translator-Manager-Setup-0.3.7.exe` 进行安装。
2. **打开管理器**：启动安装好的 `ETS2 Chat Translator Manager`。
3. **识别游戏目录**：选择对应的游戏（ETS2 或 ATS），管理器会尝试自动定位。若未找到，可手动选择游戏主程序所在的 bin 目录。
4. **一键部署 DLL**：点击 `安装 / 更新 DLL` 按钮。
5. **配置翻译平台**：在主翻译平台面板中选择所需的服务商，填写 API Key、模型等信息（详见下文 API 配置指南）。
6. **保存并生效**：点击 `保存配置`，管理器会自动在正确位置生成配置文件。
7. **启动游戏**：像往常一样启动 ETS2 或 ATS 并进入 TruckersMP 联机，即可在游戏中看到翻译悬浮窗。

> [!TIP]
> **开发者调试模式**：
> 如果您是开发者，可以直接运行 `build\ets2_chat_translator_app\ETS2 Chat Translator Manager.exe` 进行免安装调试。

### 2. 文件位置参考

| 文件类型 | 默认部署路径 |
| :--- | :--- |
| **插件 DLL 路径** | `[游戏安装目录]\bin\win_x64\plugins\ets2_chat_translator.dll` |
| **配置文件 JSON** | `[游戏安装目录]\bin\win_x64\plugins\ets2_chat_translator_config.json` |

---

## 🌐 翻译 API 获取教程与配置指南

插件内置了极具鲁棒性的**多国语言自动检测与分层处理**，能精准识别 TruckersMP 复杂的国际化聊天环境（支持英语、土耳其语、俄语、波兰语、德语等）。

<details>
<summary><b>🤖 1. OpenAI 兼容协议 (支持第三方中转与本地大模型)</b></summary>

推荐：低延迟、高性价比。

**适用场景：**
* **OpenAI 官方**: 访问 [OpenAI Platform](https://platform.openai.com/api-keys) 获取。
* **硅基流动 (SiliconFlow)**: 提供超多开源大模型的高速中转。访问 [硅基流动官网](https://cloud.siliconflow.cn/account/ak) 注册，Base URL 填 `https://api.siliconflow.cn/v1` .
* **小米 MiMo 开放平台（强力推荐）**: 我在用 MiMo 开放平台，体验小米顶尖模型 MiMo V2.5 等。通过我的邀请码注册：双方各得 ¥10 API 体验金 + 首单 9 折。邀请码：`TM9LQB`。注册：[https://platform.xiaomimimo.com?ref=TM9LQB](https://platform.xiaomimimo.com?ref=TM9LQB)（注册后自动填入 · 体验金 40 天有效）。Base URL 填 `https://api.xiaomimimo.com/v1`。
* **本地化部署 (Ollama / LM Studio)**: 本地离线翻译。Ollama 的 Base URL 通常为 `http://localhost:11434/v1`，API Key 可留空或填任意字符。

**配置模板 (`kind: "openai_compatible"`)：**

```json
{
  "kind": "openai_compatible",
  "label": "MiMo V2.5 Pro",
  "enabled": true,
  "base_url": "https://api.xiaomimimo.com/v1",
  "api_key": "sk-xxxxxxxxxxxxxxxxxxxxxxxx",
  "model": "mimo-v2.5-pro",
  "source": "auto",
  "target": "zh-CN"
}
```
</details>

<details>
<summary><b>🧠 2. Anthropic Claude 接口</b></summary>

高端机翻译体验。

**获取步骤：**
1. 访问 [Anthropic Console](https://console.anthropic.com/settings/keys) 注册并生成 Key。
2. 推荐使用轻量且高质量的 `claude-4-5-haiku-latest` 模型。

**配置模板 (`kind: "anthropic"`)：**

```json
{
  "kind": "anthropic",
  "label": "Claude",
  "enabled": true,
  "base_url": "https://api.anthropic.com/v1",
  "api_key": "sk-ant-xxxxxxxxxxxxxxxxxxxxxxxx",
  "model": "claude-4-5-haiku-latest",
  "source": "auto",
  "target": "zh-CN"
}
```
</details>

<details>
<summary><b>🤖 3. DeepSeek 官方接口</b></summary>

**获取步骤：**
1. 访问 [DeepSeek 开放平台](https://platform.deepseek.com/) 注册账号。
2. 进入 API Keys 页面，创建并复制您的密钥。

**配置模板 (`kind: "deepseek"`)：**
* **Base URL**: `https://api.deepseek.com`
* **推荐模型**: `deepseek-v4-flash` (速度极快，适合聊天翻译)
* *注：插件会自动为 DeepSeek 禁用思考（thinking）过程，以换取最低的延迟。*

```json
{
  "kind": "deepseek",
  "label": "DeepSeek",
  "enabled": true,
  "base_url": "https://api.deepseek.com",
  "api_key": "sk-xxxxxxxxxxxxxxxxxxxxxxxx",
  "model": "deepseek-v4-flash",
  "source": "auto",
  "target": "zh-CN"
}
```
</details>

<details>
<summary><b>🌍 4. 传统云翻译平台 (百度、腾讯、火山、DeepL 等)</b></summary>

传统翻译 API 通常稳定性极佳，且都有一定的每月免费额度。

* **DeepL (`deepl`)**:
  - 免费版 Base URL: `https://api-free.deepl.com`
  - 专业版 Base URL: `https://api.deepl.com`
* **微软 Azure 翻译 (`microsoft`)**:
  - 需要在 `api_secret` 字段中填写您服务所在的 Azure 区域（如 `eastasia`、`global`）。
* **百度翻译 (`baidu`)**:
  - `api_key` 填 **APP ID**，`api_secret` 填 **应用密钥**。
  - 插件会自动把 `zh-CN` 映射为百度专用的 `zh` 代码，避免 `INVALID_TO_PARAM` 报错。
* **腾讯云翻译 (`tencent`)**:
  - `api_key` 填 **SecretId**，`api_secret` 填 **SecretKey**，`model` 填区域（如 `ap-guangzhou`）。
* **火山翻译 (`volcengine`)**:
  - `api_key` 填 **Access Key ID**，`api_secret` 填 **Secret Access Key**。
</details>

<details>
<summary><b>🆓 5. 免费兜底通道 (无需 API Key)</b></summary>

如果您没有上述任何平台的 Key，可以使用内置的免 Key 兜底通道：

* **MyMemory (`mymemory`)**: 免费公共接口，适合简单句子的日常翻译。
* **LibreTranslate (`libretranslate`)**: 开源的去中心化翻译服务，需填写可用的公共节点 Base URL。
</details>

---

## ⚙️ 配置文件参数详解 (`config.json`)

配置文件位于插件目录中，常用配置字段如下：

| 参数字段 | 类型 | 默认值 | 作用说明 |
| :--- | :--- | :--- | :--- |
| `target_lang` | String | `zh-CN` | 目标翻译语言（译文语言） |
| `overlay_hotkey` | String | `Ctrl+Shift+T` | 控制游戏内悬浮窗显示与隐藏的全局快捷键 |
| `workers` | Integer | `8` | 异步请求的并发 Worker 数量（推荐范围：`2` - `16`） |
| `queue_limit` | Integer | `1000` | 等待队列的最大消息缓存数，超出后将丢弃最旧的消息 |
| `cache_limit` | Integer | `1500` | 翻译结果在内存中的最大缓存数量，相同文本不重复请求 API |
| `timeout_ms` | Integer | `5000` | HTTP 请求超时时间（毫秒，系统会限制在 `1500` - `6000` 之间） |
| `font_size` | Integer | `18` | 游戏内悬浮窗的字体大小 (px) |
| `providers` | Array | `[...]` | 翻译提供商列表。**系统会按数组顺序尝试，首位失败自动降级至下一位** |

> `api_key`、`api_secret`、`secret_key` 会在管理器保存时自动加密写入配置文件，格式类似 `enc:dpapi:...`。管理器读取时会自动解密并显示明文，插件运行时也会自动解密使用。加密使用 Windows 当前用户 DPAPI，通常只能由同一台电脑的同一 Windows 用户解密。

---

## 🛠️ 本地开发与构建

构建本项目需要具备 Windows x64 开发环境，并安装 Visual Studio 构建工具和 Node.js。

### 1. 环境依赖准备
* **Visual Studio 2022 Build Tools** (勾选 **使用 C++ 的桌面开发** 工作负载，并确保包含 **MSVC v143** 和 **Windows 10/11 SDK**)。
* **Node.js LTS** (在终端中能成功运行 `node -v` 及 `npm -v`)。

### 2. 编译步骤
在项目根目录下，双击运行或在命令行执行 `build.bat` 脚本。该脚本会自动利用 `vswhere` 定位 MSVC 编译器并完成 Electron 和 DLL 的构建：

```bat
# 编译并打包 (无需手动按回车确认)
build.bat --no-pause
```

### 3. 构建输出目录树
构建成功后，生成的文件将存放在 `build\` 目录下：

```text
build/
├── ets2_chat_translator.dll                        # 核心 C++ 插件 DLL
├── ets2_chat_translator_app/                       # 绿色版管理器 (绿色免安装)
│   └── ETS2 Chat Translator Manager.exe
└── installer/
    └── ETS2-Chat-Translator-Manager-Setup-0.3.7.exe # 独立安装包 (集成 NSIS)
```

---

## 🔍 日志与排错

遇到不翻译、翻译很慢、悬浮窗不显示、API 报错等问题时，请先看日志。日志里会记录插件初始化、配置热重载、翻译队列、HTTP 状态和响应预览。

### 1. 日志位置

| 游戏 | 日志文件 |
| :--- | :--- |
| ETS2 | `C:\Users\<您的用户名>\Documents\Euro Truck Simulator 2\game.log.txt` |
| ATS | `C:\Users\<您的用户名>\Documents\American Truck Simulator\game.log.txt` |

### 2. 搜索关键字

| 关键字 | 用途 |
| :--- | :--- |
| `[ChatTranslator]` | 插件初始化、配置热重载、悬浮窗状态、是否跳过翻译 |
| `[Translate]` | 翻译入队、缓存命中、本地字典、Provider 降级、失败原因 |
| `[TranslateHTTP]` | HTTP 状态码、接口耗时、响应预览 |

### 3. 提交 Issues

如果按上面方法还无法解决，请到 [GitHub Issues](https://github.com/Seven-TMP/ets2-chat-translator/issues) 提交问题，并尽量带上这些信息：

* 游戏类型：ETS2 或 ATS
* 插件版本：例如 `v0.3.6`
* 使用的翻译平台：例如 DeepSeek、MiMo、百度、腾讯云等
* 问题现象：不显示、翻译失败、延迟很高、401、429 等
* 复现步骤：进入游戏后做了什么、出现问题的大概时间
* 日志片段：从 `game.log.txt` 中复制相关的 `[ChatTranslator]`、`[Translate]`、`[TranslateHTTP]` 行

> 提交前请先隐藏 API Key、Secret、Token 等敏感内容。

### 4. 常见 HTTP 错误

| 错误代码 / 提示 | 可能原因 | 推荐解决方法 |
| :--- | :--- | :--- |
| **HTTP 400** | 模型名称不受支持 | 检查配置中 `model` 字段拼写及大小写是否符合平台规范。 |
| **HTTP 401** | API Key 无效 | 检查 Key 是否复制完整，或是否该 Key 属于当前选择的服务商。 |
| **HTTP 403** | 鉴权失败或服务未开通 | 确认您的账户是否有欠费，或对应的翻译 API 服务是否已在后台启用。 |
| **HTTP 429** | 触发频率限制或额度耗尽 | 降低 `workers`，或切换到额度更高的 API / 备用 Provider。 |
| **INVALID_TO_PARAM** | 目标语言参数不支持 | 百度翻译特有错误，请确保您的目标语言设置为 `zh-CN`，插件会自动转译。 |
| **cannot parse response** | 返回的 JSON 格式无法解析 | 接口可能返回了错误页面（如 HTML 报错网关），请检查 base_url。 |
| **returned original...** | 接口直接返回了原文 | 翻译平台认为源语言和目标语言一致，或翻译接口判定无需翻译。 |

---

## 🧾 历史版本更新

### 🚀 v0.3.7
* **⚡ 翻译并发优化**：优化 Provider 请求调度，减少高峰聊天时的串行等待。
* **🚦 智能节流退避**：正常请求允许更高并发，遇到 `429` 或频率限制后再自动降速。
* **🧵 高峰队列优化**：队列积压时减少单 Provider 额外重试，避免旧请求长期占住 Worker。
* **🔐 密钥加密保存**：管理器保存配置和预设时会加密 `api_key` / `api_secret`，插件读取时自动解密使用。
* **🧾 排错文档优化**：日志与排错章节新增 Issues 提交流程和日志片段要求。

### 🚀 v0.3.6
* **🎨 管理器 UI 焕新**：调整整体视觉风格、顶部标题、配置预览区和按钮状态，界面更紧凑清晰。
* **📋 配置预览增强**：新增一键复制配置预览内容，方便快速备份和排查。
* **🌐 镜像列表修复**：浏览器预览与正式 APP 保持同一套完整镜像列表，避免只显示少量节点。
* **📏 镜像面板优化**：加高镜像列表区域并保留滚动，测速排行能展示更多节点。

### 🚀 v0.3.5
* **✨ 在线更新系统**：管理器新增 GitHub Release 检查更新，支持自动抓取最新 `version.json` 并展示详细的中文更新日志。
* **⚡ 下载加速节点**：内置 GitHub 代理节点完整并发测速与选择，支持通过代理直接下载并启动新版本安装包。
* **🛠️ 百度翻译加固**：针对百度翻译的特殊语言码映射机制进行深度加固，彻底杜绝 `INVALID_TO_PARAM` 报错。
* **🌐 配置库扩充**：在 `README` 与管理器中新增 MiMo V2.5 推荐配置和专有注册邀请链接。

### 🚀 v0.3.4
* **🎨 悬浮窗显示调优**：修复了悬浮窗整体透明度会连带字体一起变淡的问题，现在透明度只影响背景底色，字体保持清晰可读。
* **💬 聊天布局精简**：去除了聊天列表中的冗余英文原文副行，仅保留翻译译文，使屏幕展示更加简洁明了。
* **🔍 搜索栏重构**：将搜索框移动到窗口左侧标题栏区域，完美适配窄屏及多分辨率场景。

---

## 🔮 后续规划

- [x] Provider 级别的细粒度限流和自动重试机制
- [x] 翻译耗时统计与请求时间戳记录
- [x] 智能合并相同文本的翻译请求
- [x] Provider 错误熔断与自动恢复功能
- [x] 游戏内热更新覆盖窗口的字体 and 布局
- [x] 接入腾讯云机器翻译（TC3-HMAC-SHA256 签名）
- [x] 接入阿里云、火山翻译等带签名的国内 API
- [x] 支持 ATS / ATSMP 日志监听和安装目标
- [x] App 设置快捷键控制悬浮窗显示/隐藏

---

## 📄 开源协议

本项目采用 [MIT](https://opensource.org/licenses/MIT) 许可协议开源。您可以自由使用、修改和分发，但请保留原作者的版权声明和许可声明。

<div align="center">

*Made with ❤️ for the TruckersMP Community*

</div>
