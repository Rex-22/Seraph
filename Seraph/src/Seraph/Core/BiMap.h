//
// Created by Ruben on 2026/07/13.
//

#pragma once

#include <initializer_list>
#include <unordered_map>
#include <optional>
#include <utility>

namespace Seraph
{

template <typename KeyLeft, typename KeyRight>
class BiMap {
public:
    BiMap() = default;

    BiMap(std::initializer_list<std::pair<KeyLeft, KeyRight>> entries) {
        for (const auto& [left, right] : entries)
            Insert(left, right);
    }

    void Insert(const KeyLeft& left, const KeyRight& right) {
        left_to_right[left] = right;
        right_to_left[right] = left;
    }

    std::optional<KeyRight> GetRight(const KeyLeft& left) const {
        auto it = left_to_right.find(left);
        if (it != left_to_right.end()) return it->second;
        return std::nullopt;
    }

    std::optional<KeyLeft> GetLeft(const KeyRight& right) const {
        auto it = right_to_left.find(right);
        if (it != right_to_left.end()) return it->second;
        return std::nullopt;
    }
private:
    std::unordered_map<KeyLeft, KeyRight> left_to_right;
    std::unordered_map<KeyRight, KeyLeft> right_to_left;
};
}