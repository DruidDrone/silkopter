#include "stdafx.h"
#include "Sensors.h"
#include "physics/constants.h"


Sensors::Sensors(QWidget *parent /* = 0 */)
	: QWidget(parent)
    , m_comms(nullptr)
{
	m_ui.setupUi(this);

    m_ui.a_plot->addGraph();
    m_ui.a_plot->graph(0)->setPen(QPen(Qt::red));
    m_ui.a_plot->addGraph();
    m_ui.a_plot->graph(1)->setPen(QPen(Qt::green));
    m_ui.a_plot->addGraph();
    m_ui.a_plot->graph(2)->setPen(QPen(Qt::blue));
//    m_ui.a_plot->setInteractions(QCP::Interactions(QCP::iRangeZoom | QCP::iRangeDrag));
    //m_ui.a_plot->yAxis->setRange(-1.2, -1);

    m_ui.a_spectrum->addGraph();
    m_ui.a_spectrum->graph(0)->setPen(QPen(Qt::red));
    m_ui.a_spectrum->addGraph();
    m_ui.a_spectrum->graph(1)->setPen(QPen(Qt::green));
    m_ui.a_spectrum->addGraph();
    m_ui.a_spectrum->graph(2)->setPen(QPen(Qt::blue));

    m_ui.g_plot->addGraph();
    m_ui.g_plot->graph(0)->setPen(QPen(Qt::red));
    m_ui.g_plot->addGraph();
    m_ui.g_plot->graph(1)->setPen(QPen(Qt::green));
    m_ui.g_plot->addGraph();
    m_ui.g_plot->graph(2)->setPen(QPen(Qt::blue));

    m_ui.g_spectrum->addGraph();
    m_ui.g_spectrum->graph(0)->setPen(QPen(Qt::red));
    m_ui.g_spectrum->addGraph();
    m_ui.g_spectrum->graph(1)->setPen(QPen(Qt::green));
    m_ui.g_spectrum->addGraph();
    m_ui.g_spectrum->graph(2)->setPen(QPen(Qt::blue));

    m_ui.c_plot->addGraph();
    m_ui.c_plot->graph(0)->setPen(QPen(Qt::red));
    m_ui.c_plot->addGraph();
    m_ui.c_plot->graph(1)->setPen(QPen(Qt::green));
    m_ui.c_plot->addGraph();
    m_ui.c_plot->graph(2)->setPen(QPen(Qt::blue));

	m_ui.barometer_plot->addGraph();
	m_ui.barometer_plot->graph(0)->setPen(QPen(Qt::red));
    m_ui.barometer_plot->addGraph();
    m_ui.barometer_plot->graph(1)->setPen(QPen(Qt::blue));

    m_ui.sonar_plot->addGraph();
    m_ui.sonar_plot->graph(0)->setPen(QPen(Qt::red));
    m_ui.sonar_plot->addGraph();
    m_ui.sonar_plot->graph(1)->setPen(QPen(Qt::blue));

    m_ui.thermometer_plot->addGraph();
    m_ui.thermometer_plot->graph(0)->setPen(QPen(Qt::red));
    m_ui.thermometer_plot->addGraph();
    m_ui.thermometer_plot->graph(1)->setPen(QPen(Qt::blue));

    connect(m_ui.a_calibrate, &QPushButton::released, this, &Sensors::start_accelerometer_calibration);
    connect(m_ui.g_calibrate, &QPushButton::released, this, &Sensors::start_gyroscope_calibration);
    connect(m_ui.c_calibrate, &QPushButton::released, this, &Sensors::start_compass_calibration);

    m_last_time = q::Clock::now();

    m_calibration.message_box.setParent(this);

    // Allocate input and output buffers
    m_gyroscope_fft.temp_input.reset(static_cast<double*>(fftw_malloc(FFT_Data::MAX_INPUT_SIZE * sizeof(double))), fftw_free);
    m_gyroscope_fft.temp_output.reset(static_cast<fftw_complex*>(fftw_malloc(FFT_Data::MAX_INPUT_SIZE * sizeof(fftw_complex))), fftw_free);

    m_accelerometer_fft.temp_input.reset(static_cast<double*>(fftw_malloc(FFT_Data::MAX_INPUT_SIZE * sizeof(double))), fftw_free);
    m_accelerometer_fft.temp_output.reset(static_cast<fftw_complex*>(fftw_malloc(FFT_Data::MAX_INPUT_SIZE * sizeof(fftw_complex))), fftw_free);
}

Sensors::~Sensors()
{
}

void Sensors::init(silk::Comms* comms)
{
    m_comms = comms;
}

