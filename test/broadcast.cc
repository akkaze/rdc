#include <rdc.h>
using namespace rdc;
int main(int argc, char *argv[]) {
    rdc::Init(argc, argv);
    std::string s;
    if (rdc::GetRank() == 0) s = "hello world";
    LOG_F(INFO, "@node[%d] before-broadcast: s=\"%s\"\n",
           rdc::GetRank(), s.c_str());
    // broadcast s from node 0 to all other nodes
    rdc::Broadcast(s, 0);
    LOG_F(INFO, "@node[%d] after-broadcast: s=\"%s\"\n",
           rdc::GetRank(), s.c_str());
    CHECK_F(s == "hello world");
    rdc::Finalize();
    return 0;
}
