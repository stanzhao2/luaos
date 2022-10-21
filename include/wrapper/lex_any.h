/************************************************************************************
**
** Copyright 2021 Shaoguang
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to deal
** in the Software without restriction, including without limitation the rights
** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
** copies of the Software, and to permit persons to whom the Software is furnished
** to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in all
** copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
** OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
** SOFTWARE.
**
************************************************************************************/

#ifndef __ANY_H_
#define __ANY_H_

#ifdef _MSVC_LANG
# define CPLUSPLUS__ _MSVC_LANG
#else
# define CPLUSPLUS__ __cplusplus
#endif /* _MSVC_LANG */

#if CPLUSPLUS__ > 201402L
# if (defined(_MSC_VER) && _MSC_VER >= 1910) || (defined(__GNUC__) && __GNUC__ >= 7) || (defined(__clang__) && __clang_major__ >= 4)
# define HAS_STDANY__ /* std::any */
# endif
#endif

#if defined(HAS_STDANY__)
# include <any>
#else

#include <new>
#include <typeinfo>
#include <utility>

namespace detail_any {

template <bool B>
using _bool_constant = std::integral_constant<bool, B>;

// _or

template<typename...>
struct _or;

template<>
struct _or<>
    : public std::false_type
{ };

template<typename _B1>
struct _or<_B1>
    : public _B1
{ };

template<typename _B1, typename _B2>
struct _or<_B1, _B2>
    : public std::conditional<_B1::value, _B1, _B2>::type
{ };

template<typename _B1, typename _B2, typename _B3, typename... _Bn>
struct _or<_B1, _B2, _B3, _Bn...>
    : public std::conditional<_B1::value, _B1, _or<_B2, _B3, _Bn...>>::type
{ };

// _and

template<typename...>
struct _and;

template<>
struct _and<>
    : public std::true_type
{ };

template<typename _B1>
struct _and<_B1>
    : public _B1
{ };

template<typename _B1, typename _B2>
struct _and<_B1, _B2>
    : public std::conditional<_B1::value, _B2, _B1>::type
{ };

template<typename _B1, typename _B2, typename _B3, typename... _Bn>
struct _and<_B1, _B2, _B3, _Bn...>
    : public std::conditional<_B1::value, _and<_B2, _B3, _Bn...>, _B1>::type
{ };

// _and_v

template<typename... _Bn>
constexpr bool _and_v() noexcept
{
    return _and<_Bn...>::value;
}

// _not

template<typename _Pp>
struct _not
    : public _bool_constant<!bool(_Pp::value)>
{ };

// _in_place_type_t

template<typename _Tp>
struct _in_place_type_t
{
    explicit _in_place_type_t() = default;
};

// _is_in_place_type

template<typename>
struct _is_in_place_type_impl : std::false_type
{ };

template<typename _Tp>
struct _is_in_place_type_impl<_in_place_type_t<_Tp>> : std::true_type
{ };

template<typename _Tp>
struct _is_in_place_type
    : public _is_in_place_type_impl<_Tp>
{ };

} // namespace detail_any

/**
 * \class bad_any_cast
 * \brief Exception thrown by the value-returning forms of any_cast on a type mismatch.
 *
 * Defines a type of object to be thrown by the value-returning forms of any_cast on failure.
 */
class bad_any_cast : public std::bad_cast
{
public:
    /**
     * Returns an explanatory string.
     */
    virtual const char* what() const noexcept {
        return "bad any_cast";
    }
};

[[noreturn]] inline void throw_bad_any_cast()
{
    throw bad_any_cast{};
}

/**
 * \class any
 * \brief The class any describes a type-safe container for single values of any type.
 *
 * An object of class any stores an instance of any type that satisfies the constructor requirements or is empty,
 * and this is referred to as the state of the class any object. The stored instance is called the contained object.
 * Two states are equivalent if they are either both empty or if both are not empty and if the contained objects are equivalent.
 *
 * The non-member any_cast functions provide type-safe access to the contained object.
 *
 * \sa make_any(), any_cast()
 */
namespace std {

class any final
{
    // Holds either pointer to a heap object or the contained object itself.
    union Storage
    {
        constexpr Storage()
            : m_ptr(nullptr) {}

