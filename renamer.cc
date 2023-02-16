#include <inttypes.h>
#include <assert.h>
#include <iostream>
#include <vector>

#include "renamer.h"

#include<signal.h>

using namespace std;

Free_List::Free_List(uint64_t free_list_size, uint64_t log_reg_size, uint64_t n_phys_regs){
    //free_list_size = free_list_size;
    head = 0;
    tail = 0;
    head_phase = true;
    tail_phase = false;

    free_list.resize(n_phys_regs - log_reg_size);
    this->free_list_size = n_phys_regs - log_reg_size;
    // uint64_t free_prf = log_reg_size -1;
    // uint64_t j =0;
    // cout<<"Free_List::Free_List"<<endl;
    for(uint64_t i=0; i < free_list.size(); i++){
        free_list[i] = i+ log_reg_size;
        // cout<<free_list[i]<<" ";
    }

    // uint64_t free_prf = log_reg_size -1;
    // for(uint64_t i =0; i <free_list_size; i++){
    //     free_list[i] =free_prf++;
    // }

    // for(uint64_t i=0; i < free_list_size; i++)
    //     free_list[i] = i;
}

bool Free_List::isFull(){
    return tail == head  && head_phase != tail_phase;
}

bool Free_List::isEmpty(){
    return head == tail && head_phase == tail_phase;
}

uint64_t Free_List::count_free_regs(){
    if(head_phase != tail_phase)
        return free_list_size - head+ tail ;
    else
        return tail - head ;

}

void Free_List::push_into_fl(uint64_t val){
    assert(isFull() == false);

    free_list[tail]= val;
    tail = tail +1;
    if(tail  == free_list_size){
        tail =0;
        tail_phase= !tail_phase;
    }
}

uint64_t Free_List::pop_from_fl(){
    assert(isEmpty() == false);
    // cout<<"In Free_List::pop_from_fl()"<<endl;
    // cout<<"head is "<<head<<endl;
    // cout<<" head value is "<<free_list[head]<<endl;
    uint64_t val = free_list[head];

    head = head +1;
    if(head== free_list_size){
        head =0;
        head_phase = !head_phase;
    }

    return val;
}

void Free_List::resize(uint64_t free_list_size, uint64_t log_reg_size){
    free_list.resize(free_list_size);
    uint64_t free_prf = log_reg_size -1;
    for(uint64_t i =0; i <free_list_size; i++){
        free_list[i] =free_prf++;
    }
}

Active_List_entry::Active_List_entry(){
    this->dest_flag = false;
    this->log_reg_num= 0;
    this->p_reg_num= 0;
    this->completed=false;
    this->exception=false;
    this->load_violation = false;
    this->branch_misprediction = false;
    this->value_misprediction = false;
    this->load_flag= false;
    this->store_flag = false;
    this->amo_flag = false;
    this->csr_flag = false;
    this->branch = false;
    this->PC=0;
}
Active_List_entry::Active_List_entry(const Active_List_entry& entry){
    this->dest_flag = entry.dest_flag;
    this->log_reg_num=entry.log_reg_num;
    this->p_reg_num=entry.p_reg_num;
    this->completed=entry.completed;
    this->exception=entry.exception;
    this->load_violation = entry.load_violation;
    this->branch_misprediction = entry.branch_misprediction;
    this->value_misprediction = entry.value_misprediction;
    this->load_flag= entry.load_flag;
    this->store_flag = entry.store_flag;
    this->amo_flag = entry.amo_flag;
    this->csr_flag = entry.csr_flag;
    this->branch = entry.branch;
    this->PC=entry.PC;
}

Active_List::Active_List(uint64_t active_list_size){
    head =0;
    tail =0;
    head_phase = true;
    tail_phase = true;
    this->active_list_size = active_list_size;
    active_list.resize(active_list_size);
}

bool Active_List::isFull(){
    return tail == head  && (head_phase != tail_phase);
}

bool Active_List::isEmpty(){
    return head == tail && head_phase == tail_phase;
}

void Active_List::push_into_al(Active_List_entry entry){
    assert(isFull() == false);
    // cout<<" Active_List::push_into_al"<<endl;
    // cout<<entry.dest_flag<<" "<<entry.log_reg_num<<" "<<entry.p_reg_num<<endl;

    active_list[tail] = entry;
    tail++;
    if(tail==active_list_size){
        tail =0;
        tail_phase= !tail_phase;
    }

}
void Active_List::resize(uint64_t active_list_size){
    active_list.resize(active_list_size);
}

