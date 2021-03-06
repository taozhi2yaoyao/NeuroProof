

#include "IterativeLearn_semi.h"

using namespace NeuroProof;


IterativeLearn_semi::IterativeLearn_semi(BioStack* pstack, string psession_name, string pclfr_name): IterativeLearn(pstack, pclfr_name),
				  INITPCT_SM(0.035), CHUNKSZ_SM(10), w_dist_thd(5) {
    trn_idx.clear();
    edgelist.clear();
    initial_set_strategy = INITIAL_METHOD_DEGREE;
    parallel_mode = 0;
    
    if (!(feature_mgr->get_classifier())){
	EdgeClassifier* eclfr = new OpencvRFclassifier();
      
	feature_mgr->set_classifier(eclfr);
    }
    nfeat_channels = feature_mgr->get_num_channels();
    printf("num feat channels: %u\n", nfeat_channels);
    
    string param_filename= psession_name + "/param_semi.txt";
    printf("%s\n",param_filename.c_str());
    FILE* fp = fopen(param_filename.c_str(),"rt");
    if(!fp)
	printf("No param file for semi-supervised\n");
    else{
	fscanf(fp, "%lf %lf %lf %d", &INITPCT_SM, &CHUNKSZ_SM, &w_dist_thd, &parallel_mode);
	fclose(fp);
	printf("initial # edge= %.5lf, chunksz=%.1lf, max weight dist=%.5lf \n", INITPCT_SM, CHUNKSZ_SM, w_dist_thd);
    }
    
    
    std::vector< std::vector<double> >& all_features = dtst.get_features();
    std::vector< int >& all_labels = dtst.get_labels();
    compute_all_edge_features(all_features, all_labels);
    dtst.initialize();
    
    std::vector<unsigned int> ignore_list;
    feature_mgr->find_useless_features(all_features, ignore_list);
    
    printf("total edges generated: %u\n",all_features.size());
    printf("total features: %u\n",all_features[0].size());
    
    std::srand ( unsigned ( std::time(0) ) );

    
     
    
    std::time_t start, end;
    std::time(&start);

    /*C* Assuming feature format: node 1 feat, node 2 feat, edges feat and diff feat
     * The first feature for each of these three components is size which we ignore
     * followed by 4 moments and 5 percentiles for each channel
     /**/ 
    
   

    printf("ignore features for weight matrix only:");
    for(size_t ff=0 ;ff<ignore_list.size(); ff++)
	printf("%u ", ignore_list[ff]);
    printf("\n");
    
    wt1 = new WeightMatrix1(w_dist_thd, ignore_list);
    wt1->weight_matrix_parallel(all_features, false);
    
//     wt2 = new WeightMatrix2(w_dist_thd, ignore_list);
//     wt2->weight_matrix_parallel(all_features, false);
    
    
    std::time(&end);	
    printf("Time needed to compute W : %.2f min\n", (difftime(end,start))*1.0/60);
    printf("nonzero ratio: %.4f\n", 100*wt1->nnz_pct());
    
}

void IterativeLearn_semi::get_initial_edges(std::vector<unsigned int>& new_idx){

    std::vector< std::vector<double> >& all_features = dtst.get_features();
    size_t start_with = (size_t) (INITPCT_SM*all_features.size());
    if (initial_set_strategy == INITIAL_METHOD_DEGREE)
	//*C* initial samples by large degree
	wt1->find_large_degree(init_trn_idx);
    else if  (initial_set_strategy == INITIAL_METHOD_KMEANS){ 
	//*C* initial samples by clustering, e.g., kmeans.
	std::vector< std::vector<double> > scaled_features;
	wt1->scale_features(all_features, scaled_features);
	
	kMeans km(start_with, 100, 1e-2);
	km.compute_centers(scaled_features, init_trn_idx);
    }
    
    new_idx = init_trn_idx;
    if (new_idx.size()>start_with)
	new_idx.erase(new_idx.begin()+ start_with, new_idx.end());
    
    if (init_trn_idx.size()> start_with)
	init_trn_idx.erase(init_trn_idx.begin(), init_trn_idx.begin()+start_with);
//     else
// 	init_trn_idx.erase(init_trn_idx.begin(), init_trn_idx.end());
    
        
}

