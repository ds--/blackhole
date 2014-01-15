#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

#include <boost/mpl/for_each.hpp>
#include <boost/mpl/is_sequence.hpp>

#include "blackhole/repository/config/formatter.hpp"
#include "blackhole/repository/config/sink.hpp"
#include "blackhole/repository/factory/factory.hpp"
#include "blackhole/repository/factory/frontend.hpp"
#include "blackhole/repository/registrator.hpp"

namespace blackhole {

template<typename Sink, typename Formatter, class = void>
struct frontend_repository {
    template<typename Level>
    static void push(frontend_factory_t<Level>& factory) {
        factory.template add<Sink, Formatter>();
    }
};

template<typename Sink, typename Formatters>
struct frontend_repository<Sink, Formatters, typename std::enable_if<boost::mpl::is_sequence<Formatters>::type::value>::type> {
    template<typename Level>
    static void push(frontend_factory_t<Level>& factory) {
        aux::registrator::frontend<Level> action { factory };
        boost::mpl::for_each<
            Formatters,
            aux::mpl::id<Sink, boost::mpl::_>
        >(action);
    }
};

template<typename Level>
class group_factory_t {
    typedef std::unique_ptr<base_frontend_t> return_type;
    typedef return_type(*factory_type)(const frontend_factory_t<Level>&, const formatter_config_t&, const sink_config_t&);

    mutable std::mutex mutex;
    std::unordered_map<std::string, factory_type> sinks;

    frontend_factory_t<Level> factory;
public:
    template<typename Sink, typename Formatter>
    void add() {
        std::lock_guard<std::mutex> lock(mutex);
        sinks[Sink::name()] = static_cast<factory_type>(&factory_t<Level>::template create<Sink>);
        frontend_repository<Sink, Formatter>::push(factory);
    }

    template<typename Sink, typename Formatter>
    bool has() const {
        std::lock_guard<std::mutex> lock(mutex);
        return sinks.find(Sink::name()) != sinks.end() && factory.template has<Sink, Formatter>();
    }

    return_type create(const formatter_config_t& formatter_config, const sink_config_t& sink_config) const {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = sinks.find(sink_config.type);
        if (it == sinks.end()) {
            throw error_t("sink '%s' is not registered", sink_config.type);
        }

        return it->second(factory, formatter_config, sink_config);
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex);
        sinks.clear();
        factory.clear();
    }
};

} // namespace blackhole