Active_List_entry Active_List::pop_from_al(){
    assert(isEmpty() == false);

    Active_List_entry entry;
    entry = active_list[head];

    head++;
    if(head == active_list_size){
        head =0;
        head_phase = !head_phase;
    }

    return entry;

}

uint64_t Active_List::free_space(){
    if(head_phase == tail_phase)
        return active_list_size - tail + head;
    else
        return head - tail;

}

Branch_Checkpoints_entry::Branch_Checkpoints_entry(){
    for(uint64_t i=0; i <rmt_c.size(); i++){
        rmt_c[i] = i;
    }
    fl_head_c = -1;
    fl_head_phase_c = false;
    GBM_c =0;
    al_tail_c = -1;
    al_tail_phase_c = false;
}

Branch_Checkpoints_entry::Branch_Checkpoints_entry(const Branch_Checkpoints_entry& entry){
    for(uint64_t i =0; i <entry.rmt_c.size(); i++)
        rmt_c[i] = entry.rmt_c[i];
    fl_head_c = entry.fl_head_c;
    fl_head_phase_c = entry.fl_head_phase_c;
    GBM_c = entry.GBM_c;
    al_tail_c = entry.al_tail_c;
    al_tail_phase_c = entry.al_tail_phase_c;
}

renamer::renamer(uint64_t n_log_regs_,uint64_t n_phys_regs_, uint64_t n_branches_, uint64_t n_active_){
    n_log_regs = n_log_regs_;
    n_phys_regs = n_phys_regs_;
    n_branches = n_branches_;
    n_active = n_active_;

    // cout<<"renamer::renamer()"<<endl;
    // cout<<" logical regs max"<<n_log_regs<<" physical regs max"<<n_phys_regs<<endl;
    assert(n_active >0);
    assert(n_branches>=1);
    assert(n_branches <= 64);
    uint64_t free_list_size = n_phys_regs - n_log_regs;
    assert(n_active >= n_phys_regs-n_log_regs);

    fl = Free_List(free_list_size, n_log_regs, n_phys_regs);
    al = Active_List(n_active);

    // al.resize(n_active);

    rmt.resize(n_log_regs);
    for(uint64_t i =0; i < n_log_regs; i++)
        rmt[i]=i;

    amt.resize(n_log_regs);
    for(uint64_t i =0; i < n_log_regs; i++)
        amt[i]=i;
    
    prf.resize(n_phys_regs);
    for(uint64_t i =0; i <n_phys_regs; i++)
        prf[i]=0;
    
    prf_ready_bit.resize(n_phys_regs);
    for(uint64_t i =0; i < n_phys_regs; i++)
        prf_ready_bit[i] = true;

    GBM=0;
    //fl.resize(free_list_size_, n_log_regs);
    // for(uint64_t i =0; i <free_list_size_; i++)
    //     cout<<i<<" "<<fl.free_list[i]<<endl;


    branch_checkpoints.resize(n_branches);

    vector<Branch_Checkpoints_entry>::iterator it;
    for(it = branch_checkpoints.begin(); it!= branch_checkpoints.end(); it++){
        it->rmt_c.resize(n_log_regs);
    }
}
renamer::~renamer(){}

bool renamer::stall_reg(uint64_t bundle_dst){
    // cout<<"renamer::stall_reg "<<bundle_dst<<endl;
    // cout<<"Available regs "<<fl.count_free_regs()<<endl;
    // cout<<fl.head<<" "<<fl.head_phase<<" "<<fl.tail<<" "<<fl.tail_phase<<endl;
    if (fl.count_free_regs() >= bundle_dst){
        // cout<<"renamer::stall_reg returns false"<<endl;
        return false;
    }
    else{
        // cout<<"renamer::stall_reg returns true"<<endl;
        return true;
    }
       
}

bool renamer::stall_branch(uint64_t bundle_branch){
    uint64_t temp = GBM;
    uint64_t count_checkpoints =0;

    for(uint64_t i =0; i <n_branches; i++){
        if((temp & 1 ) ==1)
            count_checkpoints++;
        temp = temp >>1;
    }

    if(bundle_branch > (n_branches - count_checkpoints)){
        // cout<<"renamer::stall_branch returns true "<<endl;
        return true;
    }
    else{
        // cout<<"renamer::stall_branch returns false" <<endl;
        return false;
    }

}

uint64_t renamer::get_branch_mask(){
    return GBM;
}

uint64_t renamer::rename_rsrc(uint64_t log_reg){
    return rmt[log_reg];
}

