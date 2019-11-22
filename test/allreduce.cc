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
    rdc::Init(argc, argv);
    rdc::NewCommunicator(rdc::kMainCommName);
    int N = 3;
    if (argc >= 2) N = atoi(argv[1]);
    std::vector<int> a(N);
    // test allreduce max
    for (int i = 0; i < N; ++i) {
        a[i] = rdc::GetRank() + N + i;
    }
    std::vector<int> result_max(N, 0);
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < rdc::GetWorldSize(); ++j) {
            result_max[i] = std::max(result_max[i], j + N + i);
        }
    }

    LOG_F(INFO, "@node[%d] before-allreduce-max: a={%d, %d, %d}\n",
            rdc::GetRank(), a[0], a[1], a[2]);
    // allreduce take max of each elements in all processes
    Allreduce<op::Max>(&a[0], N);
    LOG_F(INFO, "@node[%d] after-allreduce-max: a={%d, %d, %d}\n",
            rdc::GetRank(), a[0], a[1], a[2]);
    for (int i = 0; i < N; ++i) {
        DCHECK_F(a[i] == result_max[i]);
    }
    std::vector<int> result_sum(N, 0);
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < rdc::GetWorldSize(); ++j) {
            result_sum[i] += (j + N + i);
        }
    }

    for (int i = 0; i < N; ++i) {
        a[i] = rdc::GetRank() + N + i;
    }
    LOG_F(INFO, "@node[%d] before-allreduce-sum: a={%d, %d, %d}\n",
            rdc::GetRank(), a[0], a[1], a[2]);
    // second allreduce that sums everything up
    Allreduce<op::Sum>(&a[0], N);
    LOG_F(INFO, "@node[%d] after-allreduce-sum: a={%d, %d, %d}\n",
            rdc::GetRank(), a[0], a[1], a[2]);
    for (int i = 0; i < N; ++i) {
        DCHECK_EQ_F(a[i], result_sum[i]);
    }
    Finalize();
    return 0;
}
