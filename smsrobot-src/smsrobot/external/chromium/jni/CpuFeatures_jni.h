// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is autogenerated by
//     base/android/jni_generator/jni_generator.py
// For
//     org/chromium/base/CpuFeatures

#ifndef org_chromium_base_CpuFeatures_JNI
#define org_chromium_base_CpuFeatures_JNI

#include <jni.h>

#include "base/android/jni_generator/jni_generator_helper.h"

// Step 1: forward declarations.
JNI_REGISTRATION_EXPORT extern const char
    kClassPath_org_chromium_base_CpuFeatures[];
const char kClassPath_org_chromium_base_CpuFeatures[] =
    "org/chromium/base/CpuFeatures";

// Leaking this jclass as we cannot use LazyInstance from some threads.
JNI_REGISTRATION_EXPORT base::subtle::AtomicWord
    g_org_chromium_base_CpuFeatures_clazz = 0;
#ifndef org_chromium_base_CpuFeatures_clazz_defined
#define org_chromium_base_CpuFeatures_clazz_defined
inline jclass org_chromium_base_CpuFeatures_clazz(JNIEnv* env) {
  return base::android::LazyGetClass(env,
      kClassPath_org_chromium_base_CpuFeatures,
      &g_org_chromium_base_CpuFeatures_clazz);
}
#endif

namespace base {
namespace android {

// Step 2: method stubs.

static jint JNI_CpuFeatures_GetCoreCount(JNIEnv* env, const
    base::android::JavaParamRef<jclass>& jcaller);

JNI_GENERATOR_EXPORT jint
    Java_org_chromium_base_CpuFeatures_nativeGetCoreCount(JNIEnv* env, jclass
    jcaller) {
  TRACE_NATIVE_EXECUTION_SCOPED("GetCoreCount");
  return JNI_CpuFeatures_GetCoreCount(env,
      base::android::JavaParamRef<jclass>(env, jcaller));
}

static jlong JNI_CpuFeatures_GetCpuFeatures(JNIEnv* env, const
    base::android::JavaParamRef<jclass>& jcaller);

JNI_GENERATOR_EXPORT jlong
    Java_org_chromium_base_CpuFeatures_nativeGetCpuFeatures(JNIEnv* env, jclass
    jcaller) {
  TRACE_NATIVE_EXECUTION_SCOPED("GetCpuFeatures");
  return JNI_CpuFeatures_GetCpuFeatures(env,
      base::android::JavaParamRef<jclass>(env, jcaller));
}

}  // namespace android
}  // namespace base

#endif  // org_chromium_base_CpuFeatures_JNI
