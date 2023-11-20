/***********************************************************************
 ws-util.h - Declarations for the Winsock utility functions module.
***********************************************************************/

#if !defined(WS_UTIL_H)
#define WS_UTIL_H

#include <winsock2.h>
#include <string>

extern std::string WSAGetLastErrorMessage(
    const std::string& prefix,
    int nErrorID = 0
);
extern bool ShutdownConnection(SOCKET sd);

#endif // !defined (WS_UTIL_H)

