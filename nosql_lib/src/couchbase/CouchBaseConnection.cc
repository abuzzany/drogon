/**
 *
 *  @file CouchBaseConnection.cc
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
#define LCB_IOPS_V12_NO_DEPRECATE
#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif
#include "CouchBaseConnection.h"
#include <trantor/net/EventLoop.h>
#include <trantor/net/Channel.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <libcouchbase/plugins/io/bsdio-inl.c>
using namespace drogon::nosql;

void CouchBaseConnection::lcbDestroyIoOpts(struct lcb_io_opt_st *iops)
{
    CouchBaseConnection *self =
        static_cast<CouchBaseConnection *>(iops->v.v2.cookie);
    self->ioop_.reset();
}
void CouchBaseConnection::lcbDeleteEvent(struct lcb_io_opt_st *iops,
                                         lcb_socket_t sock,
                                         void *event)
{
    CouchBaseConnection *self =
        static_cast<CouchBaseConnection *>(iops->v.v2.cookie);
    if (self->channelPtr_)
    {
        int ev = *static_cast<int *>(event);
        if (ev & trantor::Channel::kWriteEvent &&
            self->channelPtr_->isWriting())
        {
            self->channelPtr_->disableWriting();
        }
        if (ev & trantor::Channel::kReadEvent && self->channelPtr_->isReading())
        {
            self->channelPtr_->disableReading();
        }
    }
    (void)sock;
}

void CouchBaseConnection::lcbDeleteTimer(struct lcb_io_opt_st *iops,
                                         void *event)
{
    CouchBaseConnection *self =
        static_cast<CouchBaseConnection *>(iops->v.v2.cookie);
    auto timeId = static_cast<trantor::TimerId *>(event);
    self->loop_->invalidateTimer(*timeId);
}

int CouchBaseConnection::lcbUpdateEvent(struct lcb_io_opt_st *iops,
                                        lcb_socket_t sock,
                                        void *event,
                                        short flags,
                                        void *cb_data,
                                        void (*handler)(lcb_socket_t sock,
                                                        short which,
                                                        void *cb_data))
{
    CouchBaseConnection *self =
        static_cast<CouchBaseConnection *>(iops->v.v2.cookie);
    int evt = *static_cast<int *>(event);
    if (!self->channelPtr_)
    {
        self->channelPtr_ =
            std::make_unique<trantor::Channel>(self->loop_, sock);
    }
    int events = 0;

    if (flags & LCB_READ_EVENT)
    {
        events |= trantor::Channel::kReadEvent;
    }
    if (flags & LCB_WRITE_EVENT)
    {
        events |= trantor::Channel::kWriteEvent;
    }

    if (handler == self->handlerMap_[events])
    {
        /* no change! */
        return 0;
    }
    self->handlerMap_[events] = handler;
    if (flags & LCB_READ_EVENT)
    {
        self->channelPtr_->setReadCallback(

            [self, handler, cb_data, sock]() {
                handler(sock, LCB_READ_EVENT, cb_data);
            });
        if (!self->channelPtr_->isReading())
        {
            self->channelPtr_->enableReading();
        }
    }
    if (flags & LCB_WRITE_EVENT)
    {
        self->channelPtr_->setWriteCallback(

            [self, handler, cb_data, sock]() {
                handler(sock, LCB_WRITE_EVENT, cb_data);
            });
        if (!self->channelPtr_->isWriting())
        {
            self->channelPtr_->enableWriting();
        }
    }

    return 0;
}
int CouchBaseConnection::lcbUpdateTimer(struct lcb_io_opt_st *iops,
                                        void *timer,
                                        lcb_uint32_t usec,
                                        void *cb_data,
                                        void (*handler)(lcb_socket_t sock,
                                                        short which,
                                                        void *cb_data))
{
    CouchBaseConnection *self =
        static_cast<CouchBaseConnection *>(iops->v.v2.cookie);
    auto timerId = static_cast<trantor::TimerId *>(timer);
    auto iter = self->timerMap_.find(*timerId);
    if (iter != self->timerMap_.end() && iter->second == handler)
    {
        return 0;
    }
    if (iter != self->timerMap_.end())
    {
        self->timerMap_.erase(iter);
        self->loop_->invalidateTimer(*timerId);
    }
    *timerId = self->loop_->runAfter(std::chrono::microseconds(usec),
                                     [handler, cb_data]() {
                                         handler(-1, LCB_ERROR_EVENT, cb_data);
                                     });
    assert(*timerId);
    self->timerMap_[*timerId] = handler;
    return 0;
}
void *CouchBaseConnection::lcbCreateEvent(struct lcb_io_opt_st *iops)
{
    int *event = new int(0);
    (void)iops;
    return event;
}
void *CouchBaseConnection::lcbCreateTimer(struct lcb_io_opt_st *iops)
{
    trantor::TimerId *id = new trantor::TimerId(0);
    (void)iops;
    return id;
}
void CouchBaseConnection::lcbDestroyEvent(struct lcb_io_opt_st *iops,
                                          void *event)
{
    delete static_cast<int *>(event);
}
void CouchBaseConnection::lcbDestroyTimer(struct lcb_io_opt_st *iops,
                                          void *event)
{
    lcbDeleteTimer(iops, event);
    delete static_cast<trantor::TimerId *>(event);
}
void nullFunction(lcb_io_opt_t iops)
{
}
void CouchBaseConnection::procs2TrantorCallback(
    int version,
    lcb_loop_procs *loop_procs,
    lcb_timer_procs *timer_procs,
    lcb_bsd_procs *bsd_procs,
    lcb_ev_procs *ev_procs,
    lcb_completion_procs *completion_procs,
    lcb_iomodel_t *iomodel)
{
    ev_procs->cancel = CouchBaseConnection::lcbDeleteEvent;
    ev_procs->create = CouchBaseConnection::lcbCreateEvent;
    ev_procs->watch = CouchBaseConnection::lcbUpdateEvent;
    ev_procs->destroy = CouchBaseConnection::lcbDestroyEvent;

    timer_procs->create = CouchBaseConnection::lcbCreateTimer;
    timer_procs->cancel = CouchBaseConnection::lcbDeleteTimer;
    timer_procs->schedule = CouchBaseConnection::lcbUpdateTimer;
    timer_procs->destroy = CouchBaseConnection::lcbDestroyTimer;

    loop_procs->start = nullFunction;
    loop_procs->stop = nullFunction;
    loop_procs->tick = nullFunction;

    *iomodel = LCB_IOMODEL_EVENT;
    wire_lcb_bsd_impl2(bsd_procs, version);
}

CouchBaseConnection::CouchBaseConnection(const std::string &connStr,
                                         trantor::EventLoop *loop)
    : loop_(loop)
{
    ioop_ = std::make_unique<lcb_io_opt_st>();
    /* setup io iops! */
    ioop_->version = 3;
    ioop_->dlhandle = NULL;
    ioop_->destructor = CouchBaseConnection::lcbDestroyIoOpts;
    ioop_->v.v3.get_procs = procs2TrantorCallback;

    /* consider that struct isn't allocated by the library,
     * `need_cleanup' flag might be set in lcb_create() */
    ioop_->v.v3.need_cleanup = 0;

    assert(loop);
    ioop_->v.v3.cookie = this;

    wire_lcb_bsd_impl(ioop_.get());
}