void Sensors::process()
{
    auto now = q::Clock::now();
    //auto d = now - m_last_time;
	m_last_time = now;

    QASSERT(m_comms);

    m_ui.a_calibrate->setEnabled(m_comms->is_connected());
    m_ui.a_test_filter->setEnabled(m_comms->is_connected());
    m_ui.g_calibrate->setEnabled(m_comms->is_connected());
    m_ui.g_test_filter->setEnabled(m_comms->is_connected());

    static const float graph_length_seconds = 3.f;

    if (isVisible()/* && m_comms->is_connected()*/)
	{
        {
            auto data = m_comms->get_sensor_gyroscope_data();


            if (0)
            {
                //freq, amplitude
                static const std::chrono::milliseconds period(1);
                static const std::vector<std::pair<float, float>> frequencies =
                {{10, 1}, {110, 1}, {180, 1}, {250, 1}, {310, 1}, {370, 1}, {480, 1}};

                {
                    data.value.sample_time = period;

                    static q::Clock::time_point last_time = q::Clock::now();
                    static float time = 0;
                    auto now = q::Clock::now();
                    auto d = std::chrono::duration_cast<std::chrono::microseconds>(now - last_time);
                    size_t sample_count = d / period;
                    if (sample_count > 0)
                    {
                        last_time = now;

                        for (size_t i = 0; i < sample_count; i++)
                        {
                            float sample = 0;
                            for (auto freq: frequencies)
                            {
                                sample += math::sin(time * math::anglef::_2pi * freq.first) * freq.second;
                            }
                            data.value.angular_velocities.push_back(math::vec3f(sample));
                            time += q::Seconds(period).count();
                        }
                    }
                }
            }

            m_gyroscope_fft.sample_rate = static_cast<size_t>(1.f / q::Seconds(data.value.sample_time).count());

            float time_increment = q::Seconds(data.value.sample_time).count();
            for (auto& av: data.value.angular_velocities)
            {
                m_gyroscope_sample_time += time_increment;
                m_ui.g_plot->graph(0)->addData(m_gyroscope_sample_time, av.x);
                m_ui.g_plot->graph(1)->addData(m_gyroscope_sample_time, av.y);
                m_ui.g_plot->graph(2)->addData(m_gyroscope_sample_time, av.z);
                m_gyroscope_fft.input.push_back(av);
            }

            if (m_gyroscope_sample_time > graph_length_seconds)
            {
                m_ui.g_plot->graph(0)->removeDataBefore(m_gyroscope_sample_time - graph_length_seconds);
                m_ui.g_plot->graph(1)->removeDataBefore(m_gyroscope_sample_time - graph_length_seconds);
                m_ui.g_plot->graph(2)->removeDataBefore(m_gyroscope_sample_time - graph_length_seconds);
            }
            m_ui.g_plot->rescaleAxes(true);
            m_ui.g_plot->replot();

            process_gyroscope_fft(m_gyroscope_fft);
            if (!m_gyroscope_fft.output.empty())
            {
                m_ui.g_spectrum->graph(0)->clearData();
                m_ui.g_spectrum->graph(1)->clearData();
                m_ui.g_spectrum->graph(2)->clearData();

                for (size_t i = 0; i < m_gyroscope_fft.output.size(); i++)
                {
                    auto const& s = m_gyroscope_fft.output[i];
                    auto freq = (m_gyroscope_fft.sample_rate / 2) * i / m_gyroscope_fft.output.size();
                    m_ui.g_spectrum->graph(0)->addData(freq, s.x);
                    m_ui.g_spectrum->graph(1)->addData(freq, s.y);
                    m_ui.g_spectrum->graph(2)->addData(freq, s.z);
                }
                m_ui.g_spectrum->rescaleAxes(true);
                m_ui.g_spectrum->replot();
            }
        }

        {
            auto data = m_comms->get_sensor_accelerometer_data();
            m_accelerometer_fft.sample_rate = static_cast<size_t>(1.f / q::Seconds(data.value.sample_time).count());

            float time_increment = q::Seconds(data.value.sample_time).count();
            for (auto& av: data.value.accelerations)
            {
                m_accelerometer_sample_time += time_increment;
                m_ui.a_plot->graph(0)->addData(m_accelerometer_sample_time, av.x);
                m_ui.a_plot->graph(1)->addData(m_accelerometer_sample_time, av.y);
                m_ui.a_plot->graph(2)->addData(m_accelerometer_sample_time, av.z);
                m_accelerometer_fft.input.push_back(av);
            }

            if (m_accelerometer_sample_time > graph_length_seconds)
            {
                m_ui.a_plot->graph(0)->removeDataBefore(m_accelerometer_sample_time - graph_length_seconds);
                m_ui.a_plot->graph(1)->removeDataBefore(m_accelerometer_sample_time - graph_length_seconds);
                m_ui.a_plot->graph(2)->removeDataBefore(m_accelerometer_sample_time - graph_length_seconds);
            }
            m_ui.a_plot->rescaleAxes(true);
            m_ui.a_plot->replot();

            process_accelerometer_fft(m_accelerometer_fft);
            if (!m_accelerometer_fft.output.empty())
            {
                m_ui.a_spectrum->graph(0)->clearData();
                m_ui.a_spectrum->graph(1)->clearData();
                m_ui.a_spectrum->graph(2)->clearData();

                for (size_t i = 0; i < m_accelerometer_fft.output.size(); i++)
                {
                    auto const& s = m_accelerometer_fft.output[i];
                    auto freq = (m_accelerometer_fft.sample_rate / 2) * i / m_accelerometer_fft.output.size();
                    m_ui.a_spectrum->graph(0)->addData(freq, s.x);
                    m_ui.a_spectrum->graph(1)->addData(freq, s.y);
                    m_ui.a_spectrum->graph(2)->addData(freq, s.z);
                }
                m_ui.a_spectrum->rescaleAxes(true);
                m_ui.a_spectrum->replot();
            }
        }


        auto alive = m_comms->get_uav_alive_duration();
        if (alive != m_last_uav_alive_duration)
		{
            m_last_uav_alive_duration = alive;

            double seconds = q::Seconds(alive).count();
            //static double seconds = 0;// double(time_ms) / 1000.0;
			//seconds += 0.01f;

// 			static float s_cpu = 0;
// 			s_cpu = math::lerp(s_cpu, m_protocol->data_board_cpu_usage.value, 0.01f);
// 			m_ui.g_plot->graph(0)->addData(seconds, s_cpu);
            {
                auto data = m_comms->get_sensor_compass_data();
                auto direction = math::normalized<float, math::safe>(data.value);
                m_ui.c_plot->graph(0)->addData(seconds, direction.x);
                m_ui.c_plot->graph(1)->addData(seconds, direction.y);
                m_ui.c_plot->graph(2)->addData(seconds, direction.z);
                //SILK_INFO("compass: {}", data.value);
            }

            {
                auto data = m_comms->get_sensor_sonar_data();
                m_ui.sonar_plot->graph(0)->addData(seconds, data.value);
                m_average_sonar = math::lerp(m_average_sonar, data.value, 0.1f);
                m_ui.sonar_plot->graph(1)->addData(seconds, m_average_sonar);
            }
            {
                auto data = m_comms->get_sensor_barometer_data();
                m_ui.barometer_plot->graph(0)->addData(seconds, data.value);
                m_average_barometer = math::lerp(m_average_barometer, data.value, 0.01f);
                m_ui.barometer_plot->graph(1)->addData(seconds, m_average_barometer);
            }
            {
                auto data = m_comms->get_sensor_thermometer_data();
                m_ui.thermometer_plot->graph(0)->addData(seconds, data.value);
                m_average_thermometer = math::lerp(m_average_thermometer, data.value, 0.01f);
                m_ui.thermometer_plot->graph(1)->addData(seconds, m_average_thermometer);
            }

			//printf("\n%f, %f, %f", m_protocol->data_board_accelerometer.value.x, m_protocol->data_board_accelerometer.value.y, m_protocol->data_board_accelerometer.value.z);

			//////////////////////////////////////////////////////////////////////////

            if (seconds > graph_length_seconds)
            {
                m_ui.c_plot->graph(0)->removeDataBefore(seconds - graph_length_seconds);
                m_ui.c_plot->graph(1)->removeDataBefore(seconds - graph_length_seconds);
                m_ui.c_plot->graph(2)->removeDataBefore(seconds - graph_length_seconds);
                m_ui.barometer_plot->graph(0)->removeDataBefore(seconds - graph_length_seconds);
                m_ui.barometer_plot->graph(1)->removeDataBefore(seconds - graph_length_seconds);
                m_ui.sonar_plot->graph(0)->removeDataBefore(seconds - graph_length_seconds);
                m_ui.sonar_plot->graph(1)->removeDataBefore(seconds - graph_length_seconds);
                m_ui.thermometer_plot->graph(0)->removeDataBefore(seconds - graph_length_seconds);
                m_ui.thermometer_plot->graph(1)->removeDataBefore(seconds - graph_length_seconds);
            }


            m_ui.c_plot->rescaleAxes(true);
            m_ui.c_plot->replot();

			m_ui.barometer_plot->rescaleAxes(true);
			m_ui.barometer_plot->replot();

            m_ui.sonar_plot->rescaleAxes(true);
			m_ui.sonar_plot->replot();

            m_ui.thermometer_plot->rescaleAxes(true);
            m_ui.thermometer_plot->replot();
        }
	}
	else
	{
        m_last_uav_alive_duration = q::Clock::duration();
        m_gyroscope_sample_time = 0;
        m_accelerometer_sample_time = 0;

        m_ui.g_plot->graph(0)->clearData();
        m_ui.g_plot->graph(1)->clearData();
        m_ui.g_plot->graph(2)->clearData();
        m_ui.g_spectrum->graph(0)->clearData();
        m_ui.g_spectrum->graph(1)->clearData();
        m_ui.g_spectrum->graph(2)->clearData();
        m_ui.a_plot->graph(0)->clearData();
        m_ui.a_plot->graph(1)->clearData();
        m_ui.a_plot->graph(2)->clearData();
        m_ui.a_spectrum->graph(0)->clearData();
        m_ui.a_spectrum->graph(1)->clearData();
        m_ui.a_spectrum->graph(2)->clearData();
        m_ui.c_plot->graph(0)->clearData();
        m_ui.c_plot->graph(1)->clearData();
        m_ui.c_plot->graph(2)->clearData();
        m_ui.barometer_plot->graph(0)->clearData();
        m_ui.barometer_plot->graph(1)->clearData();
        m_ui.sonar_plot->graph(0)->clearData();
        m_ui.sonar_plot->graph(1)->clearData();
        m_ui.thermometer_plot->graph(0)->clearData();
        m_ui.thermometer_plot->graph(1)->clearData();
    }

    update_calibration();
}

