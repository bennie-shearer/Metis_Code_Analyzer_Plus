/* Metis Code Analyzer Plus - minimal JSON writer
 * Header-only, zero external dependencies, 7-bit ASCII, C++20.
 * Includes an explicit const char* overload so string literals are not
 * silently converted to bool.
 */
#ifndef METIS_CODE_ANALYZER_JSON_HPP
#define METIS_CODE_ANALYZER_JSON_HPP

#include <cstdint>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace metis::codeanalyzer::json {

class Value;
using Array  = std::vector<Value>;
using Object = std::vector<std::pair<std::string, Value>>;

enum class Type { Null, Bool, Int, Double, String, Array, Object };

class Value {
public:
    Value() : type_(Type::Null) {}
    Value(std::nullptr_t) : type_(Type::Null) {}
    Value(bool b) : type_(Type::Bool), bool_(b) {}
    Value(int v) : type_(Type::Int), int_(static_cast<std::int64_t>(v)) {}
    Value(long v) : type_(Type::Int), int_(static_cast<std::int64_t>(v)) {}
    Value(long long v) : type_(Type::Int), int_(static_cast<std::int64_t>(v)) {}
    Value(unsigned v) : type_(Type::Int), int_(static_cast<std::int64_t>(v)) {}
    Value(unsigned long v) : type_(Type::Int), int_(static_cast<std::int64_t>(v)) {}
    Value(unsigned long long v) : type_(Type::Int), int_(static_cast<std::int64_t>(v)) {}
    Value(double v) : type_(Type::Double), dbl_(v) {}
    Value(const char* s) : type_(Type::String), str_(s ? s : "") {}
    Value(std::string s) : type_(Type::String), str_(std::move(s)) {}
    Value(Array a) : type_(Type::Array), arr_(std::move(a)) {}
    Value(Object o) : type_(Type::Object), obj_(std::move(o)) {}

    static Value array() { Value v; v.type_ = Type::Array; return v; }
    static Value object() { Value v; v.type_ = Type::Object; return v; }

    void push_back(Value v) { type_ = Type::Array; arr_.push_back(std::move(v)); }
    void set(const std::string& key, Value v) {
        type_ = Type::Object;
        obj_.emplace_back(key, std::move(v));
    }

    std::string dump() const {
        std::ostringstream os;
        write(os);
        return os.str();
    }

private:
    void write(std::ostringstream& os) const {
        switch (type_) {
        case Type::Null:   os << "null"; break;
        case Type::Bool:   os << (bool_ ? "true" : "false"); break;
        case Type::Int:    os << int_; break;
        case Type::Double: write_double(os); break;
        case Type::String: write_string(os, str_); break;
        case Type::Array:  write_array(os); break;
        case Type::Object: write_object(os); break;
        }
    }

    void write_double(std::ostringstream& os) const {
        std::ostringstream tmp;
        tmp.precision(6);
        tmp << dbl_;
        os << tmp.str();
    }

    void write_array(std::ostringstream& os) const {
        os << '[';
        for (std::size_t i = 0; i < arr_.size(); ++i) {
            if (i) os << ',';
            arr_[i].write(os);
        }
        os << ']';
    }

    void write_object(std::ostringstream& os) const {
        os << '{';
        for (std::size_t i = 0; i < obj_.size(); ++i) {
            if (i) os << ',';
            write_string(os, obj_[i].first);
            os << ':';
            obj_[i].second.write(os);
        }
        os << '}';
    }

    static void write_string(std::ostringstream& os, const std::string& s) {
        os << '"';
        for (char c : s) {
            switch (c) {
            case '"':  os << "\\\""; break;
            case '\\': os << "\\\\"; break;
            case '\b': os << "\\b"; break;
            case '\f': os << "\\f"; break;
            case '\n': os << "\\n"; break;
            case '\r': os << "\\r"; break;
            case '\t': os << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    static const char* hex = "0123456789abcdef";
                    os << "\\u00"
                       << hex[(static_cast<unsigned char>(c) >> 4) & 0xF]
                       << hex[static_cast<unsigned char>(c) & 0xF];
                } else {
                    os << c;
                }
            }
        }
        os << '"';
    }

    Type type_;
    bool bool_ = false;
    std::int64_t int_ = 0;
    double dbl_ = 0.0;
    std::string str_;
    Array arr_;
    Object obj_;
};

} /* namespace metis::codeanalyzer::json */

#endif /* METIS_CODE_ANALYZER_JSON_HPP */
