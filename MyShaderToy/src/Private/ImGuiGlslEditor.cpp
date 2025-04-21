#include "ImGuiGlslEditor.h"

void ImGuiGlslEditor::Draw()
{
    ImGui::Begin("Editor");
      
    ImVec2 ImageRegionAvail = ImGui::GetContentRegionAvail();
    float lineHeight = ImGui::GetTextLineHeight();
    uint32_t nb_lines = 1u;
    if (lineHeight > 0.0f && ImageRegionAvail.y > 0.0f)
        nb_lines = std::min(static_cast<uint32_t>(ImageRegionAvail.y / lineHeight), 10000u);

    float lineNumberWidth = ImGui::CalcTextSize(std::to_string(nb_lines).c_str()).y;

    {
        ImGui::BeginChild("LineNumbers", ImVec2(lineNumberWidth, 0), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        for (uint32_t i = 1; i <= nb_lines; ++i)
            ImGui::TextUnformatted((std::to_string(i)).c_str());
        ImGui::EndChild();
    }

    ImGui::SameLine();

    {
        ImGui::BeginChild("TextEditor", ImVec2(0, ImageRegionAvail.y), false, ImGuiWindowFlags_NoScrollbar);
        ImGui::InputTextMultiline(
            "##editor",
            &m_EditableText,
            ImVec2(-FLT_MIN, -FLT_MIN), 
            ImGuiInputTextFlags_AllowTabInput
        );
        ImGui::EndChild();
    }

    ImGui::End();
}

void ImGuiGlslEditor::GetText(std::string outText)
{
}
