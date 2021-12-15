#include <iostream>
#include <math.h>
//#include <vector>
#include <stdlib.h>
#include <random>
#include <string>
#include <chrono>
#include "Bitmap.hpp"
#include "Ant.hpp"
#include "2opt.hpp"
#include "NN.hpp"
#include "utils.hpp"

// #include "2opt.cpp"

// How mow to exploit the cumulated knowledge
#define ALPHA 1.0//0.9
// How much to explore based on distances
#define BETA 2.0//1.9
#define TIMED 1

// TODO CHECK CODE
int get_next_best_city(int i, std::vector<std::vector<double> > *phero, std::vector<std::vector<double> > *dist,  Bitmap * visited) {
    double best_exploitation = 0.0;
    int n = (*dist).size();
    int best_c = -1;
    double curr_ = 0.0;
    bool flag = false;
    for(int j=0;j<n;++j) {
        if ( (*visited).get(j) == 0 && i != j) {
            flag = true;

            curr_ = pow((*phero)[i][j], ALPHA)  * pow( (1.0/(*dist)[i][j]) , BETA);
            
            if (curr_ > best_exploitation) {
                best_exploitation = curr_;
                best_c = j;
            }
        }
    }
    if (best_c == -1) {
        std::cout << "PROBLEM; no best city found" << std::endl;
        return 0;
    }
    return best_c;
}

int get_next_city(int i, int to_ignore, double prob, std::vector<std::vector<double> > *phero, std::vector<std::vector<double> > *dist,  Bitmap * visited) {
    int n = (*dist).size();
    // get denominator
    double tau_tot = 0.0;
    for (int k=0;k<n;++k) {
        // TODO SKIP IGNORED CITY??
        if ( (*visited).get(k) == 0 && i != k ) {
            tau_tot += pow((*phero)[i][k], ALPHA) * pow((1.0/(*dist)[i][k]), BETA);
        }
    }

    double prev = 0.0;
    double curr_prob = 0.0;
    for(int j=0;j<n;++j) {
        if ( (*visited).get(j) == 0 && i != j ) {// if not visited and not the best city
            curr_prob = prev + (pow((*phero)[i][j], ALPHA) * pow((1.0/(*dist)[i][j]), BETA))/tau_tot;
            if (prob <= curr_prob) {
                return j;
            }
            prev = curr_prob;
        }
    }
    // if no best city found, it means we finished the loop without finding a city to explore
    // this is a tricky case in which the best and the last city to visit are the same, so we can not ignore it
    return to_ignore;
}

