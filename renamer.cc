#include <inttypes.h>
#include <assert.h>
#include <iostream>
#include <deque>
#include <vector>

#include "renamer.h"

using namespace std;

renamer::renamer(uint64_t n_log_regs_, uint64_t n_phys_regs_, uint64_t n_branches_, uint64_t n_active_){

    assert(n_phys_regs_ >n_branches_);

    assert(n_branches_>=1);
    assert(n_branches_<=64);
    assert(n_active_>0);
    
    n_log_regs = n_log_regs_;
    n_phys_regs = n_phys_regs_;
    n_branches = n_branches_;
    n_active = n_active_;


    //Allocate space

    uint64_t list_size;
    list_size = n_phys_regs-n_log_regs;

    //Initialization
    rmt.resize(n_log_regs);
    GBM=0;
    for(uint64_t i=0; i < rmt.size(); i++){
        rmt[i] =i;
    }

    amt.resize(n_log_regs);
    for(uint64_t i=0; i <amt.size(); i++){
        amt[i] =i;
    }

    branch_checkpoints.resize(n_branches);

    fl.free_list.resize(list_size);
    al.active_list.resize(list_size);

    // arf.resize(n_log_regs);
    prf.resize(n_phys_regs);
    for(uint64_t i=0; i < prf.size(); i++){
        prf[i]=i;
    }

    prf_ready_bit.resize(n_phys_regs);
    for(uint64_t i=0; i <prf_ready_bit.size(); i++){
        prf_ready_bit[i] =true;
    }

    vector<Branch_Checkpoints_entry>::iterator it;
    for(it = branch_checkpoints.begin(); it!= branch_checkpoints.end(); it++){
        it->rmt_c.resize(n_log_regs);
    }
}


renamer::~renamer(){}

bool renamer::stall_reg(uint64_t bundle_dst){

    return fl.Future_isFull(bundle_dst);
}

bool renamer::stall_branch(uint64_t bundle_branch){

    uint64_t temp = GBM;
    uint64_t count_checkpoints = 0;
    for(uint64_t i =0; i <n_branches; i++){
        // count the #1s in the gbm. 
        // If the #branches - #checkpoints > #incoming branch then we don't need to stall;
        // 
        if(temp & 1)
            count_checkpoints++;
        temp = temp >> 1;
    }
    if( bundle_branch > (n_branches - count_checkpoints))
        return true;
    else
        return false;

}
uint64_t  renamer::get_branch_mask(){
    return GBM;
}

uint64_t renamer::rename_rsrc(uint64_t log_reg){
    //Obtain mappings from RMT
    return rmt[log_reg];
}

uint64_t renamer::rename_rdst(uint64_t log_reg){
    //1. Pop free register from free list
    
    assert(!fl.isEmpty());//check if free list is empty
    uint64_t val = fl.free_list[fl.head];
    fl.pop_from_fl();

    //2. Assign physical register to logical destination register

    //3. update RMT to reflect new mapping
    rmt[log_reg] = val;

}
uint64_t renamer::checkpoint(){
    // * Find a free bit -- i.e., a '0' bit -- in the GBM. Assert that
	//   a free bit exists: it is the user's responsibility to avoid
	//   a structural hazard by calling stall_branch() in advance.

    //scan for zero throughout gbm and grab the position that is zero

    uint64_t temp = GBM;

    // pos holds branch id 
    int pos=-1;
    for(uint64_t i =0; i <n_branches; i++){
        if( !(temp &1)){
            pos = i;
            break;
        }
        temp = temp >> 1;
    }

    assert( pos < n_branches);
    // copy_state(RMT,branch_checkpoints[pos].rmt_c);
    branch_checkpoints[pos].set_fl_head_c(fl.head);
    branch_checkpoints[pos].set_fl_head_phase_c(fl.head_phase);
    branch_checkpoints[pos].set_smt(rmt);

    GBM|= 1<<pos;

    return pos;


}

bool renamer::stall_dispatch(uint64_t bundle_inst){
    // Return "true" (stall) if the Active List does not have enough
	// space for all instructions in the dispatch bundle.
    return al.Future_isFull(bundle_inst);

}

uint64_t renamer::dispatch_inst(bool dest_valid,uint64_t log_reg, uint64_t phys_reg,bool load, bool store, bool branch,bool amo,bool csr,uint64_t PC){
    assert(stall_dispatch(1));
    Active_List_entry entry;
    // inputs from params
    entry.dest_flag = dest_valid;
    entry.log_reg_num = log_reg;
    entry.p_reg_num = phys_reg;
    entry.load_flag = load;
    entry.store_flag = store;
    entry.branch_misprediction = branch;
    entry.amo_flag = amo;
    entry.csr_flag = csr;
    entry.PC = PC;
    // flags set in dispatch stage
    entry.completed = false;
    entry.exception = false;
    entry.value_misprediction = false;
    entry.load_violation = false;

    // This function dispatches a single instruction into the Active List.

    al.push_into_al(entry);
    //Return the instruction's index in the Active List.
    // tail is incremented whenever entry is added to the active list so that it always points
    // to the next free entry in the active list
    return (al.tail-1);

}

bool renamer::is_ready(uint64_t phys_reg){
    return prf_ready_bit[phys_reg];
}

void renamer::clear_ready(uint64_t phys_reg){
    prf_ready_bit[phys_reg] = false;
}

uint64_t renamer::read(uint64_t phys_reg){
    return prf[phys_reg];
}

