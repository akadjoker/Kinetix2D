#include "k2d/ZenDebugger.h"

#include "k2d/ZenRuntime.h"

#include <zen/object.h>
#include <zen/vm.h>

#include <ct/json.hpp>
#include <platform.h>

#include <cstring>

namespace k2d
{

    namespace
    {
        ct::String slashes(const char *path)
        {
            ct::String out = path ? path : "";
            for (std::size_t i = 0; i < out.size(); ++i)
                if (out[i] == '\\')
                    out[i] = '/';
            return out;
        }

        // The editor names a script by its project path, the VM by the path
        // it was compiled from: the same file when one ends the other.
        bool sameFile(const ct::String &a, const ct::String &b)
        {
            if (a.empty() || b.empty())
                return false;
            const ct::String &longer = a.size() >= b.size() ? a : b;
            const ct::String &shorter = a.size() >= b.size() ? b : a;
            if (longer.size() == shorter.size())
                return longer == shorter;
            const std::size_t at = longer.size() - shorter.size();
            return longer[at - 1] == '/' && std::strcmp(longer.c_str() + at, shorter.c_str()) == 0;
        }
    }

    bool ZenDebugger::Connect(std::uint16_t port)
    {
        ct::Address address;
        if (!ct::Address::parse("127.0.0.1", port, address) || !mStream.connect(address, 2000))
        {
            log_error("Debugger: nothing listening on 127.0.0.1:%u", (unsigned)port);
            mStream.close();
            return false;
        }
        mStream.set_nodelay(true);
        mStream.set_nonblocking(true);
        if (!ZenRuntime::instance().vm().set_debug_hook(&ZenDebugger::hook, this))
        {
            log_error("Debugger: this runner's VM has no debug hook; the editor starts k2d_runner_debug");
            mStream.close();
            return false;
        }
        log_info("Debugger connected on 127.0.0.1:%u", (unsigned)port);
        send("{\"event\":\"hello\"}");
        // The editor sends its breakpoints first; the game starts once they
        // are in, so a breakpoint on the first frame is not missed.
        const double until = time_seconds() + 1.0;
        while (mStream.valid() && time_seconds() < until && !mResume)
            readLines(&ZenRuntime::instance().vm(), true);
        mResume = false;
        return mStream.valid();
    }

    void ZenDebugger::Disconnect()
    {
        if (mStream.valid())
            ZenRuntime::instance().vm().set_debug_hook(nullptr, nullptr);
        mStream.close();
        mPending.clear();
        mBreakpoints.clear();
        mCachedSource = nullptr;
        mCachedLines = nullptr;
        mStep = Step::None;
        mPauseRequested = false;
    }

    void ZenDebugger::Poll()
    {
        if (mStream.valid())
            readLines(&ZenRuntime::instance().vm(), false);
    }

    void ZenDebugger::hook(zen::VM *vm, void *user)
    {
        static_cast<ZenDebugger *>(user)->onLine(vm);
    }

    void ZenDebugger::onLine(zen::VM *vm)
    {
        // A script stuck in a loop still hears a pause.
        if (++mLinesSincePoll >= 4096)
        {
            mLinesSincePoll = 0;
            readLines(vm, false);
        }
        if (!mStream.valid())
            return;
        zen::ObjFunc *function = vm->debug_frame_function(0);
        const int line = vm->debug_frame_line(0);
        const int depth = vm->debug_frame_count();
        const char *reason = nullptr;
        if (mPauseRequested)
            reason = "pause";
        else if (mStep == Step::In || (mStep == Step::Over && depth <= mStepDepth) ||
                 (mStep == Step::Out && depth < mStepDepth))
            reason = "step";
        else if (function && breakpointAt(function->source, line))
            reason = "breakpoint";
        if (reason)
            stop(vm, reason);
    }

    bool ZenDebugger::breakpointAt(const zen::ObjString *source, int line)
    {
        if (!source || mBreakpoints.empty())
            return false;
        if (source != mCachedSource)
        {
            mCachedSource = source;
            mCachedLines = nullptr;
            const ct::String file = slashes(source->chars);
            for (const Breakpoints &entry : mBreakpoints)
                if (sameFile(entry.file, file))
                    mCachedLines = &entry;
        }
        if (!mCachedLines)
            return false;
        for (int at : mCachedLines->lines)
            if (at == line)
                return true;
        return false;
    }

