#include "BrainStdAfx.h"
#include "Velocity_Controller.h"

#include "sz_math.hpp"
#include "sz_Velocity_Controller.hpp"

namespace silk
{
namespace node
{

Velocity_Controller::Velocity_Controller(HAL& hal)
    : m_hal(hal)
    , m_init_params(new sz::Velocity_Controller::Init_Params())
    , m_config(new sz::Velocity_Controller::Config())
{
    autojsoncxx::to_document(*m_init_params, m_init_paramsj);
}

auto Velocity_Controller::init(rapidjson::Value const& init_params) -> bool
{
    QLOG_TOPIC("velocity_controller::init");

    sz::Velocity_Controller::Init_Params sz;
    autojsoncxx::error::ErrorStack result;
    if (!autojsoncxx::from_value(sz, init_params, result))
    {
        std::ostringstream ss;
        ss << result;
        QLOGE("Cannot deserialize Velocity_Controller data: {}", ss.str());
        return false;
    }
    jsonutil::clone_value(m_init_paramsj, init_params, m_init_paramsj.GetAllocator());
    *m_init_params = sz;
    return init();
}
auto Velocity_Controller::init() -> bool
{
    m_frame_stream = std::make_shared<Frame>();
    m_force_stream = std::make_shared<Force>();
    if (m_init_params->rate == 0)
    {
        QLOGE("Bad rate: {}Hz", m_init_params->rate);
        return false;
    }
    m_frame_stream->rate = m_init_params->rate;
    m_force_stream->rate = m_init_params->rate;
    m_dt = std::chrono::microseconds(1000000 / m_init_params->rate);
    return true;
}

auto Velocity_Controller::get_stream_inputs() const -> std::vector<Stream_Input>
{
    std::vector<Stream_Input> inputs =
    {{
        { stream::IVelocity::TYPE, m_init_params->rate, "Input" },
        { stream::IVelocity::TYPE, m_init_params->rate, "Target" }
    }};
    return inputs;
}
auto Velocity_Controller::get_stream_outputs() const -> std::vector<Stream_Output>
{
    std::vector<Stream_Output> outputs(2);
    outputs[0].type = stream::IFrame::TYPE;
    outputs[0].name = "Frame";
    outputs[0].stream = m_frame_stream;
    outputs[1].type = stream::IForce::TYPE;
    outputs[1].name = "Collective Frame";
    outputs[1].stream = m_force_stream;
    return outputs;
}

void Velocity_Controller::process()
{
    QLOG_TOPIC("velocity_controller::process");

    m_frame_stream->samples.clear();
    m_force_stream->samples.clear();

    auto target_stream = m_target_stream.lock();
    auto input_stream = m_input_stream.lock();
    if (!target_stream || !input_stream)
    {
        return;
    }

    //accumulate the input streams
    {
        auto const& samples = target_stream->get_samples();
        m_target_samples.reserve(m_target_samples.size() + samples.size());
        std::copy(samples.begin(), samples.end(), std::back_inserter(m_target_samples));
    }
    {
        auto const& samples = input_stream->get_samples();
        m_input_samples.reserve(m_input_samples.size() + samples.size());
        std::copy(samples.begin(), samples.end(), std::back_inserter(m_input_samples));
    }

    //TODO add some protecton for severely out-of-sync streams

    size_t count = std::min(m_target_samples.size(), m_input_samples.size());
    if (count == 0)
    {
        return;
    }

    m_frame_stream->samples.resize(count);
    m_force_stream->samples.resize(count);

    for (size_t i = 0; i < count; i++)
    {
        m_frame_stream->last_sample.dt = m_dt;
        m_frame_stream->last_sample.sample_idx++;
        m_frame_stream->samples[i] = m_frame_stream->last_sample;

        m_force_stream->last_sample.dt = m_dt;
        m_force_stream->last_sample.sample_idx++;
        m_force_stream->samples[i] = m_force_stream->last_sample;
    }

    //consume processed samples
    m_target_samples.erase(m_target_samples.begin(), m_target_samples.begin() + count);
    m_input_samples.erase(m_input_samples.begin(), m_input_samples.begin() + count);
}

auto Velocity_Controller::set_config(rapidjson::Value const& json) -> bool
{
    QLOG_TOPIC("velocity_controller::set_config");

    sz::Velocity_Controller::Config sz;
    autojsoncxx::error::ErrorStack result;
    if (!autojsoncxx::from_value(sz, json, result))
    {
        std::ostringstream ss;
        ss << result;
        QLOGE("Cannot deserialize Velocity_Controller config data: {}", ss.str());
        return false;
    }

    *m_config = sz;

    auto input_stream = m_hal.get_streams().find_by_name<stream::IVelocity>(sz.input_streams.input);
    auto target_stream = m_hal.get_streams().find_by_name<stream::IVelocity>(sz.input_streams.target);

    auto rate = input_stream ? input_stream->get_rate() : 0u;
    if (rate != m_init_params->rate)
    {
        m_config->input_streams.input.clear();
        QLOGW("Bad input stream '{}'. Expected rate {}Hz, got {}Hz", sz.input_streams.input, m_init_params->rate, rate);
        m_input_stream.reset();
    }
    else
    {
        m_input_stream = input_stream;
    }

    rate = target_stream ? target_stream->get_rate() : 0u;
    if (rate != m_init_params->rate)
    {
        m_config->input_streams.target.clear();
        QLOGW("Bad input stream '{}'. Expected rate {}Hz, got {}Hz", sz.input_streams.target, m_init_params->rate, rate);
        m_target_stream.reset();
    }
    else
    {
        m_target_stream = target_stream;
    }

    return true;
}
auto Velocity_Controller::get_config() const -> rapidjson::Document
{
    rapidjson::Document json;
    autojsoncxx::to_document(*m_config, json);
    return std::move(json);
}

auto Velocity_Controller::get_init_params() const -> rapidjson::Document const&
{
    return m_init_paramsj;
}
auto Velocity_Controller::send_message(rapidjson::Value const& /*json*/) -> rapidjson::Document
{
    return rapidjson::Document();
}


}
}