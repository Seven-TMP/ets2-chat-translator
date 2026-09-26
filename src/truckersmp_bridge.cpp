#include "truckersmp_bridge.h"

#include "text_codec.h"

#include <algorithm>
#include <cwctype>

namespace
{
std::wstring Normalize(const std::wstring& value)
{
    std::wstring out;
    out.reserve(value.size());
    bool pendingSpace = false;
    for (wchar_t ch : value) {
        if (iswspace(ch)) {
            if (!out.empty()) pendingSpace = true;
            continue;
        }
        if (pendingSpace) {
            out.push_back(L' ');
            pendingSpace = false;
        }
        out.push_back((wchar_t)towlower(ch));
    }
    return out;
}

PlayerRole RoleOf(const TruckersMP::Player& player)
{
    if (player.IsManager().value_or(false)) return PlayerRole::Manager;
    if (player.IsGameModerator().value_or(false)) return PlayerRole::GameModerator;
    if (player.IsTeamMember().value_or(false)) return PlayerRole::TeamMember;
    if (player.IsPatron().value_or(false)) return PlayerRole::Patron;
    return PlayerRole::None;
}
} // namespace

bool TruckersmpBridge::MakeSnapshot(const TruckersMP::Player& player, PlayerSnapshot& out)
{
    const auto name = player.GetUsername();
    if (!name || name->empty()) return false;

    const PlayerRole role = RoleOf(player);
    if (role == PlayerRole::None) return false; 

    std::wstring username = text::FromUtf8(*name);
    const std::wstring normalizedName = Normalize(username);

    std::wstring fullName = normalizedName;
    if (const auto tag = player.GetTagText(); tag && !tag->empty()) {
        std::wstring tagText = text::FromUtf8(*tag);
        tagText.erase(std::remove(tagText.begin(), tagText.end(), L'['), tagText.end());
        tagText.erase(std::remove(tagText.begin(), tagText.end(), L']'), tagText.end());
        if (!tagText.empty()) {
            fullName = Normalize(tagText + L" " + username);
        }
    }

    out.username = normalizedName;
    out.fullName = fullName;
    out.role = role;
    return true;
}

bool TruckersmpBridge::Start(const TruckersMP_Host* host)
{
    Stop();

    session_ = TruckersMP::Session::Create(host);
    if (!session_) {
        return false;
    }

    {
        std::vector<PlayerSnapshot> snapshot;
        if (const auto players = session_->Player().GetAllPlayers()) {
            snapshot.reserve(players->size());
            for (const auto& player : *players) {
                PlayerSnapshot entry;
                if (MakeSnapshot(player, entry)) {
                    snapshot.push_back(std::move(entry));
                }
            }
        }
        std::lock_guard<std::mutex> guard(mutex_);
        players_ = std::move(snapshot);
        cache_.clear();
    }

    session_->Player().OnStreamIn.Register([this](TruckersMP::PlayerStreamInEvent& event) {
        UpsertPlayer(event.GetPlayer());
    });

    session_->Player().OnStreamOut.Register([this](TruckersMP::PlayerStreamOutEvent& event) {
        RemovePlayer(event.GetPlayer());
    });

    if (!overlay_) overlay_ = std::make_unique<SdkOverlay>();
    overlay_->Start(session_.get());

    return true;
}

void TruckersmpBridge::Stop()
{
    if (overlay_) {
        overlay_->Stop(session_.get());
        overlay_.reset();
    }
    session_.reset();
    std::lock_guard<std::mutex> guard(mutex_);
    players_.clear();
    cache_.clear();
}

void TruckersmpBridge::UpsertPlayer(const TruckersMP::Player& player)
{
    PlayerSnapshot entry;
    if (!MakeSnapshot(player, entry)) {
        return;
    }

    std::lock_guard<std::mutex> guard(mutex_);
    for (auto& existing : players_) {
        if (existing.username == entry.username) {
            existing = std::move(entry);
            cache_.clear();
            return;
        }
    }
    players_.push_back(std::move(entry));
    cache_.clear();
}

void TruckersmpBridge::RemovePlayer(const TruckersMP::Player& player)
{
    const auto name = player.GetUsername();
    if (!name) return;

    const std::wstring key = Normalize(text::FromUtf8(*name));
    std::lock_guard<std::mutex> guard(mutex_);
    const auto removed = std::remove_if(players_.begin(), players_.end(),
        [&key](const PlayerSnapshot& entry) { return entry.username == key; });
    if (removed != players_.end()) {
        players_.erase(removed, players_.end());
        cache_.clear();
    }
}

PlayerRole TruckersmpBridge::RoleFor(const std::wstring& author) const
{
    const std::wstring key = Normalize(author);
    if (key.empty()) return PlayerRole::None;

    std::lock_guard<std::mutex> guard(mutex_);
    if (const auto cached = cache_.find(key); cached != cache_.end()) {
        return cached->second;
    }

    PlayerRole best = PlayerRole::None;

    for (const auto& entry : players_) {
        if (entry.fullName == key || entry.username == key) {
            best = entry.role;
            break;
        }
    }

    if (best == PlayerRole::None) {
        for (const auto& entry : players_) {
            if (entry.username.empty() || entry.username.size() >= key.size()) continue;
            const size_t offset = key.size() - entry.username.size();
            if (key.compare(offset, entry.username.size(), entry.username) != 0) continue;
            const wchar_t separator = key[offset - 1];
            if (separator != L' ' && separator != L']') continue;
            if (entry.role > best) best = entry.role;
        }
    }

    cache_[key] = best;
    return best;
}