uint64_t renamer::rename_rdst(uint64_t log_reg){
    // Pop a free physical register from the free list
    //assert(fl.isEmpty() == false);
   
    uint64_t popped_reg = fl.pop_from_fl();
    // Update rename map table to reflect the new mapping
    // prf_ready_bit[popped_reg] = false;
    rmt[log_reg] = popped_reg;
    // cout<<" renamer::rename_rdst "<<log_reg<<" is renamed to "<<popped_reg<<endl;
    // cout<<" log reg size"<<n_log_regs<<" phys regs size "<<n_phys_regs<<endl;
    
    return popped_reg;

}

uint64_t renamer::checkpoint(){
	// * Find a free bit -- i.e., a '0' bit -- in the GBM. Assert that
	//   a free bit exists: it is the user's responsibility to avoid
	//   a structural hazard by calling stall_branch() in advance.
    // cout<<"renamer::checkpoint()"<<endl;

    uint64_t temp = GBM;
    assert(stall_branch(1) == false);
    uint64_t position = -1;
    for(uint64_t i =0; i < n_branches; i++){
        if(!(temp & 1)){
            position =i;
            break;
        }
        temp = temp >>1;
    }
    // cout<<"In renamer::checkpoint()"<<endl;
    // cout<<" GBM is "<<GBM<<endl;
    // cout<<" Position is "<<position<<endl;


    assert(position < n_branches);

    branch_checkpoints[position].fl_head_c =fl.head;
    branch_checkpoints[position].fl_head_phase_c = fl.head_phase;
    branch_checkpoints[position].al_tail_c = al.tail;
    branch_checkpoints[position].al_tail_phase_c = al.tail_phase;
    for(uint64_t i =0; i < rmt.size(); i++)
        branch_checkpoints[position].rmt_c[i] = rmt[i];
    //copy_state(rmt, branch_checkpoints[position].rmt_c);
    branch_checkpoints[position].GBM_c = GBM;
    GBM|= 1<<position;

  

    return position;
}

bool renamer::stall_dispatch(uint64_t bundle_inst){
    // cout<<" Active list free space is"<< al.free_space()<<endl;
    // cout<<al.head<<" "<<al.head_phase<<" "<<al.tail<<" "<<al.tail_phase<<endl;
    //cout<<"renamer::stall_dispatch"<<endl;
    if(al.free_space() >=bundle_inst){
        // cout<<"renamer::stall_dispatch returning false"<<endl;
        return false;
    }
    else{
        // cout<<"renamer::stall_dispatch returning true"<<endl;
        return true;
    }
}
uint64_t renamer::dispatch_inst(bool dest_valid,uint64_t log_reg, uint64_t phys_reg,bool load, bool store, bool branch,bool amo,bool csr,uint64_t PC){
    //cout<<"renamer::dispatch_inst "<<endl;
    assert(al.isFull() == false);
    Active_List_entry entry;
    // inputs from params
    entry.dest_flag = dest_valid;
    if(dest_valid){
        entry.log_reg_num = log_reg;
        entry.p_reg_num = phys_reg;
    }
    // else{
    //     entry.log_reg_num = -1;
    //     entry.p_reg_num = -1;
    // }
    entry.load_flag = load;
    entry.store_flag = store;
    entry.branch_misprediction = false;
    entry.amo_flag = amo;
    entry.csr_flag = csr;
    entry.branch = branch;
    entry.PC = PC;
    // flags set in dispatch stage
    entry.completed = false;
    entry.exception = false;
    entry.value_misprediction = false;
    entry.load_violation = false;

    // This function dispatches a single instruction into the Active List.
    uint64_t al_tail_val = al.tail;

    al.push_into_al(entry);
    //Return the instruction's index in the Active List.
    // tail is incremented whenever entry is added to the active list so that it always points
    // to the next free entry in the active list
    // cout<<"renamer::dispatch_inst"<<endl;
    // cout<<al_tail_val<<endl;

    return al_tail_val;

}

bool renamer::is_ready(uint64_t phys_reg){
    // assert(phys_reg < n_log_regs);
    return prf_ready_bit[phys_reg];
}

