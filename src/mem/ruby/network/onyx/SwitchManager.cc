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


#include "mem/ruby/network/onyx/SwitchManager.hh"

#include "debug/RubyNetwork.hh"
#include "mem/ruby/network/onyx/OnyxNetwork.hh"
#include "mem/ruby/network/onyx/InportModule.hh"
#include "mem/ruby/network/onyx/OutportModule.hh"
#include "mem/ruby/network/onyx/Switcher.hh"

namespace gem5
{

namespace ruby
{

namespace onyx
{

//SwitchManager constructor
SwitchManager::SwitchManager(Switcher *router)
    : Consumer(router)
{
    //set the router for this SwitchManager
    m_router = router;
    //set the number of vcs
    m_num_vcs = m_router->get_num_vcs();
    //set the number of vcs per vnet
    m_vc_per_vnet = m_router->get_vc_per_vnet();

    //initialize input_arbiter & output_arbiter activity
    m_input_arbiter_activity = 0;
    m_output_arbiter_activity = 0;
}

//initializing SwitchManager class variables
void
SwitchManager::init()
{
    //get the number of router inports
    m_num_inports = m_router->get_num_inports();
    //get the number of router outports
    m_num_outports = m_router->get_num_outports();
    //set the size of m_round_robin_inport to the size of inports
    m_round_robin_inport.resize(m_num_outports);
    //set the size of m_round_robin_invc to the size of inports
    //(we choose one input vc from each inport)
    m_round_robin_invc.resize(m_num_inports);
    //set the size of m_port_requests to the size of inports
    //(one request from each inport is sent to outports every cycle)
    m_port_requests.resize(m_num_inports);
    //set the size of m_vc_winners to the size of inports
    //(one vc wins in each inport)
    m_vc_winners.resize(m_num_inports);

    //initialize m_round_robin_invc, m_port_requests, m_vc_winners
    for (int i = 0; i < m_num_inports; i++) {
        m_round_robin_invc[i] = 0;
        m_port_requests[i] = -1;
        m_vc_winners[i] = -1;
    }
    //initialize m_round_robin_inport
    for (int i = 0; i < m_num_outports; i++) {
        m_round_robin_inport[i] = 0;
    }
}

/*
 * The wakeup function of the SwitchManager performs a 2-stage
 * seperable switch allocation. At the end of the 2nd stage, a free
 * output VC is assigned to the winning flits of each output port.
 * There is no separate VCAllocator stage like the one in garnet1.0.
 * At the end of this function, the router is rescheduled to wakeup
 * next cycle for peforming SA for any flits ready next cycle.
 */
void
SwitchManager::wakeup()
{
    arbitrate_inports(); // First stage of allocation
    arbitrate_outports(); // Second stage of allocation

    //Clear the request vector within the allocator at end of SA-II.
    //Was populated by SA-I.
    clear_request_vector();
    //Wakeup the router next cycle to perform SA again
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
SwitchManager::arbitrate_inports()
{
    // Select a VC from each input in a round robin manner
    // Independent arbiter at each input port
    for (int inport = 0; inport < m_num_inports; inport++) {
        //the pointer is on what vc at this inport (e.g., third vc)
        int invc = m_round_robin_invc[inport];

        for (int invc_iter = 0; invc_iter < m_num_vcs; invc_iter++) {
            //get the InputUnit with the inport number from
            //the router this SwitchManager is a part of
            auto input_unit = m_router->getInputUnit(inport);

            //if the top flit in invc in input_unit is currently
            //in SwitchAllocation stage
            if (input_unit->need_stage(invc, SA_, curTick())) {
                // This flit is in SA stage

                //get the outport the flit in invc wants to go to
                int outport = input_unit->get_outport(invc);
                //get the output vc the flit in invc wants to go to
                // (ex: fifth outvc in the third outport)
                int outvc = input_unit->get_outvc(invc);

                // Check if the flit in this InputVC is allowed to be sent.
                // send_allowed conditions are described in that function.
                bool make_request =
                    send_allowed(inport, invc, outport, outvc);

                if (make_request) { //if we're allowed to send
                    //increment the input_arbiter activity
                    m_input_arbiter_activity++;
                    //allocate outport to inport
                    m_port_requests[inport] = outport;
                    //the winner of inport in this round is invc
                    m_vc_winners[inport] = invc;

                    break; // got one vc winner for this port
                }
            }

            invc++; //choose the next invc in inport next time
            //we return to the first input vc again (round-robin)
            if (invc >= m_num_vcs)
                invc = 0;
        }
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
 * to the upstream router. For HEAD_TAIL/TAIL flits, is_free_signal in the
 * credit is set to true.
 */
void
SwitchManager::arbitrate_outports()
{
    // Now there are a set of input vc requests for output vcs.
    // Again do round robin arbitration on these requests.
    // Independent arbiter at each output port
    for (int outport = 0; outport < m_num_outports; outport++) {
        //choose an inport in a round-robin manner
        int inport = m_round_robin_inport[outport];

        for (int inport_iter = 0; inport_iter < m_num_inports;
                 inport_iter++) {

            //if inport has a request this cycle for outport
            if (m_port_requests[inport] == outport) {
                //get the OutputUnit with the outport number from
                //the router this SwitchManager is a part of
                auto output_unit = m_router->getOutputUnit(outport);
                //get the InputUnit with the inport number from
                //the router this SwitchManager is a part of
                auto input_unit = m_router->getInputUnit(inport);

                // get the winner vc for this inport
                int invc = m_vc_winners[inport];

                //get the outvc for this invc
                int outvc = input_unit->get_outvc(invc);
                //if no outvc is already assigned to the flit in invc
                //for HEAD/HEAD_TAIL flits
                if (outvc == -1) {
                    // VC Allocation - select any free VC from outport
                    outvc = vc_allocate(outport, inport, invc);
                }

                // remove flit from Input VC
                chunk *t_flit = input_unit->getTopFlit(invc);
                //printing what just happened
                DPRINTF(RubyNetwork, "SwitchManager at Router %d "
                                     "granted outvc %d at outport %d "
                                     "to invc %d at inport %d to flit %s at "
                                     "cycle: %lld\n",
                        m_router->get_id(), outvc,
                        m_router->getPortDirectionName(
                            output_unit->get_direction()),
                        invc,
                        m_router->getPortDirectionName(
                            input_unit->get_direction()),
                            *t_flit,
                        m_router->curCycle());


                // Update outport field in the flit since this is
                // used by CrossbarSwitch code to send it out of
                // correct outport.
                // Note: post route compute in InputUnit,
                // outport is updated in VC, but not in flit
                t_flit->set_outport(outport);

                // set outvc (i.e., invc for next hop) in flit
                // (This was updated in VC by vc_allocate, but not in flit)
                t_flit->set_vc(outvc);

                // decrement credit in outvc (i.e., invc of the next router)
                output_unit->decrement_credit(outvc);

                // flit ready for Switch Traversal
                //change the flit stage from SA_ to ST_
                t_flit->advance_stage(ST_, curTick());
                //grant the switch (crossbar) to t_flit
                m_router->grant_switch(inport, t_flit);
                //increment the activity of output_arbiter
                m_output_arbiter_activity++;

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

                // remove this request
                m_port_requests[inport] = -1;

                // Update Round Robin pointer
                //go to the next inport for this outport (we now check invcs
                //in this inport to see if any flit needs this outport)
                m_round_robin_inport[outport] = inport + 1;
                //start from the beginning again (round-robin)
                if (m_round_robin_inport[outport] >= m_num_inports)
                    m_round_robin_inport[outport] = 0;

                // Update Round Robin pointer to the next VC
                // We do it here to keep it fair.
                // Only the VC which got switch traversal
                // is updated.
                m_round_robin_invc[inport] = invc + 1; //go to the next invc
                //start from the beginning again (round-robin)
                if (m_round_robin_invc[inport] >= m_num_vcs)
                    m_round_robin_invc[inport] = 0;


                break; // got a input winner for this outport
            }

            inport++; //go to the next inport
            //start from the beginning again (round-robin)
            if (inport >= m_num_inports)
                inport = 0;
        }
    }
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
SwitchManager::send_allowed(int inport, int invc, int outport, int outvc)
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

    //get the right outport from the router which
    //this SwitchManager is a part of
    auto output_unit = m_router->getOutputUnit(outport);
    if (!has_outvc) { //if we don't have an outvc for this invc

        // needs outvc
        // this is only true for HEAD and HEAD_TAIL flits.

        //check if output_unit in vnet has any free vc
        if (output_unit->has_free_vc(vnet)) {

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
    if ((m_router->get_net_ptr())->isVNetOrdered(vnet)) { //if vnet is ordered
        //get the right inport from the router which
        //this SwitchManager is a part of
        auto input_unit = m_router->getInputUnit(inport);

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
SwitchManager::vc_allocate(int outport, int inport, int invc)
{
    // Select a free VC from the output port
    int outvc =
        m_router->getOutputUnit(outport)->select_free_vc(get_vnet(invc));

    // has to get a valid VC since it checked before performing SA
    assert(outvc != -1);
    //grant the free outvc to the winner invc (winner of the output port)
    m_router->getInputUnit(inport)->grant_outvc(invc, outvc);
    return outvc; //return the free outvc (free invc of the next router)
}

// Wakeup the router next cycle to perform SA again
// if there are flits ready.
void
SwitchManager::check_for_wakeup()
{
    //get the next clockEdge (cycle) of this router
    Tick nextCycle = m_router->clockEdge(Cycles(1));
    //if the router is already scheduled for the next
    //cycle, then no action needs to be done
    if (m_router->alreadyScheduled(nextCycle)) {
        return;
    }

    for (int i = 0; i < m_num_inports; i++) { //for every inport
        for (int j = 0; j < m_num_vcs; j++) { //for every vc in that inport
            //if the vc in that inport has a flit in SA_ stage
            if (m_router->getInputUnit(i)->need_stage(j, SA_, nextCycle)) {
                //schedule the router to wakeup in the next cycle
                m_router->schedule_wakeup(Cycles(1));
                return;
            }
        }
    }
}

//get the vnet of an input vc
int
SwitchManager::get_vnet(int invc)
{
    int vnet = invc/m_vc_per_vnet;
    assert(vnet < m_router->get_num_vnets());
    return vnet;
}


// Clear the request vector within the allocator at end of SA-II.
// Was populated by SA-I.
void
SwitchManager::clear_request_vector()
{
    std::fill(m_port_requests.begin(), m_port_requests.end(), -1);
}

//resetting SwitchManager statistics
void
SwitchManager::resetStats()
{
    m_input_arbiter_activity = 0;
    m_output_arbiter_activity = 0;
}

} // namespace onyx
} // namespace ruby
} // namespace gem5
