#include "FCStdAfx.h"
#include "Multirotor_Simulation.h"
#include "physics/constants.h"

#include "btBulletCollisionCommon.h"
#include "btBulletDynamicsCommon.h"

static math::vec3f bt_to_vec3f(btVector3 const& v)
{
    return math::vec3f(v.x(), v.y(), v.z());
}
static math::quatf bt_to_quatf(btQuaternion const& v)
{
    return math::quatf(v.x(), v.y(), v.z(), v.w());
}
static btVector3 vec3f_to_bt(math::vec3f const& v)
{
    return btVector3(v.x, v.y, v.z);
}
static btQuaternion quatf_to_bt(math::quatf const& v)
{
    return btQuaternion(v.x, v.y, v.z, v.w);
}

namespace silk
{
namespace node
{

static constexpr uint32_t MAX_PHYSICS_RATE = 200; //hz
static constexpr Clock::duration MAX_PHYSICS_DT = std::chrono::microseconds(1000000 / MAX_PHYSICS_RATE);


//////////////////////////////////////////////////////////////////////////

Multirotor_Simulation::Multirotor_Simulation()
{
}

Multirotor_Simulation::~Multirotor_Simulation()
{
}

bool Multirotor_Simulation::init(uint32_t rate)
{
    if (rate == 0)
    {
        QLOGE("Bad rate: {}Hz", rate);
        return false;
    }
    m_dt = std::chrono::microseconds(1000000 / rate);

    if (rate <= MAX_PHYSICS_RATE)
    {
        m_physics_rate = rate;
    }
    else
    {
        uint32_t div = rate / MAX_PHYSICS_RATE + 1;
        m_physics_rate = rate / div;
    }
    m_physics_dt = std::chrono::microseconds(1000000 / m_physics_rate);

    ///collision configuration contains default setup for memory, collision setup
    m_collision_configuration.reset(new btDefaultCollisionConfiguration());
    ///use the default collision dispatcher. For parallel processing you can use a diffent dispatcher (see Extras/BulletMultiThreaded)
    m_dispatcher.reset(new btCollisionDispatcher(m_collision_configuration.get()));

    m_broadphase.reset(new btDbvtBroadphase());

    ///the default constraint solver. For parallel processing you can use a different solver (see Extras/BulletMultiThreaded)
    m_solver.reset(new btSequentialImpulseConstraintSolver);
    m_world.reset(new btDiscreteDynamicsWorld(m_dispatcher.get(), m_broadphase.get(), m_solver.get(), m_collision_configuration.get()));

    m_world->setGravity(btVector3(0, 0, -physics::constants::g));

    //////////////////////////////////////////////////////////////////////////
    //ground

    m_ground_shape.reset(new btStaticPlaneShape(btVector3(0, 0, 1), 0));

    btTransform transform;
    transform.setIdentity();
    transform.setOrigin(btVector3(0, 0, 0));

    btRigidBody::btRigidBodyConstructionInfo info(0.f, nullptr, m_ground_shape.get());
    m_ground_body.reset(new btRigidBody(info));
    m_world->addRigidBody(m_ground_body.get());

    return true;
}

bool Multirotor_Simulation::init_uav(std::shared_ptr<const IMultirotor_Properties> multirotor_properties)
{
    m_uav.properties = multirotor_properties;
    m_uav.state.radius = multirotor_properties->get_radius();

    std::random_device rd;  //Will be used to obtain a seed for the random number engine
    std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
    gen.seed(1);
    std::uniform_real_distribution<float> motor_thrust_factor_distribution(0.8f, 1.2f);
    std::uniform_real_distribution<float> motor_drag_factor_distribution(0.001f, 0.002f);

    if (m_uav.state.motors.size() != m_uav.properties->get_motors().size())
    {
        m_uav.state.motors.clear();
        m_uav.state.motors.resize(m_uav.properties->get_motors().size());
        m_uav.motor_drag_factors.resize(m_uav.properties->get_motors().size());
        m_uav.motor_thrust_factors.resize(m_uav.properties->get_motors().size());
        for (size_t i = 0; i < m_uav.state.motors.size(); i++)
        {
            m_uav.motor_drag_factors[i] = motor_drag_factor_distribution(gen);
            m_uav.motor_thrust_factors[i] = motor_thrust_factor_distribution(gen);
            m_uav.state.motors[i].max_thrust = multirotor_properties->get_motor_thrust();
            m_uav.state.motors[i].position = multirotor_properties->get_motors()[i].position;
        }
    }

    if (m_uav.body)
    {
        m_world->removeRigidBody(m_uav.body.get());
    }
    m_uav.body.reset();
    m_uav.motion_state.reset();
    m_uav.shape.reset();

    m_uav.shape.reset(new btCylinderShapeZ(btVector3(m_uav.properties->get_radius(), m_uav.properties->get_radius(), m_uav.properties->get_height()*0.5f)));

    if (m_is_ground_enabled)
    {
        math::vec3f enu_position = m_uav.state.enu_position;
        enu_position.z = math::max(enu_position.z, m_uav.properties->get_height()*0.5f);
        m_uav.state.enu_position = enu_position;
    }

    btTransform transform;
    transform.setIdentity();
    transform.setOrigin(vec3f_to_bt(m_uav.state.enu_position));

    transform.setRotation(quatf_to_bt(m_uav.state.local_to_enu_rotation));

    btVector3 local_inertia(0, 0, 0);
    m_uav.shape->calculateLocalInertia(m_uav.properties->get_mass(), local_inertia);

    //using motionstate is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects
    m_uav.motion_state.reset(new btDefaultMotionState(transform));
    btRigidBody::btRigidBodyConstructionInfo rbInfo(m_uav.properties->get_mass(), m_uav.motion_state.get(), m_uav.shape.get(), local_inertia);
    m_uav.body.reset(new btRigidBody(rbInfo));
    m_uav.body->setActivationState(DISABLE_DEACTIVATION);
    m_uav.body->setDamping(0.01f, 0.05f); //simulate air resistance

    m_world->addRigidBody(m_uav.body.get());

    m_uav.body->setLinearVelocity(vec3f_to_bt(m_uav.state.enu_velocity));
    m_uav.body->setAngularVelocity(vec3f_to_bt(m_uav.state.angular_velocity));
    m_uav.body->setGravity(btVector3(0, 0, m_is_gravity_enabled ? -physics::constants::g : 0));

    return true;
}
void Multirotor_Simulation::reset()
{
    btTransform trans;
    trans.setIdentity();
    trans.setOrigin(btVector3(0, 0, m_uav.properties->get_height()/2.f));
    m_uav.body->setWorldTransform(trans);
    m_uav.body->setAngularVelocity(btVector3(0, 0, 0));
    m_uav.body->setLinearVelocity(btVector3(0, 0, 0));
    m_uav.body->clearForces();

    m_uav.state.acceleration = math::vec3f::zero;
    m_uav.state.angular_velocity = math::vec3f::zero;
    m_uav.state.enu_linear_acceleration = math::vec3f::zero;
    m_uav.state.enu_position = math::vec3f::zero;
    m_uav.state.enu_velocity = math::vec3f::zero;
    m_uav.state.local_to_enu_rotation = math::quatf::identity;
    for (State::Motor_State& m: m_uav.state.motors)
    {
        m.throttle = 0;
        m.thrust = 0;
    }
}

void Multirotor_Simulation::stop_motion()
{
    m_uav.body->clearForces();
    m_uav.body->setLinearVelocity(btVector3(0, 0, 0));
    m_uav.body->setAngularVelocity(btVector3(0, 0, 0));

    btTransform wt;
    m_uav.motion_state->getWorldTransform(wt);

    {
        math::quatf rotation = bt_to_quatf(wt.getRotation());
        m_uav.state.local_to_enu_rotation = rotation;
        m_uav.state.angular_velocity = math::vec3f::zero;
    }

    {
        math::vec3f new_position = bt_to_vec3f(wt.getOrigin());
        m_uav.state.enu_position = new_position;
        m_uav.state.enu_linear_acceleration = math::vec3f::zero;
        m_uav.state.enu_velocity = math::vec3f::zero;
        math::quatf enu_to_local = math::inverse(m_uav.state.local_to_enu_rotation);
        m_uav.state.acceleration = math::rotate(enu_to_local, m_uav.state.enu_linear_acceleration + math::vec3f(0, 0, physics::constants::g));
    }

}

void Multirotor_Simulation::process(Clock::duration dt, std::function<void(Multirotor_Simulation&, Clock::duration)> const& callback)
{
    m_accumulated_duration += dt;
    m_accumulated_physics_duration += dt;
    //m_duration_to_simulate = math::min(m_duration_to_simulate, Clock::duration(std::chrono::milliseconds(100)));

    while (m_accumulated_duration >= m_dt)
    {
        //limit angular velocity
        if (m_uav.body)
        {
            math::vec3f vel = bt_to_vec3f(m_uav.body->getAngularVelocity());
            vel = math::clamp(vel, math::vec3f(-10.f), math::vec3f(10.f));// + math::vec3f(0.001f, 0.f, 0.f);
            m_uav.body->setAngularVelocity(vec3f_to_bt(vel));
        }

        if (m_is_simulation_enabled)
        {
            float physics_dts = std::chrono::duration<float>(m_physics_dt).count();
            while (m_accumulated_physics_duration >= m_physics_dt)
            {
                m_world->stepSimulation(physics_dts, 0, physics_dts);
                process_uav(m_physics_dt);
                m_accumulated_physics_duration -= m_physics_dt;
            }
        }

        process_uav_sensors(m_dt);

        if (callback)
        {
            callback(*this, m_dt);
        }

        //update_world(std::chrono::microseconds(50));
        m_accumulated_duration -= m_dt;
    }
}

void Multirotor_Simulation::set_gravity_enabled(bool yes)
{
    if (m_is_gravity_enabled != yes)
    {
        m_is_gravity_enabled = yes;
        m_uav.body->setGravity(btVector3(0, 0, m_is_gravity_enabled ? -physics::constants::g : 0));
    }
}

void Multirotor_Simulation::set_ground_enabled(bool yes)
{
    if (m_is_ground_enabled != yes)
    {
        m_is_ground_enabled = yes;
        if (m_is_ground_enabled)
        {
            m_world->addRigidBody(m_ground_body.get());
        }
        else
        {
            m_world->removeRigidBody(m_ground_body.get());
        }
    }
}

void Multirotor_Simulation::set_simulation_enabled(bool yes)
{
    m_is_simulation_enabled = yes;
}
void Multirotor_Simulation::set_drag_enabled(bool yes)
{
    m_is_drag_enabled = yes;
}

void Multirotor_Simulation::set_motor_throttle(size_t motor, float throttle)
{
    m_uav.state.motors[motor].throttle = throttle;
}


void Multirotor_Simulation::process_uav(Clock::duration dt)
{
    if (!m_uav.body)
    {
        return;
    }

    float dts = std::chrono::duration<float>(dt).count();

    btTransform wt;
    m_uav.motion_state->getWorldTransform(wt);

    math::trans3df local_to_enu_trans(bt_to_vec3f(wt.getOrigin()), bt_to_quatf(wt.getRotation()), math::vec3f::one);

    {
        math::quatf rotation = bt_to_quatf(wt.getRotation());
        QASSERT(math::is_finite(rotation));
        math::quatf delta = (~m_uav.state.local_to_enu_rotation) * rotation;
        QASSERT(math::is_finite(delta));
        m_uav.state.local_to_enu_rotation = rotation;

        math::vec3f euler;
        delta.get_as_euler_zxy(euler);
        QASSERT(math::is_finite(euler));
        m_sensor_data.angular_velocity = euler / dts;
        QASSERT(math::is_finite(m_sensor_data.angular_velocity));
//        float angle = 0.f;
//        math::vec3f axis;
//        delta.get_as_angle_axis(angle, axis);
//        QASSERT(math::is_finite(axis) && math::is_finite(angle));

//        m_accumulated_data.angular_velocity_sum += (axis * angle) / dts;
//        QASSERT(math::is_finite(m_accumulated_data.angular_velocity_sum));
    }

    math::vec3f velocity = bt_to_vec3f(m_uav.body->getLinearVelocity());
    {
        math::vec3f new_position = bt_to_vec3f(wt.getOrigin());

        m_uav.state.enu_position = new_position;
        m_sensor_data.enu_linear_acceleration = (velocity - m_sensor_data.prev_enu_velocity) / dts;
        m_sensor_data.prev_enu_velocity = velocity;

        m_sensor_data.enu_velocity = velocity;

        //m_uav.state.acceleration = math::rotate(enu_to_local_rotation, m_uav.state.enu_linear_acceleration + math::vec3f(0, 0, physics::constants::g));
        //QLOGI("v: {.4} / la:{.4} / a: {.4}", m_uav.state.enu_velocity, m_uav.state.enu_linear_acceleration, m_uav.state.acceleration);
    }

    //motors
    math::vec3f z_torque;
    float total_force = 0.f;
    {
        for (size_t i = 0; i < m_uav.state.motors.size(); i++)
        {
            State::Motor_State& m = m_uav.state.motors[i];
            IMultirotor_Properties::Motor const& mc = m_uav.properties->get_motors()[i];

            const float motor_thrust = m_uav.properties->get_motor_thrust() * m_uav.motor_thrust_factors[i];
            float thrust = m.thrust;
            {
                float target_thrust = math::square(m.throttle) * motor_thrust;
                if (!math::equals(thrust, target_thrust))
                {
                    float delta = target_thrust - thrust;
                    float acc = delta > 0 ? m_uav.properties->get_motor_acceleration() : m_uav.properties->get_motor_deceleration();
                    float d = math::min(math::abs(delta), acc / std::chrono::duration<float>(dt).count());
                    thrust += math::sgn(delta) * d;
                    thrust = math::clamp(thrust, 0.f, motor_thrust);
                    m.thrust = thrust;
                }
            }
            z_torque += mc.thrust_vector * m_uav.properties->get_motor_z_torque() * (mc.clockwise ? 1 : -1) * (thrust / motor_thrust);

//            total_rpm += m.rpm * (mc.clockwise ? 1.f : -1.f);

            total_force += thrust;
            math::vec3f pos = math::rotate(local_to_enu_trans, mc.position);

            const math::vec3f dir = math::rotate(local_to_enu_trans, mc.thrust_vector);
            m_uav.body->applyForce(vec3f_to_bt(dir * thrust), vec3f_to_bt(pos));
            //m_uav.body->applyForce(btVector3(0, 0, 10), wt.getOrigin());
        }
    }

    //SILK_INFO("total_force {}", total_force);

    //yaw
    {
        math::vec3f torque = math::rotate(local_to_enu_trans, z_torque);
        m_uav.body->applyTorque(vec3f_to_bt(torque));
    }

    //air drag
    {
        float speed_sq = math::length_sq(velocity);
        if (speed_sq > std::numeric_limits<float>::epsilon())
        {
            math::vec3f velocity_normalized = math::normalized(velocity);

            //body
            {
                float drag_factor = 0.05f;

                float intensity = math::abs(math::dot(local_to_enu_trans.get_axis_z(), velocity_normalized));
                float drag = intensity * drag_factor;
                math::vec3f force = -velocity_normalized * drag * speed_sq;
                m_uav.body->applyForce(vec3f_to_bt(force), btVector3(0, 0, 0));
            }

            //motors
            if (m_is_drag_enabled)
            {
                for (size_t i = 0; i < m_uav.state.motors.size(); i++)
                {
                    State::Motor_State& m = m_uav.state.motors[i];
                    IMultirotor_Properties::Motor const& mc = m_uav.properties->get_motors()[i];

                    float intensity = -math::dot(local_to_enu_trans.get_axis_z(), velocity_normalized);
                    float drag = intensity * m_uav.motor_drag_factors[i];
                    math::vec3f force = local_to_enu_trans.get_axis_z() * drag * speed_sq;

                    math::vec3f pos = math::rotate(local_to_enu_trans, mc.position);
                    m_uav.body->applyForce(vec3f_to_bt(force), vec3f_to_bt(pos));
                }
            }
        }
    }
}

void Multirotor_Simulation::process_uav_sensors(Clock::duration dt)
{
    if (!m_uav.body)
    {
        return;
    }

    m_uav.state.enu_velocity = m_sensor_data.enu_velocity;
    m_uav.state.enu_linear_acceleration = m_sensor_data.enu_linear_acceleration;
    m_uav.state.angular_velocity = m_sensor_data.angular_velocity;

    btTransform wt;
    m_uav.motion_state->getWorldTransform(wt);

    math::trans3df local_to_enu_trans(bt_to_vec3f(wt.getOrigin()), bt_to_quatf(wt.getRotation()), math::vec3f::one);
    math::trans3df enu_to_local_trans = math::inverse(local_to_enu_trans);

    math::quatf enu_to_local_rotation = math::inverse(m_uav.state.local_to_enu_rotation);

    m_uav.state.acceleration = math::rotate(enu_to_local_rotation, m_uav.state.enu_linear_acceleration + math::vec3f(0, 0, physics::constants::g));

    {
        math::vec3f enu_magnetic_field(0, 50.f, 0); //50 uT
        m_uav.state.magnetic_field = math::rotate(enu_to_local_rotation, enu_magnetic_field);
    }

    {
        //https://en.wikipedia.org/wiki/Atmospheric_pressure
        double h = m_uav.state.enu_position.z; //height
        double p0 = 101325.0; //sea level standard atmospheric pressure
        double M = 0.0289644; //molar mass of dry air
        double R = 8.31447; //universal gas constant
        double T0 = 288.15; //sea level standard temperature (K)
        m_uav.state.pressure = (p0 * std::exp(-(physics::constants::g * M * h) / (R * T0))) * 0.001; //kilopascals
    }

    {
        float h = m_uav.state.enu_position.z; //height
        m_uav.state.temperature = 20.f + h / 1000.f;
    }

    {
        math::planef ground(math::vec3f::zero, math::vec3f(0, 0, 1));
        math::vec3f start_point = m_uav.state.enu_position;
        math::vec3f ray_dir = math::rotate(m_uav.state.local_to_enu_rotation, math::vec3f(0, 0, -1));
        float t = 0;
        if (math::ray_intersect_plane(start_point, ray_dir, ground, t) && t > 0.f)
        {
            math::vec3f point = start_point + ray_dir * t;
            m_uav.state.proximity_distance = math::transform(enu_to_local_trans, point);
        }
        else
        {
            m_uav.state.proximity_distance = math::vec3f::zero;
        }
    }
}


Multirotor_Simulation::State const& Multirotor_Simulation::get_state() const
{
    return m_uav.state;
}


}
}
