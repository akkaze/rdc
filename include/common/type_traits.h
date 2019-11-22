/*!
 *  Copyright (c) 2018 by Contributors
 * \file type_traits.h
 * @brief type traits information header
 */
#pragma once

#include "core/base.h"
#include <type_traits>
#include <string>

namespace rdc {
/*!
 * @brief the string representation of type name
 * @tparam T the type to query
 * \return a const string of typename.
 */
template<typename T>
inline const char* type_name() {
  return "";
}

/*!
 * @brief whether a type have save/load function
 * @tparam T the type to query
 */
template<typename T>
struct has_saveload {
  /*! @brief the value of the traits */
  static const bool value = false;
};

/*!
 * @brief template to select type based on condition
 * For example, IfThenElseType<true, int, float>::Type will give int
 * @tparam cond the condition
 * @tparam Then the typename to be returned if cond is true
 * @tparam The typename to be returned if cond is false
*/
template<bool cond, typename Then, typename Else>
struct IfThenElseType;

/*! @brief macro to quickly declare traits information */
#define RDC_DECLARE_TRAITS(Trait, Type, Value)       \
  template<>                                          \
  struct Trait<Type> {                                \
    static const bool value = Value;                  \
  }

/*! @brief macro to quickly declare traits information */
#define RDC_DECLARE_TYPE_NAME(Type, Name)            \
  template<>                                          \
  inline const char* type_name<Type>() {              \
    return Name;                                      \
  }

//! \cond Doxygen_Suppress
RDC_DECLARE_TYPE_NAME(float, "float");
RDC_DECLARE_TYPE_NAME(double, "double");
RDC_DECLARE_TYPE_NAME(int, "int");
RDC_DECLARE_TYPE_NAME(uint32_t, "int (non-negative)");
RDC_DECLARE_TYPE_NAME(uint64_t, "long (non-negative)");
RDC_DECLARE_TYPE_NAME(std::string, "string");
RDC_DECLARE_TYPE_NAME(bool, "boolean");

template<typename Then, typename Else>
struct IfThenElseType<true, Then, Else> {
  typedef Then Type;
};

template<typename Then, typename Else>
struct IfThenElseType<false, Then, Else> {
  typedef Else Type;
};
//! \endcond
}  // namespace rdc
