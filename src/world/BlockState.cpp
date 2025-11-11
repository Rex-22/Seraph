//
// Created by Claude Code on 2025/11/11.
//

#include "BlockState.h"

#include "Block.h"

#include <algorithm>
#include <sstream>

namespace World
{

BlockState::BlockState(const Block* block, uint16_t stateId)
    : m_Block(block)
    , m_StateId(stateId)
{
}

void BlockState::SetProperty(const std::string& key, const std::string& value)
{
    m_Properties[key] = value;
}

std::string BlockState::GetProperty(const std::string& key) const
{
    auto it = m_Properties.find(key);
    return it != m_Properties.end() ? it->second : "";
}

bool BlockState::HasProperty(const std::string& key) const
{
    return m_Properties.find(key) != m_Properties.end();
}

std::string BlockState::BuildPropertyString() const
{
    if (m_Properties.empty()) {
        return "";
    }

    // Sort properties by key for consistent ordering
    std::vector<std::pair<std::string, std::string>> sortedProps(
        m_Properties.begin(), m_Properties.end());
    std::sort(
        sortedProps.begin(), sortedProps.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });

    // Build property string: "key1=value1,key2=value2"
    std::ostringstream oss;
    for (size_t i = 0; i < sortedProps.size(); ++i) {
        if (i > 0) {
            oss << ",";
        }
        oss << sortedProps[i].first << "=" << sortedProps[i].second;
    }

    return oss.str();
}

} // namespace World
