#ifndef DSB_SLAVE_MOCK_SLAVES_HPP
#define DSB_SLAVE_MOCK_SLAVES_HPP

#include <memory>
#include <string>


class ISlaveInstance
{
public:
    virtual double GetVariable(int varRef) = 0;
    virtual void SetVariable(int varRef, double value) = 0;
    virtual bool DoStep(double currentT, double deltaT) = 0;
};


std::unique_ptr<ISlaveInstance> NewSlave(const std::string& type);


#endif  // header guard
