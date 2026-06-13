#pragma once

#include <string>

struct ChatEntry
{
    unsigned int id = 0;
    std::wstring time;
    std::wstring channel;
    std::wstring author;
    std::wstring body;
    std::wstring translated;
    bool serviceLine = false;
};

struct RuntimeConfig
{
    std::wstring targetLanguage = L"zh-CN";
    std::wstring overlayHotkey = L"Ctrl+Shift+T";
    int workerCount = 8;
    size_t queueLimit = 1000;
    size_t cacheLimit = 1500;
    int timeoutMs = 5000;
    int fontSize = 18;
    int overlayOpacity = 98;
};

struct ProviderSettings
{
    std::wstring kind;
    std::wstring label;
    bool enabled = true;
    std::wstring baseUrl;
    std::wstring apiKey;
    std::wstring model;
    std::wstring sourceLanguage = L"auto";
    std::wstring targetLanguage = L"zh-CN";
    std::wstring apiSecret;
};
