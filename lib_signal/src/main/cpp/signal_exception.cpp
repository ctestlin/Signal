//
// Created by pika on 2023/1/6.
//

#include <jni.h>
#include "signal_exception.h"
void handle_exception(JNIEnv *env) {
    // 异常处理
    jclass main = (*env).FindClass(CALL_JNI_CLASS_NAME);
    jmethodID id = (*env).GetStaticMethodID(main, "signalError", "()V");
    (*env).CallStaticVoidMethod(main, id);
}