        // Prevent trivial copies of this type, buffer might hold a non-POD.
        Storage(const Storage&) = delete;
        Storage& operator=(const Storage&) = delete;

        void* m_ptr;
        std::aligned_storage<sizeof(void*), alignof(void*)>::type m_buffer;
    };

    template<typename _Tp, typename _Safe = std::is_nothrow_move_constructible<_Tp>,
        bool _Fits = (sizeof(_Tp) <= sizeof(Storage)) && (alignof(_Tp) <= alignof(Storage))>
        using Internal = std::integral_constant<bool, _Safe::value && _Fits>;

    template<typename _Tp>
    struct Manager_internal; // uses small-object optimization

    template<typename _Tp>
    struct Manager_external; // creates contained object on the heap

    template<typename _Tp>
    using Manager = typename std::conditional<Internal<_Tp>::value,
        Manager_internal<_Tp>, Manager_external<_Tp>>::type;

    template<typename _Tp, typename _Decayed = typename std::decay<_Tp>::type>
    using Decay = typename std::enable_if<!std::is_same<_Decayed, any>::value, _Decayed>::type;

    /** Emplace with an object created from \a args as the contained object. */
    template <typename _Tp, typename... _Args, typename Mgr = Manager<_Tp>>
    void _do_emplace(_Args&&... args)
    {
        reset();
        Mgr::S_create(m_storage, std::forward<_Args>(args)...);
        m_manager = &Mgr::S_manage;
    }

    /** Emplace with an object created from \a list and \a list as the contained object. */
    template <typename _Tp, typename _Up, typename... _Args,
        typename Mgr = Manager<_Tp>>
        void _do_emplace(std::initializer_list<_Up> list, _Args&&... args)
    {
        reset();
        Mgr::S_create(m_storage, list, std::forward<_Args>(args)...);
        m_manager = &Mgr::S_manage;
    }

public:
    //
    //  construct/destruct
    //

    /**
     * Default constructor, creates an empty object.
     *
     * \note Because the default constructor is constexpr, static anys are initialized as part
     * of [static non-local initialization](https://en.cppreference.com/w/cpp/language/initialization),
     * before any dynamic non-local initialization begins. This makes it safe to use an object of
     * type any in a constructor of any static object.
     */
    constexpr any() noexcept
        : m_manager(nullptr) {
    }

    /**
     * Copies content of \a other into a new instance, so that any content is equivalent in both type
     * and value to those of \a other prior to the constructor call, or empty if \a other is empty.
     *
     * Formally, If \a other is empty, the constructed object is empty. Otherwise, equivalent
     * to <code>any(std::in_place_type<T>, std::any_cast<const T&>(other))</code>,
     * where \c T is the type of the object contained in \a other.
     *
     * \exception Throws any exception thrown by the constructor of the contained type.
     */
    any(const any& other) {
        if (!other.has_value()) {
            m_manager = nullptr;
        } else {
            ManageArg marg;
            marg._any = this;
            other.m_manager(MOClone, &other, &marg);
        }
    }

    /**
     * Moves content of \a other into a new instance, so that any content is equivalent in both type
     * and value to those of \a other prior to the constructor call, or empty if \a other is empty.
     *
     * Formally, If \a other is empty, the constructed object is empty. Otherwise, the constructed
     * object contains either the object contained in other, or an object of the same type constructed
     * from the object contained in other, considering that object as an rvalue.
     */
    any(any&& other) noexcept {
        if (!other.has_value()) {
            m_manager = nullptr;
        } else {
            ManageArg marg;
            marg._any = this;
            other.m_manager(MOXfer, &other, &marg);
        }
    }

    template <typename _Res, typename _Tp, typename... _Args>
    using _any_constructible = std::enable_if<detail_any::_and<std::is_copy_constructible<_Tp>,
        std::is_constructible<_Tp, _Args...>>::value, _Res>;

    template <typename _Tp, typename... _Args>
    using _any_constructible_t = typename _any_constructible<bool, _Tp, _Args...>::type;

