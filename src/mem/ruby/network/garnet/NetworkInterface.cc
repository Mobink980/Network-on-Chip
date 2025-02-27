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


#include "mem/ruby/network/garnet/NetworkInterface.hh"

#include <cassert>
#include <cmath>

#include "base/cast.hh"
#include "debug/RubyNetwork.hh"
#include "mem/ruby/network/MessageBuffer.hh"
#include "mem/ruby/network/garnet/Credit.hh"
#include "mem/ruby/network/garnet/flitBuffer.hh"
#include "mem/ruby/slicc_interface/Message.hh"


//=====================================
#include <iostream>
//=====================================

namespace gem5
{

namespace ruby
{

namespace garnet
{

//NetworkInterface constructor
NetworkInterface::NetworkInterface(const Params &p)
  : ClockedObject(p), Consumer(this), m_id(p.id),
    m_virtual_networks(p.virt_nets), m_vc_per_vnet(0),
    m_vc_allocator(m_virtual_networks, 0),
    m_deadlock_threshold(p.garnet_deadlock_threshold),
    vc_busy_counter(m_virtual_networks, 0)
{
    //counting stall numbers for each vnet
    m_stall_count.resize(m_virtual_networks);
    //NI outvcs have no element in the beginning
    niOutVcs.resize(0);
}

//add an input port to the NetworkInterface
void
NetworkInterface::addInPort(NetworkLink *in_link,
                              CreditLink *credit_link)
{
    //instantiate a new input port
    InputPort *newInPort = new InputPort(in_link, credit_link);
    //push the newly created input port in inPorts vector
    inPorts.push_back(newInPort);
    //printing the input port that was added and its vnets
    DPRINTF(RubyNetwork, "Adding input port:%s with vnets %s\n",
    in_link->name(), newInPort->printVnets());

    //NetworkInterface is the consumer of the inport network link
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

//add an output port to the NetworkInterface
void
NetworkInterface::addOutPort(NetworkLink *out_link,
                             CreditLink *credit_link,
                             SwitchID router_id, uint32_t consumerVcs)
{
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
            NetworkLink *inNetLink = iPort->inNetLink();
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
    //NetworkInterface is the consumer of credit_link for outport
    credit_link->setLinkConsumer(this);
    //set the number of VCs per Vnet (e.g., 4) for
    //credit_link (outport credit link)
    credit_link->setVcsPerVnet(m_vc_per_vnet);
}

//add a node to the NetworkInterface (e.g., east, west, etc.)
void
NetworkInterface::addNode(std::vector<MessageBuffer *>& in,
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
            it->setConsumer(this);
        }
    }
}

//for enqueuing a stalled message into the MessageBuffer
//in the next cycle, after a message was dequeued this cycle
void
NetworkInterface::dequeueCallback()
{
    // An output MessageBuffer has dequeued something this cycle and there
    // is now space to enqueue a stalled message. However, we cannot wake
    // on the same cycle as the dequeue. Schedule a wake at the soonest
    // possible time (next cycle).
    scheduleEventAbsolute(clockEdge(Cycles(1)));
}

//incremet the stats for the NI and the flit
void
NetworkInterface::incrementStats(flit *t_flit)
{
    //get the vnet of the flit
    int vnet = t_flit->get_vnet();

    // Latency
    //increment the received flits for the vnet in GarnetNetwork
    m_net_ptr->increment_received_flits(vnet);

    // std::cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
    // std::cout<<"One flit received.\n";

    //**************************************************
    // if (t_flit->is_broadcast()) {
    //     std::cout<<"The received flit was from bus!\n";
    // }
    //**************************************************
    // std::cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
    //network delay = flit_dequeue_time - flit_enqueue_time - Ticks(1_cycle)
    Tick network_delay =
        t_flit->get_dequeue_time() -
        t_flit->get_enqueue_time() - cyclesToTicks(Cycles(1));
    //queuing delay at src node
    Tick src_queueing_delay = t_flit->get_src_delay();
    //queuing delay at dest node = current_tick - last time the flit was dequeued
    Tick dest_queueing_delay = (curTick() - t_flit->get_dequeue_time());
    //queueing_delay for the flit
    Tick queueing_delay = src_queueing_delay + dest_queueing_delay;

    //increment the flit network and queuing latency for the GarnetNetwork
    m_net_ptr->increment_flit_network_latency(network_delay, vnet);
    m_net_ptr->increment_flit_queueing_latency(queueing_delay, vnet);

    //if the flit is of type TAIL_ or HEAD_TAIL_
    if (t_flit->get_type() == TAIL_ || t_flit->get_type() == HEAD_TAIL_) {
        //increment the number of received packets for the GarnetNetwork
        m_net_ptr->increment_received_packets(vnet);
        //increment packet network latency for the GarnetNetwork
        m_net_ptr->increment_packet_network_latency(network_delay, vnet);
        //increment packet queuing latency for the GarnetNetwork
        m_net_ptr->increment_packet_queueing_latency(queueing_delay, vnet);
    }

    // Hops that the flit traversed
    m_net_ptr->increment_total_hops(t_flit->get_route().hops_traversed);
}

/*
 * The NI wakeup checks whether there are any ready messages in the protocol
 * buffer. If yes, it picks that up, flitisizes it into a number of flits and
 * puts it into an output buffer and schedules the output link. On a wakeup
 * it also checks whether there are flits in the input link. If yes, it picks
 * them up and if the flit is a tail, the NI inserts the corresponding message
 * into the protocol buffer. It also checks for credits being sent by the
 * downstream router.
 */

void
NetworkInterface::wakeup()
{
    //define an std::ostringstream
    std::ostringstream oss;
    //get the router_id and vnets for all the outports of the NI
    for (auto &oPort: outPorts) {
        oss << oPort->routerID() << "[" << oPort->printVnets() << "] ";
    }
    //for printing what NI waked up when
    DPRINTF(RubyNetwork, "Network Interface %d connected to router:%s "
            "woke up. Period: %ld\n", m_id, oss.str(), clockPeriod());

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

    // Check if there are flits stalling a virtual channel. Track if a
    // message is enqueued to restrict ejection to one message per cycle.
    checkStallQueue();

    /*********** Check the incoming flit link **********/
    DPRINTF(RubyNetwork, "Number of input ports: %d\n", inPorts.size());
    for (auto &iPort: inPorts) { //for every inport
        //get the network link for that inport
        NetworkLink *inNetLink = iPort->inNetLink();
        //if the network link buffer has a ready flit at the current tick
        if (inNetLink->isReady(curTick())) {
            //consume that flit on the network link and put it in t_flit
            flit *t_flit = inNetLink->consumeLink();
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
                    Credit *cFlit = new Credit(t_flit->get_vc(),
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
                Credit *cFlit = new Credit(t_flit->get_vc(), false,
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

    /****************** Check the incoming credit link *******/

    for (auto &oPort: outPorts) { //for every outport
        //get the credit link for that outport
        CreditLink *inCreditLink = oPort->inCreditLink();
        //if that credit link has a ready flit at current tick
        if (inCreditLink->isReady(curTick())) {
            //consume that flit on the credit link and put it in t_credit
            Credit *t_credit = (Credit*) inCreditLink->consumeLink();
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
            //in the next clock edge
            iPort->outCreditLink()->
                scheduleEventAbsolute(clockEdge(Cycles(1)));
        }
    }

    checkReschedule();
}

// Check if there are flits stalling a virtual channel. Track if a
// message is enqueued to restrict ejection to one message per cycle.
void
NetworkInterface::checkStallQueue()
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
                //get the stalled flit and save it to stallFlit variable
                flit *stallFlit = *stallIter;
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
                    Credit *cFlit = new Credit(stallFlit->get_vc(), true,
                                                   curTick());
                    //send the credit flit to the upstream router
                    iPort->sendCredit(cFlit);

                    // Update Stats
                    incrementStats(stallFlit);

                    // Flit can now safely be deleted and removed from stall
                    // queue
                    delete stallFlit; //delete stallFlit variable
                    //erase the ejected flit from m_stall_queue
                    iPort->m_stall_queue.erase(stallIter);
                    //decrement the number of stalled messages for this vnet
                    m_stall_count[vnet]--;

                    // If there are no more stalled messages for this vnet, the
                    // callback on it's MessageBuffer is not needed.
                    if (m_stall_count[vnet] == 0)
                        outNode_ptr[vnet]->unregisterDequeueCallback();

                    //the inport message was enqueued this cycle
                    iPort->messageEnqueuedThisCycle = true;
                    break;
                } else { //no empty slot is available in outNode_ptr[vnet]
                    ++stallIter; //go to the next flit in iPort->m_stall_queue
                }
            }
        }
    }
}

// Embed the protocol message into flits
bool
NetworkInterface::flitisizeMessage(MsgPtr msg_ptr, int vnet)
{
    //get a pointer to the protocol message we want to flitisize
    Message *net_msg_ptr = msg_ptr.get();
    //get the destination of this message
    NetDest net_msg_dest = net_msg_ptr->getDestination();

    // gets all the destinations associated with this message.
    std::vector<NodeID> dest_nodes = net_msg_dest.getAllDest();

    //==============================================================
    std::cout << "All the destinations of this Message are: (in NetworkInterface.cc)\n";
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
    std::cout << "Number of destinations for this Message are: " << dest_nodes.size() <<" (in NetworkInterface.cc)\n";
    std::cout << "Number of flits for this Message are: " << num_flits <<" (in NetworkInterface.cc)\n";
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
        std::cout << "Information of this flit: (in NetworkInterface.cc)\n";
        std::cout << "Source NI: NI" << route.src_ni <<" (in NetworkInterface.cc)\n";
        std::cout << "Destination NI: NI" << route.dest_ni <<" (in NetworkInterface.cc)\n";
        std::cout << "Source router: R" << route.src_router <<" (in NetworkInterface.cc)\n";
        std::cout << "Destination router: R" << route.dest_router <<" (in NetworkInterface.cc)\n";
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
            flit *fl = new flit(packet_id,
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
NetworkInterface::calculateVC(int vnet)
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

//choose a vc from the outport in a round-robin manner
void
NetworkInterface::scheduleOutputPort(OutputPort *oPort)
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
                flit *t_flit = niOutVcs[vc].getTopFlit();
                //the flit will traverse the link in the next cycle
                t_flit->set_time(clockEdge(Cycles(1)));

                // Scheduling the flit
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



/** This function looks at the NI buffers
 *  if some buffer has flits which are ready to traverse the link in the next
 *  cycle, and the downstream output vc associated with this flit has buffers
 *  left, the link is scheduled for the next cycle
 */
void
NetworkInterface::scheduleOutputLink()
{
    // Schedule each output link
    for (auto &oPort: outPorts) { //for each NI outport
        scheduleOutputPort(oPort); //schedule that outport
    }
}

//get the inport for the given vnet
NetworkInterface::InputPort *
NetworkInterface::getInportForVnet(int vnet)
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

/*
 * This function returns the outport which supports the given vnet.
 * Currently, HeteroGarnet does not support multiple outports to
 * support same vnet. Thus, this function returns the first-and
 * only outport which supports the vnet.
 */
NetworkInterface::OutputPort *
NetworkInterface::getOutportForVnet(int vnet)
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

//schedule a flit to be sent from an NI output port
void
NetworkInterface::scheduleFlit(flit *t_flit)
{
    //get the outport associated with the vnet of t_flit
    OutputPort *oPort = getOutportForVnet(t_flit->get_vnet());

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

//get the vnet for a vc
int
NetworkInterface::get_vnet(int vc)
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
// messages in the protocol buffer, or waiting flits in the
// output VC buffer.
// Also check if we have to reschedule because of a clock period
// difference.
void
NetworkInterface::checkReschedule()
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
        NetworkLink *inNetLink = iPort->inNetLink();
        //if the network link has a ready flit
        if (inNetLink->isReady(curTick())) {
            scheduleEvent(Cycles(1)); //wake up the NI in the next cycle
            return;
        }
    }

    for (auto &oPort : outPorts) { //for every outport in NI
        //get the credit link coming into that outport
        CreditLink *inCreditLink = oPort->inCreditLink();
        //if the credit link has a ready flit
        if (inCreditLink->isReady(curTick())) {
            scheduleEvent(Cycles(1)); //wake up the NI in the next cycle
            return;
        }
    }
}

//for printing the NI
void
NetworkInterface::print(std::ostream& out) const
{
    out << "[Network Interface]";
}

bool
NetworkInterface::functionalRead(Packet *pkt, WriteMask &mask)
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

    return read;
}

//updating niOutVcs and outport outFlitQueue flits with the
//data from the packet. It returns the number of functional writes.
//niOutVcs ==> input flitBuffers that serve the consumer (NI)
//oPort->outFlitQueue ==> output flitBuffers for sending flits to the network
uint32_t
NetworkInterface::functionalWrite(Packet *pkt)
{
    uint32_t num_functional_writes = 0;
    for (auto& ni_out_vc : niOutVcs) { //for every outvc in NI
        num_functional_writes += ni_out_vc.functionalWrite(pkt);
    }

    for (auto &oPort: outPorts) { //for every outport in NI
        num_functional_writes += oPort->outFlitQueue()->functionalWrite(pkt);
    }
    return num_functional_writes;
}

} // namespace garnet
} // namespace ruby
} // namespace gem5
