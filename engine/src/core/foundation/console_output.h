#pragma once

#include "conjure/core/log.h"

#include <cstdio>

// What console_log_sink writes and where, split from the sink so the routing, the colour codes and
// the flushing can be tested against an ordinary file instead of the process's own console.

namespace conjure::core::detail {

    /// <summary>
    /// The stream a console line of 'level' goes to: stdout below WARN, stderr from WARN up.
    /// </summary>
    [[nodiscard]] std::FILE *console_stream_for(log_level level) noexcept;

    /// <summary>
    /// Writes format_log_line(message) and a newline to 'stream', wrapped in the level's ANSI
    /// colour and a reset when 'useColour' is set. Flushes the stream for ERR and above so the
    /// line survives a crash that follows.
    /// </summary>
    void write_console_line(std::FILE *stream, const log_message &message, bool useColour);
}