    /**
     * Constructs an object with initial content an object of type \c std::decay_t<_ValueType> ,
     * [direct-initialized](https://en.cppreference.com/w/cpp/language/direct_initialization) from
     * std::forward<_ValueType>(value). If std::is_copy_constructible<std::decay_t<_ValueType>>::value
     * is false, the program is ill-formed.
     *
     * - This overload only participates in overload resolution if \c std::decay_t<_ValueType> is not
     * the same type as any nor a specialization of \c std::in_place_type_t ,
     * and \c std::is_copy_constructible_v<std::decay_t<_ValueType>> is true.
     *
     * \exception Throws any exception thrown by the constructor of the contained type.
     */
    template <typename _ValueType, typename _Tp = Decay<_ValueType>,
        typename Mgr = Manager<_Tp>,
        _any_constructible_t<_Tp, _ValueType&&> = true,
        typename std::enable_if<!detail_any::_is_in_place_type<_Tp>::value, bool>::type = true>
        any(_ValueType&& value)
        : m_manager(&Mgr::S_manage)
    {
        Mgr::S_create(m_storage, std::forward<_ValueType>(value));
    }

    /**
     * Constructs an object with initial content an object of type \c std::decay_t<_ValueType> ,
     * [direct-initialized](https://en.cppreference.com/w/cpp/language/direct_initialization) from
     * std::forward<_ValueType>(value). If std::is_copy_constructible<std::decay_t<_ValueType>>::value
     * is false, the program is ill-formed.
     *
     * - This overload only participates in overload resolution if \c std::decay_t<_ValueType> is not
     * the same type as any nor a specialization of \c std::in_place_type_t ,
     * and \c std::is_copy_constructible_v<std::decay_t<_ValueType>> is true.
     *
     * \exception Throws any exception thrown by the constructor of the contained type.
     */
    template <typename _ValueType, typename _Tp = Decay<_ValueType>,
        typename Mgr = Manager<_Tp>,
        typename std::enable_if<detail_any::_and_v<std::is_copy_constructible<_Tp>,
        detail_any::_not<std::is_constructible<_Tp, _ValueType&&>>,
        detail_any::_not<detail_any::_is_in_place_type<_Tp>>>(), bool>::value = false>
        any(_ValueType&& value)
        : m_manager(&Mgr::S_manage)
    {
        Mgr::S_create(m_storage, value);
    }

    /**
     * Constructs an object with initial content an object of type \c std::decay_t<_ValueType> ,
     * [direct-non-list-initialized](https://en.cppreference.com/w/cpp/language/direct_initialization) from
     * \c std::forward<Args>(args)....
     *
     * - This overload only participates in overload resolution if
     * <code>std::is_constructible_v<std::decay_t<_ValueType>, Args...></code> and
     * \c std::is_copy_constructible_v<std::decay_t<_ValueType>> are both true.
     *
     * \exception Throws any exception thrown by the constructor of the contained type.
     */
    template <typename _ValueType, typename... _Args,
        typename _Tp = Decay<_ValueType>,
        typename Mgr = Manager<_Tp>,
        _any_constructible_t<_Tp, _Args&& ... > = false>
        explicit any(detail_any::_in_place_type_t<_ValueType>, _Args&&... args)
        : m_manager(&Mgr::S_manage)
    {
        Mgr::S_create(m_storage, std::forward<_Args>(args)...);
    }

    /**
     * Construct with an object created from \a list and \a args as the contained object.
     *
     * Constructs an object with initial content an object of type std::decay_t<_ValueType>,
     * [direct-non-list-initialized](https://en.cppreference.com/w/cpp/language/direct_initialization) from
     * \a list, </code>std::forward<_Args>(args)....</code>
     *
     * - This overload participates in overload resolution only if <code>std::is_constructible_v<std::decay_t<_ValueType></code>,
     * <code>std::initializer_list<_Up>&, _Args...></code> and
     * <code>std::is_copy_constructible_v<std::decay_t<_ValueType>></code> are both true.
     *
     * \exception Throws any exception thrown by the constructor of the contained type.
     */
    template <typename _ValueType, typename _Up, typename... _Args,
        typename _Tp = Decay<_ValueType>, typename Mgr = Manager<_Tp>,
        _any_constructible_t<_Tp, std::initializer_list<_Up>, _Args&&...> = false>
        explicit any(detail_any::_in_place_type_t<_ValueType>, std::initializer_list<_Up> list, _Args&&... args)
        : m_manager(&Mgr::S_manage)
    {
        Mgr::S_create(m_storage, list, std::forward<_Args>(args)...);
    }

