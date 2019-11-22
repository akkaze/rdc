#pragma once
inline void Finalize(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