void renamer::clear_ready(uint64_t phys_reg){    
     assert(phys_reg < n_phys_regs);

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
    uint64_t mask = 1<<branch_ID;
    uint64_t temp_mask = ~mask;

    if(correct == true){
        GBM = GBM & temp_mask;
        for(uint64_t i =0; i < n_branches; i++){
            // if((branch_checkpoints[i].GBM_c & mask)!=0)
                branch_checkpoints[i].GBM_c= branch_checkpoints[i].GBM_c & temp_mask;
        }
        return;
    }
    else{
        assert((GBM & mask)!=0);
        branch_checkpoints[branch_ID].GBM_c = branch_checkpoints[branch_ID].GBM_c & temp_mask;
        GBM = branch_checkpoints[branch_ID].GBM_c;
        // uint64_t old_tail = al.tail;
        al.tail = AL_index +1;
        if(al.tail == al.active_list_size){
            al.tail =0;
            //al.tail_phase = !al.head_phase;
        }
        if(al.tail > al.head)
            al.tail_phase = al.head_phase;
        else if(al.tail < al.head)
            al.tail_phase = !al.head_phase;
        else if(al.tail == al.head){
            al.tail_phase = !al.head_phase;
        }


        // uint64_t new_tail = AL_index + 1;
        // if(new_tail == al.active_list_size)
        //     new_tail =0;
        // if( al.head == new_tail){ // full condition
        //     al.tail = new_tail;
        //     al.tail_phase = !al.head_phase;
        // }
        // else if(new_tail < al.head && new_tail < al.tail){
        //     al.tail = new_tail;
        //     al.tail_phase != al.head_phase;
        // }
        // else if(new_tail > al.head && new_tail > al.tail){
        //     al.tail = new_tail;
        //     al.tail_phase = al.head_phase;
        // }

        // if(((AL_index +1)%al.active_list_size) > al.tail)
        //     al.tail_phase = !al.tail_phase;
        // al.tail = (AL_index +1 )% al.active_list_size;

        fl.head = branch_checkpoints[branch_ID].fl_head_c;
        fl.head_phase = branch_checkpoints[branch_ID].fl_head_phase_c;

        for(uint64_t i =0; i < rmt.size(); i++)
            rmt[i]= branch_checkpoints[branch_ID].rmt_c[i];
        return;
        //copy_state(branch_checkpoints[branch_ID].rmt_c, rmt);
    }
}

bool renamer::precommit(bool &completed,
                       bool &exception, bool &load_viol, bool &br_misp, bool &val_misp,
	               bool &load, bool &store, bool &branch, bool &amo, bool &csr,
		       uint64_t &PC){
    // cout<<"renamer::precommit "<<al.head<<" "<<al.tail<<" "<<al.head_phase<<" "<<al.tail_phase<<endl;
    if(al.isEmpty() == false){
        completed = al.active_list[al.head].completed;
        exception = al.active_list[al.head].exception;
        load_viol = al.active_list[al.head].load_violation;
        br_misp = al.active_list[al.head].branch_misprediction;
        val_misp = al.active_list[al.head].value_misprediction;
        load = al.active_list[al.head].load_flag;
        store = al.active_list[al.head].store_flag;
        amo = al.active_list[al.head].amo_flag;
        csr = al.active_list[al.head].csr_flag;
        branch = al.active_list[al.head].branch;
        PC = al.active_list[al.head].PC;
        return true;
    }
    else 
        return false;
}

void renamer::commit(){
    //cout<<"renamer::commit()"<<endl;
    bool completed, exception, load_viol, br_misp, val_misp, load, store, branch, amo, csr;
    uint64_t PC;

    assert(precommit(completed, exception, load_viol, br_misp, val_misp, load, store, branch,amo, csr, PC) == true);
    
    // wait for instruction head to compelete
    assert(completed == true);
    assert(exception == false);
    assert(load_viol == false);

    

    
    //index AMT using head instruction's logical destination specifier
    if(al.active_list[al.head].dest_flag == true){
        uint64_t amt_index = al.active_list[al.head].log_reg_num;
    
        uint64_t previous_mapping = amt[amt_index];
        uint64_t current_mapping = al.active_list[al.head].p_reg_num;

    // Physical register indicated by current mapping is committed by updating AMT with 
    // current mapping
        amt[amt_index] = current_mapping;
    //free the previous mapping that is contained in AMT: push it onto the free list
        fl.push_into_fl(previous_mapping);


    }
    Active_List_entry entry;
    //Advance the head pointer of Active list
    entry = al.pop_from_al();


}
void renamer::squash(){
    //copy amt to rmt
    //copy_state(amt, rmt);
    for(uint64_t i =0; i < rmt.size(); i++)
        rmt[i] = amt[i];
    //Restoring free list
    fl.head = fl.tail;
    fl.head_phase=!fl.tail_phase ;

    //Restoring Active list
    al.tail = al.head;
    al.tail_phase =al.head_phase;
    
    //set the ready bits to true for those instructions in AMT

    // for(uint64_t i =0; i <n_phys_regs; i++){
    //     clear_ready(i);
    // }

    // for(uint64_t i =0; i <n_log_regs; i++){
    //     set_ready(rmt[i]);
    // }

    GBM =0;

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






