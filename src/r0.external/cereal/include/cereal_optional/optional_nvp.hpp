#ifndef CEREAL_OPTIONAL_OPTIONAL_NVP_HPP
#define CEREAL_OPTIONAL_OPTIONAL_NVP_HPP

#include "cereal/cereal.hpp"

namespace cereal {

/// @struct OptionalNameValuePair
/// @brief Name-value pair that can be default-initialized in case of name search failure
/// @tparam T Property type
/// @tparam TV Property value type
template<class T, class TV>
struct OptionalNameValuePair {
  // same as the cereal::NameValuePair::Type cause it's private
  using Type = typename std::conditional<std::is_array<typename std::remove_reference<T>::type>::value,
      typename std::remove_cv<T>::type,
      typename std::conditional<std::is_lvalue_reference<T>::value,
          T,
          typename std::decay<T>::type>::type>::type;
  
  
  // source property
  Type value;
  
  // default property value
  TV defaultValue;
  
  // property name
  const char *name;
  
  
  OptionalNameValuePair(const char *name, T &&value, TV &&defaultValue)
      : name(name), value(std::forward<T>(value)), defaultValue(std::forward<TV>(defaultValue)) { }
  
  /// Delete default copy constructor
  OptionalNameValuePair(const OptionalNameValuePair &) = delete;
  
  /// And enable move constructor by default
  OptionalNameValuePair(OptionalNameValuePair &&) = default;
};

template<class T>
struct OptionalNameValuePair<T, void> {
  // same as the cereal::NameValuePair::Type cause it's private
  using Type = typename std::conditional<std::is_array<typename std::remove_reference<T>::type>::value,
      typename std::remove_cv<T>::type,
      typename std::conditional<std::is_lvalue_reference<T>::value,
          T,
          typename std::decay<T>::type>::type>::type;
  
  
  // source property
  Type value;
  
  // property name
  const char *name;
  
  
  OptionalNameValuePair(const char *name, T &&value)
      : name(name), value(std::forward<T>(value)) { }
  
  /// Delete default copy constructor
  OptionalNameValuePair(const OptionalNameValuePair &) = delete;
  
  /// And enable move constructor by default
  OptionalNameValuePair(OptionalNameValuePair &&) = default;
};

/// Makes optional name-value pair
/// @param name Property name
/// @param option Property
/// @param default_value Default property value
/// @return Optional NVP
template<class T, class TV>
OptionalNameValuePair<T, TV> make_optional_nvp(const char *name, T &&option, TV &&defaultValue) {
    return { name, std::forward<T>(option), std::forward<TV>(defaultValue) };
};
template<class T>
OptionalNameValuePair<T, void> make_optional_nvp(const char *name, T &&option) {
    return { name, std::forward<T>(option) };
};

/// Optional name-value pair builder method overload for std::string property name
template<class T, class TV>
OptionalNameValuePair<T, TV> make_optional_nvp(const std::string &name, T &&option, TV &&defaultValue) {
    return { name.c_str(), std::forward<T>(option), std::forward<TV>(defaultValue) };
};
template<class T>
OptionalNameValuePair<T, void> make_optional_nvp(const std::string &name, T &&option) {
    return { name.c_str(), std::forward<T>(option) };
};
}

#define CEREAL_OPTIONAL_NVP(T, ...) ::cereal::make_optional_nvp(#T, T, __VA_ARGS__)


#endif //CEREAL_OPTIONAL_OPTIONAL_NVP_HPP