    /**
     * Destroys the contained object, if any, as if by a call to reset().
     */
    ~any() {
        reset();
    }

    //
    //  assignments
    //

    /**
     *  Assigns by copying the state of \a rhs, as if by any(rhs).swap(*this).
     *
     *  \exception bad_alloc or any exception thrown by the constructor of the contained type.
     *  If an exception is thrown, there are no effects (strong exception guarantee).
     */
    any& operator=(const any& rhs) {
        *this = any(rhs);
        return *this;
    }

    /**
     * Assigns by moving the state of \a rhs, as if by any(std::move(rhs)).swap(*this).
     * \a rhs is left in a valid but unspecified state after the assignment.
     */
    any& operator=(any&& rhs) noexcept {
        if (!rhs.has_value()) {
            reset();
        } else if (this != &rhs) {
            reset();
            ManageArg marg;
            marg._any = this;
            rhs.m_manager(MOXfer, &rhs, &marg);
        }
        return *this;
    }

    /**
     * Assigns the type and value of rhs, as if by <code>any(std::forward<_ValueType>(rhs)).swap(*this)</code>.
     * This overload participates in overload resolution only if \c std::decay_t<_ValueType> is not the same
     * type as any and <code>std::is_copy_constructible_v<std::decay_t<_ValueType>></code> is true.
     *
     * \exception bad_alloc or any exception thrown by the constructor of the contained type.
     * If an exception is thrown, there are no effects (strong exception guarantee).
     *
     * \note \c std::decay_t<_ValueType> must meet the requirements of
     * [CopyConstructible](https://en.cppreference.com/w/cpp/named_req/CopyConstructible).
     */
    template<typename _ValueType>
    typename std::enable_if<std::is_copy_constructible<Decay<_ValueType>>::value, any&>::type
        operator=(_ValueType&& rhs)
    {
        *this = any(std::forward<_ValueType>(rhs));
        return *this;
    }

    /**
     * Changes the contained object to one of type \c std::decay_t<_ValueType> constructed from the arguments.
     * First destroys the current contained object (if any) by reset(), then:
     * constructs an object of type std::decay_t<_ValueType>,
     * [direct-non-list-initialized](https://en.cppreference.com/w/cpp/language/direct_initialization) from
     * <code>std::forward<Args>(args)...</code>, as the contained object.
     *
     * - This overload only participates in overload resolution if
     * <code>std::is_constructible_v<std::decay_t<_ValueType>, _Args...></code> and
     * <code>std::is_copy_constructible_v<std::decay_t<_ValueType>></code> are both true.
     *
     * \exception Throws any exception thrown by T's constructor. If an exception is thrown,
     * the previously contained object (if any) has been destroyed, and *this does not contain a value.
     *
     * \note \c std::decay_t<_ValueType> must meet the requirements of
     * [CopyConstructible](https://en.cppreference.com/w/cpp/named_req/CopyConstructible).
     */
    template <typename _ValueType, typename... _Args>
    typename _any_constructible<Decay<_ValueType>&, Decay<_ValueType>, _Args&&...>::type
        emplace(_Args&&... args)
    {
        _do_emplace<Decay<_ValueType>>(std::forward<_Args>(args)...);
        any::ManageArg marg;
        this->m_manager(any::MOAccess, this, &marg);
        return *static_cast<Decay<_ValueType>*>(marg._obj);
    }

    /**
     * Changes the contained object to one of type \c std::decay_t<_ValueType> constructed from the arguments.
     * First destroys the current contained object (if any) by reset(), then:
     * constructs an object of type <code>std::decay_t<_ValueType></code>,
     * [direct-non-list-initialized](https://en.cppreference.com/w/cpp/language/direct_initialization) from
     * \a list, <code>std::forward<_Args>(args)...</code>, as the contained object.
     *
     * - This overload only participates in overload resolution if
     * <code>std::is_constructible_v<std::decay_t<_ValueType>, std::initializer_list<_Up>&, _Args...></code> and
     * <code>std::is_copy_constructible_v<std::decay_t<_ValueType>></code> are both true.
     *
     * \exception Throws any exception thrown by T's constructor. If an exception is thrown,
     * the previously contained object (if any) has been destroyed, and *this does not contain a value.
     *
     * \note \c std::decay_t<_ValueType> must meet the requirements of
     * [CopyConstructible](https://en.cppreference.com/w/cpp/named_req/CopyConstructible).
     */
    template <typename _ValueType, typename _Up, typename... _Args>
    typename _any_constructible<Decay<_ValueType>&, Decay<_ValueType>,
        std::initializer_list<_Up>, _Args&&...>::type
        emplace(std::initializer_list<_Up> list, _Args&&... args)
    {
        _do_emplace<Decay<_ValueType>, _Up>(list, std::forward<_Args>(args)...);
        any::ManageArg marg;
        this->m_manager(any::MOAccess, this, &marg);
        return *static_cast<Decay<_ValueType>*>(marg._obj);
    }

