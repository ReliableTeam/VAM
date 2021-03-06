#include "rcpp_sim_vam.h"
#include "rcpp_stop_policy.h"


DataFrame SimVam::get_last_data() {
    //printf("size:%d,%d\n",(model->time).size(),model->k+1);
    if((model->time).size() > model->k+1) {
        size = model->k+1;
        //printf("data size:%d\n",size);
        (model->time).resize(size);
        (model->type).resize(size);
    }
    return DataFrame::create(_["Time"]=model->time,_["Type"]=model->type);
}

//for calls of simulate from R with NULL vectors for TimeInit and TypeInit don't forget to write
//something like simulate(10,as.numeric(c()),as.numeric(c())) otherwise there will be an error
DataFrame SimVam::simulate(int nbsim, NumericVector TimeInit, NumericVector TypeInit) {
    init(nbsim);
    RNGScope rngScope;
    int idMod;

    //DEBUG[distrib type1]: int type1CptAV=0,typeCptAV=0,type1CptAP=0,typeCptAP=0;

    NumericVector::iterator itTimeInit=TimeInit.begin();
    NumericVector::iterator itTypeInit=TypeInit.begin();

    stop_policy->first();

    if(model->maintenance_policy != NULL) model->maintenance_policy->first();

    //for the simulation of the futur of a data set, initialize with the event befor the one to simulate
    while((itTimeInit<TimeInit.end())&(stop_policy->ok())){
        if(*(itTypeInit)==0){
          break;
        }
        resize();
        model->time[model->k + 1]=*(itTimeInit);
        model->type[model->k + 1]=*(itTypeInit);
        if(*(itTypeInit)==-1){
          idMod=0;
        } else {
          idMod=*(itTypeInit);
        }
        //# used in the next update
        model->update_Vleft(false,false);

        //# update the next k, and save model in model too!
        model->models->at(idMod)->update(false,false);
        itTypeInit++;
        itTimeInit++;
    }

    while(stop_policy->ok()) {//model->k < nbsim) {
        //printf("k=%d\n",model->k);
        //To dynamically increase the size of simulation
        resize();
        //printf("k2=%d\n",model->k);

        //### modAV <- if(Type[k]<0) obj$vam.CM[[1]]$model else obj$vam.PM$models[[obj$data$Type[k]]]
        //# Here, obj$model$k means k-1
        //#print(c(obj$model$Vleft,obj$model$Vright))
        double u=log(runif(1))[0];
        if(model->nb_paramsCov>0) u *= compute_covariates();//set_current_system launched in R for simulation
        double timePM= 0.0, timeCM = model->virtual_age_inverse(model->family->inverse_cumulative_hazardRate(model->family->cumulative_hazardRate(model->virtual_age(model->time[model->k]))-u));
        //TODO: submodels

        List timeAndTypePM;
        if(model->maintenance_policy != NULL) {
            timeAndTypePM = model->maintenance_policy->update(model); //# Peut-??tre ajout Vright comme argument de update
            //timeAndTypePM = model->maintenance_policy->update(model->time[model->k]); //# Peut-??tre ajout Vright comme argument de update

            //DEBUG[distrib type1]:
            //NumericVector tmp0=timeAndTypePM["type"];
            //int type0PM=tmp0[0];
            //typeCptAV++;if(type0PM==1) type1CptAV++;

            NumericVector tmp=timeAndTypePM["time"];
            timePM=tmp[0];
            //DEBUG[distrib type1]: printf("sim: timePM(%d):%lf, timeCM=%lf\n",type0PM,timePM,timeCM);
            if(timePM<timeCM && timePM<model->time[model->k]) {
          		printf("Warning: PM ignored since next_time(=%lf)<current_time(=%lf) at rank %d.\n",timePM,model->time[model->k],model->k);
            }
        }
        if(model->maintenance_policy == NULL || timeCM < timePM || timePM<model->time[model->k]) {
            model->time[model->k + 1]=timeCM;
            model->type[model->k + 1]=-1;
            idMod=0;
        } else {
            model->time[model->k + 1]=timePM;
            NumericVector tmp2=timeAndTypePM["type"];
            int typePM=tmp2[0];
            //DEBUG[distrib type1]: typeCptAP++;if(typePM==1) type1CptAP++;printf("typePM=%d\n",typePM);
            model->type[model->k + 1]=typePM;
            idMod=timeAndTypePM["type"];
        }
        //printf("k=%d: cm=%lf,pm=%lf\n",model->k,timeCM,timePM);
        //# used in the next update
        model->update_Vleft(false,false);


        //# update the next k, and save model in model too!
        model->models->at(idMod)->update(false,false);

    }
    //DEBUG[distrib type1]: printf("cpt: %d/%d et %d/%d\n",type1CptAV,typeCptAV,type1CptAP,typeCptAP);

    return get_last_data();
}

void SimVam::add_stop_policy(List policy) {
    stop_policy=newStopPolicy(this,policy);
}

void SimVam::init(int cache_size_) {
    int i;
    // Almost everything in the 5 following lines are defined in model->init_computation_values() (but this last one initializes more than this 5 lines)
    model->Vright=0;
    model->A=1;
    model->k=0;
    for(i=0;i<model->nbPM + 1;i++) model->models->at(i)->init();

    size=cache_size_+1;cache_size=cache_size_;
    model->idMod=0; // Since no maintenance is possible!
    //model->time=rep(0,size);
    //model->type= rep(1,size);
    (model->time).clear();
    (model->type).clear();
    (model->time).resize(size,0);
    (model->type).resize(size,1);
}

#define print_vector(x)                                                                     \
    for (std::vector<double>::const_iterator i = x.begin(); i != x.end(); ++i)   \
    std::cout << *i << ' '; \
    std::cout << "\n";

void SimVam::resize() {
    if(model->k > size-2) {
        //printf("RESIZE!\n");
        //print_vector((model->time))
        //printf("SIZE=%d",size);
        size += cache_size;//printf("->%d\n",size);
        (model->time).resize(size);
        //print_vector((model->time))
        //printf("model->SIZE=%d\n",(model->time).size());
        (model->type).resize(size);
    }
}
