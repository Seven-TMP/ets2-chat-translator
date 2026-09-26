#pragma once

#include "overlay_host.h"

#include <memory>

namespace overlay_link
{
    void Publish(std::shared_ptr<OverlayHost> host);
    void Retract(const OverlayHost* host);
    std::shared_ptr<OverlayHost> Current();
}
