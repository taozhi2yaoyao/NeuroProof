#ifndef EDGEPRIORITY_H
#define EDGEPRIORITY_H

#include "../Rag/Rag.h"
#include <vector>
#include <boost/tuple/tuple.hpp>

namespace NeuroProof {

class EdgePriority {
  public:
    typedef boost::tuple<Node_uit, Node_uit> NodePair;
    typedef boost::tuple<unsigned int, unsigned int, unsigned int> Location;

    EdgePriority(Rag_uit& rag_) : rag(rag_), updated(false)
    {
        property_names.push_back(std::string("location"));
        property_names.push_back(std::string("edge_size"));
    }
    virtual NodePair getTopEdge(Location& location) = 0;
    virtual bool isFinished() = 0;
    virtual void updatePriority() = 0;
    // low weight == no connection
    virtual void setEdge(NodePair node_pair, double weight);
    virtual bool undo();
    virtual void removeEdge(NodePair node_pair, bool remove) = 0;
    void clear_history();

  protected:
    virtual void removeEdge(NodePair node_pair, bool remove, std::vector<std::string>& node_properties);
    void setUpdated(bool status);
    bool isUpdated(); 
    Rag_uit& rag;
    
  private:
    typedef std::tr1::unordered_map<std::string, boost::shared_ptr<Property> > NodePropertyMap; 
    struct EdgeHistory {
        Node_uit node1;
        Node_uit node2;
        unsigned long long size1;
        unsigned long long size2;
        std::vector<Node_uit> node_list1;
        std::vector<Node_uit> node_list2;
        NodePropertyMap node_properties1;
        NodePropertyMap node_properties2;
        std::vector<double> edge_weight1;
        std::vector<double> edge_weight2;
        std::vector<bool> preserve_edge1;
        std::vector<bool> preserve_edge2;
        std::vector<bool> false_edge1;
        std::vector<bool> false_edge2;
        std::vector<std::vector<boost::shared_ptr<Property> > > property_list1;
        std::vector<std::vector<boost::shared_ptr<Property> > > property_list2;
        bool remove;
        double weight;
        bool false_edge;
        bool preserve_edge;
        std::vector<PropertyPtr> property_list_curr;
    };

