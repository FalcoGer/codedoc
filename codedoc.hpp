// HTML documentation generator built on C++26 static reflection

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <meta>
#include <ranges>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace codedoc
{

// =============================================================================
// Part 1: naming helpers (no dependency on the documentation types)
// =============================================================================
namespace detail
{
    namespace meta = std::meta;

    consteval std::string Escape(std::string_view string)
    {
        std::string out;
        for (const char c : string)
        {
            switch (c)
            {
                case '&': out += "&amp;"; break;
                case '<': out += "&lt;"; break;
                case '>': out += "&gt;"; break;
                case '"': out += "&quot;"; break;
                default:  out += c; break;
            }
        }
        return out;
    }

    consteval std::string UIntString(std::uint64_t value)
    {
        if (value == 0)
        {
            return "0";
        }
        std::string s;
        while (value != 0)
        {
            s.insert(s.begin(), static_cast<char>('0' + value % 10));
            value /= 10;
        }
        return s;
    }

    consteval std::string HexString(std::uint64_t value)
    {
        constexpr char digits[] = "0123456789abcdef";
        std::string    out(8, '0');
        for (int i = 7; i >= 0; --i)
        {
            out[static_cast<std::size_t>(i)]   = digits[value & 0xF];
            value                                >>= 4;
        }
        return out;
    }

    consteval std::uint64_t Fnv1a(std::string_view string)
    {
        std::uint64_t h = 1'469'598'103'934'665'603ull;
        for (const char c : string)
        {
            h ^= static_cast<unsigned char>(c);
            h *= 1'099'511'628'211ull;
        }
        return h;
    }

    consteval std::string Sanitize(std::string_view string)
    {
        std::string out;
        for (const char c : string)
        {
            const bool alnum = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
            if (alnum)
            {
                out += c;
            }
            else if (out.empty() || out.back() != '_')
            {
                out += '_';
            }
        }
        return out;
    }

    // Link markers inside CodeDocumentationString:  \x01 <index> ; <escaped text> \x02
    // They are resolved when a page is built (the referenced entity may be
    // incomplete while the annotation is being evaluated).
    consteval std::string RebaseMarkers(std::string_view html, std::size_t offset)
    {
        std::string out;
        for (std::size_t i = 0; i < html.size(); ++i)
        {
            out += html[i];
            if (html[i] == '\x01')
            {
                std::size_t idx = 0;
                while (html[++i] != ';')
                {
                    idx = idx * 10 + static_cast<std::size_t>(html[i] - '0');
                }
                out += UIntString(idx + offset);
                out += ';';
            }
        }
        return out;
    }

    consteval bool IsGlobalNamespace(meta::info info)
    {
        return info == ^^::;
    }

    consteval bool IsClassLike(meta::info info)
    {
        return meta::is_type(info) && !meta::is_type_alias(info) && (meta::is_class_type(info) || meta::is_union_type(info));
    }

    // A class, union or enum that is only declared, not defined (aliases are not
    // looked through). Reflecting its members, bases, enumerators or annotations
    // is not a constant expression, so every such query must be guarded with this.
    consteval bool IsIncomplete(meta::info info)
    {
        if (!meta::is_type(info))
        {
            return false;
        }
        return !meta::is_complete_type(info);
    }


    consteval bool HasParent(meta::info info)
    {
        if (IsGlobalNamespace(info))
        {
            return false;
        }
        return meta::is_namespace(info)
               || meta::is_function(info)
               || meta::is_variable(info)
               || meta::is_template(info)
               || meta::is_type_alias(info)
               || meta::is_nonstatic_data_member(info)
               || meta::is_enumerator(info)
               || (meta::is_type(info) && (meta::is_class_type(info) || meta::is_union_type(info) || meta::is_enum_type(info)));
    }

    consteval bool IsMemberOfClass(meta::info info)
    {
        return HasParent(info) && IsClassLike(meta::parent_of(info));
    }

    // True for `std` and everything nested in it, also when reached through a
    // namespace alias such as `namespace meta = std::meta;`.
    consteval bool IsInStd(meta::info info)
    {
        while (!IsGlobalNamespace(info))
        {
            if (meta::is_namespace(info))
            {
                info = meta::dealias(info);
                if (IsGlobalNamespace(info))
                {
                    return false;
                }
                if (meta::has_identifier(info) && meta::identifier_of(info) == "std" && IsGlobalNamespace(meta::parent_of(info)))
                {
                    return true;
                }
            }
            if (!HasParent(info))
            {
                return false;
            }
            info = meta::parent_of(info);
        }
        return false;
    }


    consteval std::string SimpleName(meta::info info)
    {
        if (IsGlobalNamespace(info))
        {
            return "(global namespace)";
        }
        if (meta::is_function(info))
        {
            if (meta::is_constructor(info))
            {
                return SimpleName(meta::parent_of(info));
            }
            if (meta::is_destructor(info))
            {
                return "~" + SimpleName(meta::parent_of(info));
            }
            if (meta::is_operator_function(info))
            {
                return "operator" + std::string(meta::symbol_of(meta::operator_of(info)));
            }
            if (meta::is_conversion_function(info))
            {
                return "operator " + std::string(meta::display_string_of(meta::return_type_of(info)));
            }
        }
        if (meta::has_identifier(info))
        {
            return std::string(meta::identifier_of(info));
        }
        return std::string(meta::display_string_of(info));
    }

    consteval std::string QualifiedName(meta::info info)
    {
        if (IsGlobalNamespace(info))
        {
            return "(global namespace)";
        }
        std::string name;
        if (HasParent(info))
        {
            const meta::info parentInfo = meta::parent_of(info);
            if (!IsGlobalNamespace(parentInfo))
            {
                name = QualifiedName(parentInfo) + "::";
            }
        }
        return name + SimpleName(info);
    }

    consteval std::size_t OverloadIndex(meta::info info)
    {
        if (!meta::is_function(info) || !HasParent(info))
        {
            return 0;
        }
        std::size_t       idx  = 0;
        const std::string name = SimpleName(info);
        for (const meta::info memberInfo : meta::members_of(meta::parent_of(info), meta::access_context::unchecked()))
        {
            if (memberInfo == info)
            {
                break;
            }
            if (meta::is_function(memberInfo) && SimpleName(memberInfo) == name)
            {
                ++idx;
            }
        }
        return idx;
    }

    // Unique, file-system safe name (including ".html") for an entity.
    consteval std::string FileName(meta::info info)
    {
        if (IsGlobalNamespace(info))
        {
            return "global_namespace.html";
        }
        std::string stem   = Sanitize(QualifiedName(info));
        bool        hashed = meta::is_function(info) || !meta::has_identifier(info) || meta::has_template_arguments(info);
        if (stem.size() > 96)
        {
            stem.resize(96);
            hashed = true;
        }
        if (hashed)
        {
            stem += '_';
            stem += HexString(Fnv1a(QualifiedName(info) + "|" + std::string(meta::display_string_of(info))));
            if (const std::size_t idx = OverloadIndex(info); idx != 0)
            {
                stem += '_';
                stem += UIntString(idx);
            }
        }
        return stem + ".html";
    }

}    // namespace detail

// =============================================================================
// Part 2: documentation strings and annotation types
// =============================================================================

/// Text of a documentation field. Structural (usable inside annotations).
/// Plain text passed to the constructors is HTML-escaped; use Raw() for markup.
/// Contains link markers resolved by the generator, so do not print View()
/// directly.
struct CodeDocumentationString
{
    const char*            data         = nullptr;
    std::size_t            size         = 0;
    const std::meta::info* refs         = nullptr;
    std::size_t            refCount     = 0;

    constexpr CodeDocumentationString() = default;

    consteval CodeDocumentationString(std::string_view plainText)
    {
        const std::string html = detail::Escape(plainText);
        data                   = std::define_static_string(html);
        size                   = html.size();
    }

    consteval CodeDocumentationString(const char* plainText) : CodeDocumentationString(std::string_view {plainText}) {}

    constexpr std::string_view                 View() const { return {data != nullptr ? data : "", size}; }
    constexpr bool                             Empty() const { return size == 0; }
    constexpr std::span<const std::meta::info> References() const { return {refs, refCount}; }

    /// Wraps already-formed HTML (not escaped).
    static consteval CodeDocumentationString Raw(std::string_view html, std::span<const std::meta::info> references = {})
    {
        CodeDocumentationString docString;
        docString.data = std::define_static_string(html);
        docString.size = html.size();
        if (!references.empty())
        {
            docString.refs     = std::define_static_array(references).data();
            docString.refCount = references.size();
        }
        return docString;
    }

    static consteval CodeDocumentationString LineBreak()
    {
        return Raw("</br>");
    }

    static consteval CodeDocumentationString Code(std::string_view text)
    {
        return Raw("<code>" + detail::Escape(text) + "</code>");
    }

    static consteval CodeDocumentationString Bold(std::string_view text)
    {
        return Raw("<b>" + detail::Escape(text) + "</b>");
    }

    static consteval CodeDocumentationString Link(std::string_view url, std::string_view text)
    {
        return Raw("<a href=\"" + detail::Escape(url) + "\">" + detail::Escape(text) + "</a>");
    }

    /// Link to the documentation page of `item`; the page is generated too.
    /// Empty displayText = qualified name of the entity.
    static consteval CodeDocumentationString MakeReferenceTo(std::meta::info item, std::string_view displayText = {})
    {
        std::string html;
        html                        += '\x01';
        html                        += "0;";
        html                        += detail::Escape(displayText);
        html                        += '\x02';
        const std::meta::info references[]  = {item};
        return Raw(html, references);
    }

    /// CodeDocumentationString::MakeReference<^^Class>()
    template <std::meta::info Item>
    static consteval CodeDocumentationString MakeReference(std::string_view displayText = {})
    {
        return MakeReferenceTo(Item, displayText);
    }

    friend consteval CodeDocumentationString operator+ (
      const CodeDocumentationString& lhs, const CodeDocumentationString& rhs
    )
    {
        std::string html;
        html += lhs.View();
        html += detail::RebaseMarkers(rhs.View(), lhs.refCount);
        std::vector<std::meta::info> all(lhs.References().begin(), lhs.References().end());
        all.insert(all.end(), rhs.References().begin(), rhs.References().end());
        return Raw(html, all);
    }
};

/// Name + description pair (template parameters, function parameters).
struct NamedDoc
{
    CodeDocumentationString name;
    CodeDocumentationString description;
};

// Annotation for function parameters:
//   void f([[=ParameterDocumentation{.brief = "..."}]] int x);
// Deliberately has no returns/pre/post/tparams, so filling those on a
// parameter is a compile error.
struct ParameterDocumentation
{
    CodeDocumentationString brief {};
    CodeDocumentationString description {};
};

/// The annotation: [[=CodeDocumentation{.brief = "...", ...}]]
/// Keep the member order, designated initializers must follow it.
struct CodeDocumentation
{
    CodeDocumentationString brief {};
    CodeDocumentationString description {};
    bool                    ignore = false;    ///< exclude from listings and generation

    CodeDocumentationString returns {};        ///< \returns
    CodeDocumentationString pre {};            ///< \pre
    CodeDocumentationString post {};           ///< \post
    CodeDocumentationString throws {};         ///< \throws
    CodeDocumentationString notes {};          ///< \note
    CodeDocumentationString warning {};        ///< \warning
    CodeDocumentationString since {};          ///< \since
    CodeDocumentationString deprecated {};     ///< \deprecated
    CodeDocumentationString see {};            ///< \see

    std::array<NamedDoc, 8> tparams {};        ///< \tparam  (ordered like the template parameter list)
};

// =============================================================================
// Part 3: page generation
// =============================================================================
namespace detail
{

    struct Page
    {
        std::string                      html;
        std::vector<std::meta::info>     refs;
        std::span<const std::meta::info> roots;

        consteval void                   AddRef(meta::info info)
        {
            if (IsInStd(info))
            {
                return;
            }
            for (const meta::info refInfo : refs)
            {
                if (refInfo == info)
                {
                    return;
                }
            }
            refs.push_back(info);
        }

        consteval void Put(std::initializer_list<std::string_view> parts)
        {
            for (const std::string_view part : parts)
            {
                html += part;
            }
        }
    };

    inline constexpr const char* kStyle = R"CSS(
body{font:15px/1.5 system-ui,sans-serif;color:#1d2129;background:#f6f7f9;margin:0}
main{max-width:1000px;margin:0 auto;padding:24px 28px;background:#fff;min-height:100vh}
h1{font-size:1.7em;margin:.2em 0}h2{font-size:1.2em;margin:1.6em 0 .5em;border-bottom:1px solid #dde1e6}
.kind{font-size:.55em;text-transform:uppercase;letter-spacing:.06em;color:#fff;background:#4a6fa5;padding:2px 8px;border-radius:4px;vertical-align:middle}
.crumbs{color:#667;font-size:.9em}.brief{font-size:1.1em;color:#333}
pre.decl{background:#f0f2f5;padding:10px 14px;border-radius:6px;overflow-x:auto}
code{font-family:ui-monospace,Consolas,monospace;font-size:.92em}
table{border-collapse:collapse;width:100%}th,td{text-align:left;vertical-align:top;padding:6px 10px;border-bottom:1px solid #e6e9ed}
th{background:#f0f2f5;font-weight:600}a{color:#1a5fb4;text-decoration:none}a:hover{text-decoration:underline}
.acc{font-size:.8em;padding:1px 7px;border-radius:10px;color:#fff}.public{background:#2e8b57}.protected{background:#c98a00}.private{background:#b5473a}
dl dt{font-weight:700;margin-top:.8em}dl dd{margin:.1em 0 0 1.2em}.tag{font-size:.8em;background:#e6e9ed;border-radius:4px;padding:1px 6px;margin-right:4px}
.incomplete{background:#fff4e0;border-left:4px solid #c98a00;padding:8px 12px;margin:1em 0}
)CSS";

    consteval std::string        LinkHtml(meta::info info, std::string_view escapedText)
    {
        std::string text = escapedText.empty() ? Escape(QualifiedName(info)) : std::string(escapedText);
        if (IsInStd(info))
        {
            return text;
        }
        return "<a href=\"" + FileName(info) + "\">" + text + "</a>";
    }

    /// Appends a documentation string, resolving link markers and collecting references.
    consteval void Append(Page& page, const CodeDocumentationString& docString)
    {
        const std::string_view docStringView    = docString.View();
        const auto             refs = docString.References();
        for (std::size_t i = 0; i < docStringView.size(); ++i)
        {
            if (docStringView[i] != '\x01')
            {
                page.html += docStringView[i];
                continue;
            }
            std::size_t idx = 0;
            while (docStringView[++i] != ';')
            {
                idx = idx * 10 + static_cast<std::size_t>(docStringView[i] - '0');
            }
            std::string text;
            while (docStringView[++i] != '\x02')
            {
                text += docStringView[i];
            }
            page.AddRef(refs[idx]);
            page.html += LinkHtml(refs[idx], text);
        }
    }

    // ---- annotation access ------------------------------------------------------

    consteval bool HasDoc(meta::info info)
    {
        if (IsGlobalNamespace(info) || IsIncomplete(info))
        {
            return false;
        }
        else
        {
            return meta::annotations_of_with_type(info, ^^CodeDocumentation).size() > 0;
        }
    }

    consteval CodeDocumentation DocOf(meta::info info)
    {
        if (IsGlobalNamespace(info) || IsIncomplete(info) || meta::is_template(info))
        {
            // annotations_of is invalid for templates or incomplete types (declaration only).
            return {};
        }
        else
        {
            for (const meta::info annotationInfo : meta::annotations_of_with_type(info, ^^CodeDocumentation))
            {
                return meta::extract<CodeDocumentation>(annotationInfo);
            }
        }
        return {};
    }

    consteval bool IsIgnored(meta::info info)
    {
        return DocOf(info).ignore;
    }
    consteval ParameterDocumentation ParameterDocOf(meta::info param)
    {
        for (const meta::info annotationInfo : meta::annotations_of_with_type(param, ^^ParameterDocumentation))
        {
            return meta::extract<ParameterDocumentation>(annotationInfo);
        }
        return {};
    }
    consteval bool IsDocumented(meta::info info)
    {
        return HasDoc(info) && !DocOf(info).ignore;
    }

    consteval bool IsDocumentedType(meta::info info)
    {
        return meta::is_type(info)
               && (meta::is_type_alias(info) || meta::is_class_type(info) || meta::is_union_type(info) || meta::is_enum_type(info))
               && IsDocumented(info);
    }

    // A namespace counts as documented when it was passed to WriteDocumentation, or
    // when it is (and is not ignored) nested in one that was.
    consteval bool IsDocumentedNamespace(meta::info namespaceInfo, std::span<const meta::info> roots)
    {
        for (const meta::info root : roots)
        {
            if (root == namespaceInfo)
            {
                return true;
            }
        }
        if (IsGlobalNamespace(namespaceInfo) || IsIgnored(namespaceInfo))
        {
            return false;
        }
        return IsDocumentedNamespace(meta::parent_of(namespaceInfo), roots);
    }

    // Ancestors are linked unless they are ignored, in std, or namespaces that are not
    // being documented.
    consteval bool LinkableAncestor(meta::info info, std::span<const meta::info> roots)
    {
        return !IsIgnored(info)
               && !IsInStd(info)
               && (IsDocumented(info) || !meta::is_namespace(info) || IsDocumentedNamespace(info, roots));
    }

    // ---- kinds, access ----------------------------------------------------------

    consteval std::string_view KindLabel(meta::info info)
    {
        if (IsGlobalNamespace(info) || meta::is_namespace(info))
        {
            return "Namespace";
        }
        if (meta::is_template(info))
        {
            if (meta::is_class_template(info))
            {
                return "Class template";
            }
            if (meta::is_function_template(info))
            {
                return "Function template";
            }
            if (meta::is_alias_template(info))
            {
                return "Alias template";
            }
            if (meta::is_variable_template(info))
            {
                return "Variable template";
            }
            return "Template";
        }
        if (meta::is_function(info))
        {
            if (meta::is_constructor(info))
            {
                return "Constructor";
            }
            if (meta::is_destructor(info))
            {
                return "Destructor";
            }
            if (meta::is_operator_function(info))
            {
                return "Operator";
            }
            if (meta::is_conversion_function(info))
            {
                return "Conversion operator";
            }
            if (IsMemberOfClass(info))
            {
                return meta::is_static_member(info) ? "Static member function" : "Member function";
            }
            return "Function";
        }
        if (meta::is_enumerator(info))
        {
            return "Enumerator";
        }
        if (meta::is_nonstatic_data_member(info))
        {
            return "Data member";
        }
        if (meta::is_variable(info))
        {
            return IsMemberOfClass(info) ? "Static data member" : "Variable";
        }
        if (meta::is_type_alias(info))
        {
            return "Type alias";
        }
        if (meta::is_type(info))
        {
            if (meta::is_enum_type(info))
            {
                return "Enumeration";
            }
            if (meta::is_union_type(info))
            {
                return "Union";
            }
            if (meta::is_class_type(info))
            {
                return "Class";
            }
        }
        return "Entity";
    }

    consteval std::string_view AccessOf(meta::info memberInfo)
    {
        if (meta::is_public(memberInfo))
        {
            return "public";
        }
        if (meta::is_protected(memberInfo))
        {
            return "protected";
        }
        if (meta::is_private(memberInfo))
        {
            return "private";
        }
        return "";
    }

    // ---- types, signatures, declarations ----------------------------------------

    consteval meta::info StripType(meta::info info)
    {
        for (;;)
        {
            meta::info typeInfo = meta::remove_cv(info);
            if (meta::is_pointer_type(typeInfo))
            {
                typeInfo = meta::remove_pointer(typeInfo);
            }
            else if (meta::is_lvalue_reference_type(typeInfo) || meta::is_rvalue_reference_type(typeInfo))
            {
                typeInfo = meta::remove_reference(typeInfo);
            }
            else if (meta::is_array_type(typeInfo))
            {
                typeInfo = meta::remove_extent(typeInfo);
            }
            if (typeInfo == info)
            {
                return typeInfo;
            }
            info = typeInfo;
        }
    }

    /// True for user-defined types that may get a page of their own: classes, unions,
    /// enums and aliases. Builtin types and everything in std are excluded.
    consteval bool IsReferenceableType(meta::info info)
    {
        if (!meta::is_type(info) || IsInStd(info))
        {
            return false;
        }
        if (meta::is_type_alias(info))
        {
            // aliases in the global namespace (size_t, uint64_t, ...) mostly come from system headers
            return IsDocumented(info) || (!IsIgnored(info) && HasParent(info) && !IsGlobalNamespace(meta::parent_of(info)));
        }
        return (meta::is_class_type(info) || meta::is_union_type(info) || meta::is_enum_type(info))
               && meta::has_identifier(info)
               && !IsIgnored(info);
    }

    /// Type as HTML. The core type (without cv/ref/pointer/array) is linked when it
    /// carries a CodeDocumentation annotation; the linked page is generated too.
    /// With `linkUndocumented` (used for return types) any user-defined core type is
    /// linked, even without an annotation. Builtin types and anything in std are never linked.
    consteval std::string TypeHtml(Page& page, meta::info info, bool linkUndocumented = false)
    {
        std::string      full   = Escape(meta::display_string_of(info));
        const meta::info core   = StripType(info);

        meta::info       target = core;
        bool             link   = IsDocumentedType(core);
        if (!link && meta::is_type_alias(core))
        {
            target = meta::dealias(core);
            link   = IsDocumentedType(target);
        }
        if (!link && linkUndocumented && IsReferenceableType(core))
        {
            target = core;
            link   = true;
        }
        if (!link)
        {
            return full;
        }

        page.AddRef(target);
        const std::string name   = Escape(meta::display_string_of(core));
        const std::string anchor = "<a href=\"" + FileName(target) + "\">" + name + "</a>";
        if (const std::size_t pos = full.find(name); pos != std::string::npos)
        {
            full.replace(pos, name.size(), anchor);
        }
        else
        {
            full += " (" + anchor + ")";
        }
        return full;
    }

    consteval std::string NameHtml(Page& p, meta::info info, bool link)
    {
        std::string name = Escape(SimpleName(info));
        if (!link || IsInStd(info))
        {
            return name;
        }
        p.AddRef(info);
        return "<a href=\"" + FileName(info) + "\">" + name + "</a>";
    }

    consteval std::string SignatureHtml(Page& page, meta::info fn, bool linkName)
    {
        std::string signature;
        const bool  member = IsMemberOfClass(fn);
        const bool  isCtor = meta::is_constructor(fn);
        const bool  isDtor = meta::is_destructor(fn);
        const bool  isConv = meta::is_conversion_function(fn);

        if (member && meta::is_function(fn) && meta::is_static_member(fn))
        {
            signature += "static ";
        }
        if (member && meta::is_virtual(fn))
        {
            signature += "virtual ";
        }
        if ((isCtor || isConv) && meta::is_explicit(fn))
        {
            signature += "explicit ";
        }
        if (!isCtor && !isDtor && !isConv)
        {
            signature += TypeHtml(page, meta::return_type_of(fn), true);
            signature += ' ';
        }
        signature          += NameHtml(page, fn, linkName);
        signature          += '(';
        bool first  = true;
        for (const meta::info paramInfo : meta::parameters_of(fn))
        {
            if (!first)
            {
                signature += ", ";
            }
            first  = false;
            signature     += TypeHtml(page, meta::type_of(paramInfo));
            if (meta::has_identifier(paramInfo))
            {
                signature += ' ';
                signature += Escape(meta::identifier_of(paramInfo));
            }
            if (meta::has_default_argument(paramInfo))
            {
                signature += " = ...";
            }
        }
        signature += ')';

        if (member && meta::is_function(fn) && !meta::is_static_member(fn) && !isCtor && !isDtor)
        {
            if (meta::is_const(fn))
            {
                signature += " const";
            }
            if (meta::is_volatile(fn))
            {
                signature += " volatile";
            }
            if (meta::is_lvalue_reference_qualified(fn))
            {
                signature += " &amp;";
            }
            if (meta::is_rvalue_reference_qualified(fn))
            {
                signature += " &amp;&amp;";
            }
        }
        if (meta::is_noexcept(fn))
        {
            signature += " noexcept";
        }
        if (member && meta::is_override(fn))
        {
            signature += " override";
        }
        if (member && meta::is_virtual(fn) && meta::is_final(fn))
        {
            signature += " final";
        }
        if (member && meta::is_pure_virtual(fn))
        {
            signature += " = 0";
        }
        if (meta::is_deleted(fn))
        {
            signature += " = delete";
        }
        if (meta::is_defaulted(fn))
        {
            signature += " = default";
        }
        return signature;
    }

    consteval std::string DeclarationHtml(Page& page, meta::info info, bool linkName)
    {
        if (meta::is_function(info))
        {
            return SignatureHtml(page, info, linkName);
        }

        const std::string name = NameHtml(page, info, linkName);
        if (meta::is_nonstatic_data_member(info) || meta::is_variable(info))
        {
            std::string declaration;
            if (meta::is_variable(info) && IsMemberOfClass(info))
            {
                declaration += "static ";
            }
            if (meta::is_nonstatic_data_member(info) && meta::is_mutable_member(info))
            {
                declaration += "mutable ";
            }
            declaration += TypeHtml(page, meta::type_of(info));
            declaration += ' ';
            declaration += name;
            if (meta::is_nonstatic_data_member(info) && meta::is_bit_field(info))
            {
                declaration += " : ";
                declaration += UIntString(meta::bit_size_of(info));
            }
            return declaration;
        }
        if (meta::is_type_alias(info))
        {
            return "using " + name + " = " + TypeHtml(page, meta::dealias(info));
        }
        if (meta::is_template(info))
        {
            return "template&lt;...&gt; " + name;    // can not get template parameters using reflection yet
        }
        if (meta::is_namespace(info))
        {
            return "namespace " + name;
        }
        if (meta::is_type(info))
        {
            std::string declaration;
            if (meta::is_enum_type(info))
            {
                declaration += "enum " + name;
            }
            else if (meta::is_union_type(info))
            {
                declaration += "union " + name;
            }
            else if (meta::is_class_type(info))
            {
                declaration += "class " + name;
            }
            if (IsIncomplete(info))
            {
                declaration += " /* Incomplete (forward-declared) type */";
            }
            return declaration;
        }
        return name;
    }

    // ---- member listings ----------------------------------------------------------

    enum class Group
    {
        Namespaces,
        Types,
        DataMembers,
        Variables,
        Functions
    };

    consteval Group GroupOf(meta::info m)
    {
        if (meta::is_namespace(m))
        {
            return Group::Namespaces;
        }
        if (meta::is_function(m) || meta::is_function_template(m))
        {
            return Group::Functions;
        }
        if (meta::is_nonstatic_data_member(m))
        {
            return Group::DataMembers;
        }
        if (meta::is_variable(m) || meta::is_variable_template(m))
        {
            return Group::Variables;
        }
        return Group::Types;
    }

    consteval std::vector<meta::info> ListableMembers(meta::info owner)
    {
        std::vector<meta::info> out;
        for (const meta::info memberInfo : meta::members_of(owner, meta::access_context::unchecked()))
        {
            if (IsIgnored(memberInfo))
            {
                continue;
            }
            if (meta::is_function(memberInfo) && !meta::is_user_declared(memberInfo))    // implicit special members
            {
                continue;
            }
            if (!meta::has_identifier(memberInfo) && !meta::is_function(memberInfo) && !meta::has_template_arguments(memberInfo))
            {
                continue;    // anonymous unions etc.
            }
            out.push_back(memberInfo);
        }
        return out;
    }

    consteval void MemberTables(Page& page, meta::info owner, bool ownerIsClass)
    {
        const std::vector<meta::info> members = ListableMembers(owner);
        for (const Group group : {Group::Namespaces, Group::Types, Group::DataMembers, Group::Variables, Group::Functions})
        {
            bool any = false;
            for (const meta::info memberInfo : members)
            {
                any = any || GroupOf(memberInfo) == group;
            }
            if (!any)
            {
                continue;
            }

            std::string_view title;
            switch (group)
            {
                case Group::Namespaces:  title = "Namespaces"; break;
                case Group::Types:       title = "Types"; break;
                case Group::DataMembers: title = "Data members"; break;
                case Group::Variables:   title = ownerIsClass ? "Static data members" : "Variables"; break;
                case Group::Functions:   title = ownerIsClass ? "Member functions" : "Functions"; break;
            }

            page.Put({"<h2>", title, "</h2>\n<table class=\"members\"><thead><tr>"});
            if (ownerIsClass)
            {
                page.html += "<th>Access</th>";
            }
            page.html += "<th>Declaration</th><th>Brief</th></tr></thead><tbody>\n";
            for (const meta::info memberInfo : members)
            {
                if (GroupOf(memberInfo) != group)
                {
                    continue;
                }
                page.html += "<tr>";
                if (ownerIsClass)
                {
                    const std::string_view accessString = AccessOf(memberInfo);
                    page.Put({"<td><span class=\"acc ", accessString, "\">", accessString, "</span></td>"});
                }
                page.html += "<td><code>";
                page.html += DeclarationHtml(page, memberInfo, true);
                page.html += "</code></td><td>";
                Append(page, DocOf(memberInfo).brief);
                page.html += "</td></tr>\n";
            }
            page.html += "</tbody></table>\n";
        }
    }

    // ---- page sections ------------------------------------------------------------

    consteval void TemplateSection(Page& page, meta::info entityInfo, const CodeDocumentation& doc)
    {
        if (meta::is_template(entityInfo))
        {
            bool any = false;
            for (const NamedDoc& tparamDoc : doc.tparams)
            {
                any = any || !tparamDoc.name.Empty();
            }
            if (!any)
            {
                return;
            }
            page.html +=
              "<h2>Template parameters</h2>\n<table><thead><tr><th>Name</th><th>Description</th></tr></thead><tbody>\n";
            for (const NamedDoc& tparamDoc : doc.tparams)
            {
                if (tparamDoc.name.Empty())
                {
                    continue;
                }
                page.html += "<tr><td><code>";
                Append(page, tparamDoc.name);
                page.html += "</code></td><td>";
                Append(page, tparamDoc.description);
                page.html += "</td></tr>\n";
            }
            page.html += "</tbody></table>\n";
            return;
        }

        if (!meta::has_template_arguments(entityInfo))
        {
            return;
        }

        const meta::info tmplInfo  = meta::template_of(entityInfo);
        page.html                 += "<h2>Template arguments</h2>\n<p>Specialization of ";
        if (IsDocumented(tmplInfo))
        {
            page.AddRef(tmplInfo);
            page.html += LinkHtml(tmplInfo, "");
        }
        else
        {
            page.html += "<code>" + Escape(QualifiedName(tmplInfo)) + "</code>";
        }
        page.html +=
          ".</p>\n<table><thead><tr><th>Parameter</th><th>Argument</th><th>Description</th></tr></thead><tbody>\n";

        std::size_t index = 0;
        for (const meta::info argInfo : meta::template_arguments_of(entityInfo))
        {
            const NamedDoc* tparamDoc  = index < doc.tparams.size() ? &doc.tparams[index] : nullptr;
            page.html                 += "<tr><td><code>";
            if (tparamDoc != nullptr && !tparamDoc->name.Empty())
            {
                Append(page, tparamDoc->name);
            }
            else
            {
                page.html += "#" + UIntString(index);
            }
            page.html += "</code></td><td><code>";
            page.html += meta::is_type(argInfo) ? TypeHtml(page, argInfo) : Escape(meta::display_string_of(argInfo));
            page.html += "</code></td><td>";
            if (tparamDoc != nullptr)
            {
                Append(page, tparamDoc->description);
            }
            page.html += "</td></tr>\n";
            ++index;
        }
        page.html += "</tbody></table>\n";
    }

    consteval void FunctionSection(Page& page, meta::info functionInfo)
    {
        const auto paramInfos = meta::parameters_of(functionInfo);
        if (paramInfos.empty())
        {
            return;
        }
        page.html +=
          "<h2>Parameters</h2>\n<table><thead><tr><th>Type</th><th>Name</th><th>Description</th></tr></thead><tbody>\n";
        for (const meta::info paramInfo : paramInfos)
        {
            const ParameterDocumentation paramDoc  = ParameterDocOf(paramInfo);
            const std::string_view       name      = meta::has_identifier(paramInfo) ? meta::identifier_of(paramInfo)
                                                                                     : std::string_view {};
            page.html                             += "<tr><td><code>"
                                                     + TypeHtml(page, meta::type_of(paramInfo))
                                                     + "</code></td><td><code>"
                                                     + Escape(name)
                                                     + "</code></td><td>";
            Append(page, paramDoc.brief);
            if (!paramDoc.description.Empty())
            {
                page.html += "<br>";
                Append(page, paramDoc.description);
            }
            page.html += "</td></tr>\n";
        }
        page.html += "</tbody></table>\n";
    }

    consteval void ClassSection(Page& page, meta::info classInfo)
    {
        if (meta::is_class_type(classInfo))
        {
            const auto baseInfos = meta::bases_of(classInfo, meta::access_context::unchecked());
            if (!baseInfos.empty())
            {
                page.html += "<h2>Base classes</h2>\n<ul>\n";
                for (const meta::info baseInfo : baseInfos)
                {
                    const std::string_view accessString = meta::is_public(baseInfo)      ? "public"
                                                          : meta::is_protected(baseInfo) ? "protected"
                                                                                         : "private";
                    page.Put({"<li><span class=\"acc ", accessString, "\">", accessString, "</span> "});
                    if (meta::is_virtual(baseInfo))
                    {
                        page.html += "virtual ";
                    }
                    page.html += "<code>" + TypeHtml(page, meta::type_of(baseInfo)) + "</code></li>\n";
                }
                page.html += "</ul>\n";
            }
        }
        MemberTables(page, classInfo, true);
    }

    consteval void EnumSection(Page& page, meta::info enumInfo)
    {
        page.html   += "<p>Underlying type: <code>" + TypeHtml(page, meta::underlying_type(enumInfo)) + "</code></p>\n";
        bool opened  = false;
        for (const meta::info enumeratorInfo : meta::enumerators_of(enumInfo))
        {
            if (IsIgnored(enumeratorInfo))
            {
                continue;
            }
            if (!opened)
            {
                page.html +=
                  "<h2>Enumerators</h2>\n<table><thead><tr><th>Name</th><th>Brief</th></tr></thead><tbody>\n";
                opened = true;
            }
            page.html += "<tr><td><code>" + NameHtml(page, enumeratorInfo, true) + "</code></td><td>";
            Append(page, DocOf(enumeratorInfo).brief);
            page.html += "</td></tr>\n";
        }
        if (opened)
        {
            page.html += "</tbody></table>\n";
        }
    }

    consteval void DoxygenField(Page& page, std::string_view title, const CodeDocumentationString& docString)
    {
        if (docString.Empty())
        {
            return;
        }
        page.Put({"<dt>", title, "</dt><dd>"});
        Append(page, docString);
        page.html += "</dd>\n";
    }

    consteval void DoxygenSection(Page& page, const CodeDocumentation& doc)
    {
        page.html += "<dl>\n";
        DoxygenField(page, "Returns", doc.returns);
        DoxygenField(page, "Preconditions", doc.pre);
        DoxygenField(page, "Postconditions", doc.post);
        DoxygenField(page, "Throws", doc.throws);
        DoxygenField(page, "Note", doc.notes);
        DoxygenField(page, "Warning", doc.warning);
        DoxygenField(page, "Deprecated", doc.deprecated);
        DoxygenField(page, "Since", doc.since);
        DoxygenField(page, "See also", doc.see);
        page.html += "</dl>\n";
    }

    /// Builds the complete HTML page for `entityInfo` and collects every entity it references.
    consteval Page BuildPage(meta::info entityInfo, std::span<const meta::info> roots)
    {
        Page page;
        page.roots                         = roots;
        const CodeDocumentation doc        = DocOf(entityInfo);
        const std::string       qname      = Escape(QualifiedName(entityInfo));
        const std::string_view  kind       = KindLabel(entityInfo);
        const bool              incomplete = IsIncomplete(entityInfo);
        page.Put(
          {"<!DOCTYPE html>\n<html lang=\"en\"><head><meta charset=\"utf-8\">\n<title>",
           kind,
           " ",
           qname,
           "</title>\n<style>",
           kStyle,
           "</style></head>\n<body><main>\n"}
        );

        // breadcrumbs
        page.html += "<nav class=\"crumbs\">";
        {
            std::vector<meta::info> chain;
            meta::info              currentInfo = entityInfo;
            while (HasParent(currentInfo))
            {
                currentInfo = meta::parent_of(currentInfo);
                if (IsGlobalNamespace(currentInfo))
                {
                    break;
                }
                chain.push_back(currentInfo);
            }
            for (auto iterator = chain.rbegin(); iterator != chain.rend(); ++iterator)
            {
                if (LinkableAncestor(*iterator, page.roots))
                {
                    page.AddRef(*iterator);
                    page.html += LinkHtml(*iterator, Escape(SimpleName(*iterator)));
                }
                else
                {
                    page.html += Escape(SimpleName(*iterator));
                }
                page.html += " :: ";
            }
        }
        page.html += "</nav>\n";

        page.Put({"<h1><span class=\"kind\">", kind, "</span> ", qname, "</h1>\n"});
        page.html += "<pre class=\"decl\">" + DeclarationHtml(page, entityInfo, false) + "</pre>\n";

        // tags
        if (IsMemberOfClass(entityInfo))
        {
            const std::string_view accessString = AccessOf(entityInfo);
            page.Put({"<p><span class=\"acc ", accessString, "\">", accessString, "</span>"});
            const meta::info ownerInfo  = meta::parent_of(entityInfo);
            page.html                  += " member of ";
            page.AddRef(ownerInfo);
            page.html += LinkHtml(ownerInfo, "") + "</p>\n";
        }
        if (incomplete)
        {
            page.html += "<span class=\"tag\">incomplete</span>";
        }
        else if (IsClassLike(entityInfo) && meta::is_class_type(entityInfo))
        {
            if (meta::is_final(entityInfo))
            {
                page.html += "<span class=\"tag\">final</span>";
            }
            if (meta::is_abstract_type(entityInfo))
            {
                page.html += "<span class=\"tag\">abstract</span>";
            }
        }

        if (!doc.brief.Empty())
        {
            page.html += "<p class=\"brief\">";
            Append(page, doc.brief);
            page.html += "</p>\n";
        }
        if (!doc.description.Empty())
        {
            page.html += "<h2>Description</h2>\n<div>";
            Append(page, doc.description);
            page.html += "</div>\n";
        }

        TemplateSection(page, entityInfo, doc);

        if (incomplete)
        {
            page.html += "<p class=\"incomplete\"><b>Incomplete type.</b> Only a forward declaration is visible, so "
                         "its " "members, base classes and enumerators cannot be listed.</p>\n";
        }
        else if (meta::is_function(entityInfo))
        {
            FunctionSection(page, entityInfo);
        }
        else if (meta::is_type_alias(entityInfo))
            ;    // declaration line already shows the aliased type
        else if (meta::is_type(entityInfo) && meta::is_enum_type(entityInfo))
        {
            EnumSection(page, entityInfo);
        }
        else if (IsClassLike(entityInfo))
        {
            ClassSection(page, entityInfo);
        }
        else if (IsGlobalNamespace(entityInfo) || meta::is_namespace(entityInfo))
        {
            MemberTables(page, entityInfo, false);
        }

        DoxygenSection(page, doc);
        page.html += "</main></body></html>\n";
        return page;
    }

    consteval std::span<const std::meta::info> References(meta::info item, std::span<const std::meta::info> roots)
    {
        return std::define_static_array(BuildPage(item, roots).refs);
    }

    template <std::meta::info Item, std::meta::info... Roots>
    void WriteImpl(const std::filesystem::path& dir, std::set<std::string>& done);

}    // namespace detail

// =============================================================================
// Part 4: public API (three free functions)
// =============================================================================

/// 1. The complete HTML document for `item`.

///    `roots` are the entities passed to WriteDocumentation; namespaces among them (and
///    namespaces nested in them) are linked from the breadcrumbs of their members.
consteval std::string_view DocumentationHtml(std::meta::info item, std::span<const std::meta::info> roots = {})
{
    const std::string htmlText = detail::BuildPage(item, roots).html;
    return std::define_static_string(htmlText);
}

/// 2. File name (e.g. "mylib_Vec2.html") used for `item` and in every
///    <a href="file_name.html"> pointing at it.
consteval std::string_view DocumentationFileName(std::meta::info item)
{
    const std::string name = detail::FileName(item);
    return std::define_static_string(name);
}

/// 3. Writes the documentation of every given item into `outputDir`, and
///    recursively that of everything they reference or list (members, base
///    classes, documented types in signatures, explicit references, ...).
///    Each file is written once.
template <std::meta::info First, std::meta::info... Rest>
void WriteDocumentation(const std::filesystem::path& outputDir)
{
    std::set<std::string> done;
    detail::WriteImpl<First, First, Rest...>(outputDir, done);
    (detail::WriteImpl<Rest, First, Rest...>(outputDir, done), ...);
}

namespace detail
{
    template <std::meta::info Item, std::meta::info... Roots>
    void WriteImpl(const std::filesystem::path& dir, std::set<std::string>& done)
    {
        constexpr std::string_view fileName = DocumentationFileName(Item);
        if (!done.emplace(fileName).second)
        {
            return;
        }

        static constexpr std::array<std::meta::info, sizeof...(Roots)> roots {{Roots...}};

        constexpr std::string_view                                     html = DocumentationHtml(Item, roots);
        std::filesystem::create_directories(dir);
        std::ofstream out(dir / std::string(fileName), std::ios::binary | std::ios::trunc);
        if (!out)
        {
            throw std::runtime_error("codedoc: cannot write " + (dir / std::string(fileName)).display_string());
        }
        out.write(html.data(), static_cast<std::streamsize>(html.size()));

        static constexpr auto refs = References(Item, roots);
        template for (constexpr std::meta::info ref : refs) WriteImpl<ref, Roots...>(dir, done);
    }
}    // namespace detail

}    // namespace codedoc
