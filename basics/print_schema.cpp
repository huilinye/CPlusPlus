#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>

struct TreeNode {
    std::string value;
    std::vector<std::shared_ptr<TreeNode>> children;
};

struct PrintableNode {
    std::string value;
    int position;
    std::shared_ptr<PrintableNode> parent;
    std::vector<std::shared_ptr<PrintableNode>> children;
};

using TreeNodePtr = std::shared_ptr<TreeNode>;
using PrintableNodePtr = std::shared_ptr<PrintableNode>;

void groupSiblings(TreeNodePtr node, PrintableNodePtr parent, std::vector<std::vector<PrintableNodePtr>>& lines, int depth) {
    if (lines.size() <= depth) {
        lines.push_back({});
    }
    auto printableNode = std::make_shared<PrintableNode>();
    printableNode->value = node->value;
    printableNode->parent = parent;
    lines[depth].push_back(printableNode);
    if (parent) {
        parent->children.push_back(printableNode);
    }
    for (auto& child : node->children) {
        groupSiblings(child, printableNode, lines, depth + 1);
    }
}

void calculatePositions(std::vector<std::vector<PrintableNodePtr>>& lines) {
    for (auto& line : lines) {
        int position = 0;
        for (auto& node : line) {
            node->position = position;
            position += node->value.length() + 2; // Add space between nodes
        }
    }
}

void adjustPositions(std::vector<std::vector<PrintableNodePtr>>& lines) {
    for (int depth = lines.size() - 1; depth > 0; --depth) {
        for (auto& node : lines[depth]) {
            auto parent = node->parent;
            if (!parent) continue;
            int firstChildPos = node->position;
            int lastChildPos = node->position + static_cast<int>(node->value.length());
            for (auto& sibling : parent->children) {
                firstChildPos = std::min(firstChildPos, sibling->position);
                lastChildPos = std::max(lastChildPos, sibling->position + static_cast<int>(sibling->value.length()));
            }
            int middle = (firstChildPos + lastChildPos) / 2;
            parent->position = middle - parent->value.length() / 2;
        }
    }
}

void printTree(const std::vector<std::vector<PrintableNodePtr>>& lines) {
    for (size_t i = 0; i < lines.size(); ++i) {
        const auto& line = lines[i];
        int currentPos = 0;
        for (const auto& node : line) {
            while (currentPos < node->position) {
                std::cout << ' ';
                ++currentPos;
            }
            std::cout << node->value;
            currentPos += node->value.length();
        }
        std::cout << '\n';

        if (i < lines.size() - 1) {
            // First line with vertical relationship centered in the middle of all children
            currentPos = 0;
            for (const auto& node : line) {
                if (!node->children.empty()) {
                    int start = node->children.front()->position + node->children.front()->value.length() / 2;
                    int end = node->children.back()->position + node->children.back()->value.length() / 2;
                    int middle = (start + end) / 2;
                    while (currentPos < middle) {
                        std::cout << ' ';
                        ++currentPos;
                    }
                    std::cout << '|';
                    ++currentPos;
                }
            }
            std::cout << '\n';

            // Second line with horizontal relationship and vertical relationship for single children
            currentPos = 0;
            for (const auto& node : line) {
                if (!node->children.empty()) {
                    int start = node->children.front()->position + node->children.front()->value.length() / 2;
                    int end = node->children.back()->position + node->children.back()->value.length() / 2;
                    while (currentPos < start) {
                        std::cout << ' ';
                        ++currentPos;
                    }
                    if (node->children.size() > 1) {
                        while (currentPos < end) {
                            std::cout << '-';
                            ++currentPos;
                        }
                    } else {
                        std::cout << '|';
                        ++currentPos;
                    }
                }
            }
            std::cout << '\n';

            // Third line with vertical relationship for each child
            currentPos = 0;
            for (const auto& node : lines[i + 1]) {
                while (currentPos < node->position + node->value.length() / 2) {
                    std::cout << ' ';
                    ++currentPos;
                }
                std::cout << '|';
                ++currentPos;
            }
            std::cout << '\n';
        }
    }
}

int main() {
    auto root = std::make_shared<TreeNode>();
    root->value = "Schema(int)";
    auto child1 = std::make_shared<TreeNode>();
    child1->value = "Schema1(float)";
    auto child2 = std::make_shared<TreeNode>();
    child2->value = "Schema2(double)";
    root->children = {child1, child2};

    auto child3 = std::make_shared<TreeNode>();
    child3->value = "Schema3(int)";
    auto child4 = std::make_shared<TreeNode>();
    child4->value = "Schema4(char)";
    child1->children = {child3, child4};

    auto child5 = std::make_shared<TreeNode>();
    child5->value = "Schema5(float)";
    child2->children = {child5};

    // Adding more levels and nodes
    auto child6 = std::make_shared<TreeNode>();
    child6->value = "Schema6(bool)";
    auto child7 = std::make_shared<TreeNode>();
    child7->value = "Schema7(string)";
    child3->children = {child6, child7};

    auto child8 = std::make_shared<TreeNode>();
    child8->value = "Schema8(long)";
    auto child9 = std::make_shared<TreeNode>();
    child9->value = "Schema9(short)";
    auto child10 = std::make_shared<TreeNode>();
    child10->value = "Schema10(byte)";
    auto child11 = std::make_shared<TreeNode>();
    child11->value = "Schema11(long)";
    auto child12 = std::make_shared<TreeNode>();
    child12->value = "Schema12(short)";
    auto child13 = std::make_shared<TreeNode>();
    child13->value = "Schema13(byte)";
    child4->children = {child8, child9, child10, child11, child12, child13};

    auto child18 = std::make_shared<TreeNode>();
    child18->value = "Schema18(byte)";
    child5->children = {child18};

    std::vector<std::vector<PrintableNodePtr>> lines;
    groupSiblings(root, nullptr, lines, 0);
    calculatePositions(lines);
    adjustPositions(lines);
    printTree(lines);

    return 0;
}