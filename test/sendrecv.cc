#include "rdc.h"
#include "utils/string_utils.h"
int main(int argc, char** argv) {
    rdc::Init(argc, argv);
    rdc::NewCommunicator(rdc::kMainCommName);
    for (int i = 0; i < 100; i++) {
        if (rdc::GetRank() == 0) {
            std::string str = rdc::str_utils::SPrintf("hello world %u ", i);
            rdc::Send(const_cast<char*>(str.c_str()), str.size(), 1);
        } else if (rdc::GetRank() == 1) {
            char* s = new char[16];
            std::memset(s, 0, 16);
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::string str = rdc::str_utils::SPrintf("hello world %u ", i);
            // std::to_string(i);
            rdc::Recv(s, str.size(), 0);
            rdc::TrackerPrint(s);
            LOG(INFO) << s;
            DCHECK_EQ(strncmp(str.c_str(), s, str.size()), 0);
            delete[] s;
        }
    }
    return 0;
}