void Sensors::process_gyroscope_fft(FFT_Data& fft)
{
    const std::chrono::milliseconds required_duration(1000);
    const size_t required_sample_count = math::min(
                static_cast<size_t>(fft.sample_rate * q::Seconds(required_duration).count()),
                fft.MAX_INPUT_SIZE);
    if (fft.input.size() < required_sample_count)
    {
        fft.output.resize(0);
        return;
    }

    size_t output_size = required_sample_count/2 + 1;
    fft.output.resize(output_size);

    auto temp_out = fft.temp_output.get();

    fft.plan = fftw_plan_dft_r2c_1d(required_sample_count, fft.temp_input.get(), fft.temp_output.get(), FFTW_ESTIMATE);

    std::transform(fft.input.begin(), fft.input.begin() + required_sample_count, fft.temp_input.get(), [](math::vec3f const& v) { return v.x; });
    fftw_execute(fft.plan);
    for (size_t i = 0; i < output_size; i++)
    {
        fft.output[i].x = (temp_out[i][0]*temp_out[i][0] + temp_out[i][1]*temp_out[i][1]) / (required_sample_count*required_sample_count);
    }

    std::transform(fft.input.begin(), fft.input.begin() + required_sample_count, fft.temp_input.get(), [](math::vec3f const& v) { return v.y; });
    fftw_execute(fft.plan);
    for (size_t i = 0; i < output_size; i++)
    {
        fft.output[i].y = (temp_out[i][0]*temp_out[i][0] + temp_out[i][1]*temp_out[i][1]) / (required_sample_count*required_sample_count);
    }

    std::transform(fft.input.begin(), fft.input.begin() + required_sample_count, fft.temp_input.get(), [](math::vec3f const& v) { return v.z; });
    fftw_execute(fft.plan);
    for (size_t i = 0; i < output_size; i++)
    {
        fft.output[i].z = (temp_out[i][0]*temp_out[i][0] + temp_out[i][1]*temp_out[i][1]) / (required_sample_count*required_sample_count);
    }

    //consume input
    fft.input.erase(fft.input.begin(), fft.input.begin() + required_sample_count);
}

