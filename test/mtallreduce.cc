/*!
 *  Copyright (c) 2018 by Contributors
 * \file basic.cc
 * \brief This is an example demonstrating what is Allreduce
 *
 * \author AnkunZheng
 */
#include <vector>
#include "rdc.h"
using namespace rdc;
int main(int argc, char *argv[]) {
//    int N = atoi(argv[1]);
//    int iter = atoi(argv[2]);
//    int nworkers = atoi(argv[3]);
//    std::vector<std::thread> thrds;
//    rdc::Init(argc, argv);
//    // idx is the thread index, from 1 to nworkers
//    auto allreduce_func = [N, iter] (int idx) {
//        std::string comm_name = std::to_string(idx);
//        if (idx != 0) {
//            NewCommunicator(comm_name);
//        } else {
//            comm_name = kWorldCommName;
//        }
//
//        std::vector<int> a(N);
//        // test allreduce max
//        for (int k = 0; k < iter; k++) {
//          for (int i = 0; i < N; ++i) {
//              a[i] = rdc::GetRank() + N + k + i;
//          }
//          std::vector<int> result_max(N, 0);
//          for (int i = 0; i < N; ++i) {
//              for (int j = 0; j < rdc::GetWorldSize(); ++j) {
//                  result_max[i] = std::max(result_max[i], j + N + k + i);
//              }
//          }
//          LOG_F(INFO, "@node[%d] before-allreduce: a={%d, %d, %d}\n",
//                  rdc::GetRank(), a[0], a[1], a[2]);
//          // allreduce take max of each elements in all processes
//          Allreduce<op::Max>(&a[0], N, comm_name);
//          LOG_F(INFO, "@node[%d] after-allreduce-max: a={%d, %d, %d}\n",
//                  rdc::GetRank(), a[0], a[1], a[2]);
//          for (int i = 0; i < N; ++i) {
//              DCHECK_F(a[i] == result_max[i]);
//          }
//          std::vector<int> result_sum(N, 0);
//          for (int i = 0; i < N; ++i) {
//              for (int j = 0; j < rdc::GetWorldSize(); ++j) {
//                  result_sum[i] += (j + N + k + i);
//              }
//          }
//
//          for (int i = 0; i < N; ++i) {
//              a[i] = rdc::GetRank() + N + k + i;
//          }
//          // second allreduce that sums everything up
//          Allreduce<op::Sum>(&a[0], N, comm_name);
//          LOG_F(INFO, "@node[%d] after-allreduce-sum: a={%d, %d, %d}\n",
//                  rdc::GetRank(), a[0], a[1], a[2]);
//          for (int i = 0; i < N; ++i) {
//              DCHECK_EQ_F(a[i], result_sum[i]);
//          }
//        }
//    };
//    for (int i = 0; i < nworkers; ++i) {
//        thrds.emplace_back(std::thread(std::bind(allreduce_func, i)));
//    }
//    for (int i = 0; i < nworkers; ++i) {
//        thrds[i].join();
//        LOG_F(INFO, "finished %d", i);
//    }
    //Finalize();
    return 0;
}
