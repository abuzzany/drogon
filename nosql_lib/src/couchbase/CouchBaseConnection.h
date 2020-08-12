/**
 *
 *  @file CouchBaseConnection.h
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */
#pragma once
#include <libcouchbase/couchbase.h>
#include <trantor/net/EventLoop.h>
#include <memory>
#include <unordered_map>
namespace trantor
{
class EventLoop;
class Channel;
}  // namespace trantor
namespace drogon
{
namespace nosql
{
class CouchBaseConnection
{
  public:
    CouchBaseConnection(const std::string &connStr, trantor::EventLoop *loop);

  private:
    trantor::EventLoop *loop_;
    std::unique_ptr<lcb_io_opt_st> ioop_;
    std::unique_ptr<trantor::Channel> channelPtr_;
    static void lcbDestroyIoOpts(struct lcb_io_opt_st *iops);
    static void procs2TrantorCallback(int version,
                                      lcb_loop_procs *loop_procs,
                                      lcb_timer_procs *timer_procs,
                                      lcb_bsd_procs *bsd_procs,
                                      lcb_ev_procs *ev_procs,
                                      lcb_completion_procs *completion_procs,
                                      lcb_iomodel_t *iomodel);
    static void lcbDeleteEvent(struct lcb_io_opt_st *iops,
                               lcb_socket_t sock,
                               void *event);
    static void *lcbCreateEvent(struct lcb_io_opt_st *iops);
    static void lcbDestroyEvent(struct lcb_io_opt_st *iops, void *event);
    static int lcbUpdateEvent(struct lcb_io_opt_st *iops,
                              lcb_socket_t sock,
                              void *event,
                              short flags,
                              void *cb_data,
                              void (*handler)(lcb_socket_t sock,
                                              short which,
                                              void *cb_data));

    static void *lcbCreateTimer(struct lcb_io_opt_st *iops);
    static void lcbDeleteTimer(struct lcb_io_opt_st *iops, void *event);
    static int lcbUpdateTimer(struct lcb_io_opt_st *iops,
                              void *timer,
                              lcb_uint32_t usec,
                              void *cb_data,
                              void (*handler)(lcb_socket_t sock,
                                              short which,
                                              void *cb_data));
    static void lcbDestroyTimer(struct lcb_io_opt_st *iops, void *event);
    using EventHandler = void (*)(lcb_socket_t sock,
                                  short which,
                                  void *cb_data);
    std::unordered_map<int, EventHandler> handlerMap_;
    std::unordered_map<trantor::TimerId, EventHandler> timerMap_;
};
using CouchBaseConnectionPtr = std::shared_ptr<CouchBaseConnection>;
}  // namespace nosql
}  // namespace drogon