void IterativeLearn_semi::read_saved_edges(string& psession_name, std::vector<unsigned int>& saved_idx, std::vector<int>& saved_lbl)
{

    string param_filename= psession_name + "/all_labeled_edges.txt";
    printf("%s\n",param_filename.c_str());
    FILE* fp = fopen(param_filename.c_str(),"rt");
    if(!fp){
	printf("No saved labeled edges\n");
	return;
    }
    else{
	saved_idx.clear();
	saved_lbl.clear();
      
	std::map<unsigned int, std::vector< IdxLbl > > prev_edges;
	size_t count=0;
	while (!feof(fp)){
	    unsigned int idx1, idx2;
	    int label1;
	    fscanf(fp, "%u  %u  %d\n", &idx1, &idx2, &label1);
	    count++;
	    unsigned int max_idx = (idx1 >= idx2? idx1 : idx2);
	    unsigned int min_idx = (max_idx == idx1? idx2 : idx1);
	    
	    if (prev_edges.find(max_idx) != prev_edges.end()){
	      
		prev_edges[max_idx].push_back(IdxLbl(min_idx, label1));
	    }
	    else{
		std::vector<IdxLbl> edge1;
		edge1.push_back(IdxLbl(min_idx, label1));
		prev_edges.insert(std::make_pair(max_idx, edge1));
	    }
	    
	}
	fclose(fp);
	printf("number of already labeled edges = %u \n", count);
	
	for(size_t i=0; i< edgelist.size(); i++){
	    unsigned int idx1 = edgelist[i].first;
	    unsigned int idx2 = edgelist[i].second;
	    
	    unsigned int max_idx = (idx1 >= idx2? idx1 : idx2);
	    unsigned int min_idx = (max_idx == idx1? idx2 : idx1);
	    
	    if (prev_edges.find(max_idx) != prev_edges.end()){
		std::vector<IdxLbl>& edges1 = prev_edges[max_idx];
		for(size_t j=0; j< edges1.size(); j++){
		    if (edges1[j].idx == min_idx){
			saved_idx.push_back(i);
			saved_lbl.push_back(edges1[j].lbl);
		    }
		}
	    }
	    
	}
    }
    
        
}



void IterativeLearn_semi::compute_new_risks(std::multimap<double, unsigned int>& risks)
{

    feature_mgr->get_classifier()->learn(cum_train_features, cum_train_labels); // number of trees

//     wt2->solve(m_prop_lbl);

    std::time_t start, end;
    std::time(&start);	
//     std::map<unsigned int, double> prop_lbl1;
    wt1->AMGsolve(m_prop_lbl);
    std::time(&end);	
    printf("C: Time to solve linear equations: %.2f sec\n", (difftime(end,start))*1.0);
    
//     std::map<unsigned int, double>::iterator plit1=prop_lbl1.begin();
//     double max_diff=0;
//     for(; plit1 != prop_lbl1.end(); plit1++){
// 	unsigned int idx1 = plit1->first;
// 	double val1 = plit1->second;
// 	
// 	if (m_prop_lbl.find(idx1) == m_prop_lbl.end()){
// 	    printf("prop lbl: entry does not exist\n");
// 	    exit(0);
// 	}
// 	double val2 = m_prop_lbl[idx1];
// 	max_diff = ((fabs(val1-val2) > max_diff) ? fabs(val1-val2) : max_diff);
//     }
//     printf("maxdiff= %.5lf\n", max_diff);

    
    
    std::vector< std::vector<double> >& all_features = dtst.get_features();

    double prop_diff=0;


    unsigned int idx;
    double pp;

    m_dis_pred.clear();
    std::map<unsigned int, double>::iterator plit;
    for(plit = m_prop_lbl.begin(); plit != m_prop_lbl.end(); plit++){
	
	idx = plit->first;
	pp = plit->second;
	
	double pp_clipped = (pp > 1.0)? 1.0 : pp;
	pp_clipped = (pp_clipped < -1.0)? -1.0: pp;
	
	
	double rf_p1 = feature_mgr->get_classifier()->predict(all_features[idx]);
	double rf_p = 2*(rf_p1-0.5);

	m_dis_pred.insert(std::make_pair(idx, rf_p));  
	
	double risk = rf_p * pp_clipped ;
	
	risks.insert(std::make_pair(risk, idx));
    }
}

