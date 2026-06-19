# ETS2 / ATS TruckersMP 聊天翻译使用教程

本文用于发布在 https://ets2.seventmp.cn/，面向第一次安装 ETS2 / ATS TruckersMP Chat Translator 的玩家。

## 这个工具能做什么

ETS2 / ATS TruckersMP Chat Translator 是一个实时聊天翻译插件。它会读取 TruckersMP 在本地生成的聊天日志，把聊天内容发送到你配置的翻译 API，然后在游戏画面上显示中文翻译悬浮窗。

它不修改游戏内存，不 Hook 游戏进程，不读取游戏进程内存。插件以 SCS Telemetry 插件形式加载，主要工作是读取本地日志和显示悬浮窗。

## 准备工作

你需要准备：

- Windows 10 / Windows 11 64 位系统
- 已安装 Euro Truck Simulator 2 或 American Truck Simulator
- 已安装 TruckersMP
- 一个可用的翻译 API 配置，推荐 MiMo、DeepSeek、OpenAI 兼容接口、百度翻译或腾讯云机器翻译

如果只是先体验，可以先使用免费兜底通道 MyMemory，但效果和稳定性不如正式 API。

## 下载与安装

1. 打开项目 Releases 页面：
   https://github.com/Seven-TMP/ets2-chat-translator/releases

2. 下载最新版安装包：
   `ETS2-Chat-Translator-Manager-Setup-x.x.x.exe`

3. 安装并打开 `ETS2 Chat Translator Manager`。

4. 在管理器里选择游戏：
   - Euro Truck Simulator 2
   - American Truck Simulator

5. 点击路径检测。如果自动识别失败，可以手动选择游戏安装目录。

6. 点击安装 DLL。安装位置通常是：
   `[游戏目录]\bin\win_x64\plugins\ets2_chat_translator.dll`

## 推荐配置：MiMo V2.5

MiMo 是 OpenAI 兼容协议，管理器里选择 `OpenAI 协议` 即可。

我在用 MiMo 开放平台，体验小米顶尖模型 MiMo V2.5 等。通过我的邀请码注册：双方各得 ¥10 API 体验金 + 首单 9 折。邀请码：`TM9LQB`。

注册链接：
https://platform.xiaomimimo.com?ref=TM9LQB

注册后自动填入，体验金 40 天有效。

管理器配置示例：

```json
{
  "kind": "openai_compatible",
  "label": "MiMo V2.5 Pro",
  "enabled": true,
  "base_url": "https://api.xiaomimimo.com/v1",
  "api_key": "你的 API Key",
  "model": "mimo-v2.5-pro",
  "source": "auto",
  "target": "zh-CN"
}
```

## 其他常用 API 平台入口

- OpenAI：https://platform.openai.com/api-keys
- DeepSeek：https://platform.deepseek.com/
- Anthropic Claude：https://platform.claude.com/settings/keys
- 硅基流动：https://cloud.siliconflow.cn/account/ak
- 百度翻译开放平台：https://fanyi-api.baidu.com/
- 腾讯云机器翻译：https://cloud.tencent.com/product/tmt
- 阿里云机器翻译：https://www.aliyun.com/product/ai/base_alimt
- 火山引擎机器翻译：https://www.volcengine.com/product/machine-translation
- 有道智云文本翻译：https://ai.youdao.com/product-fanyi-text.s
- DeepL API：https://www.deepl.com/pro-api
- Azure Translator：https://azure.microsoft.com/products/ai-services/ai-translator
- Google Cloud Translation：https://cloud.google.com/translate
- Ollama：https://ollama.com/
- LM Studio：https://lmstudio.ai/

## 本地或远端 Ollama 配置

Ollama 使用 OpenAI 兼容地址时，Base URL 填：

```text
http://localhost:11434/v1
```

如果 Ollama 在另一台电脑上，Base URL 填：

```text
http://对方IP:11434/v1
```

不要填 `/api`。`/api` 是 Ollama 原生接口，管理器的 OpenAI 兼容配置应该使用 `/v1`。

远端 Ollama 需要在运行 Ollama 的电脑上允许外部访问：

