#include "translate_engine.h"
#include "text_codec.h"

#include <windows.h>
#include <wincrypt.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cwctype>
#include <thread>

namespace
{
std::wstring ShortPhraseFallback(const std::wstring& input);
std::wstring TrimChatEdgePunctuation(std::wstring value);

bool SplitUrl(const std::wstring& url, std::wstring& host, INTERNET_PORT& port, std::wstring& prefix, bool& tls)
{
    std::wstring u = url;
    tls = true;
    port = 443;
    if (u.rfind(L"https://", 0) == 0) {
        u = u.substr(8);
    } else if (u.rfind(L"http://", 0) == 0) {
        tls = false;
        port = 80;
        u = u.substr(7);
    }
    size_t slash = u.find(L'/');
    std::wstring hostPort = slash == std::wstring::npos ? u : u.substr(0, slash);
    prefix = slash == std::wstring::npos ? L"" : u.substr(slash);
    while (!prefix.empty() && prefix.back() == L'/') prefix.pop_back();

    size_t colon = hostPort.rfind(L':');
    if (colon != std::wstring::npos) {
        host = hostPort.substr(0, colon);
        try { port = (INTERNET_PORT)std::stoi(hostPort.substr(colon + 1)); } catch (...) { return false; }
    } else {
        host = hostPort;
    }
    return !host.empty();
}

bool IsChineseChar(wchar_t ch)
{
    return (ch >= 0x4E00 && ch <= 0x9FFF)
        || (ch >= 0x3400 && ch <= 0x4DBF)
        || (ch >= 0x3000 && ch <= 0x303F)
        || (ch >= 0xFF00 && ch <= 0xFFEF);
}

bool HasChinese(const std::wstring& value)
{
    for (wchar_t ch : value) {
        if (IsChineseChar(ch)) return true;
    }
    return false;
}

int ChineseCharCount(const std::wstring& value)
{
    int count = 0;
    for (wchar_t ch : value) {
        if (ch >= 0x4E00 && ch <= 0x9FFF) ++count;
    }
    return count;
}

std::wstring LowerAscii(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        if (ch >= 0xFF01 && ch <= 0xFF5E) ch = (wchar_t)(ch - 0xFEE0);
        if (ch == 0x3000) ch = L' ';
        return (wchar_t)towlower(ch);
    });
    return value;
}

std::wstring NormalizeChatForDictionary(const std::wstring& value)
{
    std::wstring out;
    out.reserve(value.size());
    for (wchar_t ch : value) {
        if (ch == 0xFEFF || ch == 0x200B || ch == 0x200C || ch == 0x200D || ch == 0x2060) continue;
        if (ch >= 0xFF01 && ch <= 0xFF5E) ch = (wchar_t)(ch - 0xFEE0);
        if (ch == 0x3000) ch = L' ';
        out.push_back((wchar_t)towlower(ch));
    }
    return text::Trim(out);
}