void Sensors::process_accelerometer_fft(FFT_Data& fft)
{
    if (fft.input.size() < fft.sample_rate)
    {
        return;
    }


}


void Sensors::update_calibration()
{
    if (m_calibration.step == Calibration::Step::IDLE)
    {
        return;
    }

    switch(m_calibration.sensor)
    {
    case Calibration::Sensor::ACCELEROMETER: update_accelerometer_calibration(); break;
    case Calibration::Sensor::GYROSCOPE: update_gyroscope_calibration(); break;
    case Calibration::Sensor::COMPASS: update_compass_calibration(); break;
    }
}

void Sensors::start_accelerometer_calibration()
{
    m_calibration.sensor = Calibration::Sensor::ACCELEROMETER;
    m_calibration.step = Calibration::Step::START;
    m_calibration.step_timepoint = q::Clock::now();
    m_calibration.collect_data_step = 0;

    std::string msg;
    q::util::format(msg, "Accelerometer calibration will start now.\nMake sure you can rotate the UAV when instructed.");

    m_calibration.message_box.setWindowTitle("Accelerometer Calibration");
    m_calibration.message_box.setText(msg.c_str());
    m_calibration.message_box.setWindowModality(Qt::ApplicationModal);
    m_calibration.message_box.show();
}

void Sensors::update_accelerometer_calibration()
{
    std::chrono::seconds duration(5);

    auto now = q::Clock::now();

    char const* direction_str[] =
    {
        "level",
        "on its LEFT side",
        "on its RIGHT side",
        "nose DOWN",
        "nose UP",
        "on its BACK",
    };

    //q::util::format(msg, "Place the board {} for {} seconds.\nPress Ok to continue.", direction_str[step], duration.count());


    //first reset the bias
    if (m_calibration.step == Calibration::Step::START)
    {
        if (!m_calibration.message_box.isVisible())
        {
            m_calibration.connection = m_comms->accelerometer_calibration_data_received.connect([this](math::vec3f const& bias, math::vec3f const& scale)
            {
                m_calibration.received_bias = bias;
                m_calibration.received_scale = scale;
                m_calibration.is_received_data_valid = true;
            });

            m_calibration.step = Calibration::Step::RESET;
            m_calibration.step_timepoint = q::Clock::now();
            m_calibration.is_received_data_valid = false;

            m_comms->set_accelerometer_calibration_data(math::vec3f::zero, math::vec3f::one);
        }
    }
    else if (m_calibration.step == Calibration::Step::RESET)
    {
        if (now - m_calibration.step_timepoint > std::chrono::seconds(1))
        {
            QMessageBox::critical(this, "Error", "Cannot start the calibration procedure.\nFailed to reset the sensors.");
            m_calibration.step = Calibration::Step::IDLE;
            return;
        }
        if (m_calibration.is_received_data_valid &&
                math::is_zero(m_calibration.received_bias) &&
                math::is_one(m_calibration.received_scale))
        {
            m_calibration.step = Calibration::Step::SHOW_INSTRUCTIONS;
            m_calibration.step_timepoint = q::Clock::now();

            std::string msg;
            q::util::format(msg, "Place the board {} for {} seconds.\nPress Ok to continue.", direction_str[m_calibration.collect_data_step], duration.count());
            m_calibration.message_box.setStandardButtons(QMessageBox::Button::Ok);
            m_calibration.message_box.setText(msg.c_str());
            m_calibration.message_box.setWindowModality(Qt::NonModal);
            m_calibration.message_box.show();
        }
    }
    else if (m_calibration.step == Calibration::Step::SHOW_INSTRUCTIONS)
    {
        if (!m_calibration.message_box.isVisible())
        {
            m_calibration.step = Calibration::Step::COLLECT_DATA;
            m_calibration.step_timepoint = q::Clock::now();
            m_calibration.samples.clear();
            m_calibration.message_box.setStandardButtons(0);
            m_calibration.message_box.show();
        }
    }
    else if (m_calibration.step == Calibration::Step::COLLECT_DATA)
    {
        auto& samples = m_calibration.samples;

        auto time_passed = now - m_calibration.step_timepoint;
        if (time_passed < duration)
        {
            auto data = m_comms->get_sensor_accelerometer_data();
            if (data.timestamp > m_calibration.last_data_timepoint && time_passed > std::chrono::seconds(2))
            {
                m_calibration.last_data_timepoint = data.timestamp;
                std::copy(data.value.accelerations.begin(), data.value.accelerations.end(), std::back_inserter(samples));
            }

            auto left = std::chrono::duration_cast<std::chrono::seconds>(duration - time_passed).count();
            left = std::max<int>(left, 0);

            std::string msg;
            q::util::format(msg, "Collected {} sample{}...\n{} second{} left.",
                            samples.size(),
                            samples.size() != 1 ? "s" : "",
                            left,
                            left != 1 ? "s" : "");

            m_calibration.message_box.setText(msg.c_str());
        }
        else
        {
            m_calibration.message_box.hide();

            //make sure we have enough data
            if (samples.size() < 60)
            {
                QMessageBox::critical(this, "Error", "Calibration failed - not enough samples collected!");
                m_calibration.step = Calibration::Step::IDLE;
            }
            else
            {
                m_calibration.averages[m_calibration.collect_data_step].set(std::accumulate(samples.begin(), samples.end(), math::vec3f()));
                m_calibration.averages[m_calibration.collect_data_step] /= static_cast<double>(samples.size());

                m_calibration.collect_data_step++;
                if (m_calibration.collect_data_step == 6)
                {
                    math::vec3<double> bias, scale;
                    calibrate_accelerometer(m_calibration.averages, bias, scale);

                    m_calibration.new_bias = math::vec3f(bias);
                    m_calibration.new_scale = math::vec3f(scale);
                    m_calibration.is_received_data_valid = false;
                    m_comms->set_accelerometer_calibration_data(math::vec3f(bias), math::vec3f(scale));

                    m_calibration.step = Calibration::Step::SET;
                    m_calibration.step_timepoint = q::Clock::now();
                }
                else
                {
                    m_calibration.step = Calibration::Step::SHOW_INSTRUCTIONS;
                    m_calibration.step_timepoint = q::Clock::now();

                    std::string msg;
                    q::util::format(msg, "Place the board {} for {} seconds.\nPress Ok to continue.", direction_str[m_calibration.collect_data_step], duration.count());
                    m_calibration.message_box.setStandardButtons(QMessageBox::Button::Ok);
                    m_calibration.message_box.setText(msg.c_str());
                    m_calibration.message_box.show();
                }
            }
        }
    }
    else if (m_calibration.step == Calibration::Step::SET)
    {
        if (now - m_calibration.step_timepoint > std::chrono::seconds(1))
        {
            QMessageBox::critical(this, "Error", "Failed to set new calibration data.\nRepeat the procedure please.");
            m_calibration.step = Calibration::Step::IDLE;
            m_calibration.connection.disconnect();
            return;
        }

        if (m_calibration.is_received_data_valid &&
                math::equals(m_calibration.received_bias, m_calibration.new_bias) &&
                math::equals(m_calibration.received_scale, m_calibration.new_scale))
        {
            m_calibration.step = Calibration::Step::IDLE;

            std::string msg;
            q::util::format(msg, "New accelerometer bias / scale: {.9} / {.9}.", m_calibration.received_bias, m_calibration.received_scale);
            m_calibration.message_box.setText(msg.c_str());
            m_calibration.message_box.setStandardButtons(QMessageBox::Button::Ok);
            m_calibration.message_box.exec();
            m_calibration.connection.disconnect();
            return;
        }
    }
}

