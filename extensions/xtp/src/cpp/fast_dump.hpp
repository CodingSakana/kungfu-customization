// fast_dump.hpp
#pragma once
#include <nlohmann/json.hpp>
#include <charconv>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdio>

struct FastDumpOptions {
    // pretty == 0 紧凑输出；>0 表示每层缩进空格数
    int  pretty = 0;
    // false：非 ASCII 字节原样输出（不做 UTF-8 校验，适配 GBK 场景）
    // true ：将非 ASCII 与控制字符转 \u00XX（这里仅对单字节做 \u00XX，若要 UTF-8 严格编码可自行扩展）
    bool ensure_ascii = false;
    // 是否按 key 排序（稳定输出），会有额外开销
    bool sort_keys = false;
};

namespace fastdump_detail {

inline void append_literal(std::string& out, const char* s) {
    out.append(s);
}

inline void append_char(std::string& out, char c) {
    out.push_back(c);
}

// 仅做 JSON 最小必要转义；不做 UTF-8 校验。
// ensure_ascii=true 时：对非 ASCII 字节走 \u00XX（单字节）。
inline void append_escaped_string(std::string& out, const std::string& s, bool ensure_ascii) {
    out.push_back('"');

    const unsigned char* p = reinterpret_cast<const unsigned char*>(s.data());
    const unsigned char* const e = p + s.size();
    const unsigned char* chunk_start = p;

    auto flush_chunk = [&](const unsigned char* start, const unsigned char* end){
        out.append(reinterpret_cast<const char*>(start), reinterpret_cast<const char*>(end));
    };

    static const char* HEX = "0123456789ABCDEF";

    while (p < e) {
        unsigned char c = *p;
        bool need_escape = false;

        if (c < 0x20 || c == '"' || c == '\\') {
            need_escape = true;
        } else if (ensure_ascii && c >= 0x80) {
            need_escape = true;
        }

        if (!need_escape) { ++p; continue; }

        if (chunk_start < p) flush_chunk(chunk_start, p);

        switch (c) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default: {
                // 控制字符或非 ASCII（在 ensure_ascii 情况下）→ \u00XX
                out += "\\u00";
                out.push_back(HEX[(c >> 4) & 0xF]);
                out.push_back(HEX[c & 0xF]);
            } break;
        }
        ++p;
        chunk_start = p;
    }
    if (chunk_start < p) flush_chunk(chunk_start, p);

    out.push_back('"');
}

inline void append_integer(std::string& out, int64_t v) {
    char buf[32];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), v);
    out.append(buf, ptr);
}

inline void append_uinteger(std::string& out, uint64_t v) {
    char buf[32];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), v);
    out.append(buf, ptr);
}

inline void append_double(std::string& out, double v) {
    char buf[128];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), v, std::chars_format::general);
    if (ec == std::errc{}) {
        out.append(buf, ptr);
    } else {
        int n = std::snprintf(buf, sizeof(buf), "%.17g", v);
        if (n > 0) out.append(buf, buf + n);
    }
}

inline void indent_if_needed(std::string& out, int pretty, int level) {
    if (pretty > 0) {
        out.push_back('\n');
        out.append(static_cast<size_t>(pretty * level), ' ');
    }
}

struct Frame {
    enum Kind { VALUE, OBJECT, ARRAY } kind;
    const nlohmann::json* node = nullptr;

    // ——对象两种遍历方式——
    // 1) 不排序：直接迭代 object
    nlohmann::json::object_t::const_iterator obj_it{}, obj_end{};

    // 2) 排序：使用有序迭代器数组
    bool   use_ordered = false;
    size_t ord_idx = 0, ord_size = 0;

    // ——数组遍历——
    size_t idx = 0, size = 0;

    bool first = true;
    int  indent_level = 0;
};

} // namespace fastdump_detail

