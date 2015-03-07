#include "BrainStdAfx.h"
#include "ADC_Ammeter.h"

#include "sz_math.hpp"
#include "sz_ADC_Ammeter.hpp"

namespace silk
{
namespace node
{

ADC_Ammeter::ADC_Ammeter(HAL& hal)
    : m_hal(hal)
    , m_init_params(new sz::ADC_Ammeter::Init_Params())
    , m_config(new sz::ADC_Ammeter::Config())
{
}

auto ADC_Ammeter::init(rapidjson::Value const& init_params) -> bool
{
    QLOG_TOPIC("adc_ammeter::init");

    sz::ADC_Ammeter::Init_Params sz;
    autojsoncxx::error::ErrorStack result;
    if (!autojsoncxx::from_value(sz, init_params, result))
    {
        std::ostringstream ss;
        ss << result;
        QLOGE("Cannot deserialize ADC_Ammeter data: {}", ss.str());
        return false;
    }
    *m_init_params = sz;
    return init();
}
auto ADC_Ammeter::init() -> bool
{
    m_stream = std::make_shared<Stream>();
    return true;
}

auto ADC_Ammeter::get_inputs() const -> std::vector<Input>
{
    std::vector<Input> inputs(1);
    inputs[0].class_id = q::rtti::get_class_id<stream::IADC_Value>();
    inputs[0].name = "ADC Value";
    return inputs;
}
auto ADC_Ammeter::get_outputs() const -> std::vector<Output>
{
    std::vector<Output> outputs(1);
    outputs[0].class_id = q::rtti::get_class_id<stream::ICurrent>();
    outputs[0].name = "Current";
    outputs[0].stream = m_stream;
    return outputs;
}

void ADC_Ammeter::process()
{
    m_stream->samples.clear();

    auto adc_stream = m_adc_stream.lock();
    if (!adc_stream)
    {
        return;
    }

    auto const& s = adc_stream->get_samples();
    m_stream->samples.resize(s.size());

    std::transform(s.begin(), s.end(), m_stream->samples.begin(), [this](stream::IADC_Value::Sample const& sample)
    {
       Stream::Sample vs;
       vs.dt = sample.dt;
       vs.sample_idx = sample.sample_idx;
       vs.value = sample.value * m_config->outputs.current.scale + m_config->outputs.current.bias;
       return vs;
    });

    //TODO - apply scale - bias
}

auto ADC_Ammeter::set_config(rapidjson::Value const& json) -> bool
{
    sz::ADC_Ammeter::Config sz;
    autojsoncxx::error::ErrorStack result;
    if (!autojsoncxx::from_value(sz, json, result))
    {
        std::ostringstream ss;
        ss << result;
        QLOGE("Cannot deserialize ADC_Ammeter config data: {}", ss.str());
        return false;
    }

    auto adc_stream = m_hal.get_streams().find_by_name<stream::IADC_Value>(sz.inputs.adc_value);
    if (!adc_stream || adc_stream->get_rate() == 0)
    {
        QLOGE("No input angular velocity stream specified");
        return false;
    }
    if (m_stream->rate != 0 && m_stream->rate != adc_stream->get_rate())
    {
        QLOGE("Input streams rate has changed: {} != {}",
              adc_stream->get_rate(),
              m_stream->rate);
        return false;
    }

    m_adc_stream = adc_stream;
    m_stream->rate = adc_stream->get_rate();

    *m_config = sz;
    return true;
}
auto ADC_Ammeter::get_config() const -> rapidjson::Document
{
    rapidjson::Document json;
    autojsoncxx::to_document(*m_config, json);
    return std::move(json);
}

auto ADC_Ammeter::get_init_params() const -> rapidjson::Document
{
    rapidjson::Document json;
    autojsoncxx::to_document(*m_init_params, json);
    return std::move(json);
}


}
}