void Sensors::start_gyroscope_calibration()
{
    m_calibration.sensor = Calibration::Sensor::GYROSCOPE;
    m_calibration.step = Calibration::Step::START;
    m_calibration.step_timepoint = q::Clock::now();

    std::string msg;
    q::util::format(msg, "Place the board level and completely still");

    m_calibration.message_box.setWindowTitle("Gyroscope Calibration");
    m_calibration.message_box.setText(msg.c_str());
    m_calibration.message_box.setWindowModality(Qt::ApplicationModal);
    m_calibration.message_box.show();
}

void Sensors::update_gyroscope_calibration()
{
    std::chrono::seconds duration(15);

    auto now = q::Clock::now();

	//first reset the bias
    if (m_calibration.step == Calibration::Step::START)
    {
        if (!m_calibration.message_box.isVisible())
        {
            m_calibration.connection = m_comms->gyroscope_calibration_data_received.connect([this](math::vec3f const& bias)
            {
                m_calibration.received_bias = bias;
                m_calibration.is_received_data_valid = true;
            });

            m_calibration.is_received_data_valid = false;
            m_calibration.step = Calibration::Step::RESET;
            m_calibration.step_timepoint = q::Clock::now();

            m_comms->set_gyroscope_calibration_data(math::vec3f::zero);
        }
    }
    else if (m_calibration.step == Calibration::Step::RESET)
	{
        if (now - m_calibration.step_timepoint > std::chrono::seconds(1))
        {
            QMessageBox::critical(this, "Error", "Cannot start the calibration procedure.\nFailed to reset the sensors.");
            m_calibration.step = Calibration::Step::IDLE;
            return;
        }

        if (m_calibration.is_received_data_valid &&
                math::is_zero(m_calibration.received_bias))
        {
            m_calibration.step = Calibration::Step::COLLECT_DATA;
            m_calibration.step_timepoint = q::Clock::now();
            m_calibration.samples.clear();

            m_calibration.message_box.setStandardButtons(0);
            m_calibration.message_box.setText(QString());
            m_calibration.message_box.setWindowModality(Qt::NonModal);
            m_calibration.message_box.show();
            return;
        }
	}
    else if (m_calibration.step == Calibration::Step::COLLECT_DATA)
    {
        auto& samples = m_calibration.samples;

        auto time_passed = now - m_calibration.step_timepoint;
        if (time_passed < duration)
        {
            auto data = m_comms->get_sensor_gyroscope_data();
            if (data.timestamp > m_calibration.last_data_timepoint && time_passed > std::chrono::seconds(2))
            {
                m_calibration.last_data_timepoint = data.timestamp;
                std::copy(data.value.angular_velocities.begin(), data.value.angular_velocities.end(), std::back_inserter(samples));
            }

            auto left = std::chrono::duration_cast<std::chrono::seconds>(duration - time_passed).count();
            left = std::max<int>(left, 0);

            auto bias = std::accumulate(samples.begin(), samples.end(), math::vec3f::zero);
            if (!samples.empty())
            {
                bias /= static_cast<float>(samples.size());
            }

            std::string msg;
            q::util::format(msg, "Collected {} sample{}...\n{} second{} left.\nApproximated bias: {.9}",
                            samples.size(),
                            samples.size() != 1 ? "s" : "",
                            left,
                            left != 1 ? "s" : "",
                            bias);

            m_calibration.message_box.setText(msg.c_str());
        }
        else
        {
            m_calibration.message_box.hide();
            //make sure we have enough data
            if (samples.size() < 100)
            {
                QMessageBox::critical(this, "Error", "Calibration failed - not enough samples collected!");
                m_calibration.step = Calibration::Step::IDLE;
            }
            else
            {
                auto bias = std::accumulate(samples.begin(), samples.end(), math::vec3f::zero);
                bias /= static_cast<float>(samples.size());

                m_calibration.is_received_data_valid = false;
                m_calibration.new_bias = bias;
                m_calibration.step = Calibration::Step::SET;
                m_calibration.step_timepoint = q::Clock::now();

                m_comms->set_gyroscope_calibration_data(bias);
            }
        }
    }
    else if (m_calibration.step == Calibration::Step::SET)
    {
        if (now - m_calibration.step_timepoint > std::chrono::seconds(1))
        {
            QMessageBox::critical(this, "Error", "Failed to set new calibration data.\nRepeat the procedure please.");
            m_calibration.step = Calibration::Step::IDLE;
            m_calibration.connection.disconnect();
            return;
        }

        if (m_calibration.is_received_data_valid &&
                math::equals(m_calibration.received_bias, m_calibration.new_bias))
        {
            m_calibration.step = Calibration::Step::IDLE;

            std::string msg;
            q::util::format(msg, "New gyroscope bias: {.9}.", m_calibration.received_bias);
            m_calibration.message_box.setText(msg.c_str());
            m_calibration.message_box.setStandardButtons(QMessageBox::Button::Ok);
            m_calibration.message_box.exec();
            m_calibration.connection.disconnect();
            return;
        }
    }
}

