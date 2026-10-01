#pragma once

#include "UIScrollContainer.h"

namespace LambUI {

class LAMBUI_API UITreeView : public UIScrollContainer, public IFocusable {
public:
    using NodeId = size_t;
    static constexpr NodeId RootNode = 0;

    explicit UITreeView(std::string name = {});
    NodeId AddNode(NodeId parent, std::string label);
    void ClearNodes();
    void SetNodeText(NodeId node, std::string label);
    void SetExpanded(NodeId node, bool expanded);
    bool IsExpanded(NodeId node) const;
    void SetSelectedNode(NodeId node);
    NodeId GetSelectedNode() const { return m_selectedNode; }
    size_t GetVisibleNodeCount() const { return m_visibleRows.size(); }
    bool CanFocus() const override { return IsMouseEnabled(); }
    void OnCharacter(char32_t) override {}
    void OnKeyEvent(uint32_t scanCode, bool isDown) override;

protected:
    void OnLayoutChanged() override;
    void OnEvent(const UIEventData& data) override;
    void OnGenerateRenderCommands(std::vector<UIRenderCommand>& bucket) override;

private:
    struct Node {
        std::string label;
        std::vector<NodeId> children;
        bool expanded = true;
    };
    struct Row { NodeId node; size_t depth; };
    void RebuildRows();
    std::vector<Node> m_nodes;
    std::vector<NodeId> m_roots;
    std::vector<Row> m_visibleRows;
    NodeId m_selectedNode = RootNode;
    static constexpr float RowHeight = 24.0f;
    static constexpr float Indent = 18.0f;
};

} // namespace LambUI