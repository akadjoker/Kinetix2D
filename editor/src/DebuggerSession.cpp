#include "DebuggerSession.h"

#include <platform.h>

namespace k2d::editor
{

DebuggerSession::~DebuggerSession()
{
    close();
}

bool DebuggerSession::listen(std::uint16_t port)
{
    close();
    ct::Address address;
    if (!ct::Address::parse("127.0.0.1", port, address) || !mListener.bind(address) || !mListener.listen(1))
    {
        log_error("Debugger: cannot listen on 127.0.0.1:%u", (unsigned)port);
        mListener.close();
        return false;
    }
    mListener.set_nonblocking(true);
    return true;
}

void DebuggerSession::close()
{
    const bool was = mStream.valid();
    mStream.close();
    mListener.close();
    mPending.clear();
    mStopped = false;
    if (was)
        ended.emit();
}

void DebuggerSession::poll()
{
    if (!mStream.valid() && mListener.valid())
    {
        ct::TcpStream stream;
        if (mListener.accept(stream))
        {
            mStream = static_cast<ct::TcpStream&&>(stream);
            mStream.set_nonblocking(true);
            mStream.set_nodelay(true);
        }
    }
    if (!mStream.valid())
        return;
    char buffer[8192];
    for (;;)
    {
        const long got = mStream.recv(buffer, sizeof buffer);
        if (got > 0)
        {
            mPending.append(buffer, static_cast<std::size_t>(got));
            continue;
        }
        if (got == 0 || !mStream.would_block())
        {
            log_info("Debugger: the game ended");
            close();
            return;
        }
        break;
    }
    std::size_t start = 0;
    for (std::size_t end = mPending.find('\n'); end != ct::String::npos; end = mPending.find('\n', start))
    {
        handle(mPending.substr(start, end - start));
        start = end + 1;
    }
    mPending = mPending.substr(start);
}

void DebuggerSession::handle(const ct::String& line)
{
    ct::Json::Error error;
    const ct::Json message = ct::Json::parse(line.c_str(), &error);
    if (error || !message.is_object())
        return;
    const ct::String event = message["event"].as_cstr("");
    if (event == "hello")
        connectedToGame.emit();
    else if (event == "stopped")
    {
        mStopped = true;
        stoppedAt.emit(message);
    }
    else if (event == "running")
    {
        mStopped = false;
        resumed.emit();
    }
}

void DebuggerSession::send(const ct::Json& message)
{
    if (!mStream.valid())
        return;
    ct::String text = message.dump();
    text += '\n';
    mStream.set_nonblocking(false);
    const bool sent = mStream.send_all(text);
    mStream.set_nonblocking(true);
    if (!sent)
        close();
}

void DebuggerSession::command(const char* name)
{
    ct::Json message = ct::Json::object();
    message.set("cmd", ct::Json(name));
    send(message);
}

} // namespace k2d::editor
