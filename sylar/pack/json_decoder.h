#ifndef __SYLAR_PACK_JSON_DECODER_H__
#define __SYLAR_PACK_JSON_DECODER_H__

#include <json/json.h>
#include "pack.h"
#include <string.h>
#include <list>
#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <memory>

namespace sylar {
namespace pack {

class JsonDecoder {
public:
    JsonDecoder(const Json::Value& value)
        :m_value(value)
        ,m_cur(&m_value) {
    }

    //template<class T>
    //SYLAR_NOT_PACK(T, bool) decode(const std::string& name, T& v, const PackFlag& flag) {
    //    if((*m_cur)[name].isNull()) {
    //        return true;
    //    }
    //    v = boost::lexical_cast<T>((*m_cur)[name].asString());
    //    return true;
    //}

#define XX_DECODE(ctype, type) \
    bool decode(const std::string& name, ctype& v, const PackFlag& flag) { \
        if(!m_cur->isMember(name)) { \
            return true; \
        } \
        auto& tmp = (*m_cur)[name]; \
        if(tmp.is##type()) { \
            v = tmp.as##type(); \
        } else if(tmp.isString()) { \
            v = boost::lexical_cast<ctype>(tmp.asString()); \
        } \
        return true; \
    } \
    bool decode(ctype& v, const PackFlag& flag) { \
        if(m_cur->is##type()) { \
            v = m_cur->as##type(); \
        } else if(m_cur->isString()) { \
            v = boost::lexical_cast<ctype>(m_cur->asString()); \
        } \
        return true; \
    }

    XX_DECODE(int8_t,  Int);
    XX_DECODE(int16_t, Int);
    XX_DECODE(int32_t, Int);
    XX_DECODE(long long, Int64);
    XX_DECODE(int64_t, Int64);
    XX_DECODE(uint8_t,  UInt);
    XX_DECODE(uint16_t, UInt);
    XX_DECODE(uint32_t, UInt);
    XX_DECODE(unsigned long long, UInt64);
    XX_DECODE(uint64_t, UInt64);
    XX_DECODE(float,    Double);
    XX_DECODE(double,   Double);
#undef XX_DECODE
    bool decode(const std::string& name, bool& v, const PackFlag& flag) {
        if(!m_cur->isMember(name)) {
            return true;
        }
        auto& tmp = (*m_cur)[name];
        if(tmp.isBool()) {
            v = tmp.asBool();
        } else if(tmp.isString()) {
            if(tmp.asString() == "true") {
                v = true;
            } else if(tmp.asString() == "false") {
                v = false;
            } else {
                //TODO
            }
        }
        //TODO
        return true;
    }

    bool decode(const std::string& name, std::string& v, const PackFlag& flag) {
        if(!m_cur->isMember(name)) {
            return true;
        }
        auto& tmp = (*m_cur)[name];
        if(tmp.isString()) {
            v = tmp.asString();
        }
        return true;
    }

    bool decode(const std::string& name, char* v, const PackFlag& flag) {
        if(!m_cur->isMember(name)) {
            return true;
        }
        auto& tmp = (*m_cur)[name];
        if(tmp.isString()) {
            auto t = tmp.asString();
            strncpy(v, t.data(), t.size());
        }
        return true;
    }

    bool decode(bool& v, const PackFlag& flag) {
        if(m_cur->isBool()) {
            v = m_cur->asBool();
        } else if(m_cur->isString()) {
            if(m_cur->asString() == "true") {
                v = true;
            } else if(m_cur->asString() == "false") {
                v = false;
            } else {
                //TODO
            }
        }
        //TODO
        return true;
    }

    bool decode(std::string& v, const PackFlag& flag) {
        if(m_cur->isString()) {
            v = m_cur->asString();
        } else {
            //TODO
        }
        return true;
    }

    bool decode(char* v, const PackFlag& flag) {
        if(m_cur->isString()) {
            auto t = m_cur->asString();
            strncpy(v, t.data(), t.size());
        } else {
            //TODO
        }
        return true;
    }

    template<class T, int N>
    bool decode(const std::string& name, T (&v)[N], const PackFlag& flag) {
        memset(v, 0, sizeof(T) * N);
        if(!m_cur->isMember(name)) {
            return true;
        }
        auto& tmp = (*m_cur)[name];
        if(tmp.isArray()) {
            auto cur = m_cur;
            int idx = 0;
            for(auto& x : tmp) {
                m_cur = &x;
                decode(v[idx], flag);
                ++idx;
            }
            m_cur = cur;
        } else {
        }
        return true;
    }

    template<class T, int N>
    bool decode(T (&v)[N], const PackFlag& flag) {
        memset(v, 0, sizeof(T) * N);
        auto& tmp = *m_cur;
        if(tmp.isArray()) {
            auto cur = m_cur;
            int idx = 0;
            for(auto& x : tmp) {
                m_cur = &x;
                decode(v[idx], flag);
                ++idx;
            }
            m_cur = cur;
        } else {
            /*//TODO */
        }
        return true;
    }