    std::vector<EdgeHistory> history_queue;
    std::vector<std::string> property_names;
    bool updated;
};


void EdgePriority::setUpdated(bool status)
{
    updated = status;
}

bool EdgePriority::isUpdated()
{
    return updated;
}

void EdgePriority::setEdge(NodePair node_pair, double weight)
{
    EdgeHistory history;
    history.node1 = boost::get<0>(node_pair);
    history.node2 = boost::get<1>(node_pair);
    RagEdge_uit* edge = rag.find_rag_edge(history.node1, history.node2);
    history.weight = edge->get_weight();
    history.preserve_edge = edge->is_preserve();
    history.false_edge = edge->is_false_edge();
    history.remove = false;
    history_queue.push_back(history);
    edge->set_weight(weight);
}

void EdgePriority::clear_history()
{
    history_queue.clear();
}


bool EdgePriority::undo()
{
    if (history_queue.empty()) {
        return false;
    }
    EdgeHistory& history = history_queue.back();

    if (history.remove) {
        rag.remove_rag_node(rag.find_rag_node(history.node1));
    
        RagNode_uit* temp_node1 = rag.insert_rag_node(history.node1);
        RagNode_uit* temp_node2 = rag.insert_rag_node(history.node2);
        temp_node1->set_size(history.size1);
        temp_node2->set_size(history.size2);

        for (int i = 0; i < history.node_list1.size(); ++i) {
            RagEdge_uit* temp_edge = rag.insert_rag_edge(temp_node1, rag.find_rag_node(history.node_list1[i]));
            temp_edge->set_weight(history.edge_weight1[i]);
            temp_edge->set_preserve(history.preserve_edge1[i]);
            temp_edge->set_false_edge(history.false_edge1[i]);
            
            temp_edge->set_property_ptr("location", history.property_list1[i][0]);
            temp_edge->set_property_ptr("edge_size", history.property_list1[i][1]);
        } 
        for (int i = 0; i < history.node_list2.size(); ++i) {
            RagEdge_uit* temp_edge = rag.insert_rag_edge(temp_node2, rag.find_rag_node(history.node_list2[i]));
            temp_edge->set_weight(history.edge_weight2[i]);
            temp_edge->set_preserve(history.preserve_edge2[i]);
            temp_edge->set_false_edge(history.false_edge2[i]);
            
            temp_edge->set_property_ptr("location", history.property_list2[i][0]);
            temp_edge->set_property_ptr("edge_size", history.property_list2[i][1]);
        }

        RagEdge_uit* temp_edge = rag.insert_rag_edge(temp_node1, temp_node2);
        temp_edge->set_weight(history.weight);
        temp_edge->set_preserve(history.preserve_edge); 
        temp_edge->set_false_edge(history.false_edge); 
        
        temp_edge->set_property_ptr("location", history.property_list_curr[0]);
        temp_edge->set_property_ptr("edge_size", history.property_list_curr[1]);
        
        for (NodePropertyMap::iterator iter = history.node_properties1.begin(); 
            iter != history.node_properties1.end();
                ++iter) {
            temp_node1->set_property_ptr(iter->first, iter->second);
        }
        for (NodePropertyMap::iterator iter = history.node_properties2.begin();
            iter != history.node_properties2.end();
                ++iter) {
            temp_node2->set_property_ptr(iter->first, iter->second);
        }
    } else {
        RagNode_uit* temp_node1 = rag.find_rag_node(history.node1);
        RagNode_uit* temp_node2 = rag.find_rag_node(history.node2);

        RagEdge_uit* temp_edge = rag.find_rag_edge(temp_node1, temp_node2);
        temp_edge->set_weight(history.weight);
        temp_edge->set_preserve(history.preserve_edge); 
        temp_edge->set_false_edge(history.false_edge); 
    }

    history_queue.pop_back();
    return true;
}


void EdgePriority::removeEdge(NodePair node_pair, bool remove, std::vector<std::string>& node_properties)
{
    assert(remove);

    EdgeHistory history;
    history.node1 = boost::get<0>(node_pair);
    history.node2 = boost::get<1>(node_pair);
    RagEdge_uit* edge = rag.find_rag_edge(history.node1, history.node2);
    history.weight = edge->get_weight();
    history.preserve_edge = edge->is_preserve();
    history.false_edge = edge->is_false_edge();
    history.property_list_curr.push_back(edge->get_property_ptr("location"));
    history.property_list_curr.push_back(edge->get_property_ptr("edge_size")); 

    // node id that is kept
    history.remove = true;
    RagNode_uit* node1 = rag.find_rag_node(history.node1);
    RagNode_uit* node2 = rag.find_rag_node(history.node2);
    history.size1 = node1->get_size();
    history.size2 = node2->get_size();

    for (int i = 0; i < node_properties.size(); ++i) {
        std::string property_type = node_properties[i];
        try {
            PropertyPtr property = node1->get_property_ptr(property_type);
            history.node_properties1[property_type] = property;
        } catch(...) {
            //
        }
        try {
            PropertyPtr property = node2->get_property_ptr(property_type);
            history.node_properties2[property_type] = property;
        } catch(...) {
            //
        }
    }

    for (RagNode_uit::edge_iterator iter = node1->edge_begin(); iter != node1->edge_end(); ++iter) {
        RagNode_uit* other_node = (*iter)->get_other_node(node1);
        if (other_node != node2) {
            history.node_list1.push_back(other_node->get_node_id());
            history.edge_weight1.push_back((*iter)->get_weight());
            history.preserve_edge1.push_back((*iter)->is_preserve());
            history.false_edge1.push_back((*iter)->is_false_edge());

            std::vector<boost::shared_ptr<Property> > property_list;
            property_list.push_back((*iter)->get_property_ptr("location"));  
            property_list.push_back((*iter)->get_property_ptr("edge_size"));  
            history.property_list1.push_back(property_list);
        }
    } 

    for (RagNode_uit::edge_iterator iter = node2->edge_begin(); iter != node2->edge_end(); ++iter) {
        RagNode_uit* other_node = (*iter)->get_other_node(node2);
        if (other_node != node1) {
            history.node_list2.push_back(other_node->get_node_id());
            history.edge_weight2.push_back((*iter)->get_weight());
            history.preserve_edge2.push_back((*iter)->is_preserve());
            history.false_edge2.push_back((*iter)->is_false_edge());

            std::vector<boost::shared_ptr<Property> > property_list;
            property_list.push_back((*iter)->get_property_ptr("location"));  
            property_list.push_back((*iter)->get_property_ptr("edge_size"));  
            history.property_list2.push_back(property_list);
        }
    }

    rag_merge_edge(rag, edge, node1, property_names); 
    history_queue.push_back(history);
}

}

#endif

