#ifndef RDB_PROTOCOL_CHANGEFEED_HPP_
#define RDB_PROTOCOL_CHANGEFEED_HPP_

#include <deque>
#include <map>

#include "containers/counted.hpp"
#include "containers/scoped.hpp"
#include "protocol_api.hpp"
#include "rpc/serialize_macros.hpp"

class auto_drainer_t;
template<class T> class base_namespace_repo_t;
class mailbox_manager_t;
struct rdb_modification_report_t;
struct rdb_protocol_t;

namespace ql {

class base_exc_t;
class batcher_t;
class changefeed_t;
class datum_stream_t;
class datum_t;
class env_t;
class table_t;

struct changefeed_msg_t {
    enum type_t { UNINITIALIZED, CHANGE, TABLE_DROP };

    changefeed_msg_t();
    ~changefeed_msg_t();
    static changefeed_msg_t change(const rdb_modification_report_t *report);
    static changefeed_msg_t table_drop();

    type_t type;
    counted_t<const datum_t> old_val;
    counted_t<const datum_t> new_val;
    RDB_DECLARE_ME_SERIALIZABLE;
};

class changefeed_manager_t : public home_thread_mixin_t {
public:
    changefeed_manager_t(mailbox_manager_t *_manager);
    ~changefeed_manager_t();

    // Uses the home thread of the subscribing stream, NOT the home thread of
    // the changefeed manager.
    class subscription_t : public home_thread_mixin_t {
    public:
        subscription_t(uuid_u uuid,
                       base_namespace_repo_t<rdb_protocol_t> *ns_repo,
                       changefeed_manager_t *manager)
            THROWS_ONLY(cannot_perform_query_exc_t);
        ~subscription_t();
        std::vector<counted_t<const datum_t> > get_els(
            batcher_t *batcher, const signal_t *interruptor)
            THROWS_ONLY(interrupted_exc_t);
    private:
        friend class changefeed_t;
        void maybe_signal_cond() THROWS_NOTHING;
        void add_el(counted_t<const datum_t> d);
        void finish();

        bool finished;
        scoped_ptr_t<base_exc_t> exc;
        cond_t *cond; // NULL unless we're waiting.
        std::deque<counted_t<const datum_t> > els;

        changefeed_manager_t *manager;
        std::map<uuid_u, scoped_ptr_t<changefeed_t> >::iterator changefeed;

        scoped_ptr_t<auto_drainer_t> drainer;
    };

    // Throws query language exceptions.
    counted_t<datum_stream_t> changefeed(const counted_t<table_t> &tbl, env_t *env);
    mailbox_manager_t *get_manager() { return manager; }
private:
    mailbox_manager_t *manager;
    std::map<uuid_u, scoped_ptr_t<changefeed_t> > changefeeds;
};

} // namespace ql

#endif // RDB_PROTOCOL_CHANGEFEED_HPP_