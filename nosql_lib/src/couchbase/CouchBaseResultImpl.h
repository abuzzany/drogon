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
#include <couchbase/couchbase.h>
namespace drogon
{
namespace nosql
{
class CouchBaseResultImpl
{
  public:
    ~CouchBaseResultImpl()
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
struct Stats
{
    using CType = lcb_CMDSTATS;
    using RType = lcb_RESPSTATS;
};
struct Observe
{
    using CType = lcb_CMDOBSERVE;
    using RType = lcb_RESPOBSERVE;
};
struct Endure
{
    using CType = lcb_CMDENDURE;
    using RType = lcb_RESPENDURE;
};
}  // namespace operation
template <typename T = operation::Base>
class LcbResult : CouchBaseResultImpl
{
  public:
    using RType = T::RType;
    LcbResult(RType *resp)
    {
        u_.resp_ = *resp;
        init();
    }
    virtual void init() override
    {
    }

  private:
    union {
        lcb_RESTBASE base_, RType resp_
    } u_;
};
class GetLcbResult : public LcbResult<operation::Get>
{
  public:
    GetLcbResult(const RType *resp) : LcbResult<operation::Get>(resp)
    {
    }
    virtual void init() override
    {
        assert(u_.base_.rc == LCB_SUCCESS);
        if (u.resp.bufh)
        {
            lcb_backbuf_ref((lcb_BACKBUF)u.resp.bufh);
        }
        else if (u.resp.nvalue)
        {
            char *tmp = new char[u.resp.nvalue + sizeof(size_t)];
            u.resp.value = tmp;
            size_t rc = 1;
            memcpy(vbuf_refcnt(), &rc, sizeof rc);
        }
    }
};
}  // namespace nosql
}  // namespace drogon