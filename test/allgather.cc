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
    int N = atoi(argv[1]);
    int world_size = rdc::GetWorldSize();
    int rank = rdc::GetRank();
    std::vector<std::vector<int>> a(world_size);
    for (int i = 0; i < world_size; ++i) {
        a[i].resize(i + N);
        if (i == rank) {
            for (int j = 0; j < i + N; ++j) {
                a[i][j] = i + j;
            }
        }
    }
    rdc::Allgather(a);
    for (int i = 0; i < world_size; ++i) {
        for (int j = 0; j < i + N; ++j) {
            CHECK_F(a[i][j] == i + j);
        }
    }
    Finalize();
}
