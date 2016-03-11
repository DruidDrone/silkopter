#include "BrainStdAfx.h"
#include "Servo_Gimbal.h"

#include "sz_math.hpp"
#include "sz_Servo_Gimbal.hpp"

namespace silk
{
namespace node
{

Servo_Gimbal::Servo_Gimbal(HAL& hal)
    : m_hal(hal)
    , m_init_params(new sz::Servo_Gimbal::Init_Params())
    , m_config(new sz::Servo_Gimbal::Config())
{
    m_x_output_stream = std::make_shared<Output_Stream>();
    m_y_output_stream = std::make_shared<Output_Stream>();
    m_z_output_stream = std::make_shared<Output_Stream>();
}

auto Servo_Gimbal::init(rapidjson::Value const& init_params) -> bool
{
    QLOG_TOPIC("servo_gimbal::init");

    sz::Servo_Gimbal::Init_Params sz;
    autojsoncxx::error::ErrorStack result;
    if (!autojsoncxx::from_value(sz, init_params, result))
    {
        std::ostringstream ss;
        ss << result;
        QLOGE("Cannot deserialize Servo_Gimbal data: {}", ss.str());
        return false;
    }
    *m_init_params = sz;
    return init();
}

auto Servo_Gimbal::init() -> bool
{
    if (m_init_params->rate == 0)
    {
        QLOGE("Bad frame rate: {}Hz", m_init_params->rate);
        return false;
    }

    m_x_output_stream->set_rate(m_init_params->rate);
    m_y_output_stream->set_rate(m_init_params->rate);
    m_z_output_stream->set_rate(m_init_params->rate);
    return true;
}

auto Servo_Gimbal::start(q::Clock::time_point tp) -> bool
{
    m_x_output_stream->set_tp(tp);
    m_y_output_stream->set_tp(tp);
    m_z_output_stream->set_tp(tp);
    return true;
}

auto Servo_Gimbal::get_inputs() const -> std::vector<Input>
{
    std::vector<Input> inputs =
    {{
        { stream::IUAV_Frame::TYPE, m_init_params->rate, "UAV Frame", m_frame_accumulator.get_stream_path(0) },
        { stream::IMulti_Commands::TYPE, m_init_params->commands_rate, "Commands", m_commands_accumulator.get_stream_path(0) }
    }};
    return inputs;
}
auto Servo_Gimbal::get_outputs() const -> std::vector<Output>
{
    std::vector<Output> outputs =
    {{
         { "X PWM", m_x_output_stream },
         { "Y PWM", m_y_output_stream },
         { "Z PWM", m_z_output_stream }
    }};
    return outputs;
}

void Servo_Gimbal::process()
{
    QLOG_TOPIC("servo_gimbal::process");

    m_x_output_stream->clear();
    m_y_output_stream->clear();
    m_z_output_stream->clear();

    m_commands_accumulator.process([this](stream::IMulti_Commands::Sample const& i_commands)
    {
        m_commands_sample = i_commands;
    });

    m_frame_accumulator.process([this](stream::IUAV_Frame::Sample const& i_sample)
    {
        if (i_sample.is_healthy && m_commands_sample.is_healthy)
        {
            math::quatf rotation;


            if (m_commands_sample.value.gimbal.reference_frame.get() == stream::IMulti_Commands::Gimbal::Reference_Frame::GIMBAL)
            {
                rotation = m_commands_sample.value.gimbal.target_frame.get();
            }
            else
            {
                math::quatf const& target_rotation = m_commands_sample.value.gimbal.target_frame.get();
                rotation = math::inverse(i_sample.value) * target_rotation;
            }


            math::vec3f rotation_euler;
            rotation.get_as_euler_xyz(rotation_euler.x, rotation_euler.y, rotation_euler.z);

            {
                auto const& config = m_config->x_pwm;

                math::anglef angle(rotation_euler.x);
                angle.normalize();
                float a = angle.radians;
                if (a > math::anglef::pi)
                {
                    a = a - math::anglef::_2pi;
                }
                auto min_a = math::radians(config.min_angle);
                auto max_a = math::radians(config.max_angle);
                auto mu = (a - min_a) / (max_a - min_a);
                mu = math::clamp(mu, 0.f, 1.f);

                m_x_output_stream->push_sample(mu * (config.max_pwm - config.min_pwm) + config.min_pwm, true);
            }
            {
                auto const& config = m_config->y_pwm;
                float mu = 0;
                m_y_output_stream->push_sample(mu * (config.max_pwm - config.min_pwm) + config.min_pwm, true);
            }
            {
                auto const& config = m_config->z_pwm;
                float mu = 0;
                m_z_output_stream->push_sample(mu * (config.max_pwm - config.min_pwm) + config.min_pwm, true);
            }
        }
        else
        {
            m_x_output_stream->push_last_sample(false);
            m_y_output_stream->push_last_sample(false);
            m_z_output_stream->push_last_sample(false);
        }
    });

}

void Servo_Gimbal::set_input_stream_path(size_t idx, q::Path const& path)
{
    if (idx == 0)
    {
        m_frame_accumulator.set_stream_path(0, path, m_init_params->rate, m_hal);
    }
    else if (idx == 1)
    {
        m_commands_accumulator.set_stream_path(0, path, m_init_params->commands_rate, m_hal);
    }
}

auto Servo_Gimbal::set_config(rapidjson::Value const& json) -> bool
{
    QLOG_TOPIC("servo_gimbal::set_config");

    sz::Servo_Gimbal::Config sz;
    autojsoncxx::error::ErrorStack result;
    if (!autojsoncxx::from_value(sz, json, result))
    {
        std::ostringstream ss;
        ss << result;
        QLOGE("Cannot deserialize Servo_Gimbal config data: {}", ss.str());
        return false;
    }

    *m_config = sz;

    return true;
}
auto Servo_Gimbal::get_config() const -> rapidjson::Document
{
    rapidjson::Document json;
    autojsoncxx::to_document(*m_config, json);
    return std::move(json);
}

auto Servo_Gimbal::get_init_params() const -> rapidjson::Document
{
    rapidjson::Document json;
    autojsoncxx::to_document(*m_init_params, json);
    return std::move(json);
}

auto Servo_Gimbal::send_message(rapidjson::Value const& /*json*/) -> rapidjson::Document
{
    return rapidjson::Document();
}

}
}
