#include "settings_store.h"
#include "text_codec.h"

#include <windows.h>
#include <wincrypt.h>

#include <fstream>
#include <sstream>
#include <algorithm>
#include <vector>

namespace
{
const wchar_t* kSecretPrefix = L"enc:dpapi:";

bool StartsWith(const std::wstring& value, const wchar_t* prefix)
{
    return value.rfind(prefix, 0) == 0;
}

std::vector<BYTE> Base64Decode(const std::wstring& value)
{
    DWORD needed = 0;
    if (!CryptStringToBinaryW(value.c_str(), 0, CRYPT_STRING_BASE64, nullptr, &needed, nullptr, nullptr) || needed == 0) {
        return {};
    }
    std::vector<BYTE> out(needed);
    if (!CryptStringToBinaryW(value.c_str(), 0, CRYPT_STRING_BASE64, out.data(), &needed, nullptr, nullptr)) {
        return {};
    }
    out.resize(needed);
    return out;
}

std::wstring DecryptSecret(std::wstring value)
{
    if (!StartsWith(value, kSecretPrefix)) return value;

    std::wstring payload = value.substr(wcslen(kSecretPrefix));
    std::vector<BYTE> encrypted = Base64Decode(payload);
    if (encrypted.empty()) return L"";

    DATA_BLOB in{};
    in.pbData = encrypted.data();
    in.cbData = (DWORD)encrypted.size();
    DATA_BLOB out{};
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &out)) {
        return L"";
    }

    std::string utf8((const char*)out.pbData, (const char*)out.pbData + out.cbData);
    LocalFree(out.pbData);
    return text::FromUtf8(utf8);
}

std::wstring Jstr(const std::string& json, const std::string& key, const std::wstring& fallback = L"")
{
    std::wstring v = text::JsonString(json, key);
    return v.empty() ? fallback : v;
}

int Jint(const std::string& json, const std::string& key, int fallback)
{
    std::string needle = "\"" + key + "\"";
    size_t p = json.find(needle);
    if (p == std::string::npos) return fallback;
    p = json.find(':', p + needle.size());
    if (p == std::string::npos) return fallback;
    try { return std::stoi(json.substr(p + 1)); } catch (...) { return fallback; }
}

bool Jbool(const std::string& json, const std::string& key, bool fallback)
{
    std::string needle = "\"" + key + "\"";
    size_t p = json.find(needle);
    if (p == std::string::npos) return fallback;
    p = json.find(':', p + needle.size());
    if (p == std::string::npos) return fallback;
    ++p;
    while (p < json.size() && (unsigned char)json[p] <= ' ') ++p;
    if (json.compare(p, 4, "true") == 0) return true;
    if (json.compare(p, 5, "false") == 0) return false;
    return fallback;
}

std::vector<std::string> ProviderBlocks(const std::string& json)
{
    std::vector<std::string> blocks;
    size_t p = json.find("\"providers\"");
    if (p == std::string::npos) return blocks;
    p = json.find('[', p);
    if (p == std::string::npos) return blocks;

    bool inString = false;
    bool escape = false;
    int depth = 0;
    size_t start = std::string::npos;
    for (++p; p < json.size(); ++p) {
        char ch = json[p];
        if (inString) {
            if (escape) escape = false;
            else if (ch == '\\') escape = true;
            else if (ch == '"') inString = false;
            continue;
        }
        if (ch == '"') inString = true;
        else if (ch == '{') {
            if (depth == 0) start = p;
            ++depth;
        } else if (ch == '}') {
            --depth;
            if (depth == 0 && start != std::string::npos) {
                blocks.push_back(json.substr(start, p - start + 1));
                start = std::string::npos;
            }
        } else if (ch == ']' && depth == 0) {
            break;
        }
    }
    return blocks;
}
}

namespace settings
{
AppSettings Defaults()
{
    AppSettings s;
    s.providers.push_back({ L"mymemory", L"MyMemory", true, L"", L"", L"", L"auto", L"zh-CN", L"" });
    s.providers.push_back({ L"andeer", L"Andeer", false, L"", L"", L"", L"auto", L"zh-CN", L"" });
    return s;
}

AppSettings Load(const std::wstring& path)
{
    AppSettings s = Defaults();
    std::ifstream f(path, std::ios::binary);
    if (!f) return s;

    std::ostringstream buf;
    buf << f.rdbuf();
    std::string json = buf.str();
    if (json.empty()) return s;

    s.runtime.targetLanguage = Jstr(json, "target_lang", s.runtime.targetLanguage);
    s.runtime.overlayHotkey = Jstr(json, "overlay_hotkey", s.runtime.overlayHotkey);
    s.runtime.workerCount = (std::max)(1, (std::min)(32, Jint(json, "workers", s.runtime.workerCount)));
    s.runtime.queueLimit = (size_t)(std::max)(50, Jint(json, "queue_limit", (int)s.runtime.queueLimit));
    s.runtime.cacheLimit = (size_t)(std::max)(100, Jint(json, "cache_limit", (int)s.runtime.cacheLimit));
    s.runtime.timeoutMs = (std::max)(1500, (std::min)(30000, Jint(json, "timeout_ms", s.runtime.timeoutMs)));
    s.runtime.fontSize = (std::max)(12, (std::min)(28, Jint(json, "font_size", s.runtime.fontSize)));
    s.runtime.overlayOpacity = (std::max)(0, (std::min)(100, Jint(json, "overlay_opacity", s.runtime.overlayOpacity)));

    auto blocks = ProviderBlocks(json);
    if (!blocks.empty()) {
        s.providers.clear();
        for (const std::string& b : blocks) {
            ProviderSettings p;
            p.kind = Jstr(b, "kind", Jstr(b, "type"));
            p.label = Jstr(b, "label", Jstr(b, "name", p.kind));
            p.enabled = Jbool(b, "enabled", true);
            p.baseUrl = Jstr(b, "base_url");
            p.apiKey = DecryptSecret(Jstr(b, "api_key"));
            p.model = Jstr(b, "model");
            p.sourceLanguage = Jstr(b, "source", Jstr(b, "source_lang", L"auto"));
            p.targetLanguage = Jstr(b, "target", Jstr(b, "target_lang", s.runtime.targetLanguage));
            p.apiSecret = DecryptSecret(Jstr(b, "api_secret", Jstr(b, "secret_key")));
            if (!p.kind.empty()) s.providers.push_back(p);
        }
    }
    if (s.providers.empty()) s = Defaults();
    return s;
}

bool WriteDefaultFile(const std::wstring& path)
{
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f <<
R"({
  "target_lang": "zh-CN",
  "overlay_hotkey": "Ctrl+Shift+T",
  "workers": 8,
  "queue_limit": 1000,
  "cache_limit": 1500,
  "timeout_ms": 10000,
  "font_size": 18,
  "overlay_opacity": 98,
  "providers": [
    {
      "kind": "openai_compatible",
      "label": "LLM Translator",
      "enabled": false,
      "base_url": "https://api.example.com/v1",
      "api_key": "",
      "model": "gpt-4o-mini",
      "source": "auto",
      "target": "zh-CN",
      "api_secret": ""
    },
    {
      "kind": "deepseek",
      "label": "DeepSeek",
      "enabled": false,
      "base_url": "https://api.deepseek.com",
      "api_key": "",
      "model": "deepseek-v4-flash",
      "source": "auto",
      "target": "zh-CN",
      "api_secret": ""
    },
    {
      "kind": "anthropic",
      "label": "Anthropic",
      "enabled": false,
      "base_url": "https://api.anthropic.com/v1",
      "api_key": "",
      "model": "claude-3-5-haiku-latest",
      "source": "auto",
      "target": "zh-CN",
      "api_secret": ""
    },
    {
      "kind": "baidu",
      "label": "Baidu",
      "enabled": false,
      "base_url": "https://fanyi-api.baidu.com",
      "api_key": "",
      "api_secret": "",
      "source": "auto",
      "target": "zh-CN"
    },
    {
      "kind": "youdao",
      "label": "Youdao",
      "enabled": false,
      "base_url": "https://openapi.youdao.com",
      "api_key": "",
      "api_secret": "",
      "source": "auto",
      "target": "zh-CN"
    },
    {
      "kind": "tencent",
      "label": "Tencent Cloud TMT",
      "enabled": false,
      "base_url": "https://tmt.tencentcloudapi.com",
      "api_key": "",
      "api_secret": "",
      "model": "ap-guangzhou",
      "source": "auto",
      "target": "zh-CN"
    },
    {
      "kind": "aliyun",
      "label": "Aliyun Machine Translation",
      "enabled": false,
      "base_url": "https://mt.cn-hangzhou.aliyuncs.com",
      "api_key": "",
      "api_secret": "",
      "model": "general",
      "source": "auto",
      "target": "zh-CN"
    },
    {
      "kind": "volcengine",
      "label": "Volcengine Translate",
      "enabled": false,
      "base_url": "https://translate.volcengineapi.com",
      "api_key": "",
      "api_secret": "",
      "model": "cn-north-1",
      "source": "auto",
      "target": "zh-CN"
    },
    {
      "kind": "microsoft",
      "label": "Microsoft Translator",
      "enabled": false,
      "base_url": "https://api.cognitive.microsofttranslator.com",
      "api_key": "",
      "api_secret": "",
      "source": "auto",
      "target": "zh-CN"
    },
    {
      "kind": "mymemory",
      "label": "MyMemory",
      "enabled": true,
      "source": "auto",
      "target": "zh-CN"
    },
    {
      "kind": "andeer",
      "label": "Andeer",
      "enabled": false,
      "source": "auto",
      "target": "zh-CN"
    }
  ]
}
)";
    return true;
}
}
