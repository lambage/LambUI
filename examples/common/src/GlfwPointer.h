#pragma once

#include "LambUI/UILog.h"
#include "LambUI/UITypes.h"
#include <GLFW/glfw3.h>
#include <memory>
#include <vector>

namespace LambUIExamples {

class GlfwPointer {
public:
    explicit GlfwPointer(GLFWwindow* window) : m_window(window) {
        LAMBUI_LOGT(TAG, "Construct native cursors");
        m_cursors.reserve(5);
        for (const int shape : {GLFW_ARROW_CURSOR, GLFW_RESIZE_EW_CURSOR, GLFW_RESIZE_NS_CURSOR,
                               GLFW_RESIZE_NWSE_CURSOR, GLFW_RESIZE_NESW_CURSOR}) {
            m_cursors.emplace_back(glfwCreateStandardCursor(shape), glfwDestroyCursor);
            if (!m_cursors.back()) LAMBUI_LOGW(TAG, "Native cursor {} unavailable; using default arrow", shape);
        }
    }

    ~GlfwPointer() {
        LAMBUI_LOGT(TAG, "Detach and destroy native cursors");
        glfwSetCursor(m_window, nullptr);
    }

    GlfwPointer(const GlfwPointer&) = delete;
    GlfwPointer& operator=(const GlfwPointer&) = delete;

    bool Apply(LambUI::PointerShape shape) {
        if (m_applied && shape == m_current) return true;
        const auto index = static_cast<size_t>(shape);
        if (index >= m_cursors.size()) return false;
        LAMBUI_LOGT(TAG, "Apply({})", LambUI::ToString(shape));
        glfwGetError(nullptr);
        glfwSetCursor(m_window, m_cursors[index].get());
        if (glfwGetError(nullptr) != GLFW_NO_ERROR) return false;
        m_current = shape;
        m_applied = true;
        return true;
    }

    bool SmokeTest() {
        LAMBUI_LOGT(TAG, "Check standard native cursors");
        bool passed = true;
        for (size_t index = 0; index < m_cursors.size(); ++index) {
            passed = Apply(static_cast<LambUI::PointerShape>(index)) && passed;
            passed = m_cursors[index] != nullptr && passed;
        }
        passed = Apply(LambUI::PointerShape::Arrow) && passed;
        LAMBUI_LOGI(TAG, "Standard native cursors: {}", passed ? "PASS" : "FAIL");
        return passed;
    }

private:
    static constexpr const char* TAG = "GlfwPointer";
    GLFWwindow* m_window;
    std::vector<std::unique_ptr<GLFWcursor, decltype(&glfwDestroyCursor)>> m_cursors;
    LambUI::PointerShape m_current = LambUI::PointerShape::Arrow;
    bool m_applied = false;
};

}