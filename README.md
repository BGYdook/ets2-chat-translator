# ETS2 TruckersMP Chat Translator

ETS2 / TruckersMP 聊天翻译插件。DLL 以 SCS Telemetry 插件形式加载，读取 TruckersMP 聊天日志，把新聊天异步翻译后显示在游戏覆盖窗口里。管理器是 JavaScript/Electron 程序，用来安装 DLL、编辑配置和保存翻译平台参数。

## 功能

- ETS2 / TruckersMP 聊天覆盖窗口
- 只读取新增聊天，不回放历史日志
- 翻译配置热重载：保存配置后，下一条新聊天会重启翻译引擎
- 多 worker 并发、队列限制、缓存
- 本地短语字典兜底，例如 `sry`、`pls`、`gg`、`brb`
- HTTP 状态和响应预览写入 `game.log.txt`
- Electron 管理器：自动识别 ETS2、安装/卸载 DLL、生成配置

## 快速使用

1. 运行安装包：

   ```text
   build\installer\ETS2-Chat-Translator-Manager-Setup-0.1.0.exe
   ```

2. 打开 `ETS2 Chat Translator Manager`。

3. 选择或自动识别 ETS2 安装目录。

4. 点击 `安装 / 更新 DLL`。

5. 在 `主翻译平台` 里选择接口协议或服务商，填写 `base_url`、`API Key`、`模型` 等字段。

6. 点击 `保存配置`。

7. 启动或重启 ETS2 / TruckersMP。

调试版管理器也可以直接运行：

```text
build\ets2_chat_translator_app\ETS2 Chat Translator Manager.exe
```

DLL 安装位置：

```text
[ETS2 安装目录]\bin\win_x64\plugins\ets2_chat_translator.dll
```

配置文件位置：

```text
[ETS2 安装目录]\bin\win_x64\plugins\ets2_chat_translator_config.json
```

## 构建

需要 Windows x64、Visual Studio 2019/2022 Build Tools、Node.js/npm。

```bat
build.bat --no-pause
```

输出：

```text
build\ets2_chat_translator.dll
build\installer\ETS2-Chat-Translator-Manager-Setup-0.1.0.exe
build\ets2_chat_translator_app\ETS2 Chat Translator Manager.exe
```

说明：

- 管理器使用 NSIS 安装包。安装包是单个 exe，安装后释放完整 Electron 文件夹。
- DLL 会打包进管理器安装目录的 `resources\ets2_chat_translator.dll`。
- 旧的 portable 单 exe 不再使用，因为它曾出现缺少 `ffmpeg.dll` 的运行错误。

## DLL 如何翻译

运行时链路：

```text
SCS Telemetry 加载 DLL
  -> AppRuntime
      -> ChatTailer       读取 C:\Users\<你>\Documents\ETS2MP\logs\chat_YYYY_MM_DD_log.txt
      -> ChatPanel        创建游戏覆盖窗口
      -> TranslateEngine  队列、缓存、并发 worker、provider 回退
      -> HttpAgent        WinHTTP 请求翻译 API
      -> SettingsStore    读取 JSON 配置
```

处理步骤：

1. DLL 启动后读取 `ets2_chat_translator_config.json`。
2. `ChatTailer` 定位当天 TruckersMP 聊天日志，并从文件末尾开始监听，避免旧消息刷屏。
3. 新聊天进入覆盖窗口。
4. 如果文本不是中文，提交到翻译队列。
5. 先查本地短语字典和缓存。
6. 按 `providers` 顺序调用翻译接口。
7. 某个 provider 返回有效目标语言后，写入缓存并更新覆盖窗口。
8. 如果配置文件被保存过，下一条聊天到来时会重载翻译引擎。

注意：字体大小影响已创建窗口，通常需要重启游戏才完全生效。翻译平台、API Key、base_url、模型、worker、超时等翻译参数支持热重载。

## 配置结构

示例：

```json
{
  "target_lang": "zh-CN",
  "workers": 8,
  "queue_limit": 1000,
  "cache_limit": 1500,
  "timeout_ms": 10000,
  "font_size": 18,
  "providers": [
    {
      "kind": "anthropic",
      "label": "MiMo Anthropic",
      "enabled": true,
      "base_url": "https://token-plan-sgp.xiaomimimo.com/anthropic/v1",
      "api_key": "YOUR_KEY",
      "model": "mimo-v2.5",
      "source": "auto",
      "target": "zh-CN"
    },
    {
      "kind": "mymemory",
      "label": "MyMemory",
      "enabled": true,
      "source": "auto",
      "target": "zh-CN"
    }
  ]
}
```

通用字段：