    //
    //  modifiers
    //

    /**
     * If not empty, destroy the contained object.
     */
    void reset() noexcept {
        if (has_value()) {
            m_manager(MODestroy, this, nullptr);
            m_manager = nullptr;
        }
    }

    /**
     * Swaps the content of two any objects.
     */
    void swap(any& rhs) noexcept {
        if (!has_value() && !rhs.has_value()) {
            return;
        }

        if (has_value() && rhs.has_value()) {
            if (this == &rhs) {
                return;
            }

            any tmp;
            ManageArg marg;
            marg._any = &tmp;
            rhs.m_manager(MOXfer, &rhs, &marg);
            marg._any = &rhs;
            m_manager(MOXfer, this, &marg);
            marg._any = this;
            tmp.m_manager(MOXfer, &tmp, &marg);
        }
        else {
            any* empty = !has_value() ? this : &rhs;
            any* full = !has_value() ? &rhs : this;
            ManageArg marg;
            marg._any = empty;
            full->m_manager(MOXfer, full, &marg);
        }
    }

    //
    //  observers
    //

    /**
     * Checks whether the object contains a value.
     *
     * \return \c true if instance contains a value, otherwise \c false. 
     */
    bool has_value() const noexcept {
        return m_manager != nullptr;
    }

#if __cpp_rtti
    /**
     * Returns the typeid of the contained value.
     *
     * \return The typeid of the contained value if instance is non-empty, otherwise \c typeid(void).
     */
    const std::type_info& type() const noexcept {
        if (!has_value()) {
            return typeid(void);
        }
        ManageArg marg;
        m_manager(MOGetTypeInfo, this, &marg);
        return *marg._typeinfo;
    }
#endif

    template<typename _Tp>
    static constexpr bool _is_valid_cast()
    {
        return detail_any::_or<std::is_reference<_Tp>, std::is_copy_constructible<_Tp>>::value;
    }

private:
    enum ManageOption {
        MOAccess,
        MOGetTypeInfo,
        MOClone,
        MODestroy,
        MOXfer
    };

    union ManageArg
    {
        void* _obj;
        const std::type_info* _typeinfo;
        any* _any;
    };

    void(*m_manager)(ManageOption, const any*, ManageArg*);
    Storage m_storage;

    template<typename _Tp>
    friend void* _any_caster(const any* __any);

    // Manage in-place contained object.
    template<typename _Tp>
    struct Manager_internal
    {
        static void S_manage(ManageOption which, const any* anyp, ManageArg* arg);

        template<typename _Up>
        static void S_create(Storage& storage, _Up&& value)
        {
            void* addr = &storage.m_buffer;
            ::new (addr) _Tp(std::forward<_Up>(value));
        }

        template<typename... _Args>
        static void S_create(Storage& storage, _Args&&... args)
        {
            void* addr = &storage.m_buffer;
            ::new (addr) _Tp(std::forward<_Args>(args)...);
        }
    };

    // Manage external contained object.
    template<typename _Tp>
    struct Manager_external
    {
        static void S_manage(ManageOption __which, const any* __anyp, ManageArg* __arg);

