#include "mock_slaves.hpp"

#include <cassert>
#include <cmath>
#include "dsb/compat_helpers.hpp"


namespace
{

class Mass1D : public SlaveInstance
{
public:
    Mass1D() : m_mass(1.0), m_pos_x(0.0), m_vel_x(0.0), m_force_x(0.0) { }

    double GetVariable(int varRef) override
    {
        switch (varRef) {
            case 0: return m_force_x;   break;
            case 1: return m_pos_x;     break;
            case 2: return m_vel_x;     break;
            case 3: return m_mass;      break;
            default:
                assert (!"Mass1D::GetVariable(): Invalid variable reference");
        }
        return 0.0;
    }

    void SetVariable(int varRef, double value) override
    {
        switch (varRef) {
            case 0: m_force_x = value;  break;
            case 1: m_pos_x = value;    break;
            case 2: m_vel_x = value;    break;
            case 3: m_mass = value;     break;
            default:
                assert (!"Mass1D::SetVariable(): Invalid variable reference");
        }
    }

    void DoStep(double currentT, double deltaT) override
    {
        const double accel = m_force_x / m_mass;
        const double deltaV = accel * deltaT;
        m_pos_x += m_vel_x*deltaT + 0.5*deltaV*deltaT;
        m_vel_x += deltaV;
    }

private:
    double m_mass;
    double m_pos_x;
    double m_vel_x;
    double m_force_x;
};

class Spring1D : public SlaveInstance
{
public:
    Spring1D() : m_length(1.0), m_stiffness(1.0), m_pos_a_x(0.0), m_pos_b_x(1.0),
                 m_force_a_x(0.0), m_force_b_x(0.0) { }

    double GetVariable(int varRef) override
    {
        switch (varRef) {
            case 0: return m_pos_a_x;   break;
            case 1: return m_pos_b_x;   break;
            case 2: return m_force_a_x; break;
            case 3: return m_force_b_x; break;
            case 4: return m_length;    break;
            case 5: return m_stiffness; break;
            default:
                assert (!"Spring1D::GetVariable(): Invalid variable reference");
        }
        return 0.0;
    }

    void SetVariable(int varRef, double value) override
    {
        switch (varRef) {
            case 0: m_pos_a_x = value;   break;
            case 1: m_pos_b_x = value;   break;
            case 2: m_force_a_x = value; break;
            case 3: m_force_b_x = value; break;
            case 4: m_length = value;    break;
            case 5: m_stiffness = value; break;
            default:
                assert (!"Spring1D::SetVariable(): Invalid variable reference");
        }
    }

    void DoStep(double currentT, double deltaT) override
    {
        const double deltaX = std::fabs(m_pos_b_x - m_pos_a_x) - m_length;
        m_force_a_x = m_pos_a_x <= m_pos_b_x ?   m_stiffness * deltaX
                                             : - m_stiffness * deltaX;
        m_force_b_x = - m_force_a_x;
    }

private:
    double m_length;
    double m_stiffness;
    double m_pos_a_x;
    double m_pos_b_x;
    double m_force_a_x;
    double m_force_b_x;
};

} // namespace

std::unique_ptr<SlaveInstance> NewSlave(const std::string& type)
{
    if (type == "mass_1d") return std::make_unique<Mass1D>();
    else if (type == "spring_1d") return std::make_unique<Spring1D>();
    assert (!"NewSlave(): Invalid slave type");
    return std::unique_ptr<SlaveInstance>();
}