    template<class T>
    SYLAR_IS_PACK(T, bool) decode(const std::string& name, T& v, const PackFlag& flag) {
        if((*m_cur)[name].isNull()) {
            return true;
        }
        auto cur = m_cur;
        m_cur = &(*m_cur)[name];
        v.__sylar_decode__(*this, flag);
        m_cur = cur;
        return true;
    }

    template<class T>
    SYLAR_IS_PACK(T, bool) decode(T& v, const PackFlag& flag) {
        v.__sylar_decode__(*this, flag);
        return true;
    }

    template<class T>
    SYLAR_IS_PACK(T, bool) decode_inherit(T& v, const PackFlag& flag) {
        v.__sylar_decode__(*this, flag);
        return true;
    }

    template<class T>
    SYLAR_IS_PACK_OUT(T, bool) decode(const std::string& name, T& v, const PackFlag& flag) {
        if((*m_cur)[name].isNull()) {
            return true;
        }
        auto cur = m_cur;
        m_cur = &(*m_cur)[name];
        __sylar_decode__(*this, v, flag);
        m_cur = cur;
        return true;
    }

    template<class T>
    SYLAR_IS_PACK_OUT(T, bool) decode(T& v, const PackFlag& flag) {
        __sylar_decode__(*this, v, flag);
        return true;
    }

#define XX_DECODE(arr, fun) \
    template<class T, class... Args> \
    bool decode(const std::string& name, arr<T, Args...>& v, const PackFlag& flag) { \
        v.clear(); \
        if(!m_cur->isMember(name)) { \
            return true; \
        } \
        auto& tmp = (*m_cur)[name]; \
        if(tmp.isArray()) { \
            auto cur = m_cur; \
            for(auto& x : tmp) { \
                m_cur = &x; \
                T t; \
                decode(t, flag); \
                v.fun(t); \
            } \
            m_cur = cur; \
        } else { \
        } \
        return true; \
    } \
    template<class T, class... Args> \
    bool decode(arr<T, Args...>& v, const PackFlag& flag) { \
        v.clear(); \
        auto& tmp = *m_cur; \
        if(tmp.isArray()) { \
            auto cur = m_cur; \
            for(auto& x : tmp) { \
                m_cur = &x; \
                T t; \
                decode(t, flag); \
                v.fun(t); \
            } \
            m_cur = cur; \
        } else { \
            /*//TODO */\
        } \
        return true; \
    }

    XX_DECODE(std::vector,             emplace_back);
    XX_DECODE(std::list,               emplace_back);
    XX_DECODE(std::set,                emplace);
    XX_DECODE(std::unordered_set,      emplace);
#undef XX_DECODE

#define XX_DECODE(m) \
    template<class T, class... Args> \
    bool decode(const std::string& name, m<std::string, T, Args...>& v, const PackFlag& flag) { \
        v.clear(); \
        if(!m_cur->isMember(name)) { \
            return true; \
        } \
        auto& tmp = (*m_cur)[name]; \
        if(tmp.isObject()) { \
            auto cur = m_cur; \
            for(auto it = tmp.begin(); it != tmp.end(); ++it) { \
                m_cur = &(*it); \
                T t; \
                decode(t, flag); \
                v[it.name()] = t; \
            } \
            m_cur = cur; \
        } else { \
        } \
        return true; \
    } \
    template<class T, class... Args> \
    bool decode(m<std::string, T, Args...>& v, const PackFlag& flag) { \
        v.clear(); \
        auto& tmp = *m_cur; \
        if(tmp.isObject()) { \
            auto cur = m_cur; \
            for(auto it = tmp.begin(); it != tmp.end(); ++it) { \
                m_cur = &(*it); \
                T t; \
                decode(t, flag); \
                v[it.name()] = t; \
            } \
            m_cur = cur; \
        } else { \
            /*//TODO */\
        } \
        return true; \
    }
    XX_DECODE(std::map);
    XX_DECODE(std::unordered_map);
#undef XX_DECODE

#define XX_DECODE(type, fun) \
    template<class T> \
    bool decode(const std::string& name, type<T>& v, const PackFlag& flag) { \
        v = fun(); \
        return decode(name, *v, flag); \
    } \
    template<class T> \
    bool decode(type<T>& v, const PackFlag& flag) { \
        v = fun(); \
        return decode(*v, flag); \
    }

    XX_DECODE(std::shared_ptr, std::make_shared<T>);
    //XX_DECODE(std::unique_ptr, std::make_unique<T>);
    XX_DECODE(std::unique_ptr, std::unique_ptr<T>(new T));
#undef XX_DECODE

    Json::Value& getValue() { return m_value;}
private:
    Json::Value m_value;
    Json::Value* m_cur;
};

template<class T>
bool DecodeFromJson(const Json::Value& json, T& value, const PackFlag& flag) {
    JsonDecoder jd(json);
    return jd.decode(value, flag);
}

template<class T>
bool DecodeFromJsonString(const std::string& json, T& value, const PackFlag& flag) {
    Json::Value jvalue;
    if(!sylar::JsonUtil::FromString(jvalue, json)) {
        //TODO
        return false;
    }
    return DecodeFromJson(jvalue, value, flag);
}

}
}

#endif
