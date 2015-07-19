#pragma once

#include "Stream_Base.h"

namespace silk
{
namespace node
{
namespace stream
{

class IProximity : public IScalar_Stream<Type::PROXIMITY>
{
public:
    typedef std::false_type can_be_filtered_t;

    struct Value
    {
        std::vector<math::vec3f> distances;
    };

    typedef stream::Sample<Value>     Sample;
    virtual auto get_samples() const -> std::vector<Sample> const& = 0;
};
DECLARE_CLASS_PTR(IProximity);

}
}
}

