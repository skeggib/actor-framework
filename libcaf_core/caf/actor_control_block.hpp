// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <atomic>

#include "caf/config.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/error.hpp"
#include "caf/fwd.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/node_id.hpp"
#include "caf/weak_intrusive_ptr.hpp"

namespace caf {

/// Actors are always allocated with a control block that stores its identity
/// as well as strong and weak reference counts to it. Unlike
/// "common" weak pointer designs, the goal is not to allocate the data
/// separately. Instead, the only goal is to break cycles. For
/// example, linking two actors automatically creates a cycle when using
/// strong reference counts only.
///
/// When allocating a new actor, CAF will always embed the user-defined
/// actor in an `actor_storage` with the control block prefixing the
/// actual actor type, as shown below.
///
///     +----------------------------------------+
///     |            actor_storage<T>            |
///     +----------------------------------------+
///     | +-----------------+------------------+ |
///     | |  control block  |  actor data (T)  | |
///     | +-----------------+------------------+ |
///     | | ref count       | mailbox          | |
///     | | weak ref count  | .                | |
///     | | actor ID        | .                | |
///     | | node ID         | .                | |
///     | +-----------------+------------------+ |
///     +----------------------------------------+
///
/// Actors start with a strong reference count of 1. This count is transferred
/// to the first `actor` or `typed_actor` handle used to store the actor.
/// Actors will also start with a weak reference count of 1. This count
/// is decremenated once the strong reference count drops to 0.
///
/// The data block is destructed by calling the destructor of `T` when the
/// last strong reference expires. The storage itself is destroyed when
/// the last weak reference expires.
class CAF_CORE_EXPORT actor_control_block {
public:
  using data_destructor = void (*)(abstract_actor*);
  using block_destructor = void (*)(actor_control_block*);

  actor_control_block(actor_id x, node_id& y, actor_system* sys,
                      data_destructor ddtor, block_destructor bdtor)
    : strong_refs(1),
      weak_refs(1),
      aid(x),
      nid(std::move(y)),
      home_system(sys),
      data_dtor(ddtor),
      block_dtor(bdtor) {
    // nop
  }

  actor_control_block(const actor_control_block&) = delete;
  actor_control_block& operator=(const actor_control_block&) = delete;

  std::atomic<size_t> strong_refs;
  std::atomic<size_t> weak_refs;
  const actor_id aid;
  const node_id nid;
  actor_system* const home_system;
  const data_destructor data_dtor;
  const block_destructor block_dtor;

  static_assert(sizeof(std::atomic<size_t>) == sizeof(void*),
                "std::atomic not lockfree on this platform");

  static_assert(sizeof(intrusive_ptr<int>) == sizeof(int*),
                "intrusive_ptr<T> and T* have different size");

  static_assert(sizeof(node_id) == sizeof(void*),
                "sizeof(node_id) != sizeof(size_t)");

  static_assert(sizeof(data_destructor) == sizeof(void*),
                "functiion pointer and regular pointers have different size");

  /// Returns a pointer to the actual actor instance.
  abstract_actor* get() {
    // this pointer arithmetic is compile-time checked in actor_storage's ctor
    return reinterpret_cast<abstract_actor*>(reinterpret_cast<intptr_t>(this)
                                             + CAF_CACHE_LINE_SIZE);
  }

  /// Returns a pointer to the control block that stores
  /// identity and reference counts for this actor.
  static actor_control_block* from(const abstract_actor* ptr) {
    // this pointer arithmetic is compile-time checked in actor_storage's ctor
    return reinterpret_cast<actor_control_block*>(
      reinterpret_cast<intptr_t>(ptr) - CAF_CACHE_LINE_SIZE);
  }

  /// @cond PRIVATE

  actor_addr address();

  actor_id id() const noexcept {
    return aid;
  }

  const node_id& node() const noexcept {
    return nid;
  }

  bool enqueue(strong_actor_ptr sender, message_id mid, message content,
               execution_unit* host);

