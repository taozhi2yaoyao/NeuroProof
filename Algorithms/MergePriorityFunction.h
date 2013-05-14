#ifndef MERGEPRIORITYFUNCTION_H
#define MERGEPRIORITYFUNCTION_H

#include "../FeatureManager/FeatureManager.h"
#include "../DataStructures/Rag.h"
#include <tr1/unordered_set>
#include "../DataStructures/AffinityPair.h"

namespace NeuroProof {

class MergePriority {
  public:
    MergePriority(FeatureMgr* feature_mgr_, Rag<Label>* rag_) : 
                        feature_mgr(feature_mgr_), rag(rag_), synapse_mode(false),
                        kicked_out(0) {} 

    MergePriority(FeatureMgr* feature_mgr_, Rag<Label>* rag_, bool synapse_mode_) : 
                        feature_mgr(feature_mgr_), rag(rag_), synapse_mode(synapse_mode_),
                        kicked_out(0) {} 
    
    virtual void initialize_priority(double threshold, bool use_edge_weight=false) = 0;

    virtual void initialize_random(double pthreshold) {}
    
    virtual RagEdge<Label>* get_top_edge() = 0;

    virtual void add_dirty_edge(RagEdge<Label>* edge) = 0;

    bool valid_edge(RagEdge<Label>* edge)
    {
        if (!synapse_mode) {
            if (edge->is_preserve() || edge->is_false_edge()) {
                return false;
            }
            return true;
        } else {
            if (edge->is_false_edge()) {
                return false;
            } else if ((edge->get_size() < 400) && edge->is_preserve()) {
                return false;
            }
            return true;
        }
    }

    virtual bool empty() = 0;
    virtual int qlen(){ return 0;};	
    virtual int get_kout() {return 0;}; 	
    virtual void set_fileid(FILE* pid) {};

  protected:
    Rag<Label>* rag;
    FeatureMgr* feature_mgr;
    int kicked_out;

  private:
    bool synapse_mode;    

};

class ProbPriority : public MergePriority {
  public:
    ProbPriority(FeatureMgr* feature_mgr_, Rag<Label>* rag_) :
                    MergePriority(feature_mgr_, rag_), Epsilon(0.00001), kicked_fid(NULL) {}

    ProbPriority(FeatureMgr* feature_mgr_, Rag<Label>* rag_, bool synapse_mode_) :
                    MergePriority(feature_mgr_, rag_, synapse_mode_),
                    Epsilon(0.00001), kicked_fid(NULL) {}
    void initialize_priority(double threshold_, bool use_edge_weight=false);
    void initialize_random(double pthreshold);
    void clear_dirty();
    bool empty();
    RagEdge<Label>* get_top_edge();
    void add_dirty_edge(RagEdge<Label>* edge);
   
    int qlen(){ return ranking.size();}	

    int get_kout(){return kicked_out;};	
    
    void set_fileid(FILE* pid){kicked_fid = pid;};

  private:

    double threshold;
    const double Epsilon;
    typedef std::multimap<double, std::pair<Label, Label> > EdgeRank_t; 
    typedef std::tr1::unordered_set<OrderedPair<Label>, OrderedPair<Label> > Dirty_t; 
    EdgeRank_t ranking;
    Dirty_t dirty_edges;
    
    FILE* kicked_fid;

};

class MitoPriority : public MergePriority {
  public:
    MitoPriority(FeatureMgr* feature_mgr_, Rag<Label>* rag_) :
                    MergePriority(feature_mgr_, rag_), Epsilon(0.00001) {}

    void initialize_priority(double threshold_, bool use_edge_weight=false);
    
    //void initialize_random(double pthreshold);
    void clear_dirty();
    bool empty();
    RagEdge<Label>* get_top_edge();
    void add_dirty_edge(RagEdge<Label>* edge);

    int qlen(){ return ranking.size();}	

    int get_kout(){return kicked_out;};	
    

  private:

    double threshold;
    const double Epsilon;
    typedef std::multimap<double, std::pair<Label, Label> > EdgeRank_t; 
    typedef std::tr1::unordered_set<OrderedPair<Label>, OrderedPair<Label> > Dirty_t; 
    EdgeRank_t ranking;
    Dirty_t dirty_edges;

};





}

#endif
