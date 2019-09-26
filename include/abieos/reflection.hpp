#pragma once

// TODO: for_each_field -> for_each_member
// TODO: rename ABIEOS_*
// TODO: namespace

#include <type_traits>

template <auto P>
struct member_ptr;

template <class C, typename M>
const C* class_from_void(M C::*, const void* v) {
    return reinterpret_cast<const C*>(v);
}

template <class C, typename M>
C* class_from_void(M C::*, void* v) {
    return reinterpret_cast<C*>(v);
}

template <auto P>
auto& member_from_void(const member_ptr<P>&, const void* p) {
    return class_from_void(P, p)->*P;
}

template <auto P>
auto& member_from_void(const member_ptr<P>&, void* p) {
    return class_from_void(P, p)->*P;
}

template <auto P>
struct member_ptr {
    using member_type = std::decay_t<decltype(member_from_void(std::declval<member_ptr<P>>(), std::declval<void*>()))>;
    static constexpr member_type* null = nullptr;
};

template <typename T>
struct has_for_each_field {
  private:
    struct F {
        template <typename A, typename B>
        void operator()(const A&, const B&);
    };

    template <typename C>
    static char test(decltype(for_each_field((C*)nullptr, F{}))*);

    template <typename C>
    static long test(...);

  public:
    static constexpr bool value = sizeof(test<T>((void*)nullptr)) == sizeof(char);
};

template <typename T>
inline constexpr bool has_for_each_field_v<T> = has_for_each_field<T>::value;

#define ABIEOS_REFLECT(STRUCT)                                                                                         \
    inline const char* get_type_name(STRUCT*) { return #STRUCT; }                                                      \
    template <typename F>                                                                                              \
    void for_each_field(STRUCT*, F f)

#define ABIEOS_MEMBER(STRUCT, MEMBER) f(#MEMBER, member_ptr<&STRUCT::MEMBER>{});

#define ABIEOS_BASE(BASE) for_each_field((BASE*)nullptr, f);
