/**
 *
 *  @file NosqlClient.h
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
#include <drogon/nosql/Result.h>
#include <functional>
namespace drogon
{
namespace nosql
{
using NosqlCallback = std::function<void(const Result &)>;
class CouchBaseClient
{
  public:
    void execute(const std::string &command, const NosqlCallback &callback);
    
};
}  // namespace nosql
}  // namespace drogon