int aco_solution_improved(const char * problem, unsigned seed, int ants_number, bool print_path, bool cleanup) {
    int best_known = 0;
    std::vector<std::vector<double> > *mat = get_matrix(problem, &best_known);
    int n_cities = (*mat).size();
    double Q_0 = 1.0-(20.0/double(n_cities));
    // double Q_0 = 0.90;
    // double Q_0 = 0.93;
    double local_evaporate = 0.3;//03
    double global_evaporate = 0.1;//02
    std::vector<int> best_global_path(n_cities);
    
    // get initial pheromone in relation with the size of the NN
    int nn_len =  NN(mat, 0);
    
    int best_global_length = nn_len;
    double tau_0 = pow((n_cities*double(nn_len)),-1);

    std::vector<std::vector<double> > phero = std::vector<std::vector<double> >(n_cities, std::vector<double> (n_cities, tau_0));

    // initialize ants
    std::vector<Ant *> ants(ants_number);
    for(int a=0; a<ants_number; ++a) {
        ants[a] = new Ant(n_cities);
    }
    std::default_random_engine generator (seed);
    std::uniform_real_distribution<double> distr(0, 1);

    int n_iterations = 100000;

    #if TIMED
        long long totalMs = 0;
    #endif

    for (int i=0; i < n_iterations; ++i) { // for each iteration:
        #if TIMED
            auto start = std::chrono::high_resolution_clock::now();
        #endif
        
        // std::cout << i << std::endl;
        
        // reset ants and position them randomly:
        for(int a=0; a < ants_number; a++) {
            ants[a]->visited.clear();
            ants[a]->tour_len = 0;
            int ant_city = distr(generator) * n_cities;
            ants[a]->tour[0] = ant_city;
            ants[a]->visited.set(ant_city,1);
        }

        // make each ant make a step untill they all make a tour
        for(int c=1; c < n_cities; c++) {

            for(int a=0; a < ants_number; a++) {
                int last_ant_city = ants[a]->tour[c-1];

                // select next city for ant a
                int next_city = get_next_best_city(last_ant_city, &phero, mat, &(ants[a]->visited));
                double rand_0_1_q = distr(generator);
                if (rand_0_1_q > Q_0) { // explore and ignore best city
                    double rand_0_1_c = distr(generator);
                    next_city = get_next_city(last_ant_city, next_city, rand_0_1_c, &phero, mat, &(ants[a]->visited));
                }
                ants[a]->tour[c] = next_city;
                ants[a]->tour_len += (*mat)[last_ant_city][next_city];
                ants[a]->visited.set(next_city, 1);

                // local pheromone update
                double old_phero = phero[last_ant_city][next_city];
                phero[last_ant_city][next_city] = (1.0 - local_evaporate)*old_phero + local_evaporate*tau_0;
                phero[next_city][last_ant_city] = phero[last_ant_city][next_city];
            }
        }
        // add last edge and get best tour
        int best_ant_len = std::numeric_limits<int>::max();;
        int best_a = -1;

        for(int a=0; a<ants_number; a++) {
            int city_i = ants[a]->tour[0];
            int city_j = ants[a]->tour[n_cities-1];
            //add the last local update
            double old_phero = phero[city_i][city_j];
            phero[city_i][city_j] = (1.0 - local_evaporate)*old_phero + local_evaporate*tau_0;
            phero[city_j][city_i] = phero[city_i][city_j];
            // TODO add last local pheromone update??? it is never set
            ants[a]->tour_len += (*mat)[city_i][city_j];
            // if(ants[a]->tour_len < best_global_length) { // TODO Bug updatest best LAST ant
            //     best_global_length = ants[a]->tour_len; // TODO move this assignment to be done only once
                
            // }
            if(ants[a]->tour_len < best_ant_len) {
                best_ant_len = ants[a]->tour_len;
                best_a = a;
            }
        }
        if (best_ant_len < best_global_length) {
            best_global_length = best_ant_len;
            best_global_path = ants[best_a]->tour;
        }

        // apply local search (2opt) with best ant (theoretically any ant or best k ants can work)
        // an ant that is not currently the best could be placed in a local area where the global minimum is.
        // Apply 2opt one time to the best and and the other time to a random ant
        Ant * lucky_ant;
        int rand_0_1_ant = distr(generator)* ants_number;
        lucky_ant = ants[rand_0_1_ant];
        if(!(i%4==0) || best_a == -1) {
            int rand_0_1_ant = distr(generator)* ants_number;
            lucky_ant = ants[rand_0_1_ant];
        } else {
            lucky_ant = ants[best_a];
        }

        int possible_best_length = loop2opt(&(lucky_ant->tour), mat, lucky_ant->tour_len);
        if (possible_best_length < best_global_length) {
            best_global_length = possible_best_length;
            best_global_path = lucky_ant->tour;
            std::cout << "BEST GLOBAL " << best_global_length << std::endl;
            std::cout << "At iteration: "<< i << std::endl;
        }
        if (best_known == best_global_length) goto give_solution;
        


        // //global evaporate on all edges (not used usually)
        // for(int i=0; i<n_cities; i++) {
        //     for(int j=0; j<n_cities; j++) {
        //         double old_phero = phero[i][j];
        //         phero[i][j] = phero[j][i] = (1.0 - global_evaporate)*old_phero;
        //         // phero[j][i] = phero[i][j];
        //         //  = phero[i][j];
        //     }
        // }
        
        // global update on best tour
        for(int k=0; k<n_cities-1; k++) {
            int city_i = best_global_path[k];
            int city_j = best_global_path[k+1];
            double old_phero = phero[city_i][city_j];
            phero[city_i][city_j] = (1.0 - global_evaporate)*old_phero + global_evaporate  * 1.0/best_global_length;
            // phero[city_i][city_j] = global_evaporate  * 1.0/best_global_length;
            phero[city_j][city_i] = phero[city_i][city_j];
        }
        // update last edge
        int city_i = best_global_path[0];
        int city_j = best_global_path[n_cities-1];
        double old_phero = phero[city_i][city_j];
        phero[city_i][city_j] = (1.0 - global_evaporate)*old_phero + global_evaporate  * 1.0/best_global_length;
        // phero[city_i][city_j] = global_evaporate  * 1.0/best_global_length;
        phero[city_j][city_i] = phero[city_i][city_j];

        #if TIMED
            auto end = std::chrono::high_resolution_clock::now();
            auto currItr = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            totalMs += currItr;
            auto avgIterTime = totalMs / (i + 1);
            if (totalMs + avgIterTime >= 176000) {
                goto give_solution;
            } 
        #endif
        // std::cout << "BEST GLOBAL" << best_global_length << std::endl;

    }
    give_solution:
    std::cout << "Best lenght found: " << best_global_length << std::endl;
    std::cout << "GAP IS " << gap(best_global_length, best_known) <<std::endl;
    if(print_path) {
        for(int i=0;i<n_cities;i++) {
            std::cout << best_global_path[i] << ',';
        }
    }
    // cleanup dynamic memory
    if(cleanup) {
        cleanUpAnts(ants);
        cleanUpMatrix(mat);
    }
    return best_global_length;
}


int main(int argc, const char ** argv) {
    unsigned seed = 123456;
    if(argc == 3) {
        seed = atoi(argv[2]);
        std::cout << "seed" << seed << std::endl;
    }

    // for (int i=0; i < 100; i++) {
    //     std::cout << "Seed:" <<i << std::endl;
    //     int solution = aco_solution_improved("../AI_cup_2021_problems/u1060.tsp", i, 12, true, true);
    //     std::cout << std::endl;
    // }
   

    int solution = aco_solution_improved(argv[1], seed, 12, true, false);
    
}