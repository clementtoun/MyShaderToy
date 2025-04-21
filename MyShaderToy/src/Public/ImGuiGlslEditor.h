#pragma once

#include <imgui/imgui.h>
#include <imgui/imgui_stdlib.h>
#include <iostream>

class ImGuiGlslEditor
{
public:
	void Draw();

	void GetText(std::string outText);

private:
	std::string m_EditableText = "";
	std::string m_NbLinesText = "";
};