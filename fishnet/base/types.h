#pragma once

#include <stdint.h>
#include <cstring>
#include <string>

#ifndef NDEBUG
#include <cassert>
#endif

namespace fishnet {
using std::string;

inline void memZero(void* p, size_t n) { memset(p, 0, n); }

// 作者：kenton\@google.com (Kenton Varda) 和其他
// 包含库其余部分使用的基本类型和实用程序。使用implicit_cast 作为 static_cast 或
// const_cast 的安全版本，用于在类型层次结构中向上转换（即将指向 Foo
// 的指针转换为指向 SuperclassOfFoo 的指针或将指向 Foo 的指针转换为指向 Foo 的
// const 指针）。当您使用implicit_cast 时，编译器会检查强制转换是否安全。在 C++
// 需要精确类型匹配而不是可转换为目标类型的参数类型的许多情况下，这种显式的implicit_casts
// 是必要的。可以推断出 From 类型，因此使用 implicit_cast 的首选语法与
// static_cast 等相同：implicit_cast<ToType>(expr)implicit_cast 本来是 C++
// 标准库的一部分，但提案提交得太晚了。它可能会在未来进入该语言。
template <typename To, typename From>
inline To implicit_cast(From const& f) {
    return f;
}

// When you upcast (that is, cast a pointer from type Foo to type
// SuperclassOfFoo), it's fine to use implicit_cast<>, since upcasts
// always succeed.  When you downcast (that is, cast a pointer from
// type Foo to type SubclassOfFoo), static_cast<> isn't safe, because
// how do you know the pointer is really of type SubclassOfFoo?  It
// could be a bare Foo, or of type DifferentSubclassOfFoo.  Thus,
// when you downcast, you should use this macro.  In debug mode, we
// use dynamic_cast<> to double-check the downcast is legal (we die
// if it's not).  In normal mode, we do the efficient static_cast<>
// instead.  Thus, it's important to test in debug mode to make sure
// the cast is legal!
//    This is the only place in the code we should use dynamic_cast<>.
// In particular, you SHOULDN'T be using dynamic_cast<> in order to
// do RTTI (eg code like this:
//    if (dynamic_cast<Subclass1>(foo)) HandleASubclass1Object(foo);
//    if (dynamic_cast<Subclass2>(foo)) HandleASubclass2Object(foo);
// You should design the code some other way not to need this.
template <typename To, typename From>
inline To down_cast(From& f) {
    // 确保 To 是 From * 的子类型。
    // 这个测试只在这里用于编译时类型检查，并且在在运行时优化构建，因为它将被完全地优化掉。
    if (false) {
        implicit_cast<From*, To>(0);
    }
#if !defined(NDEBUG) && !defined(GOOGLE_PROTOBUF_NO_RTTI)
    assert(f == NULL || dynamic_cast<To>(f) != NULL);
#endif
    return static_cast<To>(f);
}

}  // namespace fishnet