#pragma once

#include "IString_Type.h"
#include "IString_Value.h"
#include "ep/Value_Template_EP.h"

namespace ts
{

class String_Value;

struct String_Traits : public IString_Traits
{
    typedef String_Value value_implementation;
};

class String_Value final : public Value_Template_EP<String_Traits>
{
public:

    String_Value(std::shared_ptr<IString_Type const> type);

    using Value_Template_EP<String_Traits>::copy_assign;
    Result<void> copy_assign(IInitializer const& initializer) override;
    std::shared_ptr<IValue> clone() const override;

    Result<void> serialize(ISerializer& serializer) const override;

private:
};

}