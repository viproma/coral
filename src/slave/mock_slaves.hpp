#ifndef DSB_SLAVE_MOCK_SLAVES_HPP
#define DSB_SLAVE_MOCK_SLAVES_HPP

#include <memory>
#include <string>


class SlaveInstance
{
public:
    virtual double GetVariable(int varRef) = 0;
    virtual void SetVariable(int varRef, double value) = 0;
    virtual void DoStep(double currentT, double deltaT) = 0;
};


std::unique_ptr<SlaveInstance> NewSlave(const std::string& type);


#endif  // header guard
