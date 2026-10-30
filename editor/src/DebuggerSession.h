#pragma once

#include "Signal.hpp"

#include <ct/json.hpp>
#include <ct/socket.hpp>
#include <ct/string.hpp>

#include <cstdint>

namespace k2d::editor
{

// The editor's side of the script debugger: listens on 127.0.0.1 for the
// runner started with --debug=port, hands it the breakpoints, and turns its
// messages into signals; never waits, so the editor keeps drawing while the
// game is stopped. See k2d::ZenDebugger for the messages.
class DebuggerSession
{
public:
    ~DebuggerSession();

    bool listen(std::uint16_t port);
    void close();
    bool listening() const { return mListener.valid(); }
    bool connected() const { return mStream.valid(); }
    bool stopped() const { return mStopped; }

    // Once a frame: takes the runner's connection, reads its messages.
    void poll();
    void send(const ct::Json& message);
    void command(const char* name);

    // The runner connected and waits for the breakpoints before it starts.
    ig::retained::Signal<> connectedToGame;
    // Where it stopped and why: {"reason":..., "frames":[...]}.
    ig::retained::Signal<const ct::Json&> stoppedAt;
    ig::retained::Signal<> resumed;
    ig::retained::Signal<> ended;

private:
    void handle(const ct::String& line);

    ct::TcpListener mListener;
    ct::TcpStream mStream;
    ct::String mPending;
    bool mStopped = false;
};

} // namespace k2d::editor