| 字段 | 说明 |
| --- | --- |
| `target_lang` | 默认目标语言，例如 `zh-CN` |
| `workers` | 并发翻译 worker 数，1-32 |
| `queue_limit` | 等待翻译队列上限 |
| `cache_limit` | 翻译缓存条数 |
| `timeout_ms` | HTTP 接收超时 |
| `font_size` | 覆盖窗口字体大小 |
| `providers` | 按顺序尝试，失败后回退到下一个 |

Provider 字段：

| 字段 | 说明 |
| --- | --- |
| `kind` | Provider 类型 |
| `label` | 日志和界面显示名 |
| `enabled` | 是否启用 |
| `base_url` | 接口基础地址 |
| `api_key` | API Key、App ID 或订阅 Key |
| `api_secret` | 签名密钥、App Secret，或 Microsoft region |
| `model` | 大模型名称 |
| `source` | 源语言，通常填 `auto` |
| `target` | 目标语言 |

## 语言检测

聊天不一定是英语。插件对 `source=auto` 做了分层处理：

- OpenAI / Anthropic 大模型：不传固定源语言，提示词明确要求识别任意语言。
- DeepL / Google Cloud / Microsoft / LibreTranslate：尽量让服务端自动识别源语言。
- MyMemory：接口需要 `source|target`，插件会先做轻量本地语言猜测，再传入源语言。
- 百度翻译：`source=auto` 时也会先用本地猜测，避免土耳其语、俄语等被错误当作英语。

本地猜测会识别常见脚本和字符特征，例如西里尔文、希腊文、土耳其语特殊字符、波兰语/捷克语/罗马尼亚语/西葡德法意等常见特征词。它不是完整语言检测器，但比固定 `en` 更适合 TruckersMP 多语言聊天。

## 支持的平台和协议

### OpenAI 协议

`kind`:

```text
openai_compatible
openai
chat_completions
```

请求方式：`POST {base_url}/chat/completions`

适合：

- OpenAI
- OpenAI-compatible 网关
- DeepSeek、硅基流动、OpenRouter 等兼容服务
- Ollama / LM Studio 等本地服务
- MiMo 的 OpenAI 兼容入口

示例：

```json
{
  "kind": "openai_compatible",
  "label": "OpenAI",
  "enabled": true,
  "base_url": "https://api.openai.com/v1",
  "api_key": "YOUR_KEY",
  "model": "gpt-4o-mini",
  "source": "auto",
  "target": "zh-CN"
}
```

本地服务如果不需要 key，可以把 `api_key` 留空。

### Anthropic 协议

`kind`:

```text
anthropic
claude
anthropic_messages
```

请求方式：`POST {base_url}/messages`

适合：

- Anthropic Claude 官方 API
- Anthropic-compatible 网关
- MiMo 的 Anthropic 兼容入口

示例：

```json
{
  "kind": "anthropic",
  "label": "Claude",
  "enabled": true,
  "base_url": "https://api.anthropic.com/v1",
  "api_key": "YOUR_KEY",
  "model": "claude-3-5-haiku-latest",
  "source": "auto",
  "target": "zh-CN"
}
```

MiMo Anthropic 示例：

```json
{
  "kind": "anthropic",
  "label": "MiMo Anthropic",
  "enabled": true,
  "base_url": "https://token-plan-sgp.xiaomimimo.com/anthropic/v1",
  "api_key": "YOUR_TOKEN_PLAN_KEY",
  "model": "mimo-v2.5",
  "source": "auto",
  "target": "zh-CN"
}
```

### DeepL

`kind`:

```text
deepl
```

示例：

```json
{
  "kind": "deepl",
  "label": "DeepL",
  "enabled": true,
  "base_url": "https://api-free.deepl.com",
  "api_key": "YOUR_DEEPL_KEY",
  "source": "auto",
  "target": "zh-CN"
}
```

免费版通常使用 `https://api-free.deepl.com`，Pro 版通常使用 `https://api.deepl.com`。

### Google Cloud Translate

`kind`:

```text
google_cloud
google_translate
```

示例：

```json
{
  "kind": "google_cloud",
  "label": "Google Cloud",
  "enabled": true,
  "base_url": "https://translation.googleapis.com",
  "api_key": "YOUR_GOOGLE_API_KEY",
  "source": "auto",
  "target": "zh-CN"
}
```

### Microsoft Translator

`kind`:

```text
microsoft
azure_translator
```

示例：

```json
{
  "kind": "microsoft",
  "label": "Microsoft Translator",
  "enabled": true,
  "base_url": "https://api.cognitive.microsofttranslator.com",
  "api_key": "YOUR_AZURE_TRANSLATOR_KEY",
  "api_secret": "eastasia",
  "source": "auto",
  "target": "zh-CN"
}
```

`api_secret` 在这里填 Azure region，例如 `eastasia`、`southeastasia`。部分全局资源可能不需要 region。