```bat
setx OLLAMA_HOST "0.0.0.0:11434"
netsh advfirewall firewall add rule name="Ollama 11434" dir=in action=allow protocol=TCP localport=11434
taskkill /F /IM ollama.exe
taskkill /F /IM "ollama app.exe"
```

然后重新打开 Ollama。

如果对方在运营商大内网或 CGNAT 下，直接端口转发可能无效。光猫或路由器 WAN IP 如果是 `100.64.x.x - 100.127.x.x`、`10.x.x.x`、`172.16.x.x - 172.31.x.x`、`192.168.x.x`，就不是真公网。建议使用 Tailscale、ZeroTier、frp 或找运营商申请公网 IPv4。

## 保存配置与测试

1. 在管理器里填好 Provider 配置。
2. 点击 `测试配置`。
3. 看到 HTTP 200 或测试成功后，再点击 `保存配置`。
4. 进入 TruckersMP 后打开聊天，悬浮窗会显示翻译结果。

建议配置：

- 云端 LLM：`workers` 可设置为 `3` 到 `6`
- 百度、腾讯、DeepL 等传统翻译：`workers` 可设置为 `6` 到 `8`
- 本地或远端 Ollama：`workers` 建议设置为 `1`
- 12B / 27B 本地模型：`timeout_ms` 建议设置为 `30000`

## 悬浮窗设置

管理器支持：

- 快捷键显示或隐藏悬浮窗
- 调整字体大小
- 调整背景透明度
- 记住上一次窗口位置和大小
- 搜索当天 TruckersMP 日志

悬浮窗隐藏时，插件会暂停翻译请求，减少 API 消耗。

## 常见问题

### 安装失败：EBUSY resource busy or locked

这通常表示游戏正在运行，DLL 被占用。

处理方法：

1. 退出 ETS2 / ATS 和 TruckersMP。
2. 确认任务管理器里没有游戏进程残留。
3. 回到管理器重新安装 DLL。

### 百度翻译 INVALID_TO_PARAM

这是百度翻译的目标语言参数错误。请在配置里使用：

```text
target: zh-CN
```

新版插件会自动把 `zh-CN` 映射成百度需要的 `zh`。

### Ollama 测试超时

常见原因：

- Base URL 写成了 `/api`，应改成 `/v1`
- 模型首次加载较慢
- 远端网络不通
- 对方电脑 Ollama 没有监听 `0.0.0.0:11434`
- 对方宽带处于 CGNAT，大内网无法直接端口转发

测试地址：

```bat
curl http://127.0.0.1:11434/api/tags
```

如果是远端：

```bat
curl http://对方IP:11434/api/tags
```

### 翻译一直显示“翻译中”

通常是请求卡住或 API 响应太慢。可以尝试：

- 降低 `workers`
- 提高 `timeout_ms`
- 换更快的模型
- 增加备用 Provider
- 本地 Ollama 使用更小模型，例如 12B 而不是 27B

### 翻译结果很怪

游戏聊天经常包含缩写、拼写错误、土耳其语、俄语、玩家 ID、表情和短噪音。插件内置了一部分本地短语修正，但 API 模型仍可能误判。

遇到明显误翻，可以带日志提交 Issues：
https://github.com/Seven-TMP/ets2-chat-translator/issues

请附上：

- 使用的版本号
- 使用的翻译平台和模型
- 管理器测试配置结果
- `game.log.txt` 里相关的 `[ChatTranslator]`、`[Translate]`、`[TranslateHTTP]` 日志片段

## 更新软件

管理器支持通过 GitHub Release 检测更新，也支持选择 GitHub 镜像代理。如果直连 GitHub 慢，可以在管理器里测速并选择可用镜像。

更新前建议先退出游戏，避免 DLL 正在被占用导致安装失败。

## 卸载

在管理器中点击卸载 DLL，或手动删除：

```text
[游戏目录]\bin\win_x64\plugins\ets2_chat_translator.dll
[游戏目录]\bin\win_x64\plugins\ets2_chat_translator_config.json
```

如果只想重装，不需要删除配置文件，直接用管理器重新安装 DLL 即可。
