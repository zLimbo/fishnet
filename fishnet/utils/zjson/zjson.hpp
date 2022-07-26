#ifndef ZJSON_H
#define ZJSON_H

#include <errno.h>

#include <cassert>
#include <cmath>
#include <cstring>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace zjson {

enum class Type { kNull, kBoolean, kNumber, kString, kArray, kObject };

enum class Ret {
    kParseOk,
    kParseInvalidValue,
    kParseExpectValue,
    kParseRootNotSingular,
    kParseNumberTooBig,
    kParseMissQuotationMark,
    kParseInvalidStringEscape,
    kParseInvalidStringChar,
    kParseInvalidUnicodeHex,
    kParseInvalidUnicodeSurrogate,
    kParseMissCommaOrSquareBracket,
    kParseMissKey,
    kParseMissColon,
    kParseMissCommaOrCurlyBracket
};

class Json {
public:
    using Boolean = bool;
    using Number = double;
    using String = std::string;
    using Array = std::vector<Json>;
    using Object = std::map<String, Json>;

    union Value {
        Boolean boolean;
        Number number;
        String *str;
        Array *array;
        Object *object;
    };

public:
    inline static const char *kLiteralNull = "null";
    inline static const char *kLiteralTrue = "true";
    inline static const char *kLiteralFalse = "false";

public:
    void clear() {
        switch (type_) {
            case Type::kString:
                if (value_.str) {
                    delete value_.str;
                    value_.str = nullptr;
                }
                break;
            case Type::kArray:
                if (value_.array) {
                    delete value_.array;
                    value_.array = nullptr;
                }
                break;
            case Type::kObject:
                if (value_.object) {
                    delete value_.object;
                    value_.object = nullptr;
                }
            default:
                break;
        }
        memset(&value_, 0, sizeof(value_));
        type_ = Type::kNull;
        stack_.clear();
    }

    void copy(const Json &other) {
        type_ = other.type_;
        switch (type_) {
            case Type::kNumber:
                value_.number = other.value_.number;
                break;
            case Type::kString:
                value_.str = new std::string(*other.value_.str);
                break;
            case Type::kArray:
                value_.array = new Array(*other.value_.array);
                break;
            case Type::kObject:
                value_.object = new Object(*other.value_.object);
                break;
            default:
                break;
        }
    }

    void move(Json &other) {
        type_ = other.type_;
        memcpy(&value_, &other.value_, sizeof(value_));
        memset(&other.value_, 0, sizeof(other.value_));
    }

    Json() : type_(Type::kNull) {
        memset(&value_, 0, sizeof(value_));
    }

    Json(const Json &other) {
        copy(other);
    }

    Json &operator=(const Json &other) {
        if (this != &other) {
            clear();
            copy(other);
        }
        return *this;
    }

    Json(Json &&other) noexcept {
        move(other);
    }

    Json &operator=(Json &&other) noexcept {
        if (this != &other) {
            clear();
            move(other);
        }
        return *this;
    }

    ~Json() {
        clear();
    }

    Json(Number number) : type_(Type::kNumber) {
        value_.number = number;
    }

    Json(int number) : Json(Number(number)) {}

    explicit Json(Boolean b) : type_(Type::kBoolean) {
        value_.boolean = b;
    }

    Json(const char *str) : type_(Type::kString) {
        value_.str = new String(str);
    }

    Json(std::string_view sv) : type_(Type::kString) {
        value_.str = new String(sv);
    }

    Type getType() const {
        return type_;
    }

    Json &operator[](size_t idx) {
        check_type(Type::kArray, "array");
        return value_.array->at(idx);
    }

    bool contain(const std::string key) {
        check_type(Type::kObject, "object");
        return value_.object->find(key) != value_.object->end();
    }

    Json &operator[](const std::string &key) {
        if (type_ == Type::kNull) {
            type_ = Type::kObject;
            value_.object = new Object();
        } else {
            check_type(Type::kObject, "object");
        }
        return (*value_.object)[key];
    }