### 百度翻译

`kind`:

```text
baidu
baidu_translate
```

示例：

```json
{
  "kind": "baidu",
  "label": "百度翻译",
  "enabled": true,
  "base_url": "https://fanyi-api.baidu.com",
  "api_key": "YOUR_APP_ID",
  "api_secret": "YOUR_SECRET_KEY",
  "source": "auto",
  "target": "zh-CN"
}
```

签名方式：`MD5(appid + q + salt + secret_key)`。

### 有道智云

`kind`:

```text
youdao
```

示例：

```json
{
  "kind": "youdao",
  "label": "有道智云",
  "enabled": true,
  "base_url": "https://openapi.youdao.com",
  "api_key": "YOUR_APP_KEY",
  "api_secret": "YOUR_APP_SECRET",
  "source": "auto",
  "target": "zh-CN"
}
```

签名方式：有道 v3 SHA-256 签名。

### LibreTranslate

`kind`:

```text
libretranslate
libre_translate
```

示例：

```json
{
  "kind": "libretranslate",
  "label": "LibreTranslate",
  "enabled": true,
  "base_url": "https://libretranslate.com",
  "api_key": "",
  "source": "auto",
  "target": "zh-CN"
}
```

不同 LibreTranslate 实例可能需要 API Key，也可能不需要。

### MyMemory

`kind`:

```text
mymemory
```

免费兜底接口。对短句、土耳其语/俄语俚语等效果不稳定，但没有 key 时可以兜底。

### Andeer

`kind`:

```text
andeer
```

旧免费接口。当前公开接口返回缺少 apikey 的 403，默认建议关闭，除非你有可用 key 或兼容接口。

## API Key 获取入口

这些入口可能会随平台调整而变化，优先从对应平台控制台进入。

| 平台 | 获取位置 |
| --- | --- |
| OpenAI | `https://platform.openai.com/api-keys` |
| Anthropic | `https://console.anthropic.com/settings/keys` |
| DeepL | `https://www.deepl.com/account/summary`，开通 DeepL API 后查看 Authentication Key |
| Google Cloud Translate | `https://console.cloud.google.com/apis/credentials`，启用 Cloud Translation API 后创建 API Key |
| Microsoft Translator | `https://portal.azure.com`，创建 Azure AI Translator 资源，查看 Keys and Endpoint |
| 百度翻译 | `https://fanyi-api.baidu.com`，开发者中心创建通用翻译 API 应用，获取 APP ID 和密钥 |
| 有道智云 | `https://ai.youdao.com`，自然语言翻译服务创建应用，获取 appKey 和 appSecret |
| LibreTranslate | 看你使用的实例说明；官方/公共实例可能在账户页提供 API Key |
| MiMo Token Plan | 使用 Token Plan 控制台提供的 key；OpenAI 协议填 `/v1`，Anthropic 协议填 `/anthropic/v1` |

## 日志与排错

游戏日志：

```text
C:\Users\<你>\Documents\Euro Truck Simulator 2\game.log.txt
```

搜索：

```text
[ChatTranslator]
[TranslateHTTP]
[Translate]
```

常见错误：

| 日志 | 含义 |
| --- | --- |
| `HTTP 400 Not supported model` | 模型名不被该 endpoint 支持，检查大小写和模型 ID |
| `HTTP 401 Invalid API Key` | key 错误，或 key 不属于这个 endpoint/区域 |
| `HTTP 403` | 鉴权失败、服务未开通、或接口需要额外参数 |
| `HTTP 429 Too Many Requests` | 限流或额度不足，降低 workers 或等待恢复 |
| `returned original/non-target text` | provider 返回原文，插件拒绝当成有效翻译 |
| `cannot parse response` | 返回 JSON 格式和当前解析逻辑不匹配，查看 payload 预览 |

## 项目结构

```text
include/
  scs_telemetry.h
src/
  dllmain.cpp             SCS 插件入口
  app_runtime.*           插件生命周期、热重载、模块协调
  chat_panel.*            Win32 覆盖窗口
  chat_tailer.*           TruckersMP 日志 tailer
  translate_engine.*      并发翻译引擎和 provider
  http_agent.*            WinHTTP GET/POST
  settings_store.*        JSON 配置读取与默认配置
  text_codec.*            UTF-8、URL、JSON 字符串辅助
  win_paths.*             文档目录、ETS2 安装目录发现
manager/
  package.json            Electron/NSIS 打包配置
  src/                    管理器界面和 IPC
```

## 后续方向

- Provider 级限流和重试
- 请求时间戳、耗时统计
- 同文本请求合并
- Provider 熔断与恢复
- 腾讯云、阿里云、火山翻译等带签名 API
- Overlay 热更新字体和窗口布局
