// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_C_SIGNATURE_H_
#define V8_COMPILER_C_SIGNATURE_H_

#include "src/machine-type.h"

namespace v8 {
namespace internal {
namespace compiler {

#define FOREACH_CTYPE_MACHINE_TYPE_MAPPING(V) \
  V(void, MachineType::None())                \
  V(bool, MachineType::Uint8())               \
  V(int8_t, MachineType::Int8())              \
  V(uint8_t, MachineType::Uint8())            \
  V(int16_t, MachineType::Int16())            \
  V(uint16_t, MachineType::Uint16())          \
  V(int32_t, MachineType::Int32())            \
  V(uint32_t, MachineType::Uint32())          \
  V(int64_t, MachineType::Int64())            \
  V(uint64_t, MachineType::Uint64())          \
  V(float, MachineType::Float32())            \
  V(double, MachineType::Float64())           \
  V(void*, MachineType::Pointer())            \
  V(int*, MachineType::Pointer())

template <typename T>
inline constexpr MachineType MachineTypeForC() {
  static_assert(std::is_convertible<T, Object*>::value,
                "all non-specialized types must be convertible to Object*");
  return MachineType::AnyTagged();
}

#define DECLARE_TEMPLATE_SPECIALIZATION(ctype, mtype)     \
  template <>                                             \
  inline MachineType constexpr MachineTypeForC<ctype>() { \
    return mtype;                                         \
  }
FOREACH_CTYPE_MACHINE_TYPE_MAPPING(DECLARE_TEMPLATE_SPECIALIZATION)
#undef DECLARE_TEMPLATE_SPECIALIZATION

// Helper for building machine signatures from C types.
class CSignature : public MachineSignature {
 protected:
  CSignature(size_t return_count, size_t parameter_count, MachineType* reps)
      : MachineSignature(return_count, parameter_count, reps) {}

 public:
  template <typename... Params>
  static void VerifyParams(MachineSignature* sig) {
    // Verifies the C signature against the machine types.
    std::array<MachineType, sizeof...(Params)> params{
        {MachineTypeForC<Params>()...}};
    for (size_t p = 0; p < params.size(); ++p) {
      CHECK_EQ(sig->GetParam(p), params[p]);
    }
  }

  static CSignature* FromMachine(Zone* zone, MachineSignature* msig) {
    return reinterpret_cast<CSignature*>(msig);
  }

  template <typename... ParamMachineTypes>
  static CSignature* New(Zone* zone, MachineType ret,
                         ParamMachineTypes... params) {
    constexpr size_t param_count = sizeof...(params);
    std::array<MachineType, param_count> param_arr{{params...}};
    const size_t buffer_size =
        param_count + (ret == MachineType::None() ? 0 : 1);
    MachineType* buffer = zone->NewArray<MachineType>(buffer_size);
    size_t pos = 0;
    size_t return_count = 0;
    if (ret != MachineType::None()) {
      buffer[pos++] = ret;
      return_count++;
    }
    for (MachineType p : param_arr) {
      // Check that there are no MachineType::None()'s in the parameters.
      CHECK_NE(MachineType::None(), p);
      buffer[pos++] = p;
    }
    DCHECK_EQ(buffer_size, pos);
    return new (zone) CSignature(return_count, param_count, buffer);
  }
};

// Helper classes for instantiating Signature objects to be callable from C.
template <typename Ret, typename... Params>
class CSignatureOf : public CSignature {
 public:
  CSignatureOf() : CSignature(kReturnCount, kParamCount, storage_) {
    constexpr std::array<MachineType, kParamCount> param_types{
        MachineTypeForC<Params>()...};
    if (kReturnCount == 1) storage_[0] = MachineTypeForC<Ret>();
    static_assert(
        std::is_same<decltype(*reps_), decltype(*param_types.data())>::value,
        "type mismatch, cannot memcpy");
    memcpy(storage_ + kReturnCount, param_types.data(),
           sizeof(*storage_) * kParamCount);
  }

 private:
  static constexpr size_t kReturnCount =
      MachineTypeForC<Ret>() == MachineType::None() ? 0 : 1;
  static constexpr size_t kParamCount = sizeof...(Params);

  MachineType storage_[kReturnCount + kParamCount];
};

typedef CSignatureOf<int32_t, int32_t, int32_t> CSignature_i_ii;
typedef CSignatureOf<uint32_t, uint32_t, uint32_t> CSignature_u_uu;
typedef CSignatureOf<float, float, float> CSignature_f_ff;
typedef CSignatureOf<double, double, double> CSignature_d_dd;
typedef CSignatureOf<Object*, Object*, Object*> CSignature_o_oo;

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_C_SIGNATURE_H_