void Sensors::start_compass_calibration()
{
    m_calibration.sensor = Calibration::Sensor::COMPASS;
    m_calibration.step = Calibration::Step::START;
    m_calibration.step_timepoint = q::Clock::now();

    std::string msg;
    q::util::format(msg, "Rotate the board along all axes.");

    m_calibration.message_box.setWindowTitle("Compass Calibration");
    m_calibration.message_box.setText(msg.c_str());
    m_calibration.message_box.setWindowModality(Qt::ApplicationModal);
    m_calibration.message_box.show();
}

void Sensors::update_compass_calibration()
{
    std::chrono::seconds duration(60);

    auto now = q::Clock::now();

    //first reset the bias
    if (m_calibration.step == Calibration::Step::START)
    {
        if (!m_calibration.message_box.isVisible())
        {
            m_calibration.connection = m_comms->compass_calibration_data_received.connect([this](math::vec3f const& bias)
            {
                m_calibration.received_bias = bias;
                m_calibration.is_received_data_valid = true;
            });

            m_calibration.is_received_data_valid = false;
            m_calibration.step = Calibration::Step::RESET;
            m_calibration.step_timepoint = q::Clock::now();

            m_comms->set_compass_calibration_data(math::vec3f::zero);
        }
    }
    else if (m_calibration.step == Calibration::Step::RESET)
    {
        if (now - m_calibration.step_timepoint > std::chrono::seconds(1))
        {
            QMessageBox::critical(this, "Error", "Cannot start the calibration procedure.\nFailed to reset the sensors.");
            m_calibration.step = Calibration::Step::IDLE;
            return;
        }

        if (m_calibration.is_received_data_valid &&
                math::is_zero(m_calibration.received_bias))
        {
            m_calibration.step = Calibration::Step::COLLECT_DATA;
            m_calibration.step_timepoint = q::Clock::now();
            m_calibration.samples.clear();

            m_calibration.message_box.setStandardButtons(0);
            m_calibration.message_box.setText(QString());
            m_calibration.message_box.setWindowModality(Qt::NonModal);
            m_calibration.message_box.show();
            return;
        }
    }
    else if (m_calibration.step == Calibration::Step::COLLECT_DATA)
    {
        auto& samples = m_calibration.samples;

        auto time_passed = now - m_calibration.step_timepoint;
        if (time_passed < duration)
        {
            auto data = m_comms->get_sensor_compass_data();
            if (data.timestamp > m_calibration.last_data_timepoint && time_passed > std::chrono::seconds(2))
            {
                m_calibration.last_data_timepoint = data.timestamp;
                samples.push_back(data.value);
            }

            auto left = std::chrono::duration_cast<std::chrono::seconds>(duration - time_passed).count();
            left = std::max<int>(left, 0);

            math::aabb3f box(samples.empty() ? math::vec3f::zero : samples[0]);
            for (auto const& s: samples)
            {
                box.add_internal_point(s);
            }

            auto bias = box.get_center();

            std::string msg;
            q::util::format(msg, "Collected {} sample{}...\n{} second{} left.\nApproximated bias: {.9}",
                            samples.size(),
                            samples.size() != 1 ? "s" : "",
                            left,
                            left != 1 ? "s" : "",
                            bias);

            m_calibration.message_box.setText(msg.c_str());
        }
        else
        {
            m_calibration.message_box.hide();
            //make sure we have enough data
            if (samples.size() < 100)
            {
                QMessageBox::critical(this, "Error", "Calibration failed - not enough samples collected!");
                m_calibration.step = Calibration::Step::IDLE;
            }
            else
            {
                math::aabb3f box(samples.empty() ? math::vec3f::zero : samples[0]);
                for (auto const& s: samples)
                {
                    box.add_internal_point(s);
                }

                auto bias = box.get_center();

                m_calibration.new_bias = bias;
                m_calibration.step = Calibration::Step::SET;
                m_calibration.step_timepoint = q::Clock::now();
                m_calibration.is_received_data_valid = false;

                m_comms->set_compass_calibration_data(bias);
            }
        }
    }
    else if (m_calibration.step == Calibration::Step::SET)
    {
        if (now - m_calibration.step_timepoint > std::chrono::seconds(1))
        {
            QMessageBox::critical(this, "Error", "Failed to set new calibration data.\nRepeat the procedure please.");
            m_calibration.step = Calibration::Step::IDLE;
            m_calibration.connection.disconnect();
            return;
        }

        if (m_calibration.is_received_data_valid &&
                math::equals(m_calibration.received_bias, m_calibration.new_bias))
        {
            m_calibration.step = Calibration::Step::IDLE;

            std::string msg;
            q::util::format(msg, "New compass bias: {.9}.", m_calibration.received_bias);
            m_calibration.message_box.setText(msg.c_str());
            m_calibration.message_box.setStandardButtons(QMessageBox::Button::Ok);
            m_calibration.message_box.exec();
            m_calibration.connection.disconnect();
            return;
        }
    }
}

