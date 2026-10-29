#pragma once

#include <ct/socket.hpp>
#include <ct/string.hpp>
#include <ct/vector.hpp>

#include <cstdint>

namespace zen
{
    class VM;
    struct ObjString;
}

namespace k2d
{

    // The game's side of the script debugger. It connects to the editor on
    // 127.0.0.1:port and, through the VM's debug hook, stops the scripts at a
    // breakpoint, a step or a pause: it sends where, with every frame and its
    // locals, then waits for the editor to say continue or step. The game
    // freezes while it waits, so the editor, in its own process, stays free.
    //
    // One JSON object a line. From the editor: {"cmd":"breakpoints","file":f,
    // "lines":[..]}, {"cmd":"pause"}, {"cmd":"continue"}, {"cmd":"step_over"},
    // {"cmd":"step_in"}, {"cmd":"step_out"}. To it: {"event":"stopped",
    // "reason":r,"frames":[{"function":n,"file":f,"line":l,"locals":[{"name":
    // n,"value":v}]}]} and {"event":"running"}.
    class ZenDebugger
    {
    public:
        bool Connect(std::uint16_t port);
        void Disconnect();
        bool Connected() const { return mStream.valid(); }
        // Once a frame: what the editor sent since, without waiting.
        void Poll();

    private:
        enum class Step
        {
            None,
            In,
            Over,
            Out
        };
        struct Breakpoints
        {
            ct::String file;
            ct::Vector<int> lines;
        };

        static void hook(zen::VM *vm, void *user);
        void onLine(zen::VM *vm);
        void stop(zen::VM *vm, const char *reason);
        // Commands in the bytes read so far; false once the editor says go on.
        bool handle(const ct::String &line, zen::VM *vm);
        bool readLines(zen::VM *vm, bool wait);
        bool breakpointAt(const zen::ObjString *source, int line);
        void send(const ct::String &text);

        ct::TcpStream mStream;
        ct::String mPending;
        ct::Vector<Breakpoints> mBreakpoints;
        const zen::ObjString *mCachedSource = nullptr;
        const Breakpoints *mCachedLines = nullptr;
        Step mStep = Step::None;
        int mStepDepth = 0;
        bool mPauseRequested = false;
        bool mResume = false;
        unsigned mLinesSincePoll = 0;
    };

    ZenDebugger &GetZenDebugger();

}
