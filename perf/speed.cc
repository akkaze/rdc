// This program is used to test the speed of rdc API
#include <rdc.h>
#include <time.h>
#include <utils/timer.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace rdc;

double max_tdiff, sum_tdiff, bcast_tdiff, tot_tdiff;

inline void TestMax(size_t n) {
    int rank = rdc::GetRank();
    std::vector<float> ndata(n);
    for (size_t i = 0; i < ndata.size(); ++i) {
        ndata[i] = (i * (rank + 1)) % 111;
    }
    double tstart = utils::GetTimeInMs();
    //rdc::Allreduce<op::Max>(&ndata[0], ndata.size());
    max_tdiff += utils::GetTimeInMs() - tstart;
}

inline void TestSum(size_t n) {
    int rank = rdc::GetRank();
    const int z = 131;
    std::vector<float> ndata(n);
    for (size_t i = 0; i < ndata.size(); ++i) {
        ndata[i] = (i * (rank + 1)) % z;
    }
    double tstart = utils::GetTimeInMs();
    //rdc::Allreduce<op::Sum>(&ndata[0], ndata.size());
    sum_tdiff += utils::GetTimeInMs() - tstart;
}

inline void TestBcast(size_t n, int root) {
    int rank = rdc::GetRank();
    std::string s;
    s.resize(n);
    for (size_t i = 0; i < n; ++i) {
        s[i] = char(i % 126 + 1);
    }
    std::string res;
    res.resize(n);
    if (root == rank) {
        res = s;
    }
    double tstart = utils::GetTimeInMs();
    //rdc::Broadcast(&res[0], res.length(), root);
    bcast_tdiff += utils::GetTimeInMs() - tstart;
}

inline void PrintStats(const char *name, double tdiff, int n, int nrep,
                       size_t size) {
    int nproc = rdc::GetWorldSize();
    double tsum = tdiff;
    //rdc::Allreduce<op::Sum>(&tsum, 1);
    double tavg = tsum / nproc;
    double tsqr = tdiff - tavg;
    tsqr *= tsqr;
    //rdc::Allreduce<op::Sum>(&tsqr, 1);
    double tstd = sqrt(tsqr / nproc);
    if (rdc::GetRank() == 0) {
        std::string msg = std::string(name) + ": " +
                          "mean=" + std::to_string(tavg) + ": " +
                          "std=" + std::to_string(tstd) + " millisec";
        rdc::TrackerPrint(msg);
        double ndata = n;
        ndata *= nrep * size;
        if (n != 0) {
            std::string msg =
                std::string(name) + "-speed: " +
                std::to_string((ndata / tavg) / 1024 / 1024 * 1000) + " MB/sec";
            rdc::TrackerPrint(msg);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: <ndata> <nrepeat>\n");
        return 0;
    }
    srand(0);
    int n = atoi(argv[1]);
    int nrep = atoi(argv[2]);
    CHECK_F(nrep >= 1, "need to at least repeat running once");
    rdc::Init(argc, argv);
    // int rank = rdc::GetRank();
    max_tdiff = sum_tdiff = bcast_tdiff = 0;
    //    rdc::Barrier();
    double tstart = utils::GetTimeInMs();
    for (int i = 0; i < nrep; ++i) {
        TestMax(n);
        TestSum(n);
        //  TestBcast(n, rand() % nproc);
    }
    tot_tdiff = utils::GetTimeInMs() - tstart;
    // use allreduce to get the sum and std of time
    PrintStats("max_tdiff", max_tdiff, n, nrep, sizeof(float));
    PrintStats("sum_tdiff", sum_tdiff, n, nrep, sizeof(float));
    // PrintStats("bcast_tdiff", bcast_tdiff, n, nrep, sizeof(char));
    PrintStats("tot_tdiff", tot_tdiff, 0, nrep, sizeof(float));
    rdc::Finalize();
    return 0;
}
