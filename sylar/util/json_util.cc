#include "json_util.h"
#include "sylar/util.h"

namespace sylar {

bool JsonUtil::NeedEscape(const std::string& v) {
    for(auto& c : v) {
        switch(c) {
            case '\f':
            case '\t':
            case '\r':
            case '\n':
            case '\b':
            case '"':
            case '\\':
                return true;
            default:
                break;
        }
    }
    return false;
}

std::string JsonUtil::Escape(const std::string& v) {
    size_t size = 0;
    for(auto& c : v) {
        switch(c) {
            case '\f':
            case '\t':
            case '\r':
            case '\n':
            case '\b':
            case '"':
            case '\\':
                size += 2;
                break;
            default:
                size += 1;
                break;
        }
    }
    if(size == v.size()) {
        return v;
    }

    std::string rt;
    rt.resize(size);
    for(auto& c : v) {
        switch(c) {
            case '\f':
                rt.append("\\f");
                break;
            case '\t':
                rt.append("\\t");
                break;
            case '\r':
                rt.append("\\r");
                break;
            case '\n':
                rt.append("\\n");
                break;
            case '\b':
                rt.append("\\b");
                break;
            case '"':
                rt.append("\\\"");
                break;
            case '\\':
                rt.append("\\\\");
                break;
            default:
                rt.append(1, c);
                break;

        }
    }
    return rt;
}

std::string JsonUtil::GetStringValue(const Json::Value& v 
                      ,const std::string& default_value) {
    if(v.isString()) {
        return v.asString();
    } else if(v.isArray()) {
        return ToString(v);
    } else if(v.isObject()) {
        return ToString(v);
    } else {
        return v.asString();
    }
    return default_value;
}

double JsonUtil::GetDoubleValue(const Json::Value& v
                 ,double default_value) {
    if(v.isDouble()) {
        return v.asDouble();
    } else if(v.isString()) {
        return TypeUtil::Atof(v.asString());
    }
    return default_value;
}

int32_t JsonUtil::GetInt32Value(const Json::Value& v
                 ,int32_t default_value) {
    if(v.isInt()) {
        return v.asInt();
    } else if(v.isString()) {
        return TypeUtil::Atoi(v.asString());
    }
    return default_value;
}

uint32_t JsonUtil::GetUint32Value(const Json::Value& v
                   ,uint32_t default_value) {
    if(v.isUInt()) {
        return v.asUInt();
    } else if(v.isString()) {
        return TypeUtil::Atoi(v.asString());
    }
    return default_value;
}

int64_t JsonUtil::GetInt64Value(const Json::Value& v
                 ,int64_t default_value) {
    if(v.isInt64()) {
        return v.asInt64();
    } else if(v.isString()) {
        return TypeUtil::Atoi(v.asString());
    }
    return default_value;
}

uint64_t JsonUtil::GetUint64Value(const Json::Value& v
                   ,uint64_t default_value) {
    if(v.isUInt64()) {
        return v.asUInt64();
    } else if(v.isString()) {
        return TypeUtil::Atoi(v.asString());
    }
    return default_value;
}


std::string JsonUtil::GetString(const Json::Value& json
                      ,const std::string& name
                      ,const std::string& default_value) {
    if(!json.isMember(name)) {
        return default_value;
    }
    return GetStringValue(json[name], default_value);
}

double JsonUtil::GetDouble(const Json::Value& json
                 ,const std::string& name
                 ,double default_value) {
    if(!json.isMember(name)) {
        return default_value;
    }
    return GetDoubleValue(json[name], default_value);
}

int32_t JsonUtil::GetInt32(const Json::Value& json
                 ,const std::string& name
                 ,int32_t default_value) {
    if(!json.isMember(name)) {
        return default_value;
    }
    return GetInt32Value(json[name], default_value);
}

uint32_t JsonUtil::GetUint32(const Json::Value& json
                   ,const std::string& name
                   ,uint32_t default_value) {
    if(!json.isMember(name)) {
        return default_value;
    }
    return GetUint32Value(json[name], default_value);
}

int64_t JsonUtil::GetInt64(const Json::Value& json
                 ,const std::string& name
                 ,int64_t default_value) {
    if(!json.isMember(name)) {
        return default_value;
    }
    return GetInt64Value(json[name], default_value);
}

uint64_t JsonUtil::GetUint64(const Json::Value& json
                   ,const std::string& name
                   ,uint64_t default_value) {
    if(!json.isMember(name)) {
        return default_value;
    }
    return GetUint64Value(json[name], default_value);
}

bool JsonUtil::FromString(Json::Value& json, const std::string& v) {
    Json::Reader reader;
    return reader.parse(v, json);
}

//static Json::StreamWriter* GetJsonStreamWriter() {
//    static Json::StreamWriterBuilder builder;
//    builder["commentStyle"] = "None";
//    builder["indentation"] = "";
//    return builder.newStreamWriter();
//}
//
//static Json::StreamWriterBuilder GetJsonStreamBuilder() {
//    Json::StreamWriterBuilder builder;
//    builder["commentStyle"] = "None";
//    builder["indentation"] = "";
//    return builder;
//}

std::string JsonUtil::ToString(const Json::Value& json, bool emit_utf8) {
    ////static Json::StreamWriter* s_writer = GetJsonStreamWriter();

    ////static Json::StreamWriterBuilder builder = GetJsonStreamBuilder();
    Json::StreamWriterBuilder builder;
    builder["commentStyle"] = "None";
    builder["indentation"] = "";
    builder["emitUTF8"] = emit_utf8;
    return Json::writeString(builder, json);

    //std::unique_ptr<Json::StreamWriter> const writer(
    //  builder.newStreamWriter());
    //std::stringstream ss;
    //writer->write(json, &ss);
    //return ss.str();

    //Json::FastWriter w;
    //return w.write(json);
}

}