    void ZenDebugger::stop(zen::VM *vm, const char *reason)
    {
        mPauseRequested = false;
        mStep = Step::None;
        ct::Json frames = ct::Json::array();
        const int count = vm->debug_frame_count();
        char text[256];
        for (int depth = 0; depth < count && depth < 32; ++depth)
        {
            const zen::ObjFunc *function = vm->debug_frame_function(depth);
            ct::Json frame = ct::Json::object();
            frame.set("function", ct::Json(function && function->name ? function->name->chars : "?"));
            frame.set("file", ct::Json(function && function->source ? slashes(function->source->chars).c_str() : ""));
            frame.set("line", ct::Json(vm->debug_frame_line(depth)));
            ct::Json locals = ct::Json::array();
            const int localCount = vm->debug_local_count(depth);
            for (int i = 0; i < localCount && i < 64; ++i)
            {
                const char *name = vm->debug_local_name(depth, i);
                if (!name[0] || name[0] == '(')
                    continue;
                vm->debug_value_text(vm->debug_local_value(depth, i), text, sizeof text);
                ct::Json local = ct::Json::object();
                local.set("name", ct::Json(name));
                local.set("value", ct::Json(text));
                locals.push_back(local);
            }
            frame.set("locals", locals);
            frames.push_back(frame);
        }
        ct::Json event = ct::Json::object();
        event.set("event", ct::Json("stopped"));
        event.set("reason", ct::Json(reason));
        event.set("frames", frames);
        send(event.dump());

        mResume = false;
        while (mStream.valid() && !mResume)
            readLines(vm, true);
        mResume = false;
        if (mStream.valid())
            send("{\"event\":\"running\"}");
        else
            vm->set_debug_hook(nullptr, nullptr);
    }

    bool ZenDebugger::readLines(zen::VM *vm, bool wait)
    {
        char buffer[4096];
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
                log_info("Debugger: the editor went away; the game runs on");
                vm->set_debug_hook(nullptr, nullptr);
                mStream.close();
                return false;
            }
            break;
        }
        std::size_t start = 0;
        for (std::size_t end = mPending.find('\n'); end != ct::String::npos; end = mPending.find('\n', start))
        {
            handle(mPending.substr(start, end - start), vm);
            start = end + 1;
        }
        mPending = mPending.substr(start);
        if (wait && !mResume)
            time_sleep(5);
        return true;
    }

    bool ZenDebugger::handle(const ct::String &line, zen::VM *vm)
    {
        ct::Json::Error error;
        const ct::Json message = ct::Json::parse(line.c_str(), &error);
        if (error || !message.is_object())
            return true;
        const ct::String command = message["cmd"].as_cstr("");
        if (command == "breakpoints")
        {
            const ct::String file = slashes(message["file"].as_cstr(""));
            Breakpoints *entry = nullptr;
            for (Breakpoints &existing : mBreakpoints)
                if (existing.file == file)
                    entry = &existing;
            if (!entry)
            {
                mBreakpoints.push_back(Breakpoints());
                entry = &mBreakpoints.back();
                entry->file = file;
            }
            entry->lines.clear();
            const ct::Json &lines = message["lines"];
            for (std::size_t i = 0; lines.is_array() && i < lines.size(); ++i)
                entry->lines.push_back(static_cast<int>(lines[i].as_int(0)));
            mCachedSource = nullptr;
            mCachedLines = nullptr;
        }
        else if (command == "pause")
            mPauseRequested = true;
        else if (command == "continue" || command == "start")
        {
            mStep = Step::None;
            mResume = true;
        }
        else if (command == "step_in" || command == "step_over" || command == "step_out")
        {
            mStep = command == "step_in" ? Step::In : (command == "step_over" ? Step::Over : Step::Out);
            mStepDepth = vm->debug_frame_count();
            mResume = true;
        }
        return !mResume;
    }

    void ZenDebugger::send(const ct::String &text)
    {
        if (!mStream.valid())
            return;
        ct::String line = text;
        line += '\n';
        mStream.set_nonblocking(false);
        const bool sent = mStream.send_all(line);
        mStream.set_nonblocking(true);
        if (!sent)
            mStream.close();
    }

    ZenDebugger &GetZenDebugger()
    {
        static ZenDebugger debugger;
        return debugger;
    }

}
