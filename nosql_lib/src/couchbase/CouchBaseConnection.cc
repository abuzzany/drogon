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

CouchBaseConnection::CouchBaseConnection(const drogon::string_view &connStr,
                                         const drogon::string_view &username,
                                         const drogon::string_view &password,
                                         trantor::EventLoop *loop)
    : loop_(loop),
      connString_(connStr),
      userName_(username),
      password_(password)
{
    loop->queueInLoop([this]() { connect(); });
}
void CouchBaseConnection::connect()
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

    assert(loop_);
    ioop_->v.v3.cookie = this;

    wire_lcb_bsd_impl(ioop_.get());

    lcb_STATUS error;
    lcb_CREATEOPTS *options = NULL;

    lcb_createopts_create(&options, LCB_TYPE_BUCKET);
    if (!connString_.empty())
    {
        lcb_createopts_connstr(options, connString_.data(), connString_.size());
    }
    if (!userName_.empty())
    {
        lcb_createopts_credentials(options,
                                   userName_.data(),
                                   userName_.size(),
                                   password_.data(),
                                   password_.size());
    }
    lcb_createopts_io(options, ioop_.get());
    error = lcb_create(&instance_, options);
    lcb_createopts_destroy(options);
    if (error != LCB_SUCCESS)
    {
        LOG_ERROR << "Failed to create a libcouchbase instance: "
                  << lcb_strerror_short(error);
        closeCallback_(shared_from_this());
        return;
    }

    /* Set up the callbacks */
    lcb_set_bootstrap_callback(instance_,
                               CouchBaseConnection::bootstrapCallback);
    lcb_install_callback(instance_,
                         LCB_CALLBACK_GET,
                         (lcb_RESPCALLBACK)get_callback);
    lcb_install_callback(instance_,
                         LCB_CALLBACK_STORE,
                         (lcb_RESPCALLBACK)store_callback);

    if ((error = lcb_connect(instance_)) != LCB_SUCCESS)
    {
        LOG_ERROR << "Failed to connect libcouchbase instance: "
                  << lcb_strerror_short(error);
        lcb_destroy(instance_);
        closeCallback_(shared_from_this());
    }
}
CouchBaseConnection::~CouchBaseConnection()
{
    lcb_destroy(instance_);
}

void CouchBaseConnection::bootstrapCallback(lcb_INSTANCE *instance,
                                            lcb_STATUS err)
{
    CouchBaseConnection *self =
        (CouchBaseConnection *)(lcb_get_cookie(instance));
    if (err != LCB_SUCCESS)
    {
        LOG_ERROR << "bootstrap error: " << lcb_strerror_short(err);
        lcb_destroy_async(instance, NULL);
        self->closeCallback_(self->shared_from_this());
        return;
    }
    LOG_TRACE << "bootstrap successfully";
    self->okCallback_(self->shared_from_this());
}

void CouchBaseConnection::getCallback(lcb_INSTANCE *instance,
                                      int cbtype,
                                      const lcb_RESPGET *rg)
{
    const char *value;
    size_t nvalue;
    lcb_STATUS rc = lcb_respget_status(rg);

    if (rc != LCB_SUCCESS)
    {
        LOG_ERROR << "Failed to get key: " << lcb_strerror_short(rc);
        //TODO: exception callback here;
    }

}