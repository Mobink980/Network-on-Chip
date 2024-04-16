/*
 * Copyright (c) 2020 Advanced Micro Devices, Inc.
 * Copyright (c) 2020 Inria
 * Copyright (c) 2016 Georgia Institute of Technology
 * Copyright (c) 2008 Princeton University
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


#ifndef __MEM_RUBY_NETWORK_GARNET_0_NETWORKINTERFACE_HH__
#define __MEM_RUBY_NETWORK_GARNET_0_NETWORKINTERFACE_HH__

#include <iostream>
#include <vector>

#include "mem/ruby/common/Consumer.hh"
#include "mem/ruby/network/garnet/CommonTypes.hh"
#include "mem/ruby/network/garnet/Credit.hh"
#include "mem/ruby/network/garnet/CreditLink.hh"
#include "mem/ruby/network/garnet/GarnetNetwork.hh"
#include "mem/ruby/network/garnet/NetworkLink.hh"
#include "mem/ruby/network/garnet/OutVcState.hh"
#include "mem/ruby/slicc_interface/Message.hh"
#include "params/GarnetNetworkInterface.hh"

namespace gem5
{

namespace ruby
{

class MessageBuffer;

namespace garnet
{

class flitBuffer;

//NetworkInterface (NI) is both a ClockedObject and a Consumer
class NetworkInterface : public ClockedObject, public Consumer
{
  public:
    typedef GarnetNetworkInterfaceParams Params;
    //NetworkInterface constructor
    NetworkInterface(const Params &p);
    //NetworkInterface destructor
    ~NetworkInterface() = default;

    //add an inport (input port) to the NI
    void addInPort(NetworkLink *in_link, CreditLink *credit_link);
    //add an outport (output port) to the NI
    void addOutPort(NetworkLink *out_link, CreditLink *credit_link,
        SwitchID router_id, uint32_t consumerVcs);

    //for enqueuing a stalled message into the MessageBuffer
    //in the next cycle, after a message was dequeued this cycle
    void dequeueCallback();
    //function that wakes the NI up to do its job
    void wakeup();
    //add a node to the NetworkInterface
    void addNode(std::vector<MessageBuffer *> &inNode,
                 std::vector<MessageBuffer *> &outNode);

    //printing the NetworkInterface
    void print(std::ostream& out) const;
    //get the vnet for a vc
    int get_vnet(int vc);
    //set or initialize a pointer to the GarnetNetwork
    void init_net_ptr(GarnetNetwork *net_ptr) { m_net_ptr = net_ptr; }

    bool functionalRead(Packet *pkt, WriteMask &mask);
    
    //updating niOutVcs and outport outFlitQueue flits 
    //with the data from the packet
    uint32_t functionalWrite(Packet *);

    //schedule a flit to be sent from an NI output port
    void scheduleFlit(flit *t_flit);

    //get the id of the router connected to the NI with an outport
    //each port has a specific vnet number
    int get_router_id(int vnet)
    {
        //get the outport for the given vnet
        OutputPort *oPort = getOutportForVnet(vnet);
        //make sure the outport exists
        assert(oPort);
        //return the router id of that outport
        return oPort->routerID();
    }

    //class OutputPort is a member of the NetworkInterface class
    class OutputPort
    {
      public:
          //OutputPort constructor 
          //We need a NetworkLink, a CreditLink, and a router id to
          //instantiate an NI outport
          OutputPort(NetworkLink *outLink, CreditLink *creditLink,
              int routerID)
          {
              //outport vnet
              _vnets = outLink->mVnets;
              //the flitBuffer for sending out flits to the network
              _outFlitQueue = new flitBuffer();

              //set the network link going out of the outport
              _outNetLink = outLink;
              //set the credit link coming into the outport
              _inCreditLink = creditLink;

              //set the id of the router connected to this NI
              _routerID = routerID;
              //set the outport link bitWidth (from network link)
              _bitWidth = outLink->bitWidth;
              //set the VC round-robin to zero
              _vcRoundRobin = 0;

          }

          //get the flitBuffer for sending out flits to the network
          flitBuffer *
          outFlitQueue()
          {
              return _outFlitQueue;
          }

          //get the network link going out of the outport
          NetworkLink *
          outNetLink()
          {
              return _outNetLink;
          }

          //get the credit link coming into the outport
          CreditLink *
          inCreditLink()
          {
              return _inCreditLink;
          }

          //get the id of the router connected to the NI
          int
          routerID()
          {
              return _routerID;
          }

          //get the outport links bitWidth
          uint32_t bitWidth()
          {
              return _bitWidth;
          }

          //check whether vnet is supported
          bool isVnetSupported(int pVnet)
          {
              if (!_vnets.size()) {
                  return true;
              }

              for (auto &it : _vnets) {
                  if (it == pVnet) {
                      return true;
                  }
              }
              return false;

          }

          //for printing the outport vnets
          std::string
          printVnets()
          {
              std::stringstream ss;
              for (auto &it : _vnets) {
                  ss << it;
                  ss << " ";
              }
              return ss.str();
          }

          //get the vc round-robin has selected
          int vcRoundRobin()
          {
              return _vcRoundRobin;
          }

          //set the vc for round-robin
          void vcRoundRobin(int vc)
          {
              _vcRoundRobin = vc;
          }


      private:
          //vnets vector
          std::vector<int> _vnets;
          //for sending out flits to the network
          flitBuffer *_outFlitQueue;

          //network link going out of the outport
          NetworkLink *_outNetLink;
          //credit link coming into the outport
          CreditLink *_inCreditLink;

          int _vcRoundRobin; // For round robin scheduling

          int _routerID; //id of the router connected to the NI
          uint32_t _bitWidth; //bitWidth of the outport links
    };

    //class InputPort is a member of the NetworkInterface class
    class InputPort
    {
      public:
          //InputPort constructor
          //We need a NetworkLink, and a CreditLink to instantiate
          //an NI inport
          InputPort(NetworkLink *inLink, CreditLink *creditLink)
          {
              //inport vnets
              _vnets = inLink->mVnets;
              //set the flitBuffer for sending credit flits to the network
              _outCreditQueue = new flitBuffer();

              //set the network link coming into the inport
              _inNetLink = inLink;
              //set the credit link going out of the inport
              _outCreditLink = creditLink;
              //set the inport link bitWidth (from network link)
              _bitWidth = inLink->bitWidth;
          }

          //get the flitBuffer for sending credit flits to the network
          flitBuffer *
          outCreditQueue()
          {
              return _outCreditQueue;
          }

          //get the network link coming into the inport
          NetworkLink *
          inNetLink()
          {
              return _inNetLink;
          }

          //get the credit link going out of the inport
          CreditLink *
          outCreditLink()
          {
              return _outCreditLink;
          }

          //check whether vnet is supported
          bool isVnetSupported(int pVnet)
          {
              if (!_vnets.size()) {
                  return true;
              }

              for (auto &it : _vnets) {
                  if (it == pVnet) {
                      return true;
                  }
              }
              return false;

          }

          //for sending credit flits to the network
          void sendCredit(Credit *cFlit)
          {
              //insert the given flit into _outCreditQueue flitBuffer
              _outCreditQueue->insert(cFlit);
          }

          //get the inport links bitWidth
          uint32_t bitWidth()
          {
              return _bitWidth;
          }

          //for printing inport vnets
          std::string
          printVnets()
          {
              std::stringstream ss;
              for (auto &it : _vnets) {
                  ss << it;
                  ss << " ";
              }
              return ss.str();
          }

          // Queue for stalled flits
          std::deque<flit *> m_stall_queue;
          //check to see if the message enqueued in this cycle
          bool messageEnqueuedThisCycle;
      private:
          //inport vnets
          std::vector<int> _vnets;
          //the flitBuffer for sending credit flits to the network
          flitBuffer *_outCreditQueue;

          //the network link coming into the inport
          NetworkLink *_inNetLink;
          //the credit link going out of the inport
          CreditLink *_outCreditLink;
          //bitWidth of the inport links
          uint32_t _bitWidth;
    };


  private:
    //pointer to the GarnetNetwork
    GarnetNetwork *m_net_ptr;
    //id of the NI or node (num_NIs = num_cores)
    const NodeID m_id;
    //number of VCs
    const int m_virtual_networks;
    //number of VCs per Vnet
    int m_vc_per_vnet;
    //vc allocators
    std::vector<int> m_vc_allocator;
    //NetworkInterface outports
    std::vector<OutputPort *> outPorts;
    //NetworkInterface inports
    std::vector<InputPort *> inPorts;
    //to check for possible network deadlock in a vnet
    int m_deadlock_threshold;
    //for knowing the states of the VCs
    std::vector<OutVcState> outVcState;

    //number of stalls
    std::vector<int> m_stall_count;

    // Input Flit Buffers
    // The flit buffers which will serve the Consumer
    std::vector<flitBuffer>  niOutVcs;
    std::vector<Tick> m_ni_out_vcs_enqueue_time;

    // The Message buffers that takes messages from the protocol
    //from the coherence protocol controller
    std::vector<MessageBuffer *> inNode_ptr;
    // The Message buffers that provides messages to the protocol
    //to the coherence protocol controller
    std::vector<MessageBuffer *> outNode_ptr;
    // When a vc stays busy for a long time, it indicates a deadlock
    std::vector<int> vc_busy_counter;

    //checking the stall queue to reschedule stalled flits
    void checkStallQueue();
    //NI flitisizes the messages it gets from the coherence 
    //protocol buffer in appropriate vnet and sends those flits
    //to the network.
    bool flitisizeMessage(MsgPtr msg_ptr, int vnet);
    //Looking for a free output vc
    int calculateVC(int vnet);

    //choose a vc from the outport in a round-robin manner
    void scheduleOutputPort(OutputPort *oPort);
    //schedule the outport link wakeup
    void scheduleOutputLink();
    //Wakeup the NI in the next cycle to consume msgs or flits, 
    //or when there's a clock period difference (to consume link flits)
    void checkReschedule();

    //incremet the stats within the flit
    void incrementStats(flit *t_flit);

    //get the inport for the given vnet
    InputPort *getInportForVnet(int vnet);
    //get the outport for the given vnet
    OutputPort *getOutportForVnet(int vnet);
};

} // namespace garnet
} // namespace ruby
} // namespace gem5

#endif // __MEM_RUBY_NETWORK_GARNET_0_NETWORKINTERFACE_HH__

