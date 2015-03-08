#include "BrainStdAfx.h"
#include "ADC_Voltmeter.h"

#include "sz_math.hpp"
#include "sz_ADC_Voltmeter.hpp"

namespace silk
{
namespace node
{

ADC_Voltmeter::ADC_Voltmeter(HAL& hal)
    : m_hal(hal)
    , m_init_params(new sz::ADC_Voltmeter::Init_Params())
    , m_config(new sz::ADC_Voltmeter::Config())
{
}

auto ADC_Voltmeter::init(rapidjson::Value const& init_params) -> bool
{
    QLOG_TOPIC("adc_voltmeter::init");

    sz::ADC_Voltmeter::Init_Params sz;
    autojsoncxx::error::ErrorStack result;
    if (!autojsoncxx::from_value(sz, init_params, result))
    {
        std::ostringstream ss;
        ss << result;
        QLOGE("Cannot deserialize ADC_Voltmeter data: {}", ss.str());
        return false;
    }
    *m_init_params = sz;
    return init();
}
auto ADC_Voltmeter::init() -> bool
{
    m_output_stream = std::make_shared<Stream>();
    if (m_init_params->rate == 0)
    {
        QLOGE("Bad rate: {}Hz", m_init_params->rate);
        return false;
    }
    m_output_stream->rate = m_init_params->rate;
    return true;
}

auto ADC_Voltmeter::get_inputs() const -> std::vector<Input>
{
    std::vector<Input> inputs(1);
    inputs[0].class_id = q::rtti::get_class_id<stream::IADC_Value>();
    inputs[0].rate = m_output_stream->rate;
    inputs[0].name = "ADC Value";
    return inputs;
}
auto ADC_Voltmeter::get_outputs() const -> std::vector<Output>
{
    std::vector<Output> outputs(1);
    outputs[0].class_id = q::rtti::get_class_id<stream::IVoltage>();
    outputs[0].name = "Voltage";
    outputs[0].stream = m_output_stream;
    return outputs;
}


void ADC_Voltmeter::process()
{
    m_output_stream->samples.clear();

    auto adc_stream = m_adc_stream.lock();
    if (!adc_stream)
    {
        return;
    }

    auto const& s = adc_stream->get_samples();
    m_output_stream->samples.resize(s.size());

    std::transform(s.begin(), s.end(), m_output_stream->samples.begin(), [this](stream::IADC_Value::Sample const& sample)
    {
       Stream::Sample vs;
       vs.dt = sample.dt;
       vs.sample_idx = sample.sample_idx;
       vs.value = sample.value * m_config->outputs.voltage.scale + m_config->outputs.voltage.bias;
       return vs;
    });

    //TODO - apply scale - bias
}

auto ADC_Voltmeter::set_config(rapidjson::Value const& json) -> bool
{
    sz::ADC_Voltmeter::Config sz;
    autojsoncxx::error::ErrorStack result;
    if (!autojsoncxx::from_value(sz, json, result))
    {
        std::ostringstream ss;
        ss << result;
        QLOGE("Cannot deserialize ADC_Voltmeter config data: {}", ss.str());
        return false;
    }

    auto adc_stream = m_hal.get_streams().find_by_name<stream::IADC_Value>(sz.inputs.adc_value);

    auto rate = adc_stream ? adc_stream->get_rate() : 0u;
    if (rate != m_output_stream->rate)
    {
        QLOGE("Bad input stream '{}'. Expected rate {}Hz, got {}Hz", sz.inputs.adc_value, m_output_stream->rate, rate);
        return false;
    }

    m_adc_stream = adc_stream;
    *m_config = sz;

    return true;
}
auto ADC_Voltmeter::get_config() const -> rapidjson::Document
{
    rapidjson::Document json;
    autojsoncxx::to_document(*m_config, json);
    return std::move(json);
}

auto ADC_Voltmeter::get_init_params() const -> rapidjson::Document
{
    rapidjson::Document json;
    autojsoncxx::to_document(*m_init_params, json);
    return std::move(json);
}


}
}