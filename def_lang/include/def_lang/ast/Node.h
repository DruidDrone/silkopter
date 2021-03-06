#pragma once

#include "Attribute.h"
#include "def_lang/Source_Location.h"
#include "def_lang/Serialization.h"
#include "def_lang/Result.h"

namespace ts
{
namespace ast
{

class Node
{
public:
    enum class Type
    {
        NONE,
        ROOT,
        LIST,
        TYPE,
        LITERAL,
        EXPRESSION,
        IDENTIFIER,
        INHERITANCE,

        TEMPLATE_INSTANTIATION,
        TEMPLATE_ARGUMENT,

        NAMESPACE_DECLARATION,
        NAMESPACE_BODY,

        ALIAS_DECLARATION,

        STRUCT_DECLARATION,
        STRUCT_BODY,

        ENUM_DECLARATION,
        ENUM_BODY,

        CONSTANT_DECLARATION,

        MEMBER_DECLARATION,

        INITIALIZER_LIST,
        INITIALIZER,

        ATTRIBUTE,
    };

    Node() = default;
    Node(Node const&) = default;
    Node(Node&&) = default;
    Node(Type type, ts::Source_Location const& loc);

    Node& operator=(Node const&) = default;
    Node& operator=(Node&&) = default;

    Type get_type() const;

    ts::Source_Location const& get_source_location() const;

    Node& copy_children_from(Node const& node);
    Node& move_children_from(Node&& node);

    Node& add_child(Node const& node);
    Node& add_child(Node&& node);
    std::vector<Node> const& get_children() const;

    Node const* find_first_child_by_type(Type type) const;
    std::vector<Node> get_all_children_of_type(Type type) const;

    Node& add_attribute(Attribute const& att);
    Node& add_attribute(Attribute&& att);
    std::vector<Attribute> const& get_attributes() const;
    Attribute const* find_first_attribute_by_name(std::string const& name) const;

    std::string to_string(size_t ident, bool deep) const;

    Result<sz::Value> serialize() const;
    Result<void> deserialize(sz::Value const& value);

private:
    Type m_type = Type::NONE;
    ts::Source_Location m_location;
    std::vector<Node> m_children;
    std::vector<Attribute> m_attributes;

};

}
}