void IterativeLearn_semi::get_next_edge_set(size_t feat2add, std::vector<unsigned int>& new_idx)
{
    new_idx.clear();
    
    std::multimap<double, unsigned int> risks;
    compute_new_risks(risks);
    
    std::multimap<double, unsigned int>::iterator rit = risks.begin();
    for(size_t ii=0; ii < feat2add && rit != risks.end() ; ii++, rit++){
	unsigned int idx = rit->second;
	new_idx.push_back(idx);
    }
    
}



void IterativeLearn_semi::update_new_labels(std::vector<unsigned int>& new_idx,
					    std::vector<int>& new_lbl)
{
    printf("Label propagated for %u edges\n",m_prop_lbl.size());
    printf("Classifier predicted for %u edges\n",m_dis_pred.size());
    size_t nn_incorrect=0, rf_incorrect=0;
    for(size_t i=0; i<new_idx.size() && m_prop_lbl.size()>0;i++){
	int lbl1 = new_lbl[i];
	unsigned int idx1 = new_idx[i];
	double pred = m_dis_pred[idx1];
	double prop = m_prop_lbl[idx1];
	
	nn_incorrect += (((prop > 0) && (lbl1 < 0)) ? 1 : 0);
	nn_incorrect += (((prop < 0) && (lbl1 > 0)) ? 1 : 0);
	
	rf_incorrect += (((pred > 0) && (lbl1 < 0)) ? 1 : 0);
	rf_incorrect += (((pred < 0) && (lbl1 > 0)) ? 1 : 0);
    }
    printf("rf incorrect: %u, nn incorrect:%u\n",rf_incorrect, nn_incorrect);
  
    trn_idx.insert(trn_idx.end(), new_idx.begin(), new_idx.end());
    dtst.append_trn_labels(new_lbl);
    dtst.get_train_data(trn_idx, cum_train_features, cum_train_labels);

//     wt2->add2trnset(new_idx, new_lbl);
    
    std::time_t start, end;
    std::time(&start);	
    wt1->add2trnset(trn_idx, cum_train_labels);
//     threadp = new boost::thread(&WeightMatrix1::add2trnset, wt1, trn_idx, cum_train_labels);
    std::time(&end);	
    printf("C: Time to update: %.2f sec\n", (difftime(end,start))*1.0);
    
}
void IterativeLearn_semi::save_classifier(string& psession_name)
{
  
    string classifier_name = psession_name + "/sp_classifier_"; 
    char buffer[1024];
    size_t len_trn_idx = trn_idx.size();
    sprintf(buffer, "%u", len_trn_idx);
    
    classifier_name += buffer;
    classifier_name += ".xml";
    feature_mgr->get_classifier()->save_classifier(classifier_name.c_str()); // number of trees
    
}


void IterativeLearn_semi::get_extra_edges(std::vector<unsigned int>& ret_idx, size_t nedges)
{
  
    ret_idx.clear();
    
    if(init_trn_idx.size() > nedges)
	ret_idx.insert(ret_idx.begin(), init_trn_idx.begin(), init_trn_idx.begin()+nedges);
    else
	ret_idx.insert(ret_idx.begin(), init_trn_idx.begin(), init_trn_idx.end());
    
}