void renamer::set_ready(uint64_t phys_reg){
    prf_ready_bit[phys_reg] = true;
}

void renamer::write(uint64_t phys_reg, uint64_t value){
    prf[phys_reg] = value;
}

void renamer::set_complete(uint64_t AL_index){
    al.active_list[AL_index].completed = true;
}

void renamer::resolve(uint64_t AL_index, uint64_t branch_ID, bool correct){
    if(!correct){
        // * Restore the GBM from the branch's checkpoint. Also make sure the
        //   mispredicted branch's bit is cleared in the restored GBM,
        //   since it is now resolved and its bit and checkpoint are freed.
        // * You don't have to worry about explicitly freeing the GBM bits
        //   and checkpoints of branches that are after the mispredicted
        //   branch in program order. The mere act of restoring the GBM
        //   from the checkpoint achieves this feat
        GBM = branch_checkpoints[branch_ID].GBM_c;
        // * Restore the RMT using the branch's checkpoint.
        copy_state(branch_checkpoints[branch_ID].rmt_c, rmt);
        // * Restore the Free List head pointer and its phase bit,
        //   using the branch's checkpoint.
        fl.head = branch_checkpoints[branch_ID].fl_head_c;
        fl.head_phase = branch_checkpoints[branch_ID].fl_head_phase_c;
        // * Restore the Active List tail pointer and its phase bit
        //   corresponding to the entry after the branch's entry.
        //   Hints:
        //   You can infer the restored tail pointer from the branch's
        //   AL_index. You can infer the restored phase bit, using
        //   the phase bit of the Active List head pointer, where
        //   the restored Active List tail pointer is with respect to
        //   the Active List head pointer, and the knowledge that the
        //   Active List can't be empty at this moment (because the
        //   mispredicted branch is still in the Active List).

        // 2 cases: 
        //1. when the AL index is the last entry and we need to wrap around
        //2. normal case
        if(AL_index == al.active_list.size()-1){
            al.tail = 0;
            al.tail_phase = !al.head_phase;
        }
        else{
            al.tail = AL_index + 1;
            al.tail_phase = al.head_phase;
        }

    }
    else{
        // correctly predicted
        // * Remember to clear the branch's bit in the GBM.
        // CHECK IF THIS LOGIC WORKS
        uint64_t temp = 1<<branch_ID;
        temp = ~temp;
        GBM&=temp;
	    // * Remember to clear the branch's bit in all checkpointed GBMs.
        for(int i =0; i <n_branches; i++){
            branch_checkpoints[i].GBM_c&=temp;
        }


    }
}

bool renamer::precommit(bool &completed, bool &exception, bool &load_viol, bool &br_misp, bool &val_misp, bool &load, bool &store, bool &branch, bool &amo, bool &csr, uint64_t &PC){
    
    completed = al.active_list[al.head].completed;
    exception = al.active_list[al.head].exception;
    load_viol = al.active_list[al.head].load_violation;
    br_misp = al.active_list[al.head].branch_misprediction;
    val_misp = al.active_list[al.head].value_misprediction;
    load = al.active_list[al.head].load_flag;
    store = al.active_list[al.head].store_flag;
    amo = al.active_list[al.head].amo_flag;
    csr= al.active_list[al.head].csr_flag;
    PC = al.active_list[al.head].PC;

    if(al.isEmpty() == false)
        return true;
    else
        return false;
}

void renamer::commit(){
    assert(al.isEmpty() == false);
    assert(al.active_list[al.head].completed == true);
    assert(al.active_list[al.head].exception == false);
    assert(al.active_list[al.head].load_violation == false);

    // commit instructions which have a valid dest tag

    if(al.active_list[al.head].dest_flag == true){
        // get the logical and physical regs of current head inst
        uint64_t pop_log_reg = al.active_list[al.head].log_reg_num;
        uint64_t pop_phy_reg = al.active_list[al.head].p_reg_num;
        //get the physical reg to be pushed into free list from AMT
        uint64_t push_fl_val = amt[pop_log_reg];
        //Release the physical reg into FL
        fl.push_into_fl(push_fl_val);
        // In RMT at the logical dest value, commit the phys dest of head instruction
        rmt[pop_log_reg] = pop_phy_reg;
        // pop instruction from the active list
        al.pop_from_al();

    }


}

void renamer::squash(){
    //copy from AMT to RMT
    copy_state(amt, rmt);

    // set GBM to 0
    GBM =0;

    // set PRF ready bit to true;
    for(uint64_t i=0; i <prf_ready_bit.size(); i++)
        prf_ready_bit[i] = true;
    
    // set Free list and Active list to initial state- constructor values
    fl.head = 0;
    fl.tail =0;
    fl.head_phase = false;
    fl.tail_phase = false;

    al.head =0;
    al.tail = 0;
    al.head_phase = true;
    al.tail_phase = true;

}

void renamer::set_exception(uint64_t AL_index){
    al.active_list[AL_index].exception = true;
}

void renamer::set_load_violation(uint64_t AL_index){
    al.active_list[AL_index].load_violation = true;
}

void renamer::set_branch_misprediction(uint64_t AL_index){
    al.active_list[AL_index].branch_misprediction = true;
}

void renamer::set_value_misprediction(uint64_t AL_index){
    al.active_list[AL_index].value_misprediction = true;
}

bool renamer::get_exception(uint64_t AL_index){
    return al.active_list[AL_index].exception;
}







