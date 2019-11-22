// this is a test case to test whether rdc can recover model when
// facing an exception
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "rdc.h"
#include "utils/utils.h"
using namespace rdc;

// dummy model
class Model : public rdc::Serializable {
public:
    // iterations
    std::vector<float> data;
    // load from stream
    virtual void Load(rdc::Stream *fi) {
        fi->Read(&data);
    }
    /*! \brief save the model to the stream */
    virtual void Save(rdc::Stream *fo) const {
        fo->Write(data);
    }
    virtual void InitModel(size_t n) {
        data.clear();
        data.resize(n, 1.0f);
        AddLocalState("main", utils::BeginPtr(data),
                      data.size() * sizeof(float));
    }
};

inline void TestMax(Model *model, int ntrial, int iter) {
    int rank = rdc::GetRank();
    int nproc = rdc::GetWorldSize();
    const int z = iter + 111;

    std::vector<float> ndata(model->data.size());
    for (size_t i = 0; i < ndata.size(); ++i) {
        ndata[i] = (i * (rank + 1)) % z + model->data[i];
    }
    //    rdc::Allreduce<op::Max>(&ndata[0], ndata.size());

    model->data = ndata;
}

inline void TestSum(Model *model, int ntrial, int iter) {
    int rank = rdc::GetRank();
    int nproc = rdc::GetWorldSize();
    const int z = 131 + iter;

    std::vector<float> ndata(model->data.size());
    for (size_t i = 0; i < ndata.size(); ++i) {
        ndata[i] = (i * (rank + 1)) % z + model->data[i];
    }
    //    Allreduce<op::Sum>(&ndata[0], ndata.size());

    model->data = ndata;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: <ndata> <config>\n");
        return 0;
    }
    int n = atoi(argv[1]);
    rdc::Init(argc, argv);
    int rank = rdc::GetRank();
    int nproc = rdc::GetWorldSize();
    Model model;
    srand(0);
    int ntrial = 0;
    for (int i = 1; i < argc; ++i) {
        int n;
        if (sscanf(argv[i], "rdc_num_trial=%d", &n) == 1) ntrial = n;
    }
    int iter = rdc::LoadCheckPoint();
    if (iter == 0) {
        model.InitModel(n);
        LOG_F(INFO, "[%d] reload-trail=%d, init iter=%d\n", rank, ntrial, iter);
    } else {
        LOG_F(INFO, "[%d] reload-trail=%d, init iter=%d\n", rank, ntrial, iter);
    }
    for (int r = iter; r < 3; ++r) {
        TestMax(&model, ntrial, r);
        LOG_F(INFO, "[%d] !!!TestMax pass, iter=%d\n", rank, r);
        TestSum(&model, ntrial, r);
        LOG_F(INFO, "[%d] !!!TestSum pass, iter=%d\n", rank, r);
        rdc::CheckPoint();
        LOG_F(INFO, "[%d] !!!CheckPont pass, iter=%d\n", rank, r);
    }
    rdc::Finalize();
    return 0;
}
