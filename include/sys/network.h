/**
 *  Copyright (c) 2018 by Contributors
 * @file   network_utils.h
 * @brief  network utilities
 * @author Ankun Zheng
 */
#pragma once
#include <string>
namespace rdc {
namespace network {
/**
 * @brief return the IP address for given interface eth0, eth1, ...
 */
void GetIP(const std::string& interface, std::string* ip);

/**
 * @brief return the IP address and Interface the first interface which is not
 * loopback
 *
 * only support IPv4
 */
void GetAvailableInterfaceAndIP(std::string* interface, std::string* ip);

/**
 * @brief return an available port on local machine
 *
 * only support IPv4
 * @return 0 on failure
 */
int GetAvailablePort();
}  // namespace network
}  // namespace rdc
