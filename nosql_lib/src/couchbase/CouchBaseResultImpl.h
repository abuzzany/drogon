/**
 *
 *  @file CouchBaseResultImpl.h
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
#include <libcouchbase/pktfwd.h>
#include <trantor/utils/NonCopyable.h>
namespace drogon
{
namespace nosql
{
class CouchBaseResultImpl : public trantor::NonCopyable
{
  public:
    virtual ~CouchBaseResultImpl()
    {
    }

  private:
    virtual void init() = 0;
};
namespace operation
{
struct Base
{
    using CType = lcb_CMDBASE;
    using RType = lcb_RESPBASE;
};
struct Get
{
    using CType = lcb_CMDGET;
    using RType = lcb_RESPGET;
};
struct Store
{
    using CType = lcb_CMDSTORE;
    using RType = lcb_RESPSTORE;
};
struct Touch
{
    using CType = lcb_CMDTOUCH;
    using RType = lcb_RESPTOUCH;
};
struct Remove
{
    using CType = lcb_CMDREMOVE;
    using RType = lcb_RESPREMOVE;
};
struct Unlock
{
    using CType = lcb_CMDUNLOCK;
    using RType = lcb_RESPUNLOCK;
};
struct Counter
{
    using CType = lcb_CMDCOUNTER;
    using RType = lcb_RESPCOUNTER;
};
//struct Stats
//{
//    using CType = lcb_CMDSTATS;
//    using RType = lcb_RESPSTATS;
//};
//struct Observe
//{
//    using CType = lcb_CMDOBSERVE;
//    using RType = lcb_RESPOBSERVE;
//};
//struct Endure
//{
//    using CType = lcb_CMDENDURE;
//    using RType = lcb_RESPENDURE;
//};
}  // namespace operation
template <typename T = operation::Base>
class LcbResult : CouchBaseResultImpl
{
  public:
    using RType = typename T::RType;
    LcbResult(RType *resp)
    {
        u_.resp_ = *resp;
    }
    virtual ~LcbResult()
    {
    }

  protected:
    union {
        lcb_RESPBASE base_;
        RType resp_;
    } u_;
};
class GetLcbResult : public LcbResult<operation::Get>
{
  public:
    GetLcbResult(const RType *resp) : LcbResult<operation::Get>(resp)
    {
        assert(u_.base_.rc == LCB_SUCCESS);
        if (u_.resp_.bufh)
        {
            lcb_backbuf_ref((lcb_BACKBUF)u_.resp_.bufh);
        }
        else if (u_.resp_.nvalue)
        {
            char *tmp = new char[u_.resp_.nvalue];
            u_.resp_.value = tmp;
        }
    }
    virtual ~GetLcbResult()
    {
        if (u_.resp_.bufh)
        {
            lcb_backbuf_unref((lcb_BACKBUF)u_.resp_.bufh);
        }
        else if (u_.resp_.value)
        {
            delete[] u_.resp_.value;
        }
    }
};
}  // namespace nosql
}  // namespace drogon