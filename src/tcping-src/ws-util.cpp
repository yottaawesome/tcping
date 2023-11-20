#include "ws-util.h"

#include <iostream>
#include <algorithm>
#include <strstream>
#include <string>

//// WSAGetLastErrorMessage ////////////////////////////////////////////
// A function similar in spirit to Unix's perror() that tacks a canned
// interpretation of the value of WSAGetLastError() onto the end of a
// passed string, separated by a ": ".  Generally, you should implement
// smarter error handling than this, but for default cases and simple
// programs, this function is sufficient.
//
// This function returns a pointer to an internal static buffer, so you
// must copy the data from this function before you call it again.  It
// follows that this function is also not thread-safe.

std::string WSAGetLastErrorMessage(
    const std::string& pcMessagePrefix, 
    const int nErrorId
)
{
    void* buffer = nullptr;
    HMODULE ws2 = GetModuleHandleA("ws2_32.dll");
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        ws2,
        nErrorId,
        0,
        reinterpret_cast<LPSTR>(buffer),
        0,
        nullptr
    );
    if (!buffer)
        return "Failed to format message";
    std::string errorMessage(static_cast<char*>(buffer));
    LocalFree(buffer);
    return pcMessagePrefix + errorMessage;
}


//// ShutdownConnection ////////////////////////////////////////////////
// Gracefully shuts the connection sd down.  Returns true if we're
// successful, false otherwise.

bool ShutdownConnection(SOCKET sd) 
{
    const int kBufferSize = 1024;
    // Disallow any further data sends.  This will tell the other side
    // that we want to go away now.  If we skip this step, we don't
    // shut the connection down nicely.
    if (shutdown(sd, SD_SEND) == SOCKET_ERROR) 
    {
        closesocket(sd);
        return false;
    }

    // Receive any extra data still sitting on the socket.  After all
    // data is received, this call will block until the remote host
    // acknowledges the TCP control packet sent by the shutdown above.
    // Then we'll get a 0 back from recv, signalling that the remote
    // host has closed its side of the connection.
    char acReadBuffer[kBufferSize];
    while (1) 
    {
        int nNewBytes = recv(sd, acReadBuffer, kBufferSize, 0);
        if (nNewBytes == SOCKET_ERROR) 
        {
            closesocket(sd);
            return false;
        } 
        else if (nNewBytes != 0) 
        {
            std::cout << " (" << nNewBytes << " bytes read)";
        } 
        else 
        {
            break;
        }
    }

    // Close the socket.
    if (closesocket(sd) == SOCKET_ERROR)
        return false;

    return true;
}

