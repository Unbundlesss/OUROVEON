#ifndef CEREAL_OPTIONAL_OPTIONAL_BINARY_HPP
#define CEREAL_OPTIONAL_OPTIONAL_BINARY_HPP

#include "cereal/archives/binary.hpp"
#include "optional_nvp.hpp"

namespace cereal {

template<class T, class TV>
void prologue(BinaryInputArchive &, OptionalNameValuePair<T, TV> const &) { }

template<class T, class TV>
void epilogue(BinaryOutputArchive &, OptionalNameValuePair<T, TV> const &) { }

template<class T, class TV>
void prologue(BinaryOutputArchive &, OptionalNameValuePair<T, TV> const &) { }

template<class T, class TV>
void epilogue(BinaryInputArchive &, OptionalNameValuePair<T, TV> const &) { }

template<class T, class TV>
void CEREAL_LOAD_FUNCTION_NAME(BinaryInputArchive &archive, OptionalNameValuePair<T, TV> &value) {
    archive(value.value);
}

template<class T, class TV>
void CEREAL_SAVE_FUNCTION_NAME(BinaryOutputArchive &archive, OptionalNameValuePair<T, TV> const &value) {
    archive(value.value);
}

}

#endif //CEREAL_OPTIONAL_OPTIONAL_BINARY_HPP
