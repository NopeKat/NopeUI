#include "nopeui/markup/xaml.h"

#include <algorithm>
#include <cstdint>
#include <cwchar>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "nopeui/base/strings.h"
#include "nopeui/controls/border.h"
#include "nopeui/controls/button.h"
#include "nopeui/controls/check_box.h"
#include "nopeui/controls/content_presenter.h"
#include "nopeui/controls/context_menu.h"
#include "nopeui/controls/flyout.h"
#include "nopeui/controls/image.h"
#include "nopeui/controls/label.h"
#include "nopeui/controls/progress_bar.h"
#include "nopeui/controls/radio_button.h"
#include "nopeui/controls/scroll_viewer.h"
#include "nopeui/controls/slider.h"
#include "nopeui/controls/stack_panel.h"
#include "nopeui/controls/text_box.h"
#include "nopeui/controls/toggle_button.h"
#include "nopeui/controls/toggle_switch.h"
#include "nopeui/controls/tooltip.h"
#include "nopeui/layout/style.h"
#include "nopeui/theme/control_template.h"

namespace nopeui {
namespace xaml {
namespace {

bool IsSpace(wchar_t value) noexcept {
    return value == L' ' || value == L'\t' || value == L'\n' || value == L'\r';
}

bool IsNameStart(wchar_t value) noexcept {
    return (value >= L'a' && value <= L'z') || (value >= L'A' && value <= L'Z') ||
           value == L'_' || value == L':';
}

bool IsNameChar(wchar_t value) noexcept {
    return IsNameStart(value) || (value >= L'0' && value <= L'9') || value == L'-' ||
           value == L'.';
}

bool IsHexDigit(wchar_t value) noexcept {
    return (value >= L'0' && value <= L'9') || (value >= L'a' && value <= L'f') ||
           (value >= L'A' && value <= L'F');
}

int HexValue(wchar_t value) noexcept {
    if (value >= L'0' && value <= L'9') {
        return static_cast<int>(value - L'0');
    }
    if (value >= L'a' && value <= L'f') {
        return static_cast<int>(value - L'a') + 10;
    }
    return static_cast<int>(value - L'A') + 10;
}

std::wstring NormalizeSource(std::wstring_view source) {
    std::wstring result;
    result.reserve(source.size());
    std::size_t index = 0;
    if (!source.empty() && source.front() == 0xfeff) {
        index = 1;
    }
    while (index < source.size()) {
        const wchar_t value = source[index];
        if (value == L'\r') {
            if (index + 1 < source.size() && source[index + 1] == L'\n') {
                ++index;
            }
            result.push_back(L'\n');
        } else {
            result.push_back(value);
        }
        ++index;
    }
    return result;
}

bool DecodeEntities(std::wstring_view raw, std::wstring* out) {
    out->clear();
    for (std::size_t index = 0; index < raw.size(); ++index) {
        if (raw[index] != L'&') {
            out->push_back(raw[index]);
            continue;
        }
        const std::size_t semicolon = raw.find(L';', index + 1);
        if (semicolon == std::wstring_view::npos) {
            return false;
        }
        const std::wstring_view body = raw.substr(index + 1, semicolon - index - 1);
        if (body == L"amp") {
            out->push_back(L'&');
        } else if (body == L"lt") {
            out->push_back(L'<');
        } else if (body == L"gt") {
            out->push_back(L'>');
        } else if (body == L"quot") {
            out->push_back(L'"');
        } else if (body == L"apos") {
            out->push_back(L'\'');
        } else if (body.size() > 1 && body.front() == L'#') {
            std::uint32_t codepoint = 0;
            std::size_t start = 1;
            const bool hex = body.size() > 2 && (body[1] == L'x' || body[1] == L'X');
            if (hex) {
                start = 2;
            }
            if (start >= body.size()) {
                return false;
            }
            for (std::size_t part = start; part < body.size(); ++part) {
                const wchar_t digit = body[part];
                if (hex) {
                    if (!IsHexDigit(digit)) {
                        return false;
                    }
                    codepoint = codepoint * 16u + static_cast<std::uint32_t>(HexValue(digit));
                } else {
                    if (digit < L'0' || digit > L'9') {
                        return false;
                    }
                    codepoint = codepoint * 10u + static_cast<std::uint32_t>(digit - L'0');
                }
                if (codepoint > 0x10ffffu) {
                    return false;
                }
            }
            if (codepoint >= 0xd800u && codepoint <= 0xdfffu) {
                return false;
            }
            if (codepoint <= 0xffffu) {
                out->push_back(static_cast<wchar_t>(codepoint));
            } else {
                codepoint -= 0x10000u;
                out->push_back(static_cast<wchar_t>(0xd800u + (codepoint >> 10u)));
                out->push_back(static_cast<wchar_t>(0xdc00u + (codepoint & 0x3ffu)));
            }
        } else {
            return false;
        }
        index = semicolon;
    }
    return true;
}

class Parser {
public:
    explicit Parser(std::wstring_view source)
        : source_(NormalizeSource(source)) {}

    bool ParseDocument(Node* root) {
        SkipMisc();
        if (Peek() != L'<') {
            return Fail("expected a root element");
        }
        if (!ParseNode(root)) {
            return false;
        }
        SkipMisc();
        if (!AtEnd()) {
            return Fail("unexpected content after the root element");
        }
        return true;
    }

    const Error& error() const noexcept {
        return error_;
    }

private:
    bool AtEnd() const noexcept {
        return position_ >= source_.size();
    }

    wchar_t Peek(std::size_t offset = 0) const noexcept {
        const std::size_t index = position_ + offset;
        return index < source_.size() ? source_[index] : L'\0';
    }

    bool StartsWith(std::wstring_view text) const noexcept {
        if (position_ + text.size() > source_.size()) {
            return false;
        }
        return source_.compare(position_, text.size(), text) == 0;
    }

    wchar_t Advance() {
        const wchar_t value = source_[position_];
        ++position_;
        if (value == L'\n') {
            ++line_;
            column_ = 1;
        } else {
            ++column_;
        }
        return value;
    }

    void SkipWhitespace() {
        while (!AtEnd() && IsSpace(Peek())) {
            Advance();
        }
    }

    bool SkipComment() {
        if (!StartsWith(L"<!--")) {
            return true;
        }
        position_ += 4;
        column_ += 4;
        const std::wstring_view end = L"-->";
        while (!AtEnd()) {
            if (StartsWith(end)) {
                position_ += 3;
                column_ += 3;
                return true;
            }
            Advance();
        }
        return Fail("unterminated comment");
    }

    bool SkipProcessingInstruction() {
        if (!StartsWith(L"<?")) {
            return true;
        }
        position_ += 2;
        column_ += 2;
        while (!AtEnd()) {
            if (StartsWith(L"?>")) {
                position_ += 2;
                column_ += 2;
                return true;
            }
            Advance();
        }
        return Fail("unterminated processing instruction");
    }

