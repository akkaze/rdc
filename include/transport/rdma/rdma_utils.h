#pragma once
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "core/env.h"
#include "core/logging.h"

namespace rdc {
inline void GetAvaliableDeviceAndPort(ibv_device*& dev, uint8_t& ib_port) {
    ibv_context* ctx;
    ibv_device** dev_list;
    ibv_device_attr device_attr;
    ibv_port_attr port_attr;
    CHECK_NOTNULL(dev_list = ibv_get_device_list(NULL));
    while (dev_list) {
        CHECK_NOTNULL(ctx = ibv_open_device(*dev_list));
        CHECK_EQ(ibv_query_device(ctx, &device_attr), 0)
            << "Failed to query device props";
        for (int port = 1; port <= device_attr.phys_port_cnt; port++) {
            CHECK_EQ(ibv_query_port(ctx, port, &port_attr), 0)
                << "Failed to query port props";
            if (port_attr.state == IBV_PORT_ACTIVE) {
                dev = *dev_list;
                ib_port = port;
                ibv_close_device(ctx);
                return;
            }
        }
        ibv_close_device(ctx);
        dev_list++;
    }
    CHECK_NOTNULL(dev);
    return;
}

inline ibv_mtu set_mtu(ibv_context* context, uint8_t ib_port) {
    ibv_port_attr port_attr;
    enum ibv_mtu mtu = IBV_MTU_512;
    std::string mtu_s;
    int rc, mtu_i;

    rc = ibv_query_port(context, ib_port, &port_attr);
    CHECK(!rc) << "Failed to query the port" << ib_port;

    auto mtu_cstr = Env::Get()->Find("RDC_RDMA_MTU");
    if (mtu_cstr != nullptr) mtu_s = mtu_cstr;
    if (!mtu_s.empty()) {
        mtu_i = std::stoi(mtu_s);
        switch (mtu_i) {
            case 256:
                mtu = IBV_MTU_256;
                break;
            case 512:
                mtu = IBV_MTU_512;
                break;
            case 1024:
                mtu = IBV_MTU_1024;
                break;
            case 2048:
                mtu = IBV_MTU_2048;
                break;
            case 4096:
                mtu = IBV_MTU_4096;
                break;
            default:
                CHECK(0) << "Error: MTU input value must be one of the "
                            "following: 256, "
                            "512, 1024, 2048, 4096. MTU "
                         << mtu << " is invalid\n";
                break;
        }
        CHECK(mtu < port_attr.active_mtu)
            << "MTU configuration for the QPs is larger than active MTU";
    } else {
        mtu = port_attr.active_mtu;
    }
    return mtu;
}
namespace roce {
#define RoCE_V2 "RoCE v2"
inline int ReadSysfsFile(const char* dir, const char* file, char* buf,
                         size_t size) {
    char* path;
    int fd;
    int len;

    if (asprintf(&path, "%s/%s", dir, file) < 0) {
        return -1;
    }

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        free(path);
        return -1;
    }

    len = read(fd, buf, size);

    close(fd);
    free(path);

    if (len > 0 && buf[len - 1] == '\n') {
        buf[--len] = '\0';
    }

    return len;
}

inline bool Is_Gid_RoCE_V2(ibv_context* context, int ib_port, int index) {
    char name[32];
    char buff[41];

    snprintf(name, sizeof(name), "ports/%d/gid_attrs/types/%d", ib_port, index);
    if (ReadSysfsFile(context->device->ibdev_path, name, buff, sizeof(buff)) <=
        0) {
        return false;
    }
    return !strcmp(buff, RoCE_V2);
}
inline int GetGid(const int& ib_port, ibv_context* context) {
    ibv_port_attr port_attr;
    std::string gid_str;
    union ibv_gid gid;
    int gid_index = 0;
    int gids_num = 0;
    int v2_ip_num = 0;
    int rc = ibv_query_port(context, ib_port, &port_attr);
    CHECK(!rc) << "Failed to query port" << ib_port;
    for (int i = 0; i < port_attr.gid_tbl_len; i++) {
        rc = ibv_query_gid(context, ib_port, i, &gid);
        CHECK(!rc) << "Failed to query gid to port" << ib_port;
        if (gid.global.interface_id) {
            gids_num++;
            if (gid.global.subnet_prefix == 0 &&
                Is_Gid_RoCE_V2(context, ib_port, i)) {
                if (v2_ip_num == 0) {
                    gid_index = i;
                }
                v2_ip_num++;
            }
        }
    }
    if (!Is_Gid_RoCE_V2(context, ib_port, gid_index)) {
        LOG(INFO) << "RoCE v2 is not configured for GID_INDEX "
                  << (int)gid_index;
    }
    return gid_index;
}
}  // namespace roce
}  // namespace rdc
