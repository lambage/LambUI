#include "LambUI/UITreeView.h"
#include <algorithm>

namespace LambUI {

namespace { constexpr const char* TAG = "UITreeView"; }

UITreeView::UITreeView(std::string name) : UIScrollContainer(std::move(name)) {
    LAMBUI_LOGT(TAG, "constructed '{}'", GetName());
}

UITreeView::NodeId UITreeView::AddNode(NodeId parent, std::string label) {
    LAMBUI_LOGT(TAG, "'{}' AddNode({}, '{}')", GetName(), parent, label);
    if (parent > m_nodes.size()) return RootNode;
    m_nodes.push_back({std::move(label), {}, true});
    const NodeId node = m_nodes.size();
    if (parent == RootNode) m_roots.push_back(node);
    else m_nodes[parent - 1].children.push_back(node);
    RebuildRows();
    return node;
}

void UITreeView::ClearNodes() {
    LAMBUI_LOGT(TAG, "'{}' ClearNodes", GetName());
    const bool hadSelection = m_selectedNode != RootNode;
    m_selectedNode = RootNode;
    m_nodes.clear();
    m_roots.clear();
    RebuildRows();
    if (hadSelection) FireEvent(UIEventData{UIEventType::OnValueChanged});
}

void UITreeView::SetNodeText(NodeId node, std::string label) {
    if (node == RootNode || node > m_nodes.size()) return;
    LAMBUI_LOGT(TAG, "'{}' SetNodeText({}, '{}')", GetName(), node, label);
    m_nodes[node - 1].label = std::move(label);
    MarkDirty();
}

bool UITreeView::IsExpanded(NodeId node) const {
    return node != RootNode && node <= m_nodes.size() && m_nodes[node - 1].expanded;
}

void UITreeView::SetExpanded(NodeId node, bool expanded) {
    if (node == RootNode || node > m_nodes.size() || IsExpanded(node) == expanded) return;
    LAMBUI_LOGT(TAG, "'{}' SetExpanded({}, {})", GetName(), node, expanded);
    m_nodes[node - 1].expanded = expanded;
    RebuildRows();
}

void UITreeView::SetSelectedNode(NodeId node) {
    if (node > m_nodes.size() || node == m_selectedNode) return;
    LAMBUI_LOGT(TAG, "'{}' selection {} -> {}", GetName(), m_selectedNode, node);
    m_selectedNode = node;
    FireEvent(UIEventData{UIEventType::OnValueChanged});
}

void UITreeView::RebuildRows() {
    LAMBUI_LOGT(TAG, "'{}' RebuildRows", GetName());
    m_visibleRows.clear();
    std::vector<Row> pending;
    for (auto root = m_roots.rbegin(); root != m_roots.rend(); ++root) pending.push_back({*root, 0});
    while (!pending.empty()) {
        const Row row = pending.back();
        pending.pop_back();
        m_visibleRows.push_back(row);
        const auto& node = m_nodes[row.node - 1];
        if (!node.expanded) continue;
        for (auto child = node.children.rbegin(); child != node.children.rend(); ++child) {
            pending.push_back({*child, row.depth + 1});
        }
    }
    SetContentSize(std::max(0.0f, GetComputedRect().width), static_cast<float>(m_visibleRows.size()) * RowHeight);
}

void UITreeView::OnKeyEvent(uint32_t scanCode, bool isDown) {
    LAMBUI_LOGT(TAG, "'{}' OnKeyEvent({}, {})", GetName(), scanCode, isDown);
    if (!isDown || m_visibleRows.empty()) return;
    if (scanCode != ScanCode::Up && scanCode != ScanCode::Down && scanCode != ScanCode::Left &&
        scanCode != ScanCode::Right && scanCode != ScanCode::Home && scanCode != ScanCode::End &&
        scanCode != ScanCode::Enter && scanCode != ScanCode::Space) return;
    auto selected = std::find_if(m_visibleRows.begin(), m_visibleRows.end(), [this](const Row& row) { return row.node == m_selectedNode; });
    if (selected == m_visibleRows.end()) SetSelectedNode(m_visibleRows.front().node);
    else {
        const size_t index = static_cast<size_t>(selected - m_visibleRows.begin());
        const Row row = *selected;
        const auto children = m_nodes[row.node - 1].children;
        if (scanCode == ScanCode::Down) SetSelectedNode(m_visibleRows[std::min(index + 1, m_visibleRows.size() - 1)].node);
        else if (scanCode == ScanCode::Up) SetSelectedNode(m_visibleRows[index == 0 ? 0 : index - 1].node);
        else if (scanCode == ScanCode::Right && !children.empty()) {
            if (!IsExpanded(row.node)) SetExpanded(row.node, true);
            else SetSelectedNode(children.front());
        } else if (scanCode == ScanCode::Left) {
            if (!children.empty() && IsExpanded(row.node)) SetExpanded(row.node, false);
            else {
                for (size_t parent = index; parent > 0; --parent) {
                    if (m_visibleRows[parent - 1].depth < row.depth) {
                        SetSelectedNode(m_visibleRows[parent - 1].node);
                        break;
                    }
                }
            }
        } else if ((scanCode == ScanCode::Enter || scanCode == ScanCode::Space) && !children.empty()) {
            SetExpanded(row.node, !IsExpanded(row.node));
        }
    }
    if (scanCode == ScanCode::Home) SetSelectedNode(m_visibleRows.front().node);
    else if (scanCode == ScanCode::End) SetSelectedNode(m_visibleRows.back().node);
    selected = std::find_if(m_visibleRows.begin(), m_visibleRows.end(), [this](const Row& row) { return row.node == m_selectedNode; });
    if (selected != m_visibleRows.end()) {
        const auto& rect = GetComputedRect();
        EnsureVisible(UIRect{rect.x, rect.y + static_cast<float>(selected - m_visibleRows.begin()) * RowHeight - GetScrollY(), 0.0f, RowHeight});
    }
}

void UITreeView::OnLayoutChanged() {
    LAMBUI_LOGT(TAG, "'{}' OnLayoutChanged", GetName());
    UIScrollContainer::OnLayoutChanged();
    RebuildRows();
}

void UITreeView::OnEvent(const UIEventData& data) {
    LAMBUI_LOGT(TAG, "'{}' OnEvent({})", GetName(), ToString(data.type));
    if (data.type == UIEventType::OnMouseDown || data.type == UIEventType::OnMouseUp) data.handled = true;
    if (data.type != UIEventType::OnClick || data.button != MouseButton::Left || !HitTest(data.mouseX, data.mouseY)) return;
    data.handled = true;
    const auto& rect = GetComputedRect();
    const float localY = data.mouseY - rect.y + GetScrollY();
    const size_t index = static_cast<size_t>(localY / RowHeight);
    if (index >= m_visibleRows.size()) return;
    const Row row = m_visibleRows[index];
    const float disclosureX = static_cast<float>(row.depth) * Indent;
    const float localX = data.mouseX - rect.x + GetScrollX();
    if (!m_nodes[row.node - 1].children.empty() && localX >= disclosureX && localX < disclosureX + Indent) {
        SetExpanded(row.node, !IsExpanded(row.node));
    } else {
        SetSelectedNode(row.node);
    }
}

void UITreeView::OnGenerateRenderCommands(std::vector<UIRenderCommand>& bucket) {
    const auto& rect = GetComputedRect();
    UIRenderCommand background;
    background.x = rect.x;
    background.y = rect.y;
    background.width = std::max(0.0f, rect.width);
    background.height = std::max(0.0f, rect.height);
    background.color = 0x303030FFu;
    bucket.push_back(background);
    for (size_t index = 0; index < m_visibleRows.size(); ++index) {
        const auto& row = m_visibleRows[index];
        const auto& node = m_nodes[row.node - 1];
        const float rowY = rect.y + static_cast<float>(index) * RowHeight - GetScrollY();
        if (rowY + RowHeight <= rect.y || rowY >= rect.y + rect.height) continue;
        if (m_selectedNode == row.node) {
            UIRenderCommand selection = background;
            selection.y = rowY;
            selection.height = RowHeight;
            selection.color = 0x506070FFu;
            bucket.push_back(selection);
        }
        UIRenderCommand label;
        label.type = RenderCommandType::DrawString;
        label.x = rect.x + static_cast<float>(row.depth) * Indent - GetScrollX() + 2.0f;
        label.y = rowY + 3.0f;
        if (!node.children.empty()) {
            label.text = node.expanded ? "-" : "+";
            bucket.push_back(label);
        }
        label.x += Indent;
        label.text = node.label;
        bucket.push_back(label);
    }
}

} // namespace LambUI