    bool SkipDeclaration() {
        if (!StartsWith(L"<!DOCTYPE")) {
            return true;
        }
        position_ += 9;
        column_ += 9;
        while (!AtEnd()) {
            if (Peek() == L'>') {
                Advance();
                return true;
            }
            Advance();
        }
        return Fail("unterminated declaration");
    }

    bool SkipMisc() {
        while (!AtEnd()) {
            SkipWhitespace();
            if (SkipComment() && SkipProcessingInstruction() && SkipDeclaration()) {
                if (AtEnd() || (!StartsWith(L"<!--") && !StartsWith(L"<?") &&
                                !StartsWith(L"<!DOCTYPE") && !IsSpace(Peek()))) {
                    break;
                }
            } else {
                return false;
            }
        }
        return true;
    }

    std::wstring ReadName() {
        std::wstring name;
        if (!IsNameStart(Peek())) {
            return name;
        }
        name.push_back(Advance());
        while (!AtEnd() && IsNameChar(Peek())) {
            name.push_back(Advance());
        }
        return name;
    }

    bool ParseNode(Node* node) {
        node->line = line_;
        node->column = column_;
        if (Peek() != L'<') {
            return Fail("expected '<'");
        }
        Advance();
        if (Peek() == L'/' || Peek() == L'!' || Peek() == L'?') {
            return Fail("expected an element name");
        }
        node->name = ReadName();
        if (node->name.empty()) {
            return Fail("expected an element name");
        }

        while (true) {
            SkipWhitespace();
            if (AtEnd()) {
                return Fail("unterminated start tag");
            }
            if (StartsWith(L"/>")) {
                position_ += 2;
                column_ += 2;
                return true;
            }
            if (Peek() == L'>') {
                Advance();
                break;
            }
            std::wstring name = ReadName();
            if (name.empty()) {
                return Fail("expected an attribute name");
            }
            for (const Attribute& existing : node->attributes) {
                if (existing.name == name) {
                    return Fail("duplicate attribute");
                }
            }
            SkipWhitespace();
            if (Peek() != L'=') {
                return Fail("expected '=' after an attribute name");
            }
            Advance();
            SkipWhitespace();
            const wchar_t quote = Peek();
            if (quote != L'"' && quote != L'\'') {
                return Fail("expected a quoted attribute value");
            }
            Advance();
            std::wstring raw;
            while (!AtEnd() && Peek() != quote) {
                raw.push_back(Advance());
            }
            if (AtEnd()) {
                return Fail("unterminated attribute value");
            }
            Advance();
            std::wstring value;
            if (!DecodeEntities(raw, &value)) {
                return Fail("invalid entity in an attribute value");
            }
            node->attributes.push_back(Attribute{std::move(name), std::move(value)});
        }

        while (true) {
            if (AtEnd()) {
                return Fail("unterminated element");
            }
            if (StartsWith(L"</")) {
                position_ += 2;
                column_ += 2;
                std::wstring closing = ReadName();
                if (closing != node->name) {
                    return Fail("mismatched closing tag");
                }
                SkipWhitespace();
                if (Peek() != L'>') {
                    return Fail("expected '>' after a closing tag");
                }
                Advance();
                return true;
            }
            if (SkipComment() && SkipProcessingInstruction()) {
                if (AtEnd() || StartsWith(L"</")) {
                    continue;
                }
                if (Peek() == L'<') {
                    Node child;
                    if (!ParseNode(&child)) {
                        return false;
                    }
                    node->children.push_back(std::move(child));
                    continue;
                }
                std::wstring raw;
                while (!AtEnd() && Peek() != L'<') {
                    raw.push_back(Advance());
                }
                std::wstring text;
                if (!DecodeEntities(raw, &text)) {
                    return Fail("invalid entity in text");
                }
                node->text += text;
            } else {
                return false;
            }
        }
    }

    bool Fail(std::string message) {
        if (error_.code == ErrorCode::None) {
            error_.code = ErrorCode::ParseFailure;
            error_.message = "xaml " + std::to_string(line_) + ":" +
                             std::to_string(column_) + ": " + std::move(message);
        }
        return false;
    }