    template <typename T>
    T get() const {
        if constexpr (std::is_same<T, Boolean>::value) {
            check_type(Type::kBoolean, "Boolean");
            return value_.boolean;
        } else if constexpr (std::is_same<T, Number>::value) {
            check_type(Type::kNumber, "Number");
            return value_.number;
        } else if constexpr (std::is_same<T, String>::value) {
            check_type(Type::kString, "String");
            return *value_.str;
        } else if constexpr (std::is_same<T, Array>::value) {
            check_type(Type::kArray, "Array");
            return *value_.array;
        } else if constexpr (std::is_same<T, Object>::value) {
            check_type(Type::kObject, "Object");
            return *value_.object;
        }
        throw std::runtime_error("type error");
    }

    bool isNull() const {
        return type_ == Type::kNull;
    }
    bool isBoolean() const {
        return type_ == Type::kBoolean;
    }
    bool isString() const {
        return type_ == Type::kString;
    }
    bool isArray() const {
        return type_ == Type::kArray;
    }
    bool isObject() const {
        return type_ == Type::kObject;
    }

    static Json parse(std::string_view text) {
        Json json;
        Ret ret = json.parse(text.data());
        if (ret != Ret::kParseOk) {
            throw std::runtime_error("parse error!");
        }
        return json;
    }

    std::string dump() {
        char *str = stringify();
        std::string res(str);
        free(str);
        return res;
    }

private:
    Ret parse(const char *text) {
        clear();

        Ret ret = parse_text(text);
        // assert(stack_.size() == 0);

        if (ret != Ret::kParseOk)
            return ret;

        parse_whitespace(text);
        if (*text) {
            clear();
            return Ret::kParseRootNotSingular;
        }

        return ret;
    }

    char *stringify_boolean() {
        return value_.boolean ? str_dup(kLiteralTrue, 4)
                              : str_dup(kLiteralFalse, 5);
    }

    char *stringify() {
        switch (type_) {
            case Type::kNull:
                return str_dup(kLiteralNull, 4);
            case Type::kBoolean:
                return stringify_boolean();
            case Type::kNumber:
                return stringify_number();
            case Type::kString:
                return stringify_string();
            case Type::kArray:
                return stringify_array();
            case Type::kObject:
                return stringify_object();
            default:
                break;
        }
        return nullptr;
    }

    bool is_null() const {
        return type_ == Type::kNull;
    }

    bool is_bool(bool b) const {
        return type_ == Type::kBoolean;
    }
    double get_number() const {
        check_type(Type::kNumber, "number");
        return value_.number;
    }

    const char *get_string() const {
        check_type(Type::kString, "string");
        return value_.str->data();
    }

    size_t get_array_size() const {
        check_type(Type::kArray, "array");
        return value_.array->size();
    }

    Json &get_array_element(size_t idx) const {
        check_type(Type::kArray, "array");
        return (*value_.array)[idx];
    }

    size_t get_object_size() const {
        check_type(Type::kObject, "object");
        return value_.object->size();
    }

    size_t get_object_key_length(size_t idx) const {
        check_type(Type::kObject, "object");
        auto it = value_.object->begin();
        std::advance(it, idx);
        return it->first.size();
    }

    const char *get_object_key(size_t idx) const {
        check_type(Type::kObject, "object");
        auto it = value_.object->begin();
        std::advance(it, idx);
        return it->first.c_str();
    }

    Json &get_object_value(size_t idx) const {
        check_type(Type::kObject, "object");
        auto it = value_.object->begin();
        std::advance(it, idx);
        return it->second;
    }

    void stack_push(char ch) {
        stack_.push_back(ch);
    }

    void stack_push(const char *s) {
        while (*s)
            stack_.push_back(*s++);
    }

    Ret parse_text(const char *&text) {
        parse_whitespace(text);
        if (!*text)
            return Ret::kParseExpectValue;
        switch (*text) {
            case 'n':
                return parse_literal(text, kLiteralNull, Type::kNull);
            case 't':
                return parse_boolean(text, kLiteralTrue, true);
            case 'f':
                return parse_boolean(text, kLiteralFalse, false);
            case '\"':
                return parse_string(text);
            case '[':
                return parse_array(text);
            case '{':
                return parse_object(text);
            default:
                return parse_number(text);
        }
        return Ret::kParseInvalidValue;
    }

