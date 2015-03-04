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

#include "req-handle.hpp"

namespace repo {

void
ReqHandle::onInterest(const Name& prefix, const Interest& interest)
{

  shared_ptr<ndn::Data> data = make_shared<ndn::Data>(interest.getName());
  getKeyChain().sign(*data);
  getFace().put(*data);
  m_recvCount++;
}

void
ReqHandle::onRegisterFailed(const Name& prefix, const std::string& reason)
{
  std::cerr << "ERROR: Failed to register prefix in local hub's daemon" << std::endl;
  getFace().shutdown();
}

void
ReqHandle::listen(const Name& prefix)
{
  ndn::InterestFilter filter(Name(prefix).append("req"));
  getFace().setInterestFilter(filter,
                              bind(&ReqHandle::onInterest, this, _1, _2),
                              bind(&ReqHandle::onRegisterFailed, this, _1, _2));
  m_start = ndn::time::system_clock::now();
}

void
ReqHandle::checkStatus()
{
  auto duration = ndn::time::duration_cast<ndn::time::milliseconds>(ndn::time::system_clock::now() - m_start);
  os << duration.count() << " " << m_recvCount.load() << " " << m_recvCount.load() - m_prevCount << std::endl;
  m_prevCount = m_recvCount.load();
  m_scheduler.scheduleEvent(ndn::time::milliseconds(DEFAULT_CHECK_PERIOD),
                          bind(&ReqHandle::checkStatus, this));
}

} //namespace repo