bool Sensors::calibrate_accelerometer(std::array<math::vec3<double>, 6> const& samples, math::vec3<double>& bias, math::vec3<double>& scale)
{
	//code copied from arducopter
	int iteration_count = 0;
	static const double s_eps = 0.000000001;
	static const int s_iterations = 2000;
	double change = 100.0;
	double data[3];
	double beta[6];
	double delta[6];
	double ds[6];
	double JS[6][6];
	bool success = true;

	// reset
	beta[0] = beta[1] = beta[2] = 0;
	beta[3] = beta[4] = beta[5] = 1.0f / physics::constants::g;

	while (iteration_count < s_iterations && change > s_eps)
	{
		iteration_count++;

		calibrate_reset_matrices(ds, JS);

		for (int i = 0; i < 6; i++)
		{
			data[0] = samples[i].x;
			data[1] = samples[i].y;
			data[2] = samples[i].z;
			calibrate_update_matrices(ds, JS, beta, data);
		}

		calibrate_find_delta(ds, JS, delta);

		change = delta[0] * delta[0] +
			delta[0] * delta[0] +
			delta[1] * delta[1] +
			delta[2] * delta[2] +
			delta[3] * delta[3] / (beta[3] * beta[3]) +
			delta[4] * delta[4] / (beta[4] * beta[4]) +
			delta[5] * delta[5] / (beta[5] * beta[5]);

		for (int i = 0; i < 6; i++)
		{
			beta[i] -= delta[i];
		}
	}

	// copy results out
	scale.x = beta[3] * physics::constants::g;
	scale.y = beta[4] * physics::constants::g;
	scale.z = beta[5] * physics::constants::g;
	bias.x = beta[0] * scale.x;
	bias.y = beta[1] * scale.y;
	bias.z = beta[2] * scale.z;

	// sanity check scale
// 	if (scale.is_nan() || fabsf(scale.x - 1.0f) > 0.1f || fabsf(scale.y - 1.0f) > 0.1f || fabsf(scale.z - 1.0f) > 0.1f) 
// 	{
// 		success = false;
// 	}
// 	// sanity check offsets (3.5 is roughly 3/10th of a G, 5.0 is roughly half a G)
// 	if (bias.is_nan() || fabsf(bias.x) > 3.5f || fabsf(bias.y) > 3.5f || fabsf(bias.z) > 3.5f) 
// 	{
// 		success = false;
// 	}

	// return success or failure
	return success;
}

