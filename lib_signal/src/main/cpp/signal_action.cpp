//
// Created by pika on 2023/1/5.
//


#include <malloc.h>
#include <android/log.h>
#include <sys/types.h>
#include <signal.h>
#include "signal_action.h"
#include "signal_exception.h"


void init_with_signal(JNIEnv *env, jclass klass,
                      jintArray signals,void (*handler)(int, struct siginfo *, void *)) {
    // 注意释放内存
    jint *signalsFromJava = (*env).GetIntArrayElements(signals, 0);
    int size = (*env).GetArrayLength(signals);
    int needMask = 0;

    // 是否需要监听SIGQUIT SIGQUIT默认是zygote是被屏蔽的 ANR的时候会发这个信号，所以我们需要监控这个的时候 会个标记
    for (int i = 0; i < size; i++) {
        if (signalsFromJava[i] == SIGQUIT) {
            needMask = 1;
        }
    }

    do {
        sigset_t mask;
        sigset_t old;
        // 这里需要stack_t，临时的备用信号栈，因为SIGSEGV时，当前栈空间已经是处于进程所能控制的栈中，此时就不能在这个栈里面操作，否则就会异常循环
        stack_t ss;
        if (NULL == (ss.ss_sp = calloc(1, SIGNAL_CRASH_STACK_SIZE))) {
            handle_exception(env);
            break;
        }
        ss.ss_size = SIGNAL_CRASH_STACK_SIZE;
        ss.ss_flags = 0;
        // 调用 sigaltstack(&ss, NULL) 将备用信号栈设置为 ss
        if (0 != sigaltstack(&ss, NULL)) {
            handle_exception(env);
            break;
        }

        // 当时需要监听SIGQUIT信号的时候，即ANR，需要开启监听，SIGQUIT的信号默认这个是关掉的
        if (needMask) {
            sigemptyset(&mask);
            sigaddset(&mask, SIGQUIT);
            if (0 != pthread_sigmask(SIG_UNBLOCK, &mask, &old)) {
                break;
            }
        }


        struct sigaction sigc;
        sigc.sa_sigaction = handler;
        // 信号处理时，先阻塞所有的其他信号，避免干扰正常的信号处理程序
        sigfillset(&sigc.sa_mask);
        // 将 SA_SIGINFO、SA_ONSTACK 和 SA_RESTART 标志赋值给 sigc.sa_flags，表示在处理信号时，使用扩展的信号信息、使用备用信号栈，并允许系统在信号处理函数返回后重新启动系统调用。
        sigc.sa_flags = SA_SIGINFO | SA_ONSTACK |SA_RESTART;


        // 注册所有信号
        for (int i = 0; i < size; i++) {
            // 这里不需要旧的处理函数
            // 指定SIGKILL和SIGSTOP以外的所有信号
            int flag = sigaction(signalsFromJava[i], &sigc, NULL);
            if (flag == -1) {
                __android_log_print(ANDROID_LOG_INFO, TAG, "register fail ===== signals[%d] ", i);
                handle_exception(env);
                // 失败后需要恢复原样
                if (needMask) {
                    pthread_sigmask(SIG_SETMASK, &old, NULL);
                }
                break;
            }
        }


    } while (0);
    (*env).ReleaseIntArrayElements(signals, signalsFromJava, 0);

}