/*
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


#include "mem/ruby/network/onyx/BusSwitchManager.hh"

#include "debug/RubyNetwork.hh"
#include "mem/ruby/network/onyx/OnyxNetwork.hh"
#include "mem/ruby/network/onyx/BusInport.hh"
#include "mem/ruby/network/onyx/BusOutport.hh"
#include "mem/ruby/network/onyx/Bus.hh"

//==================================
#include <iostream>
//==================================

namespace gem5
{

namespace ruby
{

namespace onyx
{

//BusSwitchManager constructor
BusSwitchManager::BusSwitchManager(Bus *bus)
    : Consumer(bus)
{
    //set the bus for this SwitchAllocator
    m_bus = bus;
    //set the number of vcs
    m_num_vcs = m_bus->get_num_vcs();
    //set the number of vcs per vnet
    m_vc_per_vnet = m_bus->get_vc_per_vnet();

    //initialize input_arbiter & output_arbiter activity
    m_input_arbiter_activity = 0;
    m_output_arbiter_activity = 0;
}

//initializing SwitchAllocator class variables
void
BusSwitchManager::init()
{
    //get the number of bus inports
    m_num_inports = m_bus->get_num_inports();
    //get the number of bus outports
    m_num_outports = m_bus->get_num_outports();
    //set the size of m_round_robin_inport to the size of inports
    m_round_robin_inport.resize(m_num_outports);
    //set the size of m_round_robin_invc to the size of inports
    //(we choose one input vc from each inport)
    m_round_robin_invc.resize(m_num_inports);
    //===============================================================
    //set the size of m_has_request to the size of inports
    //(one request from each inport is sent to outports every cycle)
    m_has_request.resize(m_num_inports);
    //at first, there is no broadcast available
    broadcast_this_cycle = false;
    //initialize the inport of the broadcast
    m_inport_broadcast = -1;
    //initialize the vc of the broadcast
    m_vc_broadcast = -1;

    //===============================================================
    //set the size of m_vc_winners to the size of inports
    //(one vc wins in each inport)
    m_vc_winners.resize(m_num_inports);

    //initialize m_round_robin_invc, m_has_request, m_vc_winners
    for (int i = 0; i < m_num_inports; i++) {
        m_round_robin_invc[i] = 0;
        //=================================
        m_has_request[i] = false;
        //=================================
        m_vc_winners[i] = -1;
    }
    //initialize m_round_robin_inport
    for (int i = 0; i < m_num_outports; i++) {
        m_round_robin_inport[i] = 0;
    }
}

/*
 * The wakeup function of the SwitchAllocator performs a 2-stage
 * seperable switch allocation. At the end of the 2nd stage, a free
 * output VC is assigned to the winning flits of each output port.
 * There is no separate VCAllocator stage like the one in garnet1.0.
 * At the end of this function, the bus is rescheduled to wakeup
 * next cycle for peforming SA for any flits ready next cycle.
 */
void
BusSwitchManager::wakeup()
{
    arbitrate_inports(); // First stage of allocation
    arbitrate_outports(); // Second stage of allocation
    //Clear the request vector within the allocator at end of SA-II.
    //Was populated by SA-I.
    clear_request_vector();
    //Wakeup the bus next cycle to perform SA again
    //if there are flits ready.
    check_for_wakeup();
}

/*
 * SA-I (or SA-i) loops through all input VCs at every input port,
 * and selects one in a round robin manner.
 *    - For HEAD/HEAD_TAIL flits only selects an input VC whose output port
 *     has at least one free output VC.
 *    - For BODY/TAIL flits, only selects an input VC that has credits
 *      in its output VC.
 * Places a request for the output port from this input VC.
 */

