#pragma once

#include "IStream.h"

namespace silk
{
namespace node
{
namespace stream
{


class IVoltage : public IScalar_Stream<Type::VOLTAGE>
{
public:
    typedef float                   Value; //volts
    typedef stream::Sample<Value>     Sample;
    virtual auto get_samples() const -> std::vector<Sample> const& = 0;

    typedef stream::Calibration_Data<float> Calibration_Data;
    Calibration_Data calibration_data;
};
DECLARE_CLASS_PTR(IVoltage);


}
}
}