        template<typename _Up>
        static void S_create(Storage& storage, _Up&& value)
        {
            storage.m_ptr = new _Tp(std::forward<_Up>(value));
        }
        template<typename... _Args>
        static void S_create(Storage& storage, _Args&&... __args)
        {
            storage.m_ptr = new _Tp(std::forward<_Args>(__args)...);
        }
    };
};

/**
 * Swaps the content of two any objects by calling <code> lhs.swap(rhs) </code>.
 */
inline void swap(any& lhs, any& rhs) noexcept
{
    lhs.swap(rhs);
}

/**
 * Constructs an any object containing an object of type _Tp, passing the provided arguments to _Tp's constructor.
 *
 * Equivalent to return <code> any(std::in_place_type<_Tp>, std::forward<_Args>(args)...); </code>
 */
template <typename _Tp, typename... _Args>
any make_any(_Args&&... args)
{
    constexpr detail_any::_in_place_type_t<_Tp> __in_place_type;
    return any(__in_place_type, std::forward<_Args>(args)...);
}

/**
 * Constructs an any object containing an object of type _Tp, passing the provided arguments to _Tp's constructor.
 *
 * Equivalent to <code> return std::any(std::in_place_type<_Tp>, list, std::forward<_Args>(args)...); </code>

 */
template <typename _Tp, typename _Up, typename... _Args>
any make_any(std::initializer_list<_Up> list, _Args&&... args)
{
    return any(detail_any::_in_place_type_t<_Tp>(), list, std::forward<_Args>(args)...);
}

/**
 * Performs type-safe access to the contained object.
 *
 * Let U be <code> std::remove_cv_t<std::remove_reference_t<_ValueType>> </code>.
 *
 * The program is ill-formed if <code>std::is_constructible_v<_ValueType, const U&></code> is false.
 *
 * \exception bad_any_cast if the typeid of the requested _ValueType does not match that of the contents of \a operand.
 *
 * \return static_cast<_ValueType>(*any_cast<U>(&operand))
 */
template<typename _ValueType>
inline _ValueType any_cast(const any& operand)
{
    using _Up = typename std::remove_cv<_ValueType>::type;
    static_assert(any::_is_valid_cast<_ValueType>(),
        "Template argument must be a reference or CopyConstructible type");
    static_assert(std::is_constructible<_ValueType, const _Up&>::value,
        "Template argument must be constructible from a const value.");
    auto resultp = any_cast<_Up>(&operand);
    if (resultp) {
        return static_cast<_ValueType>(*resultp);
    }
    throw_bad_any_cast();
}

/**
 * Performs type-safe access to the contained object.
 *
 * Let U be <code> std::remove_cv_t<std::remove_reference_t<_ValueType>> </code>.
 *
 * The program is ill-formed if <code>std::is_constructible_v<_ValueType, U&></code> is false.
 *
 * \exception bad_any_cast if the typeid of the requested _ValueType does not match that of the contents of \a operand.
 *
 * \return static_cast<_ValueType>(*any_cast<U>(&operand))
 */
template<typename _ValueType>
inline _ValueType any_cast(any& operand)
{
    using _Up = typename std::remove_cv<_ValueType>::type;
    static_assert(any::_is_valid_cast<_ValueType>(),
        "Template argument must be a reference or CopyConstructible type");
    static_assert(std::is_constructible<_ValueType, _Up&>::value,
        "Template argument must be constructible from an lvalue.");
    auto resultp = any_cast<_Up>(&operand);
    if (resultp) {
        return static_cast<_ValueType>(*resultp);
    }
    throw_bad_any_cast();
}

/**
 * Performs type-safe access to the contained object.
 *
 * Let U be <code> std::remove_cv_t<std::remove_reference_t<_ValueType>> </code>.
 *
 * The program is ill-formed if <code>std::is_constructible_v<_ValueType, U></code> is false.
 *
 * \exception bad_any_cast if the typeid of the requested _ValueType does not match that of the contents of \a operand.
 *
 * \return static_cast<_ValueType>(std::move(*any_cast<U>(&operand))).

 */
template<typename _ValueType>
inline _ValueType any_cast(any&& operand)
{
    using _Up = typename std::remove_cv<_ValueType>::type;
    static_assert(any::_is_valid_cast<_ValueType>(),
        "Template argument must be a reference or CopyConstructible type");
    static_assert(std::is_constructible<_ValueType, _Up>::value,
        "Template argument must be constructible from an rvalue.");
    auto resultp = any_cast<_Up>(&operand);
    if (resultp) {
        return static_cast<_ValueType>(std::move(*resultp));
    }
    throw_bad_any_cast();
}

/// \cond undocumented
template<typename _Tp>
void* _any_caster(const any* a)
{
    // any_cast<T> returns non-null if __any->type() == typeid(T) and
    // typeid(T) ignores cv-qualifiers so remove them:
    using _Up = typename std::remove_cv<_Tp>::type;
    // The contained value has a decayed type, so if decay_t<U> is not U,
    // then it's not possible to have a contained value of type U:
    if (!std::is_same<typename std::decay<_Up>::type, _Up>::value) {
        return nullptr;
    }
    // Only copy constructible types can be used for contained values:
    else if (!std::is_copy_constructible<_Up>::value) {
        return nullptr;
    }
    // First try comparing function addresses, which works without RTTI
    else if (a->m_manager == &any::Manager<_Up>::S_manage
#if __cpp_rtti
        || a->type() == typeid(_Tp)
#endif
        )
    {
        any::ManageArg marg;
        a->m_manager(any::MOAccess, a, &marg);
        return marg._obj;
    }
    return nullptr;
}
/// @endcond

/**
 * Performs type-safe access to the contained object.
 *
 * \return If operand is not a null pointer, and the typeid of the requested \c _ValueType matches that
 * of the contents of \a operand, a pointer to the value contained by \a operand, otherwise a null pointer.
 */
template<typename _ValueType>
inline const _ValueType* any_cast(const any* operand) noexcept
{
    if (std::is_object<_ValueType>::value) {
        if (operand) {
            return static_cast<_ValueType*>(_any_caster<_ValueType>(operand));
        }
    }
    return nullptr;
}

/**
 * Performs type-safe access to the contained object.
 *
 * \return If operand is not a null pointer, and the typeid of the requested \c _ValueType matches that
 * of the contents of \a operand, a pointer to the value contained by \a operand, otherwise a null pointer.
 */
template<typename _ValueType>
inline _ValueType* any_cast(any* operand) noexcept
{
    if (std::is_object<_ValueType>::value) {
        if (operand) {
            return static_cast<_ValueType*>(_any_caster<_ValueType>(operand));
        }
    }
    return nullptr;
}
/** @} */

template<typename _Tp>
void any::Manager_internal<_Tp>::S_manage(ManageOption which, const any* a, ManageArg* arg)
{
    // The contained object is in _M_storage._M_buffer
    auto ptr = reinterpret_cast<const _Tp*>(&a->m_storage.m_buffer);
    switch (which)
    {
    case MOAccess:
        arg->_obj = const_cast<_Tp*>(ptr);
        break;
    case MOGetTypeInfo:
#if __cpp_rtti
        arg->_typeinfo = &typeid(_Tp);
#endif
        break;
    case MOClone:
        ::new(&arg->_any->m_storage.m_buffer) _Tp(*ptr);
        arg->_any->m_manager = a->m_manager;
        break;
    case MODestroy:
        ptr->~_Tp();
        break;
    case MOXfer:
        ::new(&arg->_any->m_storage.m_buffer) _Tp
        (std::move(*const_cast<_Tp*>(ptr)));
        ptr->~_Tp();
        arg->_any->m_manager = a->m_manager;
        const_cast<any*>(a)->m_manager = nullptr;
        break;
    }
}

template<typename _Tp>
void any::Manager_external<_Tp>::S_manage(ManageOption which, const any* a, ManageArg* arg)
{
    // The contained object is *_M_storage._M_ptr
    auto ptr = static_cast<const _Tp*>(a->m_storage.m_ptr);
    switch (which)
    {
    case MOAccess:
        arg->_obj = const_cast<_Tp*>(ptr);
        break;
    case MOGetTypeInfo:
#if __cpp_rtti
        arg->_typeinfo = &typeid(_Tp);
#endif
        break;
    case MOClone:
        arg->_any->m_storage.m_ptr = new _Tp(*ptr);
        arg->_any->m_manager = a->m_manager;
        break;
    case MODestroy:
        delete ptr;
        break;
    case MOXfer:
        arg->_any->m_storage.m_ptr = a->m_storage.m_ptr;
        arg->_any->m_manager = a->m_manager;
        const_cast<any*>(a)->m_manager = nullptr;
        break;
    }
}

} // namespace std

#endif //HAS_STDANY__

#endif //__ANY_H_