void
BusSwitchManager::arbitrate_inports()
{
    bool shouldBreak = false;
    // std::cout << "Number of VCs: " <<m_num_vcs<< ".\n";
    // std::cout << "Number of Vnets: " <<m_bus->get_num_vnets()<< ".\n";
    std::cout << "Number of VCs per vnet: " <<m_vc_per_vnet<< ".\n";

    // Select a VC from each inport in a round robin manner
    // Independent arbiter at each input port
    for (int inport = 0; inport < m_num_inports && !shouldBreak; inport++) {
        //the round-robin pinter is on what vc at this inport
        int invc = m_round_robin_invc[inport];

        for (int invc_iter = 0; invc_iter < m_num_vcs  && !shouldBreak; invc_iter++) {
            //get the InputUnit with the inport number from
            //the bus this SwitchAllocator is a part of
            auto input_unit = m_bus->getInputUnit(inport);

            // std::cout << "+++++++++++++++++++++++++++++++++++++++++++++\n";
            // std::cout << "Checking invc " <<invc<< " from inport "<<inport<<" for broadcast.\n";
            // std::cout << "+++++++++++++++++++++++++++++++++++++++++++++\n";
            //if the top flit in invc in input_unit is currently
            //in SwitchAllocation stage
            if (input_unit->need_stage(invc, SA_, curTick())) {

                //==================================================
                //checking the type of flit for printing
                chunk *my_flit = input_unit->peekTopFlit(invc);
                std::cout << "Type of the flit in the invc is: " <<my_flit->get_type()<< "\n";
                //==================================================

                bool broadcast = true;
                //if the outvc vector is not empty for this invc, check them
                if (input_unit->is_outvc_allocated(invc)) {
                    for (int outport = 0; outport < m_num_outports; outport++) {
                        //get the outvc for this inport
                        int outvc = input_unit->get_broadcast_outvc(invc, outport);
                        //check whether we can send to this outvc in this outport
                        bool port_request = send_allowed(inport, invc, outport, outvc);
                        //if we can't send to one of the outports, then can't broadcast
                        if (!port_request) {
                            std::cout << "*********************************************\n";
                            std::cout << "Top flit in invc " <<invc<< " from inport "<<inport<<" cannot go to outvc " <<outvc<< " of outport " <<outport<<".\n";
                            std::cout << "*********************************************\n";
                            broadcast = false;
                        }
                    }
                } else {
                    //if the outvc vector for this invc is empty,
                    //then all the outvcs should be -1
                    for (int outport = 0; outport < m_num_outports; outport++) {
                        //get the outvc for this inport
                        int outvc = -1;
                        //check whether we can send to this outvc in this outport
                        bool port_request = send_allowed(inport, invc, outport, outvc);
                        //if we can't send to one of the outports, then can't broadcast
                        if (!port_request) {
                            std::cout << "*********************************************\n";
                            std::cout << "Top flit in invc " <<invc<< " from inport "<<inport<<" cannot find a free outvc in outport " <<outport<<".\n";
                            std::cout << "*********************************************\n";
                            broadcast = false;
                        }
                    }
                }


                if(broadcast) {
                    std::cout << "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n";
                    std::cout << "Top flit in invc " <<invc<< " from inport "<<inport<<" could go to all the outports.\n";
                    std::cout << "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n";
                    //increment the input_arbiter activity
                    m_input_arbiter_activity++;
                    //we have a broadcast request this cycle
                    broadcast_this_cycle = true;
                    //update inport broadcast
                    m_inport_broadcast = inport;
                    //update vc broadcast
                    m_vc_broadcast = invc;
                    //got one winner vc for broadcast this cycle
                    shouldBreak = true;

                }
                //======================================================
            }

            invc++; //choose the next invc in inport next time
            //we return to the first input vc again (round-robin)
            if (invc >= m_num_vcs)
                invc = 0;
        }
    }

    if (broadcast_this_cycle == false) {
        std::cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
        std::cout << "No broadcast for this cycle.\n";
        std::cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
    }
}

/*
 * SA-II (or SA-o) loops through all output ports,
 * and selects one input VC (that placed a request during SA-I)
 * as the winner for this output port in a round robin manner.
 *      - For HEAD/HEAD_TAIL flits, performs simplified outvc allocation.
 *        (i.e., select a free VC from the output port).
 *      - For BODY/TAIL flits, decrement a credit in the output vc.
 * The winning flit is read out from the input VC and sent to the
 * CrossbarSwitch.
 * An increment_credit signal is sent from the InputUnit
 * to the upstream bus. For HEAD_TAIL/TAIL flits, is_free_signal in the
 * credit is set to true.
 */
