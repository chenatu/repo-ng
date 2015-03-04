/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014,  Regents of the University of California.
 *
 * This file is part of NDN repo-ng (Next generation of NDN repository).
 * See AUTHORS.md for complete list of repo-ng authors and contributors.
 *
 * repo-ng is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * repo-ng is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * repo-ng, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef REPO_HANDLES_REQ_HANDLE_HPP
#define REPO_HANDLES_REQ_HANDLE_HPP

#include <boost/lexical_cast.hpp>
#include <atomic>
#include "base-handle.hpp"


namespace repo {

static const uint64_t DEFAULT_CHECK_PERIOD = 100;

class ReqHandle : public BaseHandle
{

public:
  ReqHandle(Face& face, RepoStorage& storageHandle, KeyChain& keyChain,
             Scheduler& scheduler, bool hasOutput, std::ostream& os)
    : BaseHandle(face, storageHandle, keyChain, scheduler)
    , hasOutput(hasOutput)
    , os(os)
    , m_prevCount(0)
    , m_recvCount(0)
  {
    if (hasOutput) {
      m_scheduler.scheduleEvent(ndn::time::milliseconds(DEFAULT_CHECK_PERIOD),
                              bind(&ReqHandle::checkStatus, this));
    }
  }

  virtual void
  listen(const Name& prefix);

public:
  bool hasOutput;
  std::ostream& os;

private:
  /**
   * @brief Read data from backend storage
   */
  void
  onInterest(const Name& prefix, const Interest& interest);

  void
  onRegisterFailed(const Name& prefix, const std::string& reason);

  void
  checkStatus();

private:
  std::atomic_int m_recvCount;
  int m_prevCount;
  ndn::time::system_clock::time_point m_start;
};

} // namespace repo

#endif // REPO_HANDLES_REQ_HANDLE_HPP