// 主函数：零递归、可选缩进/排序/ensure_ascii
inline std::string fast_dump(const nlohmann::json& j, const FastDumpOptions& opt = {}) {
    using namespace fastdump_detail;
    using json = nlohmann::json;

    std::string out;
    // 粗略预留，避免频繁扩容
    out.reserve((j.is_object() ? j.size()*24 : j.is_array() ? j.size()*8 : 64) + 256);

    std::vector<Frame> stack;
    stack.reserve(64);

    // 排序模式下使用的迭代器数组（thread_local 降低分配开销）
    static thread_local std::vector<json::object_t::const_iterator> ordered_keys;

    auto push_value = [&](const json* node, int level) {
        stack.push_back(Frame{Frame::VALUE, node, {}, {}, false, 0, 0, 0, 0, true, level});
    };

    auto push_object = [&](const json* node, int level) {
        Frame f{};
        f.kind = Frame::OBJECT;
        f.node = node;
        f.indent_level = level;
        f.first = true;

        const auto& obj = node->get_ref<const json::object_t&>();
        if (obj.empty()) {
            append_literal(out, "{}");
            return;
        }

        append_char(out, '{');
        if (opt.pretty) indent_if_needed(out, opt.pretty, level + 1);

        if (opt.sort_keys) {
            ordered_keys.clear();
            ordered_keys.reserve(obj.size());
            for (auto it = obj.begin(); it != obj.end(); ++it) ordered_keys.push_back(it);
            std::sort(ordered_keys.begin(), ordered_keys.end(),
                      [](auto a, auto b){ return a->first < b->first; });
            f.use_ordered = true;
            f.ord_idx = 0;
            f.ord_size = ordered_keys.size();
        } else {
            f.use_ordered = false;
            f.obj_it = obj.begin();
            f.obj_end = obj.end();
        }

        stack.push_back(std::move(f));
    };

    auto push_array = [&](const json* node, int level) {
        Frame f{};
        f.kind = Frame::ARRAY;
        f.node = node;
        f.indent_level = level;
        f.first = true;

        const auto& arr = node->get_ref<const json::array_t&>();
        f.size = arr.size();
        if (f.size == 0) {
            append_literal(out, "[]");
            return;
        }
        append_char(out, '[');
        if (opt.pretty) indent_if_needed(out, opt.pretty, level + 1);

        stack.push_back(std::move(f));
    };

    // 起始节点
    push_value(&j, 0);

    // 迭代式序列化
    while (!stack.empty()) {
        Frame f = stack.back();
        stack.pop_back();

        const json& node = *f.node;

        switch (f.kind) {
            case Frame::VALUE: {
                switch (node.type()) {
                    case json::value_t::null:              append_literal(out, "null"); break;
                    case json::value_t::boolean:           append_literal(out, node.get<bool>() ? "true" : "false"); break;
                    case json::value_t::number_integer:    append_integer(out,  node.get<int64_t>());  break;
                    case json::value_t::number_unsigned:   append_uinteger(out, node.get<uint64_t>()); break;
                    case json::value_t::number_float:      append_double(out,   node.get<double>());   break;
                    case json::value_t::string:            append_escaped_string(out, node.get_ref<const std::string&>(), opt.ensure_ascii); break;
                    case json::value_t::object:            push_object(&node, f.indent_level); break;
                    case json::value_t::array:             push_array(&node,  f.indent_level); break;
                    case json::value_t::binary:            append_escaped_string(out, "<binary>", false); break;      // 可根据需求改为 base64
                    case json::value_t::discarded:         append_escaped_string(out, "<discarded>", false); break;   // 占位
                }
            } break;

            case Frame::OBJECT: {
                // 选择下一条记录（排序/不排序两种模式）
                const std::string* key = nullptr;
                const json*        val = nullptr;

                bool done = false;
                if (f.use_ordered) {
                    if (f.ord_idx >= f.ord_size) done = true;
                    else {
                        auto it = ordered_keys[f.ord_idx++];
                        key = &it->first;
                        val = &it->second;
                    }
                } else {
                    if (f.obj_it == f.obj_end) done = true;
                    else {
                        key = &f.obj_it->first;
                        val = &f.obj_it->second;
                        ++f.obj_it;
                    }
                }

                if (done) {
                    if (opt.pretty) indent_if_needed(out, opt.pretty, f.indent_level);
                    append_char(out, '}');
                    break;
                }

                if (!f.first) {
                    append_char(out, ',');
                    if (opt.pretty) indent_if_needed(out, opt.pretty, f.indent_level + 1);
                }
                f.first = false;

                append_escaped_string(out, *key, opt.ensure_ascii);
                append_char(out, ':');
                if (opt.pretty) append_char(out, ' ');

                // 回栈继续处理剩余项，再压 value
                stack.push_back(std::move(f));
                push_value(val, f.indent_level + 1);
            } break;

            case Frame::ARRAY: {
                const auto& arr = node.get_ref<const json::array_t&>();
                if (f.idx == f.size) {
                    if (opt.pretty) indent_if_needed(out, opt.pretty, f.indent_level);
                    append_char(out, ']');
                    break;
                }

                if (!f.first) {
                    append_char(out, ',');
                    if (opt.pretty) indent_if_needed(out, opt.pretty, f.indent_level + 1);
                }
                f.first = false;

                const json* elem = &arr[f.idx++];
                stack.push_back(std::move(f));
                push_value(elem, f.indent_level + 1);
            } break;
        }
    }

    return out;
}