void Sensors::calibrate_update_matrices(double dS[6], double JS[6][6], double beta[6], double data[3])
{
	//code copied from arducopter
	double residual = 1.0;
	double jacobian[6];

	for (int j = 0; j < 3; j++)
	{
		double b = beta[3 + j];
		double dx = data[j] - beta[j];
		residual -= b*b*dx*dx;
		jacobian[j] = 2.0f*b*b*dx;
		jacobian[3 + j] = -2.0f*b*dx*dx;
	}

	for (int j = 0; j < 6; j++)
	{
		dS[j] += jacobian[j] * residual;
		for (int k = 0; k < 6; k++)
		{
			JS[j][k] += jacobian[j] * jacobian[k];
		}
	}
}


// _calibrate_reset_matrices - clears matrices
void Sensors::calibrate_reset_matrices(double dS[6], double JS[6][6])
{
	//code copied from arducopter
	for (int j = 0; j < 6; j++)
	{
		dS[j] = 0.0f;
		for (int k = 0; k < 6; k++)
		{
			JS[j][k] = 0.0f;
		}
	}
}

void Sensors::calibrate_find_delta(double dS[6], double JS[6][6], double delta[6])
{
	//code copied from arducopter
	//Solve 6-d matrix equation JS*x = dS
	//first put in upper triangular form

	//make upper triangular
	for (int i = 0; i < 6; i++)
	{
		//eliminate all nonzero entries below JS[i][i]
		for (int j = i + 1; j < 6; j++)
		{
			double mu = JS[i][j] / JS[i][i];
			if (mu != 0.0) 
			{
				dS[j] -= mu*dS[i];
				for (int k = j; k < 6; k++)
				{
					JS[k][j] -= mu*JS[k][i];
				}
			}
		}
	}

	//back-substitute
	for (int i = 5; i >= 0; i--)
	{
		dS[i] /= JS[i][i];
		JS[i][i] = 1.0;

		for (int j = 0; j < i; j++)
		{
			double mu = JS[i][j];
			dS[j] -= mu*dS[i];
			JS[i][j] = 0.0;
		}
	}

	for (int i = 0; i < 6; i++)
	{
		delta[i] = dS[i];
	}
}