    void check_type(Type type, const char *msg) const {
        if (type_ != type) {
            std::string error_msg =
                std::string("text value isn't' ") + msg + "!";
            throw std::runtime_error(error_msg);
        }
    }

    /* ws = *(%x20 / %x09 / %x0A / %x0D) */
    void parse_whitespace(const char *&text) {
        while (*text == ' ' || *text == '\t' || *text == '\n' ||
               *text == '\r') {
            ++text;
        }
    }

    Ret parse_literal(const char *&text, std::string_view literal, Type type) {
        for (char c : literal) {
            if (*text++ != c)
                return Ret::kParseInvalidValue;
        }
        type_ = type;
        return Ret::kParseOk;
    }

    Ret parse_boolean(const char *&text, std::string_view literal, Boolean b) {
        for (char c : literal) {
            if (*text++ != c)
                return Ret::kParseInvalidValue;
        }
        type_ = Type::kBoolean;
        value_.boolean = b;
        return Ret::kParseOk;
    }

    Ret parse_number(const char *&text) {
        // TODO 暂时使用系统库的解析方式匹配测试用例
        // 检查
        const char *p = text;
        if (*p == '-')
            ++p;
        if (*p == '0') {
            ++p;
        } else if (isdigit(*p)) {
            ++p;
            while (isdigit(*p))
                ++p;
        } else {
            return Ret::kParseInvalidValue;
        }
        if (*p == '.') {
            ++p;
            if (!isdigit(*p))
                return Ret::kParseInvalidValue;
            ++p;
            while (isdigit(*p))
                ++p;
        }
        if (*p == 'e' || *p == 'E') {
            ++p;
            if (*p == '+' || *p == '-')
                ++p;
            if (!isdigit(*p))
                return Ret::kParseInvalidValue;
            ++p;
            while (isdigit(*p))
                ++p;
        }

        double number = strtod(text, nullptr);
        if (std::isinf(number))
            return Ret::kParseNumberTooBig;

        text = p;
        type_ = Type::kNumber;
        value_.number = number;
        return Ret::kParseOk;
    }

    static int ch2hex(char ch) {
        int hex = 0;
        if (ch >= '0' && ch <= '9') {
            hex = ch - '0';
        } else if (ch >= 'a' && ch <= 'f') {
            hex = ch - 'a' + 10;
        } else if (ch >= 'A' && ch <= 'F') {
            hex = ch - 'A' + 10;
        } else {
            return -1;
        }
        return hex;
    }

    static int parse_hex4(const char *&text) {
        int code = 0;
        for (int i = 0; i < 4; ++i) {
            int hex = ch2hex(*text++);
            if (hex < 0)
                return -1;
            code = (code << 4) + hex;
        }
        return code;
    }

    Ret encode_utf8(const char *&text) {
        int code = parse_hex4(text);
        if (code < 0)
            return Ret::kParseInvalidUnicodeHex;
        if (code >= 0xD800 && code < 0xDC00) {
            if (*text++ != '\\' || *text++ != 'u') {
                return Ret::kParseInvalidUnicodeSurrogate;
            }
            int surrogate = parse_hex4(text);
            if (surrogate < 0 || !(surrogate >= 0xDC00 && surrogate < 0xE000)) {
                return Ret::kParseInvalidUnicodeSurrogate;
            }
            code = 0x10000 + (code - 0xD800) * 0x400 + (surrogate - 0xDC00);
        }

        if (code < 0x80) {
            stack_push(code);
        } else if (code < 0x800) {
            stack_push(0xC0 | (0x1F & (code >> 6)));
            stack_push(0X80 | (0x3F & code));
        } else if (code < 0x10000) {
            stack_push(0xE0 | (0x0F & (code >> 12)));
            stack_push(0X80 | (0x3F & (code >> 6)));
            stack_push(0X80 | (0x3F & code));
        } else if (code < 0x10FFFF) {
            stack_push(0xF0 | (0x07 & (code >> 18)));
            stack_push(0X80 | (0x3F & (code >> 12)));
            stack_push(0X80 | (0x3F & (code >> 6)));
            stack_push(0X80 | (0x3F & code));
        } else {
            return Ret::kParseInvalidUnicodeHex;
        }
        return Ret::kParseOk;
    }

