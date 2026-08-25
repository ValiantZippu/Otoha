package ua.syt0r.kanji.core.logger


interface NativeLogger {

    companion object {
        lateinit var instance: NativeLogger
    }

    fun logInfo(message: String)
    fun logError(message: String)

}

actual fun platformLogMessage(message: String) {
    NativeLogger.instance.logInfo(message)
}

actual fun platformLogMethod() {}

actual fun platformLogError(message: String) {
    NativeLogger.instance.logError(message)
}