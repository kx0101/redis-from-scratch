// parser.h
#pragma once
#include <vector>
#include <cstdint>
#include <optional>
#include <utility>

std::optional<std::pair<std::vector<std::vector<uint8_t>>, size_t>>
parse_resp_command(const std::vector<uint8_t>& buf, size_t buf_len);
