#include "load_balance.h"
#include "sylar/log.h"
#include "sylar/worker.h"
#include "sylar/macro.h"
#include <math.h>

namespace sylar {

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

HolderStats HolderStatsSet::getTotal() const {
    HolderStats rt;
    for(auto& i : m_stats) {
#define XX(f) rt.f += i.f
        XX(m_usedTime);
        XX(m_total);
        XX(m_doing);
        XX(m_timeouts);
        XX(m_oks);
        XX(m_errs);
#undef XX
    }
    return rt;
}

std::string HolderStats::toString() {
    std::stringstream ss;
    ss << "[Stat total=" << m_total
       << " used_time=" << m_usedTime
       << " doing=" << m_doing
       << " timeouts=" << m_timeouts
       << " oks=" << m_oks
       << " errs=" << m_errs
       << " oks_rate=" << (m_total ? (m_oks * 100.0 / m_total) : 0)
       << " errs_rate=" << (m_total ? (m_errs * 100.0 / m_total) : 0)
       << " avg_used=" << (m_oks ? (m_usedTime * 1.0 / m_oks) : 0)
       << " weight=" << getWeight(1)
       << "]";
    return ss.str();
}

void LoadBalanceItem::close() {
    if(m_stream) {
        auto stream = m_stream;
        sylar::WorkerMgr::GetInstance()->schedule("service_io", [stream](){
            stream->close();
        });
    }
}

bool LoadBalanceItem::isValid() {
    return m_stream && m_stream->isConnected();
}

std::string LoadBalanceItem::toString() {
    std::stringstream ss;
    ss << "[Item id=" << m_id
       << " weight=" << getWeight()
       << " discovery_time=" << sylar::Time2Str(m_discoveryTime);
    if(!m_stream) {
        ss << " stream=null";
    } else {
        ss << " stream=[" << m_stream->getRemoteAddressString()
           << " is_connected=" << m_stream->isConnected() << "]";
    }
    ss << m_stats.getTotal().toString() << "]";
    //float w = 0;
    //float w2 = 0;
    //for(uint64_t n = 0; n < 5; ++n) {
    //    if(n) {
    //        ss << ",";
    //    } else {
    //        ss << m_stats.get(time(0) - n).toString();
    //    }
    //    w += m_stats.get(time(0) - n).getWeight();
    //    w2 += m_stats.get(time(0) - n).getWeight() * (1 - n * 0.1);
    //    ss << m_stats.get(time(0) - n).getWeight();
    //}
    //ss << " w=" << w;
    //ss << " w2=" << w2;
    return ss.str();
}

LoadBalanceItem::ptr LoadBalance::getById(uint64_t id) {
    RWMutexType::ReadLock lock(m_mutex);
    auto it = m_datas.find(id);
    return it == m_datas.end() ? nullptr : it->second;
}

void LoadBalance::add(LoadBalanceItem::ptr v) {
    RWMutexType::WriteLock lock(m_mutex);
    m_datas[v->getId()] = v;
    initNolock();
}

void LoadBalance::del(LoadBalanceItem::ptr v) {
    RWMutexType::WriteLock lock(m_mutex);
    m_datas.erase(v->getId());
    initNolock();
}

void LoadBalance::update(const std::unordered_map<uint64_t, LoadBalanceItem::ptr>& adds
                        ,std::unordered_map<uint64_t, LoadBalanceItem::ptr>& dels) {
    RWMutexType::WriteLock lock(m_mutex);
    for(auto& i : dels) {
        auto it = m_datas.find(i.first);
        if(it != m_datas.end()) {
            i.second = it->second;
            m_datas.erase(it);
        }
    }
    for(auto& i : adds) {
        m_datas[i.first] = i.second;
    }
    initNolock();
}

void LoadBalance::set(const std::vector<LoadBalanceItem::ptr>& vs) {
    RWMutexType::WriteLock lock(m_mutex);
    m_datas.clear();
    for(auto& i : vs){
        m_datas[i->getId()] = i;
    }
    initNolock();
}

void LoadBalance::init() {
    RWMutexType::WriteLock lock(m_mutex);
    initNolock();
}

std::string LoadBalance::statusString(const std::string& prefix) {
    RWMutexType::ReadLock lock(m_mutex);
    decltype(m_datas) datas = m_datas;
    lock.unlock();
    std::stringstream ss;
    ss << prefix << "init_time: " << sylar::Time2Str(m_lastInitTime / 1000) << std::endl;
    for(auto& i : datas) {
        ss << prefix << i.second->toString() << std::endl;
    }
    return ss.str();
}

void LoadBalance::checkInit() {
    //SYLAR_LOG_INFO(g_logger) << "check init";
    uint64_t ts = sylar::GetCurrentMS();
    if(ts - m_lastInitTime > 500) {
        init();
        m_lastInitTime = ts;
    }
}

void RoundRobinLoadBalance::initNolock() {
    decltype(m_items) items;
    for(auto& i : m_datas){
        if(i.second->isValid()) {
            items.push_back(i.second);
        }
    }
    items.swap(m_items);
}

LoadBalanceItem::ptr RoundRobinLoadBalance::get(uint64_t v) {
    //checkInit();
    RWMutexType::ReadLock lock(m_mutex);
    if(m_items.empty()) {
        return nullptr;
    }
    uint32_t r = (v == (uint64_t)-1 ? rand() : v) % m_items.size();
    for(size_t i = 0; i < m_items.size(); ++i) {
        auto& h = m_items[(r + i) % m_items.size()];
        if(h->isValid()) {
            return h;
        }
    }
    return nullptr;
}

//FairLoadBalanceItem::ptr WeightLoadBalance::getAsFair() {
//    auto item = get();
//    if(item) {
//        return std::static_pointer_cast<FairLoadBalanceItem>(item);
//    }
//    return nullptr;
//}

LoadBalanceItem::ptr WeightLoadBalance::get(uint64_t v) {
    //checkInit();
    RWMutexType::ReadLock lock(m_mutex);
    int32_t idx = getIdx(v);
    if(idx == -1) {
        return nullptr;
    }

    //TODO fix weight
    for(size_t i = 0; i < m_items.size(); ++i) {
        auto& h = m_items[(idx + i) % m_items.size()];
        if(h->isValid()) {
            return h;
        }
    }
    return nullptr;
}

void WeightLoadBalance::initNolock() {
    decltype(m_items) items;
    for(auto& i : m_datas){
        if(i.second->isValid()) {
            items.push_back(i.second);
        }
    }
    items.swap(m_items);

    int64_t total = 0;
    m_weights.resize(m_items.size());
    for(size_t i = 0; i < m_items.size(); ++i) {
        total += m_items[i]->getWeight();
        m_weights[i] = total;
    }
}

int32_t WeightLoadBalance::getIdx(uint64_t v) {
    if(m_weights.empty()) {
        return -1;
    }
    int64_t total = *m_weights.rbegin();
    uint64_t dis = (v == (uint64_t)-1 ? rand() : v) % total;
    auto it = std::upper_bound(m_weights.begin()
                ,m_weights.end(), dis);
    SYLAR_ASSERT(it != m_weights.end());
    return std::distance(m_weights.begin(), it);
}

void HolderStats::clear() {
    m_usedTime = 0;
    m_total = 0;
    m_doing = 0;
    m_timeouts = 0;
    m_oks = 0;
    m_errs = 0;
}

void HolderStats::add(const HolderStats& hs) {
    this->m_usedTime += hs.m_usedTime;
    this->m_total += hs.m_total;
    this->m_doing += hs.m_doing;
    this->m_timeouts += hs.m_timeouts;
    this->m_oks += hs.m_oks;
    this->m_errs += hs.m_errs;
}

uint64_t HolderStats::getWeight(const HolderStats& hs, uint64_t join_time) {
    if(hs.m_total <= 0) {
        return 100;
    }

    float all_avg_cost = hs.m_usedTime * 1.0 / hs.m_total;
    float cost_weight = 1.0;
    float err_weight = 1.0;
    float timeout_weight = 1.0;
    float doing_weight = 1.0;

    float time_weight = 1.0;
    int64_t time_diff = time(0) - join_time;
    if(time_diff < 180) {
        time_weight = std::min(0.1, time_diff / 180.0);
    }

    if(m_total > 10) {
        cost_weight = 2 - std::min(1.9, (m_usedTime * 1.0 / m_total) / all_avg_cost);
        err_weight = 1 - std::min(0.9, m_errs * 5.0 / m_total);
        timeout_weight = 1 - std::min(0.9, m_timeouts * 2.5 / m_total);
        doing_weight = 1 - std::min(0.9, m_doing * 1.0 / m_total);
    }

    return std::min(1, (int)(200 * cost_weight * err_weight * timeout_weight * doing_weight * time_weight));
}

float HolderStats::getWeight(float rate) {
    //if(m_total == 0) {
    //    return 0.1;
    //}
    float base = m_total + 20;
    return std::min((m_oks * 1.0 / (m_usedTime + 1)) * 2.0, 50.0)
        * (1 - 4.0 * m_timeouts / base) 
        * (1 - 1 * m_doing / base)
        * (1 - 10.0 * m_errs / base) * rate;
    //return std::min((m_oks * 1.0 / (m_usedTime + 1)) * 10.0, 100.0)
    //    * (1 - (2.0 * pow(m_timeouts, 1.3) / base))
    //    * (1 - (1.0 * pow(m_doing, 1.1) / base))
    //    * (1 - (4.0 * pow(m_errs, 1.5) / base)) * rate;
    //return std::min(((m_oks + 1) * 1.0 / (m_usedTime + 1)) * 10.0, 100.0)
    //    * std::min((base / (m_timeouts * 3.0 + 1)) / 100.0, 10.0)
    //    * std::min((base / ( m_doing * 1.0 + 1)) / 100.0, 10.0)
    //    * std::min((base / (m_errs * 5.0 + 1)) / 100.0, 10.0);
}

HolderStatsSet::HolderStatsSet(uint32_t size) {
    m_stats.resize(size);
}

void HolderStatsSet::init(const uint32_t& now) {
    if(m_lastUpdateTime < now) {
        for(uint32_t t = m_lastUpdateTime + 1, i = 0;
                t <= now && i < m_stats.size(); ++t, ++i) {
            m_stats[t % m_stats.size()].clear();
        }
        m_lastUpdateTime = now;
    }
}

HolderStats& HolderStatsSet::get(const uint32_t& now) {
    init(now);
    return m_stats[now % m_stats.size()];
}

float HolderStatsSet::getWeight(const uint32_t& now) {
    init(now);
    float v = 0;
    for(size_t i = 1; i < m_stats.size(); ++i) {
        v += m_stats[(now - i) % m_stats.size()].getWeight(1 - 0.1 * i);
    }
    return v;
    //return getTotal().getWeight(1.0);
}

//int32_t FairLoadBalanceItem::getWeight() {
//    int32_t v = m_weight * m_stats.getWeight();
//    if(m_stream->isConnected()) {
//        return v > 1 ? v : 1;
//    }
//    return 1;
//}

HolderStats& LoadBalanceItem::get(const uint32_t& now) {
    return m_stats.get(now);
}

void FairLoadBalance::initNolock() {
    decltype(m_items) items;
    for(auto& i : m_datas){
        if(i.second->isValid()) {
            items.push_back(i.second);
        }
    }
    items.swap(m_items);

    int64_t total = 0;
    m_weights.resize(m_items.size());

    HolderStats total_stats;
    std::vector<HolderStats> stats;
    stats.resize(m_items.size());
    for(size_t i = 0; i < m_items.size(); ++i) {
        stats[i] = m_items[i]->getStatsSet().getTotal();
        total_stats.add(stats[i]);
    }

    for(size_t i = 0; i < stats.size(); ++i) {
        auto w = stats[i].getWeight(total_stats, m_items[i]->getDiscoveryTime());
        m_items[i]->setWeight(w);
        total += w;
        m_weights[i] = total;
    }
}

//LoadBalanceItem::ptr FairLoadBalance::get() {
//    RWMutexType::ReadLock lock(m_mutex);
//    int32_t idx = getIdx();
//    if(idx == -1) {
//        return nullptr;
//    }
//
//    //TODO fix weight
//    for(size_t i = 0; i < m_items.size(); ++i) {
//        auto& h = m_items[(idx + i) % m_items.size()];
//        if(h->isValid()) {
//            return h;
//        }
//    }
//    return nullptr;
//}
//
//void FairLoadBalance::initNolock() {
//    decltype(m_items) items;
//    for(auto& i : m_datas){
//        items.push_back(i.second);
//    }
//    items.swap(m_items);
//
//    m_weights.resize(m_items.size());
//    int32_t total = 0;
//    for(size_t i = 0; i < m_items.size(); ++i) {
//        total += m_items[i]->getWeight();
//        m_weights[i] = total;
//    }
//}
//
//int32_t FairLoadBalance::getIdx() {
//    if(m_weights.empty()) {
//        return -1;
//    }
//    int32_t total = *m_weights.rbegin();
//    auto it = std::upper_bound(m_weights.begin()
//                ,m_weights.end(), rand() % total);
//    return std::distance(it, m_weights.begin());
//}

SDLoadBalance::SDLoadBalance(IServiceDiscovery::ptr sd)
    :m_sd(sd) {
    m_sd->addServiceCallback(std::bind(&SDLoadBalance::onServiceChange, this
                ,std::placeholders::_1
                ,std::placeholders::_2
                ,std::placeholders::_3
                ,std::placeholders::_4));
}

LoadBalance::ptr SDLoadBalance::get(const std::string& domain, const std::string& service, bool auto_create) {
    do {
        RWMutexType::ReadLock lock(m_mutex);
        auto it = m_datas.find(domain);
        if(it == m_datas.end()) {
            break;
        }
        auto iit = it->second.find(service);
        if(iit == it->second.end()) {
            break;
        }
        return iit->second;
    } while(0);

    if(!auto_create) {
        return nullptr;
    }

    auto type = getType(domain, service);

    auto lb = createLoadBalance(type);
    RWMutexType::WriteLock lock(m_mutex);
    m_datas[domain][service] = lb;
    lock.unlock();
    return lb;
}


ILoadBalance::Type SDLoadBalance::getType(const std::string& domain, const std::string& service) {
    RWMutexType::ReadLock lock(m_mutex);
    auto it = m_types.find(domain);
    if(it == m_types.end()) {
        return ILoadBalance::UNKNOW;
    }
    auto iit = it->second.find(service);
    if(iit == it->second.end()) {
        iit = it->second.find("all");
        if(iit != it->second.end()) {
            return iit->second;
        }
        return ILoadBalance::UNKNOW;
    }
    return iit->second;
}

LoadBalance::ptr SDLoadBalance::createLoadBalance(ILoadBalance::Type type) {
    if(type == ILoadBalance::ROUNDROBIN) {
        return std::make_shared<RoundRobinLoadBalance>();
    } else if(type == ILoadBalance::WEIGHT) {
        return std::make_shared<WeightLoadBalance>();
    } else if(type == ILoadBalance::FAIR) {
        return std::make_shared<FairLoadBalance>();
    }
    return nullptr;
}

LoadBalanceItem::ptr SDLoadBalance::createLoadBalanceItem(ILoadBalance::Type type) {
    LoadBalanceItem::ptr item;
    if(type == ILoadBalance::ROUNDROBIN) {
        item = std::make_shared<LoadBalanceItem>();
    } else if(type == ILoadBalance::WEIGHT) {
        item = std::make_shared<LoadBalanceItem>();
    } else if(type == ILoadBalance::FAIR) {
        item = std::make_shared<LoadBalanceItem>();
    }
    return item;
}

void SDLoadBalance::onServiceChange(const std::string& domain, const std::string& service
                            ,const std::unordered_map<uint64_t, ServiceItemInfo::ptr>& old_value
                            ,const std::unordered_map<uint64_t, ServiceItemInfo::ptr>& new_value) {
    //SYLAR_LOG_INFO(g_logger) << "onServiceChange domain=" << domain
    //                         << " service=" << service;
    auto type = getType(domain, service);
    if(type == ILoadBalance::UNKNOW) {
        return;
    }

    std::unordered_map<uint64_t, ServiceItemInfo::ptr> add_values;
    std::unordered_map<uint64_t, LoadBalanceItem::ptr> del_infos;

    for(auto& i : old_value) {
        //if(i.second->getType() != m_type) {
        //    continue;
        //}
        if(new_value.find(i.first) == new_value.end()) {
            del_infos[i.first];
        }
    }
    for(auto& i : new_value) {
        //if(i.second->getType() != m_type) {
        //    continue;
        //}
        if(old_value.find(i.first) == old_value.end()) {
            add_values.insert(i);
        }
    }
    std::unordered_map<uint64_t, LoadBalanceItem::ptr> add_infos;
    for(auto& i : add_values) {
        //SYLAR_LOG_INFO(g_logger) << "*** " << i.second->getType();
        //if(i.second->getType() != m_type) {
        //    continue;
        //}
        auto stream = m_cb(domain, service, i.second);
        if(!stream) {
            SYLAR_LOG_ERROR(g_logger) << "create stream fail, " << i.second->toString();
            continue;
        }
        
        LoadBalanceItem::ptr lditem = createLoadBalanceItem(type);
        lditem->setId(i.first);
        lditem->setStream(stream);
        lditem->setWeight(10000);

        add_infos[i.first] = lditem;
    }

    if(!add_infos.empty() || !del_infos.empty()) {
        auto lb = get(domain, service, true);
        lb->update(add_infos, del_infos);
        for(auto& i : del_infos) {
            if(i.second) {
                i.second->close();
            }
        }
    }
}

bool SDLoadBalance::doQuery() {
    bool rt = m_sd->doQuery();
    return rt;
}
bool SDLoadBalance::doRegister() {
    return m_sd->doRegister();
}

void SDLoadBalance::start() {
    if(m_timer) {
        return;
    }
    m_timer = sylar::IOManager::GetThis()->addTimer(500,
            std::bind(&SDLoadBalance::refresh, this), true);
    m_sd->start();
}

void SDLoadBalance::stop() {
    if(!m_timer) {
        return;
    }
    m_timer->cancel();
    m_timer = nullptr;
    m_sd->stop();
}

void SDLoadBalance::refresh() {
    if(m_isRefresh) {
        return;
    }
    m_isRefresh = true;

    RWMutexType::ReadLock lock(m_mutex);
    auto datas = m_datas;
    lock.unlock();

    for(auto& i : datas) {
        for(auto& n : i.second) {
            n.second->checkInit();
        }
    }
    m_isRefresh = false;
}

void SDLoadBalance::initConf(const std::unordered_map<std::string
                            ,std::unordered_map<std::string,std::string> >& confs) {
    decltype(m_types) types;
    std::unordered_map<std::string, std::unordered_set<std::string> > query_infos;
    for(auto& i : confs) {
        for(auto& n : i.second) {
            ILoadBalance::Type t = ILoadBalance::FAIR;
            if(n.second == "round_robin") {
                t = ILoadBalance::ROUNDROBIN;
            } else if(n.second == "weight") {
                t = ILoadBalance::WEIGHT;
            } else if(n.second == "fair") {
                t = ILoadBalance::FAIR;
            }
            types[i.first][n.first] = t;
            query_infos[i.first].insert(n.first);
        }
    }
    m_sd->setQueryServer(query_infos);
    RWMutexType::WriteLock lock(m_mutex);
    types.swap(m_types);
    lock.unlock();
}

std::string SDLoadBalance::statusString() {
    RWMutexType::ReadLock lock(m_mutex);
    decltype(m_datas) datas = m_datas;
    lock.unlock();
    std::stringstream ss;
    for(auto& i : datas) {
        ss << i.first << ":" << std::endl;
        for(auto& n : i.second) {
            ss << "\t" << n.first << ":" << std::endl;
            ss << n.second->statusString("\t\t") << std::endl;
        }
    }
    return ss.str();
}

}