std::wstring SqueezeRepeatedAsciiLetters(const std::wstring& value, int keep)
{
    std::wstring out;
    out.reserve(value.size());
    wchar_t last = 0;
    int run = 0;
    for (wchar_t ch : value) {
        bool asciiLetter = (ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z');
        if (asciiLetter && ch == last) {
            ++run;
            if (run <= keep) out.push_back(ch);
            continue;
        }
        last = asciiLetter ? ch : 0;
        run = asciiLetter ? 1 : 0;
        out.push_back(ch);
    }
    return out;
}

std::wstring CompareKey(const std::wstring& value)
{
    std::wstring out;
    for (wchar_t ch : LowerAscii(value)) {
        if (iswalnum(ch)) out.push_back(ch);
    }
    return out;
}

std::vector<std::wstring> AlphaWords(const std::wstring& value)
{
    std::vector<std::wstring> words;
    std::wstring token;
    for (wchar_t ch : LowerAscii(value)) {
        if ((ch >= L'a' && ch <= L'z') || iswalpha(ch)) {
            token.push_back(ch);
            continue;
        }
        if (token.size() >= 2) words.push_back(token);
        token.clear();
    }
    if (token.size() >= 2) words.push_back(token);
    return words;
}

bool ContainsWord(const std::wstring& value, const std::wstring& word)
{
    if (word.empty()) return false;
    std::wstring lower = LowerAscii(value);
    for (size_t pos = lower.find(word); pos != std::wstring::npos; pos = lower.find(word, pos + 1)) {
        bool leftOk = pos == 0 || !iswalnum(lower[pos - 1]);
        size_t end = pos + word.size();
        bool rightOk = end >= lower.size() || !iswalnum(lower[end]);
        if (leftOk && rightOk) return true;
    }
    return false;
}

bool LeavesTooMuchSourceText(const std::wstring& input, const std::wstring& output)
{
    std::vector<std::wstring> sourceWords = AlphaWords(input);
    if (sourceWords.size() < 2) return false;

    int retained = 0;
    for (const auto& word : sourceWords) {
        if (ContainsWord(output, word)) ++retained;
    }

    int chinese = ChineseCharCount(output);
    if (sourceWords.size() >= 3 && retained >= (std::max)(2, (int)sourceWords.size() / 2)) return true;
    return retained >= 2 && chinese <= 4;
}

bool StartsWithMentionPrefix(const std::wstring& value, std::wstring& prefix)
{
    std::wstring trimmed = text::Trim(value);
    if (trimmed.empty() || trimmed[0] != L'@') return false;

    size_t i = 1;
    while (i < trimmed.size() && !iswspace(trimmed[i])) ++i;
    if (i <= 1) return false;

    size_t end = i;
    size_t p = i;
    while (p < trimmed.size() && iswspace(trimmed[p])) ++p;
    if (p < trimmed.size() && trimmed[p] == L'(') {
        size_t close = trimmed.find(L')', p + 1);
        if (close != std::wstring::npos) {
            bool digits = close > p + 1;
            for (size_t k = p + 1; k < close; ++k) {
                if (!iswdigit(trimmed[k])) {
                    digits = false;
                    break;
                }
            }
            if (digits) end = close + 1;
        }
    }

    prefix = trimmed.substr(0, end);
    return true;
}

std::wstring PreserveMentionPrefix(const std::wstring& input, const std::wstring& output)
{
    std::wstring prefix;
    if (!StartsWithMentionPrefix(input, prefix)) return output;
    if (output.find(prefix) != std::wstring::npos) return output;

    std::wstring mentionName = prefix;
    size_t space = mentionName.find(L' ');
    if (space != std::wstring::npos) mentionName = mentionName.substr(0, space);

    std::wstring bareName = mentionName;
    if (!bareName.empty() && bareName.front() == L'@') bareName.erase(bareName.begin());
    std::wstring trimmedOut = text::Trim(output);
    auto startsWithNoCase = [](const std::wstring& value, const std::wstring& needle) {
        return !needle.empty()
            && value.size() >= needle.size()
            && LowerAscii(value.substr(0, needle.size())) == LowerAscii(needle);
    };
    auto trimAfterLeadingName = [&](const std::wstring& name) -> std::wstring {
        if (!startsWithNoCase(trimmedOut, name)) return L"";
        std::wstring rest = text::Trim(trimmedOut.substr(name.size()));
        if (!rest.empty() && rest.front() == L'(') {
            size_t close = rest.find(L')');
            if (close != std::wstring::npos) rest = text::Trim(rest.substr(close + 1));
        }
        while (!rest.empty() && (rest.front() == L':' || rest.front() == L',' || rest.front() == L'-')) {
            rest = text::Trim(rest.substr(1));
        }
        return rest;
    };

    std::wstring rest = trimAfterLeadingName(mentionName);
    if (rest.empty()) rest = trimAfterLeadingName(bareName);
    if (!rest.empty()) return prefix + L" " + rest;

    return prefix + L" " + text::Trim(output);
}

bool LooksUntranslated(const std::wstring& input, const std::wstring& output, const RuntimeConfig& runtime)
{
    std::wstring out = text::Trim(output);
    if (out.empty()) return true;
    if (CompareKey(input) == CompareKey(out)) return true;
    if ((runtime.targetLanguage == L"zh-CN" || runtime.targetLanguage == L"zh" || runtime.targetLanguage == L"zh-Hans")
        && !HasChinese(out)) {
        // 如果输出没有中文但包含的文本和输入完全不同，可能是其他语言的意译
        // 仍判定为失败 — 目标就是中文
        return true;
    }
    if ((runtime.targetLanguage == L"zh-CN" || runtime.targetLanguage == L"zh" || runtime.targetLanguage == L"zh-Hans")
        && LeavesTooMuchSourceText(input, out)) {
        return true;
    }
    return false;
}

bool EndsWithWord(const std::wstring& value, const std::wstring& suffix, std::wstring& prefix)
{
    if (value.size() < suffix.size()) return false;
    size_t start = value.size() - suffix.size();
    if (LowerAscii(value.substr(start)) != suffix) return false;
    if (start > 0 && !iswspace(value[start - 1])) return false;
    prefix = text::Trim(value.substr(0, start));
    return true;
}

bool IsChatEdgePunctuation(wchar_t ch)
{
    return ch == L'!' || ch == L'?' || ch == L'.' || ch == L',' || ch == L';' || ch == L':'
        || ch == L'~' || ch == L'-' || ch == L'_' || ch == L'\'' || ch == L'"'
        || ch == L'/' || ch == L'\\' || ch == L'|' || ch == L'`'
        || ch == L'\x2026' || ch == L'\x2018' || ch == L'\x2019' || ch == L'\x201C' || ch == L'\x201D'
        || ch == L'\xFF01' || ch == L'\xFF1F' || ch == L'\x3002' || ch == L'\xFF0C'
        || ch == L'\xFF1B' || ch == L'\xFF1A' || ch == L'\x3001';
}

bool HasTranslatableSignal(const std::wstring& value)
{
    bool hasLetter = false;
    for (wchar_t ch : value) {
        if (iswalpha(ch)) {
            hasLetter = true;
            continue;
        }
        if ((ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z')) {
            hasLetter = true;
            continue;
        }
    }
    return hasLetter;
}

bool IsNonTranslatableChatText(const std::wstring& text)
{
    std::wstring value = TrimChatEdgePunctuation(text);
    if (value.empty()) return true;
    if (!HasTranslatableSignal(value)) return true;
    return false;
}

bool LooksLikeGestureOnly(const std::wstring& text)
{
    std::wstring value = text::Trim(text);
    if (value.empty()) return true;

    bool hasO = false;
    bool hasGestureMark = false;
    int letters = 0;
    for (wchar_t ch : value) {
        wchar_t lower = (wchar_t)towlower(ch);
        if (lower == L'o') {
            hasO = true;
            ++letters;
            continue;
        }
        if (iswalpha(ch)) return false;
        if (ch == L'/' || ch == L'\\' || ch == L'|' || ch == L'_' || ch == L'-' ||
            ch == L'(' || ch == L')' || ch == L'[' || ch == L']' || ch == L' ' ||
            ch == L'.' || ch == L',' || ch == L'!' || ch == L'~') {
            if (ch == L'/' || ch == L'\\' || ch == L'|') hasGestureMark = true;
            continue;
        }
        return false;
    }
    return hasO && hasGestureMark && letters <= 3;
}

std::wstring TrimChatEdgePunctuation(std::wstring value)
{
    value = text::Trim(value);
    while (!value.empty() && IsChatEdgePunctuation(value.front())) value.erase(value.begin());
    while (!value.empty() && IsChatEdgePunctuation(value.back())) value.pop_back();
    return text::Trim(value);
}

std::wstring TrimTrailingChatPunctuation(std::wstring value)
{
    value = text::Trim(value);
    while (!value.empty() && IsChatEdgePunctuation(value.back())) value.pop_back();
    return text::Trim(value);
}

bool IsAsciiTokenChar(wchar_t ch)
{
    return (ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') || (ch >= L'0' && ch <= L'9');
}

struct ChatTokenItem
{
    const wchar_t* key;
    const wchar_t* value;
    bool pauseAfter;
};

const ChatTokenItem* ChatTokenItems(size_t& count)
{
    static const ChatTokenItem items[] = {
        { L"wtf", L"什么鬼", true },
        { L"wdf", L"什么鬼", true },
        { L"tf", L"什么鬼", true },
        { L"wth", L"什么鬼", true },
        { L"wdym", L"你什么意思", true },
        { L"wyd", L"你在干嘛", true },
        { L"omg", L"天啊", true },
        { L"ffs", L"真服了", true },
        { L"lol", L"哈哈", true },
        { L"lmao", L"哈哈", true },
        { L"rofl", L"笑死", true },
        { L"xd", L"哈哈", true },
        { L"sb", L"傻逼", true },
        { L"sry", L"抱歉", true },
        { L"sr", L"抱歉", true },
        { L"sory", L"抱歉", true },
        { L"srry", L"抱歉", true },
        { L"srrry", L"抱歉", true },
        { L"sorry", L"抱歉", true },
        { L"soz", L"抱歉", true },
        { L"pardon", L"抱歉", true },
        { L"pls", L"请", false },
        { L"plz", L"请", false },
        { L"please", L"请", false },
        { L"ty", L"谢谢", true },
        { L"tyty", L"谢谢", true },
        { L"tysm", L"非常感谢", true },
        { L"tyvm", L"非常感谢", true },
        { L"thx", L"谢谢", true },
        { L"tnx", L"谢谢", true },
        { L"tks", L"谢谢", true },
        { L"tx", L"谢谢", true },
        { L"tq", L"谢谢", true },
        { L"thanks", L"谢谢", true },
        { L"np", L"没事", true },
        { L"nvm", L"没事", true },
        { L"brb", L"马上回", true },
        { L"afk", L"暂离", true },
        { L"idk", L"我不知道", true },
        { L"idc", L"无所谓", true },
        { L"ikr", L"就是说", true },
        { L"asap", L"尽快", true },
        { L"gg", L"打得好", true },
        { L"wp", L"打得好", true },
        { L"gl", L"祝好运", true },
        { L"hf", L"玩得开心", true },
        { L"hi", L"你好", true },
        { L"hello", L"你好", true },
        { L"hey", L"嘿", true },
        { L"yo", L"嘿", true },
        { L"ow", L"嗷", true },
        { L"sup", L"咋样", true },
        { L"bye", L"再见", true },
        { L"cya", L"再见", true },
        { L"cu", L"再见", true },
        { L"bb", L"再见", true },
        { L"gn", L"晚安", true },
        { L"gn8", L"晚安", true },
        { L"gm", L"早安", true },
        { L"bro", L"兄弟", true },
        { L"bruh", L"兄弟", true },
        { L"dude", L"老兄", true },
        { L"mate", L"伙计", true },
        { L"man", L"兄弟", true },
        { L"k", L"好", true },
        { L"kk", L"好", true },
        { L"ok", L"好的", true },
        { L"okay", L"好的", true },
        { L"yes", L"是", true },
        { L"y", L"是", true },
        { L"no", L"不", true },
        { L"n", L"不", true },
        { L"wait", L"等一下", true },
        { L"stop", L"停一下", true },
        { L"go", L"走", true },
        { L"slow", L"慢点", true },
        { L"move", L"让一下", true },
        { L"lag", L"卡顿", true },
        { L"laggy", L"很卡", true },
        { L"tamam", L"好的", true },
        { L"neden", L"为什么", true },
        { L"hayir", L"不", true },
        { L"hayır", L"不", true },
        { L"sa", L"你好", true },
        { L"as", L"你好", true },
        { L"gel", L"过来", true },
        { L"var", L"有", true },
        { L"varmi", L"有吗", true },
        { L"varmı", L"有吗", true },
        { L"dc", L"Discord", true },
        { L"ds", L"Discord", true },
        { L"crash", L"撞车", true },
        { L"ram", L"撞人", true },
        { L"rammer", L"撞人玩家", true },
        { L"rec", L"已录屏", true },
        { L"recording", L"已录屏", true },
        { L"report", L"举报", true },
        { L"rep", L"举报", true },
        { L"ban", L"封禁", true },
        { L"kick", L"踢出", true },
        { L"fk", L"靠", true },
        { L"fck", L"靠", true },
        { L"fuck", L"操", true },
        { L"shit", L"靠", true },
        { L"damn", L"该死", true },
        { L"stfu", L"闭嘴", true },
        { L"fu", L"去你的", true },
        { L"trash", L"垃圾", false },
        { L"ez", L"太简单了", true },
        { L"idiot", L"白痴", false },
        { L"stupid", L"蠢货", false },
        { L"noob", L"菜鸟", false },
        { L"moron", L"蠢货", false },
        { L"clown", L"小丑", false }
    };
    count = sizeof(items) / sizeof(items[0]);
    return items;
}

bool IsSingleLetterProviderRisk(const std::wstring& token, const std::wstring& value, size_t start, size_t end)
{
    if (token.size() != 1) return false;
    for (size_t i = 0; i < start; ++i) {
        if (!iswspace(value[i])) return true;
    }
    for (size_t i = end; i < value.size(); ++i) {
        if (!iswspace(value[i])) return true;
    }
    return false;
}

bool HasNameJoinerAroundToken(const std::wstring& value, size_t start, size_t end)
{
    auto isJoiner = [](wchar_t ch) {
        return ch == L'-' || ch == L'_' || ch == L'/' || ch == L'\\';
    };
    return (start > 0 && isJoiner(value[start - 1]))
        || (end < value.size() && isJoiner(value[end]));
}

bool ProviderLeftoverToken(const std::wstring& token, std::wstring& translated, bool& pauseAfter)
{
    std::wstring key = TrimChatEdgePunctuation(NormalizeChatForDictionary(token));
    std::wstring squeezedKey = SqueezeRepeatedAsciiLetters(key, 1);
    pauseAfter = false;

    size_t count = 0;
    const ChatTokenItem* items = ChatTokenItems(count);
    for (size_t i = 0; i < count; ++i) {
        if (key == items[i].key || squeezedKey == items[i].key) {
            translated = items[i].value;
            pauseAfter = items[i].pauseAfter;
            return true;
        }
    }
    return false;
}

bool IsDigitsOnly(const std::wstring& value)
{
    if (value.empty()) return false;
    for (wchar_t ch : value) {
        if (ch < L'0' || ch > L'9') return false;
    }
    return true;
}

bool IsIdOrNameToken(const std::wstring& value)
{
    if (value.empty()) return false;
    bool hasAlnum = false;
    bool hasDigit = false;
    bool hasMarker = false;
    for (wchar_t ch : value) {
        if ((ch >= L'a' && ch <= L'z') || (ch >= L'0' && ch <= L'9') || ch == L'_' || ch == L'-') {
            hasAlnum = true;
            if (ch >= L'0' && ch <= L'9') hasDigit = true;
            if (ch == L'_' || ch == L'-') hasMarker = true;
            continue;
        }
        return false;
    }
    return hasAlnum && (hasDigit || hasMarker || value.size() >= 6);
}

std::vector<std::wstring> SplitWords(const std::wstring& value)
{
    std::vector<std::wstring> words;
    std::wstring token;
    for (size_t i = 0; i <= value.size(); ++i) {
        wchar_t ch = (i < value.size()) ? value[i] : L' ';
        if (iswspace(ch)) {
            token = TrimChatEdgePunctuation(token);
            if (!token.empty()) words.push_back(token);
            token.clear();
        } else {
            token.push_back(ch);
        }
    }
    return words;
}

bool NeedsPauseAfterToken(wchar_t next)
{
    if (next == 0 || iswspace(next)) return false;
    if (IsChatEdgePunctuation(next) || next == L'\xFF0C' || next == L'\x3001' || next == L'\x3002') return false;
    return true;
}

bool IsTrailingEmojiToken(const std::wstring& token)
{
    std::wstring key = NormalizeChatForDictionary(token);
    return key == L":)" || key == L":(" || key == L":d" || key == L"<3";
}

std::wstring FixProviderLeftoverShorthand(const std::wstring& value)
{
    if (value.empty()) return value;

    std::wstring out;
    out.reserve(value.size() + 8);
    bool changed = false;

    for (size_t i = 0; i < value.size();) {
        if (!IsAsciiTokenChar(value[i])) {
            out.push_back(value[i++]);
            continue;
        }

        size_t start = i;
        while (i < value.size() && IsAsciiTokenChar(value[i])) ++i;
        std::wstring token = value.substr(start, i - start);
        std::wstring translated;
        bool pauseAfter = false;
        if (!IsSingleLetterProviderRisk(token, value, start, i)
            && !HasNameJoinerAroundToken(value, start, i)
            && ProviderLeftoverToken(token, translated, pauseAfter)) {
            out += translated;
            if (pauseAfter && i < value.size() && NeedsPauseAfterToken(value[i])) out += L"，";
            changed = true;
        } else {
            out += token;
        }
    }

    return changed ? text::Trim(out) : value;
}

std::wstring JoinTailTokens(const std::vector<std::wstring>& words, size_t start, size_t maxCount = 2)
{
    std::wstring out;
    size_t end = (std::min)(words.size(), start + maxCount);
    for (size_t i = start; i < end; ++i) {
        if (!IsIdOrNameToken(words[i])) break;
        if (!out.empty()) out += L" ";
        out += words[i];
    }
    return out;
}

std::wstring StructuredTruckersPhrase(const std::vector<std::wstring>& words, size_t start)
{
    if (start >= words.size()) return L"";
    if (start + 1 < words.size() && words[start] == L"rec" && words[start + 1] == L"ban") {
        std::wstring out = L"已录屏，等封禁";
        std::wstring target = JoinTailTokens(words, start + 2);
        if (!target.empty()) out += L" " + target;
        return out;
    }
    if (words[start] == L"rec" || words[start] == L"recording") {
        std::wstring out = L"已录屏";
        std::wstring target = JoinTailTokens(words, start + 1);
        if (!target.empty()) out += L" " + target;
        return out;
    }
    if (words[start] == L"report" || words[start] == L"rep") {
        std::wstring out = L"举报";
        std::wstring target = JoinTailTokens(words, start + 1);
        if (!target.empty()) out += L" " + target;
        return out;
    }
    if (words[start] == L"ban") {
        std::wstring out = L"封禁";
        std::wstring target = JoinTailTokens(words, start + 1);
        if (!target.empty()) out += L" " + target;
        return out;
    }
    if (words[start] == L"kick") {
        std::wstring out = L"踢出";
        std::wstring target = JoinTailTokens(words, start + 1);
        if (!target.empty()) out += L" " + target;
        return out;
    }
    return L"";
}

std::wstring ShortPhraseFallback(const std::wstring& input)
{
    std::wstring lower = NormalizeChatForDictionary(input);
    if (lower.empty()) return L"";
    std::wstring edgeTrimmed = TrimChatEdgePunctuation(lower);
    std::wstring trailingTrimmed = TrimTrailingChatPunctuation(lower);

    struct Item { const wchar_t* key; const wchar_t* value; };
    static const Item exact[] = {
        { L"thank you", L"谢谢" },
        { L"thank", L"谢谢" },
        { L"good luck", L"祝好运" },
        { L"have fun", L"玩得开心" },
        { L"gute reise", L"一路顺风" },
        { L"nice lag", L"卡得真漂亮" },
        { L"stop horn", L"别按喇叭" },
        { L"the player stop horn", L"那个玩家别按喇叭" },
        { L"f7 at", L"按 F7" },
        { L"dc varmi", L"有 Discord 吗" },
        { L"dc varmı", L"有 Discord 吗" },
        { L"сам виноват", L"是你自己的错" },
        { L"охренел", L"疯了吗" },
        { L"идиот", L"白痴" },
        { L"кидай дс", L"发 Discord" },
        { L"кину запись", L"我会发录像" },
        { L"ребят кто по дс", L"谁用 Discord" },
        { L"поворотник включать надо", L"该打转向灯" },
        { L"ездить научись сам", L"你自己先学会开车" },
        { L"и ты мне говоришь научится ездить", L"你还叫我学开车" },
        { L"ты поворот делал вопще с обочины", L"你刚才是从路肩转弯的" },
        { L"пф", L"哼" },
        { L"умник смотрю", L"看来挺聪明啊" },
        { L"onune kırma olm", L"别往我前面变道，兄弟" },
        { L"önüne kırma olum", L"别往我前面变道，兄弟" },
        { L"araba 100 oldu hasareı", L"车损到 100 了" },
        { L"yol vre la", L"让路啊" },
        { L"yol ver la", L"让路啊" },
        { L"rec ban", L"已录屏，等封禁" },
        { L"o/", L"挥手" },
        { L"o//", L"挥手" },
        { L"0/", L"挥手" },
        { L"0//", L"挥手" },
        { L"\\o", L"挥手" },
        { L"\\o/", L"欢呼" },
        { L"wtf is this", L"什么鬼，这是啥" },
        { L"what the fuck", L"什么鬼" },
        { L"what the hell", L"什么鬼" },
        { L"ahah", L"哈哈" },
        { L"ahaha", L"哈哈" },
        { L"ahahah", L"哈哈" },
        { L"haha", L"哈哈" },
        { L"hahaha", L"哈哈" },
        { L":)", L"微笑" },
        { L":(", L"难过" },
        { L":d", L"哈哈" },
        { L"<3", L"爱心" }
    };

    auto exactLookup = [&](const std::wstring& key) -> std::wstring {
        std::wstring squeezedKey = SqueezeRepeatedAsciiLetters(key, 1);
        for (const auto& item : exact) {
            if (key == item.key || squeezedKey == item.key) return item.value;
        }
        std::wstring translated;
        bool pauseAfter = false;
        if (ProviderLeftoverToken(key, translated, pauseAfter)) return translated;
        return L"";
    };

    if (edgeTrimmed == L"sry pls" || edgeTrimmed == L"sorry pls" || edgeTrimmed == L"sry please" || edgeTrimmed == L"sorry please") {
        return L"抱歉，请";
    }

    std::vector<std::wstring> words = SplitWords(lower);
    if (words.size() == 1) {
        std::wstring translated = exactLookup(words[0]);
        if (!translated.empty() && !IsTrailingEmojiToken(words[0])) return translated;
    }

    for (size_t start = 0; start < words.size() && start <= 2; ++start) {
        std::wstring structured = StructuredTruckersPhrase(words, start);
        if (structured.empty()) continue;
        if (start == 0) return structured;

        std::wstring prefix;
        bool prefixOk = true;
        for (size_t i = 0; i < start; ++i) {
            std::wstring translated = exactLookup(words[i]);
            if (translated.empty()) {
                prefixOk = false;
                break;
            }
            if (!prefix.empty()) prefix += L"，";
            prefix += translated;
        }
        if (prefixOk && !prefix.empty()) return prefix + L"，" + structured;
    }

    if (words.size() >= 2 && words.size() <= 4) {
        std::wstring head = exactLookup(words[0]);
        if (!head.empty()) {
            std::wstring tail = JoinTailTokens(words, 1, words.size() - 1);
            if (!tail.empty() && SplitWords(tail).size() == words.size() - 1) {
                return head + L" " + tail;
            }
        }
    }

    if (lower.find(L"cannot connect to server") != std::wstring::npos ||
        lower.find(L"can't connect to server") != std::wstring::npos ||
        lower.find(L"can not connect to server") != std::wstring::npos) {
        return L"无法连接到服务器，可能是网络连接问题。";
    }
    if (lower.find(L"automatically reconnected") != std::wstring::npos ||
        lower.find(L"automaticly reconnected") != std::wstring::npos ||
        lower.find(L"reconnected within next") != std::wstring::npos ||
        lower.find(L"reconnect within next") != std::wstring::npos) {
        return L"将在接下来的几秒内自动重新连接。";
    }
    if (lower.find(L"connection established") != std::wstring::npos ||
        lower.find(L"connected to server") != std::wstring::npos) {
        return L"已连接到服务器。";
    }
    if (lower.find(L"connection refused") != std::wstring::npos ||
        lower.find(L"connection timed out") != std::wstring::npos) {
        return L"连接失败，请检查网络或稍后重试。";
    }

    for (const auto& item : exact) {
        if ((lower == item.key || edgeTrimmed == item.key) && !IsTrailingEmojiToken(item.key)) return item.value;
    }

    std::vector<std::wstring> tokenTranslations;
    bool allSafeTokens = !words.empty() && words.size() <= 5;
    for (const auto& word : words) {
        if (IsTrailingEmojiToken(word)) {
            allSafeTokens = false;
            break;
        }
        std::wstring translated = exactLookup(word);
        if (translated.empty()) {
            allSafeTokens = false;
            break;
        }
        tokenTranslations.push_back(translated);
    }
    if (allSafeTokens) {
        std::wstring out;
        for (const auto& translated : tokenTranslations) {
            if (!out.empty()) out += L"，";
            out += translated;
        }
        return out;
    }

    std::wstring prefix;
    if (EndsWithWord(trailingTrimmed, L"sry pls", prefix) || EndsWithWord(trailingTrimmed, L"sorry pls", prefix)
        || EndsWithWord(trailingTrimmed, L"sry please", prefix) || EndsWithWord(trailingTrimmed, L"sorry please", prefix)) {
        size_t keep = input.size() - (lower.size() - prefix.size());
        return text::Trim(input.substr(0, keep)) + L" 抱歉，请";
    }

    for (const auto& item : exact) {
        if (!IsTrailingEmojiToken(item.key) && EndsWithWord(trailingTrimmed, item.key, prefix)) {
            size_t keep = input.size() - (lower.size() - prefix.size());
            return text::Trim(input.substr(0, keep)) + L" " + item.value;
        }
    }

    struct SuffixAction { const wchar_t* key; const wchar_t* value; };
    static const SuffixAction suffixActions[] = {
        { L"rec", L"已录屏" },
        { L"recording", L"已录屏" },
        { L"report", L"举报" },
        { L"rep", L"举报" },
        { L"ban", L"封禁" },
        { L"kick", L"踢出" }
    };
    for (const auto& item : suffixActions) {
        if (EndsWithWord(trailingTrimmed, item.key, prefix) && !prefix.empty() && SplitWords(prefix).size() <= 2) {
            size_t keep = input.size() - (lower.size() - prefix.size());
            return text::Trim(input.substr(0, keep)) + L" " + item.value;
        }
    }

    return L"";
}

bool LooksLikeSingleNameOrNoise(const std::wstring& input)
{
    std::wstring value = TrimChatEdgePunctuation(NormalizeChatForDictionary(input));
    if (value.empty() || HasChinese(value)) return false;
    for (wchar_t ch : value) {
        if (iswspace(ch)) return false;
    }

    std::wstring translated;
    bool pauseAfter = false;
    if (ProviderLeftoverToken(value, translated, pauseAfter)) return false;

    bool asciiLettersOnly = true;
    for (wchar_t ch : value) {
        if (ch < L'a' || ch > L'z') {
            asciiLettersOnly = false;
            break;
        }
    }
    if (asciiLettersOnly && value.size() <= 3) return true;

    return IsIdOrNameToken(value);
}

std::wstring JsonStringAfter(const std::string& json, const std::string& marker, const std::string& key)
{
    size_t p = json.find(marker);
    if (p == std::string::npos) return L"";
    return text::JsonString(json.substr(p), key);
}

std::wstring StatusLabel(const NetReply& reply)
{
    if (reply.status == 0) return L"status=0";
    return L"HTTP " + std::to_wstring(reply.status);
}

std::wstring PayloadPreview(const std::string& payload)
{
    std::wstring value = text::FromUtf8(payload);
    if (value.empty() && !payload.empty()) {
        value.assign(payload.begin(), payload.end());
    }
    value.erase(std::remove(value.begin(), value.end(), L'\r'), value.end());
    std::replace(value.begin(), value.end(), L'\n', L' ');
    std::replace(value.begin(), value.end(), L'\t', L' ');
    value = text::Trim(value);
    constexpr size_t kMaxPreview = 700;
    if (value.size() > kMaxPreview) value = value.substr(0, kMaxPreview) + L"...";
    return value;
}

std::wstring ReplyError(const NetReply& reply, const wchar_t* fallback)
{
    if (!reply.error.empty()) return reply.error;
    if (reply.status < 200 || reply.status >= 300) return StatusLabel(reply);
    return fallback;
}

bool PermanentProviderError(const std::wstring& error)
{
    return error.find(L"HTTP 400") != std::wstring::npos
        || error.find(L"HTTP 401") != std::wstring::npos
        || error.find(L"HTTP 403") != std::wstring::npos
        || error.find(L"HTTP 404") != std::wstring::npos
        || error.find(L"bad base_url") != std::wstring::npos
        || error.find(L"sign failed") != std::wstring::npos;
}

int MaxOutputTokensForChat(const std::wstring& input)
{
    int byLength = 56 + (int)(input.size() / 4);
    return (std::max)(64, (std::min)(160, byLength));
}

std::wstring NowStamp()
{
    SYSTEMTIME t{};
    GetLocalTime(&t);
    wchar_t buf[32] = {};
    swprintf_s(buf, L"%02u:%02u:%02u.%03u", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    return buf;
}

std::wstring WidenAscii(const std::string& value)
{
    std::wstring out;
    out.reserve(value.size());
    for (unsigned char ch : value) out.push_back((wchar_t)ch);
    return out;
}

std::wstring Rfc3986Encode(const std::wstring& value)
{
    std::string utf8 = text::ToUtf8(value);
    std::wstring out;
    out.reserve(utf8.size() * 3);
    static const wchar_t* hex = L"0123456789ABCDEF";
    for (unsigned char ch : utf8) {
        bool unreserved = (ch >= 'A' && ch <= 'Z')
            || (ch >= 'a' && ch <= 'z')
            || (ch >= '0' && ch <= '9')
            || ch == '-' || ch == '_' || ch == '.' || ch == '~';
        if (unreserved) {
            out.push_back((wchar_t)ch);
        } else {
            out.push_back(L'%');
            out.push_back(hex[(ch >> 4) & 0xF]);
            out.push_back(hex[ch & 0xF]);
        }
    }
    return out;
}

std::wstring FirstJsonString(const std::string& json, std::initializer_list<const char*> keys)
{
    for (const char* key : keys) {
        std::wstring out = text::Trim(text::JsonString(json, key));
        if (!out.empty()) return out;
    }
    return L"";
}

std::wstring FirstJsonArrayString(const std::string& json, const std::string& key)
{
    std::string needle = "\"" + key + "\"";
    size_t p = json.find(needle);
    if (p == std::string::npos) return L"";
    p = json.find(':', p + needle.size());
    if (p == std::string::npos) return L"";
    p = json.find('[', p);
    if (p == std::string::npos) return L"";
    ++p;
    while (p < json.size() && (unsigned char)json[p] <= ' ') ++p;
    if (p >= json.size() || json[p] != '"') return L"";
    std::string probe = "{\"" + key + "\":" + json.substr(p) + "}";
    return text::JsonString(probe, key);
}

std::string FormPair(const char* key, const std::wstring& value)
{
    return std::string(key) + "=" + text::ToUtf8(text::PercentEncode(value));
}

std::wstring TargetForSimpleApi(const std::wstring& target)
{
    std::wstring value = LowerAscii(target.empty() ? L"zh-CN" : target);
    std::replace(value.begin(), value.end(), L'_', L'-');
    if (value == L"zh-cn" || value == L"zh-hans" || value == L"zh-chs") return L"zh";
    if (value == L"zh-tw" || value == L"zh-hant" || value == L"cht") return L"zh-TW";
    return value;
}

bool ContainsAny(const std::wstring& text, const std::wstring& chars)
{
    return text.find_first_of(chars) != std::wstring::npos;
}

int CountAny(const std::wstring& text, const std::wstring& chars)
{
    int count = 0;
    for (wchar_t ch : text) {
        if (chars.find(ch) != std::wstring::npos) ++count;
    }
    return count;
}

bool HasWordHint(const std::wstring& lower, std::initializer_list<const wchar_t*> words)
{
    std::wstring padded = L" " + lower + L" ";
    for (const wchar_t* word : words) {
        if (padded.find(L" " + std::wstring(word) + L" ") != std::wstring::npos) return true;
    }
    return false;
}

std::wstring GuessSourceLanguage(const std::wstring& input)
{
    std::wstring lower = LowerAscii(input);

    int cyrillic = 0;
    int greek = 0;
    int latin = 0;
    for (wchar_t ch : lower) {
        if (ch >= 0x0400 && ch <= 0x04FF) ++cyrillic;
        else if (ch >= 0x0370 && ch <= 0x03FF) ++greek;
        else if ((ch >= L'a' && ch <= L'z') || (ch >= 0x00C0 && ch <= 0x024F)) ++latin;
    }

    if (cyrillic > 0 && cyrillic >= latin) {
        if (ContainsAny(lower, L"іїєґ")) return L"uk";
        if (ContainsAny(lower, L"ћђљњџ")) return L"sr";
        if (ContainsAny(lower, L"ъ")) return L"bg";
        return L"ru";
    }
    if (greek > 0 && greek >= latin) return L"el";

    int turkishScore = CountAny(lower, L"ığşçİĞŞÇ") + CountAny(lower, L"üö") / 2;
    int germanScore = CountAny(lower, L"äöüß");
    if (HasWordHint(lower, { L"die", L"der", L"das", L"für", L"mit", L"ohne", L"statt", L"heute", L"bei", L"uns", L"dich" })) {
        germanScore += 3;
    }
    if (HasWordHint(lower, { L"ben", L"sen", L"kanka", L"kardeşim", L"tamam", L"abi", L"gel", L"git", L"yavaş", L"için" })) {
        turkishScore += 3;
    }
    if (germanScore > 0 || turkishScore > 0) return germanScore >= turkishScore ? L"de" : L"tr";
    if (ContainsAny(lower, L"ąęłńóśźż")) return L"pl";
    if (ContainsAny(lower, L"ěščřžýáíéďťňů")) return L"cs";
    if (ContainsAny(lower, L"ăâîșţț")) return L"ro";
    if (ContainsAny(lower, L"ñ¿¡")) return L"es";
    if (ContainsAny(lower, L"ãõ")) return L"pt";
    if (ContainsAny(lower, L"ß")) return L"de";

    if (HasWordHint(lower, { L"ich", L"nicht", L"und", L"der", L"die", L"das", L"bitte", L"danke" })) return L"de";
    if (HasWordHint(lower, { L"ben", L"sen", L"kanka", L"kardeşim", L"tamam", L"abi", L"gel", L"git", L"yavaş" })) return L"tr";
    if (HasWordHint(lower, { L"bonjour", L"merci", L"avec", L"pour", L"pas", L"vous", L"oui" })) return L"fr";
    if (HasWordHint(lower, { L"hola", L"gracias", L"pero", L"porque", L"para", L"amigo" })) return L"es";
    if (HasWordHint(lower, { L"ciao", L"grazie", L"perché", L"sono", L"andare" })) return L"it";
    if (HasWordHint(lower, { L"obrigado", L"porque", L"para", L"você" })) return L"pt";
    if (HasWordHint(lower, { L"kurwa", L"dzieki", L"dzięki", L"prosze", L"proszę" })) return L"pl";

    return L"en";
}

std::wstring SourceOrGuess(const std::wstring& configured, const std::wstring& input)
{
    if (!configured.empty() && configured != L"auto") return configured;
    return GuessSourceLanguage(input);
}

std::wstring EffectiveTarget(const ProviderSettings& settings, const RuntimeConfig& runtime)
{
    return settings.targetLanguage.empty() ? runtime.targetLanguage : settings.targetLanguage;
}

std::wstring TargetForDeepL(const std::wstring& target)
{
    std::wstring value = LowerAscii(target.empty() ? L"zh-CN" : target);
    std::replace(value.begin(), value.end(), L'_', L'-');
    if (value == L"zh" || value == L"zh-cn" || value == L"zh-hans" || value == L"zh-chs") return L"ZH-HANS";
    if (value == L"zh-tw" || value == L"zh-hant" || value == L"cht") return L"ZH-HANT";
    std::wstring out = value;
    std::transform(out.begin(), out.end(), out.begin(), towupper);
    return out;
}

std::wstring ProviderBaseUrl(const ProviderSettings& settings, const wchar_t* fallback)
{
    return settings.baseUrl.empty() ? fallback : settings.baseUrl;
}

bool IsOllamaLikeProvider(const ProviderSettings& settings)
{
    std::wstring probe = LowerAscii(settings.kind + L" " + settings.label + L" " + settings.baseUrl + L" " + settings.model);
    return probe.find(L"ollama") != std::wstring::npos
        || probe.find(L"translategemma") != std::wstring::npos
        || probe.find(L":11434") != std::wstring::npos;
}

void NormalizeOllamaOpenAIBaseUrl(std::wstring& baseUrl)
{
    std::wstring lower = LowerAscii(baseUrl);
    size_t api = lower.find(L"/api");
    if (api != std::wstring::npos && (api + 4 == lower.size() || lower[api + 4] == L'/')) {
        baseUrl = baseUrl.substr(0, api) + L"/v1";
        return;
    }

    while (!baseUrl.empty() && baseUrl.back() == L'/') baseUrl.pop_back();
    lower = LowerAscii(baseUrl);
    if (lower.find(L"/v1") == std::wstring::npos) baseUrl += L"/v1";
}

std::string HexHash(const std::string& data, ALG_ID alg)
{
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    if (!CryptAcquireContextW(&provider, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) return "";
    if (!CryptCreateHash(provider, alg, 0, 0, &hash)) {
        CryptReleaseContext(provider, 0);
        return "";
    }
    BOOL ok = CryptHashData(hash, (const BYTE*)data.data(), (DWORD)data.size(), 0);
    DWORD size = 0;
    DWORD sizeLen = sizeof(size);
    if (ok) ok = CryptGetHashParam(hash, HP_HASHSIZE, (BYTE*)&size, &sizeLen, 0);
    std::vector<BYTE> bytes(size);
    if (ok && size > 0) ok = CryptGetHashParam(hash, HP_HASHVAL, bytes.data(), &size, 0);
    CryptDestroyHash(hash);
    CryptReleaseContext(provider, 0);
    if (!ok) return "";

    static const char* hex = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (BYTE b : bytes) {
        out.push_back(hex[(b >> 4) & 0xF]);
        out.push_back(hex[b & 0xF]);
    }
    return out;
}

std::vector<BYTE> Sha256Bytes(const std::string& data)
{
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    if (!CryptAcquireContextW(&provider, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) return {};
    if (!CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash)) {
        CryptReleaseContext(provider, 0);
        return {};
    }
    BOOL ok = CryptHashData(hash, (const BYTE*)data.data(), (DWORD)data.size(), 0);
    DWORD size = 0;
    DWORD sizeLen = sizeof(size);
    if (ok) ok = CryptGetHashParam(hash, HP_HASHSIZE, (BYTE*)&size, &sizeLen, 0);
    std::vector<BYTE> bytes(size);
    if (ok && size > 0) ok = CryptGetHashParam(hash, HP_HASHVAL, bytes.data(), &size, 0);
    CryptDestroyHash(hash);
    CryptReleaseContext(provider, 0);
    return ok ? bytes : std::vector<BYTE>{};
}

std::vector<BYTE> HmacSha256Bytes(const std::vector<BYTE>& key, const std::string& data)
{
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    HCRYPTKEY cryptoKey = 0;
    struct Blob
    {
        BLOBHEADER header;
        DWORD keySize;
    };

    if (!CryptAcquireContextW(&provider, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) return {};

    std::vector<BYTE> blob(sizeof(Blob) + key.size());
    auto* b = reinterpret_cast<Blob*>(blob.data());
    b->header.bType = PLAINTEXTKEYBLOB;
    b->header.bVersion = CUR_BLOB_VERSION;
    b->header.reserved = 0;
    b->header.aiKeyAlg = CALG_RC2;
    b->keySize = (DWORD)key.size();
    if (!key.empty()) memcpy(blob.data() + sizeof(Blob), key.data(), key.size());

    BOOL ok = CryptImportKey(provider, blob.data(), (DWORD)blob.size(), 0, CRYPT_IPSEC_HMAC_KEY, &cryptoKey);
    if (ok) ok = CryptCreateHash(provider, CALG_HMAC, cryptoKey, 0, &hash);
    HMAC_INFO info{};
    info.HashAlgid = CALG_SHA_256;
    if (ok) ok = CryptSetHashParam(hash, HP_HMAC_INFO, (BYTE*)&info, 0);
    if (ok) ok = CryptHashData(hash, (const BYTE*)data.data(), (DWORD)data.size(), 0);
    DWORD size = 0;
    DWORD sizeLen = sizeof(size);
    if (ok) ok = CryptGetHashParam(hash, HP_HASHSIZE, (BYTE*)&size, &sizeLen, 0);
    std::vector<BYTE> bytes(size);
    if (ok && size > 0) ok = CryptGetHashParam(hash, HP_HASHVAL, bytes.data(), &size, 0);
    if (hash) CryptDestroyHash(hash);
    if (cryptoKey) CryptDestroyKey(cryptoKey);
    CryptReleaseContext(provider, 0);
    return ok ? bytes : std::vector<BYTE>{};
}

std::string BytesHex(const std::vector<BYTE>& bytes)
{
    static const char* hex = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (BYTE b : bytes) {
        out.push_back(hex[(b >> 4) & 0xF]);
        out.push_back(hex[b & 0xF]);
    }
    return out;
}

std::string BytesBase64(const std::vector<BYTE>& bytes)
{
    if (bytes.empty()) return "";
    DWORD needed = 0;
    if (!CryptBinaryToStringA(bytes.data(), (DWORD)bytes.size(),
        CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &needed)) {
        return "";
    }
    std::string out(needed, '\0');
    if (!CryptBinaryToStringA(bytes.data(), (DWORD)bytes.size(),
        CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, out.data(), &needed)) {
        return "";
    }
    while (!out.empty() && out.back() == '\0') out.pop_back();
    return out;
}

std::string Md5Hex(const std::wstring& data)
{
    return HexHash(text::ToUtf8(data), CALG_MD5);
}

std::string HmacSha1Base64(const std::wstring& key, const std::string& data)
{
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    HCRYPTKEY cryptoKey = 0;
    struct Blob
    {
        BLOBHEADER header;
        DWORD keySize;
    };

    std::string keyUtf8 = text::ToUtf8(key);
    if (!CryptAcquireContextW(&provider, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) return "";

    std::vector<BYTE> blob(sizeof(Blob) + keyUtf8.size());
    auto* b = reinterpret_cast<Blob*>(blob.data());
    b->header.bType = PLAINTEXTKEYBLOB;
    b->header.bVersion = CUR_BLOB_VERSION;
    b->header.reserved = 0;
    b->header.aiKeyAlg = CALG_RC2;
    b->keySize = (DWORD)keyUtf8.size();
    if (!keyUtf8.empty()) memcpy(blob.data() + sizeof(Blob), keyUtf8.data(), keyUtf8.size());

    BOOL ok = CryptImportKey(provider, blob.data(), (DWORD)blob.size(), 0, CRYPT_IPSEC_HMAC_KEY, &cryptoKey);
    if (ok) ok = CryptCreateHash(provider, CALG_HMAC, cryptoKey, 0, &hash);
    HMAC_INFO info{};
    info.HashAlgid = CALG_SHA1;
    if (ok) ok = CryptSetHashParam(hash, HP_HMAC_INFO, (BYTE*)&info, 0);
    if (ok) ok = CryptHashData(hash, (const BYTE*)data.data(), (DWORD)data.size(), 0);
    DWORD size = 0;
    DWORD sizeLen = sizeof(size);
    if (ok) ok = CryptGetHashParam(hash, HP_HASHSIZE, (BYTE*)&size, &sizeLen, 0);
    std::vector<BYTE> bytes(size);
    if (ok && size > 0) ok = CryptGetHashParam(hash, HP_HASHVAL, bytes.data(), &size, 0);
    if (hash) CryptDestroyHash(hash);
    if (cryptoKey) CryptDestroyKey(cryptoKey);
    CryptReleaseContext(provider, 0);
    return ok ? BytesBase64(bytes) : "";
}

std::string Sha256Hex(const std::wstring& data)
{
    return HexHash(text::ToUtf8(data), CALG_SHA_256);
}

std::wstring RandomSalt()
{
    ULONGLONG tick = GetTickCount64();
    DWORD pid = GetCurrentProcessId();
    return std::to_wstring(tick) + L"." + std::to_wstring(pid);
}

std::wstring UnixSeconds()
{
    FILETIME ft = {};
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER value = {};
    value.LowPart = ft.dwLowDateTime;
    value.HighPart = ft.dwHighDateTime;
    unsigned long long seconds = (value.QuadPart - 116444736000000000ULL) / 10000000ULL;
    return std::to_wstring(seconds);
}

std::wstring Iso8601Utc()
{
    SYSTEMTIME st{};
    GetSystemTime(&st);
    wchar_t buf[32] = {};
    swprintf_s(buf, L"%04u-%02u-%02uT%02u:%02u:%02uZ",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buf;
}

std::string VolcDateTimeUtc(std::string& date)
{
    SYSTEMTIME st{};
    GetSystemTime(&st);
    char buf[32] = {};
    sprintf_s(buf, "%04u%02u%02uT%02u%02u%02uZ",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    char dateBuf[16] = {};
    sprintf_s(dateBuf, "%04u%02u%02u", st.wYear, st.wMonth, st.wDay);
    date = dateBuf;
    return buf;
}

std::wstring YoudaoInputForSign(const std::wstring& input)
{
    if (input.size() <= 20) return input;
    return input.substr(0, 10) + std::to_wstring(input.size()) + input.substr(input.size() - 10);
}

std::wstring SourceForBaidu(const std::wstring& source)
{
    std::wstring value = LowerAscii(source.empty() ? L"auto" : source);
    std::replace(value.begin(), value.end(), L'_', L'-');
    if (value == L"zh-cn" || value == L"zh-hans" || value == L"zh-chs") return L"zh";
    if (value == L"zh-tw" || value == L"zh-hant" || value == L"cht") return L"cht";
    return value;
}

std::wstring TargetForBaidu(const std::wstring& target)
{
    std::wstring value = LowerAscii(target.empty() ? L"zh-CN" : target);
    std::replace(value.begin(), value.end(), L'_', L'-');
    if (value == L"zh" || value == L"zh-cn" || value == L"zh-hans" || value == L"zh-chs") return L"zh";
    if (value == L"zh-tw" || value == L"zh-hant" || value == L"cht") return L"cht";
    if (value == L"ja") return L"jp";
    return value;
}

std::wstring BaiduErrorMessage(const std::wstring& codeOrMessage)
{
    std::wstring value = LowerAscii(codeOrMessage);
    if (value.find(L"invalid_to_param") != std::wstring::npos || value.find(L"58001") != std::wstring::npos) {
        return L"INVALID_TO_PARAM(百度目标语言参数无效，中文请使用 zh；插件已自动兼容 zh-CN)";
    }
    if (value.find(L"invalid_from_param") != std::wstring::npos || value.find(L"58000") != std::wstring::npos) {
        return L"INVALID_FROM_PARAM(百度源语言参数无效，建议源语言设为 auto)";
    }
    if (value.find(L"invalid_sign") != std::wstring::npos || value.find(L"54001") != std::wstring::npos) {
        return L"INVALID_SIGN(百度签名错误，请检查 APP ID 和密钥)";
    }
    if (value.find(L"54003") != std::wstring::npos) {
        return L"ACCESS_FREQUENCY_LIMITED(百度请求过快，请降低 workers 或稍后再试)";
    }
    if (value.find(L"52003") != std::wstring::npos) {
        return L"UNAUTHORIZED_USER(百度 APP ID 或密钥不正确)";
    }
    return codeOrMessage;
}

std::wstring TargetForYoudao(const std::wstring& target)
{
    std::wstring value = LowerAscii(target.empty() ? L"zh-CN" : target);
    std::replace(value.begin(), value.end(), L'_', L'-');
    if (value == L"zh" || value == L"zh-cn" || value == L"zh-hans" || value == L"zh-chs") return L"zh-CHS";
    if (value == L"zh-tw" || value == L"zh-hant" || value == L"cht") return L"zh-CHT";
    return value;
}

std::wstring TargetForTencent(const std::wstring& target)
{
    std::wstring value = LowerAscii(target.empty() ? L"zh-CN" : target);
    std::replace(value.begin(), value.end(), L'_', L'-');
    if (value == L"zh" || value == L"zh-cn" || value == L"zh-hans" || value == L"zh-chs") return L"zh";
    if (value == L"zh-tw" || value == L"zh-hant" || value == L"cht") return L"zh-TW";
    return value;
}

std::wstring SourceForTencent(const std::wstring& source)
{
    std::wstring value = LowerAscii(source.empty() ? L"auto" : source);
    std::replace(value.begin(), value.end(), L'_', L'-');
    if (value == L"auto") return L"auto";
    if (value == L"zh-cn" || value == L"zh-hans" || value == L"zh-chs") return L"zh";
    if (value == L"zh-tw" || value == L"zh-hant" || value == L"cht") return L"zh-TW";
    return value;
}

std::string DateUtcForTencent()
{
    SYSTEMTIME st{};
    GetSystemTime(&st);
    char buf[16] = {};
    sprintf_s(buf, "%04u-%02u-%02u", st.wYear, st.wMonth, st.wDay);
    return buf;
}

std::wstring TargetForAliyun(const std::wstring& target)
{
    std::wstring value = LowerAscii(target.empty() ? L"zh-CN" : target);
    std::replace(value.begin(), value.end(), L'_', L'-');
    if (value == L"zh" || value == L"zh-cn" || value == L"zh-hans" || value == L"zh-chs") return L"zh";
    if (value == L"zh-tw" || value == L"zh-hant" || value == L"cht") return L"cht";
    return value;
}

std::wstring SourceForAliyun(const std::wstring& source)
{
    std::wstring value = LowerAscii(source.empty() ? L"auto" : source);
    std::replace(value.begin(), value.end(), L'_', L'-');
    if (value == L"zh-cn" || value == L"zh-hans" || value == L"zh-chs") return L"zh";
    if (value == L"zh-tw" || value == L"zh-hant" || value == L"cht") return L"cht";
    return value;
}

std::wstring TargetForVolcengine(const std::wstring& target)
{
    std::wstring value = LowerAscii(target.empty() ? L"zh-CN" : target);
    std::replace(value.begin(), value.end(), L'_', L'-');
    if (value == L"zh" || value == L"zh-cn" || value == L"zh-hans" || value == L"zh-chs") return L"zh";
    if (value == L"zh-tw" || value == L"zh-hant" || value == L"cht") return L"zh-Hant";
    return value;
}

std::wstring SourceForVolcengine(const std::wstring& source)
{
    std::wstring value = LowerAscii(source.empty() ? L"auto" : source);
    std::replace(value.begin(), value.end(), L'_', L'-');
    if (value == L"auto") return L"";
    if (value == L"zh-cn" || value == L"zh-hans" || value == L"zh-chs") return L"zh";
    if (value == L"zh-tw" || value == L"zh-hant" || value == L"cht") return L"zh-Hant";
    return value;
}

std::wstring AliyunCanonicalQuery(std::vector<std::pair<std::wstring, std::wstring>> params)
{
    std::sort(params.begin(), params.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });

    std::wstring query;
    for (const auto& item : params) {
        if (!query.empty()) query += L"&";
        query += Rfc3986Encode(item.first) + L"=" + Rfc3986Encode(item.second);
    }
    return query;
}

class AndeerTranslator final : public TranslateProvider
{
public:
    using TranslateProvider::TranslateProvider;
    bool Ready() const override { return true; }

    std::wstring Translate(const std::wstring& input, const RuntimeConfig&, HttpAgent& http, std::wstring& error) override
    {
        std::wstring path = L"/API/fanyi2.php?msg=" + text::PercentEncode(input);
        NetReply r = http.Get(L"api.andeer.top", 443, path, true);
        if (r.payload.empty()) r = http.Get(L"api.andeer.top", 80, path, false);
        LogLine(L"[TranslateHTTP] Andeer " + StatusLabel(r) + L" payload=" + PayloadPreview(r.payload));
        if (r.payload.empty()) {
            error = ReplyError(r, L"empty reply");
            return L"";
        }
        if (r.status < 200 || r.status >= 300) {
            error = ReplyError(r, L"HTTP error");
            return L"";
        }

        std::wstring out = FirstJsonString(r.payload, {
            "\xe7\xbf\xbb\xe8\xaf\x91\xe5\x90\x8e",
            "result",
            "translation",
            "translatedText",
            "text",
            "data",
            "message"
        });
        if (out.empty()) error = L"cannot parse response";
        return out;
    }
};

class MyMemoryTranslator final : public TranslateProvider
{
public:
    using TranslateProvider::TranslateProvider;
    bool Ready() const override { return true; }

    std::wstring Translate(const std::wstring& input, const RuntimeConfig& runtime, HttpAgent& http, std::wstring& error) override
    {
        std::wstring from = SourceOrGuess(settings_.sourceLanguage, input);
        std::wstring to = EffectiveTarget(settings_, runtime).empty() ? L"zh-CN" : EffectiveTarget(settings_, runtime);
        std::wstring path = L"/get?q=" + text::PercentEncode(input) + L"&langpair=" + from + L"|" + to;
        NetReply r = http.Get(L"api.mymemory.translated.net", 443, path, true);
        LogLine(L"[TranslateHTTP] MyMemory source=" + from + L" " + StatusLabel(r) + L" payload=" + PayloadPreview(r.payload));
        if (r.payload.empty()) {
            error = ReplyError(r, L"empty reply");
            return L"";
        }
        if (r.status < 200 || r.status >= 300) {
            error = ReplyError(r, L"HTTP error");
            return L"";
        }
        std::wstring out = FirstJsonString(r.payload, { "translatedText", "match", "translation" });
        if (out.empty()) error = L"cannot parse response";
        return out;
    }
};

class ChatCompletionsTranslator final : public TranslateProvider
{
public:
    using TranslateProvider::TranslateProvider;
    bool Ready() const override
    {
        return !settings_.model.empty() || IsDeepSeek();
    }

    std::wstring Translate(const std::wstring& input, const RuntimeConfig& runtime, HttpAgent& http, std::wstring& error) override
    {
        std::wstring host, prefix;
        INTERNET_PORT port = 443;
        bool tls = true;
        bool deepSeek = IsDeepSeek();
        bool miMo = IsMiMo();
        bool ollama = IsOllama();
        std::wstring baseUrl = settings_.baseUrl;
        if (baseUrl.empty() && deepSeek) baseUrl = L"https://api.deepseek.com";
        if (baseUrl.empty() && miMo) baseUrl = L"https://api.xiaomimimo.com/v1";
        if (ollama) NormalizeOllamaOpenAIBaseUrl(baseUrl);
        if (!SplitUrl(baseUrl, host, port, prefix, tls)) {
            error = L"bad base_url";
            return L"";
        }
        std::wstring model = settings_.model.empty() && deepSeek
            ? L"deepseek-v4-flash"
            : settings_.model;

        std::wstring target = EffectiveTarget(settings_, runtime).empty() ? L"zh-CN" : EffectiveTarget(settings_, runtime);
        std::wstring prompt = L"You translate TruckersMP/ETS2 multiplayer chat into " + target +
            L". Output only the translation, no quotes, no explanations, no reasoning. "
            L"Any language/slang. Map sry=抱歉, ty/thx=谢谢, rec=已录屏, wtf=什么鬼. "
            L"Keep names, IDs, tags, URLs and emoji unchanged.";

        std::string body = "{";
        body += "\"model\":\"" + text::EscapeJson(model) + "\",";
        body += "\"temperature\":0,";
        if (miMo) {
            body += "\"max_completion_tokens\":" + std::to_string(MaxOutputTokensForChat(input)) + ",";
        } else {
            body += "\"max_tokens\":" + std::to_string(MaxOutputTokensForChat(input)) + ",";
        }
        if (deepSeek || miMo) body += "\"thinking\":{\"type\":\"disabled\"},";
        body += "\"messages\":[";
        body += "{\"role\":\"system\",\"content\":\"" + text::EscapeJson(prompt) + "\"},";
        body += "{\"role\":\"user\",\"content\":\"" + text::EscapeJson(input) + "\"}";
        body += "]}";

        std::vector<HeaderPair> headers{
            { L"Content-Type", L"application/json" },
            { L"Accept", L"application/json" }
        };
        if (!settings_.apiKey.empty()) headers.push_back({ L"Authorization", L"Bearer " + settings_.apiKey });
        NetReply r = http.Post(host, port, prefix + L"/chat/completions", tls, body, headers);
        LogLine(L"[TranslateHTTP] " + Name() + L" " + StatusLabel(r) + L" payload=" + PayloadPreview(r.payload));
        if (r.payload.empty()) {
            error = ReplyError(r, L"empty reply");
            return L"";
        }
        if (r.status < 200 || r.status >= 300) {
            error = ReplyError(r, L"HTTP error");
            return L"";
        }
        std::wstring out = text::Trim(JsonStringAfter(r.payload, "\"message\"", "content"));
        if (out.empty()) out = text::Trim(JsonStringAfter(r.payload, "\"choices\"", "text"));
        if (out.empty()) out = FirstJsonString(r.payload, {
            "content",
            "text",
            "output_text",
            "translation",
            "translatedText",
            "result",
            "answer"
        });
        if (out.empty()) {
            if (r.payload.find("\"reasoning_content\"") != std::string::npos || r.payload.find("\"finish_reason\":\"length\"") != std::string::npos) {
                error = L"empty content (thinking output consumed tokens)";
            } else {
                error = L"cannot parse response";
            }
        }
        return out;
    }

private:
    bool IsDeepSeek() const
    {
        std::wstring kind = LowerAscii(settings_.kind);
        return kind == L"deepseek" || kind == L"deepseek_chat" || kind == L"deepseek_compatible";
    }

    bool IsMiMo() const
    {
        std::wstring probe = LowerAscii(settings_.kind + L" " + settings_.label + L" " + settings_.baseUrl + L" " + settings_.model);
        return probe.find(L"mimo") != std::wstring::npos
            || probe.find(L"xiaomi") != std::wstring::npos
            || probe.find(L"xiaomimimo") != std::wstring::npos;
    }

    bool IsOllama() const
    {
        return IsOllamaLikeProvider(settings_);
    }
};

class AnthropicTranslator final : public TranslateProvider
{
public:
    using TranslateProvider::TranslateProvider;
    bool Ready() const override
    {
        return !settings_.apiKey.empty() && !settings_.model.empty();
    }

    std::wstring Translate(const std::wstring& input, const RuntimeConfig& runtime, HttpAgent& http, std::wstring& error) override
    {
        std::wstring host, prefix;
        INTERNET_PORT port = 443;
        bool tls = true;
        std::wstring baseUrl = ProviderBaseUrl(settings_, L"https://api.anthropic.com/v1");
        if (!SplitUrl(baseUrl, host, port, prefix, tls)) {
            error = L"bad base_url";
            return L"";
        }

        std::wstring target = EffectiveTarget(settings_, runtime).empty() ? L"zh-CN" : EffectiveTarget(settings_, runtime);
        std::wstring prompt = L"You translate TruckersMP/ETS2 multiplayer chat into " + target +
            L". Output only the translation, no quotes or explanations. "
            L"Translate short slang and multilingual chat. Common chat: rec=已录屏, rec ban=已录屏，等封禁, wtf=什么鬼, ty/thx=谢谢. "
            L"Keep names, IDs, [tags], URLs and emoji unchanged. Never echo the original.";

        std::string body = "{";
        body += "\"model\":\"" + text::EscapeJson(settings_.model) + "\",";
        body += "\"max_tokens\":" + std::to_string(MaxOutputTokensForChat(input)) + ",";
        body += "\"system\":\"" + text::EscapeJson(prompt) + "\",";
        body += "\"messages\":[{\"role\":\"user\",\"content\":\"" + text::EscapeJson(input) + "\"}]";
        body += "}";

        std::vector<HeaderPair> headers{
            { L"Content-Type", L"application/json" },
            { L"Accept", L"application/json" },
            { L"x-api-key", settings_.apiKey },
            { L"anthropic-version", L"2023-06-01" }
        };
        NetReply r = http.Post(host, port, prefix + L"/messages", tls, body, headers);
        LogLine(L"[TranslateHTTP] " + Name() + L" " + StatusLabel(r) + L" payload=" + PayloadPreview(r.payload));
        if (r.payload.empty()) {
            error = ReplyError(r, L"empty reply");
            return L"";
        }
        if (r.status < 200 || r.status >= 300) {
            error = ReplyError(r, L"HTTP error");
            return L"";
        }

        std::wstring out = FirstJsonString(r.payload, { "text", "content" });
        if (out.empty()) error = L"cannot parse response";
        return out;
    }
};

class DeepLTranslator final : public TranslateProvider
{
public:
    using TranslateProvider::TranslateProvider;
    bool Ready() const override { return !settings_.apiKey.empty(); }

    std::wstring Translate(const std::wstring& input, const RuntimeConfig& runtime, HttpAgent& http, std::wstring& error) override
    {
        std::wstring host, prefix;
        INTERNET_PORT port = 443;
        bool tls = true;
        std::wstring baseUrl = ProviderBaseUrl(settings_, L"https://api-free.deepl.com");
        if (!SplitUrl(baseUrl, host, port, prefix, tls)) {
            error = L"bad base_url";
            return L"";
        }

        std::string body = FormPair("text", input) + "&" +
            FormPair("target_lang", TargetForDeepL(EffectiveTarget(settings_, runtime)));
        if (!settings_.sourceLanguage.empty() && settings_.sourceLanguage != L"auto") {
            std::wstring source = settings_.sourceLanguage;
            std::transform(source.begin(), source.end(), source.begin(), towupper);
            body += "&" + FormPair("source_lang", source);
        }

        std::vector<HeaderPair> headers{
            { L"Content-Type", L"application/x-www-form-urlencoded" },
            { L"Accept", L"application/json" },
            { L"Authorization", L"DeepL-Auth-Key " + settings_.apiKey }
        };
        NetReply r = http.Post(host, port, prefix + L"/v2/translate", tls, body, headers);
        LogLine(L"[TranslateHTTP] " + Name() + L" " + StatusLabel(r) + L" payload=" + PayloadPreview(r.payload));
        if (r.payload.empty()) {
            error = ReplyError(r, L"empty reply");
            return L"";
        }
        if (r.status < 200 || r.status >= 300) {
            error = ReplyError(r, L"HTTP error");
            return L"";
        }
        std::wstring out = FirstJsonString(r.payload, { "text", "translation", "translatedText" });
        if (out.empty()) error = L"cannot parse response";
        return out;
    }
};

class GoogleCloudTranslator final : public TranslateProvider
{
public:
    using TranslateProvider::TranslateProvider;
    bool Ready() const override { return !settings_.apiKey.empty(); }

    std::wstring Translate(const std::wstring& input, const RuntimeConfig& runtime, HttpAgent& http, std::wstring& error) override
    {
        std::wstring host, prefix;
        INTERNET_PORT port = 443;
        bool tls = true;
        std::wstring baseUrl = ProviderBaseUrl(settings_, L"https://translation.googleapis.com");
        if (!SplitUrl(baseUrl, host, port, prefix, tls)) {
            error = L"bad base_url";
            return L"";
        }

        std::wstring path = prefix + L"/language/translate/v2?key=" + text::PercentEncode(settings_.apiKey) +
            L"&q=" + text::PercentEncode(input) +
            L"&target=" + text::PercentEncode(TargetForSimpleApi(EffectiveTarget(settings_, runtime))) +
            L"&format=text";
        if (!settings_.sourceLanguage.empty() && settings_.sourceLanguage != L"auto") {
            path += L"&source=" + text::PercentEncode(settings_.sourceLanguage);
        }

        NetReply r = http.Get(host, port, path, tls, { { L"Accept", L"application/json" } });
        LogLine(L"[TranslateHTTP] " + Name() + L" " + StatusLabel(r) + L" payload=" + PayloadPreview(r.payload));
        if (r.payload.empty()) {
            error = ReplyError(r, L"empty reply");
            return L"";
        }
        if (r.status < 200 || r.status >= 300) {
            error = ReplyError(r, L"HTTP error");
            return L"";
        }
        std::wstring out = FirstJsonString(r.payload, { "translatedText", "translation", "text" });
        if (out.empty()) error = L"cannot parse response";
        return out;
    }
};

class LibreTranslateTranslator final : public TranslateProvider
{
public:
    using TranslateProvider::TranslateProvider;
    bool Ready() const override { return !settings_.baseUrl.empty(); }

    std::wstring Translate(const std::wstring& input, const RuntimeConfig& runtime, HttpAgent& http, std::wstring& error) override
    {
        std::wstring host, prefix;
        INTERNET_PORT port = 443;
        bool tls = true;
        if (!SplitUrl(settings_.baseUrl, host, port, prefix, tls)) {
            error = L"bad base_url";
            return L"";
        }

        std::wstring source = settings_.sourceLanguage.empty() ? L"auto" : settings_.sourceLanguage;
        std::wstring target = TargetForSimpleApi(EffectiveTarget(settings_, runtime));
        std::string body = "{";
        body += "\"q\":\"" + text::EscapeJson(input) + "\",";
        body += "\"source\":\"" + text::EscapeJson(source) + "\",";
        body += "\"target\":\"" + text::EscapeJson(target) + "\",";
        body += "\"format\":\"text\"";
        if (!settings_.apiKey.empty()) body += ",\"api_key\":\"" + text::EscapeJson(settings_.apiKey) + "\"";
        body += "}";

        std::vector<HeaderPair> headers{
            { L"Content-Type", L"application/json" },
            { L"Accept", L"application/json" }
        };
        NetReply r = http.Post(host, port, prefix + L"/translate", tls, body, headers);
        LogLine(L"[TranslateHTTP] " + Name() + L" " + StatusLabel(r) + L" payload=" + PayloadPreview(r.payload));
        if (r.payload.empty()) {
            error = ReplyError(r, L"empty reply");
            return L"";
        }
        if (r.status < 200 || r.status >= 300) {
            error = ReplyError(r, L"HTTP error");
            return L"";
        }
        std::wstring out = FirstJsonString(r.payload, { "translatedText", "translation", "text" });
        if (out.empty()) error = L"cannot parse response";
        return out;
    }
};

class MicrosoftTranslator final : public TranslateProvider
{
public:
    using TranslateProvider::TranslateProvider;
    bool Ready() const override { return !settings_.apiKey.empty(); }

    std::wstring Translate(const std::wstring& input, const RuntimeConfig& runtime, HttpAgent& http, std::wstring& error) override
    {
        std::wstring host, prefix;
        INTERNET_PORT port = 443;
        bool tls = true;
        std::wstring baseUrl = ProviderBaseUrl(settings_, L"https://api.cognitive.microsofttranslator.com");
        if (!SplitUrl(baseUrl, host, port, prefix, tls)) {
            error = L"bad base_url";
            return L"";
        }

        std::wstring path = prefix + L"/translate?api-version=3.0&to=" +
            text::PercentEncode(TargetForSimpleApi(EffectiveTarget(settings_, runtime)));
        if (!settings_.sourceLanguage.empty() && settings_.sourceLanguage != L"auto") {
            path += L"&from=" + text::PercentEncode(settings_.sourceLanguage);
        }

        std::string body = "[{\"Text\":\"" + text::EscapeJson(input) + "\"}]";
        std::vector<HeaderPair> headers{
            { L"Content-Type", L"application/json" },
            { L"Accept", L"application/json" },
            { L"Ocp-Apim-Subscription-Key", settings_.apiKey }
        };
        if (!settings_.apiSecret.empty()) headers.push_back({ L"Ocp-Apim-Subscription-Region", settings_.apiSecret });

        NetReply r = http.Post(host, port, path, tls, body, headers);
        LogLine(L"[TranslateHTTP] " + Name() + L" " + StatusLabel(r) + L" payload=" + PayloadPreview(r.payload));
        if (r.payload.empty()) {
            error = ReplyError(r, L"empty reply");
            return L"";
        }
        if (r.status < 200 || r.status >= 300) {
            error = ReplyError(r, L"HTTP error");
            return L"";
        }
        std::wstring out = FirstJsonString(r.payload, { "text", "translatedText", "translation" });
        if (out.empty()) error = L"cannot parse response";
        return out;
    }
};

class BaiduTranslator final : public TranslateProvider
{
public:
    using TranslateProvider::TranslateProvider;
    bool Ready() const override { return !settings_.apiKey.empty() && !settings_.apiSecret.empty(); }

    std::wstring Translate(const std::wstring& input, const RuntimeConfig& runtime, HttpAgent& http, std::wstring& error) override
    {
        std::wstring host, prefix;
        INTERNET_PORT port = 443;
        bool tls = true;
        std::wstring baseUrl = ProviderBaseUrl(settings_, L"https://fanyi-api.baidu.com");
        if (!SplitUrl(baseUrl, host, port, prefix, tls)) {
            error = L"bad base_url";
            return L"";
        }

        std::wstring salt = RandomSalt();
        std::wstring from = SourceForBaidu(SourceOrGuess(settings_.sourceLanguage, input));
        std::wstring to = TargetForBaidu(EffectiveTarget(settings_, runtime));
        std::string sign = Md5Hex(settings_.apiKey + input + salt + settings_.apiSecret);
        if (sign.empty()) {
            error = L"sign failed";
            return L"";
        }

        std::wstring path = prefix + L"/api/trans/vip/translate?q=" + text::PercentEncode(input) +
            L"&from=" + text::PercentEncode(from) +
            L"&to=" + text::PercentEncode(to) +
            L"&appid=" + text::PercentEncode(settings_.apiKey) +
            L"&salt=" + text::PercentEncode(salt) +
            L"&sign=" + text::FromUtf8(sign);

        NetReply r = http.Get(host, port, path, tls, { { L"Accept", L"application/json" } });
        LogLine(L"[TranslateHTTP] " + Name() + L" " + StatusLabel(r) + L" payload=" + PayloadPreview(r.payload));
        if (r.payload.empty()) {
            error = ReplyError(r, L"empty reply");
            return L"";
        }
        if (r.status < 200 || r.status >= 300) {
            error = ReplyError(r, L"HTTP error");
            return L"";
        }
        std::wstring out = FirstJsonString(r.payload, { "dst", "translatedText", "translation" });
        if (out.empty()) {
            std::wstring msg = FirstJsonString(r.payload, { "error_msg", "error_code" });
            error = msg.empty() ? L"cannot parse response" : BaiduErrorMessage(msg);
        }
        return out;
    }
};

class YoudaoTranslator final : public TranslateProvider
{
public:
    using TranslateProvider::TranslateProvider;
    bool Ready() const override { return !settings_.apiKey.empty() && !settings_.apiSecret.empty(); }

    std::wstring Translate(const std::wstring& input, const RuntimeConfig& runtime, HttpAgent& http, std::wstring& error) override
    {
        std::wstring host, prefix;
        INTERNET_PORT port = 443;
        bool tls = true;
        std::wstring baseUrl = ProviderBaseUrl(settings_, L"https://openapi.youdao.com");
        if (!SplitUrl(baseUrl, host, port, prefix, tls)) {
            error = L"bad base_url";
            return L"";
        }

        std::wstring salt = RandomSalt();
        std::wstring curtime = UnixSeconds();
        std::wstring from = settings_.sourceLanguage.empty() ? L"auto" : settings_.sourceLanguage;
        std::wstring to = TargetForYoudao(EffectiveTarget(settings_, runtime));
        std::string sign = Sha256Hex(settings_.apiKey + YoudaoInputForSign(input) + salt + curtime + settings_.apiSecret);
        if (sign.empty()) {
            error = L"sign failed";
            return L"";
        }

        std::string body = FormPair("q", input) + "&" +
            FormPair("from", from) + "&" +
            FormPair("to", to) + "&" +
            FormPair("appKey", settings_.apiKey) + "&" +
            FormPair("salt", salt) + "&" +
            FormPair("sign", text::FromUtf8(sign)) + "&" +
            FormPair("signType", L"v3") + "&" +
            FormPair("curtime", curtime);

        std::vector<HeaderPair> headers{
            { L"Content-Type", L"application/x-www-form-urlencoded" },
            { L"Accept", L"application/json" }
        };
        NetReply r = http.Post(host, port, prefix + L"/api", tls, body, headers);
        LogLine(L"[TranslateHTTP] " + Name() + L" " + StatusLabel(r) + L" payload=" + PayloadPreview(r.payload));
        if (r.payload.empty()) {
            error = ReplyError(r, L"empty reply");
            return L"";
        }
        if (r.status < 200 || r.status >= 300) {
            error = ReplyError(r, L"HTTP error");
            return L"";
        }
        std::wstring out = FirstJsonArrayString(r.payload, "translation");
        if (out.empty()) out = FirstJsonString(r.payload, { "translation", "translatedText", "tgt" });
        if (out.empty()) {
            std::wstring code = FirstJsonString(r.payload, { "errorCode" });
            error = code.empty() ? L"cannot parse response" : L"errorCode=" + code;
        }
        return out;
    }
};

class TencentTranslator final : public TranslateProvider
{
public:
    using TranslateProvider::TranslateProvider;
    bool Ready() const override { return !settings_.apiKey.empty() && !settings_.apiSecret.empty(); }

    std::wstring Translate(const std::wstring& input, const RuntimeConfig& runtime, HttpAgent& http, std::wstring& error) override
    {
        std::wstring host, prefix;
        INTERNET_PORT port = 443;
        bool tls = true;
        std::wstring baseUrl = ProviderBaseUrl(settings_, L"https://tmt.tencentcloudapi.com");
        if (!SplitUrl(baseUrl, host, port, prefix, tls)) {
            error = L"bad base_url";
            return L"";
        }
        if (prefix.empty()) prefix = L"/";

        std::wstring source = SourceForTencent(settings_.sourceLanguage);
        std::wstring target = TargetForTencent(EffectiveTarget(settings_, runtime));
        std::string body = "{";
        body += "\"SourceText\":\"" + text::EscapeJson(input) + "\",";
        body += "\"Source\":\"" + text::EscapeJson(source) + "\",";
        body += "\"Target\":\"" + text::EscapeJson(target) + "\",";
        body += "\"ProjectId\":0";
        body += "}";

        std::string timestamp = text::ToUtf8(UnixSeconds());
        std::string date = DateUtcForTencent();
        std::string service = "tmt";
        std::string canonicalHeaders = "content-type:application/json\nhost:" + text::ToUtf8(host) + "\n";
        std::string signedHeaders = "content-type;host";
        std::string hashedPayload = Sha256Hex(text::FromUtf8(body));
        std::string canonicalRequest = "POST\n/\n\n" + canonicalHeaders + "\n" + signedHeaders + "\n" + hashedPayload;
        std::string credentialScope = date + "/" + service + "/tc3_request";
        std::string stringToSign = "TC3-HMAC-SHA256\n" + timestamp + "\n" + credentialScope + "\n" +
            BytesHex(Sha256Bytes(canonicalRequest));

        std::vector<BYTE> secret = std::vector<BYTE>{ 'T', 'C', '3' };
        std::string secretUtf8 = text::ToUtf8(settings_.apiSecret);
        secret.insert(secret.end(), secretUtf8.begin(), secretUtf8.end());
        std::vector<BYTE> dateKey = HmacSha256Bytes(secret, date);
        std::vector<BYTE> serviceKey = HmacSha256Bytes(dateKey, service);
        std::vector<BYTE> signingKey = HmacSha256Bytes(serviceKey, "tc3_request");
        std::string signature = BytesHex(HmacSha256Bytes(signingKey, stringToSign));
        if (signature.empty()) {
            error = L"sign failed";
            return L"";
        }

        std::wstring authorization = L"TC3-HMAC-SHA256 Credential=" + settings_.apiKey + L"/" +
            WidenAscii(credentialScope) + L", SignedHeaders=" + WidenAscii(signedHeaders) +
            L", Signature=" + WidenAscii(signature);
        std::vector<HeaderPair> headers{
            { L"Content-Type", L"application/json" },
            { L"Accept", L"application/json" },
            { L"Authorization", authorization },
            { L"Host", host },
            { L"X-TC-Action", L"TextTranslate" },
            { L"X-TC-Version", L"2018-03-21" },
            { L"X-TC-Timestamp", WidenAscii(timestamp) },
            { L"X-TC-Region", settings_.model.empty() ? L"ap-guangzhou" : settings_.model }
        };

        NetReply r = http.Post(host, port, prefix, tls, body, headers);
        LogLine(L"[TranslateHTTP] " + Name() + L" " + StatusLabel(r) + L" payload=" + PayloadPreview(r.payload));
        if (r.payload.empty()) {
            error = ReplyError(r, L"empty reply");
            return L"";
        }
        if (r.status < 200 || r.status >= 300) {
            error = ReplyError(r, L"HTTP error");
            return L"";
        }
        std::wstring out = FirstJsonString(r.payload, { "TargetText", "translatedText", "translation", "text" });
        if (out.empty()) {
            std::wstring msg = FirstJsonString(r.payload, { "Message", "Code" });
            error = msg.empty() ? L"cannot parse response" : msg;
        }
        return out;
    }
};

class AliyunTranslator final : public TranslateProvider
{
public:
    using TranslateProvider::TranslateProvider;
    bool Ready() const override { return !settings_.apiKey.empty() && !settings_.apiSecret.empty(); }

    std::wstring Translate(const std::wstring& input, const RuntimeConfig& runtime, HttpAgent& http, std::wstring& error) override
    {
        std::wstring host, prefix;
        INTERNET_PORT port = 443;
        bool tls = true;
        std::wstring baseUrl = ProviderBaseUrl(settings_, L"https://mt.cn-hangzhou.aliyuncs.com");
        if (!SplitUrl(baseUrl, host, port, prefix, tls)) {
            error = L"bad base_url";
            return L"";
        }
        if (prefix.empty()) prefix = L"/";

        std::wstring scene = settings_.model.empty() ? L"general" : settings_.model;
        std::vector<std::pair<std::wstring, std::wstring>> params{
            { L"AccessKeyId", settings_.apiKey },
            { L"Action", L"TranslateGeneral" },
            { L"Format", L"JSON" },
            { L"FormatType", L"text" },
            { L"RegionId", L"cn-hangzhou" },
            { L"Scene", scene },
            { L"SignatureMethod", L"HMAC-SHA1" },
            { L"SignatureNonce", RandomSalt() },
            { L"SignatureVersion", L"1.0" },
            { L"SourceLanguage", SourceForAliyun(settings_.sourceLanguage) },
            { L"SourceText", input },
            { L"TargetLanguage", TargetForAliyun(EffectiveTarget(settings_, runtime)) },
            { L"Timestamp", Iso8601Utc() },
            { L"Version", L"2018-10-12" }
        };

        std::wstring canonical = AliyunCanonicalQuery(params);
        std::string stringToSign = "GET&%2F&" + text::ToUtf8(Rfc3986Encode(canonical));
        std::string signature = HmacSha1Base64(settings_.apiSecret + L"&", stringToSign);
        if (signature.empty()) {
            error = L"sign failed";
            return L"";
        }

        std::wstring path = prefix + L"?" + canonical + L"&Signature=" + Rfc3986Encode(text::FromUtf8(signature));
        NetReply r = http.Get(host, port, path, tls, { { L"Accept", L"application/json" } });
        LogLine(L"[TranslateHTTP] " + Name() + L" " + StatusLabel(r) + L" payload=" + PayloadPreview(r.payload));
        if (r.payload.empty()) {
            error = ReplyError(r, L"empty reply");
            return L"";
        }
        if (r.status < 200 || r.status >= 300) {
            error = ReplyError(r, L"HTTP error");
            return L"";
        }

        std::wstring out = FirstJsonString(r.payload, { "Translated", "translatedText", "translation", "text" });
        if (out.empty()) {
            std::wstring msg = FirstJsonString(r.payload, { "Message", "Code" });
            error = msg.empty() ? L"cannot parse response" : msg;
        }
        return out;
    }
};

class VolcengineTranslator final : public TranslateProvider
{
public:
    using TranslateProvider::TranslateProvider;
    bool Ready() const override { return !settings_.apiKey.empty() && !settings_.apiSecret.empty(); }

    std::wstring Translate(const std::wstring& input, const RuntimeConfig& runtime, HttpAgent& http, std::wstring& error) override
    {
        std::wstring host, prefix;
        INTERNET_PORT port = 443;
        bool tls = true;
        std::wstring baseUrl = ProviderBaseUrl(settings_, L"https://translate.volcengineapi.com");
        if (!SplitUrl(baseUrl, host, port, prefix, tls)) {
            error = L"bad base_url";
            return L"";
        }
        if (prefix.empty()) prefix = L"/";

        std::wstring region = settings_.model.empty() ? L"cn-north-1" : settings_.model;
        std::wstring path = prefix + L"?Action=TranslateText&Version=2020-06-01";
        std::string body = "{";
        body += "\"TextList\":[\"" + text::EscapeJson(input) + "\"],";
        std::wstring source = SourceForVolcengine(settings_.sourceLanguage);
        if (!source.empty()) body += "\"SourceLanguage\":\"" + text::EscapeJson(source) + "\",";
        body += "\"TargetLanguage\":\"" + text::EscapeJson(TargetForVolcengine(EffectiveTarget(settings_, runtime))) + "\"";
        body += "}";

        std::string date;
        std::string amzDate = VolcDateTimeUtc(date);
        std::string payloadHash = Sha256Hex(text::FromUtf8(body));
        std::string hostUtf8 = text::ToUtf8(host);
        std::string canonicalHeaders = "content-type:application/json\nhost:" + hostUtf8 + "\nx-content-sha256:" +
            payloadHash + "\nx-date:" + amzDate + "\n";
        std::string signedHeaders = "content-type;host;x-content-sha256;x-date";
        std::string canonicalRequest = "POST\n/\nAction=TranslateText&Version=2020-06-01\n" +
            canonicalHeaders + "\n" + signedHeaders + "\n" + payloadHash;
        std::string credentialScope = date + "/" + text::ToUtf8(region) + "/translate/request";
        std::string stringToSign = "HMAC-SHA256\n" + amzDate + "\n" + credentialScope + "\n" +
            BytesHex(Sha256Bytes(canonicalRequest));

        std::string secretUtf8 = text::ToUtf8(settings_.apiSecret);
        std::vector<BYTE> secret(secretUtf8.begin(), secretUtf8.end());
        std::vector<BYTE> dateKey = HmacSha256Bytes(secret, date);
        std::vector<BYTE> regionKey = HmacSha256Bytes(dateKey, text::ToUtf8(region));
        std::vector<BYTE> serviceKey = HmacSha256Bytes(regionKey, "translate");
        std::vector<BYTE> signingKey = HmacSha256Bytes(serviceKey, "request");
        std::string signature = BytesHex(HmacSha256Bytes(signingKey, stringToSign));
        if (signature.empty()) {
            error = L"sign failed";
            return L"";
        }

        std::wstring authorization = L"HMAC-SHA256 Credential=" + settings_.apiKey + L"/" +
            WidenAscii(credentialScope) + L", SignedHeaders=" + WidenAscii(signedHeaders) +
            L", Signature=" + WidenAscii(signature);
        std::vector<HeaderPair> headers{
            { L"Content-Type", L"application/json" },
            { L"Accept", L"application/json" },
            { L"Host", host },
            { L"X-Date", WidenAscii(amzDate) },
            { L"X-Content-Sha256", WidenAscii(payloadHash) },
            { L"Authorization", authorization }
        };

        NetReply r = http.Post(host, port, path, tls, body, headers);
        LogLine(L"[TranslateHTTP] " + Name() + L" " + StatusLabel(r) + L" payload=" + PayloadPreview(r.payload));
        if (r.payload.empty()) {
            error = ReplyError(r, L"empty reply");
            return L"";
        }
        if (r.status < 200 || r.status >= 300) {
            error = ReplyError(r, L"HTTP error");
            return L"";
        }

        std::wstring out = FirstJsonString(r.payload, { "Translation", "TranslationText", "translatedText", "translation", "text" });
        if (out.empty()) {
            std::wstring msg = FirstJsonString(r.payload, { "Message", "Code" });
            error = msg.empty() ? L"cannot parse response" : msg;
        }
        return out;
    }
};

std::unique_ptr<TranslateProvider> MakeProvider(const ProviderSettings& p)
{
    std::wstring kind = p.kind;
    std::transform(kind.begin(), kind.end(), kind.begin(), towlower);
    if (!p.enabled) return nullptr;
    if (kind == L"andeer") return std::make_unique<AndeerTranslator>(p);
    if (kind == L"mymemory") return std::make_unique<MyMemoryTranslator>(p);
    if (kind == L"anthropic" || kind == L"claude" || kind == L"anthropic_messages") {
        return std::make_unique<AnthropicTranslator>(p);
    }
    if (kind == L"deepl") return std::make_unique<DeepLTranslator>(p);
    if (kind == L"google_cloud" || kind == L"google_translate") return std::make_unique<GoogleCloudTranslator>(p);
    if (kind == L"libretranslate" || kind == L"libre_translate") return std::make_unique<LibreTranslateTranslator>(p);
    if (kind == L"microsoft" || kind == L"azure_translator") return std::make_unique<MicrosoftTranslator>(p);
    if (kind == L"baidu" || kind == L"baidu_translate") return std::make_unique<BaiduTranslator>(p);
    if (kind == L"youdao") return std::make_unique<YoudaoTranslator>(p);
    if (kind == L"tencent" || kind == L"tencent_cloud" || kind == L"tencent_tmt") return std::make_unique<TencentTranslator>(p);
    if (kind == L"aliyun" || kind == L"alibaba" || kind == L"alibaba_cloud" || kind == L"alimt") return std::make_unique<AliyunTranslator>(p);
    if (kind == L"volcengine" || kind == L"volc" || kind == L"volc_translate" || kind == L"huoshan") return std::make_unique<VolcengineTranslator>(p);
    if (kind == L"deepseek" || kind == L"deepseek_chat" || kind == L"deepseek_compatible" ||
        kind == L"openai_compatible" || kind == L"openai" || kind == L"chat_completions") {
        return std::make_unique<ChatCompletionsTranslator>(p);
    }
    return nullptr;
}
}

TranslateEngine::TranslateEngine() = default;

TranslateEngine::~TranslateEngine()
{
    Stop();
}

bool TranslateEngine::Start(RuntimeConfig runtime, std::vector<ProviderSettings> providerSettings, Done done)
{
    if (running_) return true;

    runtime_ = runtime;
    done_ = std::move(done);
    providers_.clear();
    for (const auto& p : providerSettings) {
        auto provider = MakeProvider(p);
        if (provider && provider->Ready()) {
            provider->SetLogger([this](const std::wstring& line) { LogLine(line); });
            providers_.push_back(std::move(provider));
        }
    }
    if (providers_.empty()) {
        std::lock_guard<std::mutex> g(errorLock_);
        lastError_ = L"no usable translation provider";
        return false;
    }
    providerHealth_.assign(providers_.size(), ProviderHealth{});

    running_ = true;
    activeWorkers_ = (std::max)(1, (std::min)(32, runtime_.workerCount));
    for (int i = 0; i < activeWorkers_; ++i) workers_.emplace_back(&TranslateEngine::Worker, this);
    return true;
}

void TranslateEngine::Stop()
{
    running_ = false;
    {
        std::lock_guard<std::mutex> g(jobsLock_);
        while (!jobs_.empty()) jobs_.pop();
        inFlight_.clear();
    }
    jobsCv_.notify_all();
    for (auto& t : workers_) if (t.joinable()) t.join();
    workers_.clear();
    activeWorkers_ = 0;
}

void TranslateEngine::Submit(unsigned int id, const std::wstring& value)
{
    if (value.empty()) return;

    std::wstring quick = ShortPhraseFallback(value);
    if (!quick.empty()) {
        LogLine(L"[Translate] \"" + value + L"\" -> 本地字典: " + quick);
        if (done_) done_(id, quick);
        return;
    }

    if (LooksLikeSingleNameOrNoise(value)) {
        LogLine(L"[Translate] \"" + value + L"\" -> skip: name/noise");
        if (done_) done_(id, value);
        return;
    }

    if (LooksLikeGestureOnly(value) || IsNonTranslatableChatText(value) || text::MostlyChinese(value)) {
        LogLine(L"[Translate] \"" + value + L"\" -> skip: non-translatable");
        if (done_) done_(id, value);
        return;
    }

    if (!running_) {
        LogLine(L"[Translate] \"" + value + L"\" -> skip: engine not running");
        if (done_) done_(id, value);
        return;
    }

    {
        std::lock_guard<std::mutex> g(cacheLock_);
        auto found = cache_.find(value);
        if (found != cache_.end()) {
            LogLine(L"[Translate] \"" + value + L"\" -> cache: " + found->second);
            if (done_) done_(id, found->second);
            return;
        }
    }

    {
        std::lock_guard<std::mutex> g(jobsLock_);
        auto inFlight = inFlight_.find(value);
        if (inFlight != inFlight_.end()) {
            inFlight->second.push_back(id);
            LogLine(L"[Translate] \"" + value + L"\" -> joined in-flight request");
            return;
        }
        if (jobs_.size() >= runtime_.queueLimit) {
            std::lock_guard<std::mutex> e(errorLock_);
            lastError_ = L"translation queue is full";
            LogLine(L"[Translate] \"" + value + L"\" -> skip: queue full");
            if (done_) done_(id, value);
            return;
        }
        inFlight_[value].push_back(id);
        jobs_.push({ id, value });
    }
    LogLine(L"[Translate] \"" + value + L"\" -> queued");
    jobsCv_.notify_one();
}

int ProviderIntervalMs(const std::wstring& kind)
{
    std::wstring lower = LowerAscii(kind);
    if (lower == L"mymemory") return 260;
    if (lower == L"andeer") return 220;
    if (lower == L"libretranslate" || lower == L"libre_translate") return 180;
    if (lower == L"baidu" || lower == L"baidu_translate") return 90;
    if (lower == L"youdao") return 90;
    if (lower == L"tencent" || lower == L"tencent_cloud" || lower == L"tencent_tmt") return 70;
    if (lower == L"aliyun" || lower == L"alibaba" || lower == L"alibaba_cloud" || lower == L"alimt") return 70;
    if (lower == L"volcengine" || lower == L"volc" || lower == L"volc_translate" || lower == L"huoshan") return 70;
    return 0;
}

int ProviderMaxInFlight(const std::wstring& kind)
{
    std::wstring lower = LowerAscii(kind);
    if (lower == L"mymemory" || lower == L"andeer") return 2;
    if (lower == L"libretranslate" || lower == L"libre_translate") return 3;
    if (lower == L"baidu" || lower == L"baidu_translate") return 4;
    if (lower == L"youdao") return 4;
    if (lower == L"tencent" || lower == L"tencent_cloud" || lower == L"tencent_tmt") return 6;
    if (lower == L"aliyun" || lower == L"alibaba" || lower == L"alibaba_cloud" || lower == L"alimt") return 6;
    if (lower == L"volcengine" || lower == L"volc" || lower == L"volc_translate" || lower == L"huoshan") return 6;
    if (lower == L"deepl" || lower == L"google_cloud" || lower == L"google_translate" || lower == L"microsoft" || lower == L"azure_translator") return 6;
    return 8;
}

int ProviderMaxInFlight(const TranslateProvider& provider)
{
    if (IsOllamaLikeProvider(provider.Settings())) return 1;
    return ProviderMaxInFlight(provider.Kind());
}

bool ShouldRetryProvider(const TranslateProvider& provider)
{
    return !IsOllamaLikeProvider(provider.Settings());
}

bool RetryableProviderError(const std::wstring& error)
{
    return error.empty()
        || error.find(L"request failed") != std::wstring::npos
        || error.find(L"empty reply") != std::wstring::npos
        || error.find(L"empty content") != std::wstring::npos
        || error.find(L"cannot parse response") != std::wstring::npos
        || error.find(L"connect failed") != std::wstring::npos
        || error.find(L"status=0") != std::wstring::npos
        || error.find(L"HTTP 408") != std::wstring::npos
        || error.find(L"HTTP 429") != std::wstring::npos
        || error.find(L"HTTP 500") != std::wstring::npos
        || error.find(L"HTTP 502") != std::wstring::npos
        || error.find(L"HTTP 503") != std::wstring::npos
        || error.find(L"HTTP 504") != std::wstring::npos;
}

std::wstring TranslateEngine::LastError() const
{
    std::lock_guard<std::mutex> g(errorLock_);
    return lastError_;
}

void TranslateEngine::Worker()
{
    HttpAgent http(runtime_.timeoutMs);
    while (running_) {
        Job job;
        {
            std::unique_lock<std::mutex> g(jobsLock_);
            jobsCv_.wait_for(g, std::chrono::milliseconds(200), [this] {
                return !jobs_.empty() || !running_;
            });
            if (!running_) break;
            if (jobs_.empty()) continue;
            job = std::move(jobs_.front());
            jobs_.pop();
        }

        std::wstring out = RunProviders(job.text, http);
        std::vector<unsigned int> ids;
        {
            std::lock_guard<std::mutex> g(jobsLock_);
            auto found = inFlight_.find(job.text);
            if (found != inFlight_.end()) {
                ids = std::move(found->second);
                inFlight_.erase(found);
            } else {
                ids.push_back(job.id);
            }
        }
        if (done_ && running_) {
            std::wstring display = out.empty() ? job.text : out;
            for (unsigned int id : ids) done_(id, display);
        }
    }
}

std::wstring TranslateEngine::RunProviders(const std::wstring& value, HttpAgent& http)
{
    std::wstring fallback = ShortPhraseFallback(value);
    if (!fallback.empty()) {
        LogLine(L"[Translate] \"" + value + L"\" -> 本地字典: " + fallback);
        return fallback;
    }

    {
        std::lock_guard<std::mutex> g(cacheLock_);
        auto found = cache_.find(value);
        if (found != cache_.end()) return found->second;
    }

    if (LooksLikeGestureOnly(value) || IsNonTranslatableChatText(value) || text::MostlyChinese(value)) {
        LogLine(L"[Translate] \"" + value + L"\" -> 跳过(无需翻译)");
        return L"";
    }

    if (LooksLikeSingleNameOrNoise(value)) {
        LogLine(L"[Translate] \"" + value + L"\" -> 跳过(用户名/无意义文本)");
        return L"";
    }

    std::wstring errors;
    for (size_t i = 0; i < providers_.size(); ++i) {
        auto& p = providers_[i];
        int providerIndex = (int)i + 1;
        auto now = std::chrono::steady_clock::now();
        if (ProviderCoolingDown(i, now)) {
            std::wstring error = L"cooling down after repeated failures";
            LogLine(L"[Translate] \"" + value + L"\" provider[" + std::to_wstring(providerIndex) + L"/" +
                std::to_wstring(providers_.size()) + L"] " + p->Name() + L" skipped: " + error);
            if (!errors.empty()) errors += L" | ";
            errors += p->Name() + L": " + error;
            continue;
        }

        std::wstring error;
        std::wstring out;
        std::wstring requestAt = NowStamp();
        auto totalStarted = std::chrono::steady_clock::now();
        int attemptCount = 0;
        int maxAttempts = (providers_.size() == 1 && PendingJobCount() <= 2 && ShouldRetryProvider(*p)) ? 2 : 1;
        for (int attempt = 0; attempt < maxAttempts; ++attempt) {
            ++attemptCount;
            error.clear();
            {
                ProviderSlot slot = AcquireProviderSlot(i);
                if (!slot) {
                    error = L"provider busy";
                    break;
                }
                out = p->Translate(value, runtime_, http, error);
            }
            bool ok = !LooksUntranslated(value, out, runtime_);
            if (ok || !RetryableProviderError(error)) break;
            if (attempt + 1 < maxAttempts && running_) {
                LogLine(L"[Translate] \"" + value + L"\" provider[" + std::to_wstring(providerIndex) + L"/" +
                    std::to_wstring(providers_.size()) + L"] " + p->Name() + L" retry after " +
                    (error.empty() ? L"empty error" : error));
                std::this_thread::sleep_for(std::chrono::milliseconds(180));
            }
        }
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - totalStarted).count();
        LogLine(L"[Translate] \"" + value + L"\" provider[" + std::to_wstring(providerIndex) + L"/" +
            std::to_wstring(providers_.size()) + L"] " + p->Name() + L" -> " +
            (out.empty() ? L"(empty)" : out) +
            L" at=" + requestAt +
            L" ms=" + std::to_wstring(elapsed) +
            L" attempts=" + std::to_wstring(attemptCount) +
            (error.empty() ? L"" : L" err=" + error));

        if (!out.empty()) {
            std::wstring fixed = FixProviderLeftoverShorthand(out);
            if (fixed != out) {
                LogLine(L"[Translate] \"" + value + L"\" shorthand fix: " + out + L" -> " + fixed);
                out = std::move(fixed);
            }
            std::wstring preserved = PreserveMentionPrefix(value, out);
            if (preserved != out) {
                LogLine(L"[Translate] \"" + value + L"\" mention fix: " + out + L" -> " + preserved);
                out = std::move(preserved);
            }
        }

        if (!LooksUntranslated(value, out, runtime_)) {
            NoteProviderResult(i, true, L"");
            RememberCache(value, out);
            return out;
        }
        if (!out.empty() && error.empty()) error = L"returned original/non-target text";
        NoteProviderResult(i, false, error);
        if (!errors.empty()) errors += L" | ";
        errors += p->Name() + L": " + error;
    }

    {
        std::lock_guard<std::mutex> g(errorLock_);
        lastError_ = errors.empty() ? L"translation failed" : errors;
    }
    return errors.empty() ? L"" : L"[\x7FFB\x8BD1\x5931\x8D25: " + errors + L"]";
}

void TranslateEngine::RememberCache(const std::wstring& text, const std::wstring& translated)
{
    std::lock_guard<std::mutex> g(cacheLock_);
    if (cache_.size() >= runtime_.cacheLimit && !cache_.empty()) cache_.erase(cache_.begin());
    cache_[text] = translated;
}

TranslateEngine::ProviderSlot::ProviderSlot(TranslateEngine& engine, size_t index, bool acquired)
    : engine_(&engine), index_(index), acquired_(acquired)
{
}

TranslateEngine::ProviderSlot::~ProviderSlot()
{
    if (engine_ && acquired_) engine_->ReleaseProviderSlot(index_);
}

TranslateEngine::ProviderSlot::ProviderSlot(ProviderSlot&& other) noexcept
    : engine_(other.engine_), index_(other.index_), acquired_(other.acquired_)
{
    other.engine_ = nullptr;
    other.acquired_ = false;
}

TranslateEngine::ProviderSlot TranslateEngine::AcquireProviderSlot(size_t index)
{
    int intervalMs = 80;
    if (index < providers_.size()) intervalMs = ProviderIntervalMs(providers_[index]->Kind());
    int maxInFlight = 1;
    if (index < providers_.size()) maxInFlight = ProviderMaxInFlight(*providers_[index]);

    while (running_) {
        auto now = std::chrono::steady_clock::now();
        std::chrono::steady_clock::time_point waitUntil{};
        {
            std::lock_guard<std::mutex> g(providerHealthLock_);
            if (index >= providerHealth_.size()) return ProviderSlot(*this, index, false);
            auto& h = providerHealth_[index];
            if (h.active < maxInFlight && h.nextAllowed <= now) {
                ++h.active;
                if (intervalMs > 0) h.nextAllowed = now + std::chrono::milliseconds(intervalMs);
                return ProviderSlot(*this, index, true);
            }
            waitUntil = h.nextAllowed > now ? h.nextAllowed : now + std::chrono::milliseconds(25);
        }

        auto waitMs = std::chrono::duration_cast<std::chrono::milliseconds>(waitUntil - now);
        if (waitMs.count() <= 0) continue;
        std::this_thread::sleep_for((std::min)(waitMs, std::chrono::milliseconds(80)));
    }
    return ProviderSlot(*this, index, false);
}

void TranslateEngine::ReleaseProviderSlot(size_t index)
{
    std::lock_guard<std::mutex> g(providerHealthLock_);
    if (index >= providerHealth_.size()) return;
    auto& h = providerHealth_[index];
    if (h.active > 0) --h.active;
}

size_t TranslateEngine::PendingJobCount() const
{
    std::lock_guard<std::mutex> g(jobsLock_);
    return jobs_.size();
}

bool TranslateEngine::ProviderCoolingDown(size_t index, std::chrono::steady_clock::time_point now) const
{
    std::lock_guard<std::mutex> g(providerHealthLock_);
    if (index >= providerHealth_.size()) return false;
    return providerHealth_[index].coolUntil > now;
}

void TranslateEngine::NoteProviderResult(size_t index, bool success, const std::wstring& error)
{
    std::lock_guard<std::mutex> g(providerHealthLock_);
    if (index >= providerHealth_.size()) return;
    auto& h = providerHealth_[index];
    if (success) {
        h.failures = 0;
        h.coolUntil = {};
        if (h.nextAllowed < std::chrono::steady_clock::now()) h.nextAllowed = {};
        return;
    }

    ++h.failures;
    auto now = std::chrono::steady_clock::now();
    if (error.find(L"HTTP 429") != std::wstring::npos || error.find(L"ACCESS_FREQUENCY_LIMITED") != std::wstring::npos) {
        h.nextAllowed = (std::max)(h.nextAllowed, now + std::chrono::milliseconds((std::min)(2500, 400 + h.failures * 350)));
    }
    if (PermanentProviderError(error)) {
        h.coolUntil = now + std::chrono::minutes(3);
        return;
    }
    if (error.find(L"empty content") != std::wstring::npos || error.find(L"cannot parse response") != std::wstring::npos) {
        h.nextAllowed = (std::max)(h.nextAllowed, now + std::chrono::milliseconds((std::min)(2200, 400 + h.failures * 300)));
        if (h.failures >= 2) {
            h.coolUntil = now + std::chrono::seconds((std::min)(35, 5 + h.failures * 5));
        }
        return;
    }
    if (RetryableProviderError(error) && h.failures >= 2) {
        int seconds = (std::min)(45, 8 + h.failures * 6);
        h.coolUntil = now + std::chrono::seconds(seconds);
    }
}

void TranslateEngine::LogLine(const std::wstring& line) const
{
    if (logger_) logger_(line);
}

bool TranslateEngine::ShouldTranslate(const std::wstring& text)
{
    std::wstring value = text::Trim(text);
    if (value.empty()) return false;
    if (!ShortPhraseFallback(value).empty()) return true;
    if (LooksLikeGestureOnly(value) || IsNonTranslatableChatText(value)) return false;
    return !text::MostlyChinese(value);
}

void TranslateEngine::SubmitCompose(const std::wstring& text, ComposeDone onDone)
{
    if (text.empty()) {
        if (onDone) onDone(text);
        return;
    }

    std::thread([this, text, onDone = std::move(onDone)]() {
        HttpAgent http(runtime_.timeoutMs);

        std::wstring translated;
        std::wstring error;

        for (size_t i = 0; i < providers_.size(); ++i) {
            if (!providers_[i] || !providers_[i]->Settings().enabled) continue;

            // Swap source and target: translate user's Chinese input back to the
            // language that the provider normally translates FROM.
            // sourceLanguage (e.g. "auto" / "en") -> targetLanguage
            // targetLanguage (e.g. "zh-CN")   -> sourceLanguage
            // Result: zh-CN input → en output (or whatever the source is)
            std::wstring composeSource = providers_[i]->Settings().targetLanguage;
            std::wstring composeTarget = (providers_[i]->Settings().sourceLanguage == L"auto"
                || providers_[i]->Settings().sourceLanguage.empty())
                ? L"en"
                : providers_[i]->Settings().sourceLanguage;

            RuntimeConfig composeRuntime = runtime_;
            composeRuntime.targetLanguage = composeTarget;

            // Build a temporary settings with swapped source/target so
            // provider-internal helpers use the right direction
            ProviderSettings swapped = providers_[i]->Settings();
            swapped.sourceLanguage = composeSource;
            swapped.targetLanguage = composeTarget;

            // We can't call providers_[i]->Translate directly with a different
            // ProviderSettings because the provider owns its settings.
            // But Translate receives RuntimeConfig; EffectiveTarget checks
            // settings_.targetLanguage first — which is still zh-CN.
            // Workaround: create runtime with a fake targetLanguage that is really
            // what we want, BUT the provider will still check its own settings.
            // The key insight: translate the text with the original provider but
            // swap the meaning of source/target.

            // Actually, the simplest approach: the provider prompts say
            // "translate chat into {target}". If target=en and source=zh-CN:
            // the provider translates Chinese→English as desired.
            // 
            // The problem is EffectiveTarget() reads settings_.targetLanguage = "zh-CN"
            // So composeRuntime.targetLanguage gets ignored.
            //
            // FIX: override runtime.targetLanguage AND composeRuntime.targetLanguage
            // but we need the provider prompt to say "translate into en".
            // Temporarily swap the provider's source/target settings
            // so Translate() sends Chinese→English (or whatever source is)
            auto& prov = providers_[i];
            auto& settings = const_cast<ProviderSettings&>(prov->Settings());
            auto origTarget = settings.targetLanguage;
            auto origSource = settings.sourceLanguage;
            settings.targetLanguage = composeTarget;
            settings.sourceLanguage = composeSource;

            translated = prov->Translate(text, composeRuntime, http, error);

            // Restore original settings
            settings.targetLanguage = origTarget;
            settings.sourceLanguage = origSource;

            if (!translated.empty() && translated != text &&
                !LooksUntranslated(text, translated, composeRuntime)) {
                break;
            }
            translated.clear();
        }

        if (translated.empty()) translated = text;
        if (onDone) onDone(translated);
    }).detach();
}
