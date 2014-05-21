#include "model_parser.hpp"

#include "boost/foreach.hpp"
#include "boost/property_tree/info_parser.hpp"
#include "boost/property_tree/ptree.hpp"


namespace
{
    boost::property_tree::ptree ReadPtreeInfoFile(const std::string& path)
    {
        boost::property_tree::ptree pt;
        boost::property_tree::read_info(path, pt);
        return pt;
    }
}

dsb::model::Model ParseModelFile(const std::string& path)
{
    using namespace dsb::model;
    const auto ptree = ReadPtreeInfoFile(path);
    Model model;
    BOOST_FOREACH (const auto& slaveNode,
                   ptree.get_child("slaves", boost::property_tree::ptree())) {
        const auto& slaveName = slaveNode.first;
        const auto& slaveData = slaveNode.second;
        const auto& slaveType = slaveData.get<std::string>("type");
        auto slave = model.AddSlave(slaveName, slaveType);
        auto& slaveParams = slave->Params();
        BOOST_FOREACH (const auto& paramNode,
                       slaveData.get_child("params", boost::property_tree::ptree())) {
            slaveParams[paramNode.first] = paramNode.second.data();
        }
    }
    BOOST_FOREACH (const auto& connNode,
                   ptree.get_child("connections", boost::property_tree::ptree())) {
        model.Connect(Variable(connNode.first), Variable(connNode.second.data()));
    }
    return model;
}
