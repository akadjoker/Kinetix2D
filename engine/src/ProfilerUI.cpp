#include "k2d/ProfilerUI.h"
#include "k2d/Profiler.h"

#include <imgui.h>
#include <cstdio>
#include <cstring>

namespace k2d
{

    static bool IsPhysicsSample(const ProfileSample &sample)
    {
        return std::strncmp(sample.name, "kx.", 3) == 0 ||
               std::strncmp(sample.name, "physics.", 8) == 0;
    }

    static void DrawSampleGroup(const char *tableId, bool physics)
    {
        Profiler &profiler = Profiler::Get();
        const ProfileSample *samples = profiler.samples();
        uint32_t count = profiler.sampleCount();

        if (ImGui::BeginTable(tableId, 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("ms");
            ImGui::TableSetupColumn("avg");
            ImGui::TableSetupColumn("max");
            ImGui::TableSetupColumn("calls");
            ImGui::TableHeadersRow();

            for (uint32_t i = 0; i < count; ++i)
            {
                const ProfileSample &sample = samples[i];
                if (IsPhysicsSample(sample) != physics)
                    continue;
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(sample.name);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.3f", sample.display);
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%.3f", sample.average);
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%.3f", sample.maximum);
                ImGui::TableSetColumnIndex(4);
                ImGui::Text("%u", sample.displayCalls);
            }

            ImGui::EndTable();
        }

        if (ImGui::TreeNode(physics ? "graficos##phys" : "graficos##cpu"))
        {
            for (uint32_t i = 0; i < count; ++i)
            {
                const ProfileSample &sample = samples[i];
                if (IsPhysicsSample(sample) != physics)
                    continue;
                int offset = sample.historyCount == ProfileSample::HistorySize
                                 ? (int)sample.historyCursor
                                 : 0;
                char overlay[64];
                std::snprintf(overlay, sizeof(overlay), "%.3f ms", sample.display);
                ImGui::PlotLines(sample.name, sample.history, (int)sample.historyCount, offset,
                                 overlay, 0.0f, sample.maximum > 0.0f ? sample.maximum : 1.0f,
                                 ImVec2(0.0f, 40.0f));
            }
            ImGui::TreePop();
        }
    }

    void ShowProfilerWindow(bool *open)
    {
        if (!ImGui::Begin("Profiler", open))
        {
            ImGui::End();
            return;
        }

        Profiler &profiler = Profiler::Get();

        float frameMs = profiler.frameMilliseconds();
        float fps = frameMs > 0.0f ? 1000.0f / frameMs : 0.0f;
        ImGui::Text("Frame: %.3f ms  (%.1f FPS)", frameMs, fps);

        if (ImGui::CollapsingHeader("CPU", ImGuiTreeNodeFlags_DefaultOpen))
            DrawSampleGroup("cpu_samples", false);

        if (ImGui::CollapsingHeader("Physics", ImGuiTreeNodeFlags_DefaultOpen))
            DrawSampleGroup("physics_samples", true);

        if (ImGui::CollapsingHeader("GPU"))
            ImGui::TextDisabled("GLES 3.0 core: sem timer queries (EXT_disjoint_timer_query no futuro)");

        ImGui::End();
    }

}
