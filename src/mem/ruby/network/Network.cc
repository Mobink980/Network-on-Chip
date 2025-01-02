/*
 * Copyright (c) 2017 ARM Limited
 * All rights reserved.
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 1999-2008 Mark D. Hill and David A. Wood
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "mem/ruby/network/Network.hh"

#include "base/logging.hh"
#include "mem/ruby/common/MachineID.hh"
#include "mem/ruby/network/BasicLink.hh"
#include "mem/ruby/system/RubySystem.hh"

namespace gem5
{

namespace ruby
{

//number of vnets in the network
uint32_t Network::m_virtual_networks;
//size of the control message
uint32_t Network::m_control_msg_size;
//size of the data message
uint32_t Network::m_data_msg_size;

//Network constructor (for creating a Network instance)
//This is the parent. GarnetNetwork uses this to build
//the main network.
Network::Network(const Params &p)
    : ClockedObject(p)
{
    //std::cout<<"Network constructor called."<<std::endl;
    //set the number of vnets
    m_virtual_networks = p.number_of_virtual_networks;
    //set the size of the control message
    m_control_msg_size = p.control_msg_size;

    //Size of the data message sould be less than or equal to the
    //size of the CacheLine.
    fatal_if(p.data_msg_size > p.ruby_system->getBlockSizeBytes(),
             "%s: data message size > cache line size", name());
    //a data message should include control data as well as data
    m_data_msg_size = p.data_msg_size + m_control_msg_size;

    //register the network for this Network object in the ruby system
    params().ruby_system->registerNetwork(this);

    // Populate localNodeVersions with the version of each MachineType in
    // this network. This will be used to compute a global to local ID.
    // Do this by looking at the ext_node for each ext_link. There is one
    // ext_node per ext_link and it points to an AbstractController.
    // For RubySystems with one network, global and local ID are the same.
    std::unordered_map<MachineType, std::vector<NodeID>> localNodeVersions;
    for (auto &it : params().ext_links) {
        //get the AbstractController for the ext_link
        AbstractController *cntrl = it->params().ext_node;
        //add the version of cntrl (the controller connected to the
        //external link) to localNodeVersions unordered_map
        localNodeVersions[cntrl->getType()].push_back(cntrl->getVersion());
        //register the MachineID for this controller (to compute a global
        //to local id)
        params().ruby_system->registerMachineID(cntrl->getMachineID(), this);
    }

    // Compute a local ID for each MachineType using the same order as SLICC
    NodeID local_node_id = 0;
    for (int i = 0; i < MachineType_base_level(MachineType_NUM); ++i) {
        MachineType mach = static_cast<MachineType>(i);
        if (localNodeVersions.count(mach)) {
            for (auto &ver : localNodeVersions.at(mach)) {
                // Get the global ID, Ruby will pass around
                NodeID global_node_id = MachineType_base_number(mach) + ver;
                //globalToLocalMap has the local_id and global_id for each
                //node in the network.
                globalToLocalMap.emplace(global_node_id, local_node_id);
                ++local_node_id; //find the global_id for the next local_id
            }
        }
    }

    // Total nodes/controllers in network is equal to the local node count.
    // Must make sure this is called after the State Machine constructors.
    m_nodes = local_node_id;

    //the network should have nodes and vnets
    assert(m_nodes != 0);
    assert(m_virtual_networks != 0);

    //================================================================
    //################################################################
    m_topology_ptr = new Topology(m_nodes, p.routers.size(),
                                  p.busses.size(),
                                  m_virtual_networks,
                                  p.ext_links, p.bus_links, p.int_links);
    //================================================================
    //################################################################
    


    // Allocate to and from queues
    // Queues that are getting messages from protocol
    m_toNetQueues.resize(m_nodes);

    // Queues that are feeding the protocol
    m_fromNetQueues.resize(m_nodes);

    //set the size of m_ordered to the number of vnets
    m_ordered.resize(m_virtual_networks);
    //set the size of m_vnet_type_names (e.g., Cntrl, Data, etc.)
    //to the number of vnets
    m_vnet_type_names.resize(m_virtual_networks);

    //initialize m_ordered elements with false (we first assume
    //no vnet is ordered)
    for (int i = 0; i < m_virtual_networks; i++) {
        m_ordered[i] = false;
    }

    // Initialize the controller's network pointers
    for (std::vector<BasicExtLink*>::const_iterator i = p.ext_links.begin();
         i != p.ext_links.end(); ++i) {
        BasicExtLink *ext_link = (*i);
        //abs_cntrl is the external node for the ext_link
        AbstractController *abs_cntrl = ext_link->params().ext_node;
        //initialize the network pointer with this Network object for abs_cntrl
        abs_cntrl->initNetworkPtr(this);
        //get the address ranges from abs_cntrl
        const AddrRangeList &ranges = abs_cntrl->getAddrRanges();
        //if address ranges is not empty
        if (!ranges.empty()) {
            //get the machine_id for the abs_cntrl
            MachineID mid = abs_cntrl->getMachineID();
            //set the machine_id and address ranges for the
            //controller or machine
            AddrMapNode addr_map_node = {
                .id = mid.getNum(),
                .ranges = ranges
            };
            //emplace the mid_type and addr_map_node (to know the
            //global_id and local_id for each node, and that each address
            //refers to which node)
            addrMap.emplace(mid.getType(), addr_map_node);
        }
    }

    // Register a callback function for combining the statistics
    statistics::registerDumpCallback([this]() { collateStats(); });

    //initialize the message buffers for each ext_link external node
    for (auto &it : dynamic_cast<Network *>(this)->params().ext_links) {
        it->params().ext_node->initNetQueues();
    }
}

//Network destructor
Network::~Network()
{
    //for every network node
    for (int node = 0; node < m_nodes; node++) {

        // Delete the Message Buffers
        for (auto& it : m_toNetQueues[node]) {
            delete it;
        }

        for (auto& it : m_fromNetQueues[node]) {
            delete it;
        }
    }

    //delete the pointer to the topology
    delete m_topology_ptr;
}

//the size of the message for a message type (control, data, etc)
uint32_t
Network::MessageSizeType_to_int(MessageSizeType size_type)
{
    switch(size_type) {
      case MessageSizeType_Control:
      case MessageSizeType_Request_Control:
      case MessageSizeType_Reissue_Control:
      case MessageSizeType_Response_Control:
      case MessageSizeType_Writeback_Control:
      case MessageSizeType_Broadcast_Control:
      case MessageSizeType_Multicast_Control:
      case MessageSizeType_Forwarded_Control:
      case MessageSizeType_Invalidate_Control:
      case MessageSizeType_Unblock_Control:
      case MessageSizeType_Persistent_Control:
      case MessageSizeType_Completion_Control:
        return m_control_msg_size;
      case MessageSizeType_Data:
      case MessageSizeType_Response_Data:
      case MessageSizeType_ResponseLocal_Data:
      case MessageSizeType_ResponseL2hit_Data:
      case MessageSizeType_Writeback_Data:
        return m_data_msg_size;
      default:
        panic("Invalid range for type MessageSizeType");
        break;
    }
}

//sets the m_ordered and m_vnet_type_names for a vnet
//also ensure node_id and network_id are not out of range
void
Network::checkNetworkAllocation(NodeID local_id, bool ordered,
                                        int network_num,
                                        std::string vnet_type)
{
    fatal_if(local_id >= m_nodes, "Node ID is out of range");
    fatal_if(network_num >= m_virtual_networks, "Network id is out of range");

    //if the vnet is ordered, update m_ordered for that
    //vnet number (network_num)
    if (ordered) {
        m_ordered[network_num] = true;
    }

    //set the vnet type for the vnet
    m_vnet_type_names[network_num] = vnet_type;
}

//set toNetQueues (queues that are getting messages from protocol)
//for a vnet in local node
void
Network::setToNetQueue(NodeID global_id, bool ordered, int network_num,
                                 std::string vnet_type, MessageBuffer *b)
{
    //get the local_id of the node from its global_id
    NodeID local_id = getLocalNodeID(global_id);
    //set m_ordered and m_vnet_type_names for the vnet (network_num)
    //also ensure node_id and network_id are not out of range
    checkNetworkAllocation(local_id, ordered, network_num, vnet_type);

    //for every toNetQueue of the local node that is less than or
    //equal to network_num, push nullptr in m_toNetQueues[local_id]
    while (m_toNetQueues[local_id].size() <= network_num) {
        m_toNetQueues[local_id].push_back(nullptr);
    }
    //set the network_num'th toNetQueue of the local node to b MessageBuffer
    m_toNetQueues[local_id][network_num] = b;
}

//set fromNetQueues (queues that are feeding the protocol)
//for a vnet in local node
void
Network::setFromNetQueue(NodeID global_id, bool ordered, int network_num,
                                   std::string vnet_type, MessageBuffer *b)
{
    //get the local_id of the node from its global_id
    NodeID local_id = getLocalNodeID(global_id);
    //set m_ordered and m_vnet_type_names for the vnet (network_num)
    //also ensure node_id and network_id are not out of range
    checkNetworkAllocation(local_id, ordered, network_num, vnet_type);

    //for every fromNetQueue of the local node that is less than or
    //equal to network_num, push nullptr in m_fromNetQueues[local_id]
    while (m_fromNetQueues[local_id].size() <= network_num) {
        m_fromNetQueues[local_id].push_back(nullptr);
    }
    //set the network_num'th fromNetQueue of the local node to b MessageBuffer
    m_fromNetQueues[local_id][network_num] = b;
}

NodeID
Network::addressToNodeID(Addr addr, MachineType mtype)
{
    // Look through the address maps for entries with matching machine
    // type to get the responsible node for this address.
    const auto &matching_ranges = addrMap.equal_range(mtype);
    for (auto it = matching_ranges.first; it != matching_ranges.second; it++) {
        AddrMapNode &node = it->second;
        auto &ranges = node.ranges;
        for (AddrRange &range: ranges) {
            if (range.contains(addr)) {
                return node.id; //return the node_id for the given address
            }
        }
    }
    //return the total number of components for machine mtype
    return MachineType_base_count(mtype);
}

//it takes the global_id of a node and returns its local_id
NodeID
Network::getLocalNodeID(NodeID global_id) const
{
    //make sure the given global_id can be found in globalToLocalMap
    assert(globalToLocalMap.count(global_id));
    //return the local_id of the node, for the given global_id
    return globalToLocalMap.at(global_id);
}

} // namespace ruby
} // namespace gem5