  bool enqueue(mailbox_element_ptr what, execution_unit* host);

  /// @endcond
};

/// @relates actor_control_block
CAF_CORE_EXPORT bool intrusive_ptr_upgrade_weak(actor_control_block* x);

/// @relates actor_control_block
inline void intrusive_ptr_add_weak_ref(actor_control_block* x) {
  x->weak_refs.fetch_add(1, std::memory_order_relaxed);
}

/// @relates actor_control_block
CAF_CORE_EXPORT void intrusive_ptr_release_weak(actor_control_block* x);

/// @relates actor_control_block
inline void intrusive_ptr_add_ref(actor_control_block* x) {
  x->strong_refs.fetch_add(1, std::memory_order_relaxed);
}

/// @relates actor_control_block
CAF_CORE_EXPORT void intrusive_ptr_release(actor_control_block* x);

/// @relates actor_control_block
using strong_actor_ptr = intrusive_ptr<actor_control_block>;

CAF_CORE_EXPORT bool operator==(const strong_actor_ptr&, const abstract_actor*);

CAF_CORE_EXPORT bool operator==(const abstract_actor*, const strong_actor_ptr&);

inline bool operator!=(const strong_actor_ptr& x, const abstract_actor* y) {
  return !(x == y);
}

inline bool operator!=(const abstract_actor* x, const strong_actor_ptr& y) {
  return !(x == y);
}

/// @relates actor_control_block
using weak_actor_ptr = weak_intrusive_ptr<actor_control_block>;

CAF_CORE_EXPORT error_code<sec>
load_actor(strong_actor_ptr& storage, execution_unit*, actor_id aid,
           const node_id& nid);

CAF_CORE_EXPORT error_code<sec>
save_actor(strong_actor_ptr& storage, execution_unit*, actor_id aid,
           const node_id& nid);

template <class Inspector>
auto context_of(Inspector* f) -> decltype(f->context()) {
  return f->context();
}

inline execution_unit* context_of(void*) {
  return nullptr;
}

CAF_CORE_EXPORT std::string to_string(const strong_actor_ptr& x);

CAF_CORE_EXPORT void
append_to_string(std::string& x, const strong_actor_ptr& y);

CAF_CORE_EXPORT std::string to_string(const weak_actor_ptr& x);

CAF_CORE_EXPORT void append_to_string(std::string& x, const weak_actor_ptr& y);

template <class Inspector>
bool inspect(Inspector& f, strong_actor_ptr& x) {
  actor_id aid = 0;
  node_id nid;
  if constexpr (!Inspector::is_loading) {
    if (x) {
      aid = x->aid;
      nid = x->nid;
    }
  }
  auto load_cb = [&] { return load_actor(x, context_of(&f), aid, nid); };
  auto save_cb = [&] { return save_actor(x, context_of(&f), aid, nid); };
  return f.object(x)
    .pretty_name("actor")
    .on_load(load_cb)
    .on_save(save_cb)
    .fields(f.field("id", aid), f.field("node", nid));
}

template <class Inspector>
bool inspect(Inspector& f, weak_actor_ptr& x) {
  // Inspect as strong pointer, then write back to weak pointer on save.
  if constexpr (Inspector::is_loading) {
    strong_actor_ptr tmp;
    if (inspect(f, tmp)) {
      x.reset(tmp.get());
      return true;
    }
    return false;
  } else {
    auto tmp = x.lock();
    return inspect(f, tmp);
  }
}

} // namespace caf

// allow actor pointers to be used in hash maps
namespace std {

template <>
struct hash<caf::strong_actor_ptr> {
  size_t operator()(const caf::strong_actor_ptr& ptr) const {
    return ptr ? static_cast<size_t>(ptr->id()) : 0;
  }
};

template <>
struct hash<caf::weak_actor_ptr> {
  size_t operator()(const caf::weak_actor_ptr& ptr) const {
    return ptr ? static_cast<size_t>(ptr->id()) : 0;
  }
};

} // namespace std
