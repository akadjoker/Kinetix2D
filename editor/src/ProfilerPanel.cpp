#include "ProfilerPanel.h"

#include "Icons.h"

#include "BasicWidgets.hpp"
#include "ScrollWidgets.hpp"
#include "Theme.hpp"
#include "ToolBar.hpp"

#include <k2d/Profiler.h>

#include <cstdio>
#include <cstring>

namespace rt = ig::retained;

namespace k2d::editor
{

// One row a scope: name, a bar of its share of the frame, average, peak,
// calls, and its history as a small graph.
class ProfileView : public rt::Widget
{
public:
    IG_WIDGET_CLASS(ProfileView, rt::Widget);

    void capture()
    {
        const Profiler& profiler = Profiler::Get();
        mFrame = profiler.frameMilliseconds();
        mCount = profiler.sampleCount();
        std::memcpy(mSamples, profiler.samples(), sizeof(ProfileSample) * mCount);
        // The heaviest first: that is what one opens a profiler to find.
        for (uint32_t i = 1; i < mCount; ++i)
            for (uint32_t j = i; j > 0 && mSamples[j].average > mSamples[j - 1].average; --j)
            {
                ProfileSample swap = mSamples[j];
                mSamples[j] = mSamples[j - 1];
                mSamples[j - 1] = swap;
            }
        markDirty();
    }

    Vec2f sizeHint() const override { return {400.0f, 22.0f * static_cast<float>(mCount) + 8.0f}; }

    void paint(rt::PaintContext& ctx) override
    {
        if (!visible_)
            return;
        const rt::Rect abs = absoluteRect();
        ctx.pushClip(abs);
        ctx.fill.SetColor(rt::Color(24, 26, 30, 255));
        ctx.fillRect(abs.x, abs.y, abs.w, abs.h);
        const auto& theme = rt::Theme::instance();
        ctx.font.SetFontSize(theme.fontSize);
        ctx.font.SetBatch(&ctx.text);
        const float asc = ctx.font.GetAscender();
        if (mCount == 0)
        {
            ctx.font.SetColor(theme.textDisabled);
            ctx.font.Print("No samples yet", abs.x + 10.0f, abs.y + 20.0f);
            ctx.popClip();
            return;
        }
        const float rowH = 22.0f;
        const float nameW = 170.0f, numbersW = 230.0f;
        const float barX = abs.x + nameW, barW = abs.w * 0.25f;
        const float graphX = barX + barW + numbersW;
        const float graphW = abs.x + abs.w - graphX - 8.0f;
        for (uint32_t i = 0; i < mCount; ++i)
        {
            const ProfileSample& s = mSamples[i];
            const float y = abs.y + 4.0f + i * rowH;
            if (i % 2 == 1)
            {
                ctx.fill.SetColor(rt::Color(255, 255, 255, 8));
                ctx.fillRect(abs.x, y, abs.w, rowH);
            }
            const float share = mFrame > 0.0f ? s.average / mFrame : 0.0f;
            // Green under a quarter of the frame, then yellow, red past a half.
            const rt::Color color = share < 0.25f ? rt::Color(90, 200, 120, 255)
                                    : share < 0.5f ? rt::Color(230, 200, 80, 255)
                                                   : rt::Color(230, 90, 80, 255);
            ctx.font.SetColor(theme.textColor);
            ctx.font.Print(ctx.fitText(s.name, nameW - 12.0f).c_str(), abs.x + 8.0f, y + (rowH - theme.fontSize) * 0.5f + asc);
            ctx.fill.SetColor(rt::Color(50, 52, 60, 255));
            ctx.fillRect(barX, y + 5.0f, barW, rowH - 10.0f);
            ctx.fill.SetColor(color);
            ctx.fillRect(barX, y + 5.0f, barW * (share > 1.0f ? 1.0f : share), rowH - 10.0f);
            char numbers[96];
            std::snprintf(numbers, sizeof numbers, "%6.2f ms  max %6.2f  x%u", s.average, s.maximum,
                          static_cast<unsigned>(s.displayCalls));
            ctx.font.SetColor(theme.textDisabled);
            ctx.font.Print(numbers, barX + barW + 10.0f, y + (rowH - theme.fontSize) * 0.5f + asc);

            // The history, oldest on the left, scaled to the row's own peak.
            if (graphW > 20.0f && s.historyCount > 1)
            {
                float peak = 0.001f;
                for (uint32_t h = 0; h < s.historyCount; ++h)
                    peak = s.history[h] > peak ? s.history[h] : peak;
                const uint32_t count = s.historyCount;
                const float step = graphW / static_cast<float>(ProfileSample::HistorySize);
                ctx.fill.SetColor(rt::Color(color.r, color.g, color.b, 170));
                for (uint32_t h = 0; h < count; ++h)
                {
                    const uint32_t index = (s.historyCursor + ProfileSample::HistorySize - count + h) % ProfileSample::HistorySize;
                    const float v = s.history[index] / peak;
                    const float barH = (rowH - 6.0f) * v;
                    ctx.fillRect(graphX + h * step, y + rowH - 3.0f - barH, step > 1.0f ? step - 0.5f : step, barH);
                }
            }
        }
        ctx.popClip();
    }

private:
    ProfileSample mSamples[Profiler::MaxSamples];
    uint32_t mCount = 0;
    float mFrame = 0.0f;
};

ProfilerPanel::ProfilerPanel() : rt::BoxLayout(rt::LayoutDir::Vertical)
{
    setPadding(0.0f);
    setSpacing(0.0f);
    auto* bar = createChild<rt::BoxLayout>(rt::LayoutDir::Horizontal);
    bar->setPadding(2.0f, 4.0f);
    bar->setSpacing(8.0f);
    auto* tools = bar->createChild<rt::ToolBar>();
    tools->setDraggable(false);
    const int freezeItem = tools->addToggle(Icons::get("Pause"), "Freeze the numbers to read them");
    tools->toggleChanged.connect([this, freezeItem](int item, bool on)
    {
        if (item == freezeItem)
            mFrozen = on;
    });
    mFrame = bar->createChild<rt::Label>("");
    mFrame->setStretch(1.0f);
    mFrame->setLayoutAlign(rt::Align::Center);
    auto* scroll = createChild<rt::ScrollView>();
    scroll->setStretch(1.0f);
    mView = scroll->setContent<ProfileView>();
}

void ProfilerPanel::tick()
{
    if (mFrozen || !isVisible())
        return;
    const float frame = Profiler::Get().frameMilliseconds();
    char text[96];
    // The engine's share of the frame: the interface is drawn outside it.
    std::snprintf(text, sizeof text, "engine %.2f ms a frame", frame);
    mFrame->setText(text);
    mView->capture();
}

} // namespace k2d::editor
