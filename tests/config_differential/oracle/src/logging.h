#pragma once

#include "cameraunlock/logging/file_log.h"

// The process-wide log lives in cameraunlock-core. Alias it under the mod
// namespace so call sites read Log::Line(...) unqualified.
namespace DyingLightHeadTracking {
namespace Log = ::cameraunlock::logging;
}
