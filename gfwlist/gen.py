#!/usr/bin/python
# -*- encoding: utf8 -*-

import itertools
import math
import sys

import IPy


def main():
    china_list_set = IPy.IPSet()
    # for line in sys.stdin:
    #     line_params = line.split("|")
    #     if len(line_params) < 5 or line_params[2] != "ipv4" or line_params[1] != "CN":
    #         continue
    #     ip_addr = line_params[3]
    #     ip_length = float(line_params[4])
    #     ip_mask = 32 - int(math.ceil(math.log(ip_length, 2)))
    #     china_list_set.add(IPy.IP("%s/%d" % (ip_addr, ip_mask)))

    # 添加内网地址
    internal_list = IPy.IPSet(map(IPy.IP, [
        "0.0.0.0/8",
        "10.0.0.0/8",
        "100.64.0.0/10",
        "112.124.47.0/24",
        "114.114.114.0/24",
        "127.0.0.0/8",
        "169.254.0.0/16",
        "172.16.0.0/12",
        "192.0.0.0/29",
        "192.0.2.0/24",
        "192.88.99.0/24",
        "192.168.0.0/16",
        "198.18.0.0/15",
        "198.51.100.0/24",
        "203.0.113.0/24",
        "224.0.0.0/4",
        "240.0.0.0/4",
    ]))
    china_list_set += internal_list

    # 取反
    all = IPy.IPSet([IPy.IP("0.0.0.0/0")])
    # 剔除所有孤立的C段
    for ip in china_list_set:
        all.discard(ip)

    # filter = itertools.ifilter(lambda x: len(x) <= 65536, all)
    # for ip in filter:
    #     all.discard(ip)
    #     all.add(IPy.IP(ip.strNormal(0)).make_net('255.255.0.0'))

    # 输出结果
    for ip in all:
        print '<item>' + str(ip) + '</item>'


if __name__ == "__main__":
    main()