void IterativeLearn_semi::learn_edge_classifier(double trnsz){
    
    std::vector<unsigned int> new_idx;

    get_initial_edges(new_idx);
//     std::vector< std::pair<Node_t, Node_t> > tmpedgebuffer;
//     edgelist_from_index(new_idx, tmpedgebuffer);
    
    std::vector<int> new_lbl(new_idx.size());
    std::vector< int >& all_labels = dtst.get_labels();
    for(size_t ii=0; ii < new_idx.size() ; ii++){
	unsigned int idx = new_idx[ii];
	new_lbl[ii] = all_labels[idx];
    }
    update_new_labels(new_idx, new_lbl);
    
    // ***********//  
    
    
    double remaining = trnsz - trn_idx.size();
    std::vector< std::vector<double> >& all_features = dtst.get_features();
    
    std::time_t start, end;
    size_t chunksz = CHUNKSZ_SM;
    size_t nitr = (size_t) (remaining/chunksz);
    size_t st=0;
    size_t multiple_IVAL=1;
    //string clfr_name = pclfr_name; 
    double thd_s = 0.3;
    
    do{
      
	std::multimap<double, unsigned int> risks;
	compute_new_risks(risks);
	
	/*Debug*/
	printf("rf accuracy\n");
	dtst.get_test_data(trn_idx, rest_features, rest_labels);
	evaluate_accuracy(rest_features,rest_labels, 0.3);
	printf("nn accuracy\n");
	std::vector<int> tmp_lbl;
	std::vector<double> tmp_pred;
	std::map<unsigned int, double>::iterator plit;
	for(plit = m_prop_lbl.begin(); plit != m_prop_lbl.end(); plit++){
	    unsigned int idx = plit->first;
	    double pp = plit->second;
	    tmp_lbl.push_back(all_labels[idx]);
	    tmp_pred.push_back(pp);
	}
	evaluate_accuracy(tmp_lbl,tmp_pred, 0.0);
      
	/**/
	if((clfr_name.size()>0) && cum_train_features.size()>(multiple_IVAL*IVAL)){
	    update_clfr_name(clfr_name, multiple_IVAL*IVAL);
	    feature_mgr->get_classifier()->save_classifier(clfr_name.c_str());  
	    multiple_IVAL++;
	}
      
	new_idx.clear();
	new_lbl.clear();
	
	
	
	unsigned int feat2add = chunksz;
	unsigned int npos = 0 , nneg = 0;
	unsigned int rf_incorrect=0;
	unsigned int nn_incorrect=0;
// 	std::multimap<double, unsigned int>::reverse_iterator rit = risks.rbegin();
// 	for(size_t ii=0; ii < feat2add && rit != risks.rend() ; ii++, rit++){
	std::multimap<double, unsigned int>::iterator rit = risks.begin();
	for(size_t ii=0; ii < feat2add && rit != risks.end() ; ii++, rit++){
	    unsigned int idx = rit->second;
	    new_idx.push_back(idx);
	    
	    new_lbl.push_back(all_labels[idx]);
	    if (all_labels[idx] > 0) 
	      npos++;
	    else 
	      nneg++;
	    
	    /*debug*/
	    int lbl1 = all_labels[idx];
	    double pp_clipped = m_prop_lbl[idx];
	    double rf_p1 = feature_mgr->get_classifier()->predict(all_features[idx]);
	    double rf_p = 2*(rf_p1-0.5);
	    
	    
	    if ((lbl1>0) && (rf_p<0) && (pp_clipped>0))
		rf_incorrect++;
	    else if ((lbl1<0) && (rf_p>0) && (pp_clipped<0))
		rf_incorrect++;
	    
	    if ((lbl1>0) && (rf_p>0) && (pp_clipped<0))
		nn_incorrect++;
	    else if ((lbl1<0) && (rf_p<0) && (pp_clipped>0))
		nn_incorrect++;
	    /**/
	    
	    
	}
	printf("Added samples, pos: %u, neg:%u\n",npos, nneg);
	printf("Added--rf incorrect: %u, nn incorrect:%u\n",rf_incorrect, nn_incorrect);
	
	update_new_labels(new_idx, new_lbl);
	
	st++;

    }while(st<nitr);
    
    
    dtst.get_test_data(trn_idx, rest_features, rest_labels);
    evaluate_accuracy(rest_features,rest_labels, thd_s);
    double npos=0, nneg=0;
    for(size_t ii=0; ii<cum_train_labels.size(); ii++){
	npos += (cum_train_labels[ii]>0?1:0);
	nneg += (cum_train_labels[ii]<0?1:0);
    }

    printf("number of positive : %.1lf, negative: %.1lf\n",npos,nneg);
    
    /*C* debug
    fp= fopen("selected_trnidxC.txt","wt");
    for(size_t ii=0; ii < trn_idx.size(); ii++)
	fprintf(fp,"%u\n",trn_idx[ii]);
    fclose(fp);
    
    //*C*/
    
    
}