    Ret parse_string_raw(const char *&text) {
        ++text;
        while (*text && *text != '\"') {
            unsigned char ch = *text++;
            if (ch == '\\') {
                switch (*text++) {
                    case 'b':
                        stack_push('\b');
                        break;
                    case 'f':
                        stack_push('\f');
                        break;
                    case 'n':
                        stack_push('\n');
                        break;
                    case 'r':
                        stack_push('\r');
                        break;
                    case 't':
                        stack_push('\t');
                        break;
                    case '/':
                        stack_push('/');
                        break;
                    case '\"':
                        stack_push('\"');
                        break;
                    case '\\':
                        stack_push('\\');
                        break;
                    case 'u': {
                        Ret ret = encode_utf8(text);
                        if (ret != Ret::kParseOk)
                            return ret;
                    } break;
                    default:
                        return Ret::kParseInvalidStringEscape;
                }
            } else if (ch < 0x20) {
                return Ret::kParseInvalidStringChar;
            } else {
                stack_push(ch);
            }
        }
        if (*text++ != '\"')
            return Ret::kParseMissQuotationMark;
        return Ret::kParseOk;
    }

    Ret parse_string(const char *&text) {
        size_t old_top = stack_.size();
        Ret ret = parse_string_raw(text);
        if (ret != Ret::kParseOk)
            return ret;
        value_.str =
            new std::string(stack_.data() + old_top, stack_.size() - old_top);
        stack_.resize(old_top);
        type_ = Type::kString;
        return Ret::kParseOk;
    }

    Ret parse_array(const char *&text) {
        ++text;
        parse_whitespace(text);
        if (!*text) {
            return Ret::kParseMissCommaOrSquareBracket;
        } else if (*text == ']') {
            ++text;
            value_.array = new Array();
            type_ = Type::kArray;
            return Ret::kParseOk;
        }

        Array array;
        for (;;) {
            Json value;
            Ret ret = value.parse_text(text);
            if (ret != Ret::kParseOk)
                return ret;
            array.emplace_back(std::move(value));

            parse_whitespace(text);
            if (*text == ']') {
                ++text;
                break;
            } else if (*text != ',') {
                return Ret::kParseMissCommaOrSquareBracket;
            }
            ++text;
        }
        value_.array = new Array(std::move(array));
        type_ = Type::kArray;
        return Ret::kParseOk;
    }

    Ret parse_object(const char *&text) {
        ++text;
        parse_whitespace(text);
        if (!*text) {
            return Ret::kParseMissCommaOrCurlyBracket;
        } else if (*text == '}') {
            ++text;
            value_.object = new Object();
            type_ = Type::kObject;
            return Ret::kParseOk;
        }

        Object object;
        for (;;) {
            int old_top = stack_.size();
            if (*text != '\"' || parse_string_raw(text) != Ret::kParseOk) {
                return Ret::kParseMissKey;
            }
            parse_whitespace(text);
            if (*text++ != ':')
                return Ret::kParseMissColon;

            std::string key(stack_.data() + old_top, stack_.size() - old_top);
            stack_.resize(old_top);

            Json value;
            Ret ret = value.parse_text(text);
            if (ret != Ret::kParseOk)
                return ret;

            object.emplace(std::move(key), std::move(value));

            parse_whitespace(text);
            if (*text == '}') {
                ++text;
                break;
            } else if (*text != ',') {
                return Ret::kParseMissCommaOrCurlyBracket;
            }
            ++text;
            parse_whitespace(text);
        }

        value_.object = new Object(std::move(object));
        type_ = Type::kObject;
        return Ret::kParseOk;
    }