void
BusSwitchManager::arbitrate_outports()
{
    //we need to traverse the outports only if there's a winner vc
    if (broadcast_this_cycle) {
        //the inport that has won the broadcast
        int inport = m_inport_broadcast;
        //the winner vc in that inport
        int invc = m_vc_broadcast;

        //get the InputUnit with the inport number from
        //the bus this SwitchAllocator is a part of
        auto input_unit = m_bus->getInputUnit(inport);

        // remove flit from Input VC
        chunk *t_flit = input_unit->getTopFlit(invc);

        // Update Round Robin pointer to the next VC
        // We do it here to keep it fair.
        // Only the VC which got switch traversal
        // is updated.

        m_round_robin_invc[inport] = 0;

        //vector containing the output vcs for an invc
        std::vector<int> outvcs;
        // Make sure the vector is empty
        outvcs.clear();
        //if the outvcs are already allocated, retrieve them
        if (input_unit->is_outvc_allocated(invc)) {
            std::cout << "#############################################\n";
            std::cout << "Outvcs already allocated to this invc are:\n";
            for (int outport = 0; outport < m_num_outports; outport++) {
                outvcs.push_back(input_unit->get_broadcast_outvc(invc, outport));
                std::cout << outvcs[outport] <<", ";
            }
            std::cout << "#############################################\n";

        } else {
            //if outvcs are not allocatd for broadcast, allocate them
            std::cout << "#############################################\n";
            std::cout << "Outvcs being allocated to this invc are:\n";
            for (int outport = 0; outport < m_num_outports; outport++) {
                outvcs.push_back(vc_allocate(outport, inport, invc));
                std::cout << outvcs[outport] <<", ";
            }
            std::cout << "#############################################\n";
        }

        //Sending a credit flit back for the winner vc in the winner inport
        if ((t_flit->get_type() == TAIL_) ||
            t_flit->get_type() == HEAD_TAIL_) {
            // This Input VC should now be empty
            assert(!(input_unit->isReady(invc, curTick())));

            // Free this VC (change invc state from active to idle)
            //this invc became free and now is available for outvc allocation
            input_unit->set_vc_idle(invc, curTick());

            // Send a credit back
            // along with the information that this VC is now idle
            input_unit->increment_credit(invc, true, curTick());
        } else { //flit type is HEAD or BODY
            // Send a credit back
            // but do not indicate that the VC is idle
            input_unit->increment_credit(invc, false, curTick());
        }

        for (int outport = 0; outport < m_num_outports; outport++) {

            //get the OutputUnit with the outport number from
            //the bus this SwitchAllocator is a part of
            auto output_unit = m_bus->getOutputUnit(outport);

            //get the outvc for this specific outport
            int outvc = outvcs[outport];

            //printing what just happened
            DPRINTF(RubyNetwork, "SwitchAllocator at Bus %d "
                                    "granted outvc %d at outport %d "
                                    "to invc %d at inport %d to flit %s at "
                                    "cycle: %lld\n",
                    m_bus->get_id(), outvc,
                    m_bus->getPortDirectionName(
                        output_unit->get_direction()),
                    invc,
                    m_bus->getPortDirectionName(
                        input_unit->get_direction()),
                        *t_flit,
                    m_bus->curCycle());


            // Update outport field in the flit since this is
            // used by CrossbarSwitch code to send it out of
            // correct outport.
            // Note: post route compute in InputUnit,
            // outport is updated in VC, but not in flit
            t_flit->set_outport(outport);

            // set outvc (i.e., invc for next hop) in flit
            // (This was updated in VC by vc_allocate, but not in flit)
            t_flit->set_vc(outvc);

            // decrement credit in outvc (i.e., invc of the next bus)
            output_unit->decrement_credit(outvc);

            // flit ready for Switch Traversal
            //change the flit stage from SA_ to ST_
            t_flit->advance_stage(ST_, curTick());
            //grant the switch (crossbar) to t_flit
            m_bus->grant_switch(inport, t_flit);
            //increment the activity of output_arbiter
            m_output_arbiter_activity++;

        }

    }
    //we answered to the broadcast request in this cycle
    broadcast_this_cycle = false;
}

/*
 * A flit can be sent only if
 * (1) there is at least one free output VC at the
 *     output port (for HEAD/HEAD_TAIL),
 *  or
 * (2) if there is at least one credit (i.e., buffer slot)
 *     within the VC for BODY/TAIL flits of multi-flit packets.
 * and
 * (3) pt-to-pt ordering is not violated in ordered vnets, i.e.,
 *     there should be no other flit in this input port
 *     within an ordered vnet
 *     that arrived before this flit and is requesting the same output port.
 */
bool
BusSwitchManager::send_allowed(int inport, int invc, int outport, int outvc)
{
    // Check if outvc needed
    // Check if credit needed (for multi-flit packet)
    // Check if ordering violated (in ordered vnet)
    //In ordered vnet the flit that was enqueued first
    //should be sent first (FIFO)

    //get the vnet of invc
    int vnet = get_vnet(invc);
    //check if invc needs an outvc (for HEAD/HEAD_TAIL flits)
    bool has_outvc = (outvc != -1);
    //check whether the outvc has any credit left
    bool has_credit = false;

    //get the right outport from the bus which
    //this SwitchAllocator is a part of
    auto output_unit = m_bus->getOutputUnit(outport);
    if (!has_outvc) { //if we don't have an outvc for this invc

        // needs outvc
        // this is only true for HEAD and HEAD_TAIL flits.

        //check if output_unit in vnet has any free vc
        if (output_unit->has_free_vc(vnet)) {

            // std::cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
            // std::cout << "Found free outvc for invc " <<invc<< " in inport "<<inport<<" from outport "<<outport<<".\n";
            // std::cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";

            //if yes, set has_outvc to true
            has_outvc = true;

            // each VC has at least one buffer,
            // so no need for additional credit check
            has_credit = true;
        }
    } else { //if we already have an outvc for this invc (for BODY/TAIL flits)
        //check whether the outvc in output_unit has any credit left
        has_credit = output_unit->has_credit(outvc);
    }

    // cannot send if no outvc or no credit.
    if (!has_outvc || !has_credit)
        return false;


    // protocol ordering check
    if ((m_bus->get_net_ptr())->isVNetOrdered(vnet)) { //if vnet is ordered
        //get the right inport from the bus which
        //this SwitchAllocator is a part of
        auto input_unit = m_bus->getInputUnit(inport);

        // enqueue time of this flit
        Tick t_enqueue_time = input_unit->get_enqueue_time(invc);

        // check if any other flit is ready for SA and for same output port
        // and was enqueued before this flit
        int vc_base = vnet*m_vc_per_vnet; //the first vc in vnet
        for (int vc_offset = 0; vc_offset < m_vc_per_vnet; vc_offset++) {
            int temp_vc = vc_base + vc_offset;
            //if any other vc in this vnet is in SA stage, and the flit's
            //destination in that vc is the same outport, and that flit
            //was enqueued before our flit, then ordering is not right,
            //and we shouldn't send
            if (input_unit->need_stage(temp_vc, SA_, curTick()) &&
               (input_unit->get_outport(temp_vc) == outport) &&
               (input_unit->get_enqueue_time(temp_vc) < t_enqueue_time)) {
                return false;
            }
        }
    }

    //it is okay to send
    return true;
}

// Assign a free VC to the winner of the output port.
int
BusSwitchManager::vc_allocate(int outport, int inport, int invc)
{
    // Select a free VC from the output port
    int outvc =
        m_bus->getOutputUnit(outport)->select_free_vc(get_vnet(invc));

    // has to get a valid VC since it checked before performing SA
    assert(outvc != -1);
    //grant the free outvc to the winner invc (push the outvc in the outvcs vector of invc)
    m_bus->getInputUnit(inport)->grant_broadcast_outvc(invc, outvc);
    std::cout << "&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&\n";
    std::cout << "The outvc " <<outvc<< " from outport "<<outport<<" is allocated to invc " <<invc<< " from inport "<<inport<<".\n";
    std::cout << "&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&\n";
    return outvc; //return the free outvc (free invc of the next bus)
}

// Wakeup the bus next cycle to perform SA again
// if there are flits ready.
void
BusSwitchManager::check_for_wakeup()
{
    //get the next clockEdge (cycle) of this bus
    Tick nextCycle = m_bus->clockEdge(Cycles(1));
    //if the bus is already scheduled for the next
    //cycle, then no action needs to be done
    if (m_bus->alreadyScheduled(nextCycle)) {
        return;
    }

    for (int i = 0; i < m_num_inports; i++) { //for every inport
        for (int j = 0; j < m_num_vcs; j++) { //for every vc in that inport
            //if the vc in that inport has a flit in SA_ stage
            if (m_bus->getInputUnit(i)->need_stage(j, SA_, nextCycle)) {
                //schedule the bus to wakeup in the next cycle
                m_bus->schedule_wakeup(Cycles(1));
                return;
            }
        }
    }
}

//get the vnet of an input vc
int
BusSwitchManager::get_vnet(int invc)
{
    int vnet = invc/m_vc_per_vnet;
    assert(vnet < m_bus->get_num_vnets());
    return vnet;
}


// Clear the request vector within the allocator at end of SA-II.
// Was populated by SA-I.
void
BusSwitchManager::clear_request_vector()
{
    std::fill(m_has_request.begin(), m_has_request.end(), false);
}

//resetting SwitchAllocator statistics
void
BusSwitchManager::resetStats()
{
    m_input_arbiter_activity = 0;
    m_output_arbiter_activity = 0;
}

} // namespace onyx
} // namespace ruby
} // namespace gem5
