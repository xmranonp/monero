// Copyright (c) 2024, ANyONe Protocol fork contributors
// Based on tor_address.h — Copyright (c) 2018, The Monero Project
// All rights reserved. BSD 3-Clause License (same as Monero).
#pragma once

#include <cstdint>
#include <string>
#include <boost/utility/string_ref.hpp>
#include <boost/system/error_code.hpp>
#include "common/expect.h"
#include "net/enums.h"
#include "net/net_utils_base.h"
#include "serialization/keyvalue_serialization.h"
#include "span.h"

namespace epee { namespace serialization { class portable_storage; struct section; } }

namespace net
{
  //! ANyONe Protocol (.anyone) hidden-service address.
  //! Wire format: <56 base32 chars>.anyone:<port>
  //! Uses the same ED25519 key material as Tor v3 .onion with a .anyone TLD.
  class anon_address
  {
    static constexpr const char* unknown_host = "<unknown anon>";
    // 56-char base32 pubkey + ".anyone" + NUL = 62 bytes
    static constexpr std::size_t host_max = 64;

    char     host_[host_max];
    uint16_t port_;

    // Only make() should call this
    anon_address(boost::string_ref host, uint16_t port) noexcept;

  public:
    static constexpr const char* tld = ".anyone";

    //! Returns address_type::anon — used by net_utils_base.h serialization dispatch
    static constexpr epee::net_utils::address_type get_type_id() noexcept
    { return epee::net_utils::address_type::anon; }

    //! Returns zone::anon — used by zone-keyed routing in node_server
    static constexpr epee::net_utils::zone get_zone() noexcept
    { return epee::net_utils::zone::anon; }

    //! Returns a default-constructed (unknown) address — used as default_remote
    //! in anonymous_inbound so monerod accepts inbound .anyone connections
    static anon_address unknown() noexcept { return {}; }

    static constexpr bool is_loopback() noexcept { return false; }
    static constexpr bool is_local()    noexcept { return false; }

    //! .anyone addresses are not blockable by IP (they have no real IP)
    bool is_blockable() const noexcept { return false; }

    //! Quick check: is this string a valid .anyone hostname?
    static bool is_anon(boost::string_ref host) noexcept;

    //! Returns a non-zero error_code if host is malformed, {} on success.
    static boost::system::error_code host_check(boost::string_ref host) noexcept;

    //! Parse "host.anyone[:port]". Returns anon_address or std::error_code.
    static expect<anon_address> make(boost::string_ref address,
                                     uint16_t default_port);

    anon_address() noexcept;
    anon_address(const anon_address&) = default;
    anon_address& operator=(const anon_address&) = default;

    std::string str()       const;            // "host.anon:port"
    const char* host_str()  const noexcept { return host_; }
    uint16_t    port()      const noexcept { return port_; }

    bool is_same_host(const anon_address& rhs) const noexcept;
    bool equal(const anon_address& rhs)        const noexcept;
    bool less(const anon_address& rhs)         const noexcept;

    bool _load(epee::serialization::portable_storage& src,
               epee::serialization::section* hparent);
    bool store(epee::serialization::portable_storage& dest,
               epee::serialization::section* hparent) const;

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(host_)
      KV_SERIALIZE(port_)
    END_KV_SERIALIZE_MAP()
  };

  inline bool operator==(const anon_address& l, const anon_address& r) noexcept
  { return l.equal(r); }
  inline bool operator<(const anon_address& l, const anon_address& r) noexcept
  { return l.less(r); }

} // namespace net