    char *str_dup(const char *str, size_t len) {
        char *text = (char *)malloc(len + 1);
        memcpy(text, str, len);
        text[len] = '\0';
        return text;
    }

    char *stringify_number() {
        char buf[32];
        int len = sprintf(buf, "%.17g", value_.number);
        return str_dup(buf, len);
    }

    void stringify_hex4(int code) {
        char buf[5];
        for (int i = 3; i >= 0; --i) {
            int x = code % 16;
            buf[i] = x < 10 ? x + '0' : x - 10 + 'A';
            code /= 16;
        }
        buf[4] = '\0';
        stack_push(buf);
    }

    int stringify_utf8(const char *str) {
        char ch = *str++;
        int count = 1;
        if ((ch & 0xF0) == 0xF0) {
            count = 4;
        } else if ((ch & 0xE0) == 0xE0) {
            count = 3;
        } else if ((ch & 0xC0) == 0xC0) {
            count = 2;
        }
        int code = ((ch << count) & 0xFF) >> count;
        for (int i = 1; i < count; ++i) {
            code = (code << 6) + (*str++ & 0x3F);
        }
        if (code < 0x10000) {
            stack_push("\\u");
            stringify_hex4(code);
        } else {
            code &= 0xFFFF;
            int H = code / 0x400 + 0xD800;
            int L = code % 0x400 + 0xDC00;
            stack_push("\\u");
            stringify_hex4(H);
            stack_push("\\u");
            stringify_hex4(L);
        }
        return count;
    }

    char *stringify_string_raw(const char *str, int len) {
        size_t old_top = stack_.size();
        stack_push("\"");
        int pos = 0;
        while (pos < len) {
            switch (str[pos]) {
                case '\b':
                    stack_push("\\b");
                    break;
                case '\f':
                    stack_push("\\f");
                    break;
                case '\n':
                    stack_push("\\n");
                    break;
                case '\r':
                    stack_push("\\r");
                    break;
                case '\t':
                    stack_push("\\t");
                    break;
                case '/':
                    stack_push("/");
                    break;
                case '\"':
                    stack_push("\\\"");
                    break;
                case '\\':
                    stack_push("\\\\");
                    break;
                default: {
                    if (str[pos] < 0x20) {
                        pos += stringify_utf8(str + pos);
                        --pos;
                    } else {
                        stack_push(str[pos]);
                    }
                } break;
            }
            ++pos;
        }
        stack_push("\"");
        char *text = str_dup(stack_.data() + old_top, stack_.size() - old_top);
        stack_.resize(old_top);
        return text;
    }

    char *stringify_string() {
        return stringify_string_raw(value_.str->data(), value_.str->size());
    }

    char *stringify_array() {
        size_t old_top = stack_.size();
        stack_push('[');
        auto &array = *value_.array;
        for (size_t i = 0; i < array.size(); ++i) {
            char *subtext = array[i].stringify();
            stack_push(subtext);
            free(subtext);
            if (i != array.size() - 1)
                stack_push(',');
        }
        stack_push(']');
        char *text = str_dup(stack_.data() + old_top, stack_.size() - old_top);
        stack_.resize(old_top);
        return text;
    }

    char *stringify_object() {
        size_t old_top = stack_.size();
        stack_push('{');
        int cnt = 0, sz = value_.object->size();
        for (auto &[key, json] : *value_.object) {
            char *ktext = stringify_string_raw(key.c_str(), key.size());
            char *vtext = json.stringify();
            stack_push(ktext);
            stack_push(':');
            stack_push(vtext);
            free(ktext);
            free(vtext);
            if (++cnt < sz)
                stack_push(',');
        }
        stack_push('}');
        char *text = str_dup(stack_.data() + old_top, stack_.size() - old_top);
        stack_.resize(old_top);
        return text;
    }

private:
    Type type_;

    // union {
    //     double number;
    //     std::string *str;
    //     Array *array;
    //     Object *object;
    //     // std::multimap<std::string, Json> *object;
    // } value_;

    Value value_;

    mutable std::vector<char> stack_;
};

}  // namespace zjson

#endif  // ZJSON_H