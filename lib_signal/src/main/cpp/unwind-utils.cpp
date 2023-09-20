#include <unwind.h>
#include <dlfcn.h>
#include <iomanip>
#include <sstream>
#include <android/log.h>


struct BacktraceState {
    void **current;
    void **end;
};

/**
 * _Unwind_GetIP 获取当前函数调用栈中每个函数的绝对内存地址（也就是下面的 pc 值），写入到_Unwind_Context结构体中，最终返回的是当前调用栈的全部函数地址了
 */
static _Unwind_Reason_Code unwindCallback(struct _Unwind_Context *context, void *args) {
    BacktraceState *state = static_cast<BacktraceState *>(args);
    uintptr_t pc = _Unwind_GetIP(context);
    if (pc) {
        if (state->current == state->end) {
            return _URC_END_OF_STACK;
        } else {
            *state->current++ = reinterpret_cast<void *>(pc);
        }
    }
    return _URC_NO_REASON;
}

/**
 * 关于 _Unwind_Backtrace:
    在JNI（Java Native Interface）中，unwind.h头文件中的_Unwind_Backtrace函数用于获取当前线程的函数调用堆栈信息。它通过遍历函数调用链，从当前函数一直回溯到调用栈的栈顶，收集每个函数的返回地址。
    _Unwind_Backtrace函数的原型如下：
     "int _Unwind_Backtrace(_Unwind_Trace_Fn trace, void *trace_argument);"
        其中，trace参数是一个回调函数，用于接收堆栈信息。trace_argument参数是传递给回调函数的额外参数。
        通过调用_Unwind_Backtrace函数，可以获取当前线程的函数调用堆栈信息，并且可以在回调函数中对这些信息进行处理，例如打印、记录或分析。
        需要注意的是，_Unwind_Backtrace函数是一个非标准的函数，它是GNU C库提供的一个特性。在不同的平台和编译器中，可能会有不同的实现方式或函数名。因此，在使用时需要根据具体的平台和编译器进行适配和调整。
        这个函数通常在处理异常、调试和性能分析等方面使用，以获取函数调用的上下文信息，帮助开发人员进行代码调试和优化。

    结合上面的unwindCallback，简单说就是captureBacktrace()函数就是将获取的堆栈放置在传入的buffer中，然后返回堆栈的大小
 */

size_t captureBacktrace(void **buffer, size_t max) {
    BacktraceState state = {buffer, buffer + max};
    _Unwind_Backtrace(unwindCallback, &state);
    // 获取大小
    return state.current - buffer;
}

/**
 * dladdr()可以获得共享库加载到内存的起始地址，和pc值相减就可以获得相对偏移地址，并且可以获得共享库的名字
 * 简单说就是将拿到的堆栈转换成可读的数据并且填充到&os中
 */

void dumpBacktrace(std::ostream &os, void **buffer, size_t count) {
    for (size_t idx = 0; idx < count; ++idx) {
        const void *addr = buffer[idx];
        const char *symbol = "";
        const char *symbol_so = "";
        Dl_info info;
        if (dladdr(addr, &info) && info.dli_sname) {
            symbol = info.dli_sname;
            symbol_so = info.dli_fname;
        }
        os << "  #" << std::setw(2) << idx << ": " << addr << "  " << symbol << " "<< symbol_so <<"\n";
    }
}

/**
 * captureBacktrace是用于将堆栈信息填充至buffer，同时返回堆栈的大小
 * 然后调用dumpBacktrace() 遍历buffer，将每一项的堆栈转换一下，填充到oss中。
 * `std::ostringstream` 是 C++ 标准库中的一个类，它提供了一个便捷的方式来构建和操作字符串。它是一个输出字符串流，允许你像操作文件或控制台输出一样操作字符串。你可以使用 `<<` 操作符向 `std::ostringstream` 对象中插入数据，然后使用 `str()` 成员函数来获取构建的字符串。

    在 JNI（Java Native Interface）中，`std::ostringstream` 可能被用于构建错误消息、日志消息或其他需要动态生成的字符串。然而，它与 Android 的 logcat 没有直接关系。
    logcat 是 Android 平台上的一个日志系统，它收集并显示来自应用程序、系统和硬件的日志消息。在 JNI 代码中，你可以使用 Android 日志库（`<android/log.h>`）中的 `__android_log_print` 函数将日志消息发送到 logcat。为了将 `std::ostringstream` 中的字符串发送到 logcat，你需要先调用 `str()` 函数获取字符串，然后将其传递给 `__android_log_print` 函数。
        例如：
        #include <android/log.h>
        #include <sstream>

        void logMessage() {
            std::ostringstream oss;
            oss << "This is a log message from JNI.";
            __android_log_print(ANDROID_LOG_INFO, "MyAppTag", "%s", oss.str().c_str());
        }
    这段代码中，我们使用 `std::ostringstream` 构建了一个字符串，然后将其发送到 logcat。在 logcat 中，我们将看到 "This is a log message from JNI." 这条消息，标签为 "MyAppTag"。
 */

std::string backtraceToLogcat() {
    const size_t max = 30;
    void *buffer[max];
    std::ostringstream oss;


    dumpBacktrace(oss, buffer, captureBacktrace(buffer, max));
    return oss.str();
}



