#ifndef NOPEUI_MARKUP_XAML_H_
#define NOPEUI_MARKUP_XAML_H_

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "nopeui/base/ref.h"
#include "nopeui/base/result.h"
#include "nopeui/layout/element.h"
#include "nopeui/layout/style.h"

namespace nopeui {

namespace xaml {

struct Attribute {
    std::wstring name;
    std::wstring value;
};

// Parsed XML/XAML node. Names keep their namespace prefix; the loader strips
// it before resolving control and property names.
struct Node {
    std::wstring name;
    std::vector<Attribute> attributes;
    std::vector<Node> children;
    std::wstring text;
    int line = 1;
    int column = 1;
};

// Parses the XML subset used by NopeUI XAML: elements, attributes, text,
// comments, processing instructions and the five predefined entities. The
// document has exactly one root element.
Result<Node> Parse(std::wstring_view source);

struct EventArgs {
    std::wstring_view event;
    std::wstring_view handler;
    Element* sender = nullptr;
    bool boolean = false;
    double number = 0.0;
};

using EventHandler = std::function<void(const EventArgs&)>;

struct LoadContext {
    // Resolves a handler name from a Click, Submit, CheckedChanged or
    // ValueChanged attribute. An empty resolver makes event attributes fail.
    std::function<EventHandler(std::wstring_view)> resolve_event;
};

struct LoadResult {
    Ref<Element> root;
    Ref<StyleSheet> style_sheet;
    std::unordered_map<std::wstring, Ref<Element>> names;

    Element* Find(std::wstring_view name) const noexcept {
        auto found = names.find(std::wstring(name));
        return found == names.end() ? nullptr : found->second.Get();
    }
};

// Builds a control tree from XAML source. Resources are collected from
// *.Resources property elements anywhere in the document and applied to the
// whole tree after construction; local attributes are re-applied afterwards
// so they win over style setters.
Result<LoadResult> Load(std::wstring_view source, const LoadContext& context = {});

// Reads a UTF-8 XAML file and loads it.
Result<LoadResult> LoadFile(std::string_view utf8_path, const LoadContext& context = {});

}  // namespace xaml
}  // namespace nopeui

#endif  // NOPEUI_MARKUP_XAML_H_
