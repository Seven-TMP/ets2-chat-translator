#include "overlay_link.h"

#include <mutex>

namespace
{
std::mutex g_lock;
std::weak_ptr<OverlayHost> g_host;
}

namespace overlay_link
{
void Publish(std::shared_ptr<OverlayHost> host)
{
    std::lock_guard<std::mutex> guard(g_lock);
    g_host = std::move(host);
}

void Retract(const OverlayHost* host)
{
    std::lock_guard<std::mutex> guard(g_lock);
    std::shared_ptr<OverlayHost> current = g_host.lock();
    if (current && current.get() == host) {
        g_host.reset();
    }
}

std::shared_ptr<OverlayHost> Current()
{
    std::lock_guard<std::mutex> guard(g_lock);
    return g_host.lock();
}
}