    std::wstring source_;
    std::size_t position_ = 0;
    int line_ = 1;
    int column_ = 1;
    Error error_{};
};

std::wstring LocalName(std::wstring_view name) {
    const std::size_t colon = name.rfind(L':');
    return colon == std::wstring_view::npos
               ? std::wstring(name)
               : std::wstring(name.substr(colon + 1));
}

bool EndsWith(std::wstring_view value, std::wstring_view suffix) noexcept {
    return value.size() >= suffix.size() &&
           value.substr(value.size() - suffix.size()) == suffix;
}

bool StartsWith(std::wstring_view value, std::wstring_view prefix) noexcept {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

std::u16string ToUtf16(std::wstring_view value) {
    std::u16string result;
    result.reserve(value.size());
    for (wchar_t character : value) {
        result.push_back(static_cast<char16_t>(character));
    }
    return result;
}

std::wstring Trim(std::wstring_view value) {
    std::size_t begin = 0;
    std::size_t end = value.size();
    while (begin < end && IsSpace(value[begin])) {
        ++begin;
    }
    while (end > begin && IsSpace(value[end - 1])) {
        --end;
    }
    return std::wstring(value.substr(begin, end - begin));
}

const Attribute* FindAttribute(const Node& node, std::wstring_view local_name) {
    for (const Attribute& attribute : node.attributes) {
        if (LocalName(attribute.name) == local_name) {
            return &attribute;
        }
    }
    return nullptr;
}

std::wstring AttributeValue(const Node& node, std::wstring_view local_name) {
    const Attribute* attribute = FindAttribute(node, local_name);
    return attribute != nullptr ? attribute->value : std::wstring();
}

bool ParseBool(std::wstring_view raw, bool* out) {
    const std::wstring value = Trim(raw);
    if (value == L"true" || value == L"True" || value == L"1") {
        *out = true;
        return true;
    }
    if (value == L"false" || value == L"False" || value == L"0") {
        *out = false;
        return true;
    }
    return false;
}

bool ParseFloat(std::wstring_view raw, float* out) {
    const std::wstring value = Trim(raw);
    if (value.empty()) {
        return false;
    }
    wchar_t* end = nullptr;
    const float parsed = std::wcstof(value.c_str(), &end);
    if (end == nullptr || *end != L'\0') {
        return false;
    }
    *out = parsed;
    return true;
}

bool ParseDouble(std::wstring_view raw, double* out) {
    const std::wstring value = Trim(raw);
    if (value.empty()) {
        return false;
    }
    wchar_t* end = nullptr;
    const double parsed = std::wcstod(value.c_str(), &end);
    if (end == nullptr || *end != L'\0') {
        return false;
    }
    *out = parsed;
    return true;
}

bool ParseInt(std::wstring_view raw, int* out) {
    const std::wstring value = Trim(raw);
    if (value.empty()) {
        return false;
    }
    wchar_t* end = nullptr;
    const long parsed = std::wcstol(value.c_str(), &end, 10);
    if (end == nullptr || *end != L'\0') {
        return false;
    }
    *out = static_cast<int>(parsed);
    return true;
}

bool ParseColor(std::wstring_view raw, std::uint32_t* out) {
    std::wstring value = Trim(raw);
    if (value.size() > 2 && value[0] == L'0' && (value[1] == L'x' || value[1] == L'X')) {
        value.erase(0, 2);
    } else if (!value.empty() && value[0] == L'#') {
        value.erase(0, 1);
    }
    if (value.size() != 6) {
        return false;
    }
    std::uint32_t color = 0;
    for (wchar_t digit : value) {
        if (!IsHexDigit(digit)) {
            return false;
        }
        color = color * 16u + static_cast<std::uint32_t>(HexValue(digit));
    }
    *out = color;
    return true;
}

bool ParseThickness(std::wstring_view raw, Thickness* out) {
    std::vector<std::wstring> parts;
    std::size_t start = 0;
    while (start <= raw.size()) {
        const std::size_t comma = raw.find(L',', start);
        parts.push_back(Trim(raw.substr(start, comma == std::wstring_view::npos
                                                   ? raw.size() - start
                                                   : comma - start)));
        if (comma == std::wstring_view::npos) {
            break;
        }
        start = comma + 1;
    }
    std::vector<float> values;
    for (const std::wstring& part : parts) {
        float value = 0.0f;
        if (!ParseFloat(part, &value)) {
            return false;
        }
        values.push_back(value);
    }
    if (values.size() == 1) {
        out->left = values[0];
        out->top = values[0];
        out->right = values[0];
        out->bottom = values[0];
    } else if (values.size() == 2) {
        out->left = values[0];
        out->right = values[0];
        out->top = values[1];
        out->bottom = values[1];
    } else if (values.size() == 4) {
        out->left = values[0];
        out->top = values[1];
        out->right = values[2];
        out->bottom = values[3];
    } else {
        return false;
    }
    return true;
}

bool PropertyTypeFor(Element& probe, std::wstring_view name, PropertyValueType* type) {
    for (const PropertyMetadata* metadata : probe.Properties()) {
        if (metadata != nullptr && std::wstring_view(metadata->name) == name) {
            *type = metadata->type;
            return true;
        }
    }
    return false;
}

bool ConvertPropertyValue(
    PropertyValueType type,
    std::wstring_view raw,
    PropertyValue* out) {
    bool boolean = false;
    int integer = 0;
    float single = 0.0f;
    double number = 0.0;
    std::uint32_t color = 0;
    switch (type) {
        case PropertyValueType::Bool:
            if (!ParseBool(raw, &boolean)) {
                return false;
            }
            *out = PropertyValue(boolean);
            return true;
        case PropertyValueType::Int:
            if (!ParseInt(raw, &integer)) {
                return false;
            }
            *out = PropertyValue(integer);
            return true;
        case PropertyValueType::Float:
            if (!ParseFloat(raw, &single)) {
                return false;
            }
            *out = PropertyValue(single);
            return true;
        case PropertyValueType::Double:
            if (!ParseDouble(raw, &number)) {
                return false;
            }
            *out = PropertyValue(number);
            return true;
        case PropertyValueType::Color:
            if (!ParseColor(raw, &color)) {
                return false;
            }
            *out = PropertyValue(color);
            return true;
        case PropertyValueType::String:
            *out = PropertyValue(std::wstring(raw));
            return true;
    }
    return false;
}

bool ReadOnlyTextProperty(std::wstring_view tag, std::wstring_view property) {
    if (property == L"Text") {
        return tag == L"Button" || tag == L"Label" || tag == L"ToggleButton" ||
               tag == L"ToolTip";
    }
    if (property == L"Label") {
        return tag == L"CheckBox" || tag == L"RadioButton" || tag == L"ToggleSwitch";
    }
    return false;
}

bool IsStructuralAttribute(std::wstring_view name) {
    return name == L"Key" || name == L"Name" || name == L"Class" ||
           name == L"Ignorable" || name == L"xmlns";
}

std::wstring ResourceKey(std::wstring_view raw) {
    std::wstring value = Trim(raw);
    if (value.size() > 2 && value.front() == L'{' && value.back() == L'}') {
        std::wstring body = Trim(std::wstring_view(value).substr(1, value.size() - 2));
        const std::wstring_view prefix = L"StaticResource";
        if (body.size() > prefix.size() &&
            body.compare(0, prefix.size(), prefix) == 0 &&
            IsSpace(body[prefix.size()])) {
            return Trim(std::wstring_view(body).substr(prefix.size()));
        }
    }
    return value;
}

bool TemplateBindingKey(std::wstring_view raw, std::wstring* property) {
    std::wstring value = Trim(raw);
    if (value.size() < 3 || value.front() != L'{' || value.back() != L'}') {
        return false;
    }
    std::wstring body = Trim(std::wstring_view(value).substr(1, value.size() - 2));
    const std::wstring_view prefix = L"TemplateBinding";
    if (body.size() <= prefix.size() ||
        body.compare(0, prefix.size(), prefix) != 0 ||
        !IsSpace(body[prefix.size()])) {
        return false;
    }
    *property = Trim(std::wstring_view(body).substr(prefix.size()));
    return !property->empty();
}

struct LocalAttributes {
    Element* element = nullptr;
    const Node* node = nullptr;
};

struct ResourceScope {
    Element* owner = nullptr;
    const Node* block = nullptr;
    ResourceScope* parent = nullptr;
    Ref<StyleSheet> styles;
    std::unordered_map<std::wstring, Ref<ControlTemplate>> templates;
    std::unordered_map<std::wstring, Ref<Style>> styles_by_key;
};

struct DirectTemplate {
    Element* element = nullptr;
    const Node* node = nullptr;
    std::wstring key;
    ResourceScope* scope = nullptr;
};

struct DirectStyle {
    Element* element = nullptr;
    const Node* node = nullptr;
    std::wstring key;
    ResourceScope* scope = nullptr;
};

class Loader {
public:
    Loader(const LoadContext& context, const Node& root)
        : context_(context), root_(root) {}

    Result<LoadResult> Load() {
        Ref<Element> element = BuildElement(root_);
        if (!element) {
            return Failure();
        }
        if (!BuildResourceObjects()) {
            return Failure();
        }

        ApplyStyles(*element);
        for (const DirectStyle& direct : direct_styles_) {
            if (direct.element != nullptr) {
                if (Ref<Style> style = FindStyle(direct.scope, direct.key)) {
                    style->Apply(*direct.element);
                } else if (direct.node != nullptr) {
                    Fail(*direct.node, "Style references an unknown resource");
                    return Failure();
                }
            }
        }
        for (const DirectTemplate& direct : direct_templates_) {
            if (direct.element != nullptr) {
                if (Ref<ControlTemplate> control_template =
                        FindTemplate(direct.scope, direct.key)) {
                    direct.element->ApplyControlTemplate(*control_template);
                } else if (direct.node != nullptr) {
                    Fail(*direct.node, "Template references an unknown resource");
                    return Failure();
                }
            }
        }
        for (const LocalAttributes& local : locals_) {
            if (!ApplyProperties(*local.element, *local.node, true)) {
                return Failure();
            }
        }

        LoadResult result;
        result.root = std::move(element);
        result.names = std::move(names_);
        return Result<LoadResult>::Ok(std::move(result));
    }

private:
    Result<LoadResult> Failure() {
        if (error_.code == ErrorCode::None) {
            error_ = Error{ErrorCode::InvalidState, "xaml loader failed"};
        }
        return Result<LoadResult>::Fail(error_);
    }

    bool Fail(const Node& node, std::string message) {
        if (error_.code == ErrorCode::None) {
            error_.code = ErrorCode::ParseFailure;
            error_.message = "xaml " + std::to_string(node.line) + ":" +
                             std::to_string(node.column) + ": " + std::move(message);
        }
        return false;
    }

    bool FindResourceBlock(const Node& node, const Node** block) {
        for (const Node& child : node.children) {
            const std::wstring local = LocalName(child.name);
            if (!EndsWith(local, L".Resources") && local != L"Resources") {
                continue;
            }
            if (*block != nullptr) {
                return Fail(child, "element has more than one Resources block");
            }
            *block = &child;
        }
        return true;
    }

    ResourceScope* CreateScope(
        Element& owner,
        const Node& block,
        ResourceScope* parent) {
        scopes_.emplace_back();
        ResourceScope* scope = &scopes_.back();
        scope->owner = &owner;
        scope->block = &block;
        scope->parent = parent;
        return scope;
    }

    Ref<ControlTemplate> FindTemplate(
        ResourceScope* scope,
        std::wstring_view key) const {
        const std::wstring name(key);
        for (ResourceScope* current = scope; current != nullptr;
             current = current->parent) {
            if (const auto found = current->templates.find(name);
                found != current->templates.end()) {
                return found->second;
            }
        }
        return {};
    }

    Ref<Style> FindStyle(
        ResourceScope* scope,
        std::wstring_view key) const {
        const std::wstring name(key);
        for (ResourceScope* current = scope; current != nullptr;
             current = current->parent) {
            if (const auto found = current->styles_by_key.find(name);
                found != current->styles_by_key.end()) {
                return found->second;
            }
        }
        return {};
    }

    bool AddTemplate(const Node& node, ResourceScope* scope) {
        const std::wstring key = AttributeValue(node, L"Key");
        if (key.empty()) {
            return Fail(node, "ControlTemplate requires Key");
        }
        if (scope->templates.find(key) != scope->templates.end()) {
            return Fail(node, "duplicate ControlTemplate Key");
        }
        const std::wstring target_type =
            LocalName(AttributeValue(node, L"TargetType"));
        bool has_parts = false;
        bool has_visual = false;
        for (const Node& child : node.children) {
            if (LocalName(child.name) == L"Part") {
                has_parts = true;
            } else {
                has_visual = true;
            }
        }
        if (has_parts && has_visual) {
            return Fail(node, "ControlTemplate cannot mix Part and visual content");
        }
        if (!has_parts && !has_visual) {
            return Fail(node, "ControlTemplate requires Part or visual content");
        }
        if (has_visual && target_type.empty()) {
            return Fail(node, "visual ControlTemplate requires TargetType");
        }
        if (has_visual && node.children.size() != 1) {
            return Fail(node, "visual ControlTemplate requires one root element");
        }

        Ref<ControlTemplate> control_template = ControlTemplate::Create();
        control_template->SetTargetType(target_type);
        if (has_parts) {
            for (const Node& child : node.children) {
                if (!AddPart(child, control_template.Get())) {
                    return false;
                }
            }
        } else {
            Ref<Element> source_probe = ProbeFor(target_type);
            if (!source_probe) {
                return Fail(node, "unknown ControlTemplate TargetType");
            }
            std::shared_ptr<Node> root_node =
                std::make_shared<Node>(node.children.front());
            TemplateInstance prototype =
                BuildTemplateInstance(*root_node, source_probe.Get());
            if (!prototype.root) {
                return false;
            }
            control_template->SetFactory(
                [this, root_node, source_probe] {
                    return BuildTemplateInstance(*root_node, source_probe.Get());
                });
        }
        scope->templates.emplace(key, std::move(control_template));
        return true;
    }

    bool AddPart(const Node& node, ControlTemplate* control_template) {
        const std::wstring name = AttributeValue(node, L"Name");
        if (name.empty()) {
            return Fail(node, "Part requires Name");
        }
        TemplatePart part;
        for (const Attribute& attribute : node.attributes) {
            const std::wstring local = LocalName(attribute.name);
            if (local == L"Name" || local == L"xmlns") {
                continue;
            }
            if (local == L"Background") {
                if (!ParseColor(attribute.value, &part.background)) {
                    return Fail(node, "invalid Part Background");
                }
            } else if (local == L"Foreground") {
                if (!ParseColor(attribute.value, &part.foreground)) {
                    return Fail(node, "invalid Part Foreground");
                }
            } else if (local == L"Border") {
                if (!ParseColor(attribute.value, &part.border)) {
                    return Fail(node, "invalid Part Border");
                }
            } else if (local == L"CornerRadius") {
                if (!ParseFloat(attribute.value, &part.corner_radius)) {
                    return Fail(node, "invalid Part CornerRadius");
                }
            } else if (local == L"Padding") {
                if (!ParseThickness(attribute.value, &part.padding)) {
                    return Fail(node, "invalid Part Padding");
                }
            } else {
                return Fail(node, "unknown Part attribute");
            }
        }
        control_template->SetPart(name, part);
        return true;
    }

    TemplateInstance BuildTemplateInstance(const Node& node, Element* source_probe) {
        TemplateInstance instance;
        instance.root = BuildTemplateElement(node, &instance.bindings, source_probe);
        return instance;
    }

    Ref<Element> BuildTemplateElement(
        const Node& node,
        std::vector<TemplateBinding>* bindings,
        Element* source_probe) {
        const std::wstring local = LocalName(node.name);
        if (local.find(L'.') != std::wstring::npos) {
            Fail(node, "unsupported property element in ControlTemplate");
            return {};
        }
        Ref<Element> element = CreateControl(node);
        if (!element) {
            return {};
        }
        if (auto* presenter = dynamic_cast<ContentPresenter*>(element.Get())) {
            const std::wstring text = DefaultText(node);
            if (!text.empty()) {
                presenter->SetText(text);
            }
        }
        if (!ApplyTemplateProperties(element, node, bindings, source_probe)) {
            return {};
        }
        if (!AttachTemplateChildren(node, *element, bindings, source_probe)) {
            return {};
        }
        const std::wstring trimmed = Trim(node.text);
        if (!trimmed.empty() && !ConsumesText(local)) {
            Fail(node, "element does not accept text content");
            return {};
        }
        return element;
    }

    bool ApplyTemplateProperties(
        Ref<Element>& element,
        const Node& node,
        std::vector<TemplateBinding>* bindings,
        Element* source_probe) {
        const std::wstring tag = LocalName(node.name);
        for (const Attribute& attribute : node.attributes) {
            const std::wstring local = LocalName(attribute.name);
            if (attribute.name == L"xmlns" || StartsWith(attribute.name, L"xmlns:")) {
                continue;
            }
            if (IsStructuralAttribute(local)) {
                continue;
            }
            if (local == L"Style" || local == L"Template" || local == L"ToolTip" ||
                local == L"Click" || local == L"Submit" ||
                local == L"CheckedChanged" || local == L"ValueChanged") {
                return Fail(node, "attribute is not supported in ControlTemplate");
            }

            std::wstring source_property;
            if (TemplateBindingKey(attribute.value, &source_property)) {
                PropertyValueType target_type = PropertyValueType::String;
                if (!PropertyTypeFor(*element, local, &target_type)) {
                    return Fail(node, "TemplateBinding target property is unknown");
                }
                if (source_probe != nullptr) {
                    PropertyValueType source_type = PropertyValueType::String;
                    if (!PropertyTypeFor(*source_probe, source_property, &source_type)) {
                        return Fail(node, "TemplateBinding source property is unknown");
                    }
                    if (source_type != target_type) {
                        return Fail(node, "TemplateBinding property types differ");
                    }
                }
                TemplateBinding binding;
                binding.target = element;
                binding.target_property = local;
                binding.source_property = std::move(source_property);
                bindings->push_back(std::move(binding));
                continue;
            }

            if (!ApplyAttribute(*element, node, tag, local, attribute.value, false)) {
                return false;
            }
        }
        return true;
    }

    bool AttachTemplateChildren(
        const Node& node,
        Element& element,
        std::vector<TemplateBinding>* bindings,
        Element* source_probe) {
        const std::wstring tag = LocalName(node.name);
        if (tag == L"ContentPresenter" && !node.children.empty()) {
            return Fail(node, "ContentPresenter does not accept child elements");
        }

        std::vector<Ref<Element>> children;
        for (const Node& child : node.children) {
            const std::wstring local = LocalName(child.name);
            if (local.find(L'.') != std::wstring::npos) {
                return Fail(child, "property elements are not supported in ControlTemplate");
            }
            Ref<Element> child_element =
                BuildTemplateElement(child, bindings, source_probe);
            if (!child_element) {
                return false;
            }
            children.push_back(std::move(child_element));
        }
        if (children.empty()) {
            return true;
        }
        if (tag == L"StackPanel") {
            auto* panel = dynamic_cast<StackPanel*>(&element);
            if (panel == nullptr) {
                return Fail(node, "internal StackPanel mismatch");
            }
            for (Ref<Element>& child : children) {
                panel->Add(std::move(child));
            }
            return true;
        }
        if (tag == L"Border" || tag == L"ScrollViewer" || tag == L"Flyout") {
            if (children.size() != 1) {
                return Fail(node, "template container accepts one child");
            }
            if (tag == L"Border") {
                static_cast<Border*>(&element)->SetChild(std::move(children.front()));
            } else if (tag == L"ScrollViewer") {
                static_cast<ScrollViewer*>(&element)->SetChild(std::move(children.front()));
            } else {
                static_cast<Flyout*>(&element)->SetContent(std::move(children.front()));
            }
            return true;
        }
        return Fail(node, "element does not accept child elements in ControlTemplate");
    }

    bool BuildResourceObjects() {
        for (ResourceScope& scope : scopes_) {
            scope.styles = StyleSheet::Create();
            for (const Node& child : scope.block->children) {
                if (LocalName(child.name) != L"ControlTemplate") {
                    continue;
                }
                if (!AddTemplate(child, &scope)) {
                    return false;
                }
            }
            for (const Node& child : scope.block->children) {
                const std::wstring local = LocalName(child.name);
                if (local == L"ControlTemplate") {
                    continue;
                }
                if (local != L"Style") {
                    return Fail(child, "only Style and ControlTemplate belong in Resources");
                }
                if (!BuildStyle(child, &scope)) {
                    return false;
                }
            }
        }
        return true;
    }

    Ref<Element> ProbeFor(std::wstring_view target_type) {
        const std::wstring key(target_type);
        if (const auto found = probes_.find(key); found != probes_.end()) {
            return found->second;
        }
        Node probe_node;
        probe_node.name = key;
        Ref<Element> probe = CreateControl(probe_node);
        if (probe) {
            probes_.emplace(key, probe);
        }
        return probe;
    }

    bool BuildStyle(const Node& node, ResourceScope* scope) {
        const std::wstring target_type = LocalName(AttributeValue(node, L"TargetType"));
        if (target_type.empty()) {
            return Fail(node, "Style requires TargetType");
        }
        Ref<Style> style = Style::Create();
        const std::wstring key = AttributeValue(node, L"Key");
        if (!key.empty()) {
            style->SetTargetKey(key);
            if (scope->styles_by_key.find(key) != scope->styles_by_key.end()) {
                return Fail(node, "duplicate Style Key");
            }
        }
        style->SetTargetType(target_type);
        Ref<Element> probe = ProbeFor(target_type);
        if (!probe) {
            return Fail(node, "unknown Style TargetType");
        }
        for (const Node& child : node.children) {
            if (LocalName(child.name) != L"Setter") {
                return Fail(child, "Style expects Setter elements");
            }
            if (!AddSetter(child, *probe, style.Get(), scope)) {
                return false;
            }
        }
        scope->styles->Add(style);
        if (!key.empty()) {
            scope->styles_by_key.emplace(key, std::move(style));
        }
        return true;
    }

    bool AddSetter(
        const Node& node,
        Element& probe,
        Style* style,
        ResourceScope* scope) {
        const std::wstring property = AttributeValue(node, L"Property");
        if (property.empty()) {
            return Fail(node, "Setter requires Property");
        }
        const std::wstring raw = AttributeValue(node, L"Value");
        if (property == L"Template") {
            const std::wstring key = ResourceKey(raw);
            if (key.empty()) {
                return Fail(node, "Template Setter requires a resource key");
            }
            if (Ref<ControlTemplate> found = FindTemplate(scope, key)) {
                style->SetTemplate(std::move(found));
                return true;
            }
            return Fail(node, "Template Setter references an unknown template");
        }
        PropertyValueType type = PropertyValueType::String;
        if (!PropertyTypeFor(probe, property, &type)) {
            return Fail(node, "unknown Style property");
        }
        PropertyValue value;
        if (!ConvertPropertyValue(type, raw, &value)) {
            return Fail(node, "invalid Style value");
        }
        style->Set(property, std::move(value));
        return true;
    }

    Ref<Element> BuildElement(const Node& node) {
        ResourceScope* saved_scope = current_scope_;
        Ref<Element> element = BuildElementInternal(node);
        current_scope_ = saved_scope;
        return element;
    }

    Ref<Element> BuildElementInternal(const Node& node) {
        const std::wstring local = LocalName(node.name);
        if (local.find(L'.') != std::wstring::npos) {
            Fail(node, "unsupported property element");
            return {};
        }
        Ref<Element> element = CreateControl(node);
        if (!element) {
            return {};
        }

        const Node* resource_block = nullptr;
        if (!FindResourceBlock(node, &resource_block)) {
            return {};
        }
        ResourceScope* scope = current_scope_;
        if (resource_block != nullptr) {
            scope = CreateScope(*element, *resource_block, current_scope_);
            current_scope_ = scope;
        }
        element_scopes_.emplace(element.Get(), scope);

        const std::wstring key = AttributeValue(node, L"Key");
        if (!key.empty()) {
            element->SetKey(key);
        }
        const std::wstring name = AttributeValue(node, L"Name");
        if (!name.empty()) {
            if (names_.find(name) != names_.end()) {
                Fail(node, "duplicate Name");
                return {};
            }
            names_.emplace(name, element);
        }

        if (!ApplyProperties(*element, node, false)) {
            return {};
        }
        const Attribute* tooltip = FindAttribute(node, L"ToolTip");
        if (tooltip != nullptr) {
            ToolTipService::SetToolTip(element.Get(), tooltip->value);
        }
        const Attribute* template_attribute = FindAttribute(node, L"Template");
        if (template_attribute != nullptr) {
            const std::wstring template_key = ResourceKey(template_attribute->value);
            if (template_key.empty()) {
                Fail(node, "Template requires a resource key");
                return {};
            }
            direct_templates_.push_back(
                DirectTemplate{element.Get(), &node, template_key, scope});
        }
        const Attribute* style_attribute = FindAttribute(node, L"Style");
        if (style_attribute != nullptr) {
            const std::wstring style_key = ResourceKey(style_attribute->value);
            if (style_key.empty()) {
                Fail(node, "Style requires a resource key");
                return {};
            }
            direct_styles_.push_back(
                DirectStyle{element.Get(), &node, style_key, scope});
        }

        if (local == L"ContextMenu") {
            if (!BuildMenuItems(node, *element)) {
                return {};
            }
        } else if (!AttachChildren(node, *element)) {
            return {};
        }

        const std::wstring trimmed = Trim(node.text);
        if (!trimmed.empty() && !ConsumesText(local)) {
            Fail(node, "element does not accept text content");
            return {};
        }
        locals_.push_back(LocalAttributes{element.Get(), &node});
        return element;
    }

    bool ConsumesText(std::wstring_view tag) const noexcept {
        return tag == L"Label" || tag == L"Button" || tag == L"CheckBox" ||
               tag == L"RadioButton" || tag == L"ToggleButton" ||
               tag == L"ToggleSwitch" || tag == L"TextBox" || tag == L"ToolTip" ||
               tag == L"ContentPresenter";
    }

    bool AttachChildren(const Node& node, Element& element) {
        const std::wstring tag = LocalName(node.name);
        std::vector<Ref<Element>> children;
        for (const Node& child : node.children) {
            const std::wstring local = LocalName(child.name);
            if (EndsWith(local, L".Resources") || local == L"Resources") {
                continue;
            }
            if (EndsWith(local, L".ContextMenu")) {
                if (child.children.size() != 1) {
                    return Fail(child, "ContextMenu property expects one element");
                }
                Ref<Element> menu = BuildElement(child.children.front());
                if (!menu) {
                    return false;
                }
                element.SetContextMenu(std::move(menu));
                continue;
            }
            if (EndsWith(local, L".Content")) {
                if (child.children.size() != 1) {
                    return Fail(child, "Content property expects one element");
                }
                Ref<Element> content = BuildElement(child.children.front());
                if (!content) {
                    return false;
                }
                children.push_back(std::move(content));
                continue;
            }
            if (local.find(L'.') != std::wstring::npos) {
                return Fail(child, "unsupported property element");
            }
            Ref<Element> child_element = BuildElement(child);
            if (!child_element) {
                return false;
            }
            children.push_back(std::move(child_element));
        }

        if (tag == L"StackPanel") {
            auto* panel = dynamic_cast<StackPanel*>(&element);
            if (panel == nullptr) {
                return Fail(node, "internal StackPanel mismatch");
            }
            for (Ref<Element>& child : children) {
                panel->Add(std::move(child));
            }
            return true;
        }
        if (children.empty()) {
            return true;
        }
        if (tag == L"Border") {
            if (children.size() != 1) {
                return Fail(node, "Border accepts one child");
            }
            static_cast<Border*>(&element)->SetChild(std::move(children.front()));
            return true;
        }
        if (tag == L"ScrollViewer") {
            if (children.size() != 1) {
                return Fail(node, "ScrollViewer accepts one child");
            }
            static_cast<ScrollViewer*>(&element)->SetChild(std::move(children.front()));
            return true;
        }
        if (tag == L"Flyout") {
            if (children.size() != 1) {
                return Fail(node, "Flyout accepts one child");
            }
            static_cast<Flyout*>(&element)->SetContent(std::move(children.front()));
            return true;
        }
        return Fail(node, "element does not accept child elements");
    }

    Ref<Element> CreateControl(const Node& node) {
        const std::wstring tag = LocalName(node.name);
        const std::wstring text = DefaultText(node);
        if (tag == L"Border") {
            return Border::Create();
        }
        if (tag == L"Button") {
            return Button::Create(text);
        }
        if (tag == L"CheckBox") {
            return CheckBox::Create(text);
        }
        if (tag == L"RadioButton") {
            return RadioButton::Create(text);
        }
        if (tag == L"ToggleButton") {
            return ToggleButton::Create(text);
        }
        if (tag == L"ToggleSwitch") {
            return ToggleSwitch::Create(text);
        }
        if (tag == L"Label") {
            return Label::Create(text);
        }
        if (tag == L"TextBox") {
            return TextBox::Create(text);
        }
        if (tag == L"ToolTip") {
            return ToolTip::Create(text);
        }
        if (tag == L"StackPanel") {
            float spacing = 12.0f;
            if (const Attribute* attribute = FindAttribute(node, L"Spacing");
                attribute != nullptr && !ParseFloat(attribute->value, &spacing)) {
                Fail(node, "invalid Spacing");
                return {};
            }
            return StackPanel::Create(spacing);
        }
        if (tag == L"ProgressBar") {
            float value = 0.0f;
            if (const Attribute* attribute = FindAttribute(node, L"Value");
                attribute != nullptr && !ParseFloat(attribute->value, &value)) {
                Fail(node, "invalid ProgressBar Value");
                return {};
            }
            return ProgressBar::Create(value);
        }
        if (tag == L"Slider") {
            double value = 0.0;
            double minimum = 0.0;
            double maximum = 1.0;
            if (const Attribute* attribute = FindAttribute(node, L"Value");
                attribute != nullptr && !ParseDouble(attribute->value, &value)) {
                Fail(node, "invalid Slider Value");
                return {};
            }
            if (const Attribute* attribute = FindAttribute(node, L"Minimum");
                attribute != nullptr && !ParseDouble(attribute->value, &minimum)) {
                Fail(node, "invalid Slider Minimum");
                return {};
            }
            if (const Attribute* attribute = FindAttribute(node, L"Maximum");
                attribute != nullptr && !ParseDouble(attribute->value, &maximum)) {
                Fail(node, "invalid Slider Maximum");
                return {};
            }
            return Slider::Create(value, minimum, maximum);
        }
        if (tag == L"ScrollViewer") {
            return ScrollViewer::Create();
        }
        if (tag == L"Flyout") {
            return Flyout::Create();
        }
        if (tag == L"ContextMenu") {
            return ContextMenu::Create();
        }
        if (tag == L"ContentPresenter") {
            return ContentPresenter::Create();
        }
        if (tag == L"Image") {
            return Image::Create();
        }
        Fail(node, "unknown element");
        return {};
    }

    std::wstring DefaultText(const Node& node) const {
        if (const Attribute* attribute = FindAttribute(node, L"Text"); attribute != nullptr) {
            return attribute->value;
        }
        if (const Attribute* attribute = FindAttribute(node, L"Label"); attribute != nullptr) {
            return attribute->value;
        }
        return Trim(node.text);
    }

    bool ApplyProperties(Element& element, const Node& node, bool local_only) {
        const std::wstring tag = LocalName(node.name);
        for (const Attribute& attribute : node.attributes) {
            const std::wstring local = LocalName(attribute.name);
            if (attribute.name == L"xmlns" || StartsWith(attribute.name, L"xmlns:")) {
                continue;
            }
            if (IsStructuralAttribute(local) || local == L"Style" || local == L"Template" ||
                local == L"ToolTip") {
                continue;
            }
            if (local_only && (local == L"Click" || local == L"Submit" ||
                               local == L"CheckedChanged" || local == L"ValueChanged")) {
                continue;
            }
            if (!ApplyAttribute(element, node, tag, local, attribute.value, local_only)) {
                return false;
            }
        }
        return true;
    }

    bool ApplyAttribute(
        Element& element,
        const Node& node,
        std::wstring_view tag,
        std::wstring_view name,
        std::wstring_view raw,
        bool local_only) {
        if (name == L"xmlns" || StartsWith(name, L"xmlns")) {
            return true;
        }
        if (local_only && (name == L"Click" || name == L"Submit" ||
                           name == L"CheckedChanged" || name == L"ValueChanged")) {
            return true;
        }
        if (name == L"Click" || name == L"Submit" || name == L"CheckedChanged" ||
            name == L"ValueChanged") {
            return BindEvent(element, node, tag, name, raw);
        }
        if (ReadOnlyTextProperty(tag, name)) {
            return true;
        }
        if (name == L"Padding" && tag == L"Border") {
            Thickness padding;
            if (!ParseThickness(raw, &padding)) {
                return Fail(node, "invalid Padding");
            }
            static_cast<Border*>(&element)->SetPadding(padding);
            return true;
        }
        if (name == L"Background" && tag == L"Border") {
            std::uint32_t color = 0;
            if (!ParseColor(raw, &color)) {
                return Fail(node, "invalid Background");
            }
            static_cast<Border*>(&element)->SetBackground(color);
            return true;
        }
        if (name == L"BorderColor" && tag == L"Border") {
            std::uint32_t color = 0;
            if (!ParseColor(raw, &color)) {
                return Fail(node, "invalid BorderColor");
            }
            static_cast<Border*>(&element)->SetBorderColor(color);
            return true;
        }
        if (name == L"Source" && tag == L"Image") {
            Result<std::string> utf8 = Utf16ToUtf8(ToUtf16(raw));
            if (!utf8.ok()) {
                return Fail(node, "invalid image path");
            }
            if (!static_cast<Image*>(&element)->LoadFromFile(utf8.value())) {
                return Fail(node, "image failed to load");
            }
            return true;
        }
        if ((name == L"Width" || name == L"Height") && tag == L"Image") {
            float width = 0.0f;
            float height = 0.0f;
            if (const Attribute* attribute = FindAttribute(node, L"Width");
                attribute != nullptr && !ParseFloat(attribute->value, &width)) {
                return Fail(node, "invalid Width");
            }
            if (const Attribute* attribute = FindAttribute(node, L"Height");
                attribute != nullptr && !ParseFloat(attribute->value, &height)) {
                return Fail(node, "invalid Height");
            }
            static_cast<Image*>(&element)->SetDisplaySize(Size{width, height});
            return true;
        }
        if (name == L"Stretch" && tag == L"Image") {
            const std::wstring value = Trim(raw);
            ImageStretch stretch = ImageStretch::Uniform;
            if (value == L"None") {
                stretch = ImageStretch::None;
            } else if (value == L"Fill") {
                stretch = ImageStretch::Fill;
            } else if (value == L"Uniform") {
                stretch = ImageStretch::Uniform;
            } else if (value == L"UniformToFill") {
                stretch = ImageStretch::UniformToFill;
            } else {
                return Fail(node, "invalid Stretch");
            }
            static_cast<Image*>(&element)->SetStretch(stretch);
            return true;
        }
        if (name == L"Orientation" && tag == L"StackPanel") {
            const std::wstring value = Trim(raw);
            if (value == L"Vertical") {
                static_cast<StackPanel*>(&element)->SetOrientation(Orientation::Vertical);
                return true;
            }
            if (value == L"Horizontal") {
                static_cast<StackPanel*>(&element)->SetOrientation(Orientation::Horizontal);
                return true;
            }
            return Fail(node, "invalid Orientation");
        }
        PropertyValueType type = PropertyValueType::String;
        if (!PropertyTypeFor(element, name, &type)) {
            return Fail(node, "unknown property");
        }
        PropertyValue value;
        if (!ConvertPropertyValue(type, raw, &value)) {
            return Fail(node, "invalid property value");
        }
        if (!element.SetProperty(name, value)) {
            return Fail(node, "property rejected the value");
        }
        return true;
    }

    bool BindEvent(
        Element& element,
        const Node& node,
        std::wstring_view tag,
        std::wstring_view name,
        std::wstring_view raw) {
        if (!context_.resolve_event) {
            return Fail(node, "event attribute requires a handler resolver");
        }
        EventHandler handler = context_.resolve_event(std::wstring(raw));
        if (!handler) {
            return Fail(node, "event handler was not found");
        }
        Element* sender = &element;
        const std::wstring event_name(name);
        const std::wstring handler_name(raw);
        if (name == L"Click" && tag == L"Button") {
            static_cast<Button*>(&element)->SetOnClick([handler, sender, event_name, handler_name] {
                EventArgs args;
                args.event = event_name;
                args.handler = handler_name;
                args.sender = sender;
                handler(args);
            });
            return true;
        }
        if (name == L"Submit" && tag == L"TextBox") {
            static_cast<TextBox*>(&element)->SetOnSubmit(
                [handler, sender, event_name, handler_name] {
                    EventArgs args;
                    args.event = event_name;
                    args.handler = handler_name;
                    args.sender = sender;
                    handler(args);
                });
            return true;
        }
        if (name == L"CheckedChanged" &&
            (tag == L"CheckBox" || tag == L"RadioButton" || tag == L"ToggleButton" ||
             tag == L"ToggleSwitch")) {
            auto bind = [handler, sender, event_name, handler_name](bool checked) {
                EventArgs args;
                args.event = event_name;
                args.handler = handler_name;
                args.sender = sender;
                args.boolean = checked;
                handler(args);
            };
            if (tag == L"CheckBox") {
                static_cast<CheckBox*>(&element)->SetOnCheckedChanged(std::move(bind));
            } else if (tag == L"RadioButton") {
                static_cast<RadioButton*>(&element)->SetOnCheckedChanged(std::move(bind));
            } else if (tag == L"ToggleButton") {
                static_cast<ToggleButton*>(&element)->SetOnCheckedChanged(std::move(bind));
            } else {
                static_cast<ToggleSwitch*>(&element)->SetOnCheckedChanged(std::move(bind));
            }
            return true;
        }
        if (name == L"ValueChanged" && tag == L"Slider") {
            static_cast<Slider*>(&element)->SetOnValueChanged(
                [handler, sender, event_name, handler_name](double value) {
                    EventArgs args;
                    args.event = event_name;
                    args.handler = handler_name;
                    args.sender = sender;
                    args.number = value;
                    handler(args);
                });
            return true;
        }
        return Fail(node, "event is not supported by this element");
    }

    void ApplyStyles(Element& element) {
        std::vector<ResourceScope*> chain;
        for (ResourceScope* scope = ScopeFor(element); scope != nullptr;
             scope = scope->parent) {
            chain.push_back(scope);
        }
        for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
            const auto& styles = (*it)->styles->Styles();
            for (const Ref<Style>& style : styles) {
                if (style) {
                    style->Apply(element);
                }
            }
        }

        std::vector<Element*> children;
        element.CollectChildren(children);
        for (Element* child : children) {
            if (child != nullptr) {
                ApplyStyles(*child);
            }
        }
    }

    ResourceScope* ScopeFor(Element& element) const noexcept {
        const auto found = element_scopes_.find(&element);
        return found == element_scopes_.end() ? nullptr : found->second;
    }

    bool BuildMenuItems(const Node& node, Element& element) {
        auto* menu = dynamic_cast<ContextMenu*>(&element);
        if (menu == nullptr) {
            return Fail(node, "internal ContextMenu mismatch");
        }
        for (const Node& child : node.children) {
            const std::wstring local = LocalName(child.name);
            if (EndsWith(local, L".Items")) {
                for (const Node& item : child.children) {
                    if (!AddMenuItem(item, menu)) {
                        return false;
                    }
                }
                continue;
            }
            if (!AddMenuItem(child, menu)) {
                return false;
            }
        }
        return true;
    }

    bool AddMenuItem(const Node& node, ContextMenu* menu) {
        if (LocalName(node.name) != L"Item") {
            return Fail(node, "ContextMenu expects Item elements");
        }
        std::wstring text = Trim(node.text);
        if (const Attribute* attribute = FindAttribute(node, L"Text"); attribute != nullptr) {
            text = attribute->value;
        }
        if (text.empty()) {
            return Fail(node, "ContextMenu Item requires Text");
        }
        std::function<void()> handler;
        if (const Attribute* attribute = FindAttribute(node, L"Click"); attribute != nullptr) {
            if (!context_.resolve_event) {
                return Fail(node, "Click requires a handler resolver");
            }
            EventHandler resolved = context_.resolve_event(attribute->value);
            if (!resolved) {
                return Fail(node, "event handler was not found");
            }
            Element* sender = menu;
            const std::wstring handler_name(attribute->value);
            handler = [resolved, sender, handler_name] {
                EventArgs args;
                args.event = L"Click";
                args.handler = handler_name;
                args.sender = sender;
                resolved(args);
            };
        }
        menu->AddItem(std::move(text), std::move(handler));
        return true;
    }

    const LoadContext& context_;
    const Node& root_;
    std::deque<ResourceScope> scopes_;
    ResourceScope* current_scope_ = nullptr;
    std::unordered_map<Element*, ResourceScope*> element_scopes_;
    std::unordered_map<std::wstring, Ref<Element>> probes_;
    std::vector<LocalAttributes> locals_;
    std::vector<DirectTemplate> direct_templates_;
    std::vector<DirectStyle> direct_styles_;
    std::unordered_map<std::wstring, Ref<Element>> names_;
    Error error_{};
};

}  // namespace
}  // namespace xaml

Result<xaml::Node> xaml::Parse(std::wstring_view source) {
    Parser parser(source);
    Node root;
    if (!parser.ParseDocument(&root)) {
        return Result<Node>::Fail(parser.error());
    }
    return Result<Node>::Ok(std::move(root));
}

Result<xaml::LoadResult> xaml::Load(
    std::wstring_view source,
    const LoadContext& context) {
    Result<Node> parsed = Parse(source);
    if (!parsed.ok()) {
        return Result<LoadResult>::Fail(parsed.error());
    }
    Loader loader(context, parsed.value());
    return loader.Load();
}

Result<xaml::LoadResult> xaml::LoadFile(
    std::string_view utf8_path,
    const LoadContext& context) {
    Result<std::u16string> path = Utf8ToUtf16(utf8_path);
    if (!path.ok()) {
        return Result<LoadResult>::Fail(path.error());
    }
    const std::wstring wide_path(path.value().begin(), path.value().end());
    std::ifstream stream(std::filesystem::path(wide_path), std::ios::binary);
    if (!stream) {
        return Result<LoadResult>::Fail(
            Error{ErrorCode::NotFound, "xaml file not found"});
    }
    std::string bytes((std::istreambuf_iterator<char>(stream)),
                      std::istreambuf_iterator<char>());
    if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xef &&
        static_cast<unsigned char>(bytes[1]) == 0xbb &&
        static_cast<unsigned char>(bytes[2]) == 0xbf) {
        bytes.erase(0, 3);
    }
    Result<std::u16string> text = Utf8ToUtf16(bytes);
    if (!text.ok()) {
        return Result<LoadResult>::Fail(text.error());
    }
    return Load(std::wstring(text.value().begin(), text.value().end()), context);
}

}  // namespace nopeui
