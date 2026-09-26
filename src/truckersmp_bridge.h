#pragma once

#include <TruckersMP/TruckersMP.hxx>

#include "core_types.h"
#include "sdk_overlay.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

class TruckersmpBridge
{
public:
    bool Start(const TruckersMP_Host* host);
    void Stop();
    bool Available() const { return session_ != nullptr; }

    bool OverlayActive() const { return overlay_ && overlay_->RendererActive(); }

    PlayerRole RoleFor(const std::wstring& author) const;

private:
    struct PlayerSnapshot
    {
        std::wstring username; 
        std::wstring fullName; 
        PlayerRole role = PlayerRole::None;
    };

    static bool MakeSnapshot(const TruckersMP::Player& player, PlayerSnapshot& out);
    void UpsertPlayer(const TruckersMP::Player& player);
    void RemovePlayer(const TruckersMP::Player& player);

    std::unique_ptr<TruckersMP::Session> session_;
    std::unique_ptr<SdkOverlay> overlay_;
    mutable std::mutex mutex_;
    std::vector<PlayerSnapshot> players_;
    mutable std::unordered_map<std::wstring, PlayerRole> cache_;
};
