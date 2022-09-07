#pragma once

#include <vector>
#include <mutex>
#include <functional>
#include <set>
#include <map>
#include <memory>
#include <utility>
#include <iostream>

#include <base/types.h>
#include <boost/container_hash/hash_fwd.hpp>
#include <boost/functional/hash.hpp>
#include <Common/HashTable/Hash.h>

namespace DB
{

class EventNotifier
{
public:
    struct Handler
    {
      Handler(
        EventNotifier & parent_,
        UInt64 event_id_,
        UInt64 callback_id_)
        : parent(parent_)
        , event_id(event_id_)
        , callback_id(callback_id_)
      {}

      ~Handler()
      {
        std::lock_guard lock(parent.mutex);

        parent.callback_table[event_id].erase(callback_id);
        parent.storage.erase(callback_id);
      }

      private:

      EventNotifier & parent;
      UInt64 event_id;
      UInt64 callback_id;
    };

    using HandlerPtr = std::shared_ptr<Handler>;

    static EventNotifier & init();
    static EventNotifier & instance();
    static void shutdown();

    template <typename EventType, typename Callback>
    [[ nodiscard ]] HandlerPtr subscribe(EventType event, Callback && callback)
    {
      std::lock_guard lock(mutex);

      auto event_id = DefaultHash64(event);
      UInt64 callback_id = 0;
      boost::hash_combine(callback_id, event_id);
      boost::hash_combine(callback_id, ++counter);

      callback_table[event_id].insert(callback_id);
      storage[callback_id] = std::forward<Callback>(callback);

      return std::make_shared<Handler>(*this, event_id, callback_id);
    }

    template <typename EventType>
    void notify(EventType event)
    {
      std::lock_guard lock(mutex);

      for (const auto & identifier : callback_table[DefaultHash64(event)])
        storage[identifier]();
    }

private:

    using CallbackType = std::function<void()>;
    using CallbackStorage = std::map<UInt64, CallbackType>;
    using EventToCallbacks = std::map<UInt64, std::set<UInt64>>;

    /// Pairing function f: N x N -> N (bijection)
    /// Will return unique numbers given a pair of integers
    static UInt64 calculateIdentifier(UInt64 a, UInt64 b)
    {
      return 0.5 * (a + b) * (a + b + 1) + b;
    }

    std::mutex mutex;

    EventToCallbacks callback_table;
    CallbackStorage storage;
    UInt64 counter{0};

    static std::unique_ptr<EventNotifier> event_notifier;
};

}
