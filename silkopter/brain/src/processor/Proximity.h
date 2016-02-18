#pragma once

#include "HAL.h"
#include "common/stream/IDistance.h"
#include "common/stream/IProximity.h"
#include "common/node/IProcessor.h"

#include "Sample_Accumulator.h"
#include "Basic_Output_Stream.h"

namespace sz
{
namespace Proximity
{
struct Init_Params;
struct Config;
}
}

namespace silk
{
namespace node
{

class Proximity : public IProcessor
{
public:
    Proximity(HAL& hal);

    auto init(rapidjson::Value const& init_params) -> bool;
    auto get_init_params() const -> rapidjson::Document;

    auto set_config(rapidjson::Value const& json) -> bool;
    auto get_config() const -> rapidjson::Document;

    auto send_message(rapidjson::Value const& json) -> rapidjson::Document;

    auto start(q::Clock::time_point tp) -> bool override;

    void set_input_stream_path(size_t idx, q::Path const& path);
    auto get_inputs() const -> std::vector<Input>;
    auto get_outputs() const -> std::vector<Output>;

    void process();

private:
    auto init() -> bool;

    HAL& m_hal;

    std::shared_ptr<sz::Proximity::Init_Params> m_init_params;
    std::shared_ptr<sz::Proximity::Config> m_config;

    std::vector<Sample_Accumulator<stream::IDistance>> m_accumulators;

    typedef Basic_Output_Stream<stream::IProximity> Output_Stream;
    mutable std::shared_ptr<Output_Stream> m_output_stream;

    stream::IProximity::Value m_output_value;
};


}
}

