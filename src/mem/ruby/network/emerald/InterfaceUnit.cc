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


#include "mem/ruby/network/emerald/InterfaceUnit.hh"

#include <cassert>
#include <cmath>
#include <iostream>

#include "base/cast.hh"
#include "debug/RubyNetwork.hh"
#include "mem/ruby/network/MessageBuffer.hh"
#include "mem/ruby/network/emerald/Affirm.hh"
#include "mem/ruby/network/emerald/fragmentBuffer.hh"
#include "mem/ruby/slicc_interface/Message.hh"


namespace gem5
{

namespace ruby
{

namespace emerald
{

//InterfaceUnit constructor
InterfaceUnit::InterfaceUnit(const Params &p)
  : ClockedObject(p), Consumer(this), m_id(p.id),
    m_virtual_networks(p.virt_nets), m_vc_per_vnet(0),
    m_vc_allocator(m_virtual_networks, 0),
    m_bus_vc_allocator(m_virtual_networks, 0),
    m_deadlock_threshold(p.emerald_deadlock_threshold),
    vc_busy_counter(m_virtual_networks, 0),
    bus_vc_busy_counter(m_virtual_networks, 0)
{
    //counting stall numbers for each vnet
    m_stall_count.resize(m_virtual_networks);
    //NI outvcs have no element in the beginning
    niOutVcs.resize(0);
    //=============================================
    //counting stall numbers for each vnet for bus
    m_stall_count_bus.resize(m_virtual_networks);
    //ToBus outvcs have no element in the beginning
    toBusVcs.resize(0);
    //=============================================
}

//add an input port to the InterfaceUnit
void
InterfaceUnit::addInPort(GridLink *in_link,
                              AffirmLink *credit_link)
{
    //instantiate a new input port
    InputPort *newInPort = new InputPort(in_link, credit_link);
    //push the newly created input port in inPorts vector
    inPorts.push_back(newInPort);
    //printing the input port that was added and its vnets
    DPRINTF(RubyNetwork, "Adding input port:%s with vnets %s\n",
    in_link->name(), newInPort->printVnets());

    //InterfaceUnit is the consumer of the inport network link
    in_link->setLinkConsumer(this);
    //set the source queue for the credit_link
    //this source queue is the flitBuffer for sending credit flits to the network
    credit_link->setSourceQueue(newInPort->outCreditQueue(), this);
    //if number of VCs per vnet is not zero,
    //setVcsPerVnet for the network and credit link of the inport
    if (m_vc_per_vnet != 0) {
        in_link->setVcsPerVnet(m_vc_per_vnet);
        credit_link->setVcsPerVnet(m_vc_per_vnet);
    }

}

//add an output port to the InterfaceUnit
void
InterfaceUnit::addOutPort(GridLink *out_link,
                             AffirmLink *credit_link,
                             SwitchID router_id, uint32_t consumerVcs)
{
    /*
     * "consumerVcs" ==> When a flit arrives at a consumer node (NI in our case)
     * it needs to be buffered in a virtual channel before being processed by 
     * the protocol. 
    */
    //instantiate a new output port
    OutputPort *newOutPort = new OutputPort(out_link, credit_link, router_id);
    //push the newly created output port in outPorts vector
    outPorts.push_back(newOutPort);

    //ensure we have at least one consumer vc
    assert(consumerVcs > 0);
    // We are not allowing different physical links to have different vcs.
    // If it is required that the Network Interface support different VCs
    // for every physical link connected to it, then they need to change
    // the logic within outport and inport.
    if (niOutVcs.size() == 0) { //if the size of outvcs for NI is zero
        //number of VCs per vnet becomes number of consumer VCs
        m_vc_per_vnet = consumerVcs;
        //number of vcs = num_vc_per_vnet * num_vnets
        int m_num_vcs = consumerVcs * m_virtual_networks;
        //the size of niOutVcs becomes the number of vcs
        niOutVcs.resize(m_num_vcs);
        //we need to hold the state of each outvc in NI
        outVcState.reserve(m_num_vcs);
        //we need to hold the enqueue time for each vc in niOutVcs
        m_ni_out_vcs_enqueue_time.resize(m_num_vcs);
        // instantiating the NI flit buffers
        for (int i = 0; i < m_num_vcs; i++) {
            m_ni_out_vcs_enqueue_time[i] = Tick(INFINITE_);
            outVcState.emplace_back(i, m_net_ptr, consumerVcs);
        }

        // Reset VC Per VNET for input links already instantiated
        for (auto &iPort: inPorts) {
            //network link of the inport
            GridLink *inNetLink = iPort->inNetLink();
            inNetLink->setVcsPerVnet(m_vc_per_vnet);
            credit_link->setVcsPerVnet(m_vc_per_vnet);
        }
    } else { //if the size of outvcs for NI is greater than zero
        fatal_if(consumerVcs != m_vc_per_vnet,
        "%s: Connected Physical links have different vc requests: %d and %d\n",
        name(), consumerVcs, m_vc_per_vnet);
    }

    //print the NI outport and its vnets
    DPRINTF(RubyNetwork, "OutputPort:%s Vnet: %s\n",
    out_link->name(), newOutPort->printVnets());

    //set the source queue for newOutPort (the flitBuffer for sending
    //out flits to the network)
    out_link->setSourceQueue(newOutPort->outFlitQueue(), this);
    //set the number of VCs per Vnet (e.g., 4) for
    //out_link (outport network link)
    out_link->setVcsPerVnet(m_vc_per_vnet);
    //InterfaceUnit is the consumer of credit_link for outport
    credit_link->setLinkConsumer(this);
    //set the number of VCs per Vnet (e.g., 4) for
    //credit_link (outport credit link)
    credit_link->setVcsPerVnet(m_vc_per_vnet);
}

//=======================================================================
//=======================================================================
//add an NI inport (for bus communication) to the NetworkInterface
void
InterfaceUnit::addNetworkInport(GridLink *in_link,
                              AffirmLink *credit_link)
{
    //instantiate a new input port for bus communication
    NetworkInport *new_inport = new NetworkInport(in_link, credit_link);
    //push the newly created input port in inPorts vector
    ni_inports.push_back(new_inport);
    //printing the input port that was added and its vnets
    DPRINTF(RubyNetwork, "Adding input port:%s with vnets %s\n",
    in_link->name(), new_inport->printVnets());

    //NetworkInterface is the consumer of the inport network link
    in_link->setLinkConsumer(this);
    //set the source queue for the credit_link
    //this source queue is the flitBuffer for sending credit flits to the network
    credit_link->setSourceQueue(new_inport->outCreditQueue(), this);
    //if number of VCs per vnet is not zero,
    //setVcsPerVnet for the network and credit link of the inport
    if (m_vc_per_vnet != 0) {
        in_link->setVcsPerVnet(m_vc_per_vnet);
        credit_link->setVcsPerVnet(m_vc_per_vnet);
    }

}

//add an NI output port (for bus communication) to the NetworkInterface
void
InterfaceUnit::addNetworkOutport(GridLink *out_link,
                             AffirmLink *credit_link,
                             SwitchID bus_id, uint32_t consumerVcs)
{
    //instantiate a new output port for bus communication
    NetworkOutport *new_outport = new NetworkOutport(out_link, credit_link, bus_id);
    //push the newly created output port in ni_outports vector
    ni_outports.push_back(new_outport);

    //ensure we have at least one consumer vc
    assert(consumerVcs > 0);
    // We are not allowing different physical links to have different vcs.
    // If it is required that the Network Interface support different VCs
    // for every physical link connected to it, then they need to change
    // the logic within outport and inport.
    if (toBusVcs.size() == 0) { //if the size of outvcs for NI is zero
        //number of VCs per vnet becomes number of consumer VCs
        m_vc_per_vnet = consumerVcs;
        //number of vcs = num_vc_per_vnet * num_vnets
        int m_num_vcs = consumerVcs * m_virtual_networks;
        //the size of toBusVcs becomes the number of vcs
        toBusVcs.resize(m_num_vcs);
        //we need to hold the state of each outvc in NI
        toBusVcState.reserve(m_num_vcs);
        //we need to hold the enqueue time for each vc in toBusVcs
        m_to_bus_vcs_enqueue_time.resize(m_num_vcs);
        // instantiating the NI flit buffers
        for (int i = 0; i < m_num_vcs; i++) {
            m_to_bus_vcs_enqueue_time[i] = Tick(INFINITE_);
            toBusVcState.emplace_back(i, m_net_ptr, consumerVcs);
        }

        // Reset VC Per VNET for input links already instantiated
        for (auto &ni_inport: ni_inports) {
            //network link of the inport
            GridLink *inNetLink = ni_inport->inNetLink();
            inNetLink->setVcsPerVnet(m_vc_per_vnet);
            credit_link->setVcsPerVnet(m_vc_per_vnet);
        }
    } else { //if the size of outvcs for NI is greater than zero
        fatal_if(consumerVcs != m_vc_per_vnet,
        "%s: Connected Physical links have different vc requests: %d and %d\n",
        name(), consumerVcs, m_vc_per_vnet);
    }

    //print the NI outport and its vnets
    DPRINTF(RubyNetwork, "OutputPort:%s Vnet: %s\n",
    out_link->name(), new_outport->printVnets());

    //set the source queue for newOutPort (the flitBuffer for sending
    //out flits to the network)
    out_link->setSourceQueue(new_outport->outFlitQueue(), this);
    //set the number of VCs per Vnet (e.g., 4) for
    //out_link (outport network link)
    out_link->setVcsPerVnet(m_vc_per_vnet);
    //NetworkInterface is the consumer of credit_link for outport
    credit_link->setLinkConsumer(this);
    //set the number of VCs per Vnet (e.g., 4) for
    //credit_link (outport credit link)
    credit_link->setVcsPerVnet(m_vc_per_vnet);
}
//=======================================================================
//=======================================================================

//add a node to the InterfaceUnit (e.g., east, west, etc.)
void
InterfaceUnit::addNode(std::vector<MessageBuffer *>& in,
                          std::vector<MessageBuffer *>& out)
{
    //set the MessageBuffers that take messages from the protocol
    inNode_ptr = in;
    //set the MessageBuffers that provide messages for the protocol
    outNode_ptr = out;

    //for all MessageBuffers that take messages from
    //the protocol (cache controller)
    for (auto& it : in) {
        if (it != nullptr) {
            //NI is the consumer for the MessageBuffer
            //(consumes from protocol)
            it->setConsumer(this);
        }
    }
}

//for enqueuing a stalled message into the MessageBuffer
//in the next cycle, after a message was dequeued this cycle
void
InterfaceUnit::dequeueCallback()
{
    // An output MessageBuffer has dequeued something this cycle and there
    // is now space to enqueue a stalled message. However, we cannot wake
    // on the same cycle as the dequeue. Schedule a wake at the soonest
    // possible time (next cycle).
    scheduleEventAbsolute(clockEdge(Cycles(1)));
}

//incremet the stats for the NI and the flit
void
InterfaceUnit::incrementStats(fragment *t_flit)
{
    //get the vnet of the flit
    int vnet = t_flit->get_vnet();
    //increment the received flits for the vnet in EmeraldNetwork
    m_net_ptr->increment_received_flits(vnet);
  
    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // Latency
    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    //network delay = fragment_dequeue_time - fragment_enqueue_time - Ticks(1_cycle)
    Tick network_delay =
        t_flit->get_dequeue_time() -
        t_flit->get_enqueue_time() - cyclesToTicks(Cycles(1));
    //queuing delay at src node
    Tick src_queueing_delay = t_flit->get_src_delay();
    //queuing delay at dest node = current_tick - last time the flit was dequeued
    Tick dest_queueing_delay = (curTick() - t_flit->get_dequeue_time());
    //queueing_delay for the flit
    Tick queueing_delay = src_queueing_delay + dest_queueing_delay;

    //increment the flit network and queuing latency for the EmeraldNetwork
    m_net_ptr->increment_flit_network_latency(network_delay, vnet);
    m_net_ptr->increment_flit_queueing_latency(queueing_delay, vnet);

    //if the flit is of type TAIL_ or HEAD_TAIL_
    if (t_flit->get_type() == TAIL_ || t_flit->get_type() == HEAD_TAIL_) {
        //increment the number of received packets for the EmeraldNetwork
        m_net_ptr->increment_received_packets(vnet);
        //increment packet network latency for the EmeraldNetwork
        m_net_ptr->increment_packet_network_latency(network_delay, vnet);
        //increment packet queuing latency for the EmeraldNetwork
        m_net_ptr->increment_packet_queueing_latency(queueing_delay, vnet);
    }

    // Hops that the flit traversed
    m_net_ptr->increment_total_hops(t_flit->get_route().hops_traversed);
}

//=========================================================================
//=========================================================================
// Incremet the stats for the NI and the flit when came from bus
// and not going to be ejected
void
InterfaceUnit::incrementStatsSpecial(fragment *t_flit)
{
    //get the vnet of the flit
    int vnet = t_flit->get_vnet();

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // Latency
    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    //network delay for the flit
    Tick network_delay =
        t_flit->get_dequeue_time() -
        t_flit->get_enqueue_time() - cyclesToTicks(Cycles(1));
    //queuing delay at src node
    Tick src_queueing_delay = t_flit->get_src_delay();
    //queuing delay at dest node = current_tick - time the flit was dequeued
    Tick dest_queueing_delay = (curTick() - t_flit->get_dequeue_time());
    //queueing_delay for the flit
    Tick queueing_delay = src_queueing_delay + dest_queueing_delay;

    //increment the flit network and queuing latency for the vnet in EmeraldNetwork
    m_net_ptr->increment_flit_network_latency(network_delay, vnet);
    m_net_ptr->increment_flit_queueing_latency(queueing_delay, vnet);

    //if the flit is of type TAIL_ or HEAD_TAIL_ (a packet received)
    if (t_flit->get_type() == TAIL_ || t_flit->get_type() == HEAD_TAIL_) {
        //increment packet network latency for the vnet
        m_net_ptr->increment_packet_network_latency(network_delay, vnet);
        //increment packet queuing latency for the vnet
        m_net_ptr->increment_packet_queueing_latency(queueing_delay, vnet);
    }
    // Hops that the flit traversed
    m_net_ptr->increment_total_hops(t_flit->get_route().hops_traversed);
}
//=========================================================================
//=========================================================================

/*
 * The NI wakeup checks whether there are any ready messages in the protocol
 * buffer. If yes, it picks that up, flitisizes it into a number of fragments and
 * puts it into an output buffer and schedules the output link. On a wakeup
 * it also checks whether there are fragments in the input link. If yes, it picks
 * them up and if the fragment is a tail, the NI inserts the corresponding message
 * into the protocol buffer. It also checks for credits being sent by the
 * downstream router.
 */
void
InterfaceUnit::wakeup()
{ 
  
    //define an std::ostringstream
    std::ostringstream oss;
    //get the router_id and vnets for all the outports of the NI
    for (auto &oPort: outPorts) {
        oss << oPort->routerID() << "[" << oPort->printVnets() << "] ";
    }
    //===========================================================
    std::ostringstream bus_oss;
    //get the bus_id and vnets for all the outports of the NI
    for (auto &ni_outport: ni_outports) {
        bus_oss << ni_outport->busID() << "[" << ni_outport->printVnets() << "] ";
    }
    //===========================================================
    //for printing what NI waked up when
    DPRINTF(RubyNetwork, "Network Interface %d connected to router:%s and bus:%s"
            " woke up. Period: %ld\n", m_id, oss.str(), bus_oss.str(), clockPeriod());

    //make sure the current_tick is a clock edge (the tick a cycle begins)
    assert(curTick() == clockEdge());
    MsgPtr msg_ptr;
    Tick curTime = clockEdge(); //get the current time

    // Checking for messages coming from the protocol
    // can pick up a message/cycle for each virtual net
    for (int vnet = 0; vnet < inNode_ptr.size(); ++vnet) {
        //get the MessageBuffer based on vnet
        MessageBuffer *b = inNode_ptr[vnet];
        if (b == nullptr) { //no message from that protocol buffer
            continue;
        }
        if (b->isReady(curTime)) { // Is there a message waiting
            //get a pointer to the message at the head of b
            msg_ptr = b->peekMsgPtr();
            //if the message for that vnet could be flitisized
            if (flitisizeMessage(msg_ptr, vnet)) {
                //dequeue that message from b at current_time
                b->dequeue(curTime);
            }
        }
    }
    //schedule the outport link wakeup to consume the flits
    scheduleOutputLink();
    //=============================================
    //=============================================
    //schedule the bus outport link wakeup to consume the flits
    // scheduleBusOutputLink();
    //=============================================
    //=============================================
    // Check if there are flits stalling a virtual channel. Track if a
    // message is enqueued to restrict ejection to one message per cycle.
    checkStallQueue();

    /**************** Check the incoming flit link ****************/
    DPRINTF(RubyNetwork, "Number of input ports: %d\n", inPorts.size());
    for (auto &iPort: inPorts) { //for every inport
        //get the network link for that inport
        GridLink *inNetLink = iPort->inNetLink();
        //if the network link buffer has a ready flit at the current tick
        if (inNetLink->isReady(curTick())) {
            //consume that flit on the network link and put it in t_flit
            fragment *t_flit = inNetLink->consumeLink();
            //print the flit that was received by the NI
            DPRINTF(RubyNetwork, "Recieved flit:%s\n", *t_flit);
            //make sure the flit width and the bitWidth of the inport
            //are the same
            assert(t_flit->m_width == iPort->bitWidth());

            //get the vnet of t_flit
            int vnet = t_flit->get_vnet();
            //set the flit dequeue time from FIFO to current_tick
            //(dequeue the flit)
            t_flit->set_dequeue_time(curTick());

            // If a tail flit is received, enqueue into the protocol buffers
            // if space is available. Otherwise, exchange non-tail flits for
            // credits.
            //If we get a tail flit, it means all the flits of the message is
            //received, and thus, we can dequeue from vc and enqueue into the
            //protocol buffer; therefore, is_free_signal in the credit signal
            //that we send back is true, becuase we have a free vc. Else,
            //is_free_signal in the credit we're sending back would be false.
            if (t_flit->get_type() == TAIL_ ||
                t_flit->get_type() == HEAD_TAIL_) {
                if (!iPort->messageEnqueuedThisCycle &&
                    outNode_ptr[vnet]->areNSlotsAvailable(1, curTime)) {
                    // Space is available. Enqueue to protocol buffer.
                    outNode_ptr[vnet]->enqueue(t_flit->get_msg_ptr(), curTime,
                                               cyclesToTicks(Cycles(1)));

                    // Simply send a credit back since we are not buffering
                    // this flit in the NI
                    Affirm *cFlit = new Affirm(t_flit->get_vc(),
                                               true, curTick());
                    //send the cFlit credit from NI to the network
                    iPort->sendCredit(cFlit);
                    // Update stats and delete flit pointer
                    incrementStats(t_flit);
                    delete t_flit;
                } else {
                    // No space available- Place tail flit in stall queue and
                    // set up a callback for when protocol buffer is dequeued.
                    // Stat update and flit pointer deletion will occur upon
                    // unstall.
                    ////push the flit into stall queue
                    iPort->m_stall_queue.push_back(t_flit);
                    //increment the number of stalls for the vnet
                    m_stall_count[vnet]++;

                    //set up a callback for when protocol buffer is dequeued
                    outNode_ptr[vnet]->registerDequeueCallback([this]() {
                        dequeueCallback(); });
                }
            } else { //HEAD or BODY flit
                // Non-tail flit. Send back a credit but not VC free signal.
                Affirm *cFlit = new Affirm(t_flit->get_vc(), false,
                                               curTick());
                // Simply send a credit back since we are not buffering
                // this flit in the NI
                iPort->sendCredit(cFlit);

                // Update stats and delete flit pointer.
                incrementStats(t_flit);
                delete t_flit;
            }
        }
    }

    /**************** Check the incoming credit link ****************/
    for (auto &oPort: outPorts) { //for every outport
        //get the credit link for that outport
        AffirmLink *inCreditLink = oPort->inCreditLink();
        //if that credit link has a ready flit at current tick
        if (inCreditLink->isReady(curTick())) {
            //consume that flit on the credit link and put it in t_credit
            Affirm *t_credit = (Affirm*) inCreditLink->consumeLink();
            //increment credit (free space) for the vc of t_credit in
            //outVcState vector (It means that the downstream router got
            //and consumed the flit that we sent and now that vc has another
            //free slot or credit for us to send more)
            outVcState[t_credit->get_vc()].increment_credit();
            //if is_free_signal is true (meaning vc got totally free)
            if (t_credit->is_free_signal()) {
                //change the state for that vc from active to idle
                //at current_tick
                outVcState[t_credit->get_vc()].setState(IDLE_,
                    curTick());
            }
            //delete t_credit variable
            delete t_credit;
        }
    }

    // It is possible to enqueue multiple outgoing credit flits if a message
    // was unstalled in the same cycle as a new message arrives. In this
    // case, we should schedule another wakeup to ensure the credit is sent
    // back.
    for (auto &iPort: inPorts) { //for every inport
        //if we have more than one credit in the inport credit queue
        if (iPort->outCreditQueue()->getSize() > 0) {
            //print the credit flit we are sending back, the credit queue
            //we are sending from, and the time we are sending it
            DPRINTF(RubyNetwork, "Sending a credit %s via %s at %ld\n",
            *(iPort->outCreditQueue()->peekTopFlit()),
            iPort->outCreditLink()->name(), clockEdge(Cycles(1)));
            //the credit link of the inport should consume the credit flit
            //in the next clock edge (to send to local router)
            iPort->outCreditLink()->
                scheduleEventAbsolute(clockEdge(Cycles(1)));
        }
    }

  //========================================================================
  //========================================================================
    /********** Check congested_packets for retransmission ***********/
    if (!congested_packets.empty()) {
        //number of suspended packets
        int num_packets = congested_packets.size();
        //traverse over all elements in congested_packets and
        //try to resend them 
        for (int k = 0; k < num_packets; k++) {
          //peek the top flit in the flitBuffer
          fragment* suspended_flit = congested_packets[k].peekTopFlit();
          
          //If the type is HEAD or HEAD_TAIL, we need to find a free vc
          if (suspended_flit->get_type() == HEAD_TAIL_ || suspended_flit->get_type() == HEAD_) {
              //find a free vc in destination vnet in niOutVcs
              int vc = calculateVC(suspended_flit->get_vnet()); 
              // if we could find a free vc in the vnet of suspended flit 
              if (vc != -1) {
                //remove the flit from the vc in congested_packets
                suspended_flit = congested_packets[k].getTopFlit();
                //insert the flit into niOutVcs[vc]
                niOutVcs[vc].insert(suspended_flit);
                //after inserting the flit, the state of the vc becomes active
                outVcState[vc].setState(ACTIVE_, curTick());
  
                //We need to send a credit back since we could insert into niOutVcs
                bool free_signal = false;
                if (suspended_flit->get_type() == HEAD_TAIL_) { free_signal = true; }
                
                Affirm *cFlit = new Affirm(suspended_flit->get_vc(), free_signal,
                                               curTick());
                //get the correct NetworkInport to send back credit
                int correct_inport = suspended_flit->get_inport_of_flit();
                //ensure correct_inport is valid
                assert(correct_inport >= 0 && correct_inport < ni_inports.size());
                //send back the credit to bus
                ni_inports[correct_inport]->sendCredit(cFlit);
              }          
          
          } else {
              // The flit is BODY or TAIL
              // Need to find out the right vc in niOutVcs to insert the flit
              for (auto it = niOutVcs.begin(); it != niOutVcs.end(); ++it) {
                  if (!it->isEmpty()) { // If the vc is not empty
                      // Peek the top flit
                      fragment* topFlit = it->peekTopFlit(); 
                      if (topFlit) { // Check if topFlit is not null
                          // If the packet_id is the same for both flits,
                          // they belong to the same packet
                          if (topFlit->getPacketID() == suspended_flit->getPacketID()) {
                              //remove the flit from the vc in congested_packets
                              suspended_flit = congested_packets[k].getTopFlit();
                              // Insert suspended_flit in the same vc
                              it->insert(suspended_flit);
                              // Update stats for this flit (not being ejected)
                              incrementStatsSpecial(suspended_flit);
  
                              //We need to send a credit back since we could insert into niOutVcs
                              bool free_signal = false;
                              if (suspended_flit->get_type() == TAIL_) { free_signal = true; }
                              
                              Affirm *cFlit = new Affirm(suspended_flit->get_vc(), free_signal,
                                                             curTick());
                            
                              //get the correct NetworkInport to send back credit
                              int correct_inport = suspended_flit->get_inport_of_flit();
                              //ensure correct_inport is valid
                              assert(correct_inport >= 0 && correct_inport < ni_inports.size());
                              //send back the credit to bus
                              ni_inports[correct_inport]->sendCredit(cFlit);
                          }
                          
                      } else {
                          std::cerr << "Warning: Top flit is null!" << std::endl;
                      }
                  }
              }     
          }
  
        }
        
        //remove flitBuffers with no flit inside from congested_packets vector
        for (auto it = congested_packets.begin(); it != congested_packets.end();) {
            if ((*it).isEmpty()) {
                it = congested_packets.erase(it);
            } else {
                ++it;
            }
        }

    }
  
    /**************** Check the incoming flit link ****************/
    DPRINTF(RubyNetwork, "Number of bus inports: %d\n", ni_inports.size());
    //To save the NetworkInport of a suspended_flit
    int inport_of_flit = 0; 
    for (auto &ni_inport: ni_inports) { //for every NI inport (from bus)
        //get the network link for that inport
        GridLink *inNetLink = ni_inport->inNetLink();
        //if the network link buffer has a ready flit at the current tick
        if (inNetLink->isReady(curTick())) {
            //consume that flit on the network link and put it in t_flit
            fragment *t_flit = inNetLink->consumeLink();
            //Save the NetworkInport the flit was consumed from its link
            t_flit->set_suspended_flit_inport(inport_of_flit); 
            //print the flit that was received by the NI
            DPRINTF(RubyNetwork, "Recieved flit:%s\n", *t_flit);
            //make sure the flit width and the bitWidth of the inport
            //are the same
            assert(t_flit->m_width == ni_inport->bitWidth());

            //get the vnet of t_flit
            int vnet = t_flit->get_vnet();

            //The flit that comes in from a NetworkInport is either for this
            //NetworkInterface or for another NI in this layer. In first case,
            //when the flit belongs to this NI, we need to eject just as we do 
            //when the flit comes in from an InputPort.
            if (t_flit->get_route().dest_ni == m_id) { 
              //set the flit dequeue time from FIFO to current_tick
              //(dequeue the flit)
              t_flit->set_dequeue_time(curTick());
              // If a tail flit is received, enqueue into the protocol buffers
              // if space is available. Otherwise, exchange non-tail flits for
              // credits.
              if (t_flit->get_type() == TAIL_ ||
                  t_flit->get_type() == HEAD_TAIL_) {
                  if (!ni_inport->messageEnqueuedThisCycle &&
                      outNode_ptr[vnet]->areNSlotsAvailable(1, curTime)) {
                      // Space is available. Enqueue to protocol buffer.
                      outNode_ptr[vnet]->enqueue(t_flit->get_msg_ptr(), curTime,
                                                 cyclesToTicks(Cycles(1)));
  
                      // Simply send a credit back since we are not buffering
                      // this flit in the NI
                      Affirm *cFlit = new Affirm(t_flit->get_vc(),
                                                 true, curTick());
                      //send the cFlit credit from NI to the network
                      ni_inport->sendCredit(cFlit);
                      // Update stats and delete flit pointer
                      incrementStats(t_flit);
                      delete t_flit;
                  } else {
                      // No space available- Place tail flit in stall queue and
                      // set up a callback for when protocol buffer is dequeued.
                      // Stat update and flit pointer deletion will occur upon
                      // unstall.
                      ////push the flit into stall queue
                      ni_inport->m_stall_queue.push_back(t_flit);
                      //increment the number of stalls for the vnet in bus
                      m_stall_count_bus[vnet]++;
  
                      //set up a callback for when protocol buffer is dequeued
                      outNode_ptr[vnet]->registerDequeueCallback([this]() {
                          dequeueCallback(); });
                  }
              } else { //HEAD or BODY flit
                  // Non-tail flit. Send back a credit but not VC free signal.
                  Affirm *cFlit = new Affirm(t_flit->get_vc(), false,
                                                 curTick());
                  // Simply send a credit back since we are not buffering
                  // this flit in the NI
                  ni_inport->sendCredit(cFlit);
  
                  // Update stats and delete flit pointer.
                  incrementStats(t_flit);
                  delete t_flit;
              }
     
            } else { 
              //But when a flit comes in from a NetworkInport and does not belong
              //to this NI, then it belongs to some other NI in the current layer
              //and we need to pass it to the router (just like when we do that
              //when the flit was produced in this NI).  

              //If the type of the flit is HEAD or HEAD_TAIL
              if (t_flit->get_type() == HEAD_ || t_flit->get_type() == HEAD_TAIL_) {
                  //find a free vc in destination vnet in niOutVcs
                  int vc = calculateVC(vnet);  
                  // if we could not find a free vc in niOutVcs
                  // we need to save in congested_packets flitBuffer
                  if (vc == -1) {
                    //create a new flitBuffer
                    fragmentBuffer *buff = new fragmentBuffer();
                    //insert the received flit into the created flitBuffer
                    buff->insert(t_flit);
                    //attach the flitBuffer to the congested_packets vector
                    congested_packets.push_back(*buff);
                    
                  } else { 
                    //we did find a free vc in niOutVcs, therefore
                    //we send the flit into the free vc in niOutVcs
  
                    // Update stats for this flit (not being ejected)
                    incrementStatsSpecial(t_flit);
                    //push into the free vc
                    niOutVcs[vc].insert(t_flit);
                    //after inserting the flit, the state of the vc becomes active
                    outVcState[vc].setState(ACTIVE_, curTick());
  
                    //We need to send a credit back since we could insert into niOutVcs
                    bool free_signal = false;
                    if (t_flit->get_type() == HEAD_TAIL_) { free_signal = true; }
                    
                    Affirm *cFlit = new Affirm(t_flit->get_vc(), free_signal,
                                                   curTick());
                    //send back the credit to bus
                    ni_inport->sendCredit(cFlit);
                  }
                
              } else {
                    //Type of the flit is BODY or TAIL
                    //In this case, we first need to find out whether the HEAD
                    //of the packet succeeded into niOutVcs or was sent to 
                    //congested_packets. Then, we need to find the specific vc 
                    //or flitBuffer to send the rest of the packet.
                    //We know that all the flits that belong to the same packet have
                    //the same unique packet_id; we use that to find our packet.
    
                    bool packet_found = false; 
                    // First traverse over the Vcs in niOutVcs
                    for (auto it = niOutVcs.begin(); it != niOutVcs.end(); ++it) {
                        if (!it->isEmpty()) { // If the vc is not empty
                            // Peek the top flit
                            fragment* topFlit = it->peekTopFlit(); 
                            if (topFlit) { // Check if topFlit is not null
                                // If the packet_id is the same for both flits,
                                // they belong to the same packet
                                if (topFlit->getPacketID() == t_flit->getPacketID()) {
                                    // The packet of this flit is found
                                    packet_found = true;
                                    // Insert t_flit in the same vc
                                    it->insert(t_flit);
                                    // Update stats for this flit (not being ejected)
                                    incrementStatsSpecial(t_flit);

                                    //We need to send a credit back since we could insert into niOutVcs
                                    bool free_signal = false;
                                    if (t_flit->get_type() == TAIL_) { free_signal = true; }
                                    
                                    Affirm *cFlit = new Affirm(t_flit->get_vc(), free_signal,
                                                                   curTick());
                                    //send back the credit to bus
                                    ni_inport->sendCredit(cFlit);
                                }
                            } else {
                                std::cerr << "Warning: Top flit is null!" << std::endl;
                            }
                        }
                    }
    
                    // If packet was not found in niOutVcs, need to check congested_packets
                    if (!packet_found) {
                        // traverse over the flitBuffers in congested_packets
                        for (auto it = congested_packets.begin(); it != congested_packets.end(); ++it) {
                            if (!it->isEmpty()) { // If the flitBuffer is not empty
                                // Peek the top flit
                                fragment* topFlit = it->peekTopFlit(); 
                                if (topFlit) { // Check if topFlit is not null
                                    // If the packet_id is the same for both flits,
                                    // they belong to the same packet
                                    if (topFlit->getPacketID() == t_flit->getPacketID()) {
                                        // The packet of this flit is found
                                        packet_found = true;
                                        // Insert t_flit in the same vc
                                        it->insert(t_flit);
                                    }
                                } else {
                                    std::cerr << "Warning: Top flit is null!" << std::endl;
                                }
                            }
                        }      
                    }
    
                    //If we have a BODY or TAIL flit and can't find its HEAD in either
                    //niOutVcs or congested_packets, then there is a problem with logic
                    if (!packet_found) {
                        fatal("Could not find the packet associated with the BODY or TAIL flit.");
                    }
                }
                
            } 
        }

        // Increment the index
        ++inport_of_flit;
    }

    /**************** Check the incoming credit link ****************/
    for (auto &ni_outport: ni_outports) { //for every NI outport (to bus)
        //get the credit link for that outport
        AffirmLink *inCreditLink = ni_outport->inCreditLink();
        //if that credit link has a ready flit at current tick
        if (inCreditLink->isReady(curTick())) {
            //consume that flit on the credit link and put it in t_credit
            Affirm *t_credit = (Affirm*) inCreditLink->consumeLink();
            //increment credit (free space) for the vc of t_credit in
            //outVcState vector (It means that the downstream router got
            //and consumed the flit that we sent and now that vc has another
            //free slot or credit for us to send more)
            toBusVcState[t_credit->get_vc()].increment_credit();
            //if is_free_signal is true (meaning vc got totally free)
            if (t_credit->is_free_signal()) {
                //change the state for that vc from active to idle
                //at current_tick
                toBusVcState[t_credit->get_vc()].setState(IDLE_,
                    curTick());
            }
            //delete t_credit variable
            delete t_credit;
        }
    }

    // It is possible to enqueue multiple outgoing credit flits if a message
    // was unstalled in the same cycle as a new message arrives. In this
    // case, we should schedule another wakeup to ensure the credit is sent
    // back.
    for (auto &ni_inport: ni_inports) { //for every inport (from bus)
        //if we have more than one credit in the inport credit queue
        if (ni_inport->outCreditQueue()->getSize() > 0) {
            //print the credit flit we are sending back, the credit queue
            //we are sending from, and the time we are sending it
            DPRINTF(RubyNetwork, "Sending a credit %s via %s at %ld\n",
            *(ni_inport->outCreditQueue()->peekTopFlit()),
            ni_inport->outCreditLink()->name(), clockEdge(Cycles(1)));
            //the credit link of the inport should consume the credit flit
            //in the next clock edge
            ni_inport->outCreditLink()->
                scheduleEventAbsolute(clockEdge(Cycles(1)));
        }
    }
  //========================================================================
  //========================================================================
    checkReschedule();
}

// Check if there are fragments stalling a virtual channel. Track if a
// message is enqueued to restrict ejection to one message per cycle.
void
InterfaceUnit::checkStallQueue()
{
    // Check all stall queues.
    // There is one stall queue for each input link
    for (auto &iPort: inPorts) { //for every inport
        //the inport message was not enqueued this cycle
        iPort->messageEnqueuedThisCycle = false;
        //get the tick where the current cycle begins
        Tick curTime = clockEdge();

        //if the stall queue for the inport is not empty
        if (!iPort->m_stall_queue.empty()) {
            //go through all the elements in the inport stall queue
            for (auto stallIter = iPort->m_stall_queue.begin();
                 stallIter != iPort->m_stall_queue.end(); ) {
                //get the stalled fragment and save it to stallFlit variable
                fragment *stallFlit = *stallIter;
                //get the vnet of that stalled fragment
                int vnet = stallFlit->get_vnet();

                // If we can now eject to the protocol buffer,
                // send back credits
                //if there is 1 slot available in the vnet of the stalled fragment
                //(the vnet the fragment wants to go to)
                if (outNode_ptr[vnet]->areNSlotsAvailable(1,
                    curTime)) {
                    //eject to the protocol buffer (enqueue the fragment into
                    //the outNode_ptr[vnet] after one cycle delay)
                    outNode_ptr[vnet]->enqueue(stallFlit->get_msg_ptr(),
                        curTime, cyclesToTicks(Cycles(1)));

                    // Send back a credit with free signal now that the
                    // VC is no longer stalled.
                    Affirm *cFlit = new Affirm(stallFlit->get_vc(), true,
                                                   curTick());
                    //send the credit fragment to the upstream router
                    iPort->sendCredit(cFlit);

                    // Update Stats
                    incrementStats(stallFlit);

                    // Flit can now safely be deleted and removed from stall
                    // queue
                    delete stallFlit; //delete stallFlit variable
                    //erase the ejected fragment from m_stall_queue
                    iPort->m_stall_queue.erase(stallIter);
                    //decrement the number of stalled messages for this vnet
                    m_stall_count[vnet]--;

                    // If there are no more stalled messages for this vnet, the
                    // callback on it's MessageBuffer is not needed.
                    if (m_stall_count[vnet] == 0 && m_stall_count_bus[vnet] == 0)
                        outNode_ptr[vnet]->unregisterDequeueCallback();

                    //the inport message was enqueued this cycle
                    iPort->messageEnqueuedThisCycle = true;
                    break;
                } else { //no empty slot is available in outNode_ptr[vnet]
                    ++stallIter; //go to the next fragment in iPort->m_stall_queue
                }
            }
        }
    }

    //=======================================================================
    //=======================================================================
    // Check all stall queues. This is for bus-specific inports and we are 
    // basically checking if some flits that wanted to be ejected from these
    // ports were stalled (just like we do so with NI regular inports).
    // There is one stall queue for each input link
    for (auto &ni_inport: ni_inports) { //for every inport
        //the inport message was not enqueued this cycle
        ni_inport->messageEnqueuedThisCycle = false;
        //get the tick where the current cycle begins
        Tick curTime = clockEdge();

        //if the stall queue for the inport is not empty
        if (!ni_inport->m_stall_queue.empty()) {
            //go through all the elements in the inport stall queue
            for (auto stallIter = ni_inport->m_stall_queue.begin();
                 stallIter != ni_inport->m_stall_queue.end(); ) {
                //get the stalled flit and save it to stallFlit variable
                fragment *stallFlit = *stallIter;
                //get the vnet of that stalled flit
                int vnet = stallFlit->get_vnet();

                // If we can now eject to the protocol buffer,
                // send back credits
                //if there is 1 slot available in the vnet of the stalled flit
                //(the vnet the flit wants to go to)
                if (outNode_ptr[vnet]->areNSlotsAvailable(1,
                    curTime)) {
                    //eject to the protocol buffer (enqueue the flit into
                    //the outNode_ptr[vnet] after one cycle delay)
                    outNode_ptr[vnet]->enqueue(stallFlit->get_msg_ptr(),
                        curTime, cyclesToTicks(Cycles(1)));

                    // Send back a credit with free signal now that the
                    // VC is no longer stalled.
                    Affirm *cFlit = new Affirm(stallFlit->get_vc(), true,
                                                   curTick());
                    //send the credit flit to the upstream router
                    ni_inport->sendCredit(cFlit);

                    // Update Stats
                    incrementStats(stallFlit);

                    // Flit can now safely be deleted and removed from stall
                    // queue
                    delete stallFlit; //delete stallFlit variable
                    //erase the ejected flit from m_stall_queue
                    ni_inport->m_stall_queue.erase(stallIter);
                    //decrement the number of stalled messages for this vnet
                    m_stall_count_bus[vnet]--;

                    // If there are no more stalled messages for this vnet, the
                    // callback on it's MessageBuffer is not needed.
                    if (m_stall_count[vnet] == 0 && m_stall_count_bus[vnet] == 0)
                        outNode_ptr[vnet]->unregisterDequeueCallback();

                    //the inport message was enqueued this cycle
                    ni_inport->messageEnqueuedThisCycle = true;
                    break;
                } else { //no empty slot is available in outNode_ptr[vnet]
                    ++stallIter; //go to the next flit in iPort->m_stall_queue
                }
            }
        }
    }
    //=======================================================================
    //=======================================================================
}

//=======================================================================
//=======================================================================
//Find the layer of a router based on its id
int
InterfaceUnit::get_destination_layer(int router_id)
{
    int num_rows = m_net_ptr->getNumRows();
    int num_cols = m_net_ptr->getNumCols();
    int num_layers = m_net_ptr->getNumLayers();
    assert(num_rows > 0 && num_cols > 0 && num_layers > 0);
    //number of routers or RLIs per layer
    int num_routers_layer = num_rows * num_cols;
    if (num_layers > 1) { return floor(router_id/num_routers_layer); }
    //return 0 if we only have one layer
    return 0;
}
//=======================================================================
//=======================================================================

// Embed the protocol message into fragments
// bool
// InterfaceUnit::flitisizeMessage(MsgPtr msg_ptr, int vnet)
// {
//     //get a pointer to the protocol message we want to flitisize
//     Message *net_msg_ptr = msg_ptr.get();
//     //get the destination of this message
//     NetDest net_msg_dest = net_msg_ptr->getDestination();

//     // gets all the destinations associated with this message.
//     std::vector<NodeID> dest_nodes = net_msg_dest.getAllDest();

//     // Number of fragments is dependent on the link bandwidth available.
//     // This is expressed in terms of bytes/cycle or the fragment size
//     OutputPort *oPort = getOutportForVnet(vnet);
//     assert(oPort); //make sure the outport for the vnet exists
  
//     //================================================================
//     //================================================================
//     //get the correct bus outport
//     NetworkOutport *ni_outport = getNetworkOutportForVnet(vnet);
//     assert(ni_outport); //make sure the outport for the vnet exists
//     //================================================================
//     //================================================================
  
//     // Number of flits is dependent on the link bandwidth available.
//     // This is expressed in terms of bytes/cycle or the flit size
//     //calculate how many flits is needed (messageSize/link_bitWidth)
//     int num_flits = (int)divCeil((float) m_net_ptr->MessageSizeType_to_int(
//         net_msg_ptr->getMessageSize()), (float)oPort->bitWidth());

//     //printing the message size, vnet, and the link bandwidth
//     DPRINTF(RubyNetwork, "Message Size:%d vnet:%d bitWidth:%d\n",
//         m_net_ptr->MessageSizeType_to_int(net_msg_ptr->getMessageSize()),
//         vnet, oPort->bitWidth());

//     // loop to convert all multicast messages into unicast messages
//     for (int ctr = 0; ctr < dest_nodes.size(); ctr++) {

//         // this will return a free output virtual channel
//         int vc = calculateVC(vnet); //find a free vc in dest vnet

//         //copy the msg_ptr into new_msg_ptr variable
//         MsgPtr new_msg_ptr = msg_ptr->clone();
//         //get the destination node id
//         NodeID destID = dest_nodes[ctr];

//         //==============================================================
//         //==============================================================
//         //find a free vc in dest vnet for bus
//         int bus_vc = calculateBusVC(vnet); 
//         //get the current router_id
//         int current_router = oPort->routerID();
//         //get the dest router_id 
//         int destination_router = m_net_ptr->get_router_id(destID, vnet);
//         //get the layer of the current router
//         int current_layer = get_destination_layer(current_router);
//         //get the layer of the destination router
//         int destination_layer = get_destination_layer(destination_router);
//         //whether the message destination is this layer
//         bool this_layer = (current_layer == destination_layer);
//         //If the destination is this layer and we don't have vc in OutputPort,
//         //or the destination is not this layer and don't have vc in NetworkOutport,
//         //then we can't flitisize the message
//         if ((vc == -1 && this_layer) || (bus_vc == -1 && !this_layer)) {
//             return false ;
//         }
//         //==============================================================
//         //==============================================================
      
//         //get a pointer to new_msg_ptr
//         Message *new_net_msg_ptr = new_msg_ptr.get();
//         //if we have more than one destination for this message
//         //(multicast message)
//         if (dest_nodes.size() > 1) {
//             //define a NetDest
//             NetDest personal_dest;
//             for (int m = 0; m < (int) MachineType_NUM; m++) {
//                 if ((destID >= MachineType_base_number((MachineType) m)) &&
//                     destID < MachineType_base_number((MachineType) (m+1))) {
//                     // calculating the NetDest associated with this destID
//                     personal_dest.clear();
//                     personal_dest.add((MachineID) {(MachineType) m, (destID -
//                         MachineType_base_number((MachineType) m))});
//                     new_net_msg_ptr->getDestination() = personal_dest;
//                     break;
//                 }
//             }
//             net_msg_dest.removeNetDest(personal_dest);
//             // removing the destination from the original message to reflect
//             // that a message with this particular destination has been
//             // flitisized and an output vc is acquired
//             net_msg_ptr->getDestination().removeNetDest(personal_dest);
//         }

//         // Embed Route into the flits
//         // NetDest format is used by the routing table
//         // Custom routing algorithms just need destID

//         RouteInfo route; //for embedding route info into the flits
//         route.vnet = vnet; //set the vnet
//         route.net_dest = new_net_msg_ptr->getDestination(); //set the NetDest
//         route.src_ni = m_id; //set the src NetworkInterface
//         route.src_router = oPort->routerID(); //set the src router_id
//         route.dest_ni = destID; //set the dest NetworkInterface
//         //set the dest router_id
//         route.dest_router = m_net_ptr->get_router_id(destID, vnet);

//         // initialize hops_traversed to -1
//         // so that the first router increments it to 0
//         route.hops_traversed = -1;

//         //a packet was injected into the vnet in the EmeraldNetwork
//         m_net_ptr->increment_injected_packets(vnet);
//         //Keep track of the data traffic and control traffic
//         m_net_ptr->update_traffic_distribution(route);
//         int packet_id = m_net_ptr->getNextPacketID();

//         //=======================================================================
//         //=======================================================================
//         //if the destination layer of the message is the current layer
//         if (this_layer) {
//           for (int i = 0; i < num_flits; i++) {
//               //a flit was injected into the vnet in the EmeraldNetwork
//               m_net_ptr->increment_injected_flits(vnet);
//               //create a new flit and fill its fields with appropriate data
//               fragment *fl = new fragment(packet_id,
//                   i, vc, vnet, route, num_flits, new_msg_ptr,
//                   m_net_ptr->MessageSizeType_to_int(
//                   net_msg_ptr->getMessageSize()),
//                   oPort->bitWidth(), curTick());
  
//               //the src delay for the flit is the current_tick - msg_ptr_time
//               fl->set_src_delay(curTick() - msg_ptr->getTime());
//               //insert the created flit into the right vc in NI
//               niOutVcs[vc].insert(fl);
//           }
  
//           //the enqueue time in the vc is the current tick
//           m_ni_out_vcs_enqueue_time[vc] = curTick();
//           //after inserting the flit, the state of the vc becomes active
//           outVcState[vc].setState(ACTIVE_, curTick());          
        
//         } else { //the destination of the message is another layer
//           for (int i = 0; i < num_flits; i++) {
//               //a flit was injected into the vnet in the EmeraldNetwork
//               m_net_ptr->increment_injected_flits(vnet);
//               //create a new flit and fill its fields with appropriate data
//               fragment *fl = new fragment(packet_id,
//                   i, bus_vc, vnet, route, num_flits, new_msg_ptr,
//                   m_net_ptr->MessageSizeType_to_int(
//                   net_msg_ptr->getMessageSize()),
//                   ni_outport->bitWidth(), curTick());
  
//               //the src delay for the flit is the current_tick - msg_ptr_time
//               fl->set_src_delay(curTick() - msg_ptr->getTime());
//               //insert the created flit into the right vc in NI
//               toBusVcs[vc].insert(fl);
//           }
  
//           //the enqueue time in the vc is the current tick
//           m_to_bus_vcs_enqueue_time[vc] = curTick();
//           //after inserting the flit, the state of the vc becomes active
//           toBusVcState[vc].setState(ACTIVE_, curTick());            
//         }
//         //=======================================================================
//         //=======================================================================
      
//     }
//     return true ;
// }



bool
InterfaceUnit::flitisizeMessage(MsgPtr msg_ptr, int vnet)
{
    //get a pointer to the protocol message we want to flitisize
    Message *net_msg_ptr = msg_ptr.get();
    //get the destination of this message
    NetDest net_msg_dest = net_msg_ptr->getDestination();

    // gets all the destinations associated with this message.
    std::vector<NodeID> dest_nodes = net_msg_dest.getAllDest();

    //==============================================================
    std::cout << "All the destinations of this Message are: (in InterfaceUnit.cc)\n";
    for(const auto& node : dest_nodes) {
        std::cout << node << " ";
    }
    std::cout << std::endl;
    //==============================================================


    // Number of flits is dependent on the link bandwidth available.
    // This is expressed in terms of bytes/cycle or the flit size
    OutputPort *oPort = getOutportForVnet(vnet);
    assert(oPort); //make sure the outport for the vnet exists
    //calculate how many flits is needed (messageSize/link_bitWidth)
    int num_flits = (int)divCeil((float) m_net_ptr->MessageSizeType_to_int(
        net_msg_ptr->getMessageSize()), (float)oPort->bitWidth());

    //==============================================================
    std::cout << "Number of destinations for this Message are: " << dest_nodes.size() <<" (in InterfaceUnit.cc)\n";
    std::cout << "Number of flits for this Message are: " << num_flits <<" (in InterfaceUnit.cc)\n";
    //==============================================================

    //printing the message size, vnet, and the link bandwidth
    DPRINTF(RubyNetwork, "Message Size:%d vnet:%d bitWidth:%d\n",
        m_net_ptr->MessageSizeType_to_int(net_msg_ptr->getMessageSize()),
        vnet, oPort->bitWidth());

    // loop to convert all multicast messages into unicast messages
    for (int ctr = 0; ctr < dest_nodes.size(); ctr++) {

        // this will return a free output virtual channel
        int vc = calculateVC(vnet); //find a free vc in dest vnet

        //no free vc was found, so we can't flitisize the message
        if (vc == -1) {
            return false ;
        }
        //copy the msg_ptr into new_msg_ptr variable
        MsgPtr new_msg_ptr = msg_ptr->clone();
        //get the destination node id
        NodeID destID = dest_nodes[ctr];

        //get a pointer to new_msg_ptr
        Message *new_net_msg_ptr = new_msg_ptr.get();
        //if we have more than one destination for this message
        //(multicast message)
        if (dest_nodes.size() > 1) {
            //define a NetDest
            NetDest personal_dest;
            for (int m = 0; m < (int) MachineType_NUM; m++) {
                if ((destID >= MachineType_base_number((MachineType) m)) &&
                    destID < MachineType_base_number((MachineType) (m+1))) {
                    // calculating the NetDest associated with this destID
                    personal_dest.clear();
                    personal_dest.add((MachineID) {(MachineType) m, (destID -
                        MachineType_base_number((MachineType) m))});
                    new_net_msg_ptr->getDestination() = personal_dest;
                    break;
                }
            }
            net_msg_dest.removeNetDest(personal_dest);
            // removing the destination from the original message to reflect
            // that a message with this particular destination has been
            // flitisized and an output vc is acquired
            net_msg_ptr->getDestination().removeNetDest(personal_dest);
        }

        // Embed Route into the flits
        // NetDest format is used by the routing table
        // Custom routing algorithms just need destID

        RouteInfo route; //for embedding route info into the flits
        route.vnet = vnet; //set the vnet
        route.net_dest = new_net_msg_ptr->getDestination(); //set the NetDest
        route.src_ni = m_id; //set the src NetworkInterface
        route.src_router = oPort->routerID(); //set the src router_id
        route.dest_ni = destID; //set the dest NetworkInterface
        //set the dest router_id
        route.dest_router = m_net_ptr->get_router_id(destID, vnet);

        // initialize hops_traversed to -1
        // so that the first router increments it to 0
        route.hops_traversed = -1;

        //==============================================================
        std::cout << "Information of this flit: (in InterfaceUnit.cc)\n";
        std::cout << "Source NI: NI" << route.src_ni <<" (in WayFinder.cc)\n";
        std::cout << "Destination NI: NI" << route.dest_ni <<" (in WayFinder.cc)\n";
        std::cout << "Source router: R" << route.src_router <<" (in WayFinder.cc)\n";
        std::cout << "Destination router: R" << route.dest_router <<" (in WayFinder.cc)\n";
        //==============================================================

        //a packet was injected into the vnet in the GarnetNetwork
        m_net_ptr->increment_injected_packets(vnet);
        //Keep track of the data traffic and control traffic
        m_net_ptr->update_traffic_distribution(route);
        int packet_id = m_net_ptr->getNextPacketID();
        for (int i = 0; i < num_flits; i++) {
            //a flit was injected into the vnet in the GarnetNetwork
            m_net_ptr->increment_injected_flits(vnet);
            //create a new flit and fill its fields with appropriate data
            fragment *fl = new fragment(packet_id,
                i, vc, vnet, route, num_flits, new_msg_ptr,
                m_net_ptr->MessageSizeType_to_int(
                net_msg_ptr->getMessageSize()),
                oPort->bitWidth(), curTick());

            //the src delay for the flit is the current_tick - msg_ptr_time
            fl->set_src_delay(curTick() - msg_ptr->getTime());
            //insert the created flit into the right vc in NI
            niOutVcs[vc].insert(fl);
        }

        //the enqueue time in the vc is the current tick
        m_ni_out_vcs_enqueue_time[vc] = curTick();
        //after inserting the flit, the state of the vc becomes active
        outVcState[vc].setState(ACTIVE_, curTick());
    }
    return true ;
}




// Looking for a free output vc
int
InterfaceUnit::calculateVC(int vnet)
{
    for (int i = 0; i < m_vc_per_vnet; i++) {
        int delta = m_vc_allocator[vnet];
        m_vc_allocator[vnet]++;
        if (m_vc_allocator[vnet] == m_vc_per_vnet)
            m_vc_allocator[vnet] = 0;

        if (outVcState[(vnet*m_vc_per_vnet) + delta].isInState(
                    IDLE_, curTick())) {
            vc_busy_counter[vnet] = 0;
            return ((vnet*m_vc_per_vnet) + delta);
        }
    }

    vc_busy_counter[vnet] += 1;
    panic_if(vc_busy_counter[vnet] > m_deadlock_threshold,
        "%s: Possible network deadlock in vnet: %d at time: %llu \n",
        name(), vnet, curTick());

    return -1;
}

//=================================================================
//=================================================================
// Looking for a free output vc in NetworkOutport for vnet
int
InterfaceUnit::calculateBusVC(int vnet)
{
    for (int i = 0; i < m_vc_per_vnet; i++) {
        int delta = m_bus_vc_allocator[vnet];
        m_bus_vc_allocator[vnet]++;
        if (m_bus_vc_allocator[vnet] == m_vc_per_vnet)
            m_bus_vc_allocator[vnet] = 0;

        if (toBusVcState[(vnet*m_vc_per_vnet) + delta].isInState(
                    IDLE_, curTick())) {
            bus_vc_busy_counter[vnet] = 0;
            return ((vnet*m_vc_per_vnet) + delta);
        }
    }

    bus_vc_busy_counter[vnet] += 1;
    panic_if(bus_vc_busy_counter[vnet] > m_deadlock_threshold,
        "%s: Possible network deadlock in vnet: %d in Bus at time: %llu \n",
        name(), vnet, curTick());

    return -1;
}
//=================================================================
//=================================================================

//choose a vc from the outport in a round-robin manner
void
InterfaceUnit::scheduleOutputPort(OutputPort *oPort)
{
    //choose a vc from the given outport using round-robin
    int vc = oPort->vcRoundRobin();

    //go through all out VCs of the NI
    for (int i = 0; i < niOutVcs.size(); i++) { //for the size of niOutVcs
        vc++; //go to the next vc
        //if you reach the end, start again from the beginning (round-robin)
        if (vc == niOutVcs.size())
            vc = 0;

        //get the vnet for the vc
        int t_vnet = get_vnet(vc);
        //if the given outport supports this vnet
        if (oPort->isVnetSupported(t_vnet)) {
            // model buffer backpressure
            //if vc is ready and has credit
            if (niOutVcs[vc].isReady(curTick()) &&
                outVcState[vc].has_credit()) {
                //then this vc is a candidate
                bool is_candidate_vc = true;
                //the first vc in the vnet
                int vc_base = t_vnet * m_vc_per_vnet;

                //if t_vnet is ordered
                if (m_net_ptr->isVNetOrdered(t_vnet)) {
                    //go through all vcs in t_vnet
                    //trying to find the vc in the vnet with least enqueue_time
                    for (int vc_offset = 0; vc_offset < m_vc_per_vnet;
                         vc_offset++) {
                        int t_vc = vc_base + vc_offset;
                        //if t_vnet is ready at the current tick
                        if (niOutVcs[t_vc].isReady(curTick())) {
                            //if the enqueue time for t_vc is less than vc
                            if (m_ni_out_vcs_enqueue_time[t_vc] <
                                m_ni_out_vcs_enqueue_time[vc]) {
                                //then vc is not a candidate
                                is_candidate_vc = false;
                                break;
                            }
                        }
                    }
                }
                //if vc is not a candidate, move on
                if (!is_candidate_vc)
                    continue;

                // Update the round robin arbiter
                oPort->vcRoundRobin(vc);

                //one less free slot in vc
                outVcState[vc].decrement_credit();

                // Just removing the top flit
                fragment *t_flit = niOutVcs[vc].getTopFlit();
                //the fragment will traverse the link in the next cycle
                t_flit->set_time(clockEdge(Cycles(1)));

                // Scheduling the fragment
                scheduleFlit(t_flit);

                //if the type of t_flit is TAIL_ or HEAD_TAIL_
                if (t_flit->get_type() == TAIL_ ||
                   t_flit->get_type() == HEAD_TAIL_) {
                    //then enqueue time for vc is infinite
                    m_ni_out_vcs_enqueue_time[vc] = Tick(INFINITE_);
                }

                // Done with this port, continue to schedule
                // other ports
                return;
            }
        }
    }
}

//=================================================================
//=================================================================
//choose a vc from NetworkOutport in a round-robin manner
void
InterfaceUnit::scheduleBusOutport(NetworkOutport *oPort)
{
    //choose a vc from the given outport using round-robin
    int vc = oPort->vcRoundRobin();

    //go through all out VCs of the NI
    for (int i = 0; i < toBusVcs.size(); i++) { //for the size of toBusVcs
        vc++; //go to the next vc
        //if you reach the end, start again from the beginning (round-robin)
        if (vc == toBusVcs.size())
            vc = 0;

        //get the vnet for the vc
        int t_vnet = get_vnet(vc);
        //if the given outport supports this vnet
        if (oPort->isVnetSupported(t_vnet)) {
            // model buffer backpressure
            //if vc is ready and has credit
            if (toBusVcs[vc].isReady(curTick()) &&
                toBusVcState[vc].has_credit()) {
                //then this vc is a candidate
                bool is_candidate_vc = true;
                //the first vc in the vnet
                int vc_base = t_vnet * m_vc_per_vnet;

                //if t_vnet is ordered
                if (m_net_ptr->isVNetOrdered(t_vnet)) {
                    //go through all vcs in t_vnet
                    //trying to find the vc in the vnet with least enqueue_time
                    for (int vc_offset = 0; vc_offset < m_vc_per_vnet;
                         vc_offset++) {
                        int t_vc = vc_base + vc_offset;
                        //if t_vnet is ready at the current tick
                        if (toBusVcs[t_vc].isReady(curTick())) {
                            //if the enqueue time for t_vc is less than vc
                            if (m_to_bus_vcs_enqueue_time[t_vc] <
                                m_to_bus_vcs_enqueue_time[vc]) {
                                //then vc is not a candidate
                                is_candidate_vc = false;
                                break;
                            }
                        }
                    }
                }
                //if vc is not a candidate, move on
                if (!is_candidate_vc)
                    continue;

                // Update the round robin arbiter
                oPort->vcRoundRobin(vc);

                //one less free slot in vc
                toBusVcState[vc].decrement_credit();

                // Just removing the top flit
                fragment *t_flit = toBusVcs[vc].getTopFlit();
                //the flit will traverse the link in the next cycle
                t_flit->set_time(clockEdge(Cycles(1)));

                // Scheduling the flit to be sent to bus
                scheduleBusFlit(t_flit);

                //if the type of t_flit is TAIL_ or HEAD_TAIL_
                if (t_flit->get_type() == TAIL_ ||
                   t_flit->get_type() == HEAD_TAIL_) {
                    //then enqueue time for vc is infinite
                    m_to_bus_vcs_enqueue_time[vc] = Tick(INFINITE_);
                }
                // Done with this port, continue to schedule
                // other ports
                return;
            }
        }
    }
}
//=================================================================
//=================================================================


/** This function looks at the NI buffers
 *  if some buffer has fragments which are ready to traverse the link in the next
 *  cycle, and the downstream output vc associated with this fragment has buffers
 *  left, the link is scheduled for the next cycle
 */
void
InterfaceUnit::scheduleOutputLink()
{
    // Schedule each output link
    for (auto &oPort: outPorts) { //for each NI outport
        scheduleOutputPort(oPort); //schedule that outport
    }
}

//=====================================================
//=====================================================
void
InterfaceUnit::scheduleBusOutputLink()
{
    // Schedule each bus output link
    for (auto &ni_outport: ni_outports) { //for each NI bus outport
        scheduleBusOutport(ni_outport); //schedule that outport
    }
}
//=====================================================
//=====================================================

//get the inport for the given vnet
InterfaceUnit::InputPort *
InterfaceUnit::getInportForVnet(int vnet)
{
    for (auto &iPort : inPorts) { //for each NI inport
        //if that inport supports vnet
        if (iPort->isVnetSupported(vnet)) {
            return iPort; //that is our inport
        }
    }

    //if no inport in the NI has that vnet number
    return nullptr;
}

//=================================================================
//=================================================================
//get the bus inport for the given vnet
InterfaceUnit::NetworkInport *
InterfaceUnit::getNetworkInportForVnet(int vnet)
{
    for (auto &ni_inport : ni_inports) { //for each NI inport
        //if that inport supports vnet
        if (ni_inport->isVnetSupported(vnet)) {
            return ni_inport; //that is our inport
        }
    }
    //if no inport in the NI has that vnet number
    return nullptr;
}
//=================================================================
//=================================================================

/*
 * This function returns the outport which supports the given vnet.
 * Currently, HeteroEmerald does not support multiple outports to
 * support same vnet. Thus, this function returns the first-and
 * only outport which supports the vnet.
 */
InterfaceUnit::OutputPort *
InterfaceUnit::getOutportForVnet(int vnet)
{
    for (auto &oPort : outPorts) { //for each NI outport
        //if that outport supports vnet
        if (oPort->isVnetSupported(vnet)) {
            return oPort; //that is our outport
        }
    }

    //if no outport in the NI has that vnet number
    return nullptr;
}

//=================================================================
//=================================================================
//get the bus outport for the given vnet
InterfaceUnit::NetworkOutport *
InterfaceUnit::getNetworkOutportForVnet(int vnet)
{
    for (auto &ni_outport : ni_outports) { //for each NI outport
        //if that outport supports vnet
        if (ni_outport->isVnetSupported(vnet)) {
            return ni_outport; //that is our outport
        }
    }
    //if no outport in the NI has that vnet number
    return nullptr;
}
//=================================================================
//=================================================================

//schedule a fragment to be sent from an NI output port
void
InterfaceUnit::scheduleFlit(fragment *t_flit)
{
    //get the outport associated with the vnet of t_flit
    OutputPort *oPort = getOutportForVnet(t_flit->get_vnet());

    if (oPort) { //if oPort is valid
        //t_flit will be sent through oPort network link after one cycle
        DPRINTF(RubyNetwork, "Scheduling at %s time:%ld fragment:%s Message:%s\n",
        oPort->outNetLink()->name(), clockEdge(Cycles(1)),
        *t_flit, *(t_flit->get_msg_ptr()));
        //insert t_flit in the outFlitQueue of oPort
        oPort->outFlitQueue()->insert(t_flit);
        //oPort network link will consume t_flit as soon as it comes from
        //oPort (in one cycle)
        oPort->outNetLink()->scheduleEventAbsolute(clockEdge(Cycles(1)));
        return;
    }

    //panic if oPort is not valid
    panic("No output port found for vnet:%d\n", t_flit->get_vnet());
    return;
}

//======================================================================
//======================================================================
//schedule a flit to be sent from an NI Bus outport
void
InterfaceUnit::scheduleBusFlit(fragment *t_flit)
{
    //get the outport associated with the vnet of t_flit
    NetworkOutport *oPort = getNetworkOutportForVnet(t_flit->get_vnet());

    if (oPort) { //if oPort is valid
        //t_flit will be sent through oPort network link after one cycle
        DPRINTF(RubyNetwork, "Scheduling at %s time:%ld flit:%s Message:%s\n",
        oPort->outNetLink()->name(), clockEdge(Cycles(1)),
        *t_flit, *(t_flit->get_msg_ptr()));
        //insert t_flit in the outFlitQueue of oPort
        oPort->outFlitQueue()->insert(t_flit);
        //oPort network link will consume t_flit as soon as it comes from
        //oPort (in one cycle)
        oPort->outNetLink()->scheduleEventAbsolute(clockEdge(Cycles(1)));
        return;
    }
    //panic if oPort is not valid
    panic("No output port found for vnet:%d\n", t_flit->get_vnet());
    return;
}
//======================================================================
//======================================================================

//get the vnet for a vc
int
InterfaceUnit::get_vnet(int vc)
{
    //for the size of the vnets
    for (int i = 0; i < m_virtual_networks; i++) {
        //if vc number is equal or greater than base_vc for vnet i,
        //and less than base_vc for vnet (i+1), then vc belongs to vnet i
        if (vc >= (i*m_vc_per_vnet) && vc < ((i+1)*m_vc_per_vnet)) {
            return i;
        }
    }
    //when vc doesn't belong to any vnet
    fatal("Could not determine vc");
}


// Wakeup the NI in the next cycle if there are waiting
// messages in the protocol buffer, or waiting fragments in the
// output VC buffer.
// Also check if we have to reschedule because of a clock period
// difference.
void
InterfaceUnit::checkReschedule()
{
    //for every MessageBuffer in inNode_ptr
    for (const auto& it : inNode_ptr) {
        if (it == nullptr) { //move on, if the MessageBuffer is null
            continue;
        }

        //check the MessageBuffer in the current clock edge for messages
        while (it->isReady(clockEdge())) { // Is there a message waiting
            scheduleEvent(Cycles(1)); //wake up the NI in the next cycle
            return;
        }
    }

    for (auto& ni_out_vc : niOutVcs) { //for every NI outvc
        //if that outvc is ready (has a message)
        if (ni_out_vc.isReady(clockEdge(Cycles(1)))) {
            scheduleEvent(Cycles(1)); //wake up the NI in the next cycle
            return;
        }
    }

    // Check if any input links have flits to be popped.
    // This can happen if the links are operating at
    // a higher frequency.
    for (auto &iPort : inPorts) { //for every inport in NI
        //get the network link coming into that inport
        GridLink *inNetLink = iPort->inNetLink();
        //if the network link has a ready flit
        if (inNetLink->isReady(curTick())) {
            scheduleEvent(Cycles(1)); //wake up the NI in the next cycle
            return;
        }
    }

    for (auto &oPort : outPorts) { //for every outport in NI
        //get the credit link coming into that outport
        AffirmLink *inCreditLink = oPort->inCreditLink();
        //if the credit link has a ready fragment
        if (inCreditLink->isReady(curTick())) {
            scheduleEvent(Cycles(1)); //wake up the NI in the next cycle
            return;
        }
    }
  
    //===================================================================
    //===================================================================
    for (auto& to_bus_vc : toBusVcs) { //for every NI to bus vc
        //if that outvc is ready (has a message)
        if (to_bus_vc.isReady(clockEdge(Cycles(1)))) {
            scheduleEvent(Cycles(1)); //wake up the NI in the next cycle
            return;
        }
    }
  
    for (auto &ni_inport : ni_inports) { //for every bus inport in NI
        //get the network link coming into that inport
        GridLink *inNetLink = ni_inport->inNetLink();
        //if the network link has a ready flit
        if (inNetLink->isReady(curTick())) {
            scheduleEvent(Cycles(1)); //wake up the NI in the next cycle
            return;
        }
    }

    for (auto &ni_outport : ni_outports) { //for every bus outport in NI
        //get the credit link coming into that outport
        AffirmLink *inCreditLink = ni_outport->inCreditLink();
        //if the credit link has a ready flit
        if (inCreditLink->isReady(curTick())) {
            scheduleEvent(Cycles(1)); //wake up the NI in the next cycle
            return;
        }
    }
    //===================================================================
    //===================================================================
}

//for printing the NI
void
InterfaceUnit::print(std::ostream& out) const
{
    out << "[Network Interface]";
}

bool
InterfaceUnit::functionalRead(Packet *pkt, WriteMask &mask)
{
    bool read = false;
    for (auto& ni_out_vc : niOutVcs) {
        if (ni_out_vc.functionalRead(pkt, mask))
            read = true;
    }

    for (auto &oPort: outPorts) {
        if (oPort->outFlitQueue()->functionalRead(pkt, mask))
            read = true;
    }

    //==========================================================
    //==========================================================
    for (auto& to_bus_vc : toBusVcs) {
        if (to_bus_vc.functionalRead(pkt, mask))
            read = true;
    }
  
    for (auto &ni_outport: ni_outports) {
        if (ni_outport->outFlitQueue()->functionalRead(pkt, mask))
            read = true;
    } 
    //==========================================================
    //==========================================================
  
    return read;
}

//updating niOutVcs and outport outFlitQueue fragments with the
//data from the packet. It returns the number of functional writes.
//niOutVcs ==> input fragmentBuffers that serve the consumer (NI)
//oPort->outFlitQueue ==> output fragmentBuffers for sending fragments to the network
uint32_t
InterfaceUnit::functionalWrite(Packet *pkt)
{
    uint32_t num_functional_writes = 0;
    for (auto& ni_out_vc : niOutVcs) { //for every outvc in NI
        num_functional_writes += ni_out_vc.functionalWrite(pkt);
    }

    for (auto &oPort: outPorts) { //for every outport in NI
        num_functional_writes += oPort->outFlitQueue()->functionalWrite(pkt);
    }

    //======================================================================
    //======================================================================
    for (auto& to_bus_vc : toBusVcs) { //for every bus vc in NI
        num_functional_writes += to_bus_vc.functionalWrite(pkt);
    }
  
    for (auto &ni_outport: ni_outports) { //for every bus outport in NI
        num_functional_writes += ni_outport->outFlitQueue()->functionalWrite(pkt);
    }
    //======================================================================
    //======================================================================
    return num_functional_writes;
}

} // namespace emerald
} // namespace ruby
} // namespace gem5
