#pragma once

#include <functional>

/********************************** function_helper **********************************/
// functor helper
template <typename R, typename... Args>
struct function_helper;

// 0 args specialization
template <typename R>
struct function_helper<R>
{
    using function_type = std::function<R(void)>;
};

// 1+ arg specialization
template <typename R, typename Arg, typename... Rest>
struct function_helper<R, Arg, Rest...>
{
    using function_type = std::function<R(Arg, Rest...)>;

    using rest_helper = function_helper<R, Rest...>;

    static typename rest_helper::function_type bind(const function_type& f, Arg arg) {
        return [arg, f](Rest... rest) -> R { return f(arg, rest...); };
    }

    template <typename Arg0, typename Callable>
    static typename rest_helper::function_type bind(const Callable& callable, Arg0 arg0, Arg arg) {
        return [arg0, arg, callable](Rest... rest) -> R { return callable(arg0, arg, rest...); };
    }

    template <typename MemFn>
    static function_type bind(MemFn memFn) {
        return [memFn](Arg arg, Rest... rest) -> R { return ((*arg).*memFn)(rest...); };
    }
};

/********************************** handler **********************************/

// lambda helpers
template <typename Handler>
struct handler;

template <typename... Args>
struct handler<std::function<void(Args...)>>
{
    template <typename T, typename Lambda>
    static std::function<void(Args...)> bind(T* t, const Lambda& lambda) {
        return [weak = std::weak_ptr<T>(std::dynamic_pointer_cast<T>(t->shared_from_this())), lambda](Args... args) {
            if (auto shared = weak.lock()) {
                lambda(args...);
            }
        };
    }

    template <typename T>
    static std::function<void(Args...)> bind(T* t, void (T::*memFn)(Args...)) {
        return [weak = std::weak_ptr<T>(std::dynamic_pointer_cast<T>(t->shared_from_this())), memFn](Args... args) {
            if (auto shared = weak.lock()) {
                ((*shared).*memFn)(args...);
            }
        };
    }

    template <typename T>
    static std::function<void(Args...)> bind(const std::shared_ptr<T>& ptr, void (T::*memFn)(Args...)) {
        return [weak = std::weak_ptr<T>(ptr), memFn](Args... args) {
            if (auto shared = weak.lock()) {
                ((*shared).*memFn)(args...);
            }
        };
    }

    template <typename T>
    static std::function<void(Args...)> bind(const std::weak_ptr<T>& weak, void (T::* memFn)(Args...)) {
        return[weak, memFn](Args... args) {
            if (auto shared = weak.lock()) {
                ((*shared).*memFn)(args...);
            }
        };
    }
};

template<>
struct handler<std::function<void()>>
{
    template <typename T, typename Lambda>
    static std::function<void()> bind(T* t, const Lambda& lambda) {
        return[weak = std::weak_ptr<T>(std::dynamic_pointer_cast<T>(t->shared_from_this())), lambda]() {
            if (auto shared = weak.lock()) {
                lambda();
            }
        };
    }

    template <typename T, typename Fn, typename... Args>
    static std::function<void()> bind(T* t, Fn memFn, Args... args) {
        return[weak = std::weak_ptr<T>(std::dynamic_pointer_cast<T>(t->shared_from_this())), memFn, args...]() {
            if (auto shared = weak.lock()) {
                ((*shared).*memFn)(args...);
            }
        };
    }
    template <typename T, typename Fn, typename... Args>
    static std::function<void()> bind(const std::shared_ptr<T>& ptr, Fn memFn, Args... args) {
        return[weak = std::weak_ptr<T>(ptr), memFn, args...]() {
            if (auto shared = weak.lock()) {
                ((*shared).*memFn)(args...);
            }
        };
    }

    template <typename T, typename Fn, typename... Args>
    static std::function<void()> bind(const std::weak_ptr<T>& weak, Fn memFn, Args... args) {
        return[weak, memFn, args...]() {
            if (auto shared = weak.lock()) {
                ((*shared).*memFn)(args...);
            }
        };
    }
};

/********************************** function_traits **********************************/

template <typename F>
struct function_traits;

// specializations to strip refrence qualifiers
template <typename F>
struct function_traits<F&> : public function_traits<F>
{};

template <typename F>
struct function_traits<F&&> : public function_traits<F>
{};

// specialization on function signature
template <typename R>
struct function_traits<R()>
{
    using return_type = R;
    using argument_0 = void;

    using helper = function_helper<R>;
};

template <typename R, typename... Args>
struct function_traits<R(Args...)>
{
    static constexpr size_t arity = sizeof...(Args);

    using return_type = R;

    template <size_t N>
    struct arguments
    {
        static_assert(N < arity, "invalid function traits argument index.");
        using type = std::tuple_element_t<N, std::tuple<Args...>>;
    };

    template<size_t N>
    using argument_N = typename arguments<N>::type;

    using argument_0 = typename arguments<0>::type;
    using helper = function_helper<R, Args...>;

    template <typename T>
    using member_function_traits = function_traits<R(T, Args...)>;
};

// function pointer
template <typename R, typename... Args>
struct function_traits<R(*)(Args...)> : public function_traits<R(Args...)>
{};

// member function pointer
template <typename R, typename F, typename... Args>
struct function_traits<R(F::*)(Args...)> : public function_traits<R(Args...)>
{};

// const member function pointer
template <typename R, typename F, typename... Args>
struct function_traits<R(F::*)(Args...) const> : public function_traits<R(Args...)>
{};

// callable type (functor)
template <typename F>
struct function_traits : public function_traits<decltype(&F::operator())>
{};

template <typename T, typename Lambda>
typename function_traits<Lambda>::helper::function_type weak_handler(T* t, const Lambda& lambda) {
    return handler<typename function_traits<Lambda>::helper::function_type>::template bind<T, Lambda>(t, lambda);
}
