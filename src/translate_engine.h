#pragma once

#include "core_types.h"
#include "http_agent.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>

class TranslateProvider
{
public:
    using Logger = std::function<void(const std::wstring& line)>;

    explicit TranslateProvider(ProviderSettings settings) : settings_(std::move(settings)) {}
    virtual ~TranslateProvider() = default;

    virtual bool Ready() const = 0;
    virtual std::wstring Translate(const std::wstring& input, const RuntimeConfig& runtime, HttpAgent& http, std::wstring& error) = 0;
    std::wstring Kind() const { return settings_.kind; }
    std::wstring Name() const { return settings_.label.empty() ? settings_.kind : settings_.label; }
    const ProviderSettings& Settings() const { return settings_; }
    void SetLogger(Logger logger) { logger_ = std::move(logger); }

protected:
    void LogLine(const std::wstring& line) const { if (logger_) logger_(line); }

    ProviderSettings settings_;
    Logger logger_;
};

class TranslateEngine
{
public:
    using Done = std::function<void(unsigned int id, const std::wstring& translated)>;
    using Logger = std::function<void(const std::wstring& line)>;
    using ComposeDone = std::function<void(const std::wstring& translated)>;

    TranslateEngine();
    ~TranslateEngine();

    bool Start(RuntimeConfig runtime, std::vector<ProviderSettings> providers, Done done);
    void Stop();
    void Submit(unsigned int id, const std::wstring& text);
    void SubmitCompose(const std::wstring& text, ComposeDone onDone);
    void SetLogger(Logger logger) { logger_ = std::move(logger); }

    size_t ProviderCount() const { return providers_.size(); }
    int WorkerCount() const { return activeWorkers_; }
    std::wstring LastError() const;

    static bool ShouldTranslate(const std::wstring& text);

private:
    struct Job
    {
        unsigned int id = 0;
        std::wstring text;
    };

    struct ProviderHealth
    {
        int failures = 0;
        int active = 0;
        std::chrono::steady_clock::time_point coolUntil{};
        std::chrono::steady_clock::time_point nextAllowed{};
    };

    class ProviderSlot
    {
    public:
        ProviderSlot(TranslateEngine& engine, size_t index, bool acquired);
        ~ProviderSlot();
        ProviderSlot(const ProviderSlot&) = delete;
        ProviderSlot& operator=(const ProviderSlot&) = delete;
        ProviderSlot(ProviderSlot&& other) noexcept;
        ProviderSlot& operator=(ProviderSlot&& other) noexcept = delete;
        explicit operator bool() const { return acquired_; }

    private:
        TranslateEngine* engine_ = nullptr;
        size_t index_ = 0;
        bool acquired_ = false;
    };

    void Worker();
    std::wstring RunProviders(const std::wstring& text, HttpAgent& http);
    void RememberCache(const std::wstring& text, const std::wstring& translated);
    ProviderSlot AcquireProviderSlot(size_t index);
    void ReleaseProviderSlot(size_t index);
    size_t PendingJobCount() const;
    bool ProviderCoolingDown(size_t index, std::chrono::steady_clock::time_point now) const;
    void NoteProviderResult(size_t index, bool success, const std::wstring& error);
    void LogLine(const std::wstring& line) const;

    RuntimeConfig runtime_;
    Done done_;
    Logger logger_;
    std::vector<std::unique_ptr<TranslateProvider>> providers_;
    std::vector<std::thread> workers_;
    std::queue<Job> jobs_;
    std::unordered_map<std::wstring, std::vector<unsigned int>> inFlight_;
    mutable std::mutex jobsLock_;
    std::condition_variable jobsCv_;
    std::atomic<bool> running_{ false };
    int activeWorkers_ = 0;

    std::unordered_map<std::wstring, std::wstring> cache_;
    mutable std::mutex cacheLock_;
    std::vector<ProviderHealth> providerHealth_;
    mutable std::mutex providerHealthLock_;
    mutable std::mutex errorLock_;
    std::wstring lastError_;
};
