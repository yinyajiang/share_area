#pragma once
#include <QString>

namespace Constants {
    constexpr int DISCOVERY_PORT = 19876;
    constexpr int DEFAULT_TRANSFER_PORT = 19877;
    constexpr int BROADCAST_INTERVAL_MS = 3000;
    constexpr int PEER_TIMEOUT_MS = 10000;
    constexpr int TRANSFER_CHUNK_SIZE = 262144;
    constexpr int TRANSFER_PIPELINE_CHUNKS = 32;
    constexpr int TRANSFER_PROGRESS_UPDATE_INTERVAL_MS = 120;
    constexpr int TRANSFER_PROGRESS_UPDATE_BYTES = TRANSFER_CHUNK_SIZE * 2;
    const QString APP_NAME = QStringLiteral("ShareArea");
    const QString APP_VERSION = QStringLiteral("1.0.0");
}
