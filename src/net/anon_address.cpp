// Copyright (c) 2024, ANyONe Protocol fork contributors
// Based on tor_address.cpp — Copyright (c) 2018, The Monero Project
// All rights reserved. BSD 3-Clause License (same as Monero).

#include "anon_address.h"

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <system_error>
#include <boost/system/error_code.hpp>
#include "string_tools.h"
#include "string_tools_lexical.h"
#include "storages/portable_storage.h"

// ANyONe v3 hidden-service hostname: 56 base32 chars + ".anyone" = 63 chars
static constexpr std::size_t ANON_PUBKEY_CHARS = 56;
static constexpr std::size_t ANON_HOST_FULL    = 63;  // 56 + 7

namespace
{
  bool is_base32_char(char c) noexcept
  {
    return (c >= 'a' && c <= 'z') || (c >= '2' && c <= '7');
  }

  bool valid_anon_host(boost::string_ref h) noexcept
  {
    if (h.size() != ANON_HOST_FULL)           return false;
    if (h.substr(ANON_PUBKEY_CHARS) != ".anyone") return false;
    for (std::size_t i = 0; i < ANON_PUBKEY_CHARS; ++i)
      if (!is_base32_char(h[i]))              return false;
    return true;
  }
} // anonymous namespace

namespace net
{

// ─── constructors ────────────────────────────────────────────────────────────

anon_address::anon_address() noexcept : port_(0)
{
  static_assert(sizeof(unknown_host) < host_max, "unknown_host won't fit");
  std::strncpy(host_, unknown_host, host_max - 1);
  host_[host_max - 1] = '\0';
}

anon_address::anon_address(boost::string_ref host, uint16_t port) noexcept
  : port_(port)
{
  assert(host.size() < host_max);
  std::memcpy(host_, host.data(), host.size());
  host_[host.size()] = '\0';
}

// ─── static helpers ──────────────────────────────────────────────────────────

bool anon_address::is_anon(boost::string_ref host) noexcept
{
  return valid_anon_host(host);
}

boost::system::error_code anon_address::host_check(boost::string_ref host) noexcept
{
  if (!valid_anon_host(host))
    return boost::system::errc::make_error_code(
        boost::system::errc::invalid_argument);
  return {};
}

expect<anon_address> anon_address::make(boost::string_ref address,
                                        uint16_t default_port)
{
  boost::string_ref host = address;
  uint16_t          port = default_port;

  // Treat the last ':' as a port separator only when it comes after the
  // 63-char hostname (i.e., position >= 63).
  const auto colon = address.rfind(':');
  if (colon != boost::string_ref::npos && colon >= ANON_HOST_FULL)
  {
    host = address.substr(0, colon);
    boost::string_ref port_str = address.substr(colon + 1);
    uint32_t p = 0;
    if (!epee::string_tools::get_xtype_from_string(
            p, std::string{port_str.data(), port_str.size()})
        || p > 65535)
    {
      return {std::error_code{EINVAL, std::generic_category()}};
    }
    port = static_cast<uint16_t>(p);
  }

  // Lowercase normalisation (base32 is case-insensitive, but we store lower)
  std::string lower{host.data(), host.size()};
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
  host = boost::string_ref{lower};

  if (host_check(host))                    // non-zero == bad host
    return {std::error_code{EINVAL, std::generic_category()}};
  if (host.size() >= host_max)
    return {std::error_code{EINVAL, std::generic_category()}};

  return anon_address{host, port};
}

// ─── instance methods ────────────────────────────────────────────────────────

std::string anon_address::str() const
{
  return std::string{host_} + ":" + std::to_string(port_);
}

bool anon_address::is_same_host(const anon_address& rhs) const noexcept
{
  return std::strcmp(host_, rhs.host_) == 0;
}

bool anon_address::equal(const anon_address& rhs) const noexcept
{
  return port_ == rhs.port_ && is_same_host(rhs);
}

bool anon_address::less(const anon_address& rhs) const noexcept
{
  const int cmp = std::strcmp(host_, rhs.host_);
  return cmp != 0 ? cmp < 0 : port_ < rhs.port_;
}

// ─── epee serialization ──────────────────────────────────────────────────────

bool anon_address::_load(epee::serialization::portable_storage& src,
                         epee::serialization::section* hparent)
{
  std::string host{};
  uint16_t port = 0;
  if (!src.get_value("host", host, hparent) || !src.get_value("port", port, hparent))
    return false;
  if (host.size() >= host_max)
    return false;
  if (host != unknown_host && host_check(boost::string_ref{host}))
    return false;
  std::memcpy(host_, host.data(), host.size());
  host_[host.size()] = '\0';
  port_ = port;
  return true;
}

bool anon_address::store(epee::serialization::portable_storage& dest,
                         epee::serialization::section* hparent) const
{
 return dest.set_value("host", std::string{host_}, hparent)
     && dest.set_value("port", uint16_t{port_}, hparent);
}

} // namespace net
