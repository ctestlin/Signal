//
// Created by pika on 2023/1/5.
//

#include <android/log.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <pthread.h>
#include "signal_action.h"
#include "signal_exception.h"
#include "errno.h"
#include "unwind-utils.h"
#include <string.h>
#include <string>

#define JNI_CLASS_NAME "com/pika/lib_signal/SignalController"


JavaVM *javaVm = NULL;
static int notifier = -1;
static jclass callClass;

// 用于监听信号量时 触发该函数
static void sig_func(int sig_num, struct siginfo *info, void *ptr) {
    long data;
    data = sig_num;
    __android_log_print(ANDROID_LOG_INFO, TAG, "catch signal %lu %d", data,notifier);

    /// notifier由eventfd(0,EFD_CLOEXEC)的返回值修改，返回的是一个文件描述符，如果有这个值，表示初始化成功
    if (notifier >= 0) {
        // 该write函数 会触发invoke_crash中的read，从而执行java中的callNativeException函数，达到监听的目的
        write(notifier, &data, sizeof data);
    }
}

static void* invoke_crash(void *arg){
    JNIEnv *env = NULL;
    if(JNI_OK != (*javaVm).AttachCurrentThread(&env,NULL)){
        return NULL;
    }
    uint64_t data;
    /// 此函数会阻塞，直到sig_func中执行write操作，读取到sizeof data后会继续执行
    read(notifier,&data,sizeof data);
    /// 下面是调用java中的callNativeException函数

    /// 获取方法的id
    jmethodID id = (*env).GetStaticMethodID(callClass, "callNativeException", "(ILjava/lang/String;)V");
//    jstring nativeStackTrace  = (*env)->NewStringUTF(env,backtraceToLogcat());

    /// 构建入参 堆栈信息
    std::string stackTrace = backtraceToLogcat();
    jstring nativeStackTrace = (*env).NewStringUTF(stackTrace.c_str());
    jint signal_tag = data;
    // 调用函数 callClass为JNI_OnLoad中赋值的，为SignalController

    /// CallStaticVoidMethod的入参依次为：
    /// env -> JNI环境
    /// callClass-> 被执行java类的class引用
    /// id -> 被执行函数的id
    /// signal_tag -> 上述执行函数（callNativeException）的入参1
    /// nativeStackTrace -> 上述执行函数（callNativeException）的入参2
    (*env).CallStaticVoidMethod(callClass, id,signal_tag,nativeStackTrace);
    /// DeleteLocalRef是用来删除局部引用的,不删除局部引用会造成内存泄漏
    (*env).DeleteLocalRef(nativeStackTrace);
    return NULL;

}

extern "C"
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env;
    javaVm = vm;
    jclass cls;
    if (NULL == vm) return -1;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }
    if (NULL == (cls = (*env).FindClass( JNI_CLASS_NAME))) return -1;

    // 此时的cls仅仅是一个局部变量，如果错误引用会出现错误
    callClass = static_cast<jclass>((*env).NewGlobalRef(cls));


    return JNI_VERSION_1_6;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_pika_lib_1signal_SignalController_initWithSignals(JNIEnv *env, jclass clazz,
                                                              jintArray signals) {
    init_with_signal(env, clazz, signals, sig_func);
    notifier = eventfd(0,EFD_CLOEXEC);


    // 启动异步线程 该线程执行invoke_crash函数 invoke_crash中有执行read，会阻塞，直到sig_func中write之后，则执行；
    // sig_func是用来传入init_with_signal中的，就是用于sigaction的时候回调该函数
    pthread_t thd;
    if(0 != pthread_create(&thd, NULL,invoke_crash, NULL)){
        handle_exception(env);
        close(notifier);
        notifier = -1